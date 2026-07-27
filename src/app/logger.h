// ============================================================================
// logger.h - Lightweight file logger for InternalKeyfreeze
//
// Writes to %APPDATA%/InternalKeyfreeze/app.log with timestamps.
// Auto-truncates when the file exceeds 1 MB.
// Thread-safe (uses a critical section).
//
// Usage:
//   Log::Init();                        // call once at startup
//   Log::Info(L"Driver loaded OK");
//   Log::Warn(L"Device not found: %s", hwid);
//   Log::Error(L"interception_receive failed (%d)", rc);
//   Log::Shutdown();                    // optional at exit
// ============================================================================

#ifndef IKF_LOGGER_H
#define IKF_LOGGER_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <strsafe.h>
#include <stdio.h>

namespace Log {

enum Level { LEVEL_INFO, LEVEL_WARN, LEVEL_ERROR };

static CRITICAL_SECTION g_cs;
static HANDLE           g_file = INVALID_HANDLE_VALUE;
static bool             g_initialized = false;
static const DWORD      MAX_LOG_SIZE = 1024 * 1024;  // 1 MB

static void EnsureDirectory(PCWSTR dir) {
    CreateDirectoryW(dir, NULL);
}

static void OpenLogFile() {
    WCHAR appdata[MAX_PATH];
    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        // Fallback: use temp directory
        GetTempPathW(MAX_PATH, appdata);
    }

    WCHAR dir[MAX_PATH];
    StringCchPrintfW(dir, ARRAYSIZE(dir), L"%s\\InternalKeyfreeze", appdata);
    EnsureDirectory(dir);

    WCHAR path[MAX_PATH];
    StringCchPrintfW(path, ARRAYSIZE(path), L"%s\\app.log", dir);

    g_file = CreateFileW(path, FILE_APPEND_DATA,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void TruncateIfNeeded() {
    if (g_file == INVALID_HANDLE_VALUE) return;
    DWORD size = GetFileSize(g_file, NULL);
    if (size != INVALID_FILE_SIZE && size > MAX_LOG_SIZE) {
        // Close, reopen with truncation
        CloseHandle(g_file);
        WCHAR appdata[MAX_PATH];
        if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata)))
            GetTempPathW(MAX_PATH, appdata);
        WCHAR path[MAX_PATH];
        StringCchPrintfW(path, ARRAYSIZE(path), L"%s\\InternalKeyfreeze\\app.log", appdata);
        g_file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ,
                             NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        // Write a header noting truncation
        const char* msg = "[LOG TRUNCATED - exceeded 1MB]\r\n";
        DWORD written;
        WriteFile(g_file, msg, (DWORD)strlen(msg), &written, NULL);
        // Reopen in append mode
        CloseHandle(g_file);
        g_file = CreateFileW(path, FILE_APPEND_DATA,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }
}

static PCWSTR LevelStr(Level lv) {
    switch (lv) {
    case LEVEL_INFO:  return L"INFO ";
    case LEVEL_WARN:  return L"WARN ";
    case LEVEL_ERROR: return L"ERROR";
    default:          return L"?????";
    }
}

static void WriteVA(Level lv, PCWSTR fmt, va_list args) {
    if (!g_initialized || g_file == INVALID_HANDLE_VALUE) return;

    EnterCriticalSection(&g_cs);

    TruncateIfNeeded();

    // Timestamp: [2026-07-27 14:30:05.123]
    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR line[2048];
    int off = 0;
    off += swprintf_s(line + off, ARRAYSIZE(line) - off,
                      L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] ",
                      st.wYear, st.wMonth, st.wDay,
                      st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                      LevelStr(lv));

    // Format user message
    WCHAR msg[1536];
    StringCchVPrintfW(msg, ARRAYSIZE(msg), fmt, args);
    off += swprintf_s(line + off, ARRAYSIZE(line) - off, L"%s\r\n", msg);

    // Write as UTF-8 for compactness
    char utf8[4096];
    int len = WideCharToMultiByte(CP_UTF8, 0, line, off, utf8, sizeof(utf8), NULL, NULL);
    if (len > 0) {
        DWORD written;
        WriteFile(g_file, utf8, (DWORD)len, &written, NULL);
    }

    LeaveCriticalSection(&g_cs);
}

// --- Public API ---

inline void Init() {
    InitializeCriticalSection(&g_cs);
    OpenLogFile();
    g_initialized = true;
}

inline void Shutdown() {
    if (!g_initialized) return;
    g_initialized = false;
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
    DeleteCriticalSection(&g_cs);
}

inline void Info(PCWSTR fmt, ...) {
    va_list args;
    va_start(args, fmt);
    WriteVA(LEVEL_INFO, fmt, args);
    va_end(args);
}

inline void Warn(PCWSTR fmt, ...) {
    va_list args;
    va_start(args, fmt);
    WriteVA(LEVEL_WARN, fmt, args);
    va_end(args);
}

inline void Error(PCWSTR fmt, ...) {
    va_list args;
    va_start(args, fmt);
    WriteVA(LEVEL_ERROR, fmt, args);
    va_end(args);
}

} // namespace Log

#endif // IKF_LOGGER_H
