// ============================================================================
// keyboard_filter.cpp
// ============================================================================

#include "keyboard_filter.h"
#include "config.h"
#include "logger.h"
#include <strsafe.h>

// Atomic flags
std::atomic<bool> g_stop_worker{false};
std::atomic<bool> g_locked{false};
std::atomic<bool> g_learning{false};

// Interception context + worker thread
IC_Context g_ctx = NULL;
HANDLE     g_worker = NULL;

// Per-device hardware id cache
static WCHAR g_device_hwid[MAX_KBD_DEVICES + 1][256];

// Window to notify when learning completes (set by StartWorker)
static HWND g_notify_hwnd = NULL;

// Cached hardware id lookup (devices are stable for the process lifetime).
PCWSTR DeviceHwid(IC_Device dev) {
    if (dev < 1 || dev > MAX_KBD_DEVICES) return L"";
    if (g_device_hwid[dev][0] == 0) {
        WCHAR buf[256] = {0};
        p_get_hardware_id(g_ctx, dev, buf, sizeof(buf) - sizeof(WCHAR));
        buf[255] = 0;   // the api does not guarantee NUL termination
        StringCchCopyW(g_device_hwid[dev], 256, buf);
    }
    return g_device_hwid[dev];
}

// ---------------------------------------------------------------------------
// Worker thread: the interception loop. Runs for the whole app lifetime.
// ---------------------------------------------------------------------------
static DWORD WINAPI WorkerProc(LPVOID) {
    while (!g_stop_worker) {
        IC_Device dev = p_wait_with_timeout(g_ctx, 200);
        if (dev == 0)
            continue;                       // timeout -> re-check stop flag

        IC_Stroke raw;
        if (p_receive(g_ctx, dev, &raw, 1) <= 0)
            continue;

        PCWSTR hwid = DeviceHwid(dev);

        if (g_learning) {
            if (hwid[0] && wcsstr(hwid, L"VID_") == NULL) {
                // Not USB -> treat as the built-in keyboard.
                StringCchCopyW(g_internal_hwid, 256, hwid);
                g_learning = false;
                Log::Info(L"Learning success: internal keyboard hwid=%s", hwid);
                if (g_notify_hwnd)
                    PostMessageW(g_notify_hwnd, WM_APP_LEARNED, 0, 0);
                continue;                   // swallow the learning keystroke
            }
            // User pressed an external keyboard: ignore & forward it.
        }

        bool is_internal = (g_internal_hwid[0] != 0 &&
                            _wcsicmp(hwid, g_internal_hwid) == 0);
        if (is_internal && g_locked)
            continue;                       // frozen: swallow the keystroke

        p_send(g_ctx, dev, &raw, 1);        // pass through untouched
    }
    return 0;
}

bool StartWorker(HWND hwnd) {
    g_notify_hwnd = hwnd;

    g_ctx = p_create_context();
    if (!g_ctx) {
        Log::Error(L"interception_create_context failed - driver not installed?");
        return false;
    }
    p_set_filter(g_ctx, p_is_keyboard, IC_FILTER_KEY_ALL);
    g_stop_worker = false;
    g_worker = CreateThread(NULL, 0, WorkerProc, NULL, 0, NULL);
    if (g_worker) {
        // The worker forwards every keystroke; give it a small scheduling
        // edge so input latency stays flat even under heavy system load.
        SetThreadPriority(g_worker, THREAD_PRIORITY_ABOVE_NORMAL);
        Log::Info(L"Worker thread started (priority ABOVE_NORMAL)");
    }
    return g_worker != NULL;
}

void StopWorker() {
    g_stop_worker = true;
    if (g_worker) {
        WaitForSingleObject(g_worker, 3000);
        CloseHandle(g_worker);
        g_worker = NULL;
    }
    if (g_ctx) {
        p_destroy_context(g_ctx);           // driver auto-restores input
        g_ctx = NULL;
    }
    Log::Info(L"Worker stopped, driver context destroyed");
}
