/*++

Copyright (c) 2018 Shorthorn Project

Module Name:

    sqm.c

Abstract:

    Refence for Ntext functions, types and structures

Author:

    Skulltrail 06-March-2018

Revision History:

--*/
 
/* We're a core NT DLL, we don't import syscalls */
#define _NTSYSTEM_
#define _NTDLLBUILD_ 

#include <assert.h>
#include <rtl.h>
#include <ntsecapi.h>
#include <cmfuncs.h>
#include "localelcid.h"
#include <rtlavl.h>
#include <rtltypes.h>
#include <winnls.h>
#include <stdarg.h>
#include <limits.h>
#include <list.h>
#include <wmistr.h>
#include <lpcfuncs.h>
#include <baseapi.h>
#include <tlhelp32.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* Use intrinsics for x86 and x64 */
#if defined(_M_IX86) || defined(_M_AMD64)
#define InterlockedCompareExchange _InterlockedCompareExchange
#define InterlockedIncrement _InterlockedIncrement
#define InterlockedDecrement _InterlockedDecrement
#define InterlockedExchangeAdd _InterlockedExchangeAdd
#define InterlockedExchange _InterlockedExchange
#define InterlockedBitTestAndSet _interlockedbittestandset
#define InterlockedBitTestAndSet64 _interlockedbittestandset64
#endif
#define IS_PATH_SEPARATOR(x) (((x)==L'\\')||((x)==L'/'))

#define MAX_HW_COUNTERS 16
typedef __int64 timeout_t;
#define TIMEOUT_INFINITE (((timeout_t)0x7fffffff) << 32 | 0xffffffff)

#define RTL_ATOM_MAXIMUM_INTEGER_ATOM   (RTL_ATOM)0xC000
#define RTL_ATOM_INVALID_ATOM           (RTL_ATOM)0x0000
#define RTL_ATOM_TABLE_DEFAULT_NUMBER_OF_BUCKETS 37
#define RTL_ATOM_MAXIMUM_NAME_LENGTH    255
#define RTL_ATOM_PINNED 0x01

#if defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
static const UINT_PTR page_size = 0x1000;
#else
extern UINT_PTR page_size DECLSPEC_HIDDEN;
#endif

/* Definitions *****************************************/

typedef PVOID* PALPC_HANDLE;

typedef PVOID ALPC_HANDLE;

typedef DWORD WINAPI RTL_RUN_ONCE_INIT_FN(PRTL_RUN_ONCE, PVOID, PVOID*);

typedef RTL_RUN_ONCE_INIT_FN *PRTL_RUN_ONCE_INIT_FN;

static RTL_CRITICAL_SECTION loader_section;

/* IDN defines. - From wine's winnls.h*/
#define IDN_ALLOW_UNASSIGNED        0x1
#define IDN_USE_STD3_ASCII_RULES    0x2

/* SHA1 Helper Macros */

#define rol(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))
/* FIXME: This definition of DWORD2BE is little endian specific! */
#define DWORD2BE(x) (((x) >> 24) & 0xff) | (((x) >> 8) & 0xff00) | (((x) << 8) & 0xff0000) | (((x) << 24) & 0xff000000);
/* FIXME: This definition of blk0 is little endian specific! */
#define blk0(i) (Block[i] = (rol(Block[i],24)&0xFF00FF00)|(rol(Block[i],8)&0x00FF00FF))
#define blk1(i) (Block[i&15] = rol(Block[(i+13)&15]^Block[(i+8)&15]^Block[(i+2)&15]^Block[i&15],1))
#define f1(x,y,z) (z^(x&(y^z)))
#define f2(x,y,z) (x^y^z)
#define f3(x,y,z) ((x&y)|(z&(x|y)))
#define f4(x,y,z) (x^y^z)
/* (R0+R1), R2, R3, R4 are the different operations used in SHA1 */
#define R0(v,w,x,y,z,i) z+=f1(w,x,y)+blk0(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R1(v,w,x,y,z,i) z+=f1(w,x,y)+blk1(i)+0x5A827999+rol(v,5);w=rol(w,30);
#define R2(v,w,x,y,z,i) z+=f2(w,x,y)+blk1(i)+0x6ED9EBA1+rol(v,5);w=rol(w,30);
#define R3(v,w,x,y,z,i) z+=f3(w,x,y)+blk1(i)+0x8F1BBCDC+rol(v,5);w=rol(w,30);
#define R4(v,w,x,y,z,i) z+=f4(w,x,y)+blk1(i)+0xCA62C1D6+rol(v,5);w=rol(w,30);

// this is Windows 2000 code
#ifndef ARGUMENT_PRESENT
#define ARGUMENT_PRESENT(x) (((x) != NULL))
#endif

/* This is Windows 2000 code*/
#define 	RtlProcessHeap()   (NtCurrentPeb()->ProcessHeap)

#define STATUS_INCOMPATIBLE_DRIVER_BLOCKED 0xC0000424

#define ACTIVATION_CONTEXT_SECTION_APPLICATION_SETTINGS          10
#define ACTIVATION_CONTEXT_SECTION_COMPATIBILITY_INFO            11

#define RtlGetCurrentThreadId()  (HandleToUlong(NtCurrentTeb()->ClientId.UniqueThread))

#define LOAD_LIBRARY_REQUIRE_SIGNED_TARGET  0x00000080

/* flags for SetSearchPathMode */
#define BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE  0x00001
#define BASE_SEARCH_PATH_DISABLE_SAFE_SEARCHMODE 0x10000
#define BASE_SEARCH_PATH_PERMANENT               0x08000
#define BASE_SEARCH_PATH_INVALID_FLAGS         (~0x18001)

//Because we want don't touch in THREADINFOCLASS on Reactos 
#define ThreadGroupInformation 30

#define THREAD_CREATE_FLAGS_CREATE_SUSPENDED        0x00000001
#define THREAD_CREATE_FLAGS_SKIP_THREAD_ATTACH      0x00000002
#define THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER      0x00000004
#define THREAD_CREATE_FLAGS_HAS_SECURITY_DESCRIPTOR 0x00000010
#define THREAD_CREATE_FLAGS_ACCESS_CHECK_IN_TARGET  0x00000020
#define THREAD_CREATE_FLAGS_INITIAL_THREAD          0x00000080

//
// Define the bit masks exclusive to type CmResourceTypeMemoryLarge.
//
#define CM_RESOURCE_MEMORY_LARGE                0x0E00
#define CM_RESOURCE_MEMORY_LARGE_40             0x0200
#define CM_RESOURCE_MEMORY_LARGE_48             0x0400
#define CM_RESOURCE_MEMORY_LARGE_64             0x0800

/* Enumarations ****************************************/

/* Structs *********************************************/

typedef struct _LOCALE{
	LPCWSTR description;
	LPCWSTR cultureName;
	LCID lcidHex;
	LCID lcidDec;	
}LOCALE,*PLOCALE;

typedef struct {
   ULONG Unknown[6];
   ULONG State[5];
   ULONG Count[2];
   UCHAR Buffer[64];
} SHA_CTX, *PSHA_CTX;

/* Internal header for table entries */
typedef struct _TABLE_ENTRY_HEADER
{
	RTL_SPLAY_LINKS SplayLinks;
    RTL_BALANCED_LINKS BalancedLinks;
	LIST_ENTRY ListEntry;
    LONGLONG UserData;
} TABLE_ENTRY_HEADER, *PTABLE_ENTRY_HEADER;

typedef enum _RTL_AVL_BALANCE_FACTOR
{
    RtlUnbalancedAvlTree = -2,
    RtlLeftHeavyAvlTree,
    RtlBalancedAvlTree,
    RtlRightHeavyAvlTree,
} RTL_AVL_BALANCE_FACTOR;

typedef struct _RTLP_SRWLOCK_SHARED_WAKE
{
    LONG Wake;
    volatile struct _RTLP_SRWLOCK_SHARED_WAKE *Next;
} volatile RTLP_SRWLOCK_SHARED_WAKE, *PRTLP_SRWLOCK_SHARED_WAKE;

typedef struct _TP_IO TP_IO, *PTP_IO;

typedef ULONG LOGICAL;

typedef DWORD TP_VERSION, *PTP_VERSION;

typedef DWORD TP_WAIT_RESULT;

typedef PVOID* PALPC_HANDLE;

