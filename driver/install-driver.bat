@echo off
rem One-time install of the Interception keyboard filter driver.
rem Right-click this file -> "Run as administrator", then REBOOT Windows.
"%~dp0install-interception.exe" /install
echo.
echo If you saw "Interception successfully installed", please REBOOT Windows now.
echo (To uninstall later, run UninstallDriver.exe in this same folder.)
pause
