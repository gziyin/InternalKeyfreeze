// ============================================================================
// autostart.cpp
// ============================================================================

#include "autostart.h"
#include "logger.h"
#include <strsafe.h>

static const WCHAR* AUTOSTART_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const WCHAR* AUTOSTART_NAME = L"InternalKeyfreeze";

bool IsAutostartEnabled() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_READ, &key)
        != ERROR_SUCCESS)
        return false;

    WCHAR value[MAX_PATH] = {0};
    DWORD size = sizeof(value);
    DWORD type = 0;
    LSTATUS rc = RegQueryValueExW(key, AUTOSTART_NAME, NULL, &type,
                                   (LPBYTE)value, &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS && type == REG_SZ && value[0] != 0;
}

bool EnableAutostart() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_SET_VALUE, &key)
        != ERROR_SUCCESS) {
        Log::Error(L"Failed to open autostart registry key");
        return false;
    }

    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, ARRAYSIZE(path));
    // Add /minimized flag for future window-mode support
    StringCchCatW(path, ARRAYSIZE(path), L" /minimized");

    LSTATUS rc = RegSetValueExW(key, AUTOSTART_NAME, 0, REG_SZ,
                                (const BYTE*)path,
                                (DWORD)(wcslen(path) + 1) * sizeof(WCHAR));
    RegCloseKey(key);

    if (rc == ERROR_SUCCESS) {
        Log::Info(L"Autostart enabled: %s", path);
        return true;
    }
    Log::Error(L"Failed to set autostart registry value, err=%ld", rc);
    return false;
}

bool DisableAutostart() {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_SET_VALUE, &key)
        != ERROR_SUCCESS) {
        Log::Error(L"Failed to open autostart registry key");
        return false;
    }

    LSTATUS rc = RegDeleteValueW(key, AUTOSTART_NAME);
    RegCloseKey(key);

    if (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND) {
        Log::Info(L"Autostart disabled");
        return true;
    }
    Log::Error(L"Failed to delete autostart registry value, err=%ld", rc);
    return false;
}

bool ToggleAutostart() {
    if (IsAutostartEnabled()) {
        DisableAutostart();
        return false;
    } else {
        EnableAutostart();
        return true;
    }
}
