/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    cursor.c

Abstract:

    Implement Clipboard related functions

Author:

    Skulltrail 16-August-2025

Revision History:

--*/

#include "main.h"


BOOL WINAPI GetUpdatedClipboardFormats(
  _Out_  PUINT lpuiFormats,
  _In_   UINT cFormats,
  _Out_  PUINT pcFormatsOut
)
{
	lpuiFormats = 0;
	pcFormatsOut = 0;
	DbgPrint("GetUpdatedClipboardFormats is UNIMPLEMENTED\n");  
	return TRUE;
}

BOOL WINAPI AddClipboardFormatListener(
  _In_  HWND hwnd
)
{
    // PVOID AddClipboardFormatListener;
	
	// AddClipboardFormatListener = TryGetProcedure("AddClipboardFormatListener");

    // if(AddClipboardFormatListener)
	// {
		// return AddClipboardFormatListener(hwnd);
	// }
	
	DbgPrint("AddClipboardFormatListener is UNIMPLEMENTED\n"); 
	return TRUE;
}
