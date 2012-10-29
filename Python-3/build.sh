#!/bin/sh


#
# How Python builds:
#
# 1. PGen is compiled.  This is a utility the generates some sort of parsing 
#    code.
# 2. Bootstrap python is compiled.  This is a bare-bones python used to run 
#    setup.py
# 3. libpython is compiled.
# 4. sharedmodules is compiled.
# 5. Items are installed to prefix.
#
# During a cross compile, the pgen and bootstrap python for the host are
# required in order to make items required on the target.
#
# Note: Doing a 'make clean' or 'make distclean' causes the make files to 
# perform a 'find . ... exec rm -f'  which will erase build outputs.  For this
# reason, we remove write/execute permissions on the build outputs as we
# complete them.
#
# Set this variant to the stage, or SDK home

clean()
{
	SEARCHDIRS="Doc Grammar Include Lib Mac Misc Modules Objects Parser PC PCBuild Python Tools"
	find ${SEARCHDIRS} -name '*.[oa]' -delete
	find ${SEARCHDIRS} -name '*.s[ol]' -delete
	find ${SEARCHDIRS} -name '*.so.[0-9]*.[0-9]*' -delete
	find ${SEARCHDIRS} -name '*.gc??' -delete
	find ${SEARCHDIRS} -name '__pycache__' -exec rm -rf {} \;
	rm -f Lib/lib2to3/*Grammar*.pickle
	rm -f python Parser/pgen libpython*.so* libpython*.a \
	      tags TAGS Parser/pgen.stamp \
	      config.cache config.log pyconfig.h Modules/config.c
	rm -rf build platform
	rm -rf Lib/test/data
	rm -f core Makefile Makefile.pre config.status \
	      Modules/Setup Modules/Setup.local Modules/Setup.config \
	      Modules/ld_so_aix Modules/python.exp Misc/python.pc
	rm -f python*-gdb.py
	rm -f pybuilddir.txt
}

for TARGET in host nto-x86 nto-armv7; do
echo "************************************************************************"
echo " Building for ${TARGET}..."
echo "************************************************************************"
	clean
	./${TARGET}.build
	if [ $? != 0 ]; then 
		echo "Build failed!"
		break
	fi
done

