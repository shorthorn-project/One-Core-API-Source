/*
 * Copyright 2009 Henri Verbeet for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define _USER32_
#define OEMRESOURCE
#define NTOS_MODE_USER 
#define WIN32_NO_STATUS
#define _WMI_SOURCE_
#include <windows.h>
#include <accctrl.h>
#include <aclapi.h>
#include <wmistr.h>
#include <evntrace.h>
#include <winsafer.h>
#include <sddl.h>
#define NTOS_MODE_USER
#include <ntsecapi.h>
#include <ndk/cmfuncs.h>
#include <ndk/exfuncs.h>
#include <ndk/iofuncs.h>
#include <ndk/kefuncs.h>
#include <ndk/obfuncs.h>
#include <ndk/psfuncs.h>
#include <ndk/rtlfuncs.h>
#include <ndk/setypes.h>
#include <ndk/sefuncs.h>
#include <evntprov.h>
#include <csr.h>
#include <strsafe.h>
#include <wine/debug.h>
#include <wine/list.h>
#include <csrsrv.h>
#include <api.h>
#include <ntuser.h>
#include <winuser.h>

#define WH_MIN   (-1)
#define WH_MAX   14
#define WH_MINHOOK   WH_MIN
#define WH_MAXHOOK   WH_MAX

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

typedef LPBITMAPINFOHEADER(*PCONVERT_TO_DIB_PROC)(LPBITMAPINFOHEADER lpPngData, DWORD dwSize);

static PCONVERT_TO_DIB_PROC gpICSProc = NULL;

static BOOL gfdDPIProcess = FALSE;

static PVOID W32UserProbeAddress = 0;

static HDC ghdcBits2 = 0;

BITMAPINFOHEADER globalInfoHeader;

static int gfSystemInitialized = 0;

static const WCHAR string = 0;

static int InitDst[32];

static int gpidLogon = 0;

static PVOID grpdeskLogon = 0;

LIST_ENTRY gpqForeground;

BITMAPINFO *ScaleProc;

BITMAPINFO *gpsi;

BITMAPINFOHEADER pbmih;

RTL_CRITICAL_SECTION CriticalSectionObject;

PRTL_CRITICAL_SECTION gcsHdc;

LPCRITICAL_SECTION lpCriticalSection;

HANDLE pUserHeap;

BOOL IsNativePNGConversor;

typedef struct tagMAGCOLOREFFECT;

struct tagMAGCOLOREFFECT *pEffectGlobal;

typedef HANDLE HPOWERNOTIFY;

typedef HANDLE HTOUCHINPUT;

typedef HANDLE HGESTUREINFO; 

DECLARE_HANDLE(HSYNTHETICPOINTERDEVICE);

typedef struct _PROCESSINFO *PPROCESSINFO;

typedef struct _D3DMATRIX {
    union {
        struct {
            float        _11, _12, _13, _14;
            float        _21, _22, _23, _24;
            float        _31, _32, _33, _34;
            float        _41, _42, _43, _44;

        };
        float m[4][4];
    };
} D3DMATRIX;

typedef enum _ORIENTATION_PREFERENCE { 
  ORIENTATION_PREFERENCE_NONE               = 0x0,
  ORIENTATION_PREFERENCE_LANDSCAPE          = 0x1,
  
    ORIENTATION_PREFERENCE_PORTRAIT      = 0x2,
  ORIENTATION_PREFERENCE_LANDSCAPE_FLIPPED  = 0x4,
  ORIENTATION_PREFERENCE_PORTRAIT_FLIPPED   = 0x8
} ORIENTATION_PREFERENCE;

ORIENTATION_PREFERENCE Orientation;

typedef enum tagFEEDBACK_TYPE { 
  FEEDBACK_TOUCH_CONTACTVISUALIZATION  = 1,
  FEEDBACK_PEN_BARRELVISUALIZATION     = 2,
  FEEDBACK_PEN_TAP                     = 3,
  FEEDBACK_PEN_DOUBLETAP               = 4,
  FEEDBACK_PEN_PRESSANDHOLD            = 5,
  FEEDBACK_PEN_RIGHTTAP                = 6,
  FEEDBACK_TOUCH_TAP                   = 7,
  FEEDBACK_TOUCH_DOUBLETAP             = 8,
  FEEDBACK_TOUCH_PRESSANDHOLD          = 9,
  FEEDBACK_TOUCH_RIGHTTAP              = 10,
  FEEDBACK_GESTURE_PRESSANDTAP         = 11,
  FEEDBACK_MAX                         = 0xFFFFFFFF
} FEEDBACK_TYPE ;

typedef enum tagPOINTER_INPUT_TYPE { 
  PT_POINTER  = 0x00000001,
  PT_TOUCH    = 0x00000002,
  PT_PEN      = 0x00000003,
  PT_MOUSE    = 0x00000004
} POINTER_INPUT_TYPE;

typedef struct tagMAGCOLOREFFECT {
  float transform[5][5];
} MAGCOLOREFFECT, *PMAGCOLOREFFECT;

typedef struct _TOUCHINPUT {
  LONG      x;
  LONG      y;
  HANDLE    hSource;
  DWORD     dwID;
  DWORD     dwFlags;
  DWORD     dwMask;
  DWORD     dwTime;
  ULONG_PTR dwExtraInfo;
  DWORD     cxContact;
  DWORD     cyContact;
} TOUCHINPUT, *PTOUCHINPUT;

typedef struct _GESTURECONFIG {
  DWORD dwID;
  DWORD dwWant;
  DWORD dwBlock;
} GESTURECONFIG, *PGESTURECONFIG;

typedef struct _GESTUREINFO {
  UINT      cbSize;
  DWORD     dwFlags;
  DWORD     dwID;
  HWND      hwndTarget;
  POINTS    ptsLocation;
  DWORD     dwInstanceID;
  DWORD     dwSequenceID;
  ULONGLONG ullArguments;
  UINT      cbExtraArgs;
} GESTUREINFO, *PGESTUREINFO;

typedef enum tagINPUT_MESSAGE_DEVICE_TYPE { 
  IMDT_UNAVAILABLE  = 0, // 0x0
  IMDT_KEYBOARD     = 1, // 0x1
  IMDT_MOUSE        = 2, // 0x2
  IMDT_TOUCH        = 4, // 0x4
  IMDT_PEN          = 8 // 0x8
} INPUT_MESSAGE_DEVICE_TYPE;

typedef enum tagINPUT_MESSAGE_ORIGIN_ID { 
  IMO_UNAVAILABLE  = 0x00000000,
  IMO_HARDWARE     = 0x00000001,
  IMO_INJECTED     = 0x00000002,
  IMO_SYSTEM       = 0x00000004
} INPUT_MESSAGE_ORIGIN_ID;

typedef struct tagINPUT_MESSAGE_SOURCE {
  INPUT_MESSAGE_DEVICE_TYPE DeviceType;
  INPUT_MESSAGE_ORIGIN_ID   OriginId;
} INPUT_MESSAGE_SOURCE, *PINPUT_MESSAGE_SOURCE;

typedef struct tagCHANGEFILTERSTRUCT {
  DWORD cbSize;
  DWORD ExtStatus;
} CHANGEFILTERSTRUCT, *PCHANGEFILTERSTRUCT;

typedef enum  { 
  DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE        = 1,
  DISPLAYCONFIG_MODE_INFO_TYPE_TARGET        = 2,
  DISPLAYCONFIG_MODE_INFO_TYPE_FORCE_UINT32  = 0xFFFFFFFF
} DISPLAYCONFIG_MODE_INFO_TYPE;

typedef struct DISPLAYCONFIG_RATIONAL {
  UINT32 Numerator;
  UINT32 Denominator;
} DISPLAYCONFIG_RATIONAL;

typedef struct DISPLAYCONFIG_2DREGION {
  UINT32 cx;
  UINT32 cy;
} DISPLAYCONFIG_2DREGION;

typedef enum  { 
  DISPLAYCONFIG_SCANLINE_ORDERING_UNSPECIFIED                 = 0,
  DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE                 = 1,
  DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED                  = 2,
  DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED_UPPERFIELDFIRST  = DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED,
  DISPLAYCONFIG_SCANLINE_ORDERING_INTERLACED_LOWERFIELDFIRST  = 3,
  DISPLAYCONFIG_SCANLINE_ORDERING_FORCE_UINT32                = 0xFFFFFFFF
} DISPLAYCONFIG_SCANLINE_ORDERING;

typedef struct DISPLAYCONFIG_VIDEO_SIGNAL_INFO {
  UINT64                          pixelRate;
  DISPLAYCONFIG_RATIONAL          hSyncFreq;
  DISPLAYCONFIG_RATIONAL          vSyncFreq;
  DISPLAYCONFIG_2DREGION          activeSize;
  DISPLAYCONFIG_2DREGION          totalSize;
  union {
    struct {
      UINT32 videoStandard  :16;
      UINT32 vSyncFreqDivider  :6;
      UINT32 reserved  :10;
    } AdditionalSignalInfo;
    UINT32 videoStandard;
  };
  DISPLAYCONFIG_SCANLINE_ORDERING scanLineOrdering;
} DISPLAYCONFIG_VIDEO_SIGNAL_INFO;

typedef struct DISPLAYCONFIG_TARGET_MODE {
  DISPLAYCONFIG_VIDEO_SIGNAL_INFO targetVideoSignalInfo;
} DISPLAYCONFIG_TARGET_MODE;

typedef enum  { 
  DISPLAYCONFIG_TOPOLOGY_INTERNAL      = 0x00000001,
  DISPLAYCONFIG_TOPOLOGY_CLONE         = 0x00000002,
  DISPLAYCONFIG_TOPOLOGY_EXTEND        = 0x00000004,
  DISPLAYCONFIG_TOPOLOGY_EXTERNAL      = 0x00000008,
  DISPLAYCONFIG_TOPOLOGY_FORCE_UINT32  = 0xFFFFFFFF
} DISPLAYCONFIG_TOPOLOGY_ID;

// typedef struct _DESKTOPINFO
 // {
     // PVOID pvDesktopBase;
     // PVOID pvDesktopLimit;
     // struct _WND *spwnd;
     // DWORD fsHooks;
     // LIST_ENTRY aphkStart[NB_HOOKS];
 
     // HWND hTaskManWindow;
     // HWND hProgmanWindow;
     // HWND hShellWindow;
     // struct _WND * spwndShell;
 
     // PPROCESSINFO ppiShellProcess;
 
     // union
     // {
         // UINT Dummy;
         // struct
         // {
             // UINT LastInputWasKbd : 1;
         // };
     // };
 
     // WCHAR szDesktopName[1];
 // } DESKTOPINFO, *PDESKTOPINFO;

typedef enum  { 
  DISPLAYCONFIG_PIXELFORMAT_8BPP          = 1,
  DISPLAYCONFIG_PIXELFORMAT_16BPP         = 2,
  DISPLAYCONFIG_PIXELFORMAT_24BPP         = 3,
  DISPLAYCONFIG_PIXELFORMAT_32BPP         = 4,
  DISPLAYCONFIG_PIXELFORMAT_NONGDI        = 5,
  DISPLAYCONFIG_PIXELFORMAT_FORCE_UINT32  = 0xffffffff
} DISPLAYCONFIG_PIXELFORMAT;

typedef struct DISPLAYCONFIG_SOURCE_MODE {
  UINT32                    width;
  UINT32                    height;
  DISPLAYCONFIG_PIXELFORMAT pixelFormat;
  POINTL                    position;
} DISPLAYCONFIG_SOURCE_MODE;

typedef struct DISPLAYCONFIG_MODE_INFO {
  DISPLAYCONFIG_MODE_INFO_TYPE infoType;
  UINT32                       id;
  LUID                         adapterId;
  union {
    DISPLAYCONFIG_TARGET_MODE targetMode;
    DISPLAYCONFIG_SOURCE_MODE sourceMode;
  };
} DISPLAYCONFIG_MODE_INFO;

typedef enum  { 
  DISPLAYCONFIG_SCALING_IDENTITY                = 1,
  DISPLAYCONFIG_SCALING_CENTERED                = 2,
  DISPLAYCONFIG_SCALING_STRETCHED               = 3,
  DISPLAYCONFIG_SCALING_ASPECTRATIOCENTEREDMAX  = 4,
  DISPLAYCONFIG_SCALING_CUSTOM                  = 5,
  DISPLAYCONFIG_SCALING_PREFERRED               = 128,
  DISPLAYCONFIG_SCALING_FORCE_UINT32            = 0xFFFFFFFF
} DISPLAYCONFIG_SCALING;

typedef enum  { 
  DISPLAYCONFIG_ROTATION_IDENTITY      = 1,
  DISPLAYCONFIG_ROTATION_ROTATE90      = 2,
  DISPLAYCONFIG_ROTATION_ROTATE180     = 3,
  DISPLAYCONFIG_ROTATION_ROTATE270     = 4,
  DISPLAYCONFIG_ROTATION_FORCE_UINT32  = 0xFFFFFFFF
} DISPLAYCONFIG_ROTATION;

typedef enum  { 
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER                 = -1,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HD15                  = 0,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_SVIDEO                = 1,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_COMPOSITE_VIDEO       = 2,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_COMPONENT_VIDEO       = 3,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DVI                   = 4,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI                  = 5,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS                  = 6,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_D_JPN                 = 8,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_SDI                   = 9,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EXTERNAL  = 10,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED  = 11,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EXTERNAL          = 12,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED          = 13,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_SDTVDONGLE            = 14,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_MIRACAST              = 15,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL              = 0x80000000,
  DISPLAYCONFIG_OUTPUT_TECHNOLOGY_FORCE_UINT32          = 0xFFFFFFFF
} DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY;

typedef struct DISPLAYCONFIG_PATH_TARGET_INFO {
  LUID                                  adapterId;
  UINT32                                id;
  UINT32                                modeInfoIdx;
  DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY outputTechnology;
  DISPLAYCONFIG_ROTATION                rotation;
  DISPLAYCONFIG_SCALING                 scaling;
  DISPLAYCONFIG_RATIONAL                refreshRate;
  DISPLAYCONFIG_SCANLINE_ORDERING       scanLineOrdering;
  BOOL                                  targetAvailable;
  UINT32                                statusFlags;
} DISPLAYCONFIG_PATH_TARGET_INFO;

typedef struct DISPLAYCONFIG_PATH_SOURCE_INFO {
  LUID   adapterId;
  UINT32 id;
  UINT32 modeInfoIdx;
  UINT32 statusFlags;
} DISPLAYCONFIG_PATH_SOURCE_INFO;

typedef struct DISPLAYCONFIG_PATH_INFO {
  DISPLAYCONFIG_PATH_SOURCE_INFO sourceInfo;
  DISPLAYCONFIG_PATH_TARGET_INFO targetInfo;
  UINT32                         flags;
} DISPLAYCONFIG_PATH_INFO;

typedef enum  { 
  DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME            = 1,
  DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME            = 2,
  DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE  = 3,
  DISPLAYCONFIG_DEVICE_INFO_GET_ADAPTER_NAME           = 4,
  DISPLAYCONFIG_DEVICE_INFO_SET_TARGET_PERSISTENCE     = 5,
  DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_BASE_TYPE       = 6,
  DISPLAYCONFIG_DEVICE_INFO_FORCE_UINT32               = 0xFFFFFFFF
} DISPLAYCONFIG_DEVICE_INFO_TYPE;

typedef struct DISPLAYCONFIG_DEVICE_INFO_HEADER {
  DISPLAYCONFIG_DEVICE_INFO_TYPE type;
  UINT32                         size;
  LUID                           adapterId;
  UINT32                         id;
} DISPLAYCONFIG_DEVICE_INFO_HEADER;

typedef struct tagTOUCH_HIT_TESTING_INPUT {
  UINT32 pointerId;
  POINT  point;
  RECT   boundingBox;
  RECT   nonOccludedBoundingBox;
  UINT32 orientation;
} TOUCH_HIT_TESTING_INPUT, *PTOUCH_HIT_TESTING_INPUT;

typedef struct tagTOUCH_HIT_TESTING_PROXIMITY_EVALUATION {
  UINT16 score;
  POINT  adjustedPoint;
} TOUCH_HIT_TESTING_PROXIMITY_EVALUATION, *PTOUCH_HIT_TESTING_PROXIMITY_EVALUATION;

typedef struct _SCOPETABLE_ENTRY {
    int EnclosingLevel;                 /* lexical level of enclosing scope */
    int (__cdecl *Filter)(void);        /* NULL for a termination handler */
    void (__cdecl *SpecificHandler)(void); /* xcpt or termination handler */
}SCOPETABLE_ENTRY,*PSCOPETABLE_ENTRY;

