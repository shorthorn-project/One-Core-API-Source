/* MAIN INCLUDES **********************************************************/

#include <wine/config.h>

//#include <ntstatus.h>
#define WIN32_NO_STATUS

#include <wine/debug.h>

#include <winbase.h>

/* PSEH for SEH Support */
#include <pseh/pseh2.h>
#define NDEBUG

#include <winuser.h>
#include <cmfuncs.h>
#include <psfuncs.h>
#include <rtlfuncs.h>
#define NTOS_MODE_USER
#include <iofuncs.h>
#include <csr/csr.h>

#include <config.h>
#include <port.h>
#include <baseapi.h>
#include <winnls.h>
#include <unicode.h>
#include <base.h>
#include <wincon.h>
#include <strsafe.h>

#define WIN32_NO_STATUS
#include <obfuncs.h>
#include <mmfuncs.h>
#include <exfuncs.h>
#include <kefuncs.h>
#include <sefuncs.h>
#include <dbgktypes.h>
#include <umfuncs.h>

#include <winreg.h>
#include <wingdi.h>

#include "datetime.h"
#include <shlwapi.h>
#include <psapi.h>
#include <timezoneapi.h>
#include <wine/heap.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define NUMA_NO_PREFERRED_NODE ((DWORD) -1)

#define JobObjectGroupInformationEx 14

#define PROCESS_DEP_ENABLE 0x00000001
#define PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION 0x00000002

#define FIND_DATA_SIZE 0x4000
#define BASESRV_SERVERDLL_INDEX 1
#define LOCALE_NAME_USER_DEFAULT    NULL
// #define CREATE_EVENT_MANUAL_RESET 1
// #define CREATE_EVENT_INITIAL_SET  2
#define SYMBOLIC_LINK_FLAG_DIRECTORY  0x1
#define REPARSE_DATA_BUFFER_HEADER_SIZE   FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
#define APPMODEL_ERROR_NO_PACKAGE                          15700
#define APPMODEL_ERROR_PACKAGE_RUNTIME_CORRUPT             15701
#define APPMODEL_ERROR_PACKAGE_IDENTITY_CORRUPT            15702
#define APPMODEL_ERROR_NO_APPLICATION                      15703

typedef struct _PROC_THREAD_ATTRIBUTE_LIST
*PPROC_THREAD_ATTRIBUTE_LIST, *LPPROC_THREAD_ATTRIBUTE_LIST;

#define PROC_THREAD_ATTRIBUTE_NUMBER   0x0000ffff
#define PROC_THREAD_ATTRIBUTE_THREAD   0x00010000
#define PROC_THREAD_ATTRIBUTE_INPUT    0x00020000
#define PROC_THREAD_ATTRIBUTE_ADDITIVE 0x00040000

typedef enum _PROC_THREAD_ATTRIBUTE_NUM_WIN7
{
    // ProcThreadAttributeParentProcess = 0,
    ProcThreadAttributeExtendedFlags = 1,
    // ProcThreadAttributeHandleList = 2,
    // ProcThreadAttributeGroupAffinity = 3,
    ProcThreadAttributePreferredNode = 4,
    // ProcThreadAttributeIdealProcessor = 5,
    // ProcThreadAttributeUmsThread = 6,
    // ProcThreadAttributeMitigationPolicy = 7,
    // ProcThreadAttributeSecurityCapabilities = 9,
	ProcThreadAttributeConsoleReference = 10,
    // ProcThreadAttributeProtectionLevel = 11,
    // ProcThreadAttributeJobList = 13,
    // ProcThreadAttributeChildProcessPolicy = 14,
    // ProcThreadAttributeAllApplicationPackagesPolicy = 15,
    // ProcThreadAttributeWin32kFilter = 16,
    // ProcThreadAttributeSafeOpenPromptOriginClaim = 17,
    ProcThreadAttributeDesktopAppPolicy = 18,
	ProcThreadAttributePseudoConsole =  22,
	ProcThreadAttributeMax
} PROC_THREAD_ATTRIBUTE_NUM_WIN7;

#define ProcThreadAttributeValue(Number, Thread, Input, Additive) \
    (((Number) & PROC_THREAD_ATTRIBUTE_NUMBER) | \
     ((Thread != FALSE) ? PROC_THREAD_ATTRIBUTE_THREAD : 0) | \
     ((Input != FALSE) ? PROC_THREAD_ATTRIBUTE_INPUT : 0) | \
     ((Additive != FALSE) ? PROC_THREAD_ATTRIBUTE_ADDITIVE : 0))

#define PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES (ProcThreadAttributeSecurityCapabilities | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_PROTECTION_LEVEL (ProcThreadAttributeProtectionLevel | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_JOB_LIST (ProcThreadAttributeJobList | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY (ProcThreadAttributeChildProcessPolicy | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY (ProcThreadAttributeAllApplicationPackagesPolicy | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_WIN32K_FILTER (ProcThreadAttributeWin32kFilter | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_DESKTOP_APP_POLICY (ProcThreadAttributeDesktopAppPolicy | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE (ProcThreadAttributePseudoConsole | PROC_THREAD_ATTRIBUTE_INPUT)
#define PROC_THREAD_ATTRIBUTE_FLAG_REPLACE_EXTENDEDFLAGS	1


#ifndef PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES
#define PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES \
	ProcThreadAttributeValue (ProcThreadAttributeSecurityCapabilities, FALSE, TRUE, FALSE)
#endif

// #define PROC_THREAD_ATTRIBUTE_PARENT_PROCESS \
    // ProcThreadAttributeValue (ProcThreadAttributeParentProcess, FALSE, TRUE, FALSE)
#define PROC_THREAD_ATTRIBUTE_EXTENDED_FLAGS \
    ProcThreadAttributeValue (ProcThreadAttributeExtendedFlags, FALSE, TRUE, TRUE)
// #define PROC_THREAD_ATTRIBUTE_HANDLE_LIST \
    // ProcThreadAttributeValue (ProcThreadAttributeHandleList, FALSE, TRUE, FALSE)
#define PROC_THREAD_ATTRIBUTE_GROUP_AFFINITY \
    ProcThreadAttributeValue (ProcThreadAttributeGroupAffinity, TRUE, TRUE, FALSE)
#define PROC_THREAD_ATTRIBUTE_PREFERRED_NODE \
    ProcThreadAttributeValue (ProcThreadAttributePreferredNode, FALSE, TRUE, FALSE)
// #define PROC_THREAD_ATTRIBUTE_IDEAL_PROCESSOR \
    // ProcThreadAttributeValue (ProcThreadAttributeIdealProcessor, TRUE, TRUE, FALSE)
#define PROC_THREAD_ATTRIBUTE_UMS_THREAD \
    ProcThreadAttributeValue (ProcThreadAttributeUmsThread, TRUE, TRUE, FALSE)
#define PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY \
    ProcThreadAttributeValue (ProcThreadAttributeMitigationPolicy, FALSE, TRUE, FALSE)
#define PROC_THREAD_ATTRIBUTE_CONSOLE_REFERENCE \
    ProcThreadAttributeValue(ProcThreadAttributeConsoleReference, FALSE, TRUE, FALSE)

#ifndef PROC_THREAD_ATTRIBUTE_PROTECTION_LEVEL
#define PROC_THREAD_ATTRIBUTE_PROTECTION_LEVEL \
	ProcThreadAttributeValue (ProcThreadAttributeProtectionLevel, FALSE, TRUE, FALSE)
#endif

typedef struct _UMS_CREATE_THREAD_ATTRIBUTES {
  DWORD UmsVersion;
  PVOID UmsContext;
  PVOID UmsCompletionList;
} UMS_CREATE_THREAD_ATTRIBUTES, *PUMS_CREATE_THREAD_ATTRIBUTES;

#define MAX_RESTART_CMD_LINE 0x800
#define RESTART_CYCLICAL 0x1
#define RESTART_NOTIFY_SOLUTION 0x2
#define RESTART_NOTIFY_FAULT 0x4
#define VOLUME_NAME_DOS 0x0
#define VOLUME_NAME_GUID 0x1
#define VOLUME_NAME_NT 0x2
#define VOLUME_NAME_NONE 0x4
#define FILE_NAME_NORMALIZED 0x0
#define FILE_NAME_OPENED 0x8
#define FILE_SKIP_COMPLETION_PORT_ON_SUCCESS 0x1
#define FILE_SKIP_SET_EVENT_ON_HANDLE 0x2

#define NLS_DEFAULT_LANGID        0x0409

#define CREATE_EVENT_MANUAL_RESET   0x1
#define CREATE_EVENT_INITIAL_SET    0x2
#define CREATE_MUTEX_INITIAL_OWNER  0x1
#define CREATE_WAITABLE_TIMER_MANUAL_RESET  0x1
#define SRWLOCK_INIT    RTL_SRWLOCK_INIT
#define CONDITION_VARIABLE_INIT RTL_CONDITION_VARIABLE_INIT
#define CONDITION_VARIABLE_LOCKMODE_SHARED  RTL_CONDITION_VARIABLE_LOCKMODE_SHARED

