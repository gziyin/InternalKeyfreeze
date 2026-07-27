# InternalKeyfreeze

[中文](README.md) | **English**

<p align="center"><img src="assets/icon-256.png" width="140" alt="InternalKeyfreeze icon"></p>

A Windows tray utility that **freezes the laptop's built-in keyboard** while leaving **external USB / Bluetooth keyboards** fully functional.
Modeled after [OpenKeyfreeze](https://github.com/sukibaby/OpenKeyfreeze), but instead of blocking all keyboards indiscriminately, it only locks the built-in one.

## How It Works

| Approach | Why it doesn't work / Why it does |
|----------|-----------------------------------|
| `WH_KEYBOARD_LL` low-level hook (OpenKeyfreeze's approach) | `KBDLLHOOKSTRUCT` carries no device info — **cannot tell which keyboard a keystroke came from** |
| SetupAPI device-level disable | Works, but if the app crashes or is killed, the device **stays disabled persistently** (registry ConfigFlags survive reboots) — built-in keyboard is locked, requires manual recovery |
| **Interception kernel filter driver** (this project) | The driver intercepts each keystroke in the kernel keyboard stack and **tags it with the source device** — the user-mode app forwards external keyboards and swallows built-in ones |

Per-keystroke overhead is ~0.05ms, far below USB polling (8ms@125Hz) and the human perception threshold (~50ms) — unnoticeable in typing or gaming.

## System Requirements & Compatibility

### Supported Environment

| Requirement | Details |
|-------------|---------|
| OS | Windows 10 / 11 **64-bit** (Interception only has x64 driver) |
| Built-in keyboard | **PS/2 interface** (ACPI-enumerated, e.g. `ACPI\MSFT0001`, `ACPI\PNP0303`) |
| External keyboard | USB / Bluetooth keyboards both work |
| Privileges | Driver install requires admin; daily use does not |

> **Check your built-in keyboard type**: Device Manager → Keyboards → find the built-in keyboard → right-click → Properties → Details → "Device instance path". If it starts with `ACPI\`, it's PS/2 (supported); if it starts with `USB\` or `HID\`, it's USB-attached (not supported, see below).

### Unsupported Environment

- **macOS**: Interception is a Windows kernel driver; macOS uses the XNU kernel with a completely different driver model — **cannot be ported**. MacBook built-in keyboards are also USB-attached, so the identification logic doesn't apply.
- **Laptops with USB-attached built-in keyboards** (e.g. some Surface models, ultra-thins): The app identifies the built-in keyboard by "hwid does not contain `VID_`" (i.e. non-USB). A USB-attached built-in keyboard has `VID_` in its hwid and would be mistaken for an external keyboard.

### Driver won't install? Three blockers

Interception v1.0.1 is a 2018 driver (project is unmaintained). On newer Windows it may be blocked by:

| Blocker | Symptom | Fix |
|---------|---------|-----|
| **Driver signature enforcement** (Win10 1607+ 64-bit) | Driver fails to load, `sc query keyboard` shows STOPPED | Admin CMD: `bcdedit /set testsigning on` + reboot |
| **Secure Boot** (BIOS) | Driver still fails even with test signing | Disable Secure Boot in BIOS |
| **HVCI Memory Integrity** (Win10 1809+) | Driver fails, Event Viewer reports "does not meet HVCI requirements" | Settings → Device security → Core isolation → turn off "Memory integrity" + reboot |

> **Check if driver loaded**: Admin CMD `sc query keyboard` should show `STATE: 4 RUNNING`. If `STOPPED`, see the table above.

### Can't detect the built-in keyboard?

If the built-in keyboard doesn't respond and `InternalKeyfreeze.ini` has an empty `hwid=`, possible causes:

1. **Built-in keyboard is disabled** (e.g. previously ran the v1 SetupAPI approach which didn't exit cleanly, or manually disabled in Device Manager)
   - Device Manager → View → Show hidden devices → Keyboards → PS/2 Standard Keyboard (greyed out) → right-click → Enable device
   - Or admin PowerShell: `Enable-PnpDevice -InstanceId "ACPI\MSFT0001\4&6ea17be&0" -Confirm:$false` (replace with your actual instance ID)
2. **Built-in keyboard is USB-attached**: See "Unsupported Environment" above
3. **Driver didn't load**: See "Three blockers" above

## Directory Structure

```
InternalKeyfreeze\
├─ README.md                    This file (Chinese)
├─ README.en.md                 English readme
├─ LICENSE                      MIT license (project's own code)
├─ tools\                       Build scripts (see "Building from Source")
│   ├─ build_icons.bat          Regenerate icon assets (assets/*.ico)
│   └─ build.bat                One-click build: icons → resource → link exe
├─ 安装.bat                     One-click install (double-click, auto-elevates)
├─ 卸载.bat                     One-click uninstall (double-click, auto-elevates)
├─ Interception.zip             Interception v1.0.1 official release (archive)
├─ assets\                      Icon source & generated outputs (icon-master.png / *.ico / preview pngs)
├─ build\                       Build intermediates (.o / .res, gitignored)
├─ bin\
│   ├─ InternalKeyfreeze.exe    Main program (click this for daily use)
│   └─ interception.dll         Runtime library (must be next to the exe)
├─ driver\
│   ├─ install-driver.bat       Install driver (right-click → Run as admin, once)
│   ├─ UninstallDriver.exe      Uninstall driver (double-click, UAC prompt)
│   └─ install-interception.exe Official driver installer (used by both above)
├─ src\
│   ├─ InternalKeyfreeze.cpp    Main program source
│   ├─ InternalKeyfreeze.rc     Windows resource script (icon definitions)
│   ├─ resources.h              Resource ID header
│   ├─ UninstallDriver.cpp      Uninstaller source
│   ├─ UninstallDriver.manifest Uninstaller UAC manifest
│   └─ legacy\                  v1 archive (SetupAPI disable approach, unusable on this machine)
└─ sdk\                         Interception SDK (headers / lib / licenses / samples)
```

## Installation

### Option 1: One-click install (recommended, for regular users)

Download `InternalKeyfreeze-v2.1.zip` from [GitHub Release](https://github.com/gziyin/InternalKeyfreeze/releases), extract, then:

1. **Double-click `安装.bat`** (UAC prompt will appear, confirm it)
2. **Reboot**

The install script automatically: installs the driver → copies files to `C:\Program Files\InternalKeyfreeze\` → creates desktop/start-menu shortcuts. After reboot, double-click the desktop shortcut to run.

### Option 2: Manual install (for developers)

1. Right-click `driver\install-driver.bat` → **Run as administrator**
2. **Reboot** (the filter driver loads at boot, reboot is required)

## Usage

1. Run `bin\InternalKeyfreeze.exe` (no admin required)
2. First left-click on the tray icon → prompt "press any key on the built-in keyboard" → press any key on the laptop keyboard
   → The hardware ID is saved to `bin\InternalKeyfreeze.ini` and the keyboard is frozen immediately
3. **Left-click** the tray icon = toggle freeze/unfreeze; **right-click** = re-identify / exit
4. If the app exits or crashes, the driver automatically restores input — the built-in keyboard can never be permanently locked; Ctrl+Alt+Del always works

## Uninstall

### Option 1: One-click uninstall (recommended)

Double-click `卸载.bat` (auto-elevates) → automatically kills the process, uninstalls the driver, deletes `C:\Program Files\InternalKeyfreeze\`, and cleans up shortcuts → reboot to fully remove.

### Option 2: Manual uninstall

Double-click `driver\UninstallDriver.exe` (UAC prompt) → it automatically terminates any running InternalKeyfreeze, calls the official installer to uninstall the driver, and asks whether to reboot immediately.
**After reboot** the driver is fully removed. The app files remain in `bin\` — delete the project folder for a complete cleanup.

## Building from Source

Main program (no libraries to link — the dll is loaded dynamically at runtime). The icon is embedded into the exe via the Windows resource script `src/InternalKeyfreeze.rc`; run `tools/build_icons.bat` first to generate `assets/*.ico` (once, or whenever you change the icon). Alternatively, run `tools/build.bat` to do everything in one step (icons → resource compile → link exe).

```bat
:: 1) Generate icon assets (first time / after changing the icon)
tools\build_icons.bat
::    or one-shot: tools\build.bat  (runs steps 1→3 for you)

:: 2) MSVC — compile resource and link
rc src\InternalKeyfreeze.rc
cl /EHsc /W4 src\InternalKeyfreeze.cpp src\InternalKeyfreeze.res /link /SUBSYSTEM:WINDOWS /OUT:bin\InternalKeyfreeze.exe

:: 3) MinGW-w64 — compile resource to COFF and link
windres --output-format=coff -i src\InternalKeyfreeze.rc -o build\InternalKeyfreeze.res.o
g++ -O2 -municode -mwindows src\InternalKeyfreeze.cpp build\InternalKeyfreeze.res.o -o bin\InternalKeyfreeze.exe
```

> `build\` is the build-intermediate directory (already in `.gitignore`) and does not enter the repo.

Uninstaller (requires embedded manifest, MinGW example):

```bat
cd src
printf '1 24 "UninstallDriver.manifest"\n' > ud.rc
windres ud.rc -O coff -o ud.res
g++ -O2 -municode -mwindows UninstallDriver.cpp ud.res -o ..\driver\UninstallDriver.exe
del ud.rc ud.res
```

## Dependencies & License

- **Interception** by oblitum — https://github.com/oblitum/Interception (v1.0.1)
  Keyboard/mouse kernel filter driver. Non-commercial use follows **LGPL 3.0** (see `sdk\licenses\`),
  commercial use requires a commercial license. The original release package is archived in `Interception.zip`.
- This project's own code: use freely (MIT).