typedef PVOID ALPC_HANDLE;

typedef HANDLE 	DLL_DIRECTORY_COOKIE;

typedef struct _TP_TIMER TP_TIMER, *PTP_TIMER;

typedef struct _TP_CLEANUP_GROUP TP_CLEANUP_GROUP, *PTP_CLEANUP_GROUP;

typedef struct _TP_CALLBACK_INSTANCE TP_CALLBACK_INSTANCE, *PTP_CALLBACK_INSTANCE;

typedef VOID (*PTP_TIMER_CALLBACK)(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_TIMER Timer);

typedef struct _TP_WAIT TP_WAIT, *PTP_WAIT;

typedef TP_CALLBACK_ENVIRON_V1 TP_CALLBACK_ENVIRON, *PTP_CALLBACK_ENVIRON;

typedef VOID (*PTP_WAIT_CALLBACK)(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WAIT Wait, TP_WAIT_RESULT WaitResult);

typedef VOID (WINAPI *PBAD_MEMORY_CALLBACK_ROUTINE)(VOID);

typedef BOOLEAN (*PSECURE_MEMORY_CACHE_CALLBACK)(
    _In_  PVOID Addr,
    _In_  SIZE_T Range
);


typedef VOID (CALLBACK *PTP_WIN32_IO_CALLBACK)(PTP_CALLBACK_INSTANCE,PVOID,PVOID,ULONG,ULONG_PTR,PTP_IO);

typedef enum _TRANSACTION_INFORMATION_CLASS { 
  TransactionBasicInformation               = 0,
  TransactionPropertiesInformation,
  TransactionEnlistmentInformation,
  TransactionSuperiorEnlistmentInformation
} TRANSACTION_INFORMATION_CLASS;

typedef struct _ALPC_PORT_ATTRIBUTES
{
  ULONG Flags;
  SECURITY_QUALITY_OF_SERVICE SecurityQos;
  SIZE_T MaxMessageLength;
  SIZE_T MemoryBandwidth;
  SIZE_T MaxPoolUsage;
  SIZE_T MaxSectionSize;
  SIZE_T MaxViewSize;
  SIZE_T MaxTotalSectionSize;
  ULONG DupObjectTypes;
 #ifdef _M_X64
  ULONG Reserved;
 #endif
} ALPC_PORT_ATTRIBUTES, *PALPC_PORT_ATTRIBUTES;

typedef struct _ALPC_DATA_VIEW_ATTR
{
  ULONG Flags;
  ALPC_HANDLE SectionHandle;
  PVOID ViewBase; // must be zero on input
  SIZE_T ViewSize;
} ALPC_DATA_VIEW_ATTR, *PALPC_DATA_VIEW_ATTR;

typedef enum _ALPC_PORT_INFORMATION_CLASS
{
  AlpcBasicInformation, // q: out ALPC_BASIC_INFORMATION
  AlpcPortInformation, // s: in ALPC_PORT_ATTRIBUTES
  AlpcAssociateCompletionPortInformation, // s: in ALPC_PORT_ASSOCIATE_COMPLETION_PORT
  AlpcConnectedSIDInformation, // q: in SID
  AlpcServerInformation, // q: inout ALPC_SERVER_INFORMATION
  AlpcMessageZoneInformation, // s: in ALPC_PORT_MESSAGE_ZONE_INFORMATION
  AlpcRegisterCompletionListInformation, // s: in ALPC_PORT_COMPLETION_LIST_INFORMATION
  AlpcUnregisterCompletionListInformation, // s: VOID
  AlpcAdjustCompletionListConcurrencyCountInformation, // s: in ULONG
  AlpcRegisterCallbackInformation, // kernel-mode only
  AlpcCompletionListRundownInformation, // s: VOID
  MaxAlpcPortInfoClass
} ALPC_PORT_INFORMATION_CLASS;

typedef struct _ALPC_SECURITY_ATTR
{
  ULONG Flags;
  PSECURITY_QUALITY_OF_SERVICE SecurityQos;
  ALPC_HANDLE ContextHandle; // dbg
  ULONG Reserved1;
  ULONG Reserved2;
} ALPC_SECURITY_ATTR, *PALPC_SECURITY_ATTR;

typedef struct _ALPC_MESSAGE_ATTRIBUTES
{
  ULONG AllocatedAttributes;
  ULONG ValidAttributes;
} ALPC_MESSAGE_ATTRIBUTES, *PALPC_MESSAGE_ATTRIBUTES;

typedef struct _ALPC_CONTEXT_ATTR
{
  PVOID PortContext;
  PVOID MessageContext;
  ULONG Sequence;
  ULONG MessageId;
  ULONG CallbackId;
} ALPC_CONTEXT_ATTR, *PALPC_CONTEXT_ATTR;

typedef enum _WORKERFACTORYINFOCLASS
{
	WorkerFactoryTimeout, // LARGE_INTEGER
	WorkerFactoryRetryTimeout, // LARGE_INTEGER
	WorkerFactoryIdleTimeout, // s: LARGE_INTEGER
	WorkerFactoryBindingCount, // s: ULONG
	WorkerFactoryThreadMinimum, // s: ULONG
	WorkerFactoryThreadMaximum, // s: ULONG
	WorkerFactoryPaused, // ULONG or BOOLEAN
	WorkerFactoryBasicInformation, // q: WORKER_FACTORY_BASIC_INFORMATION
	WorkerFactoryAdjustThreadGoal,
	WorkerFactoryCallbackType,
	WorkerFactoryStackInformation, // 10
	WorkerFactoryThreadBasePriority, // s: ULONG
	WorkerFactoryTimeoutWaiters, // s: ULONG, since THRESHOLD
	WorkerFactoryFlags, // s: ULONG
	WorkerFactoryThreadSoftMaximum, // s: ULONG
	WorkerFactoryThreadCpuSets, // since REDSTONE5
	MaxWorkerFactoryInfoClass
} WORKERFACTORYINFOCLASS, * PWORKERFACTORYINFOCLASS;

// //
// // This structure specifies an offset (from the beginning of CONTEXT_EX
// // structure) and size of a single chunk of an extended context structure.
// //
// // N.B. Offset may be negative.
// //

// typedef struct _CONTEXT_CHUNK {
    // LONG Offset;
    // DWORD Length;
// } CONTEXT_CHUNK, *PCONTEXT_CHUNK;

// //
// // CONTEXT_EX structure is an extension to CONTEXT structure. It defines
// // a context record as a set of disjoint variable-sized buffers (chunks)
// // each containing a portion of processor state. Currently there are only
// // two buffers (chunks) are defined:
// //
// //   - Legacy, that stores traditional CONTEXT structure;
// //   - XState, that stores XSAVE save area buffer starting from
// //     XSAVE_AREA_HEADER, i.e. without the first 512 bytes.
// //
// // There a few assumptions exists that simplify conversion of PCONTEXT
// // pointer to PCONTEXT_EX pointer.
// //
// // 1. APIs that work with PCONTEXT pointers assume that CONTEXT_EX is
// //    stored right after the CONTEXT structure. It is also assumed that
// //    CONTEXT_EX is present if and only if corresponding CONTEXT_XXX
// //    flags are set in CONTEXT.ContextFlags.
// //
// // 2. CONTEXT_EX.Legacy is always present if CONTEXT_EX structure is
// //    present. All other chunks are optional.
// //
// // 3. CONTEXT.ContextFlags unambigiously define which chunks are
// //    present. I.e. if CONTEXT_XSTATE is set CONTEXT_EX.XState is valid.
// //

// typedef struct _CONTEXT_EX {

    // //
    // // The total length of the structure starting from the chunk with
    // // the smallest offset. N.B. that the offset may be negative.
    // //

    // CONTEXT_CHUNK All;

    // //
    // // Wrapper for the traditional CONTEXT structure. N.B. the size of
    // // the chunk may be less than sizeof(CONTEXT) is some cases (when
    // // CONTEXT_EXTENDED_REGISTERS is not set on x86 for instance).
    // //

    // CONTEXT_CHUNK Legacy;

    // //
    // // CONTEXT_XSTATE: Extended processor state chunk. The state is
    // // stored in the same format XSAVE operation strores it with
    // // exception of the first 512 bytes, i.e. staring from
    // // XSAVE_AREA_HEADER. The lower two bits corresponding FP and
    // // SSE state must be zero.
    // //

    // CONTEXT_CHUNK XState;