/* flags that can be passed to LoadLibraryEx */
#define LOAD_LIBRARY_REQUIRE_SIGNED_TARGET  0x00000080

#define RESOURCE_ENUM_LN          0x0001
#define RESOURCE_ENUM_MUI         0x0002
#define RESOURCE_ENUM_MUI_SYSTEM  0x0004
#define RESOURCE_ENUM_VALIDATE    0x0008

#define LOCALE_ALL                  0x00
#define LOCALE_WINDOWS              0x01
#define LOCALE_SUPPLEMENTAL         0x02
#define LOCALE_ALTERNATE_SORTS      0x04
#define LOCALE_REPLACEMENT          0x08
#define LOCALE_NEUTRALDATA          0x10
#define LOCALE_SPECIFICDATA         0x20
#define LOCALE_SLOCALIZEDDISPLAYNAME 0x0002
#define LOCALE_SLOCALIZEDCOUNTRYNAME 0x0006
#define LOCALE_SPARENT 0x0000006d
#define LOCALE_INEUTRAL             0x0071

#ifndef FileIdInformation
#define FileIdInformation (enum _FILE_INFORMATION_CLASS)59
#endif

#define SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY		0x01
#define SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY	0x02
#define SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE		0x04

#define ARGUMENT_PRESENT(x) ((x) != NULL)

#define NORM_LINGUISTIC_CASING     0x08000000
#define FIND_STARTSWITH            0x00100000
#define FIND_ENDSWITH              0x00200000
#define FIND_FROMSTART             0x00400000
#define FIND_FROMEND               0x00800000

#define EXTENDED_STARTUPINFO_PRESENT 0x00080000

//
// Do not add heap dumps for reports for the process
//
#define WER_FAULT_REPORTING_FLAG_NOHEAP 1

//
// Fault reporting UI should always be shown. This is only applicable for interactive processes
//
#define WER_FAULT_REPORTING_ALWAYS_SHOW_UI          16

#define KEY_LENGTH 1024

#define PATHCCH_MAX_CCH 0x8000

/* Wine-specific exceptions codes */

#define EXCEPTION_WINE_STUB       0x80000100  /* stub entry point called */
#define EXCEPTION_WINE_ASSERTION 0x80000101 /* assertion failed */

#if (_WIN32_WINNT < _WIN32_WINNT_VISTA)
 
#define FileIoCompletionNotificationInformation	41
#define FileIoStatusBlockRangeInformation	 42
#define FileIoPriorityHintInformation	43
#define FileSfioReserveInformation	44
#define FileSfioVolumeInformation	45
#define FileHardLinkInformation	46
#define FileProcessIdsUsingFileInformation	47
#define FileNormalizedNameInformation	48
#define FileNetworkPhysicalNameInformation	49
#define FileIdGlobalTxDirectoryInformation	50
#define FileIsRemoteDeviceInformation	51
#define FileUnusedInformation	52
#define FileNumaNodeInformation	53
#define FileStandardLinkInformation	54
#define FileRemoteProtocolInformation	55
	
#endif	

#ifdef _M_IX86
#define CONTEXT_AMD64   0x00100000
#endif

#define CONTEXT_AMD64_CONTROL   (CONTEXT_AMD64 | 0x0001)
#define CONTEXT_AMD64_INTEGER   (CONTEXT_AMD64 | 0x0002)
#define CONTEXT_AMD64_SEGMENTS  (CONTEXT_AMD64 | 0x0004)
#define CONTEXT_AMD64_FLOATING_POINT  (CONTEXT_AMD64 | 0x0008)
#define CONTEXT_AMD64_DEBUG_REGISTERS (CONTEXT_AMD64 | 0x0010)
#define CONTEXT_AMD64_FULL (CONTEXT_AMD64_CONTROL | CONTEXT_AMD64_INTEGER | CONTEXT_AMD64_FLOATING_POINT)
#define CONTEXT_AMD64_ALL (CONTEXT_AMD64_CONTROL | CONTEXT_AMD64_INTEGER | CONTEXT_AMD64_SEGMENTS | CONTEXT_AMD64_FLOATING_POINT | CONTEXT_AMD64_DEBUG_REGISTERS)

#define RtlProcessHeap() (NtCurrentPeb()->ProcessHeap)
#define AddrCVarAssocTblSize ((SIZE_T)256)

/*
 * RegGetValue() restrictions
 */

#define RRF_RT_REG_NONE         (1 << 0)
#define RRF_RT_REG_SZ           (1 << 1)
#define RRF_RT_REG_EXPAND_SZ    (1 << 2)
#define RRF_RT_REG_BINARY       (1 << 3)
#define RRF_RT_REG_DWORD        (1 << 4)
#define RRF_RT_REG_MULTI_SZ     (1 << 5)
#define RRF_RT_REG_QWORD        (1 << 6)
#define RRF_RT_DWORD            (RRF_RT_REG_BINARY | RRF_RT_REG_DWORD)
#define RRF_RT_QWORD            (RRF_RT_REG_BINARY | RRF_RT_REG_QWORD)
//#define RRF_RT_ANY              0xffff
#define RRF_SUBKEY_WOW6464KEY   (1 << 16)
#define RRF_SUBKEY_WOW6432KEY   (1 << 17)
#define RRF_WOW64_MASK          (RRF_SUBKEY_WOW6432KEY | RRF_SUBKEY_WOW6464KEY)
#define RRF_NOEXPAND            (1 << 28)
#define RRF_ZEROONFAILURE       (1 << 29)

#define MEM_ROTATE       0x800000
#define MEM_LARGE_PAGES  0x20000000

static const BOOL is_win64 = (sizeof(void *) > sizeof(int));
volatile long TzSpecificCache;
extern ULONG BaseDllTag;
PBASE_STATIC_SERVER_DATA BaseStaticServerData;
extern BOOL bIsFileApiAnsi;
extern HMODULE kernel32_handle DECLSPEC_HIDDEN;

typedef void *HPCON;

typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;

typedef RTL_CONDITION_VARIABLE CONDITION_VARIABLE, *PCONDITION_VARIABLE;

typedef struct _FILE_IO_COMPLETION_NOTIFICATION_INFORMATION {
    ULONG Flags;
} FILE_IO_COMPLETION_NOTIFICATION_INFORMATION, *PFILE_IO_COMPLETION_NOTIFICATION_INFORMATION;


/* TYPE DEFINITIONS **********************************************************/
typedef UINT(WINAPI * PPROCESS_START_ROUTINE)(VOID);
typedef RTL_CONDITION_VARIABLE CONDITION_VARIABLE, *PCONDITION_VARIABLE;
typedef NTSTATUS(NTAPI * PRTL_CONVERT_STRING)(IN PUNICODE_STRING UnicodeString, IN PANSI_STRING AnsiString, IN BOOLEAN AllocateMemory);
typedef DWORD (WINAPI *APPLICATION_RECOVERY_CALLBACK)(PVOID);
typedef VOID (CALLBACK *PTP_WIN32_IO_CALLBACK)(PTP_CALLBACK_INSTANCE,PVOID,PVOID,ULONG,ULONG_PTR,PTP_IO);
typedef INT (WINAPI *MessageBoxA_funcptr)(HWND,LPCSTR,LPCSTR,UINT);
typedef INT (WINAPI *MessageBoxW_funcptr)(HWND,LPCWSTR,LPCWSTR,UINT);

/* STRUCTS DEFINITIONS ******************************************************/

typedef enum AppPolicyProcessTerminationMethod
{
    AppPolicyProcessTerminationMethod_ExitProcess      = 0,
    AppPolicyProcessTerminationMethod_TerminateProcess = 1,
} AppPolicyProcessTerminationMethod;

typedef enum AppPolicyThreadInitializationType
{
    AppPolicyThreadInitializationType_None            = 0,
    AppPolicyThreadInitializationType_InitializeWinRT = 1,
} AppPolicyThreadInitializationType;

typedef enum AppPolicyShowDeveloperDiagnostic
{
    AppPolicyShowDeveloperDiagnostic_None   = 0,
    AppPolicyShowDeveloperDiagnostic_ShowUI = 1,
} AppPolicyShowDeveloperDiagnostic;

typedef enum AppPolicyWindowingModel
{
    AppPolicyWindowingModel_None           = 0,
    AppPolicyWindowingModel_Universal      = 1,
    AppPolicyWindowingModel_ClassicDesktop = 2,
    AppPolicyWindowingModel_ClassicPhone   = 3
} AppPolicyWindowingModel;

typedef enum _FIND_DATA_TYPE
{
    FindFile   = 1,
    FindStream = 2
} FIND_DATA_TYPE;

typedef enum _PIPE_ATTRIBUTE_TYPE {
  PipeAttribute = 0,
  PipeConnectionAttribute = 1,
  PipeHandleAttribute = 2
}PIPE_ATTRIBUTE_TYPE, *PPIPE_ATTRIBUTE_TYPE;

typedef union _DIR_INFORMATION
{
    PVOID DirInfo;
    PFILE_FULL_DIR_INFORMATION FullDirInfo;
    PFILE_BOTH_DIR_INFORMATION BothDirInfo;
} DIR_INFORMATION;

