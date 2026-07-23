
#pragma once

/* PSDK/NDK Headers */
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#include <rtl.h>
#include <ndk/rtlfuncs.h>
#include <winsvc.h>
#include <evntprov.h>
#include <ntsecapi.h>
#include <evtlib.h>
#include <strsafe.h>
#include <perflib.h>
#include <ndk/cmfuncs.h>
#include <wine/config.h>
#include <accctrl.h>
#include <aclapi.h>
#include <winefs.h>
#include <wct.h>
#include <sddl.h>
#include <wincred.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <winuser.h>
#include <wine/debug.h>
#include <wine/unicode.h>
#include <wine/list.h>
#include <wine/heap.h>

#define REG_SECURE_CONNECTION  1

#define REG_NOTIFY_THREAD_AGNOSTIC   0x10000000

#define SECURITY_APP_PACKAGE_AUTHORITY {0,0,0,0,0,15}
#define SECURITY_APP_PACKAGE_BASE_RID           __MSABI_LONG(0x000000002)
#define SECURITY_BUILTIN_PACKAGE_ANY_PACKAGE    __MSABI_LONG(0x000000001)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

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

/* FUNCTIONS ****************************************************************/
FORCEINLINE
BOOL
IsHKCRKey(_In_ HKEY hKey)
{
    return ((ULONG_PTR)hKey & 0x2) != 0;
}

typedef PVOID IELF_HANDLE;

typedef ULONG64 TRACEHANDLE, *PTRACEHANDLE;

typedef ULONG64 CONTROLTRACE_ID;

typedef struct _ENABLE_TRACE_PARAMETERS
{
    ULONG                            Version;
    ULONG                            EnableProperty;
    ULONG                            ControlFlags;
    GUID                             SourceId;
    struct _EVENT_FILTER_DESCRIPTOR *EnableFilterDesc;
    ULONG                            FilterDescCount;
} ENABLE_TRACE_PARAMETERS, *PENABLE_TRACE_PARAMETERS;

typedef struct _RPC_UNICODE_STRING
{
    USHORT Length;
    USHORT MaximumLength;
    wchar_t *Buffer;
} RPC_UNICODE_STRING, *PRPC_UNICODE_STRING;

typedef struct _RPC_SID
{
    UCHAR Revision;
    UCHAR SubAuthorityCount;
    SID_IDENTIFIER_AUTHORITY IdentifierAuthority;
    DWORD SubAuthority[];
} RPC_SID, *PRPC_SID;

/* Defined in evtlib.h */
// #define LOGFILE_SIGNATURE   0x654c664c  // "LfLe"

typedef struct _LOGFILE
{
    EVTLOGFILE LogFile;
    HANDLE FileHandle;
    WCHAR *LogName;
    RTL_RESOURCE Lock;
    BOOL Permanent;
    LIST_ENTRY ListEntry;
} LOGFILE, *PLOGFILE;

typedef struct _EVENTSOURCE
{
    LIST_ENTRY EventSourceListEntry;
    PLOGFILE LogFile;
    WCHAR szName[1];
} EVENTSOURCE, *PEVENTSOURCE;

typedef struct _LOGHANDLE
{
    LIST_ENTRY LogHandleListEntry;
    PEVENTSOURCE EventSource;
    PLOGFILE LogFile;
    ULONG CurrentRecord;
    ULONG Flags;
    WCHAR szName[1];
} LOGHANDLE, *PLOGHANDLE;


static __inline void LogfFreeRecord(PEVENTLOGRECORD Record)
{
    RtlFreeHeap(GetProcessHeap(), 0, Record);
}

DWORD
WINAPI
BaseSetLastNTError(IN NTSTATUS Status);

/* EOF */

#define ARGUMENT_PRESENT(ArgumentPointer)((CHAR*)((ULONG_PTR)(ArgumentPointer)) != (CHAR*)NULL)

typedef VOID( CALLBACK * PFN_SC_NOTIFY_CALLBACK ) (
    IN PVOID pParameter 
);

typedef PVOID LSA_HANDLE, *PLSA_HANDLE;

typedef ULONG64 TRACEHANDLE,*PTRACEHANDLE;