// } CONTEXT_EX, *PCONTEXT_EX;

//Windows 2000 or for new ntdll
typedef struct _SCOPETABLE_ENTRY *PSCOPETABLE_ENTRY;

typedef struct _EH3_EXCEPTION_REGISTRATION
{
  struct _EH3_EXCEPTION_REGISTRATION *Next;
  PVOID ExceptionHandler;
  PSCOPETABLE_ENTRY ScopeTable;
  DWORD TryLevel;
}EH3_EXCEPTION_REGISTRATION, *PEH3_EXCEPTION_REGISTRATION;

typedef struct _CPPEH_RECORD
{
  DWORD old_esp;
  EXCEPTION_POINTERS *exc_ptr;
  PEH3_EXCEPTION_REGISTRATION registration;
}CPPEH_RECORD;

typedef struct _RTLP_SRWLOCK_WAITBLOCK
{
    /* SharedCount is the number of shared acquirers. */
    LONG SharedCount;

    /* Last points to the last wait block in the chain. The value
       is only valid when read from the first wait block. */
    volatile struct _RTLP_SRWLOCK_WAITBLOCK *Last;

    /* Next points to the next wait block in the chain. */
    volatile struct _RTLP_SRWLOCK_WAITBLOCK *Next;

    union
    {
        /* Wake is only valid for exclusive wait blocks */
        LONG Wake;
        /* The wake chain is only valid for shared wait blocks */
        struct
        {
            PRTLP_SRWLOCK_SHARED_WAKE SharedWakeChain;
            PRTLP_SRWLOCK_SHARED_WAKE LastSharedWake;
        };
    };

    BOOLEAN Exclusive;
} volatile RTLP_SRWLOCK_WAITBLOCK, *PRTLP_SRWLOCK_WAITBLOCK;

/* Functions Prototypes for public*/
void 
WINAPI 
A_SHAInit(PSHA_CTX Context);

VOID 
WINAPI 
A_SHAUpdate(PSHA_CTX Context,
	const unsigned char *  	Buffer,
	UINT  	BufferSize 
);  

VOID 
WINAPI 
A_SHAFinal(
	PSHA_CTX Context, 
	PULONG Result) ;
	
PIMAGE_SECTION_HEADER
RtlSectionTableFromVirtualAddress (
    IN PIMAGE_NT_HEADERS NtHeaders,
    IN PVOID Base,
    IN ULONG Address
    );
	
PVOID
NTAPI
RtlAddressInSectionTable (
    IN PIMAGE_NT_HEADERS NtHeaders,
    IN PVOID Base,
    IN ULONG Address
    );
	
NTSTATUS 
WINAPI 
RtlLCIDToCultureName(
	IN LCID lcid, 
	OUT PUNICODE_STRING locale
);

void 
NTAPI 
RtlReportErrorPropagation(
	LPSTR a1, 
	int a2, 
	int a3, 
	int a4
);

void 
NTAPI 
RtlReportErrorOrigination(
	LPSTR a1, 
	WCHAR string, 
	int a3, 
	NTSTATUS status
);

//
// Define a RESID
//

typedef PVOID RESID;

//
// Define a RESOURCE_HANDLE
//

typedef HANDLE   RESOURCE_HANDLE;

typedef enum CLUSTER_RESOURCE_STATE { 
  ClusterResourceStateUnknown    = -1,
  ClusterResourceInherited       = 0,
  ClusterResourceInitializing    = 1,
  ClusterResourceOnline          = 2,
  ClusterResourceOffline         = 3,
  ClusterResourceFailed          = 4,
  ClusterResourcePending         = 128, // 0x80
  ClusterResourceOnlinePending   = 129, // 0x81
  ClusterResourceOfflinePending  = 130 // 0x82
} CLUSTER_RESOURCE_STATE, _CLUSTER_RESOURCE_STATE;

//
// Define Resource DLL callback method for logging events.
//
typedef enum LOG_LEVEL {
    LOG_INFORMATION,
    LOG_WARNING,
    LOG_ERROR,
    LOG_SEVERE
} LOG_LEVEL, *PLOG_LEVEL;

typedef struct _RESOURCE_STATUS {
  CLUSTER_RESOURCE_STATE ResourceState;
  DWORD                  CheckPoint;
  DWORD                  WaitHint;
  HANDLE                 EventHandle;
} RESOURCE_STATUS, *PRESOURCE_STATUS;

typedef struct _LOCALE_LCID{
	LPCWSTR localeName;
	LCID lcid;
}LOCALE_LCID;

typedef DWORD (WINAPI *PSET_RESOURCE_STATUS_ROUTINE)(
    _In_ RESOURCE_HANDLE  ResourceHandle,
    _In_ PRESOURCE_STATUS ResourceStatus
);

//
// Define Resource DLL callback method for notifying that a quorum
// resource DLL failed to hold the quorum resource.
//
typedef
VOID
(_stdcall *PQUORUM_RESOURCE_LOST) (
    IN RESOURCE_HANDLE Resource
    );

typedef VOID (WINAPI *PLOG_EVENT_ROUTINE)(
    _In_ RESOURCE_HANDLE ResourceHandle,
    _In_ LOG_LEVEL       LogLevel,
    _In_ LPCWSTR         FormatString,
                         ...
);

typedef
DWORD
(_stdcall *PRESOURCE_TYPE_CONTROL_ROUTINE) (
    IN LPCWSTR ResourceTypeName,
    IN DWORD ControlCode,
    IN PVOID InBuffer,
    IN DWORD InBufferSize,
    OUT PVOID OutBuffer,
    IN DWORD OutBufferSize,
    OUT LPDWORD BytesReturned
    );
	
typedef
DWORD
(_stdcall *PRESOURCE_CONTROL_ROUTINE) (
    IN RESID Resource,
    IN DWORD ControlCode,
    IN PVOID InBuffer,
    IN DWORD InBufferSize,
    OUT PVOID OutBuffer,
    IN DWORD OutBufferSize,
    OUT LPDWORD BytesReturned
    );
	
typedef
DWORD
(_stdcall *PRELEASE_ROUTINE) (
    IN RESID Resource
    );	
	
typedef
DWORD
(_stdcall *PARBITRATE_ROUTINE) (
    IN RESID Resource,
    IN PQUORUM_RESOURCE_LOST LostQuorumResource
    );	
	
typedef
BOOL
(_stdcall *PIS_ALIVE_ROUTINE) (
    IN RESID Resource
    );	
	
typedef
BOOL
(_stdcall *PLOOKS_ALIVE_ROUTINE) (
    IN RESID Resource
    );	
	
typedef
VOID
(_stdcall *PTERMINATE_ROUTINE) (
    IN RESID Resource
    );	

typedef
DWORD
(_stdcall *POFFLINE_ROUTINE) (
    IN RESID Resource
    );	
	
typedef
DWORD
(_stdcall *PONLINE_ROUTINE) (
    IN RESID Resource,
    IN OUT LPHANDLE EventHandle
    );	
	
typedef
VOID
(_stdcall *PCLOSE_ROUTINE) (
    IN RESID Resource
    );	
	
typedef
RESID
(_stdcall *POPEN_ROUTINE) (
    IN LPCWSTR ResourceName,
    IN HKEY ResourceKey,
    IN RESOURCE_HANDLE ResourceHandle
    );	
	
//***************************************************************
//
// Define the Function Table Format
//
//***************************************************************

//
// Version 1 Resource DLL Interface definition
//
typedef struct CLRES_V1_FUNCTIONS {
    POPEN_ROUTINE Open;
    PCLOSE_ROUTINE Close;
    PONLINE_ROUTINE Online;
    POFFLINE_ROUTINE Offline;
    PTERMINATE_ROUTINE Terminate;
    PLOOKS_ALIVE_ROUTINE LooksAlive;
    PIS_ALIVE_ROUTINE IsAlive;
    PARBITRATE_ROUTINE Arbitrate;
    PRELEASE_ROUTINE Release;
    PRESOURCE_CONTROL_ROUTINE ResourceControl;
    PRESOURCE_TYPE_CONTROL_ROUTINE ResourceTypeControl;
} CLRES_V1_FUNCTIONS, *PCLRES_V1_FUNCTIONS;

