/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    cursor.c

Abstract:

    Implement UI process related functions

Author:

    Skulltrail 16-August-2025

Revision History:

--*/

#include <main.h>

WINE_DEFAULT_DEBUG_CHANNEL(process); 

BOOL WINAPI SetProcessRestrictionExemption(BOOL verification)
{
     return TRUE;
}

BOOL WINAPI GetProcessUIContextInformation(
	IN	HANDLE							ProcessHandle,
	OUT	PPROCESS_UICONTEXT_INFORMATION	UIContextInformation)
{
	if (!UIContextInformation) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	UIContextInformation->UIContext = PROCESS_UICONTEXT_DESKTOP;
	UIContextInformation->dwFlags		= PROCESS_UIF_NONE;

	return TRUE;
}