@ECHO OFF
::REM Clean Everything

ECHO "Cleaning everything..."

dir /s /b \obj 
rmdir /s obj 

ECHO "All assemblies cleaned successfully."