typedef struct CLRES_FUNCTION_TABLE {
    DWORD   TableSize;
    DWORD   Version;
    union {
        CLRES_V1_FUNCTIONS V1Functions;
    };
} CLRES_FUNCTION_TABLE, *PCLRES_FUNCTION_TABLE;

typedef DWORD (WINAPI *PSTARTUP_ROUTINE)(
    _In_  LPCWSTR                      ResourceType,
    _In_  DWORD                        MinVersionSupported,
    _In_  DWORD                        MaxVersionSupported,
    _In_  PSET_RESOURCE_STATUS_ROUTINE SetResourceStatus,
    _In_  PLOG_EVENT_ROUTINE           LogEvent,
    _Out_ CLRES_FUNCTION_TABLE         *FunctionTable
);

typedef DWORD (CALLBACK *PRTL_WORK_ITEM_ROUTINE)(LPVOID); /* FIXME: not the right name */
typedef void (NTAPI *RTL_WAITORTIMERCALLBACKFUNC)(PVOID,BOOLEAN); /* FIXME: not the right name */
typedef VOID (CALLBACK *PRTL_OVERLAPPED_COMPLETION_ROUTINE)(DWORD,DWORD,LPVOID);

// typedef struct _RTL_CONDITION_VARIABLE {
    // PVOID Ptr;
// } RTL_CONDITION_VARIABLE, *PRTL_CONDITION_VARIABLE;

NTSTATUS
NTAPI
RtlSleepConditionVariableCS(IN OUT PRTL_CONDITION_VARIABLE ConditionVariable,
                            IN OUT PRTL_CRITICAL_SECTION CriticalSection,
                            IN const LARGE_INTEGER * TimeOut OPTIONAL); 
 
void 
WINAPI
RtlWakeConditionVariable(
  RTL_CONDITION_VARIABLE* variable
);

void 
WINAPI 
RtlInitializeConditionVariable(
	PRTL_CONDITION_VARIABLE
);

void
WINAPI 
RtlWakeAllConditionVariable(
	PRTL_CONDITION_VARIABLE
);

#define WOW64_TLS_FILESYSREDIR      8

typedef struct _WMI_LOGGER_INFORMATION {
    WNODE_HEADER Wnode;       // Had to do this since wmium.h comes later
//
// data provider by caller
    ULONG BufferSize;                   // buffer size for logging (in kbytes)
    ULONG MinimumBuffers;               // minimum to preallocate
    ULONG MaximumBuffers;               // maximum buffers allowed
    ULONG MaximumFileSize;              // maximum logfile size (in MBytes)
    ULONG LogFileMode;                  // sequential, circular
    ULONG FlushTimer;                   // buffer flush timer, in seconds
    ULONG EnableFlags;                  // trace enable flags
    LONG  AgeLimit;                     // aging decay time, in minutes
    ULONG Wow;                          // TRUE if the logger started under WOW64
    union {
        HANDLE  LogFileHandle;          // handle to logfile
        ULONG64 LogFileHandle64;
    };

// data returned to caller
// end_wmikm
    union {
// begin_wmikm
        ULONG NumberOfBuffers;          // no of buffers in use
// end_wmikm
        ULONG InstanceCount;            // Number of Provider Instances
    };
    union {
// begin_wmikm
        ULONG FreeBuffers;              // no of buffers free
// end_wmikm
        ULONG InstanceId;               // Current Provider's Id for UmLogger
    };
    union {
// begin_wmikm
        ULONG EventsLost;               // event records lost
// end_wmikm
        ULONG NumberOfProcessors;       // Passed on to UmLogger
    };
// begin_wmikm
    ULONG BuffersWritten;               // no of buffers written to file
    ULONG LogBuffersLost;               // no of logfile write failures
    ULONG RealTimeBuffersLost;          // no of rt delivery failures
    union {
        HANDLE  LoggerThreadId;         // thread id of Logger
        ULONG64 LoggerThreadId64;       // thread is of Logger
    };
    union {
        UNICODE_STRING LogFileName;     // used only in WIN64
        UNICODE_STRING64 LogFileName64; // Logfile name: only in WIN32
    };

// mandatory data provided by caller
    union {
        UNICODE_STRING LoggerName;      // Logger instance name in WIN64
        UNICODE_STRING64 LoggerName64;  // Logger Instance name in WIN32
    };

// private
    union {
        PVOID   Checksum;
        ULONG64 Checksum64;
    };
    union {
        PVOID   LoggerExtension;
        ULONG64 LoggerExtension64;
    };
} WMI_LOGGER_INFORMATION, *PWMI_LOGGER_INFORMATION;

// typedef struct _TIME_DYNAMIC_ZONE_INFORMATION
// {
     // LONG Bias;
     // WCHAR StandardName[32];
     // SYSTEMTIME StandardDate;
     // LONG StandardBias;
     // WCHAR DaylightName[32];
     // SYSTEMTIME DaylightDate;
     // LONG DaylightBias;
     // WCHAR TimeZoneKeyName[128];
     // BOOLEAN DynamicDaylightTimeDisabled;
// } DYNAMIC_TIME_ZONE_INFORMATION, *PDYNAMIC_TIME_ZONE_INFORMATION;

typedef DYNAMIC_TIME_ZONE_INFORMATION RTL_DYNAMIC_TIME_ZONE_INFORMATION;

/*
 * RTL_SYSTEM_TIME and RTL_TIME_ZONE_INFORMATION are the same as
 * the SYSTEMTIME and TIME_ZONE_INFORMATION structures defined
 * in winbase.h, however we need to define them separately so
 * winternl.h doesn't depend on winbase.h.  They are used by
 * RtlQueryTimeZoneInformation and RtlSetTimeZoneInformation.
 * The names are guessed; if anybody knows the real names, let me know.
 */
typedef struct _RTL_SYSTEM_TIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} RTL_SYSTEM_TIME, *PRTL_SYSTEM_TIME;

typedef struct _ASSEMBLY_STORAGE_MAP_ENTRY
{
    ULONG Flags;
    UNICODE_STRING DosPath;
    HANDLE Handle;
} ASSEMBLY_STORAGE_MAP_ENTRY, *PASSEMBLY_STORAGE_MAP_ENTRY;

typedef struct _ASSEMBLY_STORAGE_MAP
{
    ULONG Flags;
    ULONG AssemblyCount;
    PASSEMBLY_STORAGE_MAP_ENTRY *AssemblyArray;
} ASSEMBLY_STORAGE_MAP, *PASSEMBLY_STORAGE_MAP;

 struct file_info
{
     ULONG               type;
     WCHAR              *info;
};

struct assembly_version
{
    USHORT              major;
    USHORT              minor;
    USHORT              build;
    USHORT              revision;
};

struct assembly_identity
{
    WCHAR                *name;
    WCHAR                *arch;
    WCHAR                *public_key;
    WCHAR                *language;
    WCHAR                *type;
    struct assembly_version version;
    BOOL                  optional;
    BOOL                  delayed;
};

struct progids
{
    WCHAR        **progids;
    unsigned int   num;
    unsigned int   allocated;
};

struct entity
{
    DWORD kind;
    union
    {
        struct
        {
            WCHAR *tlbid;
            WCHAR *helpdir;
            WORD   flags;
            WORD   major;
            WORD   minor;
	} typelib;
        struct
        {
            WCHAR *clsid;
            WCHAR *tlbid;
            WCHAR *progid;
            WCHAR *name;    /* clrClass: class name */
            WCHAR *version; /* clrClass: CLR runtime version */
            DWORD  model;
            DWORD  miscstatus;
            DWORD  miscstatuscontent;
            DWORD  miscstatusthumbnail;
            DWORD  miscstatusicon;
            DWORD  miscstatusdocprint;
            struct progids progids;
	} comclass;
	struct {
            WCHAR *iid;
            WCHAR *base;
            WCHAR *tlib;
            WCHAR *name;
            WCHAR *ps32; /* only stored for 'comInterfaceExternalProxyStub' */
            DWORD  mask;
            ULONG  nummethods;
	} ifaceps;
        struct
        {
            WCHAR *name;
            BOOL   versioned;
        } class;
        struct
        {
            WCHAR *name;
            WCHAR *clsid;
            WCHAR *version;
        } clrsurrogate;
        struct
        {
            WCHAR *name;
            WCHAR *value;
            WCHAR *ns;
        } settings;
        struct
        {
            WCHAR *name;
            DWORD  threading_model;
        } activatable_class;
    } u;
};

