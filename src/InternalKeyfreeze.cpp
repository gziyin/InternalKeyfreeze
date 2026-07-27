// ============================================================================
// InternalKeyfreeze.cpp  (v2 - Interception driver edition)
//
// Freezes ONLY the laptop's built-in keyboard; external USB / Bluetooth
// keyboards keep working. Modeled after OpenKeyfreeze's tray UX.
//
// Why the Interception driver?
//   * WH_KEYBOARD_LL hooks cannot tell WHICH device a keystroke came from
//     (KBDLLHOOKSTRUCT has no device info).
//   * On many modern laptops the built-in keyboard is a "critical system
//     device" - Windows refuses to disable it (greyed out in Device Manager;
//     SetupAPI DICS_DISABLE fails). Verified on the target machine:
//     ACPI\MSFT0001 "PS/2 standard keyboard" -> "cannot disable critical
//     system device".
//   * Interception (https://github.com/oblitum/Interception) is a kernel
//     keyboard filter that reports the source device of every keystroke and
//     lets us swallow keys per-device at driver level.
//
// ONE-TIME SETUP:
//   1. Run  install-driver.bat  as administrator (installs the filter driver)
//   2. REBOOT Windows.
//   3. Keep interception.dll (x64) next to this exe.
//
// USE:
//   * First click on the tray icon asks you to press ANY key on the built-in
//     keyboard once -> its hardware id is learned and stored in
//     InternalKeyfreeze.ini. (Right-click menu can re-learn.)
//   * Left-click tray icon toggles freeze/unfreeze of the built-in keyboard.
//   * If the app exits or crashes the driver automatically resumes normal
//     input - the built-in keyboard can never be left dead. Ctrl+Alt+Del
//     always works (handled below the filter).
//
// BUILD (Visual Studio):
//   cl /EHsc /W4 InternalKeyfreeze.cpp /link /SUBSYSTEM:WINDOWS
// BUILD (MinGW-w64):
//   g++ -O2 -municode -mwindows InternalKeyfreeze.cpp -o InternalKeyfreeze.exe
// (interception.dll is loaded dynamically at runtime - nothing to link.)
// No administrator rights needed to RUN this app (only for driver install).
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include <atomic>
#include "resources.h"
#include "logger.h"

// Pull in constants from the Interception SDK header (INTERCEPTION_MAX_KEYBOARD etc.)
// INTERCEPTION_STATIC avoids __declspec(dllimport) since we load the DLL dynamically.
#define INTERCEPTION_STATIC
#include "../sdk/library/interception.h"

#define MAX_LOADSTRING   100
#define WM_TRAYICON      (WM_USER + 1)
#define WM_APP_LEARNED   (WM_USER + 2)
#define IDM_EXIT         1001
#define IDM_LEARN        1002
#define TRAY_UID         1
#define MAX_KBD_DEVICES  INTERCEPTION_MAX_KEYBOARD

// ---------------------------------------------------------------------------
// Interception ABI - mirrors interception.h, loaded dynamically so the app
// can show a friendly message when the dll/driver is missing.
// NOTE: the dll uses the C (__cdecl) calling convention - do NOT add WINAPI.
// ---------------------------------------------------------------------------
typedef void* IC_Context;
typedef int   IC_Device;
typedef int (*IC_Predicate)(IC_Device);

typedef struct {                     // InterceptionKeyStroke (first 8 bytes)
    unsigned short code;             // scan code
    unsigned short state;            // 0=down 1=up 2=E0 4=E1
    unsigned int   information;
} IC_KeyStroke;

typedef char IC_Stroke[20];          // InterceptionStroke = sizeof(MouseStroke)

#define IC_FILTER_KEY_ALL 0xFFFF

typedef IC_Context   (__cdecl *FN_create_context)(void);
typedef void         (__cdecl *FN_destroy_context)(IC_Context);
typedef void         (__cdecl *FN_set_filter)(IC_Context, IC_Predicate, unsigned short);
typedef int          (__cdecl *FN_is_keyboard)(IC_Device);
typedef IC_Device    (__cdecl *FN_wait_timeout)(IC_Context, unsigned long);
typedef int          (__cdecl *FN_receive)(IC_Context, IC_Device, void*, unsigned int);
typedef int          (__cdecl *FN_send)(IC_Context, IC_Device, const void*, unsigned int);
typedef unsigned int (__cdecl *FN_get_hwid)(IC_Context, IC_Device, void*, unsigned int);

