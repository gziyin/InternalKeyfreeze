// ============================================================================
// hotkey.h
//
// Global hotkey registration for toggle freeze/unfreeze.
// Default: Ctrl+Shift+K
// ============================================================================

#ifndef IKF_HOTKEY_H
#define IKF_HOTKEY_H

#include <windows.h>

// Hotkey identifier (passed to RegisterHotKey / WM_HOTKEY)
#define IKF_HOTKEY_ID  9001

// Window message for global hotkey (forwarded to WndProc)
#define WM_HOTKEY_IKF  (WM_USER + 10)

// Registers the global hotkey (Ctrl+Shift+K) with the given window.
// Returns true on success. The window will receive WM_HOTKEY when pressed.
bool RegisterGlobalHotkey(HWND hwnd);

// Unregisters the global hotkey. Safe to call even if not registered.
void UnregisterGlobalHotkey();

#endif // IKF_HOTKEY_H
