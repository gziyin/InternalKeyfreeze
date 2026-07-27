// ============================================================================
// config.cpp
// ============================================================================

#include "config.h"
#include "logger.h"
#include <strsafe.h>

WCHAR g_internal_hwid[256] = L"";

void ConfigPath(WCHAR* path, DWORD cch) {
    GetModuleFileNameW(NULL, path, cch);
    WCHAR* dot = wcsrchr(path, L'.');
    if (dot) *dot = 0;
    StringCchCatW(path, cch, L".ini");
}

void LoadConfig() {
    WCHAR path[MAX_PATH];
    ConfigPath(path, ARRAYSIZE(path));
    GetPrivateProfileStringW(L"keyboard", L"hwid", L"",
                             g_internal_hwid, ARRAYSIZE(g_internal_hwid), path);
    if (g_internal_hwid[0])
        Log::Info(L"Config loaded: hwid=%s", g_internal_hwid);
    else
        Log::Info(L"Config loaded: no saved hwid (first run)");
}

void SaveConfig() {
    WCHAR path[MAX_PATH];
    ConfigPath(path, ARRAYSIZE(path));
    WritePrivateProfileStringW(L"keyboard", L"hwid", g_internal_hwid, path);
}
