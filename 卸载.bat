@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
title InternalKeyfreeze Uninstaller

REM Request administrator privileges if not already elevated
>nul 2>&1 net session
if %errorlevel% neq 0 (
    echo Requesting administrator privileges...
    echo If no UAC prompt appears, right-click this file and select "Run as administrator".
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    if !errorlevel! neq 0 (
        echo.
        echo [Error] Failed to request admin rights, code: !errorlevel!
        echo Please right-click this file and select "Run as administrator".
        echo.
        pause
        exit /b 1
    )
    echo Administrator window launched. This window will close.
    timeout /t 2 /nobreak >nul
    exit /b
)

cd /d "%~dp0"
if !errorlevel! neq 0 (
    echo [Error] Cannot switch to script directory: %~dp0
    pause
    exit /b 1
)

echo ================================================================
echo            InternalKeyfreeze Uninstaller
echo ================================================================
echo.
echo  Current directory: %CD%
echo.
echo  Will perform the following:
echo    1. Stop running InternalKeyfreeze
echo    2. Uninstall Interception driver
echo    3. Delete C:\Program Files\InternalKeyfreeze
echo    4. Delete desktop and start menu shortcuts
echo    5. Remove startup registry entry
echo    6. Clean user data (logs, etc.)
echo.
echo  A reboot is required after uninstall to fully remove the driver.
echo.
echo  Press any key to continue uninstalling...
pause >nul

echo.
echo  [1/6] Stopping InternalKeyfreeze...
taskkill /F /IM InternalKeyfreeze.exe 2>nul
if !errorlevel! equ 0 (echo  Process stopped) else (echo  No running process found, skipped)

echo.
echo  [2/6] Uninstalling driver...
set "DRIVER_DIR=C:\Program Files\InternalKeyfreeze\driver"
if exist "%DRIVER_DIR%\install-interception.exe" (
    echo  Running: "%DRIVER_DIR%\install-interception.exe" /uninstall
    "%DRIVER_DIR%\install-interception.exe" /uninstall
    if !errorlevel! equ 0 (echo  Driver uninstalled) else (echo  [Warning] Driver returned non-zero code: !errorlevel!)
) else if exist "driver\install-interception.exe" (
    echo  Running: "driver\install-interception.exe" /uninstall
    "driver\install-interception.exe" /uninstall
    if !errorlevel! equ 0 (echo  Driver uninstalled) else (echo  [Warning] Driver returned non-zero code: !errorlevel!)
) else (
    echo  [Warning] install-interception.exe not found, skipped
)

echo.
echo  [3/6] Deleting program files...
set "DEST=C:\Program Files\InternalKeyfreeze"
if exist "%DEST%" (
    rmdir /S /Q "%DEST%"
    if !errorlevel! equ 0 (echo  Deleted %DEST%) else (echo  [Warning] Directory in use, please delete manually after reboot)
) else (
    echo  Directory does not exist, skipped
)

echo.
echo  [4/6] Deleting shortcuts...
set "DESKTOP_LNK=%USERPROFILE%\Desktop\InternalKeyfreeze.lnk"
set "STARTMENU_LNK=%APPDATA%\Microsoft\Windows\Start Menu\Programs\InternalKeyfreeze.lnk"
if exist "%DESKTOP_LNK%" (
    del "%DESKTOP_LNK%"
    echo  Deleted desktop shortcut
) else (
    echo  Desktop shortcut not found, skipped
)
if exist "%STARTMENU_LNK%" (
    del "%STARTMENU_LNK%"
    echo  Deleted start menu shortcut
) else (
    echo  Start menu shortcut not found, skipped
)

echo.
echo  [5/6] Removing startup registry entry...
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v InternalKeyfreeze /f >nul 2>&1
if !errorlevel! equ 0 (
    echo  Startup entry removed
) else (
    echo  No startup entry found, skipped
)

echo.
echo  [6/6] Cleaning user data...
if exist "%APPDATA%\InternalKeyfreeze" (
    rmdir /S /Q "%APPDATA%\InternalKeyfreeze" 2>nul
    echo  Cleaned %APPDATA%\InternalKeyfreeze
) else (
    echo  %APPDATA%\InternalKeyfreeze not found, skipped
)
if exist "%LOCALAPPDATA%\InternalKeyfreeze" (
    rmdir /S /Q "%LOCALAPPDATA%\InternalKeyfreeze" 2>nul
    echo  Cleaned %LOCALAPPDATA%\InternalKeyfreeze
) else (
    echo  %LOCALAPPDATA%\InternalKeyfreeze not found, skipped
)

echo.
echo  ============================================
echo    Uninstall complete. Please reboot your PC.
echo  ============================================
echo.
echo  Press any key to exit...
pause >nul
endlocal
