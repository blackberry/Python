#/bin/sh

WORKDIR="${1%%/}"
PWD=$(pwd)

# Remove bins not needed for runtime
rm -f ${WORKDIR}/bin/2to3*
rm -f ${WORKDIR}/bin/idle3*
rm -f ${WORKDIR}/bin/pydoc3*
rm -f ${WORKDIR}/bin/python3
rm -f ${WORKDIR}/bin/python3.2m
rm -f ${WORKDIR}/bin/python3-config

# Remove all headers except pyconfig.h (needed for runtime)
find "${WORKDIR}/include/python3.2m/" -d -name "*.h" -not -name "pyconfig.h" -exec rm -f {} \;

# bye-bye man pages
rm -rf "${WORKDIR}/share"

# Remove pkgconfig
rm -rf "${WORKDIR}/lib/pkgconfig"

# Remove all test modules
find "${WORKDIR}/lib/python3.2/" -d -name "test*" -type d -exec rm -rf {} \;

# Prune unwanted modules
rm -rf "${WORKDIR}/lib/python3.2/distutils"
rm -rf "${WORKDIR}/lib/python3.2/tkinter"
rm -rf "${WORKDIR}/lib/python3.2/lib2to3"
rm -rf "${WORKDIR}/lib/python3.2/turtledemo"
rm -rf "${WORKDIR}/lib/python3.2/idlelib"

# This guy is large.  Remove
rm -f "${WORKDIR}/lib/python3.2/config-3.2m/libpython3.2m.a"

PYTHONHOME=./host ./host/bin/python3.2 compile_sync.py -f -v -d /usr/lib/python3.2 -OO "${WORKDIR}/lib/python3.2" "${WORKDIR}/lib/python3.2"

# Delete __pycache__ directories
find ${WORKDIR}/lib -d -name "__pycache__" -exec rm -rf {} \;

# Delete .py files
find ${WORKDIR}/lib -d -name "*.py" -exec rm -f {} \;