static HMODULE g_dll = NULL;
static FN_create_context  p_create_context;
static FN_destroy_context p_destroy_context;
static FN_set_filter      p_set_filter;
static FN_is_keyboard     p_is_keyboard;
static FN_wait_timeout    p_wait_with_timeout;
static FN_receive         p_receive;
static FN_send            p_send;
static FN_get_hwid        p_get_hardware_id;

static bool LoadInterception() {
    g_dll = LoadLibraryW(L"interception.dll");
    if (!g_dll) {
        Log::Error(L"LoadLibrary(interception.dll) failed, err=%lu", GetLastError());
        return false;
    }
    p_create_context    = (FN_create_context) GetProcAddress(g_dll, "interception_create_context");
    p_destroy_context   = (FN_destroy_context)GetProcAddress(g_dll, "interception_destroy_context");
    p_set_filter        = (FN_set_filter)     GetProcAddress(g_dll, "interception_set_filter");
    p_is_keyboard       = (FN_is_keyboard)    GetProcAddress(g_dll, "interception_is_keyboard");
    p_wait_with_timeout = (FN_wait_timeout)   GetProcAddress(g_dll, "interception_wait_with_timeout");
    p_receive           = (FN_receive)        GetProcAddress(g_dll, "interception_receive");
    p_send              = (FN_send)           GetProcAddress(g_dll, "interception_send");
    p_get_hardware_id   = (FN_get_hwid)       GetProcAddress(g_dll, "interception_get_hardware_id");
    bool ok = p_create_context && p_destroy_context && p_set_filter && p_is_keyboard &&
           p_wait_with_timeout && p_receive && p_send && p_get_hardware_id;
    if (ok)
        Log::Info(L"interception.dll loaded, all 8 exports resolved");
    else
        Log::Error(L"interception.dll loaded but some exports missing");
    return ok;
}

// ---------------------------------------------------------------------------
// App state
// ---------------------------------------------------------------------------
HINSTANCE g_instance;
HWND      g_hwnd = NULL;
WCHAR     g_title[MAX_LOADSTRING];
WCHAR     g_window_class[MAX_LOADSTRING];
NOTIFYICONDATAW g_nid;
HICON  g_icon_enabled;     // built-in keyboard active
HICON  g_icon_disabled;    // built-in keyboard frozen
HANDLE g_mutex = NULL;

IC_Context     g_ctx = NULL;
HANDLE         g_worker = NULL;
std::atomic<bool> g_stop_worker{false};
std::atomic<bool> g_locked{false};      // swallow built-in keystrokes
std::atomic<bool> g_learning{false};    // next built-in keystroke identifies it

WCHAR g_internal_hwid[256] = L"";                 // learned built-in keyboard
WCHAR g_device_hwid[MAX_KBD_DEVICES + 1][256];    // per-device hwid cache

// Cached hardware id lookup (devices are stable for the process lifetime).
static PCWSTR DeviceHwid(IC_Device dev) {
    if (dev < 1 || dev > MAX_KBD_DEVICES) return L"";
    if (g_device_hwid[dev][0] == 0) {
        WCHAR buf[256] = {0};
        p_get_hardware_id(g_ctx, dev, buf, sizeof(buf) - sizeof(WCHAR));
        buf[255] = 0;   // the api does not guarantee NUL termination
        StringCchCopyW(g_device_hwid[dev], 256, buf);
    }
    return g_device_hwid[dev];
}