typedef enum _EVENT_INFO_CLASS { 
  EventProviderBinaryTrackInfo    = 0,
  EventProviderSetTraits          = 1,
  EventProviderUseDescriptorType  = 2,
  MaxEventInfo                    = 3
} EVENT_INFO_CLASS;

/* set last error code from NT status and get the proper boolean return value */
 /* used for functions that are a simple wrapper around the corresponding ntdll API */
 static inline BOOL set_ntstatus( NTSTATUS status )
 {
     if (status) SetLastError( RtlNtStatusToDosError( status ));
     return !status;
}

typedef enum _TAG_INFO_LEVEL {
    TagInfoLevelNameFromTag = 1,
} TAG_INFO_LEVEL;

typedef enum _TAG_TYPE {
    TagTypeService = 1,
} TAG_TYPE;

typedef struct _TAG_INFO_NAME_FROM_TAG_IN_PARAMS {
    DWORD dwPid;
    DWORD dwTag;
} TAG_INFO_NAME_FROM_TAG_IN_PARAMS, *PTAG_INFO_NAME_FROM_TAG_IN_PARAMS;

typedef struct _TAG_INFO_NAME_FROM_TAG_OUT_PARAMS {
    TAG_TYPE TagType;
    LPWSTR pszName;
} TAG_INFO_NAME_FROM_TAG_OUT_PARAMS, *PTAG_INFO_NAME_FROM_TAG_OUT_PARAMS;

typedef struct _TAG_INFO_NAME_FROM_TAG
{
    TAG_INFO_NAME_FROM_TAG_IN_PARAMS InParams;
    TAG_INFO_NAME_FROM_TAG_OUT_PARAMS OutParams;
} TAG_INFO_NAME_FROM_TAG, *PTAG_INFO_NAME_FROM_TAG;

typedef struct _PERF_COUNTER_INFO {
  ULONG     CounterId;
  ULONG     Type;
  ULONGLONG Attrib;
  ULONG     Size;
  ULONG     DetailLevel;
  LONG      Scale;
  ULONG     Offset;
} PERF_COUNTER_INFO, *PPERF_COUNTER_INFO;

typedef enum
{
    ADS_RIGHT_DS_CREATE_CHILD         = 0x00000001,
    ADS_RIGHT_DS_DELETE_CHILD         = 0x00000002,
    ADS_RIGHT_ACTRL_DS_LIST           = 0x00000004,
    ADS_RIGHT_DS_SELF                 = 0x00000008,
    ADS_RIGHT_DS_READ_PROP            = 0x00000010,
    ADS_RIGHT_DS_WRITE_PROP           = 0x00000020,
    ADS_RIGHT_DS_DELETE_TREE          = 0x00000040,
    ADS_RIGHT_DS_LIST_OBJECT          = 0x00000080,
    ADS_RIGHT_DS_CONTROL_ACCESS       = 0x00000100,

    ADS_RIGHT_DELETE                  = 0x00010000,
    ADS_RIGHT_READ_CONTROL            = 0x00020000,
    ADS_RIGHT_WRITE_DAC               = 0x00040000,
    ADS_RIGHT_WRITE_OWNER             = 0x00080000,
    ADS_RIGHT_SYNCHRONIZE             = 0x00100000,
    ADS_RIGHT_ACCESS_SYSTEM_SECURITY  = 0x00200000,

    ADS_RIGHT_GENERIC_ALL             = 0x10000000,
    ADS_RIGHT_GENERIC_EXECUTE         = 0x20000000,
    ADS_RIGHT_GENERIC_WRITE           = 0x40000000,
    ADS_RIGHT_GENERIC_READ            = 0x80000000
} ADS_RIGHTS_ENUM;


typedef struct _PERF_COUNTER_IDENTIFIER {
    GUID CounterSetGuid;
    ULONG Status;
    ULONG Size;
    ULONG CounterId;
    ULONG InstanceId;
    ULONG Index;
    ULONG Reserved;
} PERF_COUNTER_IDENTIFIER, *PPERF_COUNTER_IDENTIFIER;

#define PERF_WILDCARD_COUNTER  0xFFFFFFFF
#define PERF_WILDCARD_INSTANCE L"*"