typedef struct _EH3_EXCEPTION_REGISTRATION {
    struct _EH3_EXCEPTION_REGISTRATION *Next;
    PVOID ExceptionHandler;
    PSCOPETABLE_ENTRY ScopeTable;
    DWORD TryLevel;
} EH3_EXCEPTION_REGISTRATION, *PEH3_EXCEPTION_REGISTRATION;

typedef struct _CPPEH_RECORD{
	struct _EH3_EXCEPTION_REGISTRATION registration;
}CPPEH_RECORD, *PCPPEH_RECORD;

typedef struct _WINCOMPATTRDATA
{
    DWORD attribute; // the attribute to query, see below
    PVOID pData; // buffer to store the result
    ULONG dataSize; // size of the pData buffer
}WINCOMPATTRDATA, *PWINCOMPATTRDATA;

BOOL 
WINAPI 
GetCurrentInputMessageSource(
	PINPUT_MESSAGE_SOURCE this
);

#define POINTER_DEVICE_PRODUCT_STRING_MAX 520

#define OBJ_OTHER_PROCESS ((void *)1)  /* returned by get_user_handle_ptr on unknown handles */

typedef enum POINTER_DEVICE_TYPE { 
  POINTER_DEVICE_TYPE_INTEGRATED_PEN  = 0x00000001,
  POINTER_DEVICE_TYPE_EXTERNAL_PEN    = 0x00000002,
  POINTER_DEVICE_TYPE_TOUCH           = 0x00000003,
  POINTER_DEVICE_TYPE_TOUCH_PAD       = 0x00000004,
  POINTER_DEVICE_TYPE_MAX             = 0xFFFFFFFF
} POINTER_DEVICE_TYPE;

