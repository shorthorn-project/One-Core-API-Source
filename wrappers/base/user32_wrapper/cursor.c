/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    cursor.c

Abstract:

    Implement Cursor related functions

Author:

    Skulltrail 16-August-2025

Revision History:

--*/

#include "main.h"

BOOL WINAPI ShowSystemCursor(BOOL fShow)
{
	ShowCursor(fShow);
	return TRUE;
}

BOOL WINAPI SetPhysicalCursorPos(int X, int Y)
{
	return SetCursorPos(X, Y);
}

BOOL WINAPI GetPhysicalCursorPos(
  _Out_  LPPOINT lpPoint
)
{
	return GetCursorPos(lpPoint);
}