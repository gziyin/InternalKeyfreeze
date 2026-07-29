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

// Present-keyboard snapshot, refreshed on demand by the worker (read by the
// device-manager dialog on WM_APP_DEVICES_READY).
WCHAR g_present_hwids[MAX_KBD_DEVICES + 1][256];
int   g_present_count = 0;
std::atomic<bool> g_rescan_devices{false};
HWND  g_devices_dlg = NULL;

// Per-device hardware id cache
static WCHAR g_device_hwid[MAX_KBD_DEVICES + 1][256];

// Window to notify when learning completes (set by StartWorker)
static HWND g_notify_hwnd = NULL;

// Learning state (worker-thread only)
static DWORD g_learning_start = 0;          // GetTickCount() when learning began
static DWORD g_last_candidate_time = 0;     // GetTickCount() of last new candidate
static bool g_was_learning = false;
static const DWORD LEARNING_TIMEOUT_MS = 8000;  // hard cap for the collect window
static const DWORD LEARNING_SETTLE_MS  = 1500;  // decide if no new candidate this long

// Hotkey combo state (worker-thread only): lets Ctrl+Shift+K toggle freeze
// even when the built-in keyboard is frozen and its keystrokes are swallowed.
static bool g_ctrl_down = false;
static bool g_shift_down = false;

// Modifier state tracking (worker-thread only). We track real modifier keys
// so we can inject matching Key-Up events when the keyboard is unfrozen.
// Without this, Key-Up events swallowed during freeze leave Shift/Ctrl/Alt/Win
// stuck down in the OS keyboard state.
struct TrackedModifier {
    const wchar_t* name;
    unsigned short code;
    unsigned short state_flags;  // INTERCEPTION_KEY_E0 / E1 when applicable
    bool pressed;
};

static TrackedModifier g_modifiers[] = {
    { L"LShift", 0x2A, 0,               false },
    { L"RShift", 0x36, 0,               false },
    { L"LCtrl",  0x1D, 0,               false },
    { L"RCtrl",  0x1D, INTERCEPTION_KEY_E0, false },
    { L"LAlt",   0x38, 0,               false },
    { L"RAlt",   0x38, INTERCEPTION_KEY_E0, false },
    { L"LWin",   0x5B, INTERCEPTION_KEY_E0, false },
    { L"RWin",   0x5C, INTERCEPTION_KEY_E0, false },
};

static bool g_was_locked = false;

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

// Update Ctrl/Shift tracking from a keystroke. Returns true if this stroke is
// the K-down of a Ctrl+Shift+K combo (and resets the held modifiers so the
// combo does not re-fire until they are pressed again).
static bool UpdateHotkeyCombo(const IC_Stroke& raw) {
    const IC_KeyStroke* k = reinterpret_cast<const IC_KeyStroke*>(&raw);
    bool down = (k->state & 0x01) == 0;   // 0 = key down, 1 = key up
    switch (k->code) {
        case 0x1D: g_ctrl_down = down; break;            // Ctrl (left or right/E0)
        case 0x2A: g_shift_down = down; break;           // Left Shift
        case 0x36: g_shift_down = down; break;           // Right Shift
        case 0x42:                                          // K
            if (down && g_ctrl_down && g_shift_down) {
                g_ctrl_down = g_shift_down = false;
                return true;
            }
            break;
    }
    return false;
}

// Update real modifier-key state from a keystroke.
static void UpdateModifierState(const IC_KeyStroke* k) {
    bool down = (k->state & INTERCEPTION_KEY_UP) == 0;
    unsigned short flags = k->state & (INTERCEPTION_KEY_E0 | INTERCEPTION_KEY_E1);
    for (size_t i = 0; i < ARRAYSIZE(g_modifiers); ++i) {
        if (k->code == g_modifiers[i].code && flags == g_modifiers[i].state_flags) {
            g_modifiers[i].pressed = down;
            break;
        }
    }
}

// Inject Key-Up events for any modifiers that are still considered pressed.
// This fixes the stuck-modifier problem after the keyboard is unfrozen: while
// frozen we swallow Key-Up events, so the OS still thinks Shift/Ctrl/Alt/Win
// are held down. We send the releases to every present keyboard device.
static void ReleaseAllModifiers() {
    for (size_t i = 0; i < ARRAYSIZE(g_modifiers); ++i) {
        if (!g_modifiers[i].pressed) continue;

        IC_Stroke raw;
        ZeroMemory(raw, sizeof(raw));
        IC_KeyStroke* k = reinterpret_cast<IC_KeyStroke*>(&raw);
        k->code = g_modifiers[i].code;
        k->state = INTERCEPTION_KEY_UP | g_modifiers[i].state_flags;
        k->information = 0;

        for (int dev = 1; dev <= MAX_KBD_DEVICES; ++dev) {
            if (DeviceHwid(dev)[0])
                p_send(g_ctx, dev, &raw, 1);
        }
        g_modifiers[i].pressed = false;
        Log::Info(L"Released stuck modifier: %s", g_modifiers[i].name);
    }
}

