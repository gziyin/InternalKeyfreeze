@echo off
setlocal
set "ROOT=%~dp0"
for %%I in ("%ROOT%..") do set "ROOT=%%~fI"
set "ROOT=%ROOT%\"
set "ASSETS=%ROOT%assets"
set "SITE=%ROOT%.workbuddy\iconbuild\site"

set "PY="
where python >nul 2>nul && set "PY=python"
if not defined PY ( where python3 >nul 2>nul && set "PY=python3" )
if not defined PY ( where py >nul 2>nul && set "PY=py -3" )
if not defined PY ( echo [ERR] Python 3 not found on PATH. & exit /b 1 )

if not exist "%SITE%\PIL" (
    echo [*] Installing Pillow into %SITE% ...
    %PY% -m pip install --target "%SITE%" Pillow 2>&1 || exit /b 1
)

set "IKF_SITE=%SITE%"
set "IKF_ASSETS=%ASSETS%"
%PY% "%~dp0gen_icons.py"
if errorlevel 1 ( echo [ERR] icon generation failed. & exit /b 1 )
echo [OK] icons regenerated in %ASSETS%
