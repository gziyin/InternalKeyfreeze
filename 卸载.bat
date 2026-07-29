@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
title InternalKeyfreeze 卸载程序

REM 自动提权：未以管理员身份运行时，重新以管理员身份启动自身
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
echo  将依次执行：
echo    1. 结束正在运行的 InternalKeyfreeze
echo    2. 卸载 Interception 驱动
echo    3. 删除 C:\Program Files\InternalKeyfreeze
echo    4. 删除桌面与开始菜单快捷方式
echo    5. 清理开机自启注册表项
echo    6. 清理用户数据（日志等）
echo.
echo  卸载完成后需要重启电脑才会彻底生效。
echo.
echo  确定卸载？(Ctrl+C 取消)
pause

echo.
echo  [1/6] 结束 InternalKeyfreeze 进程...
taskkill /F /IM InternalKeyfreeze.exe 2>nul
if %errorlevel% equ 0 (echo  已结束进程) else (echo  未检测到运行中的进程，跳过)

echo.
echo  [2/6] 卸载驱动...
set "DRIVER_DIR=C:\Program Files\InternalKeyfreeze\driver"
if exist "%DRIVER_DIR%\install-interception.exe" (
    "%DRIVER_DIR%\install-interception.exe" /uninstall
    if !errorlevel! equ 0 (echo  驱动已卸载。) else (echo  [警告] 驱动返回非零退出码。)
) else if exist "driver\install-interception.exe" (
    "driver\install-interception.exe" /uninstall
    if !errorlevel! equ 0 (echo  驱动已卸载。) else (echo  [警告] 驱动返回非零退出码。)
) else (
    echo  [警告] 找不到 install-interception.exe，跳过驱动卸载。
)

echo.
echo  [3/6] 删除程序文件...
set "DEST=C:\Program Files\InternalKeyfreeze"
if exist "%DEST%" (
    rmdir /S /Q "%DEST%"
    if !errorlevel! equ 0 (echo  已删除 %DEST%) else (echo  [警告] 目录被占用，请重启后手动删除。)
) else (
    echo  目录不存在，跳过。
)

echo.
echo  [4/6] 删除快捷方式...
set "DESKTOP_LNK=%USERPROFILE%\Desktop\InternalKeyfreeze.lnk"
set "STARTMENU_LNK=%APPDATA%\Microsoft\Windows\Start Menu\Programs\InternalKeyfreeze.lnk"
if exist "%DESKTOP_LNK%" (
    del "%DESKTOP_LNK%"
    echo  已删除桌面快捷方式
) else (
    echo  桌面快捷方式不存在，跳过
)
if exist "%STARTMENU_LNK%" (
    del "%STARTMENU_LNK%"
    echo  已删除开始菜单快捷方式
) else (
    echo  开始菜单快捷方式不存在，跳过
)

echo.
echo  [5/6] 清理开机自启注册表项...
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v InternalKeyfreeze /f >nul 2>&1
if !errorlevel! equ 0 (
    echo  已移除开机自启注册项
) else (
    echo  无自启注册项，跳过
)

echo.
echo  [6/6] 清理用户数据（日志等）...
if exist "%APPDATA%\InternalKeyfreeze" (
    rmdir /S /Q "%APPDATA%\InternalKeyfreeze" 2>nul
    echo  已清理 %APPDATA%\InternalKeyfreeze
) else (
    echo  %APPDATA%\InternalKeyfreeze 不存在，跳过
)
if exist "%LOCALAPPDATA%\InternalKeyfreeze" (
    rmdir /S /Q "%LOCALAPPDATA%\InternalKeyfreeze" 2>nul
    echo  已清理 %LOCALAPPDATA%\InternalKeyfreeze
) else (
    echo  %LOCALAPPDATA%\InternalKeyfreeze 不存在，跳过
)

echo.
echo  ============================================
echo    卸载完成，请重启电脑以彻底移除驱动。
echo  ============================================
echo.
pause
endlocal
