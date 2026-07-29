// ============================================================================
// device_info.cpp
// ============================================================================

#include "device_info.h"
#include <initguid.h>    // make DEFINE_GUID emit the actual symbols
#include <devguid.h>     // GUID_DEVCLASS_KEYBOARD
#include <setupapi.h>
#include <strsafe.h>

// Returns true if |hwid| refers to the SetupAPI device described by |instance_id|
// and the REG_MULTI_SZ |hwids|. Matches case-insensitively against any hardware
// id, or as the stem of the device instance path (hwid == "ACPI\\..." and
// instance_id == "ACPI\\...\\4&...").
static bool HwidMatches(PCWSTR hwid, PCWSTR instance_id, PCWSTR hwids) {
    if (!hwid || !hwid[0]) return false;

    // Direct match against any hardware id in the multi-sz.
    if (hwids) {
        for (PCWSTR p = hwids; *p; p += wcslen(p) + 1) {
            if (_wcsicmp(p, hwid) == 0)
                return true;
        }
    }

    // Stem match against the device instance path.
    if (instance_id) {
        size_t n = wcslen(hwid);
        if (_wcsnicmp(instance_id, hwid, n) == 0 &&
            (instance_id[n] == 0 || instance_id[n] == L'\\'))
            return true;
    }
    return false;
}

void GetDeviceFriendlyName(PCWSTR hwid, WCHAR* out, DWORD cch) {
    if (!out || cch == 0) return;
    out[0] = 0;

    if (!hwid || !hwid[0]) {
        StringCchCopyW(out, cch, L"(unknown)");
        return;
    }

    HDEVINFO devs = SetupDiGetClassDevsW(&GUID_DEVCLASS_KEYBOARD, NULL, NULL,
                                         DIGCF_PRESENT);
    if (devs == INVALID_HANDLE_VALUE) {
        StringCchCopyW(out, cch, hwid);
        return;
    }

    SP_DEVINFO_DATA info;
    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(devs, i, &info); i++) {
        WCHAR instance_id[512] = {0};
        SetupDiGetDeviceInstanceIdW(devs, &info, instance_id,
                                    ARRAYSIZE(instance_id), NULL);

        WCHAR hwids[2048] = {0};
        SetupDiGetDeviceRegistryPropertyW(devs, &info, SPDRP_HARDWAREID, NULL,
                                          (PBYTE)hwids, sizeof(hwids), NULL);

        if (!HwidMatches(hwid, instance_id, hwids))
            continue;

        // Prefer the friendly name, fall back to the device description.
        WCHAR name[256] = {0};
        if ((SetupDiGetDeviceRegistryPropertyW(devs, &info, SPDRP_FRIENDLYNAME,
                                                NULL, (PBYTE)name, sizeof(name), NULL) && name[0]) ||
            (SetupDiGetDeviceRegistryPropertyW(devs, &info, SPDRP_DEVICEDESC,
                                                NULL, (PBYTE)name, sizeof(name), NULL) && name[0])) {
            StringCchCopyW(out, cch, name);
        } else {
            StringCchCopyW(out, cch, hwid);
        }
        SetupDiDestroyDeviceInfoList(devs);
        return;
    }

    SetupDiDestroyDeviceInfoList(devs);
    StringCchCopyW(out, cch, hwid);   // not found -> raw hardware id
}
