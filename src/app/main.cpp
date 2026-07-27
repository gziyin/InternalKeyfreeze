// ============================================================================
// main.cpp
//
// Entry point and message loop for InternalKeyfreeze.
// Orchestrates the modules: loads the DLL, reads config, creates the
// tray window, starts the worker thread, and runs the message loop.
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "tray_app.h"
#include "keyboard_filter.h"
#include "interception_loader.h"
#include "config.h"
#include "logger.h"

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance,
                      _In_ LPWSTR cmd_line, _In_ int cmd_show) {
    UNREFERENCED_PARAMETER(prev_instance);
    UNREFERENCED_PARAMETER(cmd_line);

    HANDLE g_mutex = CreateMutexW(NULL, FALSE, L"InternalKeyfreezeMutex");
    if (g_mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"InternalKeyfreeze 已经在运行中。",
                    L"InternalKeyfreeze", MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }

    Log::Init();
    Log::Info(L"=== InternalKeyfreeze v2 starting ===");

    if (!LoadInterception()) {
        MessageBoxW(NULL,
            L"未找到 interception.dll（或导出函数不匹配）。\n"
            L"请把 Interception 开发包里 x64 版本的 interception.dll\n"
            L"放到本程序旁边。",
            L"InternalKeyfreeze", MB_OK | MB_ICONSTOP);
        ReleaseMutex(g_mutex); CloseHandle(g_mutex);
        Log::Shutdown();
        return 1;
    }

    LoadConfig();

    wcscpy_s(g_title, MAX_LOADSTRING, L"InternalKeyfreeze");
    wcscpy_s(g_window_class, MAX_LOADSTRING, L"InternalKeyfreezeWindowClass");
    MyRegisterClass(instance);

    if (!InitInstance(instance, cmd_show))
        return FALSE;

    if (!StartWorker(g_hwnd)) {
        MessageBoxW(NULL,
            L"Interception 驱动未安装（或尚未重启）。\n\n"
            L"请右键\"以管理员身份运行\" install-driver.bat，\n"
            L"然后【重启电脑】，再启动本程序。",
            L"InternalKeyfreeze", MB_OK | MB_ICONSTOP);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        ReleaseMutex(g_mutex); CloseHandle(g_mutex);
        ShutdownInterception();
        Log::Shutdown();
        return 1;
    }

    Log::Info(L"Entering message loop");
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    StopWorker();
    ShutdownInterception();
    if (g_mutex) {
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
    }
    Log::Info(L"=== InternalKeyfreeze exiting normally ===");
    Log::Shutdown();
    return (int)msg.wParam;
}
