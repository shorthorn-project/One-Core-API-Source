/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    desktop.c

Abstract:

    Implement Desktop related functions

Author:

    Skulltrail 16-August-2025

Revision History:

--*/

#include "main.h"

BOOL WINAPI SwitchDesktopWithFade(
  _In_  HDESK hDesktop,
  _In_	BOOL active
)
{
	return SwitchDesktop(hDesktop);
}

HDESK WINAPI CreateDesktopExA(
  _In_        LPCTSTR lpszDesktop,
  _Reserved_  LPCTSTR lpszDevice,
  _Reserved_  DEVMODEA *pDevmode,
  _In_        DWORD dwFlags,
  _In_        ACCESS_MASK dwDesiredAccess,
  _In_opt_    LPSECURITY_ATTRIBUTES lpsa,
  _In_        ULONG ulHeapSize,
  _Reserved_  PVOID pvoid
)
{
	return CreateDesktopA(lpszDesktop, 
						  lpszDevice, 
						  pDevmode, 
						  dwFlags, 
						  dwDesiredAccess,
						  lpsa);
}

HDESK WINAPI CreateDesktopExW(
  _In_        LPCWSTR lpszDesktop,
  _Reserved_  LPCWSTR lpszDevice,
  _Reserved_  DEVMODEW *pDevmode,
  _In_        DWORD dwFlags,
  _In_        ACCESS_MASK dwDesiredAccess,
  _In_opt_    LPSECURITY_ATTRIBUTES lpsa,
  _In_        ULONG ulHeapSize,
  _Reserved_  PVOID pvoid
)
{
	return CreateDesktopW(lpszDesktop, 
						  lpszDevice, 
						  pDevmode, 
						  dwFlags, 
						  dwDesiredAccess,
						  lpsa);
}

HDESK WINAPI OpenThreadDesktop(DWORD desktop, BOOL verification, DWORD mask, ACCESS_MASK access)
{
  HDESK result; // eax@2
  PCSR_API_MESSAGE message = NULL; // [sp+4h] [bp-50h]@1
  NTSTATUS NtStatus = STATUS_SUCCESS; // [sp+24h] [bp-30h]@1

  CsrClientCallServer(message, 0, 197637, 8);
  if ( NtStatus < 0 )
  {
    SetLastError(NtStatus);
    result = 0;
  }
  else
  {
    result = GetThreadDesktop(desktop);
  }
  return result;
}

BOOL WINAPI CheckDesktopByThreadId(HANDLE desktop)
{
	DbgPrint("CheckDesktopByThreadId is UNIMPLEMENTED\n");  
	return TRUE;
}

BOOL WINAPI PaintMonitor(HMONITOR monnitor, HDC hdc, RECT rcDst)
{
	return PaintDesktop(hdc);	
}