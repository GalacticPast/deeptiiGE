#!/bin/bash
# Build script for rebuilding everything
set echo on

echo "cleaning everything..."


# pushd engine
# source build.sh
# popd
make -f Makefile.engine.linux.mak clean 

ERRORLEVEL=$?
if [ $ERRORLEVEL -ne 0 ]
then
echo "Error:"$ERRORLEVEL && exit
fi

# pushd testbed
# source build.sh
# popd

make -f Makefile.testbed.linux.mak clean 
ERRORLEVEL=$?
if [ $ERRORLEVEL -ne 0 ]
then
echo "Error:"$ERRORLEVEL && exit
fi

find obj/* -name '*.o' -type f 
find obj/* -name '*.o' -type f -delete 

ERROELEVEL=$?
if [ $ERRORLEVEL -ne 0 ]
then
echo "Error:"$ERRORLEVEL && exit
fi

echo "All assemblies cleaned successfully."

