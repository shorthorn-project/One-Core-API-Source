/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    main.c

Abstract:

    Implement Misc functions

Author:

    Skulltrail 16-August-2025

Revision History:

--*/

#include <main.h>

WINE_DEFAULT_DEBUG_CHANNEL(misc);

HANDLE section = NULL;
static BOOL (WINAPI *pIsSETEnabled)();

UINT_PTR 
WINAPI 
SetCoalescableTimer(
  _In_opt_  HWND hwnd,
  _In_      UINT_PTR nIDEvent,
  _In_      UINT uElapse,
  _In_opt_  TIMERPROC lpTimerFunc,
  _In_      ULONG uToleranceDelay
)
{
	return SetTimer(hwnd, nIDEvent, uElapse, lpTimerFunc);
}

BOOL WINAPI IsSETEnabled()
{
	HMODULE huserbase = GetModuleHandleA("userbase.dll");
	pIsSETEnabled = (void *)GetProcAddress(huserbase, "IsSETEnabled");
	if(pIsSETEnabled){
		return pIsSETEnabled();
	}else{
		return FALSE;
	}
}

BOOL WINAPI HangrepRegisterPort(HANDLE Port)
{
	return TRUE;	
}

BOOL WINAPI HangrepUnregisterPort(HANDLE Port)
{
	return TRUE;	
}

HANDLE WINAPI ThemeGetCurrentSection(HANDLE Port)
{
	return section;	
}

BOOL WINAPI ThemeSetCurrentSection(HANDLE Section)
{
	section = Section;
	return TRUE;
}

BOOL WINAPI GetPointerDevice(HANDLE device, POINTER_DEVICE_INFO *dev) {
	return FALSE;
}

BOOL WINAPI SkipPointerFrameMessages(UINT32 ID) {
	return TRUE;
}

BOOL WINAPI GetInputLocaleInfo(LCID locale, LPCWSTR localName)
{
	localName = L"en-US";
	DbgPrint("GetInputLocaleInfo is UNIMPLEMENTED\n"); 
	return TRUE;
}