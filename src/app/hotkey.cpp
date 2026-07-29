// ============================================================================
// hotkey.cpp
// ============================================================================

#include "hotkey.h"
#include "logger.h"

static HWND g_hotkey_hwnd = NULL;
static bool g_hotkey_registered = false;

// Ctrl+Shift+K = MOD_CONTROL | MOD_SHIFT + 'K'
static const UINT HOTKEY_MODS = MOD_CONTROL | MOD_SHIFT;
static const UINT HOTKEY_KEY  = 'K';

static bool DoRegister(HWND hwnd) {
    if (!RegisterHotKey(hwnd, IKF_HOTKEY_ID, HOTKEY_MODS, HOTKEY_KEY)) {
        Log::Warn(L"RegisterHotKey(Ctrl+Shift+K) failed, err=%lu", GetLastError());
        return false;
    }
    g_hotkey_registered = true;
    Log::Info(L"Global hotkey registered: Ctrl+Shift+K");
    return true;
}

bool IsHotkeyEnabled() {
    return g_hotkey_registered;
}

bool RegisterGlobalHotkey(HWND hwnd) {
    g_hotkey_hwnd = hwnd;
    if (g_hotkey_registered)
        return true;          // already registered
    return DoRegister(hwnd);
}

void UnregisterGlobalHotkey() {
    if (g_hotkey_registered && g_hotkey_hwnd) {
        UnregisterHotKey(g_hotkey_hwnd, IKF_HOTKEY_ID);
        g_hotkey_registered = false;
        Log::Info(L"Global hotkey unregistered");
    }
}

bool ToggleHotkey() {
    if (g_hotkey_registered) {
        UnregisterGlobalHotkey();
        return false;
    }
    if (!g_hotkey_hwnd)
        return false;
    return DoRegister(g_hotkey_hwnd);
}
