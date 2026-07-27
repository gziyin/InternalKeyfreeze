// ============================================================================
// keyboard_filter.h
//
// Worker thread + device identification logic.
// Runs the Interception receive loop for the entire app lifetime:
//   Unlocked -> forward every keystroke (transparent pass-through)
//   Locked   -> swallow keystrokes from the learned built-in keyboard
//   Learning -> first non-USB keystroke defines the built-in keyboard
// ============================================================================

#ifndef IKF_KEYBOARD_FILTER_H
#define IKF_KEYBOARD_FILTER_H

#include <windows.h>
#include <atomic>
#include "interception_loader.h"

// Window message: posted by the worker thread when device learning completes.
// Handled by tray_app's WndProc.
#define WM_APP_LEARNED   (WM_USER + 2)

// Atomic flags shared with tray_app
extern std::atomic<bool> g_stop_worker;
extern std::atomic<bool> g_locked;      // swallow built-in keystrokes
extern std::atomic<bool> g_learning;    // next built-in keystroke identifies it

// Interception context + worker thread handle
extern IC_Context g_ctx;
extern HANDLE     g_worker;

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
