// ============================================================================
// interception_loader.cpp
//
// Dynamically loads interception.dll and resolves all required exports.
// ============================================================================

#include "interception_loader.h"
#include "logger.h"

static HMODULE g_dll = NULL;

// Function pointer definitions (declared extern in header)
FN_create_context  p_create_context    = nullptr;
FN_destroy_context p_destroy_context   = nullptr;
FN_set_filter      p_set_filter        = nullptr;
FN_is_keyboard     p_is_keyboard       = nullptr;
FN_wait_timeout    p_wait_with_timeout = nullptr;
FN_receive         p_receive           = nullptr;
FN_send            p_send              = nullptr;
FN_get_hwid        p_get_hardware_id   = nullptr;

bool LoadInterception() {
    g_dll = LoadLibraryW(L"interception.dll");
    if (!g_dll) {
        Log::Error(L"LoadLibrary(interception.dll) failed, err=%lu", GetLastError());
        return false;
    }
    p_create_context    = (FN_create_context) GetProcAddress(g_dll, "interception_create_context");
    p_destroy_context   = (FN_destroy_context)GetProcAddress(g_dll, "interception_destroy_context");
    p_set_filter        = (FN_set_filter)     GetProcAddress(g_dll, "interception_set_filter");
    p_is_keyboard       = (FN_is_keyboard)    GetProcAddress(g_dll, "interception_is_keyboard");
    p_wait_with_timeout = (FN_wait_timeout)   GetProcAddress(g_dll, "interception_wait_with_timeout");
    p_receive           = (FN_receive)        GetProcAddress(g_dll, "interception_receive");
    p_send              = (FN_send)           GetProcAddress(g_dll, "interception_send");
    p_get_hardware_id   = (FN_get_hwid)       GetProcAddress(g_dll, "interception_get_hardware_id");

    bool ok = p_create_context && p_destroy_context && p_set_filter && p_is_keyboard &&
              p_wait_with_timeout && p_receive && p_send && p_get_hardware_id;
    if (ok)
        Log::Info(L"interception.dll loaded, all 8 exports resolved");
    else
        Log::Error(L"interception.dll loaded but some exports missing");
    return ok;
}

void ShutdownInterception() {
    if (g_dll) {
        FreeLibrary(g_dll);
        g_dll = NULL;
    }
    p_create_context    = nullptr;
    p_destroy_context   = nullptr;
    p_set_filter        = nullptr;
    p_is_keyboard       = nullptr;
    p_wait_with_timeout = nullptr;
    p_receive           = nullptr;
    p_send              = nullptr;
    p_get_hardware_id   = nullptr;
}
