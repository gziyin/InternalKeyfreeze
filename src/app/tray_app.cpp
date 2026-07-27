// ============================================================================
// tray_app.cpp
// ============================================================================

#include "tray_app.h"
#include "config.h"
#include "logger.h"
#include "../resources/resources.h"
#include <strsafe.h>

// ---- Global UI state ----
HINSTANCE       g_instance = NULL;
HWND           g_hwnd = NULL;
WCHAR          g_title[MAX_LOADSTRING];
WCHAR          g_window_class[MAX_LOADSTRING];
NOTIFYICONDATAW g_nid;
HICON          g_icon_enabled = NULL;
HICON          g_icon_disabled = NULL;

// ---- UI functions ----

void UpdateTray() {
    g_nid.hIcon = g_locked ? g_icon_disabled : g_icon_enabled;
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
                   g_locked ? L"InternalKeyfreeze - 内置键盘已冻结（左键恢复）"
                            : L"InternalKeyfreeze - 内置键盘正常（左键冻结）");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void StartLearning(HWND hwnd) {
    g_learning = true;
    MessageBoxW(hwnd,
        L"进入识别模式：\n\n请在笔记本【内置键盘】上按任意一个键。\n"
        L"（在外置键盘上按键会被忽略。）\n\n"
        L"识别成功后内置键盘会立即被冻结，硬件 ID 会保存到\n"
        L"InternalKeyfreeze.ini，以后无需再次识别。",
        L"InternalKeyfreeze", MB_OK | MB_ICONINFORMATION);
}

void ToggleInternalKeyboard(HWND hwnd) {
    if (!g_locked && g_internal_hwid[0] == 0) {
        Log::Info(L"Toggle requested but no hwid learned - entering learning mode");
        StartLearning(hwnd);                // first use: must learn first
        return;
    }
    g_locked = !g_locked;
    Log::Info(L"Keyboard %s", g_locked.load() ? L"FROZEN" : L"UNFROZEN");
    UpdateTray();
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
