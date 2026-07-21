/*++

Copyright (c) 2018 Shorthorn Project

Module Name:

    filemap.c

Abstract:

    This module implements Win32 mapped file APIs

Author:

    Skulltrail 21-April-2018

Revision History:

--*/

#include "main.h"

typedef BOOLEAN (WINAPI *pRtlGenRandomPtr)(PVOID RandomBuffer, ULONG RandomBufferLength);

/* typedef do ponteiro da função CreateFileMappingW */
typedef HANDLE (WINAPI *PFN_CreateFileMappingW)(
    HANDLE hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD flProtect,
    DWORD dwMaximumSizeHigh,
    DWORD dwMaximumSizeLow,
    LPCWSTR lpName
);

/* ponteiro para a implementação original */
static PFN_CreateFileMappingW orig_CreateFileMappingW = NULL;

/* handle de kernel32 mantido para evitar unload acidental */
static HMODULE hKernel32 = NULL;

#define ConvertAnsiToUnicodePrologue                                            \
{                                                                               \
    NTSTATUS Status;                                                            \
    PUNICODE_STRING UnicodeCache;                                               \
    ANSI_STRING AnsiName;
#define ConvertAnsiToUnicodeBody(name)                                          \
    UnicodeCache = &NtCurrentTeb()->StaticUnicodeString;                        \
    RtlInitAnsiString(&AnsiName, name);                                         \
    Status = RtlAnsiStringToUnicodeString(UnicodeCache, &AnsiName, FALSE);
#define ConvertAnsiToUnicodeEpilogue                                            \
    if (Status == STATUS_BUFFER_OVERFLOW)                                       \
        SetLastError(ERROR_FILENAME_EXCED_RANGE);                               \
    else                                                                        \
        BaseSetLastNTError(Status);                                           \
    return FALSE;                                                               \
}

//
// This macro uses the ConvertAnsiToUnicode macros above to convert a CreateXxxA
// Win32 API into its equivalent CreateXxxW API.
//
#define ConvertWin32AnsiObjectApiToUnicodeApi(obj, name, ...)                   \
    ConvertAnsiToUnicodePrologue                                                \
    if (!name) return Create##obj##W(__VA_ARGS__, NULL);                        \
    ConvertAnsiToUnicodeBody(name)                                              \
    if (NT_SUCCESS(Status)) return Create##obj##W(__VA_ARGS__, UnicodeCache->Buffer);  \
    ConvertAnsiToUnicodeEpilogue


/*
 * @implemented
 */
HANDLE
WINAPI
CreateFileMappingA(IN HANDLE hFile,
                   IN LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                   IN DWORD flProtect,
                   IN DWORD dwMaximumSizeHigh,
                   IN DWORD dwMaximumSizeLow,
                   IN LPCSTR lpName)
{
    /* Call the W(ide) function */
    ConvertWin32AnsiObjectApiToUnicodeApi(FileMapping,
                                          lpName,
                                          hFile,
                                          lpFileMappingAttributes,
                                          flProtect,
                                          dwMaximumSizeHigh,
                                          dwMaximumSizeLow);
}

LPWSTR CreateNewFallbackMappingName() {
    unsigned char j;
	WCHAR* chr;
	HMODULE mod;
	pRtlGenRandomPtr paddr;
	int i;
	LPWSTR res = RtlAllocateHeap(GetProcessHeap(), 0, 34); // The chance of collision is 1 in 50 sextillion or 1 in 220 billion depending on the context. So we should be fine
    if (!res)
        return NULL;
    ZeroMemory(res, sizeof(WCHAR) * 17);
    chr = (WCHAR*)((char*)res + 1);
    mod = LoadLibraryW(L"ADVAPI32.DLL");
    paddr = (pRtlGenRandomPtr) GetProcAddress(mod, "SystemFunction036");
    
    for (i = 0; i < 16; i++) {
        j = 0;
        paddr(&j, 1);
        *chr = 65 + j % 26;
        chr++;
    }
    return res;
}

