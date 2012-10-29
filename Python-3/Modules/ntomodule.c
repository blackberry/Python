#include <Python.h>
#include <spawn.h>
#include <unistd.h>
#include <sys/pathmgr.h>
#include <bytesobject.h>


#define INH_PARSE_FLAG(bit,name,type,fmt) \
        if (flags&bit) {                  \
            if (PyMapping_HasKeyString(o_inherit, #name)) {       \
                item = PyMapping_GetItemString(o_inherit, #name); \
                if (!PyArg_Parse(item, #fmt, &(inherit->name))) { \
                    PyErr_SetString(PyExc_TypeError,              \
                                    #name " must be an " #type);  \
                    Py_DECREF(item); \
                    goto cleanup;    \
                }                    \
                Py_DECREF(item);     \
            } else {                 \
                PyErr_SetString(PyExc_TypeError, #name " not found"); \
                goto cleanup; \
            } \
        }

#define INH_NOT_IMPL(bit,name) \
    if (flags&bit) { \
        PyErr_SetString(PyExc_TypeError, #name " not implemented"); \
        goto cleanup; \
    }

static PyObject *
nto_spawn(PyObject *self, PyObject *args)
{
    const char *path=0;
    int fd_count = 0;
    int *fd_map = 0;
    PyObject *o_fd_map=0;
    struct inheritance * inherit = 0;
    PyObject *o_inherit=0;
    char **argv = 0;
    char *def_argv[1]={0};
    PyObject *o_argv=0;
    char **envp = 0;
    PyObject *o_envp=0;
    pid_t pid;
    PyObject *keys=0, *vals=0;
    PyObject *o_ret = 0;
    PyObject *(*getitem)(PyObject *, Py_ssize_t);

    if (!PyArg_ParseTuple(args, "etOOOO",
                          Py_FileSystemDefaultEncoding,
                          &path, &o_fd_map, &o_inherit, &o_argv, &o_envp))
        return NULL;

    if (o_fd_map != Py_None) {
        int len;
        int i;
        if (PyList_Check(o_fd_map)) {
            len = PyList_Size(o_fd_map);
            getitem = PyList_GetItem;
        } else if (PyTuple_Check(o_fd_map)) {
            len = PyTuple_Size(o_fd_map);
            getitem = PyTuple_GetItem;
        } else {
            PyErr_SetString(PyExc_TypeError,
                            "spawn() arg 2 must be a tuple or list");
            goto cleanup;
        }
        fd_map = (int*)PyMem_New(int, len);
        if (fd_map == 0) {
            PyErr_NoMemory();
            goto cleanup;
        }
        for (i = 0; i < len; i++) {
            PyObject * item;
            item = (getitem)(o_fd_map, i);
            if (item == Py_None) {
                fd_map[i]= SPAWN_FDCLOSED;
            } else {
                if (!PyArg_Parse(item, "i", &fd_map[i])) {
                    PyErr_SetString(
                        PyExc_TypeError,
                        "spawn() arg 2 must be a list or tupple of int");
                    goto cleanup;
                }
            }
        }
        fd_count = len;
    }

    if (o_inherit != Py_None) {
        int len;
        unsigned long flags = 0;
        PyObject *item;
        len = PyMapping_Size(o_inherit);
        if (len < 0)
            goto cleanup;
        inherit = PyMem_New(struct inheritance,1);
        if (inherit == 0) {
            PyErr_NoMemory();
            goto cleanup;
        }
        memset(inherit,0,sizeof(struct inheritance));
        if (PyMapping_HasKeyString(o_inherit,"flags")) {
            item = PyMapping_GetItemString(o_inherit, "flags");
            if (!PyArg_Parse(item, "k", &(inherit->flags))) {
                PyErr_SetString(PyExc_TypeError,
                                "flags must be unsigned long");
                Py_DECREF(item);
                goto cleanup;
            }
            Py_DECREF(item);
        }

        flags = inherit->flags;

        INH_PARSE_FLAG(SPAWN_EXPLICIT_CPU, runmask, int, i);
        INH_PARSE_FLAG(SPAWN_SETGROUP,     pgroup, long, l);
        INH_PARSE_FLAG(SPAWN_SETND,        nd, uint32_t, I);
        INH_PARSE_FLAG(SPAWN_SETSTACKMAX,  stack_max, unsigned long, k);

        INH_NOT_IMPL(SPAWN_EXPLICIT_SCHED, policy);
        INH_NOT_IMPL(SPAWN_SETSIGMASK,     sigmask);
        INH_NOT_IMPL(SPAWN_SETSIGDEF,      sigdefault);
        INH_NOT_IMPL(SPAWN_SETSIGIGN,      sigignore);
    }

    if (o_argv != Py_None) {
        int len;
        int i;
        if (PyList_Check(o_argv)) {
            len = PyList_Size(o_argv);
            getitem = PyList_GetItem;
        } else if (PyTuple_Check(o_argv)) {
            len = PyTuple_Size(o_argv);
            getitem = PyTuple_GetItem;
        } else {
            PyErr_SetString(PyExc_TypeError,
                            "spawn() arg 4 must be a tuple or list");
            goto cleanup;
        }
        argv = (char**)PyMem_New(char*, len+1);
        if (argv == 0) {
            PyErr_NoMemory();
            goto cleanup;
        }
        for (i = 0; i <= len; i++)
            argv[i] = 0;
        for (i = 0; i < len; i++) {
            if (!PyArg_Parse((getitem)(o_argv, i), "et",
                             Py_FileSystemDefaultEncoding,
                             &argv[i])) {
                PyErr_SetString(PyExc_TypeError,
                                "spawn() arg 4 must contain only strings");
                goto cleanup;
            }
        }
    }

    if (o_envp != Py_None) {
        int len;
        int i;
        len = PyMapping_Size(o_envp);
        if (len < 0)
            goto cleanup;
        envp = (char**)PyMem_New(char *, len+1);
        if (envp == 0) {
            PyErr_NoMemory();
            goto cleanup;
        }
        for (i=0; i <= len; i++)
            envp[i]=0;
        keys = PyMapping_Keys(o_envp);
        vals = PyMapping_Values(o_envp);
        if (!keys || !vals)
            goto cleanup;
        if (!PyList_Check(keys) || !PyList_Check(vals)) {
            PyErr_SetString(PyExc_TypeError,
                            "spawn(): arg 5 not a dict");
            goto cleanup;
        }
        for (i = 0; i < len; i++) {
            PyObject *o_k, *o_v;
            const char *k, *v;
            char *p;
            int klen, vlen;
            o_k = PyList_GetItem(keys, i);
            o_v = PyList_GetItem(vals, i);
            if (!o_k || !o_v) {
                goto cleanup;
            }
            if (!PyArg_Parse(o_k,
                             "s#;spawn() arg 5 contains a non-string key",
                             &k, &klen) ||
                !PyArg_Parse(o_v,
                             "s#;spawn() arg 5 contains a non-string value",
                             &v, &vlen)) {
                goto cleanup;
            }
            p = (char*)PyMem_New(char, klen+vlen+2);
            if (p == 0) {
                goto cleanup;
            }
            sprintf(p, "%s=%s", k, v);
            envp[i] = p;
        }
    }

    pid = spawn(path,
                fd_count, fd_map,
                inherit,
                argv?argv:def_argv,
                envp);

    if (pid == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto cleanup;
    }

    o_ret = PyLong_FromLong((long)pid);

  cleanup:
    if (keys) { Py_DECREF(keys); }
    if (vals) { Py_DECREF(vals); }
    if (envp) {
        int i=0;
        while (envp[i]) { PyMem_Free(envp[i]); i++; }
        PyMem_Del(envp);
    }
    if (argv) {
        int i=0;
        while (argv[i]) { PyMem_Free(argv[i]); i++; }
        PyMem_Del(argv);
    }
    if (inherit) { PyMem_Free(inherit); }
    if (fd_map)  { PyMem_Del(fd_map); }

    return o_ret;
}


PyDoc_STRVAR(nto_pread__doc__,
"pread(fd, buffersize, offset) -> string\n\n\
Read a file descriptor at a given offset.");

static PyObject *
nto_pread(PyObject *self, PyObject *args)
{
    int fd, size;
    PyObject *buffer;
    off_t pos;
    PyObject *posobj;
    Py_ssize_t n;

    if (!PyArg_ParseTuple(args, "iiO:pread", &fd, &size, &posobj))
        return NULL;
    if (size < 0) {
        errno = EINVAL;
        return PyErr_SetFromErrno(PyExc_OSError);
    }
    buffer = PyBytes_FromStringAndSize((char *)NULL, size);
    if (buffer == NULL)
        return NULL;
#if !defined(HAVE_LARGEFILE_SUPPORT)
    pos = PyLong_AsLong(posobj);
#else
    pos = PyLong_Check(posobj) ?
        PyLong_AsLongLong(posobj) : PyLong_AsLong(posobj);
#endif
    if (PyErr_Occurred())
        return NULL;
    if (!_PyVerify_fd(fd))
        return PyErr_SetFromErrno(PyExc_OSError);
    Py_BEGIN_ALLOW_THREADS
    n = pread(fd, PyBytes_AsString(buffer), size, pos);
    Py_END_ALLOW_THREADS
    if (n < 0) {
        Py_DECREF(buffer);
        return PyErr_SetFromErrno(PyExc_OSError);
    }
    if (n != size)
        _PyBytes_Resize(&buffer, n);
    return buffer;
}


PyDoc_STRVAR(nto_pwrite__doc__,
"pwrite(fd, string, offset) -> byteswritten\n\n\
Write a string to a file descriptor at a given offset.");

static PyObject *
nto_pwrite(PyObject *self, PyObject *args)
{
    Py_buffer pbuf;
    int fd;
    PyObject *posobj;
    off_t pos;
    Py_ssize_t size;
    if (!PyArg_ParseTuple(args, "is*O:pwrite", &fd, &pbuf, &posobj))
        return NULL;
#if !defined(HAVE_LARGEFILE_SUPPORT)
    pos = PyLong_AsLong(posobj);
#else
    pos = PyLong_Check(posobj) ?
        PyLong_AsLongLong(posobj) : PyLong_AsLong(posobj);
#endif
    if (PyErr_Occurred())
        return NULL;
    if (!_PyVerify_fd(fd))
        return PyErr_SetFromErrno(PyExc_OSError);
    Py_BEGIN_ALLOW_THREADS
    size = pwrite(fd, pbuf.buf, (size_t)pbuf.len, pos);
    Py_END_ALLOW_THREADS
    PyBuffer_Release(&pbuf);
    if (size < 0)
        return PyErr_SetFromErrno(PyExc_OSError);
    return PyLong_FromSsize_t(size);
}

PyDoc_STRVAR(nto_pathmgr_symlink__doc__,
"pathmgr_symlink(src, dst)\n\n\
Create a pathmgr symbolic link pointing to src named dst.");

static PyObject *
nto_pathmgr_symlink(PyObject *self, PyObject *args)
{
    char *path1 = NULL, *path2 = NULL;
    int res;
    if (!PyArg_ParseTuple(args, "etet:pathmgr_symlink",
                          Py_FileSystemDefaultEncoding, &path1,
                          Py_FileSystemDefaultEncoding, &path2))
        return NULL;
    Py_BEGIN_ALLOW_THREADS
    res = pathmgr_symlink(path1, path2);
    Py_END_ALLOW_THREADS
    PyMem_Free(path1);
    PyMem_Free(path2);
    if (res != 0)
        /* XXX how to report both path1 and path2??? */
        return PyErr_SetFromErrno(PyExc_OSError);
    Py_INCREF(Py_None);
    return Py_None;
}


static PyMethodDef NtoMethods[] = {
    {"spawn", nto_spawn, METH_VARARGS, "Spawn a subprocess."},
    {"pread", nto_pread, METH_VARARGS, nto_pread__doc__},
    {"pwrite", nto_pwrite, METH_VARARGS, nto_pwrite__doc__},
    {"pathmgr_symlink", nto_pathmgr_symlink, METH_VARARGS, nto_pathmgr_symlink__doc__},
    {NULL, NULL, 0, NULL}
};


struct PyModuleDef NtoMethods_def = {
	PyModuleDef_HEAD_INIT,
	"nto",
	NULL,
	-1,
	NtoMethods,
	NULL, NULL, NULL, NULL
};


#define P_DICT_INT(d,name) \
    PyDict_SetItemString(d,#name,PyLong_FromLong((long)name));


PyMODINIT_FUNC
PyInit__nto(void)
{
    PyObject *m,*d;


    m = PyModule_Create( &NtoMethods_def );

    if (m == NULL)
      return NULL;
    d = PyModule_GetDict(m);
    P_DICT_INT(d,SPAWN_EXPLICIT_SCHED);
    P_DICT_INT(d,SPAWN_HOLD);
    P_DICT_INT(d,SPAWN_NOZOMBIE);
    P_DICT_INT(d,SPAWN_SEARCH_PATH);
    P_DICT_INT(d,SPAWN_SETGROUP);
    P_DICT_INT(d,SPAWN_SETND);
    P_DICT_INT(d,SPAWN_SETSID);
    P_DICT_INT(d,SPAWN_SETSIGDEF);
    P_DICT_INT(d,SPAWN_SETSIGIGN);
    P_DICT_INT(d,SPAWN_SETSIGMASK);
    P_DICT_INT(d,SPAWN_SETSTACKMAX);
    P_DICT_INT(d,SPAWN_TCSETPGROUP);
    P_DICT_INT(d,SPAWN_ALIGN_MASK);

    return m;

}
