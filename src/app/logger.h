// ============================================================================
// logger.h - Lightweight file logger for InternalKeyfreeze
//
// Writes to <exe-dir>\..\log\app.log (the exe lives in bin\, so this is the
// project-root \log directory when run from the build tree). If that location
// is not writable (e.g. installed under Program Files without elevation), it
// falls back to %LOCALAPPDATA%\InternalKeyfreeze\app.log.
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
static WCHAR            g_log_path[MAX_PATH] = {0};  // path of the open log file
static bool             g_initialized = false;
static const DWORD      MAX_LOG_SIZE = 1024 * 1024;  // 1 MB

// Build the primary log path: <exe-dir>\..\log\app.log
// (exe is in bin\, so this resolves to the project-root \log when run from
// the build tree).
static void BuildLogPath(WCHAR* path, DWORD cch) {
    WCHAR exe[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, exe, ARRAYSIZE(exe));
    WCHAR* slash = wcsrchr(exe, L'\\');
    if (slash) *slash = 0;                 // strip filename -> exe directory
    StringCchPrintfW(path, cch, L"%s\\..\\log\\app.log", exe);
}

// Create the directory part of |path| (everything up to the final component).
static void EnsureDirOf(WCHAR* path) {
    WCHAR* slash = wcsrchr(path, L'\\');
    if (slash) {
        *slash = 0;
        CreateDirectoryW(path, NULL);      // succeeds even if it already exists
        *slash = L'\\';
    }
}

static void OpenLogFile() {
    // 1. Primary: <exe-dir>\..\log\app.log
    BuildLogPath(g_log_path, ARRAYSIZE(g_log_path));
    EnsureDirOf(g_log_path);
    g_file = CreateFileW(g_log_path, FILE_APPEND_DATA,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (g_file != INVALID_HANDLE_VALUE)
        return;

    // 2. Fallback: %LOCALAPPDATA%\InternalKeyfreeze\app.log
    //    (e.g. when the exe sits under Program Files and is not writable as a
    //    normal user)
    WCHAR local[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local))) {
        StringCchPrintfW(g_log_path, ARRAYSIZE(g_log_path),
                         L"%s\\InternalKeyfreeze\\app.log", local);
        EnsureDirOf(g_log_path);
        g_file = CreateFileW(g_log_path, FILE_APPEND_DATA,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_file != INVALID_HANDLE_VALUE)
            return;
    }

    // 3. Give up silently; Log::* calls become no-ops.
    g_log_path[0] = 0;
}

static void TruncateIfNeeded() {
    if (g_file == INVALID_HANDLE_VALUE || g_log_path[0] == 0) return;
    DWORD size = GetFileSize(g_file, NULL);
    if (size != INVALID_FILE_SIZE && size > MAX_LOG_SIZE) {
        // Reopen with truncation, write a header, then reopen in append mode.
        CloseHandle(g_file);
        g_file = CreateFileW(g_log_path, GENERIC_WRITE, FILE_SHARE_READ,
                             NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_file == INVALID_HANDLE_VALUE) return;
        const char* msg = "[LOG TRUNCATED - exceeded 1MB]\r\n";
        DWORD written;
        WriteFile(g_file, msg, (DWORD)strlen(msg), &written, NULL);
        CloseHandle(g_file);
        g_file = CreateFileW(g_log_path, FILE_APPEND_DATA,
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
