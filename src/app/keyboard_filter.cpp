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

// Device candidates collected during learning
DeviceCandidate g_candidates[MAX_CANDIDATES];
int g_candidate_count = 0;

// Per-device hardware id cache
static WCHAR g_device_hwid[MAX_KBD_DEVICES + 1][256];

// Window to notify when learning completes (set by StartWorker)
static HWND g_notify_hwnd = NULL;

// Learning state (worker-thread only)
static DWORD g_learning_start = 0;  // GetTickCount() when learning began
static bool g_was_learning = false;
static const DWORD LEARNING_TIMEOUT_MS = 10000;  // 10 seconds

// ---------------------------------------------------------------------------
// Device identification: score how likely a hwid is the built-in keyboard.
//
// Positive patterns (built-in):
//   ACPI\...     - PS/2 / ACPI enumerated (most laptop built-in keyboards)
//   PNP03xx      - PS/2 keyboard PnP device ID
//   MSFT0001     - ACPI virtual keyboard on modern laptops
//
// Negative patterns (external):
//   VID_         - USB vendor ID (USB keyboard)
//   BTHENUM      - Bluetooth enumerator
//   HID\         - HID-over-USB (usually external)
//
// Returns 0-100. >=80 is high confidence; 0 is definitely external.
// ---------------------------------------------------------------------------
int DeviceScore(PCWSTR hwid) {
    if (!hwid || !hwid[0]) return 0;

    // --- Negative patterns (external keyboards) ---
    if (wcsstr(hwid, L"VID_"))    return 0;   // USB keyboard
    if (wcsstr(hwid, L"BTHENUM")) return 0;   // Bluetooth keyboard
    if (wcsstr(hwid, L"bthenum")) return 0;   // (case-insensitive)

    // --- Positive patterns (built-in keyboards) ---
    int score = 30;  // base: unknown but not obviously external

    // Case-insensitive prefix/pattern checks
    if (_wcsnicmp(hwid, L"ACPI\\", 5) == 0)   score += 50;
    if (wcsstr(hwid, L"PNP03"))                score += 20;
    if (wcsstr(hwid, L"pnp03"))                score += 20;
    if (wcsstr(hwid, L"MSFT0001"))             score += 20;

    // HID\ without ACPI prefix is suspicious (likely USB-HID)
    if (_wcsnicmp(hwid, L"HID\\", 4) == 0 && _wcsnicmp(hwid, L"ACPI\\", 5) != 0)
        score -= 15;

    if (score > 100) score = 100;
    if (score < 0)   score = 0;
    return score;
}

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

// Check if a hwid is already in the candidates list.
static bool IsCandidateKnown(PCWSTR hwid) {
    for (int i = 0; i < g_candidate_count; i++) {
        if (_wcsicmp(g_candidates[i].hwid, hwid) == 0)
            return true;
    }
    return false;
}

// Add a device to the candidates list (if not already present).
static void AddCandidate(PCWSTR hwid, int score) {
    if (g_candidate_count >= MAX_CANDIDATES) return;
    if (IsCandidateKnown(hwid)) return;
    StringCchCopyW(g_candidates[g_candidate_count].hwid, 256, hwid);
    g_candidates[g_candidate_count].score = score;
    g_candidate_count++;
    Log::Info(L"Candidate #%d: hwid=%s score=%d", g_candidate_count, hwid, score);
}

// ---------------------------------------------------------------------------
// Worker thread: the interception loop. Runs for the whole app lifetime.
// ---------------------------------------------------------------------------
static DWORD WINAPI WorkerProc(LPVOID) {
    while (!g_stop_worker) {
        IC_Device dev = p_wait_with_timeout(g_ctx, 200);
        if (dev == 0) {
            // Timeout: check if learning has expired
            if (g_learning && g_was_learning) {
                if (GetTickCount() - g_learning_start > LEARNING_TIMEOUT_MS) {
                    // Learning timeout: notify UI of collected candidates
                    g_learning = false;
                    g_was_learning = false;
                    if (g_candidate_count > 1) {
                        Log::Info(L"Learning timeout: %d candidates collected, requesting user selection",
                                  g_candidate_count);
                        if (g_notify_hwnd)
                            PostMessageW(g_notify_hwnd, WM_APP_MULTI_CAND, 0, 0);
                    } else if (g_candidate_count == 1) {
                        // Only one candidate: auto-select it
                        AddFrozenDevice(g_candidates[0].hwid);
                        Log::Info(L"Learning timeout: auto-selected single candidate hwid=%s",
                                  g_candidates[0].hwid);
                        if (g_notify_hwnd)
                            PostMessageW(g_notify_hwnd, WM_APP_LEARNED, 0, 0);
                    }
                }
            }
            continue;
        }

        IC_Stroke raw;
        if (p_receive(g_ctx, dev, &raw, 1) <= 0)
            continue;

        PCWSTR hwid = DeviceHwid(dev);

        if (g_learning) {
            // Detect transition into learning mode
            if (!g_was_learning) {
                g_was_learning = true;
                g_learning_start = GetTickCount();
                g_candidate_count = 0;
                Log::Info(L"Learning mode started");
            }

            if (hwid[0]) {
                int score = DeviceScore(hwid);

                if (score >= 80) {
                    // High confidence: immediately select this device
                    AddFrozenDevice(hwid);
                    g_learning = false;
                    g_was_learning = false;
                    Log::Info(L"Learning success (score=%d): internal keyboard hwid=%s",
                              score, hwid);
                    if (g_notify_hwnd)
                        PostMessageW(g_notify_hwnd, WM_APP_LEARNED, 0, 0);
                    continue;                   // swallow the learning keystroke
                }

                if (score > 0) {
                    // Low-medium confidence: add to candidates
                    AddCandidate(hwid, score);
                }
            }
            // Forward the keystroke (don't swallow during collection phase)
        }

        bool is_internal = IsDeviceFrozen(hwid);
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
