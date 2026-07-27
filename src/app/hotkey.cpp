// ============================================================================
// hotkey.cpp
// ============================================================================

#include "hotkey.h"
#include "logger.h"

static HWND g_hotkey_hwnd = NULL;

bool RegisterGlobalHotkey(HWND hwnd) {
    g_hotkey_hwnd = hwnd;
    // Ctrl+Shift+K = MOD_CONTROL | MOD_SHIFT + 'K'
    UINT mods = MOD_CONTROL | MOD_SHIFT;
    UINT key  = 'K';

    if (!RegisterHotKey(hwnd, IKF_HOTKEY_ID, mods, key)) {
        Log::Warn(L"RegisterHotKey(Ctrl+Shift+K) failed, err=%lu", GetLastError());
        return false;
    }
    Log::Info(L"Global hotkey registered: Ctrl+Shift+K");
    return true;
}

void UnregisterGlobalHotkey() {
    if (g_hotkey_hwnd) {
        UnregisterHotKey(g_hotkey_hwnd, IKF_HOTKEY_ID);
        g_hotkey_hwnd = NULL;
        Log::Info(L"Global hotkey unregistered");
    }
}