HANDLE
WINAPI
CreateFileMappingWInternal(HANDLE hFile,
                   LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                   DWORD flProtect,
                   DWORD dwMaximumSizeHigh,
                   DWORD dwMaximumSizeLow,
                   LPCWSTR lpName)
{
    NTSTATUS Status;
    HANDLE SectionHandle;
    OBJECT_ATTRIBUTES LocalAttributes;
    POBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING SectionName;
    ACCESS_MASK DesiredAccess;
    LARGE_INTEGER LocalSize;
    PLARGE_INTEGER SectionSize = NULL;
    ULONG Attributes;
	
    /* Set default access */
    DesiredAccess = STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ;

    /* Get the attributes for the actual allocation and cleanup flProtect */
    Attributes = flProtect & (SEC_FILE | SEC_IMAGE | SEC_RESERVE | SEC_NOCACHE | SEC_COMMIT | SEC_LARGE_PAGES);
    flProtect ^= Attributes;

    /* If the caller didn't say anything, assume SEC_COMMIT */
    if (!Attributes) Attributes = SEC_COMMIT;

    /* Now check if the caller wanted write access */
    if (flProtect == PAGE_READWRITE)
    {
        /* Give it */
        DesiredAccess |= SECTION_MAP_WRITE;
    }
    else if (flProtect == PAGE_EXECUTE_READWRITE)
    {
        /* Give it */
        DesiredAccess |= (SECTION_MAP_WRITE | SECTION_MAP_EXECUTE);
    }
    else if (flProtect == PAGE_EXECUTE_READ)
    {
        /* Give it */
        DesiredAccess |= SECTION_MAP_EXECUTE;
    }
    else if ((flProtect == PAGE_EXECUTE_WRITECOPY) &&
             (NtCurrentPeb()->OSMajorVersion >= 6))
    {
        /* Give it */
        DesiredAccess |= (SECTION_MAP_WRITE | SECTION_MAP_EXECUTE);
    }
    else if ((flProtect != PAGE_READONLY) && (flProtect != PAGE_WRITECOPY))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    /* Now check if we got a name */
    if (lpName) RtlInitUnicodeString(&SectionName, lpName);

    /* Now convert the object attributes */
    ObjectAttributes = BaseFormatObjectAttributes(&LocalAttributes,
                                                    lpFileMappingAttributes,
                                                    lpName ? &SectionName : NULL);

    /* Check if we got a size */
    if (dwMaximumSizeLow || dwMaximumSizeHigh)
    {
        /* Use a LARGE_INTEGER and convert */
        SectionSize = &LocalSize;
        SectionSize->LowPart = dwMaximumSizeLow;
        SectionSize->HighPart = dwMaximumSizeHigh;
    }

    /* Make sure the handle is valid */
    if (hFile == INVALID_HANDLE_VALUE)
    {
        /* It's not, we'll only go on if we have a size */
        hFile = NULL;
        if (!SectionSize)
        {
            /* No size, so this isn't a valid non-mapped section */
            SetLastError(ERROR_INVALID_PARAMETER);
            return NULL;
        }
    }

    /* Now create the actual section */
    Status = NtCreateSection(&SectionHandle,
                             DesiredAccess,
                             ObjectAttributes,
                             SectionSize,
                             flProtect,
                             Attributes,
                             hFile);
    if (!NT_SUCCESS(Status))
    {
        /* We failed */
        BaseSetLastNTError(Status);
        return NULL;
    }
    else if (Status == STATUS_OBJECT_NAME_EXISTS)
    {
        SetLastError(ERROR_ALREADY_EXISTS);
    }
    else
    {
        SetLastError(ERROR_SUCCESS);
    }

    /* Return the section */
    return SectionHandle;	
}

/*
 * @implemented
 */