typedef enum _DEP_SYSTEM_POLICY_TYPE {
    AlwaysOff = 0,
    AlwaysOn = 1,
    OptIn = 2,
    OptOut = 3
} DEP_SYSTEM_POLICY_TYPE;

typedef struct _FIND_FILE_DATA
{
    HANDLE Handle;
    FINDEX_INFO_LEVELS InfoLevel;
    FINDEX_SEARCH_OPS SearchOp;

    /*
     * For handling STATUS_BUFFER_OVERFLOW errors emitted by
     * NtQueryDirectoryFile in the FildNextFile function.
     */
    BOOLEAN HasMoreData;

    /*
     * "Pointer" to the next file info structure in the buffer.
     * The type is defined by the 'InfoLevel' parameter.
     */
    DIR_INFORMATION NextDirInfo;

    BYTE Buffer[FIND_DATA_SIZE];
} FIND_FILE_DATA, *PFIND_FILE_DATA;

typedef struct _FIND_STREAM_DATA
{
    STREAM_INFO_LEVELS InfoLevel;
    PFILE_STREAM_INFORMATION FileStreamInfo;
    PFILE_STREAM_INFORMATION CurrentInfo;
} FIND_STREAM_DATA, *PFIND_STREAM_DATA;

typedef struct _FIND_DATA_HANDLE
{
    FIND_DATA_TYPE Type;
    RTL_CRITICAL_SECTION Lock;

    /*
     * Pointer to the following finding data, located at
     * (this + 1). The type is defined by the 'Type' parameter.
     */
    union
    {
        PFIND_FILE_DATA FindFileData;
        PFIND_STREAM_DATA FindStreamData;
    } u;

} FIND_DATA_HANDLE, *PFIND_DATA_HANDLE;

typedef struct _REASON_CONTEXT {
  ULONG Version;
  DWORD Flags;
  union {
    struct {
      HMODULE LocalizedReasonModule;
      ULONG   LocalizedReasonId;
      ULONG   ReasonStringCount;
      LPWSTR  *ReasonStrings;
    } Detailed;
    LPWSTR SimpleReasonString;
  } Reason;
} REASON_CONTEXT, *PREASON_CONTEXT;

typedef struct _PROC_THREAD_ATTRIBUTE
{
	DWORD_PTR Attribute;
	SIZE_T cbSize;
	PVOID lpValue;
} PROC_THREAD_ATTRIBUTE;

typedef struct _PROC_THREAD_ATTRIBUTE_LIST
{
	DWORD AttributeFlags;
	DWORD MaxCount;
	DWORD Count;
	DWORD Pad4B;
	PROC_THREAD_ATTRIBUTE* ExtendedEntry;
	PROC_THREAD_ATTRIBUTE AttributeList[ANYSIZE_ARRAY];
} PROC_THREAD_ATTRIBUTE_LIST;

typedef enum _WER_REGISTER_FILE_TYPE
{
     WerRegFileTypeUserDocument = 1,
     WerRegFileTypeOther = 2,
     WerRegFileTypeMax
} WER_REGISTER_FILE_TYPE;

typedef struct _LOCALE_LCID{
	LPCWSTR localeName;
	LCID lcid;
}LOCALE_LCID;

typedef struct _NeutralToSpecific
{
	LPCWSTR szNeutralLocale;
	LPCWSTR szSpecificLocale;
}NeutralToSpecific;

typedef struct _LcidFallback
{
	LCID Locale;
	LCID Fallback;
}LcidFallback;

// Map of LCID to locale name.
typedef struct _LcidToLocaleName
{
	LCID    Locale;
	LPCWSTR localeName;
}LcidToLocaleName;

typedef enum _POWER_REQUEST_TYPE { 
  PowerRequestDisplayRequired,
  PowerRequestSystemRequired,
  PowerRequestAwayModeRequired,
  PowerRequestExecutionRequired
} POWER_REQUEST_TYPE, *PPOWER_REQUEST_TYPE;

typedef struct _BASEP_ACTCTX_BLOCK
{
     ULONG Flags;
     PVOID ActivationContext;
     PVOID CompletionContext;
     LPOVERLAPPED_COMPLETION_ROUTINE CompletionRoutine;
} BASEP_ACTCTX_BLOCK, *PBASEP_ACTCTX_BLOCK;

typedef struct _BASE_ACTIVATION_CONTEXT_ACTIVATION_BLOCK {
    DWORD Flags;
    PVOID CallbackFunction;
    PVOID CallbackContext;
    PACTIVATION_CONTEXT ActivationContext;
} BASE_ACTIVATION_CONTEXT_ACTIVATION_BLOCK, *PBASE_ACTIVATION_CONTEXT_ACTIVATION_BLOCK;

#define PATHCCH_NONE                            0x00
#define PATHCCH_ALLOW_LONG_PATHS                0x01
#define PATHCCH_FORCE_ENABLE_LONG_NAME_PROCESS  0x02
#define PATHCCH_FORCE_DISABLE_LONG_NAME_PROCESS 0x04
#define PATHCCH_DO_NOT_NORMALIZE_SEGMENTS       0x08
#define PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH  0x10

#define FSCTL_PIPE_GET_CONNECTION_ATTRIBUTE CTL_CODE(FILE_DEVICE_NAMED_PIPE, 12, METHOD_BUFFERED, FILE_ANY_ACCESS)

extern HANDLE           hAltSortsKey;       // handle to Locale\Alternate Sorts key

HRESULT 
WINAPI 
PathCchAddBackslash(
	WCHAR *path, 
	SIZE_T size
);

HRESULT 
WINAPI 
PathCchAddBackslashEx(
	WCHAR *path, 
	SIZE_T size, 
	WCHAR **end, 
	SIZE_T *remaining
);

HRESULT 
WINAPI 
PathCchCombineEx(
	WCHAR *out, 
	SIZE_T size, 
	const WCHAR *path1, 
	const WCHAR *path2, 
	DWORD flags
);

typedef struct {
  UINT  cbSize;
  UINT  HistoryBufferSize;
  UINT  NumberOfHistoryBuffers;
  DWORD dwFlags;
} CONSOLE_HISTORY_INFO, *PCONSOLE_HISTORY_INFO;

typedef struct _CONSOLE_SCREEN_BUFFER_INFOEX {
  ULONG      cbSize;
  COORD      dwSize;
  COORD      dwCursorPosition;
  WORD       wAttributes;
  SMALL_RECT srWindow;
  COORD      dwMaximumWindowSize;
  WORD       wPopupAttributes;
  BOOL       bFullscreenSupported;
  COLORREF   ColorTable[16];
} CONSOLE_SCREEN_BUFFER_INFOEX, *PCONSOLE_SCREEN_BUFFER_INFOEX;

typedef struct _CONSOLE_FONT_INFOEX {
  ULONG cbSize;
  DWORD nFont;
  COORD dwFontSize;
  UINT  FontFamily;
  UINT  FontWeight;
  WCHAR FaceName[LF_FACESIZE];
} CONSOLE_FONT_INFOEX, *PCONSOLE_FONT_INFOEX;

typedef struct _CONSOLE_GRAPHICS_BUFFER_INFO {
       DWORD        dwBitMapInfoLength;
       LPBITMAPINFO lpBitMapInfo;
       DWORD        dwUsage;    // DIB_PAL_COLORS or DIB_RGB_COLORS
       HANDLE       hMutex;
       PVOID        lpBitMap;
} CONSOLE_GRAPHICS_BUFFER_INFO, *PCONSOLE_GRAPHICS_BUFFER_INFO;

typedef enum _FILE_ID_TYPE {
    FileIdType,
    ObjectIdType,
    ExtendedFileIdType,
    MaximumFileIdType
} FILE_ID_TYPE, *PFILE_ID_TYPE;

typedef struct _FILE_ID_DESCRIPTOR {
    DWORD        dwSize;
    FILE_ID_TYPE Type;
    union {
        LARGE_INTEGER FileId;
        GUID          ObjectId;
    } DUMMYUNIONNAME;
} FILE_ID_DESCRIPTOR, *LPFILE_ID_DESCRIPTOR;

typedef enum _FILE_INFO_BY_HANDLE_CLASS {
    FileBasicInfo,
    FileStandardInfo,
    FileNameInfo,
    FileRenameInfo,
    FileDispositionInfo,
    FileAllocationInfo,
    FileEndOfFileInfo,
    FileStreamInfo,
    FileCompressionInfo,
    FileAttributeTagInfo,
    FileIdBothDirectoryInfo,
    FileIdBothDirectoryRestartInfo,
    FileIoPriorityHintInfo,
    FileRemoteProtocolInfo,
    FileFullDirectoryInfo,
    FileFullDirectoryRestartInfo,
    FileStorageInfo,
    FileAlignmentInfo,
    FileIdInfo,
    FileIdExtdDirectoryInfo,
    FileIdExtdDirectoryRestartInfo,
    MaximumFileInfoByHandlesClass
} FILE_INFO_BY_HANDLE_CLASS, *PFILE_INFO_BY_HANDLE_CLASS;

typedef struct _FILE_BASIC_INFO {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    DWORD FileAttributes;
} FILE_BASIC_INFO, *PFILE_BASIC_INFO;

