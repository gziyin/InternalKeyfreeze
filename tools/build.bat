@echo off
setlocal
cd /d "%~dp0.."

echo ============================================================
echo   InternalKeyfreeze build
echo ============================================================

REM --- Step 1: Icon generation ---
REM Skip in CI environment or when Python is not available.
if defined CI goto :skip_icons
where python >nul 2>nul
if not errorlevel 1 goto :do_icons
where python3 >nul 2>nul
if not errorlevel 1 goto :do_icons
where py >nul 2>nul
if not errorlevel 1 goto :do_icons
echo [1/4] Python not found - using existing icon assets
goto :skip_icons

:do_icons
echo [1/4] Generating icon assets ...
call "%~dp0build_icons.bat"
if errorlevel 1 ( echo [ERR] icon generation failed. & exit /b 1 )

:skip_icons
if defined CI echo [1/4] CI mode - using committed icon assets

REM --- Step 2: Compile resource script ---
echo [2/4] Compiling resource script ...
where windres
if not exist build mkdir build
windres --output-format=coff -I src\resources -i src\resources\InternalKeyfreeze.rc -o build\InternalKeyfreeze.res.o > build\windres.log 2>&1
type build\windres.log
if errorlevel 1 (
    echo [ERR] windres failed with errorlevel %errorlevel%
    echo CWD: %CD%
    exit /b 1
)

REM --- Step 3: Compile source modules ---
echo [3/4] Compiling source modules ...
g++ -O2 -municode -mwindows -std=c++17 -finput-charset=UTF-8 -fexec-charset=UTF-8 ^
    -I sdk\library -I src\app -I src\resources ^
    -c src\app\main.cpp ^
       src\app\tray_app.cpp ^
       src\app\keyboard_filter.cpp ^
       src\app\interception_loader.cpp ^
       src\app\config.cpp ^
       src\app\hotkey.cpp ^
       src\app\autostart.cpp
if errorlevel 1 ( echo [ERR] g++ compile failed. & exit /b 1 )

REM --- Step 4: Link executable ---
echo [4/4] Linking executable ...
if not exist bin mkdir bin
g++ -O2 -municode -mwindows ^
    main.o tray_app.o keyboard_filter.o interception_loader.o config.o hotkey.o autostart.o ^
    build\InternalKeyfreeze.res.o ^
    -o bin\InternalKeyfreeze.exe
if errorlevel 1 ( echo [ERR] g++ link failed. & exit /b 1 )

del *.o 2>nul

echo.
echo [OK] Build complete: bin\InternalKeyfreeze.exe
exit /b 0
