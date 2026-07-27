// ============================================================================
// config.h
//
// INI configuration read/write for the learned keyboard hardware id.
// Will be extended in Phase 4 to support multiple devices.
// ============================================================================

#ifndef IKF_CONFIG_H
#define IKF_CONFIG_H

#include <windows.h>

// The learned built-in keyboard hardware id (shared with keyboard_filter).
extern WCHAR g_internal_hwid[256];

// Fills |path| with the full path to InternalKeyfreeze.ini (next to the exe).
void ConfigPath(WCHAR* path, DWORD cch);

// Loads g_internal_hwid from the [keyboard] hwid= entry in the INI.
void LoadConfig();

// Saves g_internal_hwid to the [keyboard] hwid= entry in the INI.
void SaveConfig();

#endif // IKF_CONFIG_H
