// ============================================================================
// tray_app.cpp
// ============================================================================

#include "tray_app.h"
#include "config.h"
#include "logger.h"
#include "hotkey.h"
#include "autostart.h"
#include "device_info.h"
#include "../resources/resources.h"
#include <commctrl.h>
#include <windowsx.h>
#include <strsafe.h>

// ---- Global UI state ----
HINSTANCE       g_instance = NULL;
HWND           g_hwnd = NULL;
WCHAR          g_title[MAX_LOADSTRING];
WCHAR          g_window_class[MAX_LOADSTRING];
NOTIFYICONDATAW g_nid;
HICON          g_icon_enabled = NULL;
HICON          g_icon_disabled = NULL;

// ---- Device-manager dialog ----
#define IDC_DLG_LIST     2001
#define IDC_DLG_REFRESH  2002
#define IDC_DLG_RELEARN  2003
#define IDC_DLG_CLOSE    2004
#define IDC_DLG_HINT     2005
#define IDC_DLG_TOGGLE   2006
#define DLG_MAX_ROWS     20

static HWND  g_dlg_list = NULL;
static HWND  g_dlg_toggle = NULL;
static WCHAR g_dlg_hwids[DLG_MAX_ROWS][256];
static bool  g_dlg_present[DLG_MAX_ROWS];
static int   g_dlg_rows = 0;

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
    if (!g_locked && g_frozen_device_count == 0) {
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
            AddFrozenDevice(g_candidates[i].hwid);
            SaveConfig();
            g_locked = true;
            UpdateTray();
            Log::Info(L"User selected candidate #%d: hwid=%s", i + 1, g_candidates[i].hwid);
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

// ---------------------------------------------------------------------------
// Device-manager dialog
// ---------------------------------------------------------------------------

static void DlgLayoutControls(HWND hwnd);

static void DlgStatusTextFor(PCWSTR hwid, bool present, WCHAR* out, DWORD cch) {
    bool frozen = IsDeviceFrozen(hwid) && g_locked.load();
    if (frozen && !present)
        StringCchCopyW(out, cch, L"\x5df2\x51bb\x7ed3\xff08\x672a\x8fde\x63a5\xff09");  // 已冻结(未连接)
    else if (frozen)
        StringCchCopyW(out, cch, L"\x5df2\x51bb\x7ed3");                                 // 已冻结
    else
        StringCchCopyW(out, cch, L"\x8fd0\x884c\x4e2d");                                 // 运行中
}

static void DlgAddRow(PCWSTR hwid, bool present) {
    if (g_dlg_rows >= DLG_MAX_ROWS) return;
    int row = g_dlg_rows;
    StringCchCopyW(g_dlg_hwids[row], 256, hwid);
    g_dlg_present[row] = present;

    WCHAR name[256];
    GetDeviceFriendlyName(hwid, name, ARRAYSIZE(name));

    LVITEMW lvi;
    ZeroMemory(&lvi, sizeof(lvi));
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem = g_dlg_rows;          // append at the end
    lvi.iSubItem = 0;
    lvi.pszText = name;
    lvi.lParam = (LPARAM)row;        // remember our row index for hit-tests
    int inserted = ListView_InsertItem(g_dlg_list, &lvi);

    ListView_SetItemText(g_dlg_list, inserted, 1, g_dlg_hwids[row]);
    WCHAR status[64];
    DlgStatusTextFor(g_dlg_hwids[row], g_dlg_present[row], status, ARRAYSIZE(status));
    ListView_SetItemText(g_dlg_list, inserted, 2, status);

    g_dlg_rows++;
}

static void DlgPopulateList() {
    if (!g_dlg_list) return;
    ListView_DeleteAllItems(g_dlg_list);
    g_dlg_rows = 0;

    // 1. Present (scanned) keyboards.
    for (int i = 0; i < g_present_count; i++)
        DlgAddRow(g_present_hwids[i], true);

    // 2. Frozen devices that are not currently present (e.g. unplugged).
    for (int i = 0; i < g_frozen_device_count; i++) {
        PCWSTR hwid = g_frozen_hwids[i];
        bool already = false;
        for (int r = 0; r < g_dlg_rows; r++) {
            if (_wcsicmp(g_dlg_hwids[r], hwid) == 0) { already = true; break; }
        }
        if (!already)
            DlgAddRow(hwid, false);
    }

    if (g_dlg_rows == 0) {
        LVITEMW lvi;
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask = LVIF_TEXT;
        lvi.iItem = 0;
        lvi.pszText = (LPWSTR)L"\x672a\x626b\x63cf\x5230\x4efb\x4f55\x952e\x76d8\x8bbe\x5907";  // 未扫描到任何键盘设备
        ListView_InsertItem(g_dlg_list, &lvi);
    }
}

// Refresh every row's status cell. A g_locked change can flip multiple rows
// at once (turning freeze on activates every device in the list), so toggling
// one device must refresh them all.
static void DlgRefreshStatuses() {
    if (!g_dlg_list || g_dlg_rows <= 0) return;
    for (int row = 0; row < g_dlg_rows; row++) {
        WCHAR status[64];
        DlgStatusTextFor(g_dlg_hwids[row], g_dlg_present[row], status, ARRAYSIZE(status));
        ListView_SetItemText(g_dlg_list, row, 2, status);
    }
    ListView_RedrawItems(g_dlg_list, 0, g_dlg_rows - 1);
    UpdateWindow(g_dlg_list);
}

// Returns the currently selected row index, or -1 if nothing is selected.
static int DlgGetSelectedRow() {
    if (!g_dlg_list) return -1;
    int item = ListView_GetNextItem(g_dlg_list, -1, LVNI_SELECTED);
    if (item < 0 || item >= g_dlg_rows) return -1;
    return item;
}

// Update the bottom "Toggle state" button to reflect the currently selected
// device: "Freeze" when running, "Unfreeze" when frozen, disabled otherwise.
static void DlgUpdateToggleButton() {
    if (!g_dlg_toggle) return;
    int row = DlgGetSelectedRow();
    if (row < 0) {
        SetWindowTextW(g_dlg_toggle, L"\x5207\x6362\x72b6\x6001");  // 切换状态
        EnableWindow(g_dlg_toggle, FALSE);
        return;
    }
    bool frozen = IsDeviceFrozen(g_dlg_hwids[row]) && g_locked.load();
    SetWindowTextW(g_dlg_toggle, frozen ? L"\x89e3\x51bb" : L"\x51bb\x7ed3");  // 解冻 / 冻结
    EnableWindow(g_dlg_toggle, TRUE);
}

static void DlgToggleDevice(int row) {
    if (row < 0 || row >= g_dlg_rows) return;
    PCWSTR hwid = g_dlg_hwids[row];
    bool frozen = IsDeviceFrozen(hwid) && g_locked.load();

    if (frozen) {
        int idx = FindFrozenIndex(hwid);
        if (idx >= 0) {
            RemoveFrozenDevice(idx);
            SaveConfig();
            Log::Info(L"Device removed from frozen list via dialog: %s", hwid);
            if (g_frozen_device_count == 0) {
                g_locked = false;
                UpdateTray();
            }
        }
    } else {
        if (AddFrozenDevice(hwid)) {
            SaveConfig();
            g_locked = true;            // take effect immediately
            UpdateTray();
            Log::Info(L"Device added to frozen list via dialog: %s", hwid);
        }
    }

    DlgRefreshStatuses();
    DlgUpdateToggleButton();
}

static LRESULT DlgHandleCustomDraw(LPARAM lp) {
    LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lp;
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
        if (cd->iSubItem == 2) {           // status column
            int row = (int)cd->nmcd.lItemlParam;
            bool frozen = (row >= 0 && row < g_dlg_rows) &&
                          IsDeviceFrozen(g_dlg_hwids[row]) && g_locked.load();
            cd->clrText = frozen ? RGB(200, 30, 30) : RGB(0, 130, 60);
            cd->clrTextBk = RGB(245, 245, 245);
        }
        return CDRF_NEWFONT;
    }
    return CDRF_DODEFAULT;
}