// ---------------------------------------------------------------------------
// Worker thread: the interception loop. Runs for the whole app lifetime.
// Unlocked  -> forward every keystroke (transparent pass-through).
// Locked    -> swallow keystrokes whose device matches the learned hwid.
// Learning  -> first keystroke from a non-USB device defines the built-in
//              keyboard (and is swallowed so it doesn't type anywhere).
// ---------------------------------------------------------------------------
static DWORD WINAPI WorkerProc(LPVOID) {
    while (!g_stop_worker) {
        IC_Device dev = p_wait_with_timeout(g_ctx, 200);
        if (dev == 0)
            continue;                       // timeout -> re-check stop flag

        IC_Stroke raw;
        if (p_receive(g_ctx, dev, &raw, 1) <= 0)
            continue;

        PCWSTR hwid = DeviceHwid(dev);

        if (g_learning) {
            if (hwid[0] && wcsstr(hwid, L"VID_") == NULL) {
                // Not USB -> treat as the built-in keyboard.
                StringCchCopyW(g_internal_hwid, 256, hwid);
                g_learning = false;
                Log::Info(L"Learning success: internal keyboard hwid=%s", hwid);
                PostMessageW(g_hwnd, WM_APP_LEARNED, 0, 0);
                continue;                   // swallow the learning keystroke
            }
            // User pressed an external keyboard: ignore & forward it.
        }

        bool is_internal = (g_internal_hwid[0] != 0 &&
                            _wcsicmp(hwid, g_internal_hwid) == 0);
        if (is_internal && g_locked)
            continue;                       // frozen: swallow the keystroke

        p_send(g_ctx, dev, &raw, 1);        // pass through untouched
    }
    return 0;
}

static bool StartWorker() {
    g_ctx = p_create_context();
    if (!g_ctx) {
        Log::Error(L"interception_create_context failed - driver not installed?");
        return false;
    }
    p_set_filter(g_ctx, p_is_keyboard, IC_FILTER_KEY_ALL);
    g_stop_worker = false;
    g_worker = CreateThread(NULL, 0, WorkerProc, NULL, 0, NULL);
    if (g_worker) {
        // The worker forwards every keystroke; give it a small scheduling
        // edge so input latency stays flat even under heavy system load.
        SetThreadPriority(g_worker, THREAD_PRIORITY_ABOVE_NORMAL);
        Log::Info(L"Worker thread started (priority ABOVE_NORMAL)");
    }
    return g_worker != NULL;
}

static void StopWorker() {
    g_stop_worker = true;
    if (g_worker) {
        WaitForSingleObject(g_worker, 3000);
        CloseHandle(g_worker);
        g_worker = NULL;
    }
    if (g_ctx) {
        p_destroy_context(g_ctx);           // driver auto-restores input
        g_ctx = NULL;
    }
    Log::Info(L"Worker stopped, driver context destroyed");
}

// ---------------------------------------------------------------------------
// Config: InternalKeyfreeze.ini next to the exe, [keyboard] hwid=...
// ---------------------------------------------------------------------------
static void ConfigPath(WCHAR* path, DWORD cch) {
    GetModuleFileNameW(NULL, path, cch);
    WCHAR* dot = wcsrchr(path, L'.');
    if (dot) *dot = 0;
    StringCchCatW(path, cch, L".ini");
}

static void LoadConfig() {
    WCHAR path[MAX_PATH];
    ConfigPath(path, ARRAYSIZE(path));
    GetPrivateProfileStringW(L"keyboard", L"hwid", L"",
                             g_internal_hwid, ARRAYSIZE(g_internal_hwid), path);
    if (g_internal_hwid[0])
        Log::Info(L"Config loaded: hwid=%s", g_internal_hwid);
    else
        Log::Info(L"Config loaded: no saved hwid (first run)");
}

