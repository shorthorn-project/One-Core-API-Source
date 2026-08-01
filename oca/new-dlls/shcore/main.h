/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    main.h

Abstract:

    This module implements Win32 Shell IStream Interface Functions

Author:

    Skulltrail 13-June-2025

Revision History:

--*/

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "initguid.h"
#include "ocidl.h"
#include "shellscalingapi.h"
#include "shlwapi.h"
#include "featurestagingapi.h"
#include "shcore.h"

#include "wine/debug.h"
#include "wine/heap.h"

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <string.h>

#define COBJMACROS
#define NONAMELESSUNION

#include <ntsecapi.h>
#include "winerror.h"
#include "winnls.h"
#define NO_SHLWAPI_REG
#define NO_SHLWAPI_PATH
#include "wine/debug.h"
#include <stdio.h>
#include <math.h>

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

#define SPI_GETTHREADLOCALINPUTSETTINGS  0x104E
#define SPI_GETSYSTEMLANGUAGEBAR  0x1050
#define SPI_GETSCREENSAVESECURE  0x0076
#define SPI_GETDOCKMOVING  0x0090
#define SPI_GETSNAPSIZING  0x008E
#define SPI_GETWINARRANGING  0x0082
#define SPI_GETMESSAGEDURATION  0x2016
#define SPI_SETMOUSEWHEELROUTING  0x201D
#define SPI_SETMESSAGEDURATION  0x2017
#define SPI_SETSYSTEMLANGUAGEBAR  0x1051
#define SPI_SETTHREADLOCALINPUTSETTINGS  0x104F
#define SPI_SETDOCKMOVING  0x0091
#define SPI_SETSNAPSIZING  0x008F
#define SPI_SETWINARRANGING  0x0083
#define ZBID_DEFAULT 0
#define ZBID_DESKTOP 1

#define SPI_GETDISABLEOVERLAPPEDCONTENT 0x1040
#define SPI_SETDISABLEOVERLAPPEDCONTENT 0x1041
#define SPI_GETCLIENTAREAANIMATION 0x1042
#define SPI_SETCLIENTAREAANIMATION 0x1043

typedef struct tagNONCLIENTMETRICSA_VISTA {
	UINT cbSize;
	int iBorderWidth;
	int iScrollWidth;
	int iScrollHeight;
	int iCaptionWidth;
	int iCaptionHeight;
	LOGFONTA lfCaptionFont;
	int iSmCaptionWidth;
	int iSmCaptionHeight;
	LOGFONTA lfSmCaptionFont;
	int iMenuWidth;
	int iMenuHeight;
	LOGFONTA lfMenuFont;
	LOGFONTA lfStatusFont;
	LOGFONTA lfMessageFont;
    int iPaddedBorderWidth;
} NONCLIENTMETRICSA_VISTA, *PNONCLIENTMETRICSA_VISTA,*LPNONCLIENTMETRICSA_VISTA;

typedef struct tagNONCLIENTMETRICSW_VISTA {
	UINT cbSize;
	int iBorderWidth;
	int iScrollWidth;
	int iScrollHeight;
	int iCaptionWidth;
	int iCaptionHeight;
	LOGFONTW lfCaptionFont;
	int iSmCaptionWidth;
	int iSmCaptionHeight;
	LOGFONTW lfSmCaptionFont;
	int iMenuWidth;
	int iMenuHeight;
	LOGFONTW lfMenuFont;
	LOGFONTW lfStatusFont;
	LOGFONTW lfMessageFont;
    int iPaddedBorderWidth;
} NONCLIENTMETRICSW_VISTA, *PNONCLIENTMETRICSW_VISTA,*LPNONCLIENTMETRICSW_VISTA;

BOOL 
WINAPI
SystemParametersInfoWInternal(
	UINT uiAction,
	UINT uiParam,
	PVOID pvParam,
	UINT fWinIni);

ULARGE_INTEGER inline UlongToLargeInt(int i) {
    ULARGE_INTEGER li;
    li.QuadPart = i;
    return li;
}