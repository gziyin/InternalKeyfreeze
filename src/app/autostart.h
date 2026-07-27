// ============================================================================
// autostart.h
//
// Manages Windows startup registration via the registry key:
//   HKCU\Software\Microsoft\Windows\CurrentVersion\Run
// ============================================================================

#ifndef IKF_AUTOSTART_H
#define IKF_AUTOSTART_H

#include <windows.h>

// Returns true if the app is registered to start with Windows.
bool IsAutostartEnabled();

// Registers the app to start with Windows.
// Returns true on success.
bool EnableAutostart();

// Removes the app from Windows startup.
// Returns true on success.
bool DisableAutostart();

// Toggles autostart on/off. Returns the new state.
bool ToggleAutostart();

#endif // IKF_AUTOSTART_H