static void DlgLayoutControls(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    const int pad = 12, hintH = 22, btnH = 30, btnW = 96;

    HWND hint = GetDlgItem(hwnd, IDC_DLG_HINT);
    if (hint)
        MoveWindow(hint, pad, pad, rc.right - 2 * pad, hintH, TRUE);

    int listTop = pad + hintH + 6;
    int listH = rc.bottom - listTop - btnH - 16;
    if (g_dlg_list)
        MoveWindow(g_dlg_list, pad, listTop, rc.right - 2 * pad, listH, TRUE);

    int by = rc.bottom - btnH - pad;
    HWND bRefresh = GetDlgItem(hwnd, IDC_DLG_REFRESH);
    HWND bRelearn = GetDlgItem(hwnd, IDC_DLG_RELEARN);
    HWND bToggle  = GetDlgItem(hwnd, IDC_DLG_TOGGLE);
    HWND bClose   = GetDlgItem(hwnd, IDC_DLG_CLOSE);
    if (bRefresh) MoveWindow(bRefresh, pad, by, btnW, btnH, TRUE);
    if (bRelearn) MoveWindow(bRelearn, pad + btnW + 8, by, btnW, btnH, TRUE);
    if (bToggle)  MoveWindow(bToggle, rc.right - pad - 2 * btnW - 8, by, btnW, btnH, TRUE);
    if (bClose)   MoveWindow(bClose, rc.right - pad - btnW, by, btnW, btnH, TRUE);
}

