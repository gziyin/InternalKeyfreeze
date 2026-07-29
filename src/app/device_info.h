// ============================================================================
// device_info.h
//
// Resolve a keyboard hardware id (as reported by the Interception driver) to a
// human-friendly device name via SetupAPI, for display in the device manager
// dialog.
// ============================================================================

#ifndef IKF_DEVICE_INFO_H
#define IKF_DEVICE_INFO_H

#include <windows.h>

// Fills |out| (|cch| wide chars) with a friendly description for the keyboard
// whose hardware id is |hwid| (e.g. "ACPI\\VEN_MSFT&DEV_0001" -> "Standard PS/2
// Keyboard"). Prefers SPDRP_FRIENDLYNAME, falls back to SPDRP_DEVICEDESC, and
// finally to |hwid| verbatim if nothing is found. Always NUL-terminates.
void GetDeviceFriendlyName(PCWSTR hwid, WCHAR* out, DWORD cch);

#endif // IKF_DEVICE_INFO_H
