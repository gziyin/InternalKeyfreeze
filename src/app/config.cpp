// ============================================================================
// config.cpp
// ============================================================================

#include "config.h"
#include "logger.h"
#include <strsafe.h>

// ---- Frozen device list ----
WCHAR g_frozen_hwids[MAX_FROZEN_DEVICES][256];
int  g_frozen_device_count = 0;

// ---- Device list operations ----

bool IsDeviceFrozen(PCWSTR hwid) {
    if (!hwid || !hwid[0]) return false;
    for (int i = 0; i < g_frozen_device_count; i++) {
        if (_wcsicmp(g_frozen_hwids[i], hwid) == 0)
            return true;
    }
    return false;
}

int FindFrozenIndex(PCWSTR hwid) {
    if (!hwid || !hwid[0]) return -1;
    for (int i = 0; i < g_frozen_device_count; i++) {
        if (_wcsicmp(g_frozen_hwids[i], hwid) == 0)
            return i;
    }
    return -1;
}

bool AddFrozenDevice(PCWSTR hwid) {
    if (!hwid || !hwid[0]) return false;
    if (IsDeviceFrozen(hwid)) return false;       // already in list
    if (g_frozen_device_count >= MAX_FROZEN_DEVICES) return false;  // full
    StringCchCopyW(g_frozen_hwids[g_frozen_device_count], 256, hwid);
    g_frozen_device_count++;
    return true;
}

void RemoveFrozenDevice(int index) {
    if (index < 0 || index >= g_frozen_device_count) return;
    // Shift remaining devices down
    for (int i = index; i < g_frozen_device_count - 1; i++) {
        StringCchCopyW(g_frozen_hwids[i], 256, g_frozen_hwids[i + 1]);
    }
    g_frozen_device_count--;
    g_frozen_hwids[g_frozen_device_count][0] = 0;
}

void ClearFrozenDevices() {
    for (int i = 0; i < g_frozen_device_count; i++)
        g_frozen_hwids[i][0] = 0;
    g_frozen_device_count = 0;
}

// ---- INI file operations ----

void ConfigPath(WCHAR* path, DWORD cch) {
    GetModuleFileNameW(NULL, path, cch);
    WCHAR* dot = wcsrchr(path, L'.');
    if (dot) *dot = 0;
    StringCchCatW(path, cch, L".ini");
}

static void BackupOldIni(const WCHAR* path) {
    WCHAR bak[MAX_PATH];
    StringCchCopyW(bak, ARRAYSIZE(bak), path);
    StringCchCatW(bak, ARRAYSIZE(bak), L".bak");
    CopyFileW(path, bak, FALSE);  // overwrite existing .bak
}

void LoadConfig() {
    WCHAR path[MAX_PATH];
    ConfigPath(path, ARRAYSIZE(path));

    // Try new format: [devices] section
    WCHAR buf[256] = {0};
    GetPrivateProfileStringW(L"devices", L"count", L"0",
                             buf, ARRAYSIZE(buf), path);
    int new_count = _wtoi(buf);

    if (new_count > 0) {
        // New format detected
        ClearFrozenDevices();
        for (int i = 0; i < new_count && i < MAX_FROZEN_DEVICES; i++) {
            WCHAR key[16];
            StringCchPrintfW(key, ARRAYSIZE(key), L"device%d", i);
            GetPrivateProfileStringW(L"devices", key, L"",
                                     g_frozen_hwids[i], 256, path);
            if (g_frozen_hwids[i][0])
                g_frozen_device_count++;
        }
        Log::Info(L"Config loaded (new format): %d frozen device(s)", g_frozen_device_count);
        return;
    }

    // Legacy format: [keyboard] hwid=
    WCHAR legacy_hwid[256] = {0};
    GetPrivateProfileStringW(L"keyboard", L"hwid", L"",
                             legacy_hwid, ARRAYSIZE(legacy_hwid), path);

    if (legacy_hwid[0]) {
        // Migrate to new format
        Log::Info(L"Migrating legacy config: hwid=%s", legacy_hwid);
        ClearFrozenDevices();
        AddFrozenDevice(legacy_hwid);

        // Backup old ini and save in new format
        BackupOldIni(path);
        SaveConfig();

        // Remove old [keyboard] section
        WritePrivateProfileStringW(L"keyboard", NULL, NULL, path);
        Log::Info(L"Config migrated to [devices] format, backup saved as .ini.bak");
    } else {
        Log::Info(L"Config loaded: no saved devices (first run)");
    }
}

void SaveConfig() {
    WCHAR path[MAX_PATH];
    ConfigPath(path, ARRAYSIZE(path));

    // Write count
    WCHAR buf[16];
    StringCchPrintfW(buf, ARRAYSIZE(buf), L"%d", g_frozen_device_count);
    WritePrivateProfileStringW(L"devices", L"count", buf, path);

    // Write each device
    for (int i = 0; i < MAX_FROZEN_DEVICES; i++) {
        WCHAR key[16];
        StringCchPrintfW(key, ARRAYSIZE(key), L"device%d", i);
        WritePrivateProfileStringW(L"devices", key,
                                   (i < g_frozen_device_count) ? g_frozen_hwids[i] : L"",
                                   path);
    }
}