typedef struct _PERF_DATA_HEADER {
    ULONG dwTotalSize;
    ULONG dwNumCounters;
    LONGLONG PerfTimeStamp;
    LONGLONG PerfTime100NSec;
    LONGLONG PerfFreq;
    SYSTEMTIME SystemTime;
} PERF_DATA_HEADER, *PPERF_DATA_HEADER;

typedef enum _CRED_PROTECTION_TYPE {
  CredUnprotected,
  CredUserProtection,
  CredTrustedProtection,
  CredForSystemProtection
} CRED_PROTECTION_TYPE, *PCRED_PROTECTION_TYPE;

struct counterset_template
{
    PERF_COUNTERSET_INFO counterset;
    PERF_COUNTER_INFO counter[1];
};

struct counterset_instance
{
    struct list entry;
    struct counterset_template *template;
    PERF_COUNTERSET_INSTANCE instance;
};

struct perf_provider
{
    GUID guid;
    PERFLIBREQUEST callback;
    struct counterset_template **countersets;
    unsigned int counterset_count;

    struct list instance_list;
};

static struct perf_provider *perf_provider_from_handle(HANDLE prov)
{
    return (struct perf_provider *)prov;
}

typedef struct _PERF_INSTANCE_HEADER {
    ULONG Size;
    ULONG InstanceId;
} PERF_INSTANCE_HEADER, *PPERF_INSTANCE_HEADER;

typedef enum _PerfRegInfoType {
    PERF_REG_COUNTERSET_STRUCT = 1,
    PERF_REG_COUNTER_STRUCT,
    PERF_REG_COUNTERSET_NAME_STRING,
    PERF_REG_COUNTERSET_HELP_STRING,
    PERF_REG_COUNTER_NAME_STRINGS,
    PERF_REG_COUNTER_HELP_STRINGS,
    PERF_REG_PROVIDER_NAME,
    PERF_REG_PROVIDER_GUID,
    PERF_REG_COUNTERSET_ENGLISH_NAME,
    PERF_REG_COUNTER_ENGLISH_NAMES
} PerfRegInfoType;

typedef enum _TRACE_QUERY_INFO_CLASS
{
    TraceGuidQueryList,
    TraceGuidQueryInfo,
    TraceGuidQueryProcess,
    TraceStackTracingInfo,
    TraceSystemTraceEnableFlagsInfo,
    TraceSampledProfileIntervalInfo,
    TraceProfileSourceConfigInfo,
    TraceProfileSourceListInfo,
    TracePmcEventListInfo,
    TracePmcCounterListInfo,
    TraceSetDisallowList,
    TraceVersionInfo,
    TraceGroupQueryList,
    TraceGroupQueryInfo,
    TraceDisallowListQuery,
    TraceCompressionInfo,
    TracePeriodicCaptureStateListInfo,
    TracePeriodicCaptureStateInfo,
    TraceProviderBinaryTracking,
    TraceMaxLoggersQuery,
    MaxTraceSetInfoClass
} TRACE_QUERY_INFO_CLASS, TRACE_INFO_CLASS;

/* memory allocation functions */

static inline WCHAR *strdupAW( const char *src )
{
    WCHAR *dst = NULL;
    if (src)
    {
        DWORD len = MultiByteToWideChar( CP_ACP, 0, src, -1, NULL, 0 );
        if ((dst = heap_alloc( len * sizeof(WCHAR) ))) MultiByteToWideChar( CP_ACP, 0, src, -1, dst, len );
    }
    return dst;
}

DWORD SetUserFileEncryptionKey(
  PENCRYPTION_CERTIFICATE pEncryptionCertificate
);

LSTATUS Py_RegGetValueW(HKEY hKey, LPCWSTR pszSubKey, LPCWSTR pszValue,
    DWORD dwFlags, LPDWORD pdwType, PVOID pvData,
    LPDWORD pcbData);
	
BOOL WINAPI CreateWellKnownSidImpl( WELL_KNOWN_SID_TYPE type, PSID domain, PSID sid, DWORD *size );	

BOOL 
WINAPI
DECLSPEC_HOTPATCH 
ConvertStringSecurityDescriptorToSecurityDescriptorWImpl(
        const WCHAR *string, DWORD revision, PSECURITY_DESCRIPTOR *sd, ULONG *ret_size );
		