HANDLE
WINAPI
CreateFileMappingW(HANDLE hFile,
                   LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
                   DWORD flProtect,
                   DWORD dwMaximumSizeHigh,
                   DWORD dwMaximumSizeLow,
                   LPCWSTR lpName)
{
	LPCWSTR newName;
    FARPROC proc;	
	
	//Hack for chrome 110+ work
	if(hFile == INVALID_HANDLE_VALUE && 
	   lpFileMappingAttributes != NULL &&
	   flProtect == PAGE_READWRITE &&
	   dwMaximumSizeHigh == 0 &&
	   lpName == NULL)
	{
		newName = CreateNewFallbackMappingName();
		lpName = newName;
		//HeapFree(GetProcessHeap(), 0, newName);	
	}	

    /* tenta obter handle já carregado, se não existir carrega */
    hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (hKernel32 == NULL) {
        hKernel32 = LoadLibraryW(L"kernel32.dll");
        if (hKernel32 == NULL) {
            /* falha ao carregar kernel32 (muito improvável) */
            goto implDefault;
        }
    }

    proc = GetProcAddress(hKernel32, "CreateFileMappingWNative");
    if (proc) {
		orig_CreateFileMappingW = (PFN_CreateFileMappingW)proc;	
		return orig_CreateFileMappingW(
			hFile,
			lpFileMappingAttributes,
			flProtect,
			dwMaximumSizeHigh,
			dwMaximumSizeLow,
			lpName
		);		
	}else{
		goto implDefault;
	}	
	
implDefault:
    return CreateFileMappingWInternal(hFile, 
									  lpFileMappingAttributes,
									  flProtect,
									  dwMaximumSizeHigh,
									  dwMaximumSizeLow,
									  lpName);
}

/*
 * @implemented
 */
HANDLE
WINAPI
CreateFileMappingNumaW(
	HANDLE hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD flProtect,
    DWORD dwMaximumSizeHigh,
    DWORD dwMaximumSizeLow,
    LPCWSTR lpName,
	DWORD nndPreferred
)
{
    NTSTATUS Status;
    HANDLE SectionHandle;
    OBJECT_ATTRIBUTES LocalAttributes;
    POBJECT_ATTRIBUTES ObjectAttributes;
    UNICODE_STRING SectionName;
    ACCESS_MASK DesiredAccess;
    LARGE_INTEGER LocalSize;
    PLARGE_INTEGER SectionSize = NULL;
    ULONG Attributes;

    /* Set default access */
    DesiredAccess = STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ;

    /* Get the attributes for the actual allocation and cleanup flProtect */
    Attributes = flProtect & (SEC_FILE | SEC_IMAGE | SEC_RESERVE | SEC_NOCACHE | SEC_COMMIT | SEC_LARGE_PAGES);
    flProtect ^= Attributes;

    /* If the caller didn't say anything, assume SEC_COMMIT */
    if (!Attributes) Attributes = SEC_COMMIT;

    /* Now check if the caller wanted write access */
    if (flProtect == PAGE_READWRITE)
    {
        /* Give it */
        DesiredAccess |= SECTION_MAP_WRITE;
    }
    else if (flProtect == PAGE_EXECUTE_READWRITE)
    {
        /* Give it */
        DesiredAccess |= (SECTION_MAP_WRITE | SECTION_MAP_EXECUTE);
    }
    else if (flProtect == PAGE_EXECUTE_READ)
    {
        /* Give it */
        DesiredAccess |= SECTION_MAP_EXECUTE;
    }
    else if ((flProtect == PAGE_EXECUTE_WRITECOPY) &&
             (NtCurrentPeb()->OSMajorVersion >= 6))
    {
        /* Give it */
        DesiredAccess |= (SECTION_MAP_WRITE | SECTION_MAP_EXECUTE);
    }
    else if ((flProtect != PAGE_READONLY) && (flProtect != PAGE_WRITECOPY))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    /* Now check if we got a name */
    if (lpName) RtlInitUnicodeString(&SectionName, lpName);

    /* Now convert the object attributes */
    ObjectAttributes = BaseFormatObjectAttributes(&LocalAttributes,
                                                    lpFileMappingAttributes,
                                                    lpName ? &SectionName : NULL);

    /* Check if we got a size */
    if (dwMaximumSizeLow || dwMaximumSizeHigh)
    {
        /* Use a LARGE_INTEGER and convert */
        SectionSize = &LocalSize;
        SectionSize->LowPart = dwMaximumSizeLow;
        SectionSize->HighPart = dwMaximumSizeHigh;
    }

    /* Make sure the handle is valid */
    if (hFile == INVALID_HANDLE_VALUE)
    {
        /* It's not, we'll only go on if we have a size */
        hFile = NULL;
        if (!SectionSize)
        {
            /* No size, so this isn't a valid non-mapped section */
            SetLastError(ERROR_INVALID_PARAMETER);
            return NULL;
        }
    }
	
	if ( nndPreferred != -1 )
		flProtect |= nndPreferred + 1;

    /* Now create the actual section */
    Status = NtCreateSection(&SectionHandle,
                             DesiredAccess,
                             ObjectAttributes,
                             SectionSize,
                             flProtect,
                             Attributes,
                             hFile);
    if (!NT_SUCCESS(Status))
    {
        /* We failed */
        BaseSetLastNTError(Status);
        return NULL;
    }
    else if (Status == STATUS_OBJECT_NAME_EXISTS)
    {
        SetLastError(ERROR_ALREADY_EXISTS);
    }
    else
    {
        SetLastError(ERROR_SUCCESS);
    }

    /* Return the section */
    return SectionHandle;
}