typedef struct tagPOINTER_DEVICE_INFO {
  DWORD               displayOrientation;
  HANDLE              device;
  POINTER_DEVICE_TYPE pointerDeviceType;
  HMONITOR            monitor;
  ULONG               startingCursorId;
  USHORT              maxActiveContacts;
  WCHAR               productString[POINTER_DEVICE_PRODUCT_STRING_MAX];
} POINTER_DEVICE_INFO;

enum user_obj_type
{
    USER_WINDOW = 1,  /* window */
    USER_MENU,        /* menu */
    USER_ACCEL,       /* accelerator */
    USER_ICON,        /* icon or cursor */
    USER_DWP          /* DeferWindowPos structure */
};

typedef enum PROCESS_DPI_AWARENESS
{
    PROCESS_DPI_UNAWARE,
    PROCESS_SYSTEM_DPI_AWARE,
    PROCESS_PER_MONITOR_DPI_AWARE
} PROCESS_DPI_AWARENESS;


typedef enum tagPOINTER_BUTTON_CHANGE_TYPE
{
    POINTER_CHANGE_NONE,
    POINTER_CHANGE_FIRSTBUTTON_DOWN,
    POINTER_CHANGE_FIRSTBUTTON_UP,
    POINTER_CHANGE_SECONDBUTTON_DOWN,
    POINTER_CHANGE_SECONDBUTTON_UP,
    POINTER_CHANGE_THIRDBUTTON_DOWN,
    POINTER_CHANGE_THIRDBUTTON_UP,
    POINTER_CHANGE_FOURTHBUTTON_DOWN,
    POINTER_CHANGE_FOURTHBUTTON_UP,
    POINTER_CHANGE_FIFTHBUTTON_DOWN,
    POINTER_CHANGE_FIFTHBUTTON_UP,
} POINTER_BUTTON_CHANGE_TYPE;