static LRESULT CALLBACK DevicesDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_devices_dlg = hwnd;
        g_dlg_list = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_DLG_LIST, g_instance, NULL);
        ListView_SetExtendedListViewStyle(g_dlg_list,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

        LVCOLUMNW col;
        ZeroMemory(&col, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.iSubItem = 0; col.cx = 230; col.pszText = (LPWSTR)L"\x8bbe\x5907\x540d\x79f0"; ListView_InsertColumn(g_dlg_list, 0, &col);  // 设备名称
        col.iSubItem = 1; col.cx = 250; col.pszText = (LPWSTR)L"\x786c\x4ef6 ID";          ListView_InsertColumn(g_dlg_list, 1, &col);  // 硬件ID
        col.iSubItem = 2; col.cx = 96;  col.pszText = (LPWSTR)L"\x72b6\x6001";             ListView_InsertColumn(g_dlg_list, 2, &col);  // 状态

        CreateWindowExW(0, WC_STATICW,
            L"\x8bf7\x5148\x5728\x5217\x8868\x4e2d\x9009\x4e2d\x8bbe\x5907\xff0c\x7136\x540e\x70b9\x51fb\x300c\x5207\x6362\x72b6\x6001\x300d\x6309\x94ae\x8fdb\x884c\x51bb\x7ed3\x6216\x89e3\x51bb\x3002",  // 请先在列表中选中设备，然后点击「切换状态」按钮进行冻结或解冻。
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_DLG_HINT, g_instance, NULL);

        CreateWindowExW(0, WC_BUTTONW, L"\x5237\x65b0",      WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_DLG_REFRESH, g_instance, NULL);  // 刷新
        CreateWindowExW(0, WC_BUTTONW, L"\x91cd\x65b0\x8bc6\x522b", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_DLG_RELEARN, g_instance, NULL);  // 重新识别
        g_dlg_toggle = CreateWindowExW(0, WC_BUTTONW, L"\x5207\x6362\x72b6\x6001", WS_CHILD | WS_VISIBLE | WS_DISABLED, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_DLG_TOGGLE, g_instance, NULL);  // 切换状态
        CreateWindowExW(0, WC_BUTTONW, L"\x5173\x95ed",      WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_DLG_CLOSE, g_instance, NULL);  // 关闭

        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(g_dlg_list, WM_SETFONT, (WPARAM)font, TRUE);
        SendDlgItemMessageW(hwnd, IDC_DLG_HINT, WM_SETFONT, (WPARAM)font, TRUE);
        for (int id = IDC_DLG_REFRESH; id <= IDC_DLG_TOGGLE; id++)
            SendDlgItemMessageW(hwnd, id, WM_SETFONT, (WPARAM)font, TRUE);

        DlgLayoutControls(hwnd);

        // Placeholder while the worker scans.
        LVITEMW lvi;
        ZeroMemory(&lvi, sizeof(lvi));
        lvi.mask = LVIF_TEXT;
        lvi.iItem = 0;
        lvi.pszText = (LPWSTR)L"\x6b63\x5728\x626b\x63cf\x8bbe\x5907...";  // 正在扫描设备...
        ListView_InsertItem(g_dlg_list, &lvi);

        g_rescan_devices = true;   // ask the worker to refresh the snapshot
        return 0;
    }

    case WM_SIZE:
        DlgLayoutControls(hwnd);
        return 0;

    case WM_APP_DEVICES_READY:
        DlgPopulateList();
        DlgUpdateToggleButton();
        return 0;

    case WM_ACTIVATE:
        // The user may have toggled freeze via tray/hotkey while the dialog
        // was in the background; refresh status cells and the toggle button
        // when focus returns.
        if (LOWORD(wp) != WA_INACTIVE) {
            DlgRefreshStatuses();
            DlgUpdateToggleButton();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_DLG_REFRESH:
            g_rescan_devices = true;
            return 0;
        case IDC_DLG_TOGGLE: {
            int row = DlgGetSelectedRow();
            if (row < 0) {
                MessageBoxW(hwnd,
                    L"\x8bf7\x5148\x5728\x5217\x8868\x4e2d\x9009\x62e9\x4e00\x4e2a\x8bbe\x5907\x3002",  // 请先在列表中选择一个设备。
                    L"InternalKeyfreeze", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            DlgToggleDevice(row);
            return 0;
        }
        case IDC_DLG_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case IDC_DLG_RELEARN:
            DestroyWindow(hwnd);
            PostMessageW(g_hwnd, WM_COMMAND, IDM_LEARN, 0);
            return 0;
        }
        return 0;

    case WM_NOTIFY: {
        LPNMHDR nm = (LPNMHDR)lp;
        if (nm->hwndFrom != g_dlg_list)
            return DefWindowProc(hwnd, msg, wp, lp);
        if (nm->code == LVN_ITEMCHANGED) {
            DlgUpdateToggleButton();
            return 0;
        }
        if (nm->code == NM_CUSTOMDRAW)
            return DlgHandleCustomDraw(lp);
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_devices_dlg = NULL;
        g_dlg_list = NULL;
        g_dlg_toggle = NULL;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void ShowDevicesDialog(HWND parent) {
    if (g_devices_dlg && IsWindow(g_devices_dlg)) {
        SetForegroundWindow(g_devices_dlg);
        return;
    }

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DevicesDlgProc;
        wc.hInstance = g_instance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"IKFDevicesDlgClass";
        wc.hIconSm = LoadIcon(g_instance, MAKEINTRESOURCE(IDI_APP_ICON));
        RegisterClassExW(&wc);
        registered = true;
    }

    HWND dlg = CreateWindowExW(WS_EX_APPWINDOW, L"IKFDevicesDlgClass",
        L"InternalKeyfreeze \x8bbe\x5907\x7ba1\x7406",   // 设备管理
        (WS_OVERLAPPEDWINDOW & ~WS_MINIMIZEBOX & ~WS_MAXIMIZEBOX) | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 420,
        parent, NULL, g_instance, NULL);
    if (dlg) {
        ShowWindow(dlg, SW_SHOW);
        UpdateWindow(dlg);
    }
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
            AppendMenuW(menu, MF_STRING, IDM_DEVICES, L"\x8bbe\x5907\x7ba1\x7406...");   // 设备管理...
            AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(menu, MF_STRING, IDM_LEARN, L"\x91cd\x65b0\x8bc6\x522b\x5185\x7f6e\x952e\x76d8");  // 重新识别内置键盘
            AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
            UINT auto_flags = MF_STRING;
            if (IsAutostartEnabled())
                auto_flags |= MF_CHECKED;
            AppendMenuW(menu, auto_flags, IDM_AUTOSTART, L"\x5f00\x673a\x81ea\x542f");   // 开机自启
            UINT hk_flags = MF_STRING;
            if (IsHotkeyEnabled())
                hk_flags |= MF_CHECKED;
            AppendMenuW(menu, hk_flags, IDM_HOTKEY, L"\x5168\x5c40\x70ed\x952e Ctrl+Shift+K");  // 全局热键 Ctrl+Shift+K
            AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(menu, MF_STRING, IDM_EXIT,  L"\x9000\x51fa");                     // 退出
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);   // required so the menu dismisses properly
            TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(menu);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(w_param)) {
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        case IDM_LEARN:
            ClearFrozenDevices();
            SaveConfig();
            StartLearning(hwnd);
            break;
        case IDM_AUTOSTART: {
            bool enabled = ToggleAutostart();
            Log::Info(L"Autostart %s", enabled ? L"enabled" : L"disabled");
            break;
        }
        case IDM_DEVICES:
            ShowDevicesDialog(hwnd);
            break;
        case IDM_HOTKEY: {
            bool enabled = ToggleHotkey();
            Log::Info(L"Global hotkey %s", enabled ? L"enabled" : L"disabled");
            break;
        }
        default:
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

    case WM_APP_LEARN_FAILED:
        MessageBoxW(hwnd,
            L"\x672a\x68c0\x6d4b\x5230\x4efb\x4f55\x5185\x7f6e\x952e\x76d8\x5019\x9009\x8bbe\x5907\x3002\n\n"
            L"\x8bf7\x786e\x8ba4\x5728\x5185\x7f6e\x952e\x76d8\x4e0a\x6309\x4e86\x952e\xff08\x5916\x7f6e\x952e\x76d8\x4f1a\x88ab\x5ffd\x7565\xff09\xff0c\n"
            L"\x6216\x7a0d\x540e\x91cd\x8bd5\x3002",
            L"InternalKeyfreeze", MB_OK | MB_ICONINFORMATION);
        break;

    case WM_APP_MULTI_CAND:
        ShowCandidateDialog(hwnd);
        break;

    case WM_APP_HOTKEY_FIRED:
        // Ctrl+Shift+K was seen on the frozen built-in keyboard (swallowed by
        // the driver); toggle freeze here so the hotkey still works.
        ToggleInternalKeyboard(hwnd);
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
