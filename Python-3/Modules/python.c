/* Minimal main program -- everything is loaded from the library */

#include "Python.h"
#include <locale.h>

#ifdef __FreeBSD__
#include <floatingpoint.h>
#endif

#if defined(__ARM__) && defined(__VFP_FP__) && defined(__QNXNTO__)
#define ARM_DENORMAL_FIX
#define ARM_VFP_FPSCR_FZ 0x1000000 // FPSCR flush-to-zero mode enable bit

#ifndef Py_LIMITED_API
PyAPI_FUNC(unsigned long) _Py_get_vfpcontrolword(void);
PyAPI_FUNC(void) _Py_set_vfpcontrolword(unsigned long);
#endif

/* inline assembly for getting and setting the VFP FPU control word on
   ARM */

unsigned long _Py_get_vfpcontrolword(void) {
    unsigned long cw;
    __asm__ __volatile__ (
        "vmrs %0, fpscr\n"
         : "=r" (cw) : );
    return cw;
}

void _Py_set_vfpcontrolword(unsigned long cw) {
    __asm__ __volatile__ (
        "vmsr fpscr, %0\n"
         :: "r" (cw) );
}
#endif

#ifdef MS_WINDOWS
int
wmain(int argc, wchar_t **argv)
{
    return Py_Main(argc, argv);
}
#else

#ifdef __APPLE__
extern wchar_t* _Py_DecodeUTF8_surrogateescape(const char *s, Py_ssize_t size);
#endif

int
main(int argc, char **argv)
{
    wchar_t **argv_copy = (wchar_t **)PyMem_Malloc(sizeof(wchar_t*)*argc);
    /* We need a second copies, as Python might modify the first one. */
    wchar_t **argv_copy2 = (wchar_t **)PyMem_Malloc(sizeof(wchar_t*)*argc);
    int i, res;
    char *oldloc;
    /* 754 requires that FP exceptions run in "no stop" mode by default,
     * and until C vendors implement C99's ways to control FP exceptions,
     * Python requires non-stop mode.  Alas, some platforms enable FP
     * exceptions by default.  Here we disable them.
     */
#ifdef ARM_DENORMAL_FIX
    unsigned long vfp_cw = _Py_get_vfpcontrolword();
    vfp_cw &= ~ARM_VFP_FPSCR_FZ;
    _Py_set_vfpcontrolword(vfp_cw);
#endif

#ifdef __FreeBSD__
    fp_except_t m;

    m = fpgetmask();
    fpsetmask(m & ~FP_X_OFL);
#endif
    if (!argv_copy || !argv_copy2) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    oldloc = strdup(setlocale(LC_ALL, NULL));
    setlocale(LC_ALL, "");
    for (i = 0; i < argc; i++) {
#ifdef __APPLE__
        argv_copy[i] = _Py_DecodeUTF8_surrogateescape(argv[i], strlen(argv[i]));
#else
        argv_copy[i] = _Py_char2wchar(argv[i], NULL);
#endif
        if (!argv_copy[i])
            return 1;
        argv_copy2[i] = argv_copy[i];
    }
    setlocale(LC_ALL, oldloc);
    free(oldloc);
    res = Py_Main(argc, argv_copy);
    for (i = 0; i < argc; i++) {
        PyMem_Free(argv_copy2[i]);
    }
    PyMem_Free(argv_copy);
    PyMem_Free(argv_copy2);
    return res;
}
#endif