struct entity_array
{
    struct entity        *base;
    unsigned int          num;
    unsigned int          allocated;
};

/* return type of RtlDetermineDosPathNameType_U (FIXME: not the correct names) */
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

struct assembly
{
    enum assembly_type             type;
    struct assembly_identity       id;
    struct file_info               manifest;
    WCHAR                         *directory;
    BOOL                           no_inherit;
    struct dll_redirect           *dlls;
    unsigned int                   num_dlls;
    unsigned int                   allocated_dlls;
    struct entity_array            entities;
    COMPATIBILITY_CONTEXT_ELEMENT *compat_contexts;
    ULONG                          num_compat_contexts;
    ACTCTX_REQUESTED_RUN_LEVEL     run_level;
    ULONG                          ui_access;
};

typedef struct _ACTIVATION_CONTEXT
{
	ULONG magic;
    LONG RefCount;
    ULONG Flags;
    LIST_ENTRY Links;
    PACTIVATION_CONTEXT_DATA ActivationContextData;
    PVOID NotificationRoutine;
    PVOID NotificationContext;
    ULONG SentNotifications[8];
    ULONG DisabledNotifications[8];
    ASSEMBLY_STORAGE_MAP StorageMap;
    PASSEMBLY_STORAGE_MAP_ENTRY InlineStorageMapEntries;
    ULONG StackTraceIndex;
    PVOID StackTraces[4][4];
    struct file_info config;
    struct file_info appdir;
    struct assembly *assemblies;
    unsigned int num_assemblies;
    unsigned int allocated_assemblies;
    /* section data */
    DWORD sections;
    struct strsection_header *wndclass_section;
    struct strsection_header *dllredirect_section;
    struct strsection_header *progid_section;
    struct guidsection_header *tlib_section;
    struct guidsection_header *comserver_section;
    struct guidsection_header *ifaceps_section;
    struct guidsection_header *clrsurrogate_section;
} ACTIVATION_CONTEXT, *PIACTIVATION_CONTEXT;

