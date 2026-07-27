// ============================================================================
// keyboard_filter.h
//
// Worker thread + device identification logic.
// Runs the Interception receive loop for the entire app lifetime:
//   Unlocked -> forward every keystroke (transparent pass-through)
//   Locked   -> swallow keystrokes from the learned built-in keyboard
//   Learning -> first high-confidence keystroke defines the built-in keyboard
// ============================================================================

#ifndef IKF_KEYBOARD_FILTER_H
#define IKF_KEYBOARD_FILTER_H

#include <windows.h>
#include <atomic>
#include "interception_loader.h"

// Window messages posted by the worker thread:
#define WM_APP_LEARNED      (WM_USER + 2)   // learning completed (single candidate)
#define WM_APP_MULTI_CAND   (WM_USER + 3)   // multiple candidates detected

// Maximum number of device candidates collected during learning.
#define MAX_CANDIDATES 8

// A device candidate collected during learning mode.
struct DeviceCandidate {
    WCHAR hwid[256];
    int   score;       // 0-100 confidence that this is the built-in keyboard
};

// Atomic flags shared with tray_app
extern std::atomic<bool> g_stop_worker;
extern std::atomic<bool> g_locked;      // swallow built-in keystrokes
extern std::atomic<bool> g_learning;    // next built-in keystroke identifies it

// Interception context + worker thread handle
extern IC_Context g_ctx;
extern HANDLE     g_worker;

// Device candidates collected during learning (read by tray_app on WM_APP_MULTI_CAND).
extern DeviceCandidate g_candidates[MAX_CANDIDATES];
extern int g_candidate_count;

// Scores how likely a hardware id belongs to the built-in keyboard.
// Returns 0-100 (0 = definitely external, 100 = definitely built-in).
int DeviceScore(PCWSTR hwid);

// Cached hardware id lookup (devices are stable for the process lifetime).
// Returns a pointer into the per-device cache, or L"" for invalid devices.
PCWSTR DeviceHwid(IC_Device dev);

// Creates the interception context, sets the keyboard filter, and starts
// the worker thread. Returns false if the driver is not installed.
bool StartWorker(HWND hwnd);

// Signals the worker to stop, waits for it, and destroys the context.
// The driver auto-restores normal input when the context is closed.
void StopWorker();

#endif // IKF_KEYBOARD_FILTER_H
