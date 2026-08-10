/*++

Copyright (c) 2023 Shorthorn Project

Module Name:

    main.c

Abstract:

    This module implements COM Main functions APIs

Author:

    Skulltrail 12-October-2023

Revision History:

--*/

#define WIN32_NO_STATUS

#include "main.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole32);

typedef interface IUnknown IActivationFilter;

IActivationFilter globalActivationFilter = {0};

typedef HRESULT (WINAPI *PFNCoDisconnectContext)(DWORD);
typedef HRESULT (WINAPI *PFNCoGetActivationState)(GUID, DWORD, DWORD *);
typedef HRESULT (WINAPI *PFNCoGetCallState)(int, PULONG);
typedef HRESULT (WINAPI *PFNCoRegisterActivationFilter)(IActivationFilter*);

HRESULT WINAPI CoDisconnectContext(DWORD dwTimeout)
{
    static HMODULE hOleBase = NULL;
    PFNCoDisconnectContext pCoDisconnectContext;

    DbgPrint("CoDisconnectContext called\n");

    if (!hOleBase)
    {
        hOleBase = GetModuleHandleW(L"olebase.dll");
        if (!hOleBase)
            hOleBase = LoadLibraryW(L"olebase.dll");
    }

    if (hOleBase)
    {
        pCoDisconnectContext = (PFNCoDisconnectContext)
            GetProcAddress(hOleBase, "CoDisconnectContext");

        if (pCoDisconnectContext)
            return pCoDisconnectContext(dwTimeout);
    }

    return S_OK;
}

/***********************************************************************
 *      CoGetActivationState (ole32.@)
 */
HRESULT WINAPI CoGetActivationState(GUID guid, DWORD unknown, DWORD *unknown2)
{
    static HMODULE hOleBase = NULL;
    PFNCoGetActivationState pCoGetActivationState;

    DbgPrint("CoGetActivationState called\n");

    if (!hOleBase)
    {
        hOleBase = GetModuleHandleW(L"olebase.dll");
        if (!hOleBase)
            hOleBase = LoadLibraryW(L"olebase.dll");
    }

    if (hOleBase)
    {
        pCoGetActivationState = (PFNCoGetActivationState)
            GetProcAddress(hOleBase, "CoGetActivationState");

        if (pCoGetActivationState)
            return pCoGetActivationState(guid, unknown, unknown2);
    }

    return E_NOTIMPL;
}

/***********************************************************************
 *      CoGetCallState (ole32.@)
 */
HRESULT WINAPI CoGetCallState(int unknown, PULONG unknown2)
{
    static HMODULE hOleBase = NULL;
    PFNCoGetCallState pCoGetCallState;

    DbgPrint("CoGetCallState called\n");

    if (!hOleBase)
    {
        hOleBase = GetModuleHandleW(L"olebase.dll");
        if (!hOleBase)
            hOleBase = LoadLibraryW(L"olebase.dll");
    }

    if (hOleBase)
    {
        pCoGetCallState = (PFNCoGetCallState)
            GetProcAddress(hOleBase, "CoGetCallState");

        if (pCoGetCallState)
            return pCoGetCallState(unknown, unknown2);
    }

    return E_NOTIMPL;
}

HRESULT WINAPI CoRegisterActivationFilter(IActivationFilter *pActivationFilter)
{
    IActivationFilter *activationFilter; // rax
    static HMODULE hOleBase = NULL;
    PFNCoRegisterActivationFilter pCoRegisterActivationFilter;

    DbgPrint("CoRegisterActivationFilter called\n");

    if (!hOleBase)
    {
        hOleBase = GetModuleHandleW(L"olebase.dll");
        if (!hOleBase)
            hOleBase = LoadLibraryW(L"olebase.dll");
    }

    if (hOleBase)
    {
        pCoRegisterActivationFilter = (PFNCoRegisterActivationFilter)
            GetProcAddress(hOleBase, "CoRegisterActivationFilter");

        if (pCoRegisterActivationFilter)
            return pCoRegisterActivationFilter(pActivationFilter);
    } 

  if ( !pActivationFilter )
    return 0x80070057;
  activationFilter = (IActivationFilter *)_InterlockedCompareExchange64(
                                            (signed __int64*)&globalActivationFilter,
                                            (signed __int64)pActivationFilter,
                                            0i64);
  if ( !activationFilter || activationFilter == pActivationFilter )
    return 0;
  else
    return 0x80004021;
}