typedef UINT32 POINTER_FLAGS;

typedef struct tagPOINTER_INFO
{
    POINTER_INPUT_TYPE pointerType;
    UINT32 pointerId;
    UINT32 frameId;
    POINTER_FLAGS pointerFlags;
    HANDLE sourceDevice;
    HWND hwndTarget;
    POINT ptPixelLocation;
    POINT ptHimetricLocation;
    POINT ptPixelLocationRaw;
    POINT ptHimetricLocationRaw;
    DWORD dwTime;
    UINT32 historyCount;
    INT32 InputData;
    DWORD dwKeyStates;
    UINT64 PerformanceCount;
    POINTER_BUTTON_CHANGE_TYPE ButtonChangeType;
} POINTER_INFO;

typedef UINT32 PEN_FLAGS;
#define PEN_FLAG_NONE      0x00000000
#define PEN_FLAG_BARREL    0x00000001
#define PEN_FLAG_INVERTED  0x00000002
#define PEN_FLAG_ERASER    0x00000004

typedef UINT32 PEN_MASK;
#define PEN_MASK_NONE      0x00000000
#define PEN_MASK_PRESSURE  0x00000001
#define PEN_MASK_ROTATION  0x00000002
#define PEN_MASK_TILT_X    0x00000004
#define PEN_MASK_TILT_Y    0x00000008



