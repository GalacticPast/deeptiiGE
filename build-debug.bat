@ECHO OFF
REM Build Everything

ECHO "Building everything..."

make -f "Makefile.library.mak" all
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

make -f "Makefile.executable.mak" all
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)


ECHO "All assemblies built successfully."
