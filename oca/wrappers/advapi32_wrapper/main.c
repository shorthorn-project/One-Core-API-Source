/*++

Copyright (c) 2026  Shorthorn Project

Module Name:

    unimplemented.c

Abstract:

    DllMain and Related API Interfaces

Author:

    Skulltrail 27-January-2026

Revision History:

--*/

#include "main.h"

extern BOOL RegInitialize(VOID);

WINE_DEFAULT_DEBUG_CHANNEL(main); 	

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv)
{
    switch(fdwReason)
    {
        case DLL_PROCESS_ATTACH:
           RegInitialize();		
           InitNativeProcs();		
           break;
    }

    return TRUE;
}

/*
DWORD WINAPI NotifyServiceStatusChangeW(
  _In_  SC_HANDLE hService,
  _In_  DWORD dwNotifyMask,
  _In_  PSERVICE_NOTIFY pNotifyBuffer
)
{
	return ERROR_SUCCESS;
}
*/

NTSTATUS 
WINAPI
LsaLookupSids2(
  _In_  LSA_HANDLE                  PolicyHandle,
  _In_  ULONG                       LookupOptions,
  _In_  ULONG                       Count,
  _In_  PSID                        *Sids,
  _Out_ PLSA_REFERENCED_DOMAIN_LIST *ReferencedDomains,
  _Out_ PLSA_TRANSLATED_NAME        *Names
)
{
	return LsaLookupSids(PolicyHandle,
						 Count,
						 Sids,
						 ReferencedDomains,
						 Names);
}