typedef struct tagPOINTER_PEN_INFO
{
    POINTER_INFO pointerInfo;
    PEN_FLAGS penFlags;
    PEN_MASK penMask;
    UINT32 pressure;
    UINT32 rotation;
    INT32 tiltX;
    INT32 tiltY;
} POINTER_PEN_INFO;

typedef enum tagPOINTER_FLAGS {
  POINTER_FLAG_NONE           = 0x00000000,
  POINTER_FLAG_NEW            = 0x00000001,
  POINTER_FLAG_INRANGE        = 0x00000002,
  POINTER_FLAG_INCONTACT      = 0x00000004,
  POINTER_FLAG_FIRSTBUTTON    = 0x00000010,
  POINTER_FLAG_SECONDBUTTON   = 0x00000020,
  POINTER_FLAG_THIRDBUTTON    = 0x00000040,
  POINTER_FLAG_OTHERBUTTON    = 0x00000080,
  POINTER_FLAG_PRIMARY        = 0x00000100,
  POINTER_FLAG_CONFIDENCE     = 0x00000200,
  POINTER_FLAG_CANCELLED      = 0x00000400,
  POINTER_FLAG_DOWN           = 0x00010000,
  POINTER_FLAG_UPDATE         = 0x00020000,
  POINTER_FLAG_UP             = 0x00040000,
  POINTER_FLAG_WHEEL          = 0x00080000,
  POINTER_FLAG_HWHEEL         = 0x00100000 
} POINTER_FLAGS;

