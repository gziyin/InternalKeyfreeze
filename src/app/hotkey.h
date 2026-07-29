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

// Returns true if the global hotkey is currently registered.
bool IsHotkeyEnabled();

// Toggles the global hotkey on/off. Returns the new state.
bool ToggleHotkey();

#endif // IKF_HOTKEY_H
