/*++

Copyright (c) 2022 Shorthorn Project

Module Name:

    dwm.c

Abstract:

    Implement DWM (Desktop Window Manager) functions

Author:

    Skulltrail 14-February-2022

Revision History:

--*/

#include <main.h>

HANDLE gpepDwm;
LPCRITICAL_SECTION critical;
HANDLE gDwmApiPort;
int IsComposited = 0;

DWORD WINAPI DwmGetRedirSurfaceUpdateId(HANDLE h1, BOOL ver, DWORD flags)
{
	DbgPrint("DwmGetRedirSurfaceUpdateId is UNIMPLEMENTED\n");	
	return 0;
}

BOOL WINAPI DwmGetRedirSurfacePresentFlags(HANDLE a1, HANDLE a2)
{
	DbgPrint("DwmGetRedirSurfacePresentFlags is UNIMPLEMENTED\n");	
	a2 = a1;
	return TRUE;
}

BOOL WINAPI DwmSetRedirSurfacePresentFlags(HANDLE a1, BOOL a2)
{
	DbgPrint("DwmSetRedirSurfacePresentFlags is UNIMPLEMENTED\n");	
	return TRUE;
}

int WINAPI DCEQueryMode()
{
	return 0;
}

DWORD WINAPI DwmStartRedirection(HWND a1)
{
	DbgPrint("DwmStartRedirection is UNIMPLEMENTED\n");	
	return 0;
}

DWORD WINAPI DwmStopRedirection()
{
	DbgPrint("DwmStopRedirection is UNIMPLEMENTED\n");	
	return 0;
}

NTSTATUS WINAPI DwmHintDxUpdate(HANDLE window, DWORD option){
	return STATUS_SUCCESS;
}

BOOL 
WINAPI 
IsThreadDesktopComposited ( ) 
{
	DbgPrint("IsThreadDesktopComposited is UNIMPLEMENTED\n");  
	return FALSE;
}