/*
 * @implemented
 */
HANDLE
WINAPI
CreateFileMappingNumaA(
	IN HANDLE hFile,
    IN LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    IN DWORD flProtect,
    IN DWORD dwMaximumSizeHigh,
    IN DWORD dwMaximumSizeLow,
    IN LPCSTR lpName,
	IN DWORD nndPreferred
)
{
    PUNICODE_STRING Unicode;
    ANSI_STRING AnsiString;
    NTSTATUS Status;
    LPCWSTR NameBuffer;

    NameBuffer = NULL;
    if ( ARGUMENT_PRESENT(lpName) ) {
        Unicode = &NtCurrentTeb()->StaticUnicodeString;
        RtlInitAnsiString(&AnsiString,lpName);
        Status = RtlAnsiStringToUnicodeString(Unicode,&AnsiString,FALSE);
        if ( !NT_SUCCESS(Status) ) {
            if ( Status == STATUS_BUFFER_OVERFLOW ) {
                SetLastError(ERROR_FILENAME_EXCED_RANGE);
                }
            else {
                BaseSetLastNTError(Status);
                }
            return NULL;
            }
        NameBuffer = (LPCWSTR)Unicode->Buffer;
        }

    return CreateFileMappingNumaW(
                hFile,
                lpFileMappingAttributes,
                flProtect,
                dwMaximumSizeHigh,
                dwMaximumSizeLow,
                NameBuffer,
                nndPreferred);
}

/*
 * @implemented
 */
LPVOID
WINAPI
MapViewOfFileExNuma(
	HANDLE hFileMappingObject,
	DWORD dwDesiredAccess,
	DWORD dwFileOffsetHigh,
	DWORD dwFileOffsetLow,
	SIZE_T dwNumberOfBytesToMap,
	LPVOID lpBaseAddress,
	DWORD nndPreferred
)
{
    NTSTATUS Status;
    LARGE_INTEGER SectionOffset;
    SIZE_T ViewSize;
    ULONG Protect;
    LPVOID ViewBase;

    /* Convert the offset */
    SectionOffset.LowPart = dwFileOffsetLow;
    SectionOffset.HighPart = dwFileOffsetHigh;

    /* Save the size and base */
    ViewBase = lpBaseAddress;
    ViewSize = dwNumberOfBytesToMap;

    /* Convert flags to NT Protection Attributes */
    if (dwDesiredAccess == FILE_MAP_COPY)
    {
        Protect = PAGE_WRITECOPY;
    }
    else if (dwDesiredAccess & FILE_MAP_WRITE)
    {
        Protect = (dwDesiredAccess & FILE_MAP_EXECUTE) ?
                   PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
    }
    else if (dwDesiredAccess & FILE_MAP_READ)
    {
        Protect = (dwDesiredAccess & FILE_MAP_EXECUTE) ?
                   PAGE_EXECUTE_READ : PAGE_READONLY;
    }
    else
    {
        Protect = PAGE_NOACCESS;
    }
	
	if ( nndPreferred != -1 )
		Protect |= nndPreferred + 1;

    /* Map the section */
    Status = NtMapViewOfSection(hFileMappingObject,
                                NtCurrentProcess(),
                                &ViewBase,
                                0,
                                0,
                                &SectionOffset,
                                &ViewSize,
                                ViewShare,
                                0,
                                Protect);
    if (!NT_SUCCESS(Status))
    {
        /* We failed */
        BaseSetLastNTError(Status);
        return NULL;
    }

    /* Return the base */
    return ViewBase;
}