typedef UINT32 TOUCH_FLAGS;
#define TOUCH_FLAG_NONE  0x00000000

typedef UINT32 TOUCH_MASK;
#define TOUCH_MASK_NONE         0x00000000
#define TOUCH_MASK_CONTACTAREA  0x00000001
#define TOUCH_MASK_ORIENTATION  0x00000002
#define TOUCH_MASK_PRESSURE     0x00000004

typedef struct tagPOINTER_TOUCH_INFO {
  POINTER_INFO pointerInfo;
  TOUCH_FLAGS  touchFlags;
  TOUCH_MASK   touchMask;
  RECT         rcContact;
  RECT         rcContactRaw;
  UINT32       orientation;
  UINT32       pressure;
} POINTER_TOUCH_INFO;

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

typedef enum tagPROCESS_UICONTEXT {
    PROCESS_UICONTEXT_DESKTOP = 0,
    PROCESS_UICONTEXT_IMMERSIVE = 1,
    PROCESS_UICONTEXT_IMMERSIVE_BROKER = 2,
    PROCESS_UICONTEXT_IMMERSIVE_BROWSER = 3
} PROCESS_UICONTEXT;

typedef enum tagPROCESS_UI_FLAGS {
    PROCESS_UIF_NONE = 0,
    PROCESS_UIF_AUTHORING_MODE = 1,
    PROCESS_UIF_RESTRICTIONS_DISABLED = 2
} PROCESS_UI_FLAGS;