typedef struct _FILE_STANDARD_INFO {
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    DWORD NumberOfLinks;
    BOOLEAN DeletePending;
    BOOLEAN Directory;
} FILE_STANDARD_INFO, *PFILE_STANDARD_INFO;

typedef struct _FILE_NAME_INFO {
    DWORD FileNameLength;
    WCHAR FileName[1];
} FILE_NAME_INFO, *PFILE_NAME_INFO;

typedef enum _PRIORITY_HINT {
    IoPriorityHintVeryLow,
    IoPriorityHintLow,
    IoPriorityHintNormal,
    MaximumIoPriorityHintType
} PRIORITY_HINT;

typedef struct _FILE_IO_PRIORITY_HINT_INFO {
    PRIORITY_HINT PriorityHint;
} FILE_IO_PRIORITY_HINT_INFO;

typedef struct _FILE_COMPRESSION_INFO {
    LARGE_INTEGER CompressedFileSize;
    WORD CompressionFormat;
    UCHAR CompressionUnitShift;
    UCHAR ChunkShift;
    UCHAR ClusterShift;
    UCHAR Reserved[3];
} FILE_COMPRESSION_INFO, *PFILE_COMPRESSION_INFO;

typedef struct _FILE_ATTRIBUTE_TAG_INFO {
    DWORD FileAttributes;
    DWORD ReparseTag;
} FILE_ATTRIBUTE_TAG_INFO, *PFILE_ATTRIBUTE_TAG_INFO;

typedef struct _FILE_ID_BOTH_DIR_INFO {
    DWORD         NextEntryOffset;
    DWORD         FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    DWORD         FileAttributes;
    DWORD         FileNameLength;
    DWORD         EaSize;
    CCHAR         ShortNameLength;
    WCHAR         ShortName[12];
    LARGE_INTEGER FileId;
    WCHAR         FileName[1];
} FILE_ID_BOTH_DIR_INFO, *PFILE_ID_BOTH_DIR_INFO;

typedef struct _FILE_REMOTE_PROTOCOL_INFO {
    USHORT StructureVersion;
    USHORT StructureSize;
    ULONG Protocol;
    USHORT ProtocolMajorVersion;
    USHORT ProtocolMinorVersion;
    USHORT ProtocolRevision;
    USHORT Reserved;
    ULONG Flags;
    struct {
        ULONG Reserved[8];
    } GenericReserved;
    struct {
        ULONG Reserved[16];
    } ProtocolSpecificReserved;
} FILE_REMOTE_PROTOCOL_INFO, *PFILE_REMOTE_PROTOCOL_INFO;

typedef struct _FILE_RENAME_INFO {
    BOOLEAN ReplaceIfExists;
    HANDLE RootDirectory;
    DWORD FileNameLength;
    WCHAR FileName[1];
} FILE_RENAME_INFO, *PFILE_RENAME_INFO;

typedef struct _FILE_ALLOCATION_INFO {
    LARGE_INTEGER AllocationSize;
} FILE_ALLOCATION_INFO, *PFILE_ALLOCATION_INFO;

typedef struct _FILE_DISPOSITION_INFO {
    BOOLEAN DeleteFile;
} FILE_DISPOSITION_INFO, *PFILE_DISPOSITION_INFO;

typedef struct _FILE_END_OF_FILE_INFO {
    LARGE_INTEGER EndOfFile;
} FILE_END_OF_FILE_INFO, *PFILE_END_OF_FILE_INFO;

typedef struct _RTL_BARRIER
{
	DWORD Reserved1;
	DWORD Reserved2;
	ULONG_PTR Reserved3[2];
	DWORD Reserved4;
	DWORD Reserved5;
} RTL_BARRIER, *PRTL_BARRIER, SYNCHRONIZATION_BARRIER, *PSYNCHRONIZATION_BARRIER, *LPSYNCHRONIZATION_BARRIER;

typedef enum
{
     INVALID_PATH = 0,
     UNC_PATH,              /* "//foo" */
     ABSOLUTE_DRIVE_PATH,   /* "c:/foo" */
     RELATIVE_DRIVE_PATH,   /* "c:foo" */
     ABSOLUTE_PATH,         /* "/foo" */
     RELATIVE_PATH,         /* "foo" */
     DEVICE_PATH,           /* "//./foo" */
     UNC_DOT_PATH           /* "//." */
} DOS_PATHNAME_TYPE;

typedef struct _STARTUPINFOEX {
  STARTUPINFO                 StartupInfo;
  PPROC_THREAD_ATTRIBUTE_LIST lpAttributeList;
} STARTUPINFOEX, *LPSTARTUPINFOEX;

#ifdef _M_AMD64
typedef struct _FLOATING_SAVE_AREA
{
     ULONG ControlWord;
     ULONG StatusWord;
     ULONG TagWord;
     ULONG ErrorOffset;
     ULONG ErrorSelector;
     ULONG DataOffset;
     ULONG DataSelector;
     UCHAR RegisterArea[80];
     ULONG Cr0NpxState;
} FLOATING_SAVE_AREA, *PFLOATING_SAVE_AREA;
#endif

typedef struct _WOW64_CONTEXT
{
     ULONGLONG ContextFlags;
     ULONGLONG Dr0;
     ULONGLONG Dr1;
     ULONGLONG Dr2;
     ULONGLONG Dr3;
     ULONGLONG Dr6;
     ULONGLONG Dr7;
     FLOATING_SAVE_AREA FloatSave;
     ULONGLONG SegGs;
     ULONGLONG SegFs;
     ULONGLONG SegEs;
     ULONGLONG SegDs;
     ULONGLONG Edi;
     ULONGLONG Esi;
     ULONGLONG Ebx;
     ULONGLONG Edx;
     ULONGLONG Ecx;
     ULONGLONG Eax;
     ULONGLONG Ebp;
     ULONGLONG Eip;
     ULONGLONG SegCs;
     ULONGLONG EFlags;
     ULONGLONG Esp;
     ULONGLONG SegSs;
     UCHAR ExtendedRegisters[512];
} WOW64_CONTEXT, *PWOW64_CONTEXT;

typedef struct _WOW64_LDT_ENTRY {
  WORD  LimitLow;
  WORD  BaseLow;
  union {
    struct {
      BYTE BaseMid;
      BYTE Flags1;
      BYTE Flags2;
      BYTE BaseHi;
    } Bytes;
    struct {
      DWORD BaseMid  :8;
      DWORD Type  :5;
      DWORD Dpl  :2;
      DWORD Pres  :1;
      DWORD LimitHi  :4;
      DWORD Sys  :1;
      DWORD Reserved_0  :1;
      DWORD Default_Big  :1;
      DWORD Granularity  :1;
      DWORD BaseHi  :8;
    } Bits;
  } HighWord;
} WOW64_LDT_ENTRY, *PWOW64_LDT_ENTRY;

typedef struct _CREATEFILE2_EXTENDED_PARAMETERS {
  DWORD                 dwSize;
  DWORD                 dwFileAttributes;
  DWORD                 dwFileFlags;
  DWORD                 dwSecurityQosFlags;
  LPSECURITY_ATTRIBUTES lpSecurityAttributes;
  HANDLE                hTemplateFile;
} CREATEFILE2_EXTENDED_PARAMETERS, *PCREATEFILE2_EXTENDED_PARAMETERS, *LPCREATEFILE2_EXTENDED_PARAMETERS;

typedef struct _PERF_COUNTERSET_INSTANCE {
  GUID  CounterSetGuid;
  ULONG dwSize;
  ULONG InstanceId;
  ULONG InstanceNameOffset;
  ULONG InstanceNameSize;
} PERF_COUNTERSET_INSTANCE, *PPERF_COUNTERSET_INSTANCE;

typedef struct _PERF_COUNTERSET_INFO {
  GUID  CounterSetGuid;
  GUID  ProviderGuid;
  ULONG NumCounters;
  ULONG InstanceType;
} PERF_COUNTERSET_INFO, *PPERF_COUNTERSET_INFO;

typedef LPVOID (CDECL * PERF_MEM_ALLOC)(SIZE_T,LPVOID);
typedef void (CDECL * PERF_MEM_FREE)(LPVOID,LPVOID);
typedef ULONG (WINAPI * PERFLIBREQUEST)(ULONG,PVOID,ULONG);

typedef struct _PROVIDER_CONTEXT {
  DWORD          ContextSize;
  DWORD          Reserved;
  PERFLIBREQUEST ControlCallback;
  PERF_MEM_ALLOC MemAllocRoutine;
  PERF_MEM_FREE  MemFreeRoutine;
  LPVOID         pMemContext;
} PERF_PROVIDER_CONTEXT, *PPERF_PROVIDER_CONTEXT;

typedef DYNAMIC_TIME_ZONE_INFORMATION RTL_DYNAMIC_TIME_ZONE_INFORMATION;

NTSTATUS WINAPI RtlQueryDynamicTimeZoneInformation(RTL_DYNAMIC_TIME_ZONE_INFORMATION*);

