// ============================================================================
// tray_app.h
//
// Tray icon UI, window class, and message handling.
// ============================================================================

#ifndef IKF_TRAY_APP_H
#define IKF_TRAY_APP_H

#include <windows.h>
#include <shellapi.h>
#include "keyboard_filter.h"

// ---- Constants ----
#define MAX_LOADSTRING   100
#define WM_TRAYICON      (WM_USER + 1)
#define IDM_EXIT         1001
#define IDM_LEARN        1002
#define TRAY_UID         1

// ---- Global UI state ----
extern HINSTANCE       g_instance;
extern HWND           g_hwnd;
extern WCHAR          g_title[MAX_LOADSTRING];
extern WCHAR          g_window_class[MAX_LOADSTRING];
extern NOTIFYICONDATAW g_nid;
extern HICON          g_icon_enabled;    // built-in keyboard active
extern HICON          g_icon_disabled;   // built-in keyboard frozen

// ---- UI functions ----

// Updates the tray icon and tooltip to reflect current g_locked state.
void UpdateTray();

// Puts the app into learning mode (prompts user, sets g_learning flag).
void StartLearning(HWND hwnd);

// Toggles keyboard freeze. If no hwid is learned yet, enters learning mode.
void ToggleInternalKeyboard(HWND hwnd);

// Registers the hidden message-only window class.
ATOM MyRegisterClass(HINSTANCE instance);

// Creates the hidden window and adds the tray icon.
BOOL InitInstance(HINSTANCE instance, int cmd_show);

// Window procedure: handles tray clicks, menu commands, learned notification.
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

#endif // IKF_TRAY_APP_H
