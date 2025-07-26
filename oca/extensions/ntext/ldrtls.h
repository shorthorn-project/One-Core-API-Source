/*++

Copyright (c) 2025  Shorthorn Project

Module Name:

    ldrtls.h

Abstract:

    Private definitions for TLS Functions.

Author:

    Skulltrail 22-July-2025

Revision History:

--*/

typedef struct _TLS_VECTOR
{
    union
    {
        ULONG  Length;
        HANDLE ThreadId;
    };
    struct _TLS_VECTOR* PreviousDeferredTlsVector;
    PVOID ModuleTlsData[ANYSIZE_ARRAY];
} TLS_VECTOR, * PTLS_VECTOR;

typedef struct _TLS_RECLAIM_TABLE_ENTRY
{
    PTLS_VECTOR TlsVector;
    RTL_SRWLOCK Lock;
} TLS_RECLAIM_TABLE_ENTRY, * PTLS_RECLAIM_TABLE_ENTRY;

typedef struct _TLS_ENTRY
{
    LIST_ENTRY            TlsEntryLinks;
    IMAGE_TLS_DIRECTORY   TlsDirectory;
    PLDR_DATA_TABLE_ENTRY ModuleEntry;
} TLS_ENTRY, * PTLS_ENTRY;

typedef struct _LDRP_TLS_ENTRY {
    LIST_ENTRY Links;
    IMAGE_TLS_DIRECTORY Tls;
} LDRP_TLS_ENTRY, *PLDRP_TLS_ENTRY;

// TLS Information
typedef struct _THREAD_TLS_INFORMATION
{
	ULONG      Flags;

	union
	{
		PVOID *TlsVector;
		PVOID  TlsModulePointer;
	};

	HANDLE     ThreadId;
} THREAD_TLS_INFORMATION, * PTHREAD_TLS_INFORMATION;

typedef enum _PROCESS_TLS_INFORMATION_TYPE
{
	ProcessTlsReplaceIndex,
	ProcessTlsReplaceVector,
	MaxProcessTlsOperation
} PROCESS_TLS_INFORMATION_TYPE, * PPROCESS_TLS_INFORMATION_TYPE;

typedef struct _PROCESS_TLS_INFORMATION
{
	ULONG                  Reserved; // Reserved bitmask
	ULONG                  OperationType;
	ULONG                  ThreadDataCount;

	union
	{
		ULONG              TlsIndex;
		ULONG              TlsVectorLength;
	};

	THREAD_TLS_INFORMATION ThreadData[ ANYSIZE_ARRAY ];
} PROCESS_TLS_INFORMATION, * PPROCESS_TLS_INFORMATION;