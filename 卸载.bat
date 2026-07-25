@echo off
chcp 65001 >nul
setlocal
title InternalKeyfreeze 卸载程序

:: ========================================================================
:: InternalKeyfreeze 一键卸载脚本
:: 用户双击即可，脚本会自动提权并完成全部卸载
:: ========================================================================

:: 自动提权
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
echo    2. 卸载 Interception 内核驱动
echo    3. 删除程序文件 (C:\Program Files\InternalKeyfreeze)
echo    4. 删除桌面和开始菜单快捷方式
echo.
echo  卸载完成后需要重启电脑让驱动彻底移除。
echo.
echo  确认卸载？(Ctrl+C 取消)
pause

echo.
echo ================================================================
echo  [1/4] 结束正在运行的 InternalKeyfreeze
echo ================================================================
taskkill /F /IM InternalKeyfreeze.exe 2>nul
if %errorlevel% equ 0 (
    echo 已结束 InternalKeyfreeze 进程。
) else (
    echo InternalKeyfreeze 未在运行，跳过。
)

echo.
echo ================================================================
echo  [2/4] 卸载 Interception 驱动
echo ================================================================
set "DRIVER_DIR=C:\Program Files\InternalKeyfreeze\driver"
if exist "%DRIVER_DIR%\install-interception.exe" (
    "%DRIVER_DIR%\install-interception.exe" /uninstall
    if !errorlevel! equ 0 (
        echo 驱动卸载成功。
    ) else (
        echo [警告] 驱动卸载器返回非零退出码，可能驱动未安装或已卸载。
    )
) else if exist "driver\install-interception.exe" (
    "driver\install-interception.exe" /uninstall
    if !errorlevel! equ 0 (
        echo 驱动卸载成功。
    ) else (
        echo [警告] 驱动卸载器返回非零退出码。
    )
) else (
    echo [警告] 找不到 install-interception.exe，跳过驱动卸载。
    echo 如需手动卸载驱动，请从 GitHub 重新下载安装包。
)

echo.
echo ================================================================
echo  [3/4] 删除程序文件
echo ================================================================
set "DEST=C:\Program Files\InternalKeyfreeze"
if exist "%DEST%" (
    rmdir /S /Q "%DEST%"
    if !errorlevel! equ 0 (
        echo 已删除 %DEST%
    ) else (
        echo [警告] 部分文件可能被占用，未能完全删除。
        echo 请重启后手动删除 %DEST%
    )
) else (
    echo 程序目录不存在，跳过。
)

echo.
echo ================================================================
echo  [4/4] 删除快捷方式
echo ================================================================
powershell -NoProfile -Command ^
  "$ws = New-Object -ComObject WScript.Shell; " ^
  "$desktop = [Environment]::GetFolderPath('Desktop'); " ^
  "$programs = [Environment]::GetFolderPath('Programs'); " ^
  "$desktopLnk = \"$desktop\InternalKeyfreeze.lnk\"; " ^
  "$programsLnk = \"$programs\InternalKeyfreeze.lnk\"; " ^
  "if (Test-Path $desktopLnk) { Remove-Item $desktopLnk -Force; Write-Host '已删除桌面快捷方式' } else { Write-Host '桌面快捷方式不存在' }; " ^
  "if (Test-Path $programsLnk) { Remove-Item $programsLnk -Force; Write-Host '已删除开始菜单快捷方式' } else { Write-Host '开始菜单快捷方式不存在' }"

echo.
echo ================================================================
echo  卸载完成！
echo ================================================================
echo.
echo  ============================================
echo    请重启电脑让驱动彻底移除！
echo  ============================================
echo.
echo  重启后 InternalKeyfreeze 将完全从系统中移除。
echo.
pause
endlocal
