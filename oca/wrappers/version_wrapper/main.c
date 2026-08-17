/*
 * Implementation of VERSION.DLL
 *
 * Copyright 1996,1997 Marcus Meissner
 * Copyright 1997 David Cuthbert
 * Copyright 1999 Ulrich Weigand
 * Copyright 2005 Paul Vriens
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
 *
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winbase.h"
#include "winver.h"
#include "winuser.h"
#include "winnls.h"
#include "wine/winternl.h"
// #include "lzexpand.h"
// #include "winerror.h"
#include "wine/debug.h"
#define NDEBUG
#include <debug.h>
//#include <rtlfuncs.h>
//#include <verrsrc.h>

#ifndef ASSERT
#define ASSERT(x) 
#endif	

#define NtCurrentPeb() (NtCurrentTeb()->Peb)

#define RtlGetProcessHeap() (NtCurrentPeb()->ProcessHeap)

WINE_DEFAULT_DEBUG_CHANNEL(ver);

typedef struct _VERHEAD {
	WORD				wTotLen;
	WORD				wValLen;
	WORD				wType;
	WCHAR				szKey[(sizeof("VS_VERSION_INFO") + 3) & ~3];
	VS_FIXEDFILEINFO	vsf;
} VERHEAD, *PVERHEAD;

BOOL GetExportedFunctions(
    LPCWSTR filePath,
    char*** outNames,
    DWORD* outCount
) {
    HANDLE hFile;
    HANDLE hMapping;
    LPVOID baseAddress;
    PIMAGE_NT_HEADERS nt_headers;
    ULONG exportSize;
    PIMAGE_EXPORT_DIRECTORY exportDir;
    DWORD *nameRVAs;
    DWORD i;
    PIMAGE_DOS_HEADER dos_header;
    WORD machine;
    char **names = NULL;

    *outNames = NULL;
    *outCount = 0;

    {
        const WCHAR *ext = wcsrchr(filePath, L'.');
        if (!ext || lstrcmpiW(ext, L".exe") != 0) {
            return FALSE;
        }
    }

    hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return FALSE;
    }

    baseAddress = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!baseAddress) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return FALSE;
    }

    dos_header = (PIMAGE_DOS_HEADER)baseAddress;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
        goto cleanup;
    }

    nt_headers = (PIMAGE_NT_HEADERS)RtlImageNtHeader(baseAddress);
    if (!nt_headers || nt_headers->Signature != IMAGE_NT_SIGNATURE) {
        goto cleanup;
    }

    machine = nt_headers->FileHeader.Machine;
    if (machine != IMAGE_FILE_MACHINE_I386 &&
        machine != IMAGE_FILE_MACHINE_AMD64) {
        goto cleanup;
    }

    exportSize = 0;
    exportDir = (PIMAGE_EXPORT_DIRECTORY)
        RtlImageDirectoryEntryToData(baseAddress, TRUE,
                                    IMAGE_DIRECTORY_ENTRY_EXPORT,
                                    &exportSize);

    if (!exportDir || exportSize == 0) {
        goto cleanup;
    }

    __try {
        nameRVAs = (DWORD *)((BYTE *)baseAddress + exportDir->AddressOfNames);

        names = (char**)RtlAllocateHeap(RtlGetProcessHeap(), 0,
                  sizeof(char*) * exportDir->NumberOfNames);
        if (!names) goto cleanup;

        for (i = 0; i < exportDir->NumberOfNames; i++) {

            char *src = (char *)((BYTE *)baseAddress + nameRVAs[i]);
            SIZE_T len = lstrlenA(src) + 1;

            names[i] = (char*)RtlAllocateHeap(RtlGetProcessHeap(), 0, len);
            if (!names[i]) goto cleanup;

            CopyMemory(names[i], src, len);
        }

        *outNames = names;
        *outCount = exportDir->NumberOfNames;

        UnmapViewOfFile(baseAddress);
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return TRUE;

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        /* falha segura */
    }

cleanup:
    if (names) {
        for (i = 0; i < *outCount; i++) {
            if (names[i])
                RtlFreeHeap(RtlGetProcessHeap(), 0, names[i]);
        }
        RtlFreeHeap(RtlGetProcessHeap(), 0, names);
    }

    UnmapViewOfFile(baseAddress);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return FALSE;
}

BOOL HasFunctionInList(char **names, DWORD count, LPCSTR target) {
    DWORD i;

    for (i = 0; i < count; i++) {
        if (lstrcmpiA(names[i], target) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL 
CheckIsChromiumBasedExe(
	LPCWSTR lpApplicationName
){
	char **exports;
	DWORD count;
		
	if (GetExportedFunctions(lpApplicationName, &exports, &count)) {

		if (HasFunctionInList(exports, count, "IsSandboxedProcess") &&
			HasFunctionInList(exports, count, "GetHandleVerifier") &&
			!HasFunctionInList(exports, count, "g_originals") &&
			!HasFunctionInList(exports, count, "AcroRd32IsBrokerProcess") &&
			!HasFunctionInList(exports, count, "GetNtLoaderAPI")) {

			RtlFreeHeap(GetProcessHeap(), 0, exports);
			return TRUE;
		}

		RtlFreeHeap(GetProcessHeap(), 0, exports);
	}
	
	return FALSE;
}

static BOOLEAN StringEqual(
	IN	PCWSTR	String1,
	IN	PCWSTR	String2)
{
	ASSERT (String1 != NULL);
	ASSERT (String2 != NULL);

	while (*String1 && *String2 && *String1 == *String2) {
		++String1;
		++String2;
	}

	return (*String1 == *String2);
}

/***********************************************************************
 *           GetFileVersionInfoExW           [VERSION.@]
 */
BOOL WINAPI GetFileVersionInfoWInternal(
    IN  PCWSTR  FileName,
    IN  ULONG   Unused,
    IN  ULONG   BufferCb,
    OUT PVOID   VersionInfo)
{
    BOOL Success;
    WCHAR szProcessPath[MAX_PATH];
	VERHEAD *VerHead;

    Success = GetFileVersionInfoW(FileName, Unused, BufferCb, VersionInfo);
    if (!Success) {
        return FALSE;
    }

    if (GetModuleFileNameW(NULL, szProcessPath, MAX_PATH) == 0) {
        return Success; 
    }

    if (CheckIsChromiumBasedExe(szProcessPath)) {
        if (FileName != NULL && 
           (StringEqual(FileName, L"kernel32.dll") || StringEqual(FileName, L"kernelbase.dll"))) 
        {
            if (VersionInfo != NULL && BufferCb >= sizeof(VERHEAD)) {
                VerHead = (VERHEAD*)VersionInfo;

                VerHead->vsf.dwFileVersionMS = 0x000A0000; 
                VerHead->vsf.dwFileVersionLS = 0x28000000;
            }
        }
    }

    return Success;
}