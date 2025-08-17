/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    windows.c

Abstract:

    Implement Windows functions

Author:

    Skulltrail 16-August-2025

Revision History:

--*/

#include <main.h>

WINCOMPATTRDATA WindowAttribute;

static BOOL (WINAPI *pUpdateLayeredWindowIndirect)(HWND, const UPDATELAYEREDWINDOWINFO *);

BOOL 
WINAPI 
ChangeWindowMessageFilter(
  _In_  UINT message,
  _In_  DWORD dwFlag
)
{
	DbgPrint("ChangeWindowMessageFilter is UNIMPLEMENTED\n");
	return TRUE;
}

BOOL 
WINAPI 
UpdateWindowTransform(HWND hwnd, D3DMATRIX* pMatrix, DWORD unk) 
{
	DbgPrint("UpdateWindowTransform is UNIMPLEMENTED\n");
	return TRUE;
}

int 
WINAPI 
SetWindowRgnEx(HWND hWnd, HGDIOBJ hRgn, BOOL bRedraw)
{
  int obj = 0; // esi@1

  if ( obj )
    DeleteObject(hRgn);
  return obj;
}

int 
WINAPI 
GetWindowRgnEx(HWND hWnd, HGDIOBJ hRgn, BOOL bRedraw)
{
  int obj = 0; // esi@1
  return obj;
}

HWND 
WINAPI 
GhostWindowFromHungWindow (
    HWND hwndGhost
) 
{
	DbgPrint("GhostWindowFromHungWindow is UNIMPLEMENTED\n");
	return hwndGhost;
}

HWND WINAPI HungWindowFromGhostWindow(
    HWND hwndGhost
) 
{
	return hwndGhost;
}

BOOL WINAPI GetWindowMinimizeRect (
    HWND hwndToQuery,
    RECT* pRect
) 
{
	DbgPrint("GetWindowMinimizeRect is UNIMPLEMENTED\n");
	return TRUE;
}

HWND WINAPI WindowFromPhysicalPoint(
  _In_  POINT Point
)
{
	DbgPrint("WindowFromPhysicalPoint is UNIMPLEMENTED\n");
	return WindowFromPoint(Point);
}


BOOL WINAPI SetWindowCompositionAttribute(
    HWND hwnd,
    WINCOMPATTRDATA* pAttrData
) 
{
	WindowAttribute = *pAttrData;
	DbgPrint("SetWindowCompositionAttribute is UNIMPLEMENTED\n");
	return TRUE;
}

BOOL 
WINAPI 
GetWindowCompositionAttribute (
    HWND hwnd,
    WINCOMPATTRDATA* pAttrData
) 
{
	*pAttrData = WindowAttribute;
	DbgPrint("GetWindowCompositionAttribute is UNIMPLEMENTED\n");
	return TRUE;
}

HWND 
WINAPI 
FrostCrashedWindow(
    HWND hwndToReplace,
    HWND hwndErrorReportOwnerWnd
) 
{
	hwndToReplace = hwndErrorReportOwnerWnd;
	return hwndToReplace;
}

BOOL
WINAPI 
ChangeWindowMessageFilterEx(
  _In_         HWND hWnd,
  _In_         UINT message,
  _In_         DWORD action,
  _Inout_opt_  PCHANGEFILTERSTRUCT pChangeFilterStruct
)
{
	DbgPrint("ChangeWindowMessageFilterEx is UNIMPLEMENTED\n");
	return TRUE;
}

BOOL 
WINAPI 
CalculatePopupWindowPosition(
  _In_      const POINT *anchorPoint,
  _In_      const SIZE *windowSize,
  _In_      UINT flags,
  _In_opt_  RECT *excludeRect,
  _Out_     RECT *popupWindowPosition
)
{
	DbgPrint("CalculatePopupWindowPosition is UNIMPLEMENTED\n");
	return TRUE;
}

BOOL WINAPI GetTopLevelWindow(HWND window)
{
	DbgPrint("GetTopLevelWindow is UNIMPLEMENTED\n");
	return TRUE;
}

DWORD WINAPI NotifyOverlayWindow(HWND a1, DWORD a2)
{
	DbgPrint("NotifyOverlayWindow is UNIMPLEMENTED. Need implement NtUserCallTwoParam\n");	
	return 0;
}

DWORD 
WINAPI 
RegisterGhostWindow(HWND a1, DWORD a2)
{
	DbgPrint("RegisterGhostWindow is UNIMPLEMENTED. Need implement NtUserCallTwoParam\n");	
	return 0;
}

DWORD 
WINAPI 
RegisterFrostWindow(HWND a1, DWORD a2)
{
	DbgPrint("GetWindowCompositionInfo is UNIMPLEMENTED. Need implement NtUserCallTwoParam\n");	
	return 0;
}


DWORD WINAPI GetWindowCompositionInfo(HWND a1, DWORD a2)
{
	DbgPrint("GetWindowCompositionInfo is UNIMPLEMENTED\n");
	return 0;
}