typedef struct tagPROCESS_UICONTEXT_INFORMATION {
    DWORD UIContext; //PROCESS_UICONTEXT
    DWORD dwFlags; //PROCESS_UI_FLAGS
} PROCESS_UICONTEXT_INFORMATION, * PPROCESS_UICONTEXT_INFORMATION;

struct cursoricon_frame
{
    UINT     width;    /* frame-specific width */
    UINT     height;   /* frame-specific height */
    HBITMAP  color;    /* color bitmap */
    HBITMAP  alpha;    /* pre-multiplied alpha bitmap for 32-bpp icons */
    HBITMAP  mask;     /* mask bitmap (followed by color for 1-bpp icons) */
    POINT    hotspot;
};

struct user_object
{
    HANDLE             handle;
    enum user_obj_type type;
};

struct cursoricon_object
{
    struct user_object      obj;        /* object header */
    struct list             entry;      /* entry in shared icons list */
    ULONG_PTR               param;      /* opaque param used by 16-bit code */
    HMODULE                 module;     /* module for icons loaded from resources */
    LPWSTR                  resname;    /* resource name for icons loaded from resources */
    HRSRC                   rsrc;       /* resource for shared icons */
    BOOL                    is_icon;    /* whether icon or cursor */
    BOOL                    is_ani;     /* whether this object is a static cursor or an animated cursor */
    UINT                    delay;      /* delay between this frame and the next (in jiffies) */
    POINT                   hotspot;
};

struct cursoricon_desc
{
    UINT flags;
    UINT num_steps;
    UINT num_frames;
    UINT delay;
    struct cursoricon_frame *frames;
    DWORD *frame_seq;
    DWORD *frame_rates;
    HRSRC rsrc;
};

typedef struct
{
    BYTE   bWidth;
    BYTE   bHeight;
    BYTE   bColorCount;
    BYTE   bReserved;
} ICONRESDIR;

typedef struct
{
    WORD   wWidth;
    WORD   wHeight;
} CURSORDIR;

typedef struct
{   union
    { ICONRESDIR icon;
      CURSORDIR  cursor;
    } ResInfo;
    WORD   wPlanes;
    WORD   wBitCount;
    DWORD  dwBytesInRes;
    WORD   wResId;
} CURSORICONDIRENTRY;

