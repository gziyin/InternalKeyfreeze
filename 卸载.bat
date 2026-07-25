@echo off
setlocal enabledelayedexpansion
title InternalKeyfreeze 卸载程序

>nul 2>&1 net session
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"

echo ================================================================
echo            InternalKeyfreeze 卸载程序
echo ================================================================
echo.
echo  即将执行：
echo    1. 结束正在运行的 InternalKeyfreeze
echo    2. 卸载 Interception 驱动
echo    3. 删除 C:\Program Files\InternalKeyfreeze
echo    4. 删除快捷方式
echo.
echo  卸载完成后需要重启电脑。
echo.
echo  确认卸载？(Ctrl+C 取消)
pause

echo.
echo  [1/4] 结束 InternalKeyfreeze 进程...
taskkill /F /IM InternalKeyfreeze.exe 2>nul
if %errorlevel% equ 0 (echo 已结束。) else (echo 未在运行，跳过。)

echo.
echo  [2/4] 卸载驱动...
set "DRIVER_DIR=C:\Program Files\InternalKeyfreeze\driver"
if exist "%DRIVER_DIR%\install-interception.exe" (
    "%DRIVER_DIR%\install-interception.exe" /uninstall
    if !errorlevel! equ 0 (echo 驱动已卸载。) else (echo [警告] 非零退出码。)
) else if exist "driver\install-interception.exe" (
    "driver\install-interception.exe" /uninstall
    if !errorlevel! equ 0 (echo 驱动已卸载。) else (echo [警告] 非零退出码。)
) else (
    echo [警告] 找不到 install-interception.exe，跳过。
)

echo.
echo  [3/4] 删除程序文件...
set "DEST=C:\Program Files\InternalKeyfreeze"
if exist "%DEST%" (
    rmdir /S /Q "%DEST%"
    if !errorlevel! equ 0 (echo 已删除 %DEST%) else (echo [警告] 部分文件被占用，请重启后手动删除。)
) else (
    echo 目录不存在，跳过。
)

echo.
echo  [4/4] 删除快捷方式...
powershell -NoProfile -Command "$ws = New-Object -ComObject WScript.Shell; $d = [Environment]::GetFolderPath('Desktop'); $p = [Environment]::GetFolderPath('Programs'); foreach($lnk in @("$d\InternalKeyfreeze.lnk","$p\InternalKeyfreeze.lnk")){ if(Test-Path $lnk){Remove-Item $lnk -Force; Write-Host "Removed: $lnk"} }"

echo.
echo  ============================================
echo    卸载完成！请重启电脑彻底移除驱动。
echo  ============================================
echo.
pause
endlocal
