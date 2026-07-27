// ============================================================================
// tray_app.cpp
// ============================================================================

#include "tray_app.h"
#include "config.h"
#include "logger.h"
#include "hotkey.h"
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
                   g_locked ? L"InternalKeyfreeze - \x5185\x7f6e\x952e\x76d8\x5df2\x51bb\x7ed3\xff08\x5de6\x952e\x6062\x590d\xff09"
                            : L"InternalKeyfreeze - \x5185\x7f6e\x952e\x76d8\x6b63\x5e38\xff08\x5de6\x952e\x51bb\x7ed3\xff09");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void StartLearning(HWND hwnd) {
    g_learning = true;
    MessageBoxW(hwnd,
        L"\x8fdb\x5165\x8bc6\x522b\x6a21\x5f0f\xff1a\n\n"
        L"\x8bf7\x5728\x7b14\x8bb0\x672c\x3010\x5185\x7f6e\x952e\x76d8\x3011\x4e0a\x6309\x4efb\x610f\x4e00\x4e2a\x952e\x3002\n"
        L"\xff08\x5916\x7f6e\x952e\x76d8\x4e0a\x6309\x952e\x4f1a\x88ab\x5ffd\x7565\x3002\xff09\n\n"
        L"\x7a0b\x5e8f\x4f1a\x81ea\x52a8\x8bc6\x522b ACPI/PS/2 \x8bbe\x5907\xff0c\n"
        L"\x5982\x679c\x68c0\x6d4b\x5230\x591a\x4e2a\x5019\x9009\x8bbe\x5907\xff0c\x5c06\x8bf7\x60a8\x786e\x8ba4\x3002\n\n"
        L"\x8bc6\x522b\x6210\x529f\x540e\x5185\x7f6e\x952e\x76d8\x4f1a\x7acb\x5373\x88ab\x51bb\x7ed3\xff0c"
        L"\x786c\x4ef6 ID \x4f1a\x4fdd\x5b58\x5230\n"
        L"InternalKeyfreeze.ini\xff0c\x4ee5\x540e\x65e0\x9700\x518d\x6b21\x8bc6\x522b\x3002",
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

// Show a sequential candidate selection dialog (called on WM_APP_MULTI_CAND).
static void ShowCandidateDialog(HWND hwnd) {
    for (int i = 0; i < g_candidate_count; i++) {
        WCHAR msg[512];
        StringCchPrintfW(msg, ARRAYSIZE(msg),
            L"\x68c0\x6d4b\x5230\x591a\x4e2a\x53ef\x80fd\x7684\x5185\x7f6e\x952e\x76d8\x8bbe\x5907 (%d/%d)\xff1a\n\n"
            L"%s\n\n\x7f6e\x4fe1\x5ea6: %d%%\n\n"
            L"\x8fd9\x662f\x60a8\x7684\x5185\x7f6e\x952e\x76d8\x5417\xff1f",
            i + 1, g_candidate_count, g_candidates[i].hwid, g_candidates[i].score);

        if (MessageBoxW(hwnd, msg, L"\x8bbe\x5907\x9009\x62e9",
                        MB_YESNO | MB_ICONQUESTION) == IDYES) {
            StringCchCopyW(g_internal_hwid, 256, g_candidates[i].hwid);
            SaveConfig();
            g_locked = true;
            UpdateTray();
            Log::Info(L"User selected candidate #%d: hwid=%s", i + 1, g_internal_hwid);
            MessageBoxW(hwnd,
                L"\x5df2\x8bc6\x522b\x5185\x7f6e\x952e\x76d8\x5e76\x7acb\x5373\x51bb\x7ed3\xff01\n\n"
                L"\x4ee5\x540e\x5de6\x952e\x6258\x76d8\x56fe\x6807\x6216 Ctrl+Shift+K \x5373\x53ef\x5207\x6362\x3002",
                L"InternalKeyfreeze", MB_OK | MB_ICONINFORMATION);
            return;
        }
    }
    // All candidates rejected
    MessageBoxW(hwnd,
        L"\x672a\x9009\x62e9\x4efb\x4f55\x8bbe\x5907\xff0c\x8bc6\x522b\x5df2\x53d6\x6d88\x3002\n"
        L"\x53ef\x7a0d\x540e\x91cd\x8bd5\x3002",
        L"InternalKeyfreeze", MB_OK | MB_ICONINFORMATION);
    Log::Info(L"All candidates rejected by user");
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
                   L"InternalKeyfreeze - \x5185\x7f6e\x952e\x76d8\x6b63\x5e38\xff08\x5de6\x952e\x51bb\x7ed3\xff09");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // Register global hotkey (Ctrl+Shift+K)
    RegisterGlobalHotkey(g_hwnd);

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
            AppendMenuW(menu, MF_STRING, IDM_LEARN, L"\x91cd\x65b0\x8bc6\x522b\x5185\x7f6e\x952e\x76d8");
            AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"\x70ed\x952e: Ctrl+Shift+K");
            AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(menu, MF_STRING, IDM_EXIT,  L"\x9000\x51fa");
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

    case WM_HOTKEY:
        // Global hotkey (Ctrl+Shift+K) toggles freeze
        if (w_param == IKF_HOTKEY_ID) {
            ToggleInternalKeyboard(hwnd);
        }
        break;

    case WM_APP_LEARNED:
        SaveConfig();
        g_locked = true;                    // freeze right after learning
        UpdateTray();
        MessageBoxW(hwnd,
            L"\x5df2\x8bc6\x522b\x5185\x7f6e\x952e\x76d8\x5e76\x7acb\x5373\x51bb\x7ed3\xff01\n\n"
            L"\x4ee5\x540e\x5de6\x952e\x6258\x76d8\x56fe\x6807\x6216 Ctrl+Shift+K \x5373\x53ef\x5207\x6362\x3002",
            L"InternalKeyfreeze", MB_OK | MB_ICONINFORMATION);
        break;

    case WM_APP_MULTI_CAND:
        ShowCandidateDialog(hwnd);
        break;

    case WM_DESTROY:
        UnregisterGlobalHotkey();
        StopWorker();
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, message, w_param, l_param);
    }
    return 0;
}
