/*++

Copyright (c) 2022 Shorthorn Project

Module Name:

    dpi.c

Abstract:

    Implement DPI Scallig functions

Author:

    Skulltrail 14-June-2024

Revision History:

--*/

#include <main.h>

WINE_DEFAULT_DEBUG_CHANNEL(user32);

DPI_AWARENESS_CONTEXT Globalcontext = DPI_AWARENESS_CONTEXT_UNAWARE;
static DPI_AWARENESS dpi_awareness = 0;

int WINAPI GetSystemMetricsForDpi(int nIndex, UINT dpi) {
    int index;
	if (dpi == 96)
        return GetSystemMetrics(nIndex);
    index = GetSystemMetrics(nIndex);
    switch(nIndex)
    {
        case SM_CXBORDER:
        case SM_CXMAXTRACK:
        case SM_CXMIN:
        case SM_CXMINTRACK:
        case SM_CYMAXTRACK:
        case SM_CYMIN:
        case SM_CYMINTRACK:
        case SM_CXICON:
        case SM_CXICONSPACING:
        case SM_CXSMICON:
        case SM_CYICON:
        case SM_CYICONSPACING:
        case SM_CYSMICON:
            index = (index * dpi) / 96;
        default:
            break;
    }
    return index;
} 

BOOL WINAPI SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi) {
    if (SystemParametersInfoWInternal(uiAction, uiParam, pvParam, fWinIni)) {
        if(uiAction == SPI_GETICONTITLELOGFONT)
        {
            LOGFONTW* logfont = pvParam;
            logfont->lfHeight *= dpi/96;
            logfont->lfWidth *= dpi/96;
            return TRUE;
        }
        
        if(uiAction == SPI_GETICONMETRICS)
        {
            ICONMETRICSW* iconmetrics = pvParam;
            
            iconmetrics->iHorzSpacing *= dpi/96;
            iconmetrics->iVertSpacing *= dpi/96;
            return TRUE;
        }
        
        if(uiAction == SPI_GETNONCLIENTMETRICS)
        {
            NONCLIENTMETRICSW* nonclientmetrics = pvParam;
            
            nonclientmetrics->iBorderWidth *= dpi/96;
            nonclientmetrics->iScrollWidth *= dpi/96;
            nonclientmetrics->iScrollHeight *= dpi/96;
            nonclientmetrics->iCaptionWidth *= dpi/96;
            nonclientmetrics->iCaptionHeight *= dpi/96;
            
            nonclientmetrics->iSmCaptionWidth *= dpi/96;
            nonclientmetrics->iSmCaptionHeight *= dpi/96;
            nonclientmetrics->iMenuWidth *= dpi/96;
            nonclientmetrics->iMenuHeight *= dpi/96;
#if (WINVER >= 0x0600)			
            nonclientmetrics->iPaddedBorderWidth *= dpi/96;
#endif
            return TRUE;
        }    
    }
    return FALSE;
}

DPI_AWARENESS WINAPI GetAwarenessFromDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
	switch ((size_t)value) {
		case DPI_AWARENESS_CONTEXT_UNAWARE:
		case DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED:
			return DPI_AWARENESS_UNAWARE;
		case DPI_AWARENESS_CONTEXT_SYSTEM_AWARE:
			return DPI_AWARENESS_SYSTEM_AWARE;
		case DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE:
		case DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2:
			return DPI_AWARENESS_PER_MONITOR_AWARE;
		default:
			return DPI_AWARENESS_INVALID;
	}
}

BOOL WINAPI AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT dpiContextA, DPI_AWARENESS_CONTEXT dpiContextB) {
	return GetAwarenessFromDpiAwarenessContext(dpiContextA) == GetAwarenessFromDpiAwarenessContext(dpiContextB);
}
BOOL WINAPI EnableNonClientDpiScaling(HWND hwnd) {
	return TRUE;
}

BOOL WINAPI AdjustWindowRectExForDpi(RECT *lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi) {
   if(!AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle))
       return FALSE;
   else
   {
	   lpRect->left *= dpi/96;
	   lpRect->top *= dpi/96;
	   lpRect->right *= dpi/96;
	   lpRect->bottom *= dpi/96;
	   
	   return TRUE;
   }
}
BOOL WINAPI IsValidDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) {
	return GetAwarenessFromDpiAwarenessContext(value) != DPI_AWARENESS_INVALID;
}

HRESULT WINAPI GetProcessDpiAwareness(HANDLE hProcess, PROCESS_DPI_AWARENESS *value) {
	if (!value)
		return ERROR_INVALID_PARAMETER;
	if (IsProcessDPIAware())
		*value = DPI_AWARENESS_SYSTEM_AWARE;
	else
		*value = DPI_AWARENESS_UNAWARE;
	return 0;
}


WINUSERAPI UINT WINAPI GetDpiForSystem(
    VOID)
{
    HDC hdcScreen = GetDC(NULL);
    UINT uDpi = GetDeviceCaps(hdcScreen, LOGPIXELSX);
    ReleaseDC(NULL, hdcScreen);
    return uDpi;
}

WINUSERAPI UINT WINAPI GetDpiForWindow(
    IN    HWND    hWnd)
{
    HDC hdcWindow = GetDC(hWnd);
    UINT uDpi = GetDeviceCaps(hdcWindow, LOGPIXELSX);
    ReleaseDC(hWnd, hdcWindow);
    return uDpi;
}

/**********************************************************************
 *              GetThreadDpiAwarenessContext   (USER32.@)
 */
DPI_AWARENESS_CONTEXT WINAPI GetThreadDpiAwarenessContext(void)
{
    return Globalcontext;
}

/**********************************************************************
 *              SetThreadDpiAwarenessContext   (USER32.@)
 */
