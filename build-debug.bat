@ECHO OFF

ECHO "Building everything..."

make -f "Makefile.library.mak" all
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

cd ~/Documents/projects/deeptige/

make -f "Makefile.executable.mak" all
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)


ECHO "All assemblies built successfully."
