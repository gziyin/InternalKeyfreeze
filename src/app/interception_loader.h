// ============================================================================
// interception_loader.h
//
// Dynamically loads interception.dll and exposes its function pointers.
// This is the base layer — all other modules depend on it for the IC_* types
// and the loaded SDK entry points.
// ============================================================================

#ifndef IKF_INTERCEPTION_LOADER_H
#define IKF_INTERCEPTION_LOADER_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Pull in INTERCEPTION_MAX_KEYBOARD etc. from the SDK header.
// INTERCEPTION_STATIC avoids __declspec(dllimport) since we load the DLL dynamically.
#define INTERCEPTION_STATIC
#include "interception.h"   // resolved via -I sdk/library in build.bat

// ---------------------------------------------------------------------------
// Interception ABI - mirrors interception.h, loaded dynamically so the app
// can show a friendly message when the dll/driver is missing.
// NOTE: the dll uses the C (__cdecl) calling convention - do NOT add WINAPI.
// ---------------------------------------------------------------------------
typedef void* IC_Context;
typedef int   IC_Device;
typedef int   (*IC_Predicate)(IC_Device);

typedef struct {                     // InterceptionKeyStroke (first 8 bytes)
    unsigned short code;             // scan code
    unsigned short state;            // 0=down 1=up 2=E0 4=E1
    unsigned int   information;
} IC_KeyStroke;

typedef char IC_Stroke[20];          // InterceptionStroke = sizeof(MouseStroke)

#define IC_FILTER_KEY_ALL  0xFFFF
#define MAX_KBD_DEVICES    INTERCEPTION_MAX_KEYBOARD

// ---- Function pointer typedefs ----
typedef IC_Context   (__cdecl *FN_create_context)(void);
typedef void         (__cdecl *FN_destroy_context)(IC_Context);
typedef void         (__cdecl *FN_set_filter)(IC_Context, IC_Predicate, unsigned short);
typedef int          (__cdecl *FN_is_keyboard)(IC_Device);
typedef IC_Device    (__cdecl *FN_wait_timeout)(IC_Context, unsigned long);
typedef int          (__cdecl *FN_receive)(IC_Context, IC_Device, void*, unsigned int);
typedef int          (__cdecl *FN_send)(IC_Context, IC_Device, const void*, unsigned int);
typedef unsigned int (__cdecl *FN_get_hwid)(IC_Context, IC_Device, void*, unsigned int);

// ---- Global function pointers (populated by LoadInterception) ----
extern FN_create_context  p_create_context;
extern FN_destroy_context p_destroy_context;
extern FN_set_filter      p_set_filter;
extern FN_is_keyboard     p_is_keyboard;
extern FN_wait_timeout    p_wait_with_timeout;
extern FN_receive         p_receive;
extern FN_send            p_send;
extern FN_get_hwid        p_get_hardware_id;

// ---- API ----

// Loads interception.dll and resolves all exports.
// Returns true on success, false if the DLL or any export is missing.
bool LoadInterception();

// Unloads the DLL (if loaded). Called at shutdown.
void ShutdownInterception();

#endif // IKF_INTERCEPTION_LOADER_H