/* helper for kernel32->ntdll timeout format conversion */
static inline PLARGE_INTEGER get_nt_timeout( PLARGE_INTEGER pTime, DWORD timeout )
{
    if (timeout == INFINITE) return NULL;
    pTime->QuadPart = (ULONGLONG)timeout * -10000;
    return pTime;
}

int 
wine_compare_string(
	int flags, 
	const WCHAR *str1, 
	int len1,
    const WCHAR *str2, 
	int len2
);

int 
wine_get_sortkey(
	int flags, 
	const WCHAR *src, 
	int srclen, 
	char *dst, 
	int dstlen
);

typedef void *PUMS_CONTEXT;
typedef void *PUMS_COMPLETION_LIST;
typedef PRTL_UMS_SCHEDULER_ENTRY_POINT PUMS_SCHEDULER_ENTRY_POINT;

typedef struct _UMS_SCHEDULER_STARTUP_INFO
{
    ULONG UmsVersion;
    PUMS_COMPLETION_LIST CompletionList;
    PUMS_SCHEDULER_ENTRY_POINT SchedulerProc;
    PVOID SchedulerParam;
} UMS_SCHEDULER_STARTUP_INFO, *PUMS_SCHEDULER_STARTUP_INFO;

typedef enum _RTL_UMS_SCHEDULER_REASON UMS_SCHEDULER_REASON;
typedef enum _RTL_UMS_THREAD_INFO_CLASS UMS_THREAD_INFO_CLASS, *PUMS_THREAD_INFO_CLASS;

typedef struct _PSAPI_WS_WATCH_INFORMATION_EX {
  PSAPI_WS_WATCH_INFORMATION BasicInfo;
  ULONG_PTR FaultingThreadId;
  ULONG_PTR Flags;
} PSAPI_WS_WATCH_INFORMATION_EX, *PPSAPI_WS_WATCH_INFORMATION_EX;

typedef enum AppPolicyMediaFoundationCodecLoading
{
    AppPolicyMediaFoundationCodecLoading_All       = 0,
    AppPolicyMediaFoundationCodecLoading_InboxOnly = 1,
} AppPolicyMediaFoundationCodecLoading;

typedef enum _FIRMWARE_TYPE {
  FirmwareTypeUnknown,
  FirmwareTypeBios,
  FirmwareTypeUefi,
  FirmwareTypeMax
} FIRMWARE_TYPE, *PFIRMWARE_TYPE;

typedef enum _COPYFILE2_MESSAGE_TYPE
{
    COPYFILE2_CALLBACK_NONE = 0,
    COPYFILE2_CALLBACK_CHUNK_STARTED,
    COPYFILE2_CALLBACK_CHUNK_FINISHED,
    COPYFILE2_CALLBACK_STREAM_STARTED,
    COPYFILE2_CALLBACK_STREAM_FINISHED,
    COPYFILE2_CALLBACK_POLL_CONTINUE,
    COPYFILE2_CALLBACK_ERROR,
    COPYFILE2_CALLBACK_MAX,
} COPYFILE2_MESSAGE_TYPE;

typedef enum _COPYFILE2_MESSAGE_ACTION
{
    COPYFILE2_PROGRESS_CONTINUE = 0,
    COPYFILE2_PROGRESS_CANCEL,
    COPYFILE2_PROGRESS_STOP,
    COPYFILE2_PROGRESS_QUIET,
    COPYFILE2_PROGRESS_PAUSE,
} COPYFILE2_MESSAGE_ACTION;

typedef enum _COPYFILE2_COPY_PHASE
{
    COPYFILE2_PHASE_NONE = 0,
    COPYFILE2_PHASE_PREPARE_SOURCE,
    COPYFILE2_PHASE_PREPARE_DEST,
    COPYFILE2_PHASE_READ_SOURCE,
    COPYFILE2_PHASE_WRITE_DESTINATION,
    COPYFILE2_PHASE_SERVER_COPY,
    COPYFILE2_PHASE_NAMEGRAFT_COPY,
    COPYFILE2_PHASE_MAX,
} COPYFILE2_COPY_PHASE;

typedef struct COPYFILE2_MESSAGE
{
    COPYFILE2_MESSAGE_TYPE Type;
    DWORD                  dwPadding;
    union
    {
        struct
        {
            DWORD          dwStreamNumber;
            DWORD          dwReserved;
            HANDLE         hSourceFile;
            HANDLE         hDestinationFile;
            ULARGE_INTEGER uliChunkNumber;
            ULARGE_INTEGER uliChunkSize;
            ULARGE_INTEGER uliStreamSize;
            ULARGE_INTEGER uliTotalFileSize;
        } ChunkStarted;
        struct
        {
            DWORD          dwStreamNumber;
            DWORD          dwFlags;
            HANDLE         hSourceFile;
            HANDLE         hDestinationFile;
            ULARGE_INTEGER uliChunkNumber;
            ULARGE_INTEGER uliChunkSize;
            ULARGE_INTEGER uliStreamSize;
            ULARGE_INTEGER uliStreamBytesTransferred;
            ULARGE_INTEGER uliTotalFileSize;
            ULARGE_INTEGER uliTotalBytesTransferred;
        } ChunkFinished;
        struct
        {
            DWORD          dwStreamNumber;
            DWORD          dwReserved;
            HANDLE         hSourceFile;
            HANDLE         hDestinationFile;
            ULARGE_INTEGER uliStreamSize;
            ULARGE_INTEGER uliTotalFileSize;
        } StreamStarted;
        struct
        {
            DWORD          dwStreamNumber;
            DWORD          dwReserved;
            HANDLE         hSourceFile;
            HANDLE         hDestinationFile;
            ULARGE_INTEGER uliStreamSize;
            ULARGE_INTEGER uliStreamBytesTransferred;
            ULARGE_INTEGER uliTotalFileSize;
            ULARGE_INTEGER uliTotalBytesTransferred;
        } StreamFinished;
        struct
        {
            DWORD dwReserved;
        } PollContinue;
        struct
        {
            COPYFILE2_COPY_PHASE CopyPhase;
            DWORD                dwStreamNumber;
            HRESULT              hrFailure;
            DWORD                dwReserved;
            ULARGE_INTEGER       uliChunkNumber;
            ULARGE_INTEGER       uliStreamSize;
            ULARGE_INTEGER       uliStreamBytesTransferred;
            ULARGE_INTEGER       uliTotalFileSize;
            ULARGE_INTEGER       uliTotalBytesTransferred;
        } Error;
    } Info;
} COPYFILE2_MESSAGE;

typedef COPYFILE2_MESSAGE_ACTION (CALLBACK *PCOPYFILE2_PROGRESS_ROUTINE)(const COPYFILE2_MESSAGE*,PVOID);

typedef struct COPYFILE2_EXTENDED_PARAMETERS {
  DWORD                       dwSize;
  DWORD                       dwCopyFlags;
  BOOL                        *pfCancel;
  PCOPYFILE2_PROGRESS_ROUTINE pProgressRoutine;
  PVOID                       pvCallbackContext;
} COPYFILE2_EXTENDED_PARAMETERS, *PCOPYFILE2_EXTENDED_PARAMETERS;

typedef enum _OFFER_PRIORITY {
	VMOfferPriorityVeryLow			= 1,
	VMOfferPriorityLow,
	VMOfferPriorityBelowNormal,
	VMOfferPriorityNormal,
	VMOfferPriorityMaximum
} OFFER_PRIORITY;

/* Undocumented: layout of the locale data in the locale.nls file */

