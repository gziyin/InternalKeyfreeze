// ============================================================================
// config.h
//
// INI configuration for frozen keyboard devices.
// Supports multiple devices with backward-compatible migration from
// the legacy single-hwid [keyboard] format.
// ============================================================================

#ifndef IKF_CONFIG_H
#define IKF_CONFIG_H

#include <windows.h>

// Maximum number of frozen devices supported.
#define MAX_FROZEN_DEVICES 8

// The frozen device hardware id list (shared with keyboard_filter and tray_app).
extern WCHAR g_frozen_hwids[MAX_FROZEN_DEVICES][256];
extern int  g_frozen_device_count;

// ---- Device list operations ----

// Returns true if |hwid| is in the frozen device list (case-insensitive).
bool IsDeviceFrozen(PCWSTR hwid);

// Returns the index of |hwid| in the frozen device list, or -1 if not present.
int FindFrozenIndex(PCWSTR hwid);

// Adds a device to the frozen list (if not already present and list not full).
// Returns true if added, false if duplicate or full.
bool AddFrozenDevice(PCWSTR hwid);

// Removes the device at |index| from the frozen list.
void RemoveFrozenDevice(int index);

// Clears all devices from the frozen list.
void ClearFrozenDevices();

// ---- INI file operations ----

// Fills |path| with the full path to InternalKeyfreeze.ini (next to the exe).
void ConfigPath(WCHAR* path, DWORD cch);

// Loads the frozen device list from the INI file.
// Automatically migrates from the legacy [keyboard] hwid= format.
void LoadConfig();

// Saves the frozen device list to the INI file (new [devices] format).
void SaveConfig();

#endif // IKF_CONFIG_H