BOOL 
WINAPI 
DECLSPEC_HOTPATCH 
ConvertStringSidToSidWImpl( const WCHAR *string, PSID *sid );	

BOOL WINAPI IsWellKnownSidImpl( PSID sid, WELL_KNOWN_SID_TYPE type );

//Procedure to try get addresses
BOOL InitNativeProcs(void);

typedef BOOL (WINAPI *ELF_REPORT_EVENT_AND_SOURCE)(
						 HANDLE,
                         ULONG,
                         PUNICODE_STRING,
                         USHORT,
                         USHORT,
                         ULONG,
                         PSID,
                         PUNICODE_STRING,
                         USHORT,
                         ULONG,
                         PUNICODE_STRING*,
                         PVOID,
                         USHORT,
                         PULONG,
                         PULONG);

static DWORD (WINAPI *pAddMandatoryAce)(
    PACL, DWORD, DWORD, DWORD, PSID
) = NULL;

static BOOL (WINAPI *pCredFindBestCredentialA)(
    LPCSTR, DWORD, DWORD, PCREDENTIALA*
) = NULL;

static BOOL (WINAPI *pCredFindBestCredentialW)(
    LPCWSTR, DWORD, DWORD, PCREDENTIALW*
) = NULL;

static BOOL (WINAPI *pCredIsProtectedA)(
    LPCSTR, CRED_PROTECTION_TYPE*
) = NULL;

static BOOL (WINAPI *pCredIsProtectedW)(
    LPCWSTR, CRED_PROTECTION_TYPE*
) = NULL;

static BOOL (WINAPI *pCredProtectA)(
    BOOL, LPSTR, DWORD, LPSTR, DWORD*, CRED_PROTECTION_TYPE*
) = NULL;

static BOOL (WINAPI *pCredProtectW)(
    BOOL, LPWSTR, DWORD, LPWSTR, DWORD*, CRED_PROTECTION_TYPE*
) = NULL;

static BOOL (WINAPI *pCredUnprotectA)(
    BOOL, LPSTR, DWORD, LPSTR, DWORD*
) = NULL;

static BOOL (WINAPI *pCredUnprotectW)(
    BOOL, LPWSTR, DWORD, LPWSTR, DWORD*
) = NULL;

static VOID (WINAPI *pCloseThreadWaitChainSession)(PVOID) = NULL;

static ULONG (WINAPI *pEventRegister)(
    LPCGUID, PVOID, PVOID, PVOID
) = NULL;

static ULONG (WINAPI *pEventUnregister)(
    ULONGLONG
) = NULL;

static ULONG (WINAPI *pEventWrite)(
    ULONGLONG, PVOID, ULONG, PVOID
) = NULL;

static ULONG (WINAPI *pEventWriteTransfer)(
    ULONGLONG, LPCGUID, LPCGUID, PVOID, ULONG, PVOID
) = NULL;

static ULONG (WINAPI *pEventWriteString)(
    ULONGLONG, UCHAR, ULONGLONG, LPCWSTR
) = NULL;

static ULONG (WINAPI *pEventActivityIdControl)(
    ULONG, LPGUID
) = NULL;

static BOOLEAN (WINAPI *pEventEnabled)(
    ULONGLONG, PVOID
) = NULL;

static BOOLEAN (WINAPI *pEventProviderEnabled)(
    ULONGLONG, UCHAR, ULONGLONG
) = NULL;

static ULONG (WINAPI *pEventAccessControl)(
    LPCGUID, ULONG, PSID, ULONG, BOOLEAN 
) = NULL;

static ULONG (WINAPI *pEventWriteEx)(
    REGHANDLE, PCEVENT_DESCRIPTOR, ULONG64, ULONG, LPCGUID,
    LPCGUID, ULONG, PEVENT_DATA_DESCRIPTOR
) = NULL;

static ULONG (WINAPI *pEventWriteStartScenario)(
    ULONGLONG, PVOID, ULONG, PVOID
) = NULL;

static ULONG (WINAPI *pEventWriteEndScenario)(
    ULONGLONG, PVOID, ULONG, PVOID
) = NULL;

static ULONG (WINAPI *pEnableTraceEx)(
    LPCGUID, LPCGUID, TRACEHANDLE, ULONG, UCHAR,
    ULONGLONG, ULONGLONG, ULONG, PEVENT_FILTER_DESCRIPTOR
) = NULL;