typedef struct
{
    UINT   sname;                  /* 000 LOCALE_SNAME */
    UINT   sopentypelanguagetag;   /* 004 LOCALE_SOPENTYPELANGUAGETAG */
    USHORT ilanguage;              /* 008 LOCALE_ILANGUAGE */
    USHORT unique_lcid;            /* 00a unique id if lcid == 0x1000 */
    USHORT idigits;                /* 00c LOCALE_IDIGITS */
    USHORT inegnumber;             /* 00e LOCALE_INEGNUMBER */
    USHORT icurrdigits;            /* 010 LOCALE_ICURRDIGITS*/
    USHORT icurrency;              /* 012 LOCALE_ICURRENCY */
    USHORT inegcurr;               /* 014 LOCALE_INEGCURR */
    USHORT ilzero;                 /* 016 LOCALE_ILZERO */
    USHORT inotneutral;            /* 018 LOCALE_INEUTRAL (inverted) */
    USHORT ifirstdayofweek;        /* 01a LOCALE_IFIRSTDAYOFWEEK (monday=0) */
    USHORT ifirstweekofyear;       /* 01c LOCALE_IFIRSTWEEKOFYEAR */
    USHORT icountry;               /* 01e LOCALE_ICOUNTRY */
    USHORT imeasure;               /* 020 LOCALE_IMEASURE */
    USHORT idigitsubstitution;     /* 022 LOCALE_IDIGITSUBSTITUTION */
    UINT   sgrouping;              /* 024 LOCALE_SGROUPING (as binary string) */
    UINT   smongrouping;           /* 028 LOCALE_SMONGROUPING  (as binary string) */
    UINT   slist;                  /* 02c LOCALE_SLIST */
    UINT   sdecimal;               /* 030 LOCALE_SDECIMAL */
    UINT   sthousand;              /* 034 LOCALE_STHOUSAND */
    UINT   scurrency;              /* 038 LOCALE_SCURRENCY */
    UINT   smondecimalsep;         /* 03c LOCALE_SMONDECIMALSEP */
    UINT   smonthousandsep;        /* 040 LOCALE_SMONTHOUSANDSEP */
    UINT   spositivesign;          /* 044 LOCALE_SPOSITIVESIGN */
    UINT   snegativesign;          /* 048 LOCALE_SNEGATIVESIGN */
    UINT   s1159;                  /* 04c LOCALE_S1159 */
    UINT   s2359;                  /* 050 LOCALE_S2359 */
    UINT   snativedigits;          /* 054 LOCALE_SNATIVEDIGITS (array of single digits) */
    UINT   stimeformat;            /* 058 LOCALE_STIMEFORMAT (array of formats) */
    UINT   sshortdate;             /* 05c LOCALE_SSHORTDATE (array of formats) */
    UINT   slongdate;              /* 060 LOCALE_SLONGDATE (array of formats) */
    UINT   syearmonth;             /* 064 LOCALE_SYEARMONTH (array of formats) */
    UINT   sduration;              /* 068 LOCALE_SDURATION (array of formats) */
    USHORT idefaultlanguage;       /* 06c LOCALE_IDEFAULTLANGUAGE */
    USHORT idefaultansicodepage;   /* 06e LOCALE_IDEFAULTANSICODEPAGE */
    USHORT idefaultcodepage;       /* 070 LOCALE_IDEFAULTCODEPAGE */
    USHORT idefaultmaccodepage;    /* 072 LOCALE_IDEFAULTMACCODEPAGE */
    USHORT idefaultebcdiccodepage; /* 074 LOCALE_IDEFAULTEBCDICCODEPAGE */
    USHORT old_geoid;              /* 076 LOCALE_IGEOID (older version?) */
    USHORT ipapersize;             /* 078 LOCALE_IPAPERSIZE */
    BYTE   islamic_cal[2];         /* 07a calendar id for islamic calendars (?) */
    UINT   scalendartype;          /* 07c string, first char is LOCALE_ICALENDARTYPE, next chars are LOCALE_IOPTIONALCALENDAR */
    UINT   sabbrevlangname;        /* 080 LOCALE_SABBREVLANGNAME */
    UINT   siso639langname;        /* 084 LOCALE_SISO639LANGNAME */
    UINT   senglanguage;           /* 088 LOCALE_SENGLANGUAGE */
    UINT   snativelangname;        /* 08c LOCALE_SNATIVELANGNAME */
    UINT   sengcountry;            /* 090 LOCALE_SENGCOUNTRY */
    UINT   snativectryname;        /* 094 LOCALE_SNATIVECTRYNAME */
    UINT   sabbrevctryname;        /* 098 LOCALE_SABBREVCTRYNAME */
    UINT   siso3166ctryname;       /* 09c LOCALE_SISO3166CTRYNAME */
    UINT   sintlsymbol;            /* 0a0 LOCALE_SINTLSYMBOL */
    UINT   sengcurrname;           /* 0a4 LOCALE_SENGCURRNAME */
    UINT   snativecurrname;        /* 0a8 LOCALE_SNATIVECURRNAME */
    UINT   fontsignature;          /* 0ac LOCALE_FONTSIGNATURE (binary string) */
    UINT   siso639langname2;       /* 0b0 LOCALE_SISO639LANGNAME2 */
    UINT   siso3166ctryname2;      /* 0b4 LOCALE_SISO3166CTRYNAME2 */
    UINT   sparent;                /* 0b8 LOCALE_SPARENT */
    UINT   sdayname;               /* 0bc LOCALE_SDAYNAME1 (array of days 1..7) */
    UINT   sabbrevdayname;         /* 0c0 LOCALE_SABBREVDAYNAME1  (array of days 1..7) */
    UINT   smonthname;             /* 0c4 LOCALE_SMONTHNAME1 (array of months 1..13) */
    UINT   sabbrevmonthname;       /* 0c8 LOCALE_SABBREVMONTHNAME1 (array of months 1..13) */
    UINT   sgenitivemonth;         /* 0cc equivalent of LOCALE_SMONTHNAME1 for genitive months */
    UINT   sabbrevgenitivemonth;   /* 0d0 equivalent of LOCALE_SABBREVMONTHNAME1 for genitive months */
    UINT   calnames;               /* 0d4 array of calendar names */
    UINT   customsorts;            /* 0d8 array of custom sort names */
    USHORT inegativepercent;       /* 0dc LOCALE_INEGATIVEPERCENT */
    USHORT ipositivepercent;       /* 0de LOCALE_IPOSITIVEPERCENT */
    USHORT unknown1;               /* 0e0 */
    USHORT ireadinglayout;         /* 0e2 LOCALE_IREADINGLAYOUT */
    USHORT unknown2[2];            /* 0e4 */
    UINT   unused1;                /* 0e8 unused? */
    UINT   sengdisplayname;        /* 0ec LOCALE_SENGLISHDISPLAYNAME */
    UINT   snativedisplayname;     /* 0f0 LOCALE_SNATIVEDISPLAYNAME */
    UINT   spercent;               /* 0f4 LOCALE_SPERCENT */
    UINT   snan;                   /* 0f8 LOCALE_SNAN */
    UINT   sposinfinity;           /* 0fc LOCALE_SPOSINFINITY */
    UINT   sneginfinity;           /* 100 LOCALE_SNEGINFINITY */
    UINT   unused2;                /* 104 unused? */
    UINT   serastring;             /* 108 CAL_SERASTRING */
    UINT   sabbreverastring;       /* 10c CAL_SABBREVERASTRING */
    UINT   unused3;                /* 110 unused? */
    UINT   sconsolefallbackname;   /* 114 LOCALE_SCONSOLEFALLBACKNAME */
    UINT   sshorttime;             /* 118 LOCALE_SSHORTTIME (array of formats) */
    UINT   sshortestdayname;       /* 11c LOCALE_SSHORTESTDAYNAME1 (array of days 1..7) */
    UINT   unused4;                /* 120 unused? */
    UINT   ssortlocale;            /* 124 LOCALE_SSORTLOCALE */
    UINT   skeyboardstoinstall;    /* 128 LOCALE_SKEYBOARDSTOINSTALL */
    UINT   sscripts;               /* 12c LOCALE_SSCRIPTS */
    UINT   srelativelongdate;      /* 130 LOCALE_SRELATIVELONGDATE */
    UINT   igeoid;                 /* 134 LOCALE_IGEOID */
    UINT   sshortestam;            /* 138 LOCALE_SSHORTESTAM */
    UINT   sshortestpm;            /* 13c LOCALE_SSHORTESTPM */
    UINT   smonthday;              /* 140 LOCALE_SMONTHDAY (array of formats) */
    UINT   keyboard_layout;        /* 144 keyboard layouts */
} NLS_LOCALE_DATA;

typedef struct
{
    UINT   offset;                 /* 00 offset to version, always 8? */
    UINT   unknown1;               /* 04 */
    UINT   version;                /* 08 file format version */
    UINT   magic;                  /* 0c magic 'NSDS' */
    UINT   unknown2[3];            /* 10 */
    USHORT header_size;            /* 1c size of this header (?) */
    USHORT nb_lcids;               /* 1e number of lcids in index */
    USHORT nb_locales;             /* 20 number of locales in array */
    USHORT locale_size;            /* 22 size of NLS_LOCALE_DATA structure */
    UINT   locales_offset;         /* 24 offset of locales array */
    USHORT nb_lcnames;             /* 28 number of lcnames in index */
    USHORT pad;                    /* 2a */
    UINT   lcids_offset;           /* 2c offset of lcids index */
    UINT   lcnames_offset;         /* 30 offset of lcnames index */
    UINT   unknown3;               /* 34 */
    USHORT nb_calendars;           /* 38 number of calendars in array */
    USHORT calendar_size;          /* 3a size of calendar structure */
    UINT   calendars_offset;       /* 3c offset of calendars array */
    UINT   strings_offset;         /* 40 offset of strings data */
    USHORT unknown4[4];            /* 44 */
} NLS_LOCALE_HEADER;

typedef struct
{
    USHORT name;                   /* 00 locale name */
    USHORT idx;                    /* 02 index in locales array */
    UINT   id;                     /* 04 lcid */
} NLS_LOCALE_LCNAME_INDEX;