// Re-enumerate present keyboards into the snapshot. Worker-thread only.
static void RescanDevices() {
    for (int dev = 1; dev <= MAX_KBD_DEVICES; dev++)
        g_device_hwid[dev][0] = 0;   // drop stale cache so unplugs are reflected

    int n = 0;
    for (int dev = 1; dev <= MAX_KBD_DEVICES && n < MAX_KBD_DEVICES; dev++) {
        PCWSTR hwid = DeviceHwid(dev);
        if (hwid && hwid[0]) {
            StringCchCopyW(g_present_hwids[n], 256, hwid);
            n++;
        }
    }
    g_present_count = n;
    Log::Info(L"Device scan: %d keyboard(s) present", n);
}

// End the learning collect window and decide what to do with the candidates.
static void FinishLearning() {
    g_learning = false;
    g_was_learning = false;

    if (g_candidate_count == 0) {
        Log::Info(L"Learning ended: no candidate detected");
        if (g_notify_hwnd)
            PostMessageW(g_notify_hwnd, WM_APP_LEARN_FAILED, 0, 0);
        return;
    }

    if (g_candidate_count == 1) {
        AddFrozenDevice(g_candidates[0].hwid);
        Log::Info(L"Learning success (single candidate): hwid=%s",
                  g_candidates[0].hwid);
        if (g_notify_hwnd)
            PostMessageW(g_notify_hwnd, WM_APP_LEARNED, 0, 0);
        return;
    }

    // Multiple candidates: sort by score descending (insertion sort) so the
    // most likely built-in keyboard is offered first.
    for (int i = 1; i < g_candidate_count; i++) {
        DeviceCandidate tmp = g_candidates[i];
        int j = i;
        while (j > 0 && g_candidates[j - 1].score < tmp.score) {
            g_candidates[j] = g_candidates[j - 1];
            j--;
        }
        g_candidates[j] = tmp;
    }
    Log::Info(L"Learning ended: %d candidates, requesting user selection",
              g_candidate_count);
    if (g_notify_hwnd)
        PostMessageW(g_notify_hwnd, WM_APP_MULTI_CAND, 0, 0);
}

// ---------------------------------------------------------------------------
// Worker thread: the interception loop. Runs for the whole app lifetime.
// ---------------------------------------------------------------------------
static DWORD WINAPI WorkerProc(LPVOID) {
    while (!g_stop_worker) {
        // Service on-demand device rescans from the device-manager dialog.
        if (g_rescan_devices.exchange(false)) {
            RescanDevices();
            if (g_devices_dlg)
                PostMessageW(g_devices_dlg, WM_APP_DEVICES_READY, 0, 0);
        }

        // Detect transition from frozen to unfrozen. Any modifier Key-Up events
        // swallowed while frozen would leave Shift/Ctrl/Alt/Win stuck down in
        // the OS, so inject compensating Key-Up events on the thaw edge.
        bool currently_locked = g_locked.load();
        if (g_was_locked && !currently_locked)
            ReleaseAllModifiers();
        g_was_locked = currently_locked;

        IC_Device dev = p_wait_with_timeout(g_ctx, 200);
        if (dev == 0) {
            // Timeout: check if learning should end (settle or hard cap).
            if (g_learning && g_was_learning) {
                DWORD now = GetTickCount();
                bool settle = (g_candidate_count >= 1 &&
                               now - g_last_candidate_time > LEARNING_SETTLE_MS);
                bool expired = (now - g_learning_start > LEARNING_TIMEOUT_MS);
                if (settle || expired)
                    FinishLearning();
            }
            continue;
        }

        IC_Stroke raw;
        if (p_receive(g_ctx, dev, &raw, 1) <= 0)
            continue;

        const IC_KeyStroke* k = reinterpret_cast<const IC_KeyStroke*>(&raw);
        UpdateModifierState(k);

        PCWSTR hwid = DeviceHwid(dev);

        // Track Ctrl/Shift so the global hotkey still works on the built-in
        // keyboard while it is frozen (its keystrokes are swallowed below and
        // never reach the OS-level RegisterHotKey).
        bool combo_fired = UpdateHotkeyCombo(raw);

        if (g_learning) {
            // Detect transition into learning mode
            if (!g_was_learning) {
                g_was_learning = true;
                g_learning_start = GetTickCount();
                g_last_candidate_time = g_learning_start;
                g_candidate_count = 0;
                Log::Info(L"Learning mode started");
            }

            if (hwid[0]) {
                int score = DeviceScore(hwid);
                if (score > 0) {
                    AddCandidate(hwid, score);
                    g_last_candidate_time = GetTickCount();
                }
            }
            // Forward the keystroke during the collection phase (fall through).
        }

        bool is_internal = IsDeviceFrozen(hwid);
        if (is_internal && g_locked) {
            // Frozen built-in keyboard: swallow the keystroke, but let the
            // Ctrl+Shift+K toggle combo through to the UI.
            if (combo_fired && g_notify_hwnd)
                PostMessageW(g_notify_hwnd, WM_APP_HOTKEY_FIRED, 0, 0);
            continue;
        }

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