DPI_AWARENESS_CONTEXT WINAPI SetThreadDpiAwarenessContext( DPI_AWARENESS_CONTEXT context )
{
	DPI_AWARENESS_CONTEXT CurrentDpiAwareContext = Globalcontext;
	Globalcontext = context;
	return CurrentDpiAwareContext;
}

// /***********************************************************************
 // *              GetAwarenessFromDpiAwarenessContext   (USER32.@)
 // */
// DPI_AWARENESS WINAPI GetAwarenessFromDpiAwarenessContext( DPI_AWARENESS_CONTEXT context )
// {
    // switch ((ULONG_PTR)context)
    // {
    // case 0x10:
    // case 0x11:
    // case 0x12:
    // case 0x80000010:
    // case 0x80000011:
    // case 0x80000012:
        // return (ULONG_PTR)context & 3;
    // case (ULONG_PTR)DPI_AWARENESS_CONTEXT_UNAWARE:
    // case (ULONG_PTR)DPI_AWARENESS_CONTEXT_SYSTEM_AWARE:
    // case (ULONG_PTR)DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE:
        // return ~(ULONG_PTR)context;
    // default:
        // return DPI_AWARENESS_INVALID;
    // }
// }

/***********************************************************************
 *              SetProcessDPIAware   (USER32.@)
 */
BOOL WINAPI SetProcessDPIAware(void)
{
    InterlockedCompareExchange( (volatile long *)&dpi_awareness, 0x11, 0 );
    return TRUE;
}

BOOL WINAPI IsProcessDPIAware()
{
	return FALSE;
}

// /**********************************************************************
 // *              EnableNonClientDpiScaling   (USER32.@)
 // */
// BOOL WINAPI EnableNonClientDpiScaling( HWND hwnd )
// {
    // FIXME("(%p): stub\n", hwnd);
    // SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    // return FALSE;
// }

HRESULT WINAPI SetProcessDpiAwareness(PROCESS_DPI_AWARENESS value)
{
	// if (auto const pSetProcessDpiAwareness = try_get_SetProcessDpiAwareness())
	// {
		// return pSetProcessDpiAwareness(value);
	// }

	if (value != PROCESS_DPI_UNAWARE)
	{
		return SetProcessDPIAware() ? S_OK : E_FAIL;
	}

	return S_OK;
}

//Move to sysparams.c
BOOL WINAPI SetProcessDpiAwarenessInternal( DPI_AWARENESS awareness )
{
	return SetProcessDpiAwareness(awareness);
}

/**********************************************************************
 *              GetProcessDpiAwarenessInternal   (USER32.@)
 */
// based on Win7Wrapper
BOOL 
WINAPI 
GetProcessDpiAwarenessInternal(
	HANDLE hProcess, 
	PROCESS_DPI_AWARENESS *value) 
{
   if (!value) {
       SetLastError(ERROR_INVALID_PARAMETER);
       return FALSE;
   }
   
   // always set UNAWARE if you do not want this
   if (IsProcessDPIAware())
        *value = DPI_AWARENESS_SYSTEM_AWARE;
   else
        *value = DPI_AWARENESS_UNAWARE;
   return TRUE;
}

BOOL
WINAPI
SetProcessDpiAwarenessContext(
    _In_ DPI_AWARENESS_CONTEXT value
	)
{
			// if (auto const pSetProcessDpiAwarenessContext = try_get_SetProcessDpiAwarenessContext())
			// {
				// return pSetProcessDpiAwarenessContext(value);
			// }

   LSTATUS lStatus;
   HRESULT hr;

   do
   {
	   PROCESS_DPI_AWARENESS DpiAwareness;

	   if (DPI_AWARENESS_CONTEXT_UNAWARE == value
	      || DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED == value)
       {
			DpiAwareness = PROCESS_DPI_UNAWARE;
				}
				else if (DPI_AWARENESS_CONTEXT_SYSTEM_AWARE == value)
				{
					DpiAwareness = PROCESS_SYSTEM_DPI_AWARE;
				}
				else if (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE == value
					|| DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == value)
				{
					DpiAwareness = PROCESS_PER_MONITOR_DPI_AWARE;
				}
				else
				{
					lStatus = ERROR_INVALID_PARAMETER;
					break;
				}

				hr = SetProcessDpiAwareness(DpiAwareness);

				if (SUCCEEDED(hr))
				{
					return TRUE;
				}

				//将 HRESULT 错误代码转换到 LSTATUS
				if (hr & 0xFFFF0000)
				{
					if (HRESULT_FACILITY(hr) == FACILITY_WIN32)
					{
						lStatus = HRESULT_CODE(hr);
					}
					else
					{
						lStatus = ERROR_FUNCTION_FAILED;
					}
				}
				else
				{
					lStatus = hr;
				}

	} while (FALSE);
			
	SetLastError(lStatus);
	return FALSE;
}
		
DPI_AWARENESS_CONTEXT 
WINAPI 
GetWindowDpiAwarenessContext(HWND hWnd) {
    // Basically check if the DPI for system is regular.
    // But the DPI for window is not.
    if (GetDpiForSystem() == 96 && GetDpiForWindow(hWnd) != 96)
        return DPI_AWARENESS_CONTEXT_SYSTEM_AWARE;
    return DPI_AWARENESS_CONTEXT_UNAWARE;
}

/***********************************************************************
 *           NtUserGetDpiForMonitor   (win32u.@)
 */
BOOL WINAPI NtUserGetDpiForMonitor( HMONITOR monitor, UINT type, UINT *x, UINT *y )
{
	//Return normal and default size on Windows XP
	*x = 96;
	*y = 96;
	return TRUE;
}