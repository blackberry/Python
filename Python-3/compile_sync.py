"""Script to sync a source .py directory with a binary .pyc directory.

All .py files in the source directory are compiled into .pyc files
in the corresponding binary directory, and .pyc files with no corresponding
.py file are deleted.

See module py_compile for details of the actual byte-compilation.
"""
import os
import sys
import errno
import imp
import py_compile
import struct
import logging

__docformat__ = 'restructuredtext en'
__authors__ = ['Robert Xiao <roxiao@qnx.com>']
__copyright__ = 'Copyright 2011, QNX Software Systems. All Rights Reserved.'
__version__ = '1.0.0'

__all__ = ['compile_sync']

logger = logging.getLogger()

py_ext = '.py'
pyc_ext = '.pyc' if __debug__ else '.pyo'

def find_by_extension(topdir, ext, curdir=''):
    try:
        names = os.listdir(os.path.join(topdir, curdir))
    except os.error:
        return

    files = []
    for name in names:
        if os.path.isdir(os.path.join(topdir, curdir, name)):
            for x in find_by_extension(topdir, ext, os.path.join(curdir, name)):
                yield x
        else:
            fbase, fext = os.path.splitext(name)
            if fext == ext:
                files.append(fbase)
    if files:
        yield (curdir, files)

def compile_file(srcf, dstf, codef, force=False, optimize=-1):
    logger.debug("compile_file(%r, %r)", srcf, dstf)
    if not force:
        try:
            mtime = int(os.stat(srcf).st_mtime)
            expect = struct.pack('<4sl', imp.get_magic(), mtime)
            with open(dstf, 'rb') as fhandle:
                actual = fhandle.read(8)
            if expect == actual:
                return
        except IOError:
            pass

    logger.info("Compiling %r to %r", srcf, dstf)
    try:
        py_compile.compile(srcf, dstf, codef, doraise=True, optimize=optimize)
    except Exception:
        logger.exception("Error compiling %r", srcf)

def compile_sync(srcdir, dstdir, codedir=None, force=False, delete=True, optimize=-1):
    """ Synchronize a source directory with a binary directory.

    :param srcdir: Source directory containing .py files
    :param dstdir: Binary (destination) directory which will contain .pyc files
    :param codedir: Directory name to compile into the .pyc file
    :param force: If True, force compilation even if timestamps are up-to-date
    :param delete: If True, delete .pyc files if they have no matching .py file
    """

    logger.debug("compile_sync(%r, %r, codedir=%r, force=%r, delete=%r, optimize=%r)",
        srcdir, dstdir, codedir, force, delete, optimize)

    if codedir is None:
        codedir = dstdir

    compiled = set()

    for dir, fns in find_by_extension(srcdir, py_ext):
        dstd = os.path.join(dstdir, dir)
        if not os.path.isdir(dstd):
            logger.info("Creating directory %r", dstd)
            try:
                os.makedirs(dstd)
            except os.error:
                logger.exception("Unable to make directory %s", dstd)
                continue

        for fn in fns:
            srcf = os.path.join(srcdir, dir, fn+py_ext)
            dstf = os.path.join(dstdir, dir, fn+pyc_ext)
            codef = os.path.join(codedir, dir, fn+py_ext)

            compiled.add((dir, fn))
            compile_file(srcf, dstf, codef, force=force, optimize=optimize)

    if delete:
        for dir, fns in find_by_extension(dstdir, pyc_ext):
            for fn in fns:
                if (dir, fn) in compiled:
                    continue
                dstf = os.path.join(dstdir, dir, fn+pyc_ext)
                logger.info("Deleting %r", dstf)
                try:
                    os.unlink(dstf)
                except os.error:
                    logger.exception("Unable to delete %r", dstf)

def main():
    """Script main program"""
    import argparse

    parser = argparse.ArgumentParser(
        description="Utilities to support batch bytecode compilation.")
    parser.add_argument('-f', action='store_true', dest='force',
                        help='force rebuild even if timestamps are up to date')
    parser.add_argument('-v', action='count', dest='verbose', default=0,
                        help='be verbose (-vv be more verbose)')
    parser.add_argument('-q', action='store_const', dest='verbose', const=-1,
                        help='output only error messages')
    parser.add_argument('-d', metavar='CODEDIR', dest='codedir', default=None,
                        help=('directory to prepend to file paths for use in '
                              'compile time tracebacks and in runtime '
                              'tracebacks in cases where the source file is '
                              'unavailable'))
    parser.add_argument('-k', action='store_false', dest='delete', default=True,
                        help=('Keep destination directory files even if no '
                              'corresponding source file exists'))
    parser.add_argument('-O', action='count', dest='optimize', default=0,
                        help='Optimize bytecode files (-OO remove docstrings)')
    parser.add_argument('srcdir', metavar='SRCDIR',
                        help='Source directory containing .py files')
    parser.add_argument('dstdir', metavar='DESTDIR',
                        help='Destination directory to put .pyc files')
    args = parser.parse_args()

    if args.verbose < 0:
        level = logging.ERROR
    elif args.verbose == 0:
        level = logging.WARN
    elif args.verbose == 1:
        level = logging.INFO
    else:
        level = logging.DEBUG

    logging.basicConfig(format='%(message)s', level=level)
    compile_sync(args.srcdir, args.dstdir, codedir=args.codedir, force=args.force, delete=args.delete, optimize=args.optimize)

if __name__ == '__main__':
    main()
