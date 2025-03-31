@ECHO OFF

ECHO "Building everything..."

make -f "Makefile.library.mak" all
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

make -f "Makefile.executable.mak" all
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

make -f "Makefile.tests.mak" all
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

CALL post_build.bat

ECHO "All assemblies built successfully."