typedef struct _TOKEN_APPCONTAINER_INFORMATION {
  	PSID TokenAppContainer;
} TOKEN_APPCONTAINER_INFORMATION, *PTOKEN_APPCONTAINER_INFORMATION;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA
{
    ULONG Flags;
    const UNICODE_STRING *FullDllName;
    const UNICODE_STRING *BaseDllName;
    void *DllBase;
    ULONG SizeOfImage;
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA
{
    LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
    LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

typedef void (CALLBACK *PLDR_DLL_NOTIFICATION_FUNCTION)(ULONG, LDR_DLL_NOTIFICATION_DATA*, void*);

typedef struct _TP_CALLBACK_ENVIRON_V3
{
    TP_VERSION Version;
    PTP_POOL Pool;
    PTP_CLEANUP_GROUP CleanupGroup;
    PTP_CLEANUP_GROUP_CANCEL_CALLBACK CleanupGroupCancelCallback;
    PVOID RaceDll;
    struct _ACTIVATION_CONTEXT *ActivationContext;
    PTP_SIMPLE_CALLBACK FinalizationCallback;
    union
    {
        DWORD Flags;
        struct
        {
            DWORD LongFunction:1;
            DWORD Persistent:1;
            DWORD Private:30;
        } s;
    } u;
    TP_CALLBACK_PRIORITY CallbackPriority;
    DWORD Size;
} TP_CALLBACK_ENVIRON_V3;

typedef struct _PROCESS_STACK_ALLOCATION_INFORMATION
{
    SIZE_T ReserveSize;
    SIZE_T ZeroBits;
    PVOID  StackBase;
} PROCESS_STACK_ALLOCATION_INFORMATION, *PPROCESS_STACK_ALLOCATION_INFORMATION;

typedef enum _EVENT_INFO_CLASS {
  EventProviderBinaryTrackInfo,
  EventProviderSetReserved1,
  EventProviderSetTraits,
  EventProviderUseDescriptorType,
  MaxEventInfo
} EVENT_INFO_CLASS;

RTL_CRITICAL_SECTION time_tz_section;

RTL_CRITICAL_SECTION localeCritSection;

HANDLE GlobalKeyedEventHandle;

NTSTATUS
NTAPI
NtAccessCheckByTypeAndAuditAlarm(
    _In_ PUNICODE_STRING SubsystemName,
    _In_opt_ PVOID HandleId,
    _In_ PUNICODE_STRING ObjectTypeName,
    _In_ PUNICODE_STRING ObjectName,
    _In_ PSECURITY_DESCRIPTOR SecurityDescriptor,
    _In_opt_ PSID PrincipalSelfSid,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ AUDIT_EVENT_TYPE AuditType,
    _In_ ULONG Flags,
    _In_reads_opt_(ObjectTypeLength) POBJECT_TYPE_LIST ObjectTypeList,
    _In_ ULONG ObjectTypeLength,
    _In_ PGENERIC_MAPPING GenericMapping,
    _In_ BOOLEAN ObjectCreation,
    _Out_ PACCESS_MASK GrantedAccess,
    _Out_ PNTSTATUS AccessStatus,
    _Out_ PBOOLEAN GenerateOnClose);
	
DWORD 
NTAPI 
RtlRunOnceExecuteOnce( 
	RTL_RUN_ONCE *once, 
	PRTL_RUN_ONCE_INIT_FN func,
    void *param, void **context 
);

typedef struct _SYNCITEM
{
	struct _SYNCITEM* back;	//上个插入的节点
	struct _SYNCITEM* first;	//第一个插入的节点
	struct _SYNCITEM* next;	//下个插入的节点
	DWORD count;		//共享计数
	DWORD attr;			//节点属性
	RTL_SRWLOCK* lock;
	
    // 唤醒此任务的线程Id
    volatile size_t uWakeupThreadId;	
} SYNCITEM;

typedef size_t SYNCSTATUS;

//M-mask F-flag SYNC-common
#define SYNC_Exclusive	1	//当前是独占锁在等待，而不是共享锁
#define SYNC_Spinning	2	//当前线程即将休眠，而不是休眠中或唤醒后
#define SYNC_SharedLock	4	//条件变量使用共享锁等待，而不是独占锁

#if defined(_WIN64)
 #define SRWM_ITEM  0xFFFFFFFFFFFFFFF0ULL
#else
 #define SRWM_ITEM  0xFFFFFFF0U
#endif
#define SRWM_FLAG  (~SRWM_ITEM)
#define SRWM_COUNT	SRWM_ITEM

#define SRWF_Free	0	//空闲
#define SRWF_Hold   0x1 /* lock has been currently held */
#define SRWF_Wait   0x2 /* a thread is waiting on a lock */
#define SRWF_Link   0x4 /* lock is in the process of being released. */
#define SRWF_Many   0x8 /* multiple threads are waiting on lock */

#define CVM_COUNT	0x00000007
#define CVM_FLAG	0x0000000F
#define CVM_ITEM	0xFFFFFFF0

#define CVF_Full	7	//唤醒申请已满，全部唤醒
#define CVF_Link	8	//修改链表的操作进行中

#define SRW_COUNT_BIT	4
#define SRW_HOLD_BIT	0
#define SYNC_SPIN_BIT	1	//从0开始数

void NTAPI RtlBackoff(USHORT* const pCount);
void NTAPI RtlpInitSRWLock(PEB* pPEB);
void NTAPI RtlpInitConditionVariable(PEB* pPeb);
BOOL NTAPI RtlDllShutdownInProgress(VOID);

HANDLE __fastcall GetGlobalKeyedEventHandle();

extern BOOL IsUniprocessorMachine;

#define CONDITION_VARIABLE_LOCKMODE_SHARED 0x1

//读写锁参考了https://blog.csdn.net/yichigo/article/details/36898561
#define YY_SRWLOCK_Locked_BIT           0
#define YY_SRWLOCK_Waiting_BIT          1
#define YY_SRWLOCK_Waking_BIT           2
#define YY_SRWLOCK_MultipleShared_BIT   3
//已经有人获得这个锁
#define YY_SRWLOCK_Locked               0x00000001ul
//有人正在等待锁
#define YY_SRWLOCK_Waiting              0x00000002ul
//有人正在唤醒锁
#define YY_SRWLOCK_Waking               0x00000004ul
//
#define YY_SRWLOCK_MultipleShared       0x00000008ul

#define YY_SRWLOCK_MASK    (0xF)
#define YY_SRWLOCK_BITS    4
#define YY_SRWLOCK_GET_BLOCK(SRWLock) ((YY_SRWLOCK_WAIT_BLOCK*)(SRWLock & (~YY_SRWLOCK_MASK)))

//SRWLock自旋次数
//取自Win7
struct _YY_RTL_SRWLOCK
{
	union
	{
		struct
		{
			ULONG_PTR Locked : 1;       
			ULONG_PTR Waiting : 1;           
			ULONG_PTR Waking : 1;            
			ULONG_PTR MultipleShared : 1;
#ifdef _WIN64
			ULONG_PTR Shared : 60;
#else
			ULONG_PTR Shared : 28;
#endif
		};
		ULONG_PTR Value;
		VOID* Ptr;
	};
};

typedef struct __declspec(align(16)) _YY_SRWLOCK_WAIT_BLOCK
{
	struct _YY_SRWLOCK_WAIT_BLOCK* back;
	struct _YY_SRWLOCK_WAIT_BLOCK* notify;
	struct _YY_SRWLOCK_WAIT_BLOCK* next;
	volatile size_t         shareCount;
	volatile size_t         flag;
} YY_SRWLOCK_WAIT_BLOCK;

//正在优化锁
#define YY_CV_OPTIMIZE_LOCK 0x00000008ul
#define YY_CV_MASK (0x0000000F)
#define YY_CV_GET_BLOCK(CV) ((YY_CV_WAIT_BLOCK*)(CV & (~YY_CV_MASK)))

typedef struct __declspec(align(16)) _YY_CV_WAIT_BLOCK
{
	struct _YY_CV_WAIT_BLOCK* back;
	struct _YY_CV_WAIT_BLOCK* notify;
	struct _YY_CV_WAIT_BLOCK* next;
	volatile size_t    shareCount;
	volatile size_t    flag;
	volatile PRTL_SRWLOCK  SRWLock;
	
    // 以下成员YY-Thunks特有

    // 唤醒此任务的线程Id
    volatile size_t uWakeupThreadId;	
} YY_CV_WAIT_BLOCK;

#if defined(_M_AMD64)
#define CONTEXT_i386      0x00010000
#define CONTEXT_i486      0x00010000
#endif

#define CONTEXT_I386_CONTROL   (CONTEXT_i386 | 0x0001) /* SS:SP, CS:IP, FLAGS, BP */
#define CONTEXT_I386_INTEGER   (CONTEXT_i386 | 0x0002) /* AX, BX, CX, DX, SI, DI */
#define CONTEXT_I386_SEGMENTS  (CONTEXT_i386 | 0x0004) /* DS, ES, FS, GS */
#define CONTEXT_I386_FLOATING_POINT  (CONTEXT_i386 | 0x0008) /* 387 state */
#define CONTEXT_I386_DEBUG_REGISTERS (CONTEXT_i386 | 0x0010) /* DB 0-3,6,7 */
#define CONTEXT_I386_EXTENDED_REGISTERS (CONTEXT_i386 | 0x0020)
#define CONTEXT_I386_XSTATE             (CONTEXT_i386 | 0x0040)
#define CONTEXT_I386_FULL (CONTEXT_I386_CONTROL | CONTEXT_I386_INTEGER | CONTEXT_I386_SEGMENTS)
#define CONTEXT_I386_ALL (CONTEXT_I386_FULL | CONTEXT_I386_FLOATING_POINT | CONTEXT_I386_DEBUG_REGISTERS | CONTEXT_I386_EXTENDED_REGISTERS)

#ifdef _M_IX86
#define CONTEXT_AMD64   0x00100000
#endif

#define CONTEXT_AMD64_CONTROL   (CONTEXT_AMD64 | 0x0001)
#define CONTEXT_AMD64_INTEGER   (CONTEXT_AMD64 | 0x0002)
#define CONTEXT_AMD64_SEGMENTS  (CONTEXT_AMD64 | 0x0004)
#define CONTEXT_AMD64_FLOATING_POINT  (CONTEXT_AMD64 | 0x0008)
#define CONTEXT_AMD64_DEBUG_REGISTERS (CONTEXT_AMD64 | 0x0010)
#define CONTEXT_AMD64_XSTATE          (CONTEXT_AMD64 | 0x0040)
#define CONTEXT_AMD64_FULL (CONTEXT_AMD64_CONTROL | CONTEXT_AMD64_INTEGER | CONTEXT_AMD64_FLOATING_POINT)
#define CONTEXT_AMD64_ALL (CONTEXT_AMD64_CONTROL | CONTEXT_AMD64_INTEGER | CONTEXT_AMD64_SEGMENTS | CONTEXT_AMD64_FLOATING_POINT | CONTEXT_AMD64_DEBUG_REGISTERS)

#ifdef _M_IX86
typedef XSAVE_FORMAT XMM_SAVE_AREA32, *PXMM_SAVE_AREA32;
#endif

typedef struct DECLSPEC_ALIGN(16) _AMD64_CONTEXT {
    DWORD64 P1Home;          /* 000 */
    DWORD64 P2Home;          /* 008 */
    DWORD64 P3Home;          /* 010 */
    DWORD64 P4Home;          /* 018 */
    DWORD64 P5Home;          /* 020 */
    DWORD64 P6Home;          /* 028 */

    /* Control flags */
    DWORD ContextFlags;      /* 030 */
    DWORD MxCsr;             /* 034 */

    /* Segment */
    WORD SegCs;              /* 038 */
    WORD SegDs;              /* 03a */
    WORD SegEs;              /* 03c */
    WORD SegFs;              /* 03e */
    WORD SegGs;              /* 040 */
    WORD SegSs;              /* 042 */
    DWORD EFlags;            /* 044 */

    /* Debug */
    DWORD64 Dr0;             /* 048 */
    DWORD64 Dr1;             /* 050 */
    DWORD64 Dr2;             /* 058 */
    DWORD64 Dr3;             /* 060 */
    DWORD64 Dr6;             /* 068 */
    DWORD64 Dr7;             /* 070 */

    /* Integer */
    DWORD64 Rax;             /* 078 */
    DWORD64 Rcx;             /* 080 */
    DWORD64 Rdx;             /* 088 */
    DWORD64 Rbx;             /* 090 */
    DWORD64 Rsp;             /* 098 */
    DWORD64 Rbp;             /* 0a0 */
    DWORD64 Rsi;             /* 0a8 */
    DWORD64 Rdi;             /* 0b0 */
    DWORD64 R8;              /* 0b8 */
    DWORD64 R9;              /* 0c0 */
    DWORD64 R10;             /* 0c8 */
    DWORD64 R11;             /* 0d0 */
    DWORD64 R12;             /* 0d8 */
    DWORD64 R13;             /* 0e0 */
    DWORD64 R14;             /* 0e8 */
    DWORD64 R15;             /* 0f0 */

    /* Counter */
    DWORD64 Rip;             /* 0f8 */

    /* Floating point */
    union {
        XMM_SAVE_AREA32 FltSave;  /* 100 */
        struct {
            M128A Header[2];      /* 100 */
            M128A Legacy[8];      /* 120 */
            M128A Xmm0;           /* 1a0 */
            M128A Xmm1;           /* 1b0 */
            M128A Xmm2;           /* 1c0 */
            M128A Xmm3;           /* 1d0 */
            M128A Xmm4;           /* 1e0 */
            M128A Xmm5;           /* 1f0 */
            M128A Xmm6;           /* 200 */
            M128A Xmm7;           /* 210 */
            M128A Xmm8;           /* 220 */
            M128A Xmm9;           /* 230 */
            M128A Xmm10;          /* 240 */
            M128A Xmm11;          /* 250 */
            M128A Xmm12;          /* 260 */
            M128A Xmm13;          /* 270 */
            M128A Xmm14;          /* 280 */
            M128A Xmm15;          /* 290 */
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;

    /* Vector */
    M128A VectorRegister[26];     /* 300 */
    DWORD64 VectorControl;        /* 4a0 */

    /* Debug control */
    DWORD64 DebugControl;         /* 4a8 */
    DWORD64 LastBranchToRip;      /* 4b0 */
    DWORD64 LastBranchFromRip;    /* 4b8 */
    DWORD64 LastExceptionToRip;   /* 4c0 */
    DWORD64 LastExceptionFromRip; /* 4c8 */
} AMD64_CONTEXT;

/* The Win32 register context */

/* i386 context definitions */

#define I386_SIZE_OF_80387_REGISTERS      80

typedef struct _I386_FLOATING_SAVE_AREA
{
    DWORD   ControlWord;
    DWORD   StatusWord;
    DWORD   TagWord;
    DWORD   ErrorOffset;
    DWORD   ErrorSelector;
    DWORD   DataOffset;
    DWORD   DataSelector;
    BYTE    RegisterArea[I386_SIZE_OF_80387_REGISTERS];
    DWORD   Cr0NpxState;
} I386_FLOATING_SAVE_AREA, WOW64_FLOATING_SAVE_AREA, *PWOW64_FLOATING_SAVE_AREA;

#define I386_MAXIMUM_SUPPORTED_EXTENSION     512

#define XSTATE_LEGACY_FLOATING_POINT 0
#define XSTATE_LEGACY_SSE            1
#define XSTATE_GSSE                  2
#define XSTATE_AVX                   XSTATE_GSSE
#define XSTATE_MPX_BNDREGS           3
#define XSTATE_MPX_BNDCSR            4
#define XSTATE_AVX512_KMASK          5
#define XSTATE_AVX512_ZMM_H          6
#define XSTATE_AVX512_ZMM            7
#define XSTATE_IPT                   8
#define XSTATE_CET_U                 11
#define XSTATE_LWP                   62
#define MAXIMUM_XSTATE_FEATURES      64

#define XSTATE_MASK_LEGACY_FLOATING_POINT   (1 << XSTATE_LEGACY_FLOATING_POINT)
#define XSTATE_MASK_LEGACY_SSE              (1 << XSTATE_LEGACY_SSE)
#define XSTATE_MASK_LEGACY                  (XSTATE_MASK_LEGACY_FLOATING_POINT | XSTATE_MASK_LEGACY_SSE)
#define XSTATE_MASK_GSSE                    (1 << XSTATE_GSSE)

typedef struct _XSTATE_FEATURE
{
    ULONG Offset;
    ULONG Size;
} XSTATE_FEATURE, *PXSTATE_FEATURE;

typedef struct _XSTATE_CONFIGURATION
{
    ULONG64 EnabledFeatures;
    ULONG64 EnabledVolatileFeatures;
    ULONG Size;
    ULONG OptimizedSave:1;
    ULONG CompactionEnabled:1;
    XSTATE_FEATURE Features[MAXIMUM_XSTATE_FEATURES];

    ULONG64 EnabledSupervisorFeatures;
    ULONG64 AlignedFeatures;
    ULONG AllFeatureSize;
    ULONG AllFeatures[MAXIMUM_XSTATE_FEATURES];
    ULONG64 EnabledUserVisibleSupervisorFeatures;
} XSTATE_CONFIGURATION, *PXSTATE_CONFIGURATION;

typedef struct _YMMCONTEXT
{
    M128A Ymm0;
    M128A Ymm1;
    M128A Ymm2;
    M128A Ymm3;
    M128A Ymm4;
    M128A Ymm5;
    M128A Ymm6;
    M128A Ymm7;
    M128A Ymm8;
    M128A Ymm9;
    M128A Ymm10;
    M128A Ymm11;
    M128A Ymm12;
    M128A Ymm13;
    M128A Ymm14;
    M128A Ymm15;
}
YMMCONTEXT, *PYMMCONTEXT;

typedef struct _XSTATE
{
    ULONG64 Mask;
    ULONG64 CompactionMask;
    ULONG64 Reserved[6];
    YMMCONTEXT YmmContext;
} XSTATE, *PXSTATE;

#if defined(_M_IX86)

typedef struct _PEB32
{
    BOOLEAN                      InheritedAddressSpace;             /* 0000 */
    BOOLEAN                      ReadImageFileExecOptions;          /* 0001 */
    BOOLEAN                      BeingDebugged;                     /* 0002 */
    UCHAR                        ImageUsedLargePages : 1;           /* 0003 */
    UCHAR                        IsProtectedProcess : 1;
    UCHAR                        IsImageDynamicallyRelocated : 1;
    UCHAR                        SkipPatchingUser32Forwarders : 1;
    UCHAR                        IsPackagedProcess : 1;
    UCHAR                        IsAppContainer: 1;
    UCHAR                        IsProtectedProcessLight : 1;
    UCHAR                        IsLongPathAwareProcess : 1;
    ULONG                        Mutant;                            /* 0004 */
    ULONG                        ImageBaseAddress;                  /* 0008 */
    ULONG                        LdrData;                           /* 000c */
    ULONG                        ProcessParameters;                 /* 0010 */
    ULONG                        SubSystemData;                     /* 0014 */
    ULONG                        ProcessHeap;                       /* 0018 */
    ULONG                        FastPebLock;                       /* 001c */
    ULONG                        AtlThunkSListPtr;                  /* 0020 */
    ULONG                        IFEOKey;                           /* 0024 */
    ULONG                        ProcessInJob : 1;                  /* 0028 */
    ULONG                        ProcessInitializing : 1;
    ULONG                        ProcessUsingVEH : 1;
    ULONG                        ProcessUsingVCH : 1;
    ULONG                        ProcessUsingFTH : 1;
    ULONG                        ProcessPreviouslyThrottled : 1;
    ULONG                        ProcessCurrentlyThrottled : 1;
    ULONG                        ProcessImagesHotPatched : 1;
    ULONG                        ReservedBits0 : 24;
    ULONG                        KernelCallbackTable;               /* 002c */
    ULONG                        Reserved;                          /* 0030 */
    ULONG                        AtlThunkSListPtr32;                /* 0034 */
    ULONG                        ApiSetMap;                         /* 0038 */
    ULONG                        TlsExpansionCounter;               /* 003c */
    ULONG                        TlsBitmap;                         /* 0040 */
    ULONG                        TlsBitmapBits[2];                  /* 0044 */
    ULONG                        ReadOnlySharedMemoryBase;          /* 004c */
    ULONG                        SharedData;                        /* 0050 */
    ULONG                        ReadOnlyStaticServerData;          /* 0054 */
    ULONG                        AnsiCodePageData;                  /* 0058 */
    ULONG                        OemCodePageData;                   /* 005c */
    ULONG                        UnicodeCaseTableData;              /* 0060 */
    ULONG                        NumberOfProcessors;                /* 0064 */
    ULONG                        NtGlobalFlag;                      /* 0068 */
    LARGE_INTEGER                CriticalSectionTimeout;            /* 0070 */
    ULONG                        HeapSegmentReserve;                /* 0078 */
    ULONG                        HeapSegmentCommit;                 /* 007c */
    ULONG                        HeapDeCommitTotalFreeThreshold;    /* 0080 */
    ULONG                        HeapDeCommitFreeBlockThreshold;    /* 0084 */
    ULONG                        NumberOfHeaps;                     /* 0088 */
    ULONG                        MaximumNumberOfHeaps;              /* 008c */
    ULONG                        ProcessHeaps;                      /* 0090 */
    ULONG                        GdiSharedHandleTable;              /* 0094 */
    ULONG                        ProcessStarterHelper;              /* 0098 */
    ULONG                        GdiDCAttributeList;                /* 009c */
    ULONG                        LoaderLock;                        /* 00a0 */
    ULONG                        OSMajorVersion;                    /* 00a4 */
    ULONG                        OSMinorVersion;                    /* 00a8 */
    ULONG                        OSBuildNumber;                     /* 00ac */
    ULONG                        OSCSDVersion;                     /* 00ac */
    ULONG                        OSPlatformId;                      /* 00b0 */
    ULONG                        ImageSubSystem;                    /* 00b4 */
    ULONG                        ImageSubSystemMajorVersion;        /* 00b8 */
    ULONG                        ImageSubSystemMinorVersion;        /* 00bc */
    ULONG                        ActiveProcessAffinityMask;         /* 00c0 */
    ULONG                        GdiHandleBuffer[34];               /* 00c4 */
    ULONG                        PostProcessInitRoutine;            /* 014c */
    ULONG                        TlsExpansionBitmap;                /* 0150 */
    ULONG                        TlsExpansionBitmapBits[32];        /* 0154 */
    ULONG                        SessionId;                         /* 01d4 */
    ULARGE_INTEGER               AppCompatFlags;                    /* 01d8 */
    ULARGE_INTEGER               AppCompatFlagsUser;                /* 01e0 */
    ULONG                        ShimData;                          /* 01e8 */
    ULONG                        AppCompatInfo;                     /* 01ec */
    UNICODE_STRING32             CSDVersion;                        /* 01f0 */
    ULONG                        ActivationContextData;             /* 01f8 */
    ULONG                        ProcessAssemblyStorageMap;         /* 01fc */
    ULONG                        SystemDefaultActivationData;       /* 0200 */
    ULONG                        SystemAssemblyStorageMap;          /* 0204 */
    ULONG                        MinimumStackCommit;                /* 0208 */
    ULONG                        FlsCallback;                       /* 020c */
    LIST_ENTRY32                 FlsListHead;                       /* 0210 */
    ULONG                        FlsBitmap;                         /* 0218 */
    ULONG                        FlsBitmapBits[4];                  /* 021c */
    ULONG                        FlsHighIndex;                      /* 022c */
    ULONG                        WerRegistrationData;               /* 0230 */
    ULONG                        WerShipAssertPtr;                  /* 0234 */
    ULONG                        pUnused;                           /* 0238 */
    ULONG                        pImageHeaderHash;                  /* 023c */
    ULONG                        HeapTracingEnabled : 1;            /* 0240 */
    ULONG                        CritSecTracingEnabled : 1;
    ULONG                        LibLoaderTracingEnabled : 1;
    ULONG                        SpareTracingBits : 29;
    ULONGLONG                    CsrServerReadOnlySharedMemoryBase; /* 0248 */
    ULONG                        TppWorkerpListLock;                /* 0250 */
    LIST_ENTRY32                 TppWorkerpList;                    /* 0254 */
    ULONG                        WaitOnAddressHashTable [0x80];     /* 025c */
    ULONG                        TelemetryCoverageHeader;           /* 045c */
    ULONG                        CloudFileFlags;                    /* 0460 */
    ULONG                        CloudFileDiagFlags;                /* 0464 */
    CHAR                         PlaceholderCompatibilityMode;      /* 0468 */
    CHAR                         PlaceholderCompatibilityModeReserved[7]; /* 0469 */
    ULONG                        LeapSecondData;                    /* 0470 */
    ULONG                        LeapSecondFlags;                   /* 0474 */
    ULONG                        NtGlobalFlag2;                     /* 0478 */
} PEB32;
#endif

#define PS_ATTRIBUTE_THREAD   0x00010000
#define PS_ATTRIBUTE_INPUT    0x00020000
#define PS_ATTRIBUTE_ADDITIVE 0x00040000

typedef enum _PS_ATTRIBUTE_NUM
{
    PsAttributeParentProcess,
    PsAttributeDebugPort,
    PsAttributeToken,
    PsAttributeClientId,
    PsAttributeTebAddress,
    PsAttributeImageName,
    PsAttributeImageInfo,
    PsAttributeMemoryReserve,
    PsAttributePriorityClass,
    PsAttributeErrorMode,
    PsAttributeStdHandleInfo,
    PsAttributeHandleList,
    PsAttributeGroupAffinity,
    PsAttributePreferredNode,
    PsAttributeIdealProcessor,
    PsAttributeUmsThread,
    PsAttributeMitigationOptions,
    PsAttributeProtectionLevel,
    PsAttributeSecureProcess,
    PsAttributeJobList,
    PsAttributeChildProcessPolicy,
    PsAttributeAllApplicationPackagesPolicy,
    PsAttributeWin32kFilter,
    PsAttributeSafeOpenPromptOriginClaim,
    PsAttributeBnoIsolation,
    PsAttributeDesktopAppPolicy,
    PsAttributeChpe,
    PsAttributeMitigationAuditOptions,
    PsAttributeMachineType,
    PsAttributeComponentFilter,
    PsAttributeEnableOptionalXStateFeatures,
    PsAttributeMax
} PS_ATTRIBUTE_NUM;

#define PS_ATTRIBUTE_PARENT_PROCESS     (PsAttributeParentProcess | PS_ATTRIBUTE_INPUT | PS_ATTRIBUTE_ADDITIVE)
#define PS_ATTRIBUTE_DEBUG_PORT         (PsAttributeDebugPort | PS_ATTRIBUTE_INPUT | PS_ATTRIBUTE_ADDITIVE)
#define PS_ATTRIBUTE_TOKEN              (PsAttributeToken | PS_ATTRIBUTE_INPUT | PS_ATTRIBUTE_ADDITIVE)
#define PS_ATTRIBUTE_CLIENT_ID          (PsAttributeClientId | PS_ATTRIBUTE_THREAD)
#define PS_ATTRIBUTE_TEB_ADDRESS        (PsAttributeTebAddress | PS_ATTRIBUTE_THREAD)
#define PS_ATTRIBUTE_IMAGE_NAME         (PsAttributeImageName | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_IMAGE_INFO         (PsAttributeImageInfo)
#define PS_ATTRIBUTE_MEMORY_RESERVE     (PsAttributeMemoryReserve | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_PRIORITY_CLASS     (PsAttributePriorityClass | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_ERROR_MODE         (PsAttributeErrorMode | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_STD_HANDLE_INFO    (PsAttributeStdHandleInfo | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_HANDLE_LIST        (PsAttributeHandleList | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_GROUP_AFFINITY     (PsAttributeGroupAffinity | PS_ATTRIBUTE_THREAD | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_PREFERRED_NODE     (PsAttributePreferredNode | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_IDEAL_PROCESSOR    (PsAttributeIdealProcessor | PS_ATTRIBUTE_THREAD | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_MITIGATION_OPTIONS (PsAttributeMitigationOptions | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_PROTECTION_LEVEL   (PsAttributeProtectionLevel | PS_ATTRIBUTE_INPUT | PS_ATTRIBUTE_ADDITIVE)
#define PS_ATTRIBUTE_SECURE_PROCESS     (PsAttributeSecureProcess | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_JOB_LIST           (PsAttributeJobList | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_CHILD_PROCESS_POLICY (PsAttributeChildProcessPolicy | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_ALL_APPLICATION_PACKAGES_POLICY (PsAttributeAllApplicationPackagesPolicy | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_WIN32K_FILTER      (PsAttributeWin32kFilter | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_SAFE_OPEN_PROMPT_ORIGIN_CLAIM (PsAttributeSafeOpenPromptOriginClaim | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_BNO_ISOLATION      (PsAttributeBnoIsolation | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_DESKTOP_APP_POLICY (PsAttributeDesktopAppPolicy | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_CHPE               (PsAttributeChpe | PS_ATTRIBUTE_INPUT | PS_ATTRIBUTE_ADDITIVE)
#define PS_ATTRIBUTE_MITIGATION_AUDIT_OPTIONS (PsAttributeMitigationAuditOptions | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_MACHINE_TYPE       (PsAttributeMachineType | PS_ATTRIBUTE_INPUT | PS_ATTRIBUTE_ADDITIVE)
#define PS_ATTRIBUTE_COMPONENT_FILTER   (PsAttributeComponentFilter | PS_ATTRIBUTE_INPUT)
#define PS_ATTRIBUTE_ENABLE_OPTIONAL_XSTATE_FEATURES (PsAttributeEnableOptionalXStateFeatures | PS_ATTRIBUTE_THREAD | PS_ATTRIBUTE_INPUT)

typedef struct _PS_ATTRIBUTE
{
    ULONG_PTR Attribute;
    SIZE_T    Size;
    union
    {
        ULONG_PTR Value;
        void     *ValuePtr;
    };
    SIZE_T *ReturnLength;
} PS_ATTRIBUTE,*PPS_ATTRIBUTE;

typedef struct _PS_ATTRIBUTE_LIST
{
    SIZE_T       TotalLength;
    PS_ATTRIBUTE Attributes[1];
} PS_ATTRIBUTE_LIST, *PPS_ATTRIBUTE_LIST;