typedef struct
{
    WORD                idReserved;
    WORD                idType;
    WORD                idCount;
    CURSORICONDIRENTRY  idEntries[1];
} CURSORICONDIR;

// typedef struct _CURSORICONFILEDIRENTRY
// {
    // BYTE bWidth;
    // BYTE bHeight;
    // BYTE bColorCount;
    // BYTE bReserved;
    // union
    // {
        // WORD wPlanes; /* For icons */
        // WORD xHotspot; /* For cursors */
    // };
    // union
    // {
        // WORD wBitCount; /* For icons */
        // WORD yHotspot; /* For cursors */
    // };
    // DWORD dwDIBSize;
    // DWORD dwDIBOffset;
// } CURSORICONFILEDIRENTRY;

// typedef struct _CURSORICONFILEDIR
// {
    // WORD idReserved;
    // WORD idType;
    // WORD idCount;
    // CURSORICONFILEDIRENTRY idEntries[1];
// } CURSORICONFILEDIR;

typedef struct
{
    WORD idReserved;   // Sempre 0
    WORD idType;       // 1 para ícone
    WORD idCount;      // Número de imagens
} ICONDIR;

typedef struct
{
    BYTE  bWidth;
    BYTE  bHeight;
    BYTE  bColorCount;
    BYTE  bReserved;
    WORD  wPlanes;
    WORD  wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
} ICONDIRENTRY;

BOOL 
WINAPI 
IsProcessDPIAware();

BOOL 
WINAPI
SystemParametersInfoWInternal(
	UINT uiAction,
	UINT uiParam,
	PVOID pvParam,
	UINT fWinIni
);
	
HICON WINAPI CreateIconFromResourceExHook(
  _In_  PBYTE pbIconBits,
  _In_  DWORD cbIconBits,
  _In_  BOOL fIcon,
  _In_  DWORD dwVersion,
  _In_  int cxDesired,
  _In_  int cyDesired,
  _In_  UINT uFlags
);	

PVOID 
TryGetProcedure(
	char* procedureName
);

// Definitions of prototype functions for get address
	
typedef BOOL (WINAPI *AddClipboardFormatListenerFuncPtr)(
    HWND);	
	
typedef BOOL (WINAPI *CalculatePopupWindowPositionFuncPtr)(
    const POINT *,
	const SIZE  *,
	UINT,
	RECT *,
	RECT *);	

typedef BOOL (WINAPI *CancelShutdownFuncPtr)(
    VOID);

typedef BOOL (WINAPI *ChangeWindowMessageFilterFuncPtr)(
    UINT,
	DWORD);
	
typedef BOOL (WINAPI *ChangeWindowMessageFilterExFuncPtr)(
    HWND,
	UINT,
	DWORD,
	PCHANGEFILTERSTRUCT);
	
typedef NTSTATUS (WINAPI *CheckDesktopByThreadIdFuncPtr)(
    ULONG);	
	
typedef BOOL (WINAPI *CheckWindowThreadDesktopFuncPtr)(
    HWND,
	DWORD,
	ULONG);	
	
typedef BOOL (WINAPI *CheckWindowThreadDesktopFuncPtr)(
    HWND,
	DWORD,
	ULONG);
	
typedef BOOL (WINAPI *CloseGestureInfoHandleFuncPtr)(
    HGESTUREINFO);

typedef BOOL (WINAPI *CloseTouchInputHandleFuncPtr)(
    HTOUCHINPUT);
	
typedef BOOL (WINAPI *ConsoleControlFuncPtr)(
    CONSOLECONTROL,
	PVOID,
	ULONG);
	
typedef BOOL (WINAPI *ControlMagnificationFuncPtr)(
    BOOL,
	PVOID);	
	
typedef BOOL (WINAPI *PrivateRegisterICSProcFuncPtr)(
    PCONVERT_TO_DIB_PROC);		
	
static PrivateRegisterICSProcFuncPtr PrivateRegisterICSProcAddr;