static ULONG (WINAPI *pEnableTraceEx2)(
    CONTROLTRACE_ID, LPCGUID, ULONG, UCHAR,
    ULONGLONG, ULONGLONG, ULONG, PENABLE_TRACE_PARAMETERS
) = NULL;

static HWCT (WINAPI *pOpenThreadWaitChainSession)(
    DWORD, PWAITCHAINCALLBACK
) = NULL;

static BOOL (WINAPI *pGetThreadWaitChain)(
    PVOID, DWORD, DWORD, DWORD,
    PDWORD, PVOID, PDWORD
) = NULL;

static BOOL (WINAPI *pInitiateShutdownA)(
    LPCSTR, LPCSTR, DWORD, DWORD, DWORD
) = NULL;

static BOOL (WINAPI *pInitiateShutdownW)(
    LPCWSTR, LPCWSTR, DWORD, DWORD, DWORD
) = NULL;

static DWORD (WINAPI *pNotifyServiceStatusChangeA)(
    SC_HANDLE, DWORD, PVOID
) = NULL;

static DWORD (WINAPI *pNotifyServiceStatusChangeW)(
    SC_HANDLE, DWORD, PVOID
) = NULL;

static ULONG (WINAPI *pPerfOpenQueryHandle)(LPCWSTR, HANDLE*) = NULL;
static ULONG (WINAPI *pPerfCloseQueryHandle)(HANDLE) = NULL;

static ULONG (WINAPI *pPerfAddCounters)(HANDLE, PVOID, ULONG) = NULL;
static ULONG (WINAPI *pPerfDeleteCounters)(HANDLE, PVOID, ULONG) = NULL;

static PPERF_COUNTERSET_INSTANCE (WINAPI *pPerfCreateInstance)(HANDLE, PVOID, LPCWSTR, ULONG) = NULL;
static ULONG (WINAPI *pPerfDeleteInstance)(HANDLE, PVOID) = NULL;

static ULONG (WINAPI *pPerfEnumerateCounterSet)(LPCWSTR, PVOID, ULONG, PULONG) = NULL;
static ULONG (WINAPI *pPerfEnumerateCounterSetInstances)(
    LPCWSTR, PVOID, PERF_INSTANCE_HEADER, ULONG, PULONG
) = NULL;

static ULONG (WINAPI *pPerfQueryCounterData)(HANDLE, PVOID, ULONG, PULONG) = NULL;
static ULONG (WINAPI *pPerfQueryCounterInfo)(HANDLE, PVOID, ULONG, PULONG) = NULL;
static PPERF_COUNTERSET_INSTANCE (WINAPI *pPerfQueryInstance)(HANDLE, PVOID, LPCWSTR, ULONG) = NULL;

static ULONG (WINAPI *pPerfQueryCounterSetRegistrationInfo)(
    LPCWSTR, LPCGUID, PerfRegInfoType, ULONG, LPBYTE, ULONG, ULONG*
) = NULL;

static ULONG (WINAPI *pPerfSetCounterRefValue)(HANDLE, PVOID, ULONG, PVOID) = NULL;
static ULONG (WINAPI *pPerfSetCounterSetInfo)(HANDLE, PVOID, ULONG) = NULL;

static ULONG (WINAPI *pPerfSetULongCounterValue)(HANDLE, PVOID, ULONG, ULONG) = NULL;
static ULONG (WINAPI *pPerfSetULongLongCounterValue)(HANDLE, PVOID, ULONG, ULONGLONG) = NULL;

static ULONG (WINAPI *pPerfIncrementULongCounterValue)(HANDLE, PVOID, ULONG, ULONG) = NULL;
static ULONG (WINAPI *pPerfIncrementULongLongCounterValue)(HANDLE, PVOID, ULONG, ULONGLONG) = NULL;

static ULONG (WINAPI *pPerfDecrementULongCounterValue)(HANDLE, PVOID, ULONG, ULONG) = NULL;
static ULONG (WINAPI *pPerfDecrementULongLongCounterValue)(HANDLE, PVOID, ULONG, ULONGLONG) = NULL;