static void SaveConfig() {
    WCHAR path[MAX_PATH];
    ConfigPath(path, ARRAYSIZE(path));
    WritePrivateProfileStringW(L"keyboard", L"hwid", g_internal_hwid, path);
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
static void UpdateTray() {
    g_nid.hIcon = g_locked ? g_icon_disabled : g_icon_enabled;
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
                   g_locked ? L"InternalKeyfreeze - 内置键盘已冻结（左键恢复）"
                            : L"InternalKeyfreeze - 内置键盘正常（左键冻结）");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void StartLearning(HWND hwnd) {
    g_learning = true;
    MessageBoxW(hwnd,
        L"进入识别模式：\n\n请在笔记本【内置键盘】上按任意一个键。\n"
        L"（在外置键盘上按键会被忽略。）\n\n"
        L"识别成功后内置键盘会立即被冻结，硬件 ID 会保存到\n"
        L"InternalKeyfreeze.ini，以后无需再次识别。",
        L"InternalKeyfreeze", MB_OK | MB_ICONINFORMATION);
}

static void ToggleInternalKeyboard(HWND hwnd) {
    if (!g_locked && g_internal_hwid[0] == 0) {
        Log::Info(L"Toggle requested but no hwid learned - entering learning mode");
        StartLearning(hwnd);                // first use: must learn first
        return;
    }
    g_locked = !g_locked;
    Log::Info(L"Keyboard %s", g_locked.load() ? L"FROZEN" : L"UNFROZEN");
    UpdateTray();
}

ATOM MyRegisterClass(HINSTANCE instance);
BOOL InitInstance(HINSTANCE instance, int cmd_show);
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance,
                      _In_ LPWSTR cmd_line, _In_ int cmd_show) {
    UNREFERENCED_PARAMETER(prev_instance);
    UNREFERENCED_PARAMETER(cmd_line);

    g_mutex = CreateMutexW(NULL, FALSE, L"InternalKeyfreezeMutex");
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
        return 1;
    }

    LoadConfig();

    wcscpy_s(g_title, MAX_LOADSTRING, L"InternalKeyfreeze");
    wcscpy_s(g_window_class, MAX_LOADSTRING, L"InternalKeyfreezeWindowClass");
    MyRegisterClass(instance);

    if (!InitInstance(instance, cmd_show))
        return FALSE;

    if (!StartWorker()) {
        MessageBoxW(NULL,
            L"Interception 驱动未安装（或尚未重启）。\n\n"
            L"请右键“以管理员身份运行” install-driver.bat，\n"
            L"然后【重启电脑】，再启动本程序。",
            L"InternalKeyfreeze", MB_OK | MB_ICONSTOP);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        ReleaseMutex(g_mutex); CloseHandle(g_mutex);
        return 1;
    }

    Log::Info(L"Entering message loop");
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    StopWorker();
    if (g_mutex) {
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
    }
    Log::Info(L"=== InternalKeyfreeze exiting normally ===");
    Log::Shutdown();
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE instance) {
    WNDCLASSEXW wcex;
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize        = sizeof(wcex);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.hInstance     = instance;
    wcex.hIcon         = LoadIcon(instance, MAKEINTRESOURCE(IDI_APP_ICON));
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_window_class;
    wcex.hIconSm       = LoadIcon(instance, MAKEINTRESOURCE(IDI_APP_ICON));
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE instance, int cmd_show) {
    UNREFERENCED_PARAMETER(cmd_show);
    g_instance = instance;

    g_hwnd = CreateWindowW(g_window_class, g_title, WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
                           nullptr, nullptr, instance, nullptr);
    if (!g_hwnd)
        return FALSE;   // hidden message-only window

    // Custom project icons (defined in resources.h / InternalKeyfreeze.rc)
    g_icon_enabled  = LoadIcon(instance, MAKEINTRESOURCE(IDI_TRAY_ENABLED)); // keyboard active
    g_icon_disabled = LoadIcon(instance, MAKEINTRESOURCE(IDI_TRAY_FROZEN));  // keyboard frozen

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = TRAY_UID;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_icon_enabled;
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
                   L"InternalKeyfreeze - 内置键盘正常（左键冻结）");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    ShowWindow(g_hwnd, SW_HIDE);
    UpdateWindow(g_hwnd);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_TRAYICON:
        if (l_param == WM_LBUTTONUP) {
            ToggleInternalKeyboard(hwnd);
        }
        else if (l_param == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, IDM_LEARN, L"重新识别内置键盘");
            AppendMenuW(menu, MF_STRING, IDM_EXIT,  L"退出");
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);   // required so the menu dismisses properly
            TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(w_param) == IDM_EXIT) {
            DestroyWindow(hwnd);
        }
        else if (LOWORD(w_param) == IDM_LEARN) {
            g_internal_hwid[0] = 0;
            SaveConfig();
            StartLearning(hwnd);
        }
        else {
            return DefWindowProc(hwnd, message, w_param, l_param);
        }
        break;

    case WM_APP_LEARNED:
        SaveConfig();
        g_locked = true;                    // freeze right after learning
        UpdateTray();
        MessageBoxW(hwnd,
            L"已识别内置键盘并立即冻结！\n\n"
            L"以后左键托盘图标即可在 冻结/恢复 之间切换。",
            L"InternalKeyfreeze", MB_OK | MB_ICONINFORMATION);
        break;

    case WM_DESTROY:
        // The driver releases the devices as soon as the context closes,
        // so normal input resumes automatically - no restore needed here.
        StopWorker();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, message, w_param, l_param);
    }
    return 0;
}