/***********************************************************************
 *             CreateFileMappingFromApp   (kernelex.@)
 */
HANDLE 
WINAPI 
DECLSPEC_HOTPATCH 
CreateFileMappingFromApp( 
	HANDLE file, 
	LPSECURITY_ATTRIBUTES sa, 
	ULONG protect,
    ULONG64 size, 
	LPCWSTR name 
)
{
    return CreateFileMappingW( file, sa, protect, size << 32, size, name );
}

/***********************************************************************
 *             MapViewOfFileFromApp   (kernelex.@)
 */
LPVOID 
WINAPI 
DECLSPEC_HOTPATCH 
MapViewOfFileFromApp( 
	HANDLE handle, 
	ULONG access, 
	ULONG64 offset, 
	SIZE_T size 
)
{
    return MapViewOfFile( handle, access, offset << 32, offset, size );
}

HANDLE
WINAPI
OpenFileMappingFromApp(
	_In_ ULONG DesiredAccess,
	_In_ BOOL InheritHandle,
	_In_ PCWSTR Name
)
{
	return OpenFileMappingW(DesiredAccess, InheritHandle, Name);
}

BOOL
WINAPI
UnmapViewOfFileEx(
   _In_ PVOID BaseAddress,
   _In_ ULONG UnmapFlags
)
{
    // if (const auto pUnmapViewOfFileEx = try_get_UnmapViewOfFileEx())
    // {
        // return pUnmapViewOfFileEx(BaseAddress, UnmapFlags);
    // }

    return UnmapViewOfFile(BaseAddress);
}

BOOL
WINAPI
UnmapViewOfFile2(
   _In_ HANDLE process,
   _In_ PVOID BaseAddress,
   _In_ ULONG UnmapFlags
)
{
    // if (const auto pUnmapViewOfFileEx = try_get_UnmapViewOfFileEx())
    // {
        // return pUnmapViewOfFileEx(BaseAddress, UnmapFlags);
    // }

    return UnmapViewOfFile(BaseAddress);
}

// Implemented from VxKex and is needed for the MSVC++ 14.52 link.exe.
PVOID WINAPI MapViewOfFileNuma2(
    HANDLE        SectionHandle,
    HANDLE        ProcessHandle,
    ULONGLONG    Offset,
    PVOID        BaseAddress,
    SIZE_T        ViewSize,
    ULONG        AllocationType,
    ULONG        PageProtection,
    ULONG        PreferredNumaNode)
{
    NTSTATUS Status;

    if (AllocationType & ~(MEM_LARGE_PAGES | MEM_TOP_DOWN | MEM_RESERVE | MEM_ROTATE)) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (PreferredNumaNode != NUMA_NO_PREFERRED_NODE && PreferredNumaNode >= 64) {
        RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    Status = NtMapViewOfSection(
        SectionHandle,
        ProcessHandle,
        &BaseAddress,
        0,
        0,
        (PLARGE_INTEGER) &Offset,
        &ViewSize,
        ViewShare,
        AllocationType | (PreferredNumaNode + 1),
        PageProtection);

    if (!NT_SUCCESS(Status)) {
        BaseSetLastNTError(Status);
        return NULL;
    }

    return BaseAddress;
}