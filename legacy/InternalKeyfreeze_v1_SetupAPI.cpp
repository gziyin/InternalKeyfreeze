// ============================================================================
// InternalKeyfreeze.cpp
//
// Modeled after OpenKeyfreeze, but instead of blocking ALL keyboards with a
// low-level hook, it disables ONLY the laptop's built-in keyboard at the
// device level (SetupAPI / Device Manager "Disable device"). External USB /
// Bluetooth keyboards keep working normally.
//
// Why not a hook? WH_KEYBOARD_LL (KBDLLHOOKSTRUCT) carries NO device
// information, so a hook can never tell the built-in keyboard apart from an
// external one. Device-level disable is the clean and 100% reliable way.
//
// BUILD (Visual Studio developer prompt):
//   cl /EHsc /W4 InternalKeyfreeze.cpp ^
//      /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTFILE:InternalKeyfreeze.manifest
//
// BUILD (MinGW-w64):
//   g++ -O2 -municode -mwindows InternalKeyfreeze.cpp -o InternalKeyfreeze.exe -lsetupapi
//   (then embed the manifest with mt.exe, or just run the exe as administrator)
//
// IMPORTANT: disabling devices requires administrator rights (see the
// manifest's requireAdministrator). Without elevation every SetupAPI call
// fails with ERROR_ACCESS_DENIED.
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
#include <setupapi.h>
#include <devguid.h>      // GUID_DEVCLASS_KEYBOARD
#include <strsafe.h>

#pragma comment(lib, "setupapi.lib")

#define MAX_LOADSTRING 100
#define WM_TRAYICON    (WM_USER + 1)
#define IDM_EXIT       1001
#define TRAY_UID       1

HINSTANCE g_instance;
HWND      g_hwnd = NULL;
WCHAR     g_title[MAX_LOADSTRING];
WCHAR     g_window_class[MAX_LOADSTRING];
NOTIFYICONDATAW g_nid;
HICON  g_icon_enabled;    // built-in keyboard active
HICON  g_icon_disabled;   // built-in keyboard frozen
HANDLE g_mutex = NULL;
bool   g_is_disabled = false;

// ---------------------------------------------------------------------------
// OPTIONAL override: exact device instance path(s) of the built-in keyboard.
// If any non-NULL entry is present here, ONLY those devices are toggled and
// the automatic heuristic below is skipped entirely.
//
// Find the path in: Device Manager -> Keyboards -> <device> -> Properties ->
// Details tab -> "Device instance path",
// e.g. L"ACPI\\PNP0303\\4&2F8D3B1E&0"
// ---------------------------------------------------------------------------
static const WCHAR* g_forced_devices[] = {
    // L"ACPI\\PNP0303\\4&2F8D3B1E&0",
    NULL
};

static bool HasForcedList() {
    return g_forced_devices[0] != NULL;
}

static bool IsForcedDevice(PCWSTR instance_id) {
    for (int i = 0; g_forced_devices[i]; i++) {
        if (_wcsicmp(g_forced_devices[i], instance_id) == 0)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Heuristic: hardware IDs are a REG_MULTI_SZ. The built-in keyboard is PS/2
// ("PNP0303") or I2C/SMBus attached -- never USB, so it has no "VID_xxxx" id.
// Every external USB keyboard DOES carry "VID_xxxx". Bluetooth keyboards are
// "BTHENUM" / "BTHHFENUM" or HID over GATT - also not matched here.
// ---------------------------------------------------------------------------
static bool LooksLikeInternalKeyboard(PCWSTR multi_sz) {
    bool has_vid = false, has_pnp0303 = false;
    for (PCWSTR p = multi_sz; *p; p += wcslen(p) + 1) {
        if (wcsstr(p, L"VID_"))    has_vid = true;
        if (wcsstr(p, L"PNP0303")) has_pnp0303 = true;
    }
    return has_pnp0303 || !has_vid;
}

static bool IsElevated() {
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;
    TOKEN_ELEVATION te;
    DWORD size = sizeof(te);
    BOOL ok = GetTokenInformation(token, TokenElevation, &te, sizeof(te), &size);
    CloseHandle(token);
    return ok && te.TokenIsElevated;
}

// ---------------------------------------------------------------------------
// Core: enable/disable every keyboard-class device identified as internal.
// This is exactly what devcon / Device Manager "Disable device" does.
// Returns: number of devices switched, 0 if none matched, -1 on API error.
// ---------------------------------------------------------------------------
static int SetInternalKeyboardState(bool enable) {
    HDEVINFO devs = SetupDiGetClassDevsW(&GUID_DEVCLASS_KEYBOARD, NULL, NULL, DIGCF_PRESENT);
    if (devs == INVALID_HANDLE_VALUE)
        return -1;

    SP_DEVINFO_DATA info;
    info.cbSize = sizeof(info);
    int changed = 0;

    for (DWORD i = 0; SetupDiEnumDeviceInfo(devs, i, &info); i++) {
        WCHAR instance_id[512];
        if (!SetupDiGetDeviceInstanceIdW(devs, &info, instance_id,
                                         ARRAYSIZE(instance_id), NULL))
            continue;

        bool target;
        if (HasForcedList()) {
            target = IsForcedDevice(instance_id);
        } else {
            WCHAR hw_ids[2048] = {0};
            if (!SetupDiGetDeviceRegistryPropertyW(devs, &info, SPDRP_HARDWAREID,
                                                   NULL, (PBYTE)hw_ids,
                                                   sizeof(hw_ids), NULL))
                continue;
            target = LooksLikeInternalKeyboard(hw_ids);
        }
        if (!target)
            continue;   // external keyboard -> leave it alone

        // Debug trace (visible in a debugger or DebugView) so you can verify
        // exactly which device got toggled.
        WCHAR desc[256] = L"?";
        SetupDiGetDeviceRegistryPropertyW(devs, &info, SPDRP_DEVICEDESC, NULL,
                                          (PBYTE)desc, sizeof(desc), NULL);
        WCHAR dbg[1024];
        StringCchPrintfW(dbg, ARRAYSIZE(dbg), L"[InternalKeyfreeze] %s \"%s\" (%s)\n",
                         enable ? L"enable " : L"disable", desc, instance_id);
        OutputDebugStringW(dbg);

        SP_PROPCHANGE_PARAMS pcp;
        ZeroMemory(&pcp, sizeof(pcp));
        pcp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        pcp.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        pcp.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;
        pcp.Scope       = DICS_FLAG_GLOBAL;
        pcp.HwProfile   = 0;

        if (!SetupDiSetClassInstallParamsW(devs, &info, &pcp.ClassInstallHeader, sizeof(pcp)))
            continue;
        if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devs, &info))
            continue;   // e.g. ERROR_ACCESS_DENIED when not elevated

        changed++;
    }

    SetupDiDestroyDeviceInfoList(devs);
    return changed;
}

static void UpdateTray() {
    g_nid.hIcon = g_is_disabled ? g_icon_disabled : g_icon_enabled;
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
                   g_is_disabled ? L"InternalKeyfreeze - built-in keyboard DISABLED (click to restore)"
                                 : L"InternalKeyfreeze - built-in keyboard enabled (click to freeze)");
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

static void ToggleInternalKeyboard(HWND hwnd) {
    bool want_disable = !g_is_disabled;
    int changed = SetInternalKeyboardState(!want_disable);   // enable = !want_disable
    if (changed <= 0) {
        MessageBoxW(hwnd,
            L"No built-in keyboard was switched.\n\n"
            L"Possible causes:\n"
            L"  - the app is not running as administrator;\n"
            L"  - your built-in keyboard is wired internally via USB, so the\n"
            L"    heuristic cannot tell it apart from external ones. In that\n"
            L"    case copy its \"Device instance path\" from Device Manager\n"
            L"    into g_forced_devices[] and rebuild.",
            L"InternalKeyfreeze", MB_OK | MB_ICONWARNING);
        return;
    }
    g_is_disabled = want_disable;
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
        MessageBoxW(NULL, L"There is already an instance of InternalKeyfreeze running.",
                    L"InternalKeyfreeze", MB_OK | MB_ICONEXCLAMATION);
        return 1;
    }

    if (!IsElevated()) {
        MessageBoxW(NULL,
            L"InternalKeyfreeze must run as administrator to disable/enable\n"
            L"the built-in keyboard device. Right-click the exe and choose\n"
            L"\"Run as administrator\".",
            L"InternalKeyfreeze", MB_OK | MB_ICONSTOP);
        if (g_mutex) { ReleaseMutex(g_mutex); CloseHandle(g_mutex); }
        return 1;
    }

    wcscpy_s(g_title, MAX_LOADSTRING, L"InternalKeyfreeze");
    wcscpy_s(g_window_class, MAX_LOADSTRING, L"InternalKeyfreezeWindowClass");
    MyRegisterClass(instance);

    if (!InitInstance(instance, cmd_show))
        return FALSE;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_mutex) {
        ReleaseMutex(g_mutex);
        CloseHandle(g_mutex);
    }
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE instance) {
    WNDCLASSEXW wcex;
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize        = sizeof(wcex);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.hInstance     = instance;
    wcex.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_window_class;
    wcex.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
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

    // Same SHELL32 built-in icons as the original project. :-)
    g_icon_enabled  = LoadIcon(GetModuleHandleW(L"SHELL32.dll"), MAKEINTRESOURCE(246)); // blue play
    g_icon_disabled = LoadIcon(GetModuleHandleW(L"SHELL32.dll"), MAKEINTRESOURCE(200)); // red stop

    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = g_hwnd;
    g_nid.uID              = TRAY_UID;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = g_icon_enabled;
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip),
                   L"InternalKeyfreeze - built-in keyboard enabled (click to freeze)");
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
            AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");
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
        else {
            return DefWindowProc(hwnd, message, w_param, l_param);
        }
        break;

    case WM_DESTROY:
        // Safety: never leave the built-in keyboard dead behind our back.
        if (g_is_disabled) {
            SetInternalKeyboardState(true);
            g_is_disabled = false;
        }
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, message, w_param, l_param);
    }
    return 0;
}
