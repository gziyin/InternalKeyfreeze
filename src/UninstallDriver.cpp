// ============================================================================
// UninstallDriver.cpp
//
// Uninstalls the Interception keyboard filter driver. Companion tool of
// InternalKeyfreeze - lives in the driver\ folder next to the official
// install-interception.exe.
//
// What it does:
//   1. Asks for confirmation.
//   2. Terminates any running InternalKeyfreeze.exe - it holds open handles
//      to the driver, which would make the uninstall fail.
//   3. Runs "install-interception.exe /uninstall" (the OFFICIAL installer -
//      the same exe that installs, just with a different argument) and waits
//      for it. Its console window stays visible so you can read its output.
//   4. Reports the result and offers to reboot immediately (no force flag,
//      so apps with unsaved work can still abort the reboot).
//
// The embedded manifest requests requireAdministrator, so Windows shows the
// UAC prompt automatically on launch.
//
// BUILD (Visual Studio developer prompt):
//   cl /EHsc /W4 UninstallDriver.cpp ^
//      /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTFILE:UninstallDriver.manifest
// BUILD (MinGW-w64, run from this src\ folder):
//   printf '1 24 "UninstallDriver.manifest"\n' > ud.rc
//   windres ud.rc -O coff -o ud.res
//   g++ -O2 -municode -mwindows UninstallDriver.cpp ud.res -o ..\driver\UninstallDriver.exe
//   del ud.rc ud.res
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <strsafe.h>

// Terminate every running process whose image name matches (case-insensitive).
// Returns how many processes were terminated.
static int KillProcessesByName(PCWSTR image_name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    int killed = 0;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, image_name) == 0) {
                HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (proc) {
                    if (TerminateProcess(proc, 0))
                        killed++;
                    CloseHandle(proc);
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return killed;
}

static bool EnableShutdownPrivilege() {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return false;
    LUID luid;
    bool ok = LookupPrivilegeValueW(NULL, SE_SHUTDOWN_NAME, &luid);
    if (ok) {
        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        ok = AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL) &&
             GetLastError() == ERROR_SUCCESS;
    }
    CloseHandle(token);
    return ok;
}

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance,
                      _In_ LPWSTR cmd_line, _In_ int cmd_show) {
    UNREFERENCED_PARAMETER(instance);
    UNREFERENCED_PARAMETER(prev_instance);
    UNREFERENCED_PARAMETER(cmd_line);
    UNREFERENCED_PARAMETER(cmd_show);

    if (MessageBoxW(NULL,
            L"这将卸载 Interception 键盘过滤驱动。\n\n"
            L"卸载后 InternalKeyfreeze 将无法冻结内置键盘\n"
            L"（程序本体和配置文件不会被删除）。\n"
            L"卸载完成后需要重启电脑才会彻底生效。\n\n"
            L"确定继续吗？",
            L"卸载 Interception 驱动",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
        return 0;

    // 1. The tray app holds driver handles - close it first.
    int killed = KillProcessesByName(L"InternalKeyfreeze.exe");

    // 2. Locate the official installer next to this exe.
    WCHAR dir[MAX_PATH];
    GetModuleFileNameW(NULL, dir, ARRAYSIZE(dir));
    WCHAR* slash = wcsrchr(dir, L'\\');
    if (slash)
        *slash = 0;

    WCHAR installer[MAX_PATH];
    StringCchPrintfW(installer, ARRAYSIZE(installer),
                     L"%s\\install-interception.exe", dir);
    if (GetFileAttributesW(installer) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL,
            L"未找到 install-interception.exe。\n"
            L"它应与本程序（UninstallDriver.exe）在同一目录。",
            L"卸载失败", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 3. Run it with /uninstall and wait. It is a console app launched from
    //    a GUI app, so it gets its own console window showing official output.
    WCHAR cmdline[MAX_PATH + 16];
    StringCchPrintfW(cmdline, ARRAYSIZE(cmdline), L"\"%s\" /uninstall", installer);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, 0,
                        NULL, dir, &si, &pi)) {
        WCHAR err[256];
        StringCchPrintfW(err, ARRAYSIZE(err),
                         L"无法启动官方安装器（错误码 %lu）。", GetLastError());
        MessageBoxW(NULL, err, L"卸载失败", MB_OK | MB_ICONERROR);
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        WCHAR err[256];
        StringCchPrintfW(err, ARRAYSIZE(err),
            L"官方安装器返回了非零退出码（%lu），卸载可能未成功。\n"
            L"请查看它控制台窗口中的提示信息。", exit_code);
        MessageBoxW(NULL, err, L"卸载警告", MB_OK | MB_ICONWARNING);
        return 1;
    }

    // 4. Report and offer an immediate (non-forced) reboot.
    WCHAR msg[512];
    StringCchPrintfW(msg, ARRAYSIZE(msg),
        L"驱动卸载完成%s。\n\n"
        L"是否立即重启电脑？\n"
        L"（选“否”则稍后请手动重启；重启后卸载才彻底生效）",
        killed > 0 ? L"，并已结束正在运行的 InternalKeyfreeze" : L"");
    if (MessageBoxW(NULL, msg, L"卸载完成",
                    MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        if (!EnableShutdownPrivilege() ||
            !ExitWindowsEx(EWX_REBOOT, 0)) {   // no FORCE: apps may abort
            MessageBoxW(NULL, L"无法自动重启，请手动重启电脑。",
                        L"提示", MB_OK | MB_ICONINFORMATION);
        }
    }
    return 0;
}