static ULONG (WINAPI *pPerfStartProvider)(LPGUID, PVOID, HANDLE*) = NULL;
static ULONG (WINAPI *pPerfStartProviderEx)(LPGUID, PERF_PROVIDER_CONTEXT*, HANDLE*) = NULL;
static ULONG (WINAPI *pPerfStopProvider)(HANDLE) = NULL;
static VOID (WINAPI *pProcessIdleTasksW)(VOID) = NULL;
static BOOL (WINAPI *pQuerySecurityAccessMask)(SECURITY_INFORMATION, LPDWORD) = NULL;
static LSTATUS (WINAPI *pRegCopyTreeA)(HKEY, LPCSTR, HKEY) = NULL;
static LSTATUS (WINAPI *pRegCopyTreeW)(HKEY, LPCWSTR, HKEY) = NULL;

static LSTATUS (WINAPI *pRegDeleteTreeA)(HKEY, LPCSTR) = NULL;
static LSTATUS (WINAPI *pRegDeleteTreeW)(HKEY, LPCWSTR) = NULL;

static LSTATUS (WINAPI *pRegDeleteKeyValueA)(HKEY, LPCSTR, LPCSTR) = NULL;
static LSTATUS (WINAPI *pRegDeleteKeyValueW)(HKEY, LPCWSTR, LPCWSTR) = NULL;

static LSTATUS (WINAPI *pRegSetKeyValueA)(
    HKEY, LPCSTR, LPCSTR, DWORD, LPCVOID, DWORD
) = NULL;

static LSTATUS (WINAPI *pRegSetKeyValueW)(
    HKEY, LPCWSTR, LPCWSTR, DWORD, LPCVOID, DWORD
) = NULL;

static LSTATUS (WINAPI *pRegRenameKey)(HKEY, LPCWSTR, LPCWSTR) = NULL;

static LSTATUS (WINAPI *pRegLoadAppKeyA)(
    LPCSTR, PHKEY, REGSAM, DWORD, DWORD
) = NULL;

static LSTATUS (WINAPI *pRegLoadAppKeyW)(
    LPCWSTR, PHKEY, REGSAM, DWORD, DWORD
) = NULL;

static LSTATUS (WINAPI *pRegLoadMUIStringA)(
    HKEY, LPCSTR, LPSTR, DWORD, LPDWORD, DWORD, LPCSTR
) = NULL;

static LSTATUS (WINAPI *pRegLoadMUIStringW)(
    HKEY, LPCWSTR, LPWSTR, DWORD, LPDWORD, DWORD, LPCWSTR
) = NULL;

static LSTATUS (WINAPI *pRegCreateKeyTransactedA)(
    HKEY, LPCSTR, DWORD, LPSTR, DWORD, REGSAM,
    LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD,
    HANDLE, PVOID
) = NULL;

static LSTATUS (WINAPI *pRegCreateKeyTransactedW)(
    HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM,
    LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD,
    HANDLE, PVOID
) = NULL;

static LSTATUS (WINAPI *pRegOpenKeyTransactedA)(
    HKEY, LPCSTR, DWORD, REGSAM, PHKEY,
    HANDLE, PVOID
) = NULL;

static LSTATUS (WINAPI *pRegOpenKeyTransactedW)(
    HKEY, LPCWSTR, DWORD, REGSAM, PHKEY,
    HANDLE, PVOID
) = NULL;

static LSTATUS (WINAPI *pRegDeleteKeyTransactedA)(
    HKEY, LPCSTR, DWORD, DWORD, HANDLE, PVOID
) = NULL;

static LSTATUS (WINAPI *pRegDeleteKeyTransactedW)(
    HKEY, LPCWSTR, DWORD, DWORD, HANDLE, PVOID
) = NULL;
static BOOL (WINAPI *pSetSecurityAccessMask)(SECURITY_INFORMATION, LPDWORD) = NULL;
static DWORD (WINAPI *pSetUserFileEncryptionKeyEx)(PBYTE, DWORD, DWORD, PVOID) = NULL;
static DWORD (WINAPI *pTreeSetNamedSecurityInfoW)(LPCWSTR, SE_OBJECT_TYPE, SECURITY_INFORMATION,PSID, PSID, PACL, PACL, DWORD) = NULL;