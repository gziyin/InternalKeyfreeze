@echo off
setlocal enabledelayedexpansion
title InternalKeyfreeze 安装程序

>nul 2>&1 net session
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

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
echo  [1/4] 安装 Interception 驱动...
if not exist "driver\install-interception.exe" (
    echo [错误] 找不到 driver\install-interception.exe
    echo 请确认安装包完整，或从 GitHub 重新下载。
    pause
    exit /b 1
)
"driver\install-interception.exe" /install
if !errorlevel! neq 0 (
    echo [警告] 驱动安装器返回非零退出码，可能已安装过。
    echo 继续复制文件...
) else (
    echo 驱动安装成功。
)

echo.
echo  [2/4] 复制程序文件...
set "DEST=C:\Program Files\InternalKeyfreeze"
if not exist "%DEST%\bin" mkdir "%DEST%\bin"
if not exist "%DEST%\driver" mkdir "%DEST%\driver"
copy /Y "bin\InternalKeyfreeze.exe" "%DEST%\bin\" >nul
copy /Y "bin\interception.dll" "%DEST%\bin\" >nul
copy /Y "driver\install-interception.exe" "%DEST%\driver\" >nul
copy /Y "driver\UninstallDriver.exe" "%DEST%\driver\" >nul
copy /Y "driver\install-driver.bat" "%DEST%\driver\" >nul
if exist "README.md" copy /Y "README.md" "%DEST%\" >nul
if exist "LICENSE" copy /Y "LICENSE" "%DEST%\" >nul
echo 文件已复制到 %DEST%

echo.
echo  [3/4] 创建快捷方式...
:: 用 VBScript 创建快捷方式（比 PowerShell 转义更可靠）
set "VBS=%TEMP%\_icf_sc.vbs"
echo Set ws = CreateObject("WScript.Shell") > "%VBS%"
echo Set sc = ws.CreateShortcut(ws.SpecialFolders("Desktop") ^& "\InternalKeyfreeze.lnk") >> "%VBS%"
echo sc.TargetPath = "%DEST%\bin\InternalKeyfreeze.exe" >> "%VBS%"
echo sc.WorkingDirectory = "%DEST%\bin" >> "%VBS%"
echo sc.Description = "Freeze laptop built-in keyboard" >> "%VBS%"
echo sc.Save >> "%VBS%"
echo Set sc2 = ws.CreateShortcut(ws.SpecialFolders("Programs") ^& "\InternalKeyfreeze.lnk") >> "%VBS%"
echo sc2.TargetPath = "%DEST%\bin\InternalKeyfreeze.exe" >> "%VBS%"
echo sc2.WorkingDirectory = "%DEST%\bin" >> "%VBS%"
echo sc2.Description = "Freeze laptop built-in keyboard" >> "%VBS%"
echo sc2.Save >> "%VBS%"
cscript //nologo "%VBS%" >nul 2>&1
if !errorlevel! equ 0 (
    echo 桌面和开始菜单快捷方式已创建
) else (
    echo [警告] 快捷方式创建失败，请手动创建
)
del "%VBS%" >nul 2>&1

echo.
echo  [4/4] 安装完成！
echo.
echo  [OK] 驱动已安装
echo  [OK] 程序已复制到 %DEST%
echo  [OK] 桌面快捷方式已创建
echo  [OK] 开始菜单快捷方式已创建
echo.
echo  ============================================
echo    请重启电脑让驱动生效！
echo  ============================================
echo.
echo  重启后：双击桌面的 InternalKeyfreeze 快捷方式运行
echo  左键托盘图标 -^> 在内置键盘上按任意键 -^> 自动识别并冻结
echo.
echo  卸载：运行 卸载.bat 或 C:\Program Files\InternalKeyfreeze\driver\UninstallDriver.exe
echo.
pause
endlocal
