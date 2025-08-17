/*++

Copyright (c) 2022 Shorthorn Project

Module Name:

    syspams.c

Abstract:

    Implement Timer functions

Author:

    Skulltrail 14-February-2022

Revision History:

--*/

#include <main.h>

static BOOL (WINAPI *pKillSystemTimer)(HWND, UINT_PTR);
static BOOL (WINAPI *pSetSystemTimer)(HWND, UINT_PTR, UINT, TIMERPROC);

BOOL
WINAPI
KillSystemTimer( HWND hWnd, UINT_PTR IDEvent)
{
	HMODULE huserbase = GetModuleHandleA("userbase.dll");
	pKillSystemTimer = (void *)GetProcAddress(huserbase, "KillSystemTimer");
	if(pKillSystemTimer){
		return pKillSystemTimer(hWnd, IDEvent);
	}else{
		return FALSE;
	}
}

UINT_PTR
WINAPI
SetSystemTimer(HWND hWnd, UINT_PTR IDEvent, UINT Period, TIMERPROC TimerFunc)
{
	HMODULE huserbase = GetModuleHandleA("userbase.dll");
	pSetSystemTimer = (void *)GetProcAddress(huserbase, "SetSystemTimer");
	if(pSetSystemTimer){
		return pSetSystemTimer(hWnd, IDEvent, Period, TimerFunc);
	}else{
		return 0;
	}
}