DWORD WINAPI CheckWindowThreadDesktop(HANDLE a1, HANDLE a2)
{
	DbgPrint("CheckWindowThreadDesktop is UNIMPLEMENTED\n");
	return 0;
}

BOOL 
WINAPI 
SetWindowFeedbackSetting(
  _In_      HWND hwnd,
  _In_      FEEDBACK_TYPE feedback,
  _In_      DWORD dwFlags,
  _In_      UINT32 size,
  _In_opt_  const VOID *configuration
)
{
	DbgPrint("SetWindowFeedbackSetting is UNIMPLEMENTED\n");
	return TRUE;
}

BOOL 
WINAPI 
IsTopLevelWindow (
  _In_  HWND hWnd
)
{
	DbgPrint("IsTopLevelWindow is UNIMPLEMENTED\n");
	return FALSE;
}

BOOL 
WINAPI 
SetWindowRelative(HWND windowOne, HWND windowTwo)
{
	DbgPrint("SetWindowRelative is UNIMPLEMENTED\n");	
	return TRUE;
}

HWND 
WINAPI 
CreateWindowInBand(
	DWORD dwExStyle, 
	LPCWSTR lpClassName, 
	LPCWSTR lpWindowName, 
	DWORD dwStyle, 
	int x, 
	int y, 
	int nWidth, 
	int nHeight, 
	HWND hWndParent, 
	HMENU hMenu, 
	HINSTANCE hInstance, 
	LPVOID lpParam, 
	DWORD dwBand
)
{
	return CreateWindowExW(dwExStyle,
						  lpClassName, 
						  lpWindowName,
						  dwStyle,
						  x,
						  y,
						  nWidth,
						  nHeight, 
						  hWndParent,
						  hMenu,
						  hInstance,
						  lpParam);
}

// A undocumented Win32 API, only really used by Microsoft native applications.
BOOL WINAPI GetWindowBand(
    IN    HWND    Window,
    OUT    PDWORD    Band)
{
    if (!IsWindow(Window)) {
        SetLastError(ERROR_INVALID_WINDOW_HANDLE);
        return FALSE;
    }

    if (!Band) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (Window == GetDesktopWindow()) {
        *Band = ZBID_DESKTOP;
    } else {
        *Band = ZBID_DEFAULT;
    }

    return TRUE;
}

BOOL 
WINAPI
UpdateLayeredWindowIndirect(
  _In_       HWND                    hwnd,
  _In_ const UPDATELAYEREDWINDOWINFO *pULWInfo
)
{
	HMODULE huser32 = GetModuleHandleA("userbase.dll");
	pUpdateLayeredWindowIndirect = (void *)GetProcAddress(huser32, "UpdateLayeredWindowIndirect");
	if(pUpdateLayeredWindowIndirect){
		return pUpdateLayeredWindowIndirect(hwnd, pULWInfo);
	}else{
		if (pULWInfo && pULWInfo->cbSize == sizeof(*pULWInfo))
		{
			return UpdateLayeredWindow(hwnd,
									   pULWInfo->hdcDst,
									   (POINT *)pULWInfo->pptDst,
									   (SIZE *)pULWInfo->psize,
									   pULWInfo->hdcSrc,
									   (POINT *)pULWInfo->pptSrc,
									   pULWInfo->crKey,
									   (BLENDFUNCTION *)pULWInfo->pblend,
									   pULWInfo->dwFlags);
	    }
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;		
	}    	
}

/**********************************************************************
  * IsWindowRedirectedForPrint [USER32.@]
  */
BOOL WINAPI IsWindowRedirectedForPrint( HWND hwnd )
{
     DbgPrint("(%p): stub\n", hwnd);
     return FALSE;
}

// Encountered during testing: Paint.Net 3.5.10 crashes after just a second under Win 2003 RTM alpha-testing
// because of a bug in SetWindowLongPtr that returns the high-order bit that causes an OverflowException
// when attempting to convert to an int.
LONG WINAPI SetWindowLongW_Internal(HWND hWnd,int nIndex,LONG dwNewLong) {
    LONG Result = SetWindowLongW(hWnd, nIndex, dwNewLong);
    if (nIndex == GWL_EXSTYLE && (Result & 0x80000000) != 0) {
        Result ^= 0x80000000;
    }
    return Result;
}
// Please only define this function for X64
LONG_PTR WINAPI SetWindowLongPtrW_Internal(HWND hWnd,int nIndex,LONG_PTR dwNewLong) {
    LONG_PTR Result = SetWindowLongPtrW(hWnd, nIndex, dwNewLong);
    if (nIndex == GWL_EXSTYLE && (Result & 0x8000000000000000) != 0) {
        Result ^= 0x8000000000000000;
    }
    return Result;
}

// Even though this API claims it's from "1903+" it exists on 10.0.10240.
// Fixes Chromium 132 "Crash On Move Window"
BOOL WINAPI IsWindowArranged(HWND hwnd) {
    return FALSE; // No such thing as "Aero Snap" on XP.
}