typedef enum {
  PSS_CAPTURE_NONE = 0x00000000,
  PSS_CAPTURE_VA_CLONE = 0x00000001,
  PSS_CAPTURE_RESERVED_00000002 = 0x00000002,
  PSS_CAPTURE_HANDLES = 0x00000004,
  PSS_CAPTURE_HANDLE_NAME_INFORMATION = 0x00000008,
  PSS_CAPTURE_HANDLE_BASIC_INFORMATION = 0x00000010,
  PSS_CAPTURE_HANDLE_TYPE_SPECIFIC_INFORMATION = 0x00000020,
  PSS_CAPTURE_HANDLE_TRACE = 0x00000040,
  PSS_CAPTURE_THREADS = 0x00000080,
  PSS_CAPTURE_THREAD_CONTEXT = 0x00000100,
  PSS_CAPTURE_THREAD_CONTEXT_EXTENDED = 0x00000200,
  PSS_CAPTURE_RESERVED_00000400 = 0x00000400,
  PSS_CAPTURE_VA_SPACE = 0x00000800,
  PSS_CAPTURE_VA_SPACE_SECTION_INFORMATION = 0x00001000,
  PSS_CAPTURE_IPT_TRACE = 0x00002000,
  PSS_CAPTURE_RESERVED_00004000,
  PSS_CREATE_BREAKAWAY_OPTIONAL = 0x04000000,
  PSS_CREATE_BREAKAWAY = 0x08000000,
  PSS_CREATE_FORCE_BREAKAWAY = 0x10000000,
  PSS_CREATE_USE_VM_ALLOCATIONS = 0x20000000,
  PSS_CREATE_MEASURE_PERFORMANCE = 0x40000000,
  PSS_CREATE_RELEASE_SECTION = 0x80000000
} PSS_CAPTURE_FLAGS;

typedef enum {
  PSS_QUERY_PROCESS_INFORMATION = 0,
  PSS_QUERY_VA_CLONE_INFORMATION = 1,
  PSS_QUERY_AUXILIARY_PAGES_INFORMATION = 2,
  PSS_QUERY_VA_SPACE_INFORMATION = 3,
  PSS_QUERY_HANDLE_INFORMATION = 4,
  PSS_QUERY_THREAD_INFORMATION = 5,
  PSS_QUERY_HANDLE_TRACE_INFORMATION = 6,
  PSS_QUERY_PERFORMANCE_COUNTERS = 7
} PSS_QUERY_INFORMATION_CLASS;

typedef enum {
  PSS_PROCESS_FLAGS_NONE = 0x00000000,
  PSS_PROCESS_FLAGS_PROTECTED = 0x00000001,
  PSS_PROCESS_FLAGS_WOW64 = 0x00000002,
  PSS_PROCESS_FLAGS_RESERVED_03 = 0x00000004,
  PSS_PROCESS_FLAGS_RESERVED_04 = 0x00000008,
  PSS_PROCESS_FLAGS_FROZEN = 0x00000010
} PSS_PROCESS_FLAGS;

typedef struct _PACKAGE_INFO_REFERENCE {
  void *reserved;
} PACKAGE_INFO_REFERENCE;

typedef struct {
  DWORD             ExitStatus;
  void              *PebBaseAddress;
  ULONG_PTR         AffinityMask;
  LONG              BasePriority;
  DWORD             ProcessId;
  DWORD             ParentProcessId;
  PSS_PROCESS_FLAGS Flags;
  FILETIME          CreateTime;
  FILETIME          ExitTime;
  FILETIME          KernelTime;
  FILETIME          UserTime;
  DWORD             PriorityClass;
  ULONG_PTR         PeakVirtualSize;
  ULONG_PTR         VirtualSize;
  DWORD             PageFaultCount;
  ULONG_PTR         PeakWorkingSetSize;
  ULONG_PTR         WorkingSetSize;
  ULONG_PTR         QuotaPeakPagedPoolUsage;
  ULONG_PTR         QuotaPagedPoolUsage;
  ULONG_PTR         QuotaPeakNonPagedPoolUsage;
  ULONG_PTR         QuotaNonPagedPoolUsage;
  ULONG_PTR         PagefileUsage;
  ULONG_PTR         PeakPagefileUsage;
  ULONG_PTR         PrivateUsage;
  DWORD             ExecuteFlags;
  wchar_t           ImageFileName[MAX_PATH];
} PSS_PROCESS_INFORMATION;

typedef enum _CPU_SET_INFORMATION_TYPE { 
  CpuSetInformation
} CPU_SET_INFORMATION_TYPE, *PCPU_SET_INFORMATION_TYPE;

typedef struct _SYSTEM_CPU_SET_INFORMATION {
  DWORD                    Size;
  CPU_SET_INFORMATION_TYPE Type;
  union {
    struct {
      DWORD   Id;
      WORD    Group;
      BYTE    LogicalProcessorIndex;
      BYTE    CoreIndex;
      BYTE    LastLevelCacheIndex;
      BYTE    NumaNodeIndex;
      BYTE    EfficiencyClass;
      union {
        BYTE AllFlags;
        struct {
          BYTE Parked : 1;
          BYTE Allocated : 1;
          BYTE AllocatedToTargetProcess : 1;
          BYTE RealTime : 1;
          BYTE ReservedFlags : 4;
        } DUMMYSTRUCTNAME;
      } DUMMYUNIONNAME2;
      union {
        DWORD Reserved;
        BYTE  SchedulingClass;
      };
      DWORD64 AllocationTag;
    } CpuSet;
  } DUMMYUNIONNAME;
} SYSTEM_CPU_SET_INFORMATION, *PSYSTEM_CPU_SET_INFORMATION;

typedef struct _FILE_FULL_DIR_INFO {
  ULONG         NextEntryOffset;
  ULONG         FileIndex;
  LARGE_INTEGER CreationTime;
  LARGE_INTEGER LastAccessTime;
  LARGE_INTEGER LastWriteTime;
  LARGE_INTEGER ChangeTime;
  LARGE_INTEGER EndOfFile;
  LARGE_INTEGER AllocationSize;
  ULONG         FileAttributes;
  ULONG         FileNameLength;
  ULONG         EaSize;
  WCHAR         FileName[1];
} FILE_FULL_DIR_INFO, *PFILE_FULL_DIR_INFO;

typedef struct _FILE_ID_128 {
  BYTE Identifier[16];
} FILE_ID_128, *PFILE_ID_128;

typedef struct _FILE_ID_INFO {
  ULONGLONG   VolumeSerialNumber;
  FILE_ID_128 FileId;
} FILE_ID_INFO, *PFILE_ID_INFO;

typedef struct _FILE_STORAGE_INFO {
  ULONG LogicalBytesPerSector;
  ULONG PhysicalBytesPerSectorForAtomicity;
  ULONG PhysicalBytesPerSectorForPerformance;
  ULONG FileSystemEffectivePhysicalBytesPerSectorForAtomicity;
  ULONG Flags;
  ULONG ByteOffsetForSectorAlignment;
  ULONG ByteOffsetForPartitionAlignment;
} FILE_STORAGE_INFO, *PFILE_STORAGE_INFO;

typedef struct _FILE_ALIGNMENT_INFO {
  ULONG AlignmentRequirement;
} FILE_ALIGNMENT_INFO, *PFILE_ALIGNMENT_INFO;

typedef enum IORING_VERSION {
  IORING_VERSION_INVALID,
  IORING_VERSION_1,
  IORING_VERSION_2,
  IORING_VERSION_3,
  IORING_VERSION_4
} IORING_VERSION;

typedef enum IORING_FEATURE_FLAGS {
  IORING_FEATURE_FLAGS_NONE,
  IORING_FEATURE_UM_EMULATION,
  IORING_FEATURE_SET_COMPLETION_EVENT
} IORING_FEATURE_FLAGS;

typedef struct IORING_CAPABILITIES {
  IORING_VERSION       MaxVersion;
  UINT32               MaxSubmissionQueueSize;
  UINT32               MaxCompletionQueueSize;
  IORING_FEATURE_FLAGS FeatureFlags;
} IORING_CAPABILITIES;

#define MEM_EXTENDED_PARAMETER_TYPE_BITS 8

typedef enum MEM_EXTENDED_PARAMETER_TYPE {
    MemExtendedParameterInvalidType = 0,
    MemExtendedParameterAddressRequirements,
    MemExtendedParameterNumaNode,
    MemExtendedParameterPartitionHandle,
    MemExtendedParameterUserPhysicalHandle,
    MemExtendedParameterAttributeFlags,
    MemExtendedParameterImageMachine,
    MemExtendedParameterMax
} MEM_EXTENDED_PARAMETER_TYPE, *PMEM_EXTENDED_PARAMETER_TYPE;

typedef struct MEM_EXTENDED_PARAMETER {
  struct {
    DWORD64 Type : MEM_EXTENDED_PARAMETER_TYPE_BITS;
    DWORD64 Reserved : 64 - MEM_EXTENDED_PARAMETER_TYPE_BITS;
  } DUMMYSTRUCTNAME;
  union {
    DWORD64 ULong64;
    PVOID   Pointer;
    SIZE_T  Size;
    HANDLE  Handle;
    DWORD   ULong;
  } DUMMYUNIONNAME;
} MEM_EXTENDED_PARAMETER, *PMEM_EXTENDED_PARAMETER;

typedef HANDLE HPSS;

ULONG
WINAPI
BaseSetLastNTError(
    IN NTSTATUS Status
);

PUNICODE_STRING
WINAPI
Basep8BitStringToStaticUnicodeString(
	IN LPCSTR String
);

PWCHAR 
FilenameA2W(
	LPCSTR NameA, 
	BOOL alloc
);

LCID 
WINAPI 
LocaleNameToLCID( 
	LPCWSTR name, 
	DWORD flags 
);

