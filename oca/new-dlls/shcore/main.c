/*++

Copyright (c) 2022 Shorthorn Project

Module Name:

    dpi.c

Abstract:

    Implement DPI Scallig functions

Author:

    Skulltrail 14-June-2024

Revision History:

--*/

#include <main.h>

WINE_DEFAULT_DEBUG_CHANNEL(shcore);

static DWORD shcore_tls;
// BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
// {
    // TRACE("(%p, %u, %p)\n", instance, reason, reserved);

    // switch (reason)
    // {
        // case DLL_PROCESS_ATTACH:
            // DisableThreadLibraryCalls(instance);
            // shcore_tls = TlsAlloc();
            // break;
        // case DLL_PROCESS_DETACH:
            // if (reserved) break;
            // if (shcore_tls != TLS_OUT_OF_INDEXES)
                // TlsFree(shcore_tls);
            // break;
    // }

    // return TRUE;
// }
BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,  
    DWORD fdwReason,     
    LPVOID lpvReserved )  
{
    HMODULE ignored;
    switch( fdwReason ) 
    { 
        case DLL_PROCESS_ATTACH:
         // Should be OK
            DisableThreadLibraryCalls(hinstDLL);
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"shcore.dll", &ignored); // It's incredibly unlikely that something will load shcore.dll and unload it later on. Android Studio unloads it, and still calls functions inside it, which will crash without this code
            shcore_tls = TlsAlloc();
			break;
        case DLL_PROCESS_DETACH:
            if (lpvReserved) break;
            if (shcore_tls != TLS_OUT_OF_INDEXES)
                TlsFree(shcore_tls);
            break;			
        default:
            break;
    }
    return TRUE;
}

/*************************************************************************
 * SHGetThreadRef        [SHCORE.@]
 */
HRESULT WINAPI SHGetThreadRef(IUnknown **out)
{
    TRACE("(%p)\n", out);

    if (shcore_tls == TLS_OUT_OF_INDEXES)
        return E_NOINTERFACE;

    *out = TlsGetValue(shcore_tls);
    if (!*out)
        return E_NOINTERFACE;

    IUnknown_AddRef(*out);
    return S_OK;
}

/*************************************************************************
 * SHSetThreadRef        [SHCORE.@]
 */
HRESULT WINAPI SHSetThreadRef(IUnknown *obj)
{
    TRACE("(%p)\n", obj);

    if (shcore_tls == TLS_OUT_OF_INDEXES)
        return E_NOINTERFACE;

    TlsSetValue(shcore_tls, obj);
    return S_OK;
}