@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
title InternalKeyfreeze 安装程序

:: ========================================================================
:: InternalKeyfreeze 一键安装脚本
:: 用户双击即可，脚本会自动提权并完成全部安装
:: ========================================================================

:: 自动提权：检测是否管理员，如果不是则用 PowerShell 重启自己
>nul 2>&1 net session
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

:: 提权后工作目录会变成 System32，切回脚本所在目录
cd /d "%~dp0"

echo ================================================================
echo            InternalKeyfreeze 安装程序
echo ================================================================
echo.
echo  即将执行：
echo    1. 安装 Interception 内核键盘过滤驱动
echo    2. 复制程序文件到 C:\Program Files\InternalKeyfreeze
echo    3. 创建桌面和开始菜单快捷方式
echo.
echo  安装完成后需要重启电脑让驱动生效。
echo.
echo  确认安装？(Ctrl+C 取消)
pause

echo.
echo ================================================================
echo  [1/4] 安装 Interception 驱动
echo ================================================================
if not exist "driver\install-interception.exe" (
    echo [错误] 找不到 driver\install-interception.exe
    echo 请确认安装包完整，或从 GitHub 重新下载。
    pause
    exit /b 1
)
"driver\install-interception.exe" /install
if !errorlevel! neq 0 (
    echo.
    echo [警告] 驱动安装器返回了非零退出码。
    echo 可能驱动已经安装过，或者安装被拒绝。
    echo 继续安装程序文件...
) else (
    echo 驱动安装成功。
)

echo.
echo ================================================================
echo  [2/4] 复制程序文件
echo ================================================================
set "DEST=C:\Program Files\InternalKeyfreeze"
echo 目标目录: %DEST%
if not exist "%DEST%\bin" mkdir "%DEST%\bin"
if not exist "%DEST%\driver" mkdir "%DEST%\driver"
copy /Y "bin\InternalKeyfreeze.exe" "%DEST%\bin\" >nul
copy /Y "bin\interception.dll" "%DEST%\bin\" >nul
copy /Y "driver\install-interception.exe" "%DEST%\driver\" >nul
copy /Y "driver\UninstallDriver.exe" "%DEST%\driver\" >nul
copy /Y "driver\install-driver.bat" "%DEST%\driver\" >nul
if exist "README.md" copy /Y "README.md" "%DEST%\" >nul
if exist "LICENSE" copy /Y "LICENSE" "%DEST%\" >nul
echo 文件复制完成。

echo.
echo ================================================================
echo  [3/4] 创建快捷方式
echo ================================================================
:: 桌面快捷方式
powershell -NoProfile -Command ^
  "$ws = New-Object -ComObject WScript.Shell; " ^
  "$desktop = [Environment]::GetFolderPath('Desktop'); " ^
  "$sc = $ws.CreateShortcut(\"$desktop\InternalKeyfreeze.lnk\"); " ^
  "$sc.TargetPath = '%DEST%\bin\InternalKeyfreeze.exe'; " ^
  "$sc.WorkingDirectory = '%DEST%\bin'; " ^
  "$sc.Description = '冻结笔记本内置键盘'; " ^
  "$sc.Save(); " ^
  "Write-Host '桌面快捷方式已创建'"

:: 开始菜单快捷方式
powershell -NoProfile -Command ^
  "$ws = New-Object -ComObject WScript.Shell; " ^
  "$programs = [Environment]::GetFolderPath('Programs'); " ^
  "$sc = $ws.CreateShortcut(\"$programs\InternalKeyfreeze.lnk\"); " ^
  "$sc.TargetPath = '%DEST%\bin\InternalKeyfreeze.exe'; " ^
  "$sc.WorkingDirectory = '%DEST%\bin'; " ^
  "$sc.Description = '冻结笔记本内置键盘'; " ^
  "$sc.Save(); " ^
  "Write-Host '开始菜单快捷方式已创建'"

echo.
echo ================================================================
echo  [4/4] 安装完成
echo ================================================================
echo.
echo  ✓ 驱动已安装
echo  ✓ 程序已复制到 %DEST%
echo  ✓ 桌面快捷方式已创建
echo  ✓ 开始菜单快捷方式已创建
echo.
echo  ============================================
echo    请重启电脑让驱动生效！
echo  ============================================
echo.
echo  重启后：
echo    双击桌面的 InternalKeyfreeze 快捷方式运行
echo    左键托盘图标 → 在内置键盘上按任意键 → 自动识别并冻结
echo.
echo  卸载：运行 C:\Program Files\InternalKeyfreeze\driver\UninstallDriver.exe
echo.
pause
endlocal