INT 
WINAPI 
LCIDToLocaleName( 
	LCID lcid, 
	LPWSTR name, 
	INT count, 
	DWORD flags 
);

POBJECT_ATTRIBUTES 
WINAPI 
BaseFormatObjectAttributes( 	
	OUT POBJECT_ATTRIBUTES  	ObjectAttributes,
	IN PSECURITY_ATTRIBUTES SecurityAttributes  	OPTIONAL,
	IN PUNICODE_STRING  	ObjectName 
);

NTSTATUS 
NTAPI
NtPowerInformation(
  _In_      POWER_INFORMATION_LEVEL InformationLevel,
  _In_opt_  PVOID                   InputBuffer,
  _In_      ULONG                   InputBufferLength,
  _Out_opt_ PVOID                   OutputBuffer,
  _In_      ULONG                   OutputBufferLength
);

HANDLE 
WINAPI 
BaseGetNamedObjectDirectory(VOID);

NTSTATUS 
WINAPI 
RtlGetUserPreferredUILanguages(
	DWORD dwFlags, 
	BOOL verification,
	PULONG pulNumLanguages, 
	PZZWSTR pwszLanguagesBuffer, 
	PULONG pcchLanguagesBuffer
);

NTSTATUS 
WINAPI 
RtlGetThreadPreferredUILanguages(
	_In_       DWORD dwFlags,
	_Out_      PULONG pulNumLanguages,
	_Out_opt_  PZZWSTR pwszLanguagesBuffer,
	_Inout_    PULONG pcchLanguagesBuffer
);
	
NTSYSAPI 
NTSTATUS 
WINAPI 
NtClose(HANDLE);	

NTSTATUS 
NTAPI
RtlRunOnceExecuteOnce(
  _Inout_ PRTL_RUN_ONCE         RunOnce,
  _In_    PRTL_RUN_ONCE_INIT_FN InitFn,
  _Inout_ PVOID                 Parameter,
  _Out_   PVOID                 *Context
);

NTSTATUS 
NTAPI
RtlRunOnceBeginInitialize(
  _Inout_ PRTL_RUN_ONCE RunOnce,
  _In_    ULONG         Flags,
  _Out_   PVOID         *Context
);

BOOL
WINAPI
SleepConditionVariableCS(
	PCONDITION_VARIABLE ConditionVariable, 
	PCRITICAL_SECTION CriticalSection, 
	DWORD Timeout
);

void 
WINAPI 
TpSetTimer(
	TP_TIMER *, 
	LARGE_INTEGER *,
	LONG,
	LONG
);

BOOL 
WINAPI 
TpSetTimerEx( 
	TP_TIMER *timer, 
	LARGE_INTEGER *timeout, LONG period, LONG window_length );

void 
WINAPI 
TpSetWait(
	TP_WAIT *,
	HANDLE,
	LARGE_INTEGER *
);

BOOL 
WINAPI 
TpSetWaitEx( 
	TP_WAIT *wait, 
	HANDLE handle, 
	LARGE_INTEGER *timeout, 
	PVOID Reserved 
);

NTSTATUS 
WINAPI 
TpCallbackMayRunLong(
	TP_CALLBACK_INSTANCE *
);

NTSTATUS NTAPI LdrFindResourceDirectory_U 	( 	IN PVOID  	BaseAddress,
		IN PLDR_RESOURCE_INFO  	info,
		IN ULONG  	level,
		OUT PIMAGE_RESOURCE_DIRECTORY *  	addr 
	);
	
BOOL
WINAPI
CreateProcessInternalW(
    HANDLE hUserToken,
    LPCWSTR lpApplicationName,
    LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation,
    PHANDLE hRestrictedUserToken
    );	
	
BOOL
WINAPI
CreateProcessInternalA(
    HANDLE hUserToken,
    LPCSTR lpApplicationName,
    LPSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    LPVOID lpEnvironment,
    LPCSTR lpCurrentDirectory,
    LPSTARTUPINFOA lpStartupInfo,
    LPPROCESS_INFORMATION lpProcessInformation,
    PHANDLE hRestrictedUserToken
);	
	
BOOL
WINAPI
GetNumaNodeProcessorMask(
    UCHAR Node,
    PULONGLONG ProcessorMask
    );	
	
BOOL 
WINAPI 
GetNumaAvailableMemoryNode(
	UCHAR Node, 
	PULONGLONG AvailableBytes
);	

PLARGE_INTEGER 
WINAPI 
BaseFormatTimeOut(
	OUT PLARGE_INTEGER  	Timeout,
	IN DWORD  	dwMilliseconds 
);

INT 
WINAPI 
GetUserDefaultLocaleName(
	LPWSTR localename, 
	int buffersize
);

static inline BOOL set_ntstatus( NTSTATUS status )
{
    if (status) SetLastError( RtlNtStatusToDosError( status ));
    return !status;
}

/* FUNCTIONS ****************************************************************/
FORCEINLINE
BOOL
IsHKCRKey(_In_ HKEY hKey)
{
    return ((ULONG_PTR)hKey & 0x2) != 0;
}

FORCEINLINE
void
MakeHKCRKey(_Inout_ HKEY* hKey)
{
    *hKey = (HKEY)((ULONG_PTR)(*hKey) | 0x2);
}

LONG
WINAPI
CreateHKCRKey(
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey,
    _In_ DWORD Reserved,
    _In_opt_ LPWSTR lpClass,
    _In_ DWORD dwOptions,
    _In_ REGSAM samDesired,
    _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _Out_ PHKEY phkResult,
    _Out_opt_ LPDWORD lpdwDisposition);

LONG
WINAPI
DeleteHKCRKey(
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey,
    _In_ REGSAM RegSam,
    _In_ DWORD Reserved);	

LONG
WINAPI
EnumHKCRKey(
    _In_ HKEY hKey,
    _In_ DWORD dwIndex,
    _Out_ LPWSTR lpName,
    _Inout_ LPDWORD lpcbName,
    _Reserved_ LPDWORD lpReserved,
    _Out_opt_ LPWSTR lpClass,
    _Inout_opt_ LPDWORD lpcbClass,
    _Out_opt_ PFILETIME lpftLastWriteTime);

LONG
WINAPI
EnumHKCRValue(
    _In_ HKEY hKey,
    _In_ DWORD index,
    _Out_ LPWSTR value,
    _Inout_ PDWORD val_count,
    _Reserved_ PDWORD reserved,
    _Out_opt_ PDWORD type,
    _Out_opt_ LPBYTE data,
    _Inout_opt_ PDWORD count);
	
LONG
WINAPI
OpenHKCRKey(
    _In_ HKEY hKey,
    _In_ LPCWSTR lpSubKey,
    _In_ DWORD ulOptions,
    _In_ REGSAM samDesired,
    _In_ PHKEY phkResult);		

LONG
WINAPI
QueryHKCRValue(
    _In_ HKEY hKey,
    _In_ LPCWSTR Name,
    _In_ LPDWORD Reserved,
    _In_ LPDWORD Type,
    _In_ LPBYTE Data,
    _In_ LPDWORD Count);	
	
LONG
WINAPI
SetHKCRValue(
    _In_ HKEY hKey,
    _In_ LPCWSTR Name,
    _In_ DWORD Reserved,
    _In_ DWORD Type,
    _In_ CONST BYTE* Data,
    _In_ DWORD DataSize);	
	
INT 
WINAPI 
CompareStringOrdinal(
	const WCHAR *str1, 
	INT len1, 
	const WCHAR *str2, 
	INT len2, 
	BOOL ignore_case
);	

HRESULT 
WINAPI 
PathMatchSpecExW(
	const WCHAR *path, 
	const WCHAR *mask, 
	DWORD flags
);

BOOL
WINAPI
GetNumaProcessorNode(
	IN UCHAR Processor,
    OUT PUCHAR NodeNumber
);

DWORD 
WINAPI 
GetActiveProcessorCount(
  WORD GroupNumber
);

BOOL 
WINAPI 
GetNumaProcessorNodeEx(
  IN PPROCESSOR_NUMBER Processor,
  OUT PUSHORT           NodeNumber
);

DECLSPEC_NORETURN
VOID
WINAPI
BaseProcessStartup(
    _In_ PPROCESS_START_ROUTINE lpStartAddress);

#define ForEachListEntry(pListHead, pListEntry) for (((PLIST_ENTRY) (pListEntry)) = (pListHead)->Flink; ((PLIST_ENTRY) (pListEntry)) != (pListHead); ((PLIST_ENTRY) (pListEntry)) = ((PLIST_ENTRY) (pListEntry))->Flink)

typedef unsigned __int64 QWORD, *PQWORD, *LPQWORD, **PPQWORD;

struct pseudo_console
{
    HANDLE signal;
    HANDLE reference;
    HANDLE process;
};

VOID
WINAPI
BaseThreadStartupThunk(VOID);

DECLSPEC_NORETURN
VOID
WINAPI
BaseFiberStartup(VOID);

VOID
WINAPI
BaseProcessStartThunk(VOID);

extern BOOLEAN BaseRunningInServerProcess;