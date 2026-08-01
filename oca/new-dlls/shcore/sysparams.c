/*++

Copyright (c) 2026 Shorthorn Project

Module Name:

    sysparams.c

Abstract:

    User System Params functions

Author:

    Skulltrail 17-July-2026

Revision History:

--*/

#include <main.h>

BOOL 
WINAPI
SystemParametersInfoWInternal(
	UINT uiAction,
	UINT uiParam,
	PVOID pvParam,
	UINT fWinIni)
{
	BOOL res;
	
	// HACK: Qt6.6.1 after WinRT classes defined crashes due to NONCLIENTMETRICS being on NT6 size, so convert to NT5
	if ((uiAction == SPI_SETNONCLIENTMETRICS || uiAction == SPI_GETNONCLIENTMETRICS) && pvParam && ((LPNONCLIENTMETRICSW)pvParam)->cbSize == sizeof(NONCLIENTMETRICSW) + 4) {
		// Set size
		((LPNONCLIENTMETRICSW)pvParam)->cbSize -= 4;
		res = SystemParametersInfoW(uiAction, sizeof(NONCLIENTMETRICSW), pvParam, fWinIni);
		((LPNONCLIENTMETRICSW)pvParam)->cbSize += 4;
		if (res) {
			 ((LPNONCLIENTMETRICSW_VISTA)pvParam)->iPaddedBorderWidth = 0;
		}
		return res;
	}	
	switch(uiAction)
    {
	  case SPI_GETCLIENTAREAANIMATION:
		/*
			In Visual Studio 2012, the WPF designer crashes without having this. Client area animations simply do not exist
			before Windows Vista, so return TRUE.
			
			TODO: If someone installs the Longhorn 5048 Win32ss and enables DWM, then client area animations actually become
			meaningful. This is why we return TRUE
		*/
		if (!pvParam) {
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}
		*(PBOOL)pvParam = TRUE;
		return TRUE;
	  case SPI_GETDISABLEOVERLAPPEDCONTENT:
	  case SPI_GETSYSTEMLANGUAGEBAR:
	  case SPI_GETTHREADLOCALINPUTSETTINGS:
	  case SPI_GETSCREENSAVESECURE: // This exists internally in Server 2003 SP2...
	  case SPI_GETDOCKMOVING:
	  case SPI_GETSNAPSIZING:
	  case SPI_GETWINARRANGING:
		if (!pvParam) {
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}
		*(PBOOL)pvParam = FALSE;
		return TRUE;
	  case SPI_GETMESSAGEDURATION:
		if (!pvParam) {
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}
		*(PULONG)pvParam = 5;
		return TRUE;
	  case SPI_SETMOUSEWHEELROUTING: // Support Windows 10 mouse wheel options.
		SystemParametersInfoW(uiAction, uiParam, pvParam, fWinIni);
		return TRUE;
	  case SPI_SETCLIENTAREAANIMATION:
	  case SPI_SETMESSAGEDURATION:
	  case SPI_SETDISABLEOVERLAPPEDCONTENT:
	  case SPI_SETSYSTEMLANGUAGEBAR:
	  case SPI_SETTHREADLOCALINPUTSETTINGS:
	  case SPI_SETDOCKMOVING:
	  case SPI_SETSNAPSIZING:
	  case SPI_SETWINARRANGING:
		return TRUE;
	  default:
		return SystemParametersInfoW(uiAction, uiParam, pvParam, fWinIni);
	}
}