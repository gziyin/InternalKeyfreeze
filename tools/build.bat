@echo off
setlocal
cd /d "%~dp0.."

echo ============================================================
echo   InternalKeyfreeze build
echo ============================================================

echo [1/4] Generating icon assets (tools\build_icons.bat) ...
call "%~dp0build_icons.bat"
if errorlevel 1 ( echo [ERR] icon generation failed. & exit /b 1 )

echo [2/4] Compiling resource script (windres) ...
if not exist build mkdir build
windres --output-format=coff -I src\resources -i src\resources\InternalKeyfreeze.rc -o build\InternalKeyfreeze.res.o
if errorlevel 1 ( echo [ERR] windres failed. & exit /b 1 )

echo [3/4] Compiling source modules (g++) ...
g++ -O2 -municode -mwindows -std=c++17 -finput-charset=UTF-8 -fexec-charset=UTF-8 ^
    -I sdk\library -I src\app -I src\resources ^
    -c src\app\main.cpp ^
       src\app\tray_app.cpp ^
       src\app\keyboard_filter.cpp ^
       src\app\interception_loader.cpp ^
       src\app\config.cpp
if errorlevel 1 ( echo [ERR] g++ compile failed. & exit /b 1 )

echo [4/4] Linking executable (g++) ...
if not exist bin mkdir bin
g++ -O2 -municode -mwindows ^
    main.o tray_app.o keyboard_filter.o interception_loader.o config.o ^
    build\InternalKeyfreeze.res.o ^
    -o bin\InternalKeyfreeze.exe
if errorlevel 1 ( echo [ERR] g++ link failed. & exit /b 1 )

del *.o 2>nul

echo.
echo [OK] Build complete: bin\InternalKeyfreeze.exe
exit /b 0
