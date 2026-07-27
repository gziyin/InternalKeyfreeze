@echo off
setlocal
cd /d "%~dp0.."

echo ============================================================
echo   InternalKeyfreeze build
echo ============================================================

echo [1/3] Generating icon assets (tools\build_icons.bat) ...
call "%~dp0build_icons.bat"
if errorlevel 1 ( echo [ERR] icon generation failed. & exit /b 1 )

echo [2/3] Compiling resource script (windres) ...
if not exist build mkdir build
windres --output-format=coff -i src\InternalKeyfreeze.rc -o build\InternalKeyfreeze.res.o
if errorlevel 1 ( echo [ERR] windres failed. & exit /b 1 )

echo [3/3] Linking executable (g++) ...
g++ -O2 -municode -mwindows src\InternalKeyfreeze.cpp build\InternalKeyfreeze.res.o -o bin\InternalKeyfreeze.exe
if errorlevel 1 ( echo [ERR] g++ failed. & exit /b 1 )

echo.
echo [OK] Build complete: bin\InternalKeyfreeze.exe
exit /b 0
