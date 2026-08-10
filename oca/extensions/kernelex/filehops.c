/*++

Copyright (c) 2017 Shorthorn Project

Module Name:

    filehops.c

Abstract:

    This module implements Win32 file handle APIs

Author:

    Skulltrail 18-March-2017

Revision History:

--*/

#include "main.h"
#include <winbase.h>

WINE_DEFAULT_DEBUG_CHANNEL(filehops); 

//
// Network types
//

#define     WNNC_NET_MSNET       0x00010000
#define     WNNC_NET_SMB         0x00020000
#define     WNNC_NET_NETWARE     0x00030000
#define     WNNC_NET_VINES       0x00040000
#define     WNNC_NET_10NET       0x00050000
#define     WNNC_NET_LOCUS       0x00060000
#define     WNNC_NET_SUN_PC_NFS  0x00070000
#define     WNNC_NET_LANSTEP     0x00080000
#define     WNNC_NET_9TILES      0x00090000
#define     WNNC_NET_LANTASTIC   0x000A0000
#define     WNNC_NET_AS400       0x000B0000
#define     WNNC_NET_FTP_NFS     0x000C0000
#define     WNNC_NET_PATHWORKS   0x000D0000
#define     WNNC_NET_LIFENET     0x000E0000
#define     WNNC_NET_POWERLAN    0x000F0000
#define     WNNC_NET_BWNFS       0x00100000
#define     WNNC_NET_COGENT      0x00110000
#define     WNNC_NET_FARALLON    0x00120000
#define     WNNC_NET_APPLETALK   0x00130000
#define     WNNC_NET_INTERGRAPH  0x00140000
#define     WNNC_NET_SYMFONET    0x00150000
#define     WNNC_NET_CLEARCASE   0x00160000
#define     WNNC_NET_FRONTIER    0x00170000
#define     WNNC_NET_BMC         0x00180000
#define     WNNC_NET_DCE         0x00190000
#define     WNNC_NET_AVID        0x001A0000
#define     WNNC_NET_DOCUSPACE   0x001B0000
#define     WNNC_NET_MANGOSOFT   0x001C0000
#define     WNNC_NET_SERNET      0x001D0000
#define     WNNC_NET_RIVERFRONT1 0X001E0000
#define     WNNC_NET_RIVERFRONT2 0x001F0000
#define     WNNC_NET_DECORB      0x00200000
#define     WNNC_NET_PROTSTOR    0x00210000
#define     WNNC_NET_FJ_REDIR    0x00220000
#define     WNNC_NET_DISTINCT    0x00230000
#define     WNNC_NET_TWINS       0x00240000
#define     WNNC_NET_RDR2SAMPLE  0x00250000
#define     WNNC_NET_CSC         0x00260000
#define     WNNC_NET_3IN1        0x00270000
#define     WNNC_NET_EXTENDNET   0x00290000
#define     WNNC_NET_STAC        0x002A0000
#define     WNNC_NET_FOXBAT      0x002B0000
#define     WNNC_NET_YAHOO       0x002C0000
#define     WNNC_NET_EXIFS       0x002D0000
#define     WNNC_NET_DAV         0x002E0000
#define     WNNC_NET_KNOWARE     0x002F0000
#define     WNNC_NET_OBJECT_DIRE 0x00300000
#define     WNNC_NET_MASFAX      0x00310000
#define     WNNC_NET_HOB_NFS     0x00320000
#define     WNNC_NET_SHIVA       0x00330000
#define     WNNC_NET_IBMAL       0x00340000
#define     WNNC_NET_LOCK        0x00350000
#define     WNNC_NET_TERMSRV     0x00360000
#define     WNNC_NET_SRT         0x00370000
#define     WNNC_NET_QUINCY      0x00380000
#define     WNNC_NET_OPENAFS     0x00390000
#define     WNNC_NET_AVID1       0X003A0000
#define     WNNC_NET_DFS         0x003B0000
#define     WNNC_NET_KWNP        0x003C0000
#define     WNNC_NET_ZENWORKS    0x003D0000
#define     WNNC_NET_DRIVEONWEB  0x003E0000
#define     WNNC_NET_VMWARE      0x003F0000
#define     WNNC_NET_RSFX        0x00400000
#define     WNNC_NET_MFILES      0x00410000
#define     WNNC_NET_MS_NFS      0x00420000
#define     WNNC_NET_GOOGLE      0x00430000
#define     WNNC_NET_NDFS        0x00440000
#define     WNNC_NET_DOCUSHARE   0x00450000

#define     WNNC_CRED_MANAGER   0xFFFF0000

//
// Protocol generic flags for version 1 and higher
//

#define REMOTE_PROTOCOL_FLAG_LOOPBACK           0x00000001
#define REMOTE_PROTOCOL_FLAG_OFFLINE            0x00000002

/*
 * @implemented
 */
BOOLEAN
WINAPI
Wow64EnableWow64FsRedirection(IN BOOLEAN Wow64EnableWow64FsRedirection)
{
    NTSTATUS Status;
    BOOL Result;

    Status = RtlWow64EnableFsRedirection(Wow64EnableWow64FsRedirection);
    if (NT_SUCCESS(Status))
    {
        Result = TRUE;
    }
    else
    {
        BaseSetLastNTError(Status);
        Result = FALSE;
    }
    return Result;
}

/*
 * @implemented
 */
BOOL
WINAPI
Wow64DisableWow64FsRedirection(IN PVOID *OldValue)
{
    NTSTATUS Status;
    BOOL Result;

    Status = RtlWow64EnableFsRedirectionEx((PVOID)TRUE, OldValue);
    if (NT_SUCCESS(Status))
    {
        Result = TRUE;
    }
    else
    {
        BaseSetLastNTError(Status);
        Result = FALSE;
    }
    return Result;
}

/*
 * @implemented
 */
BOOL
WINAPI
Wow64RevertWow64FsRedirection(IN PVOID OldValue)
{
    NTSTATUS Status;
    BOOL Result;

    Status = RtlWow64EnableFsRedirectionEx(OldValue, &OldValue);
    if (NT_SUCCESS(Status))
    {
        Result = TRUE;
    }
    else
    {
        BaseSetLastNTError(Status);
        Result = FALSE;
    }
    return Result;
}

typedef struct _FILE_STREAM_INFO {
  DWORD         NextEntryOffset;
  DWORD         StreamNameLength;
  LARGE_INTEGER StreamSize;
  LARGE_INTEGER StreamAllocationSize;
  WCHAR         StreamName[1];
} FILE_STREAM_INFO, *PFILE_STREAM_INFO;

/***********************************************************************
*             GetFileInformationByHandleEx (KERNEL32.@)
*/
BOOL
WINAPI
GetFileInformationByHandleEx(
	_In_  HANDLE hFile,
	_In_  FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
	_Out_writes_bytes_(dwBufferSize) LPVOID lpFileInformation,
	_In_  DWORD dwBufferSize
)
{
			// if (auto const pGetFileInformationByHandleEx = try_get_GetFileInformationByHandleEx())
			// {
				// return pGetFileInformationByHandleEx(hFile, FileInformationClass, lpFileInformation, dwBufferSize);
			// }


	FILE_INFORMATION_CLASS NtFileInformationClass;
	DWORD cbMinBufferSize;
	BOOLEAN bNtQueryDirectoryFile = FALSE;
	BOOLEAN RestartScan = FALSE;
	IO_STATUS_BLOCK IoStatusBlock;
	NTSTATUS Status;			

	switch (FileInformationClass)
	{
		case FileBasicInfo:
			NtFileInformationClass = FileBasicInformation;
			cbMinBufferSize = sizeof(FILE_BASIC_INFO);
			break;
		case FileStandardInfo:
			NtFileInformationClass = FileStandardInformation;
			cbMinBufferSize = sizeof(FILE_STANDARD_INFO);
			break;
		case FileNameInfo:
			NtFileInformationClass = FileNameInformation;
			cbMinBufferSize = sizeof(FILE_NAME_INFO);
			break;
		case FileStreamInfo:
			NtFileInformationClass = FileStreamInformation;
			cbMinBufferSize = sizeof(FILE_STREAM_INFO);
			break;
		case FileCompressionInfo:
			NtFileInformationClass = FileCompressionInformation;
			cbMinBufferSize = sizeof(FILE_COMPRESSION_INFO);
			break;
		case FileAttributeTagInfo:
			NtFileInformationClass = FileAttributeTagInformation;
			cbMinBufferSize = sizeof(FILE_ATTRIBUTE_TAG_INFO);
			break;
		case FileIdBothDirectoryRestartInfo:
			NtFileInformationClass = FileIdBothDirectoryInformation;
			cbMinBufferSize = sizeof(FILE_ID_BOTH_DIR_INFO);
			bNtQueryDirectoryFile = TRUE;
			RestartScan = TRUE;
		case FileIdBothDirectoryInfo:
			NtFileInformationClass = FileIdBothDirectoryInformation;
			cbMinBufferSize = sizeof(FILE_ID_BOTH_DIR_INFO);
			bNtQueryDirectoryFile = TRUE;
			RestartScan = FALSE;
			break;
		case FileFullDirectoryRestartInfo:
			NtFileInformationClass = FileFullDirectoryInformation;
			cbMinBufferSize = sizeof(FILE_FULL_DIR_INFO);
			bNtQueryDirectoryFile = TRUE;
			RestartScan = TRUE;
		case FileFullDirectoryInfo:
			NtFileInformationClass = FileFullDirectoryInformation;
			cbMinBufferSize = sizeof(FILE_FULL_DIR_INFO);
			bNtQueryDirectoryFile = TRUE;
			RestartScan = FALSE;
			break;
		case FileRemoteProtocolInfo:
			NtFileInformationClass = FileRemoteProtocolInformation;
			cbMinBufferSize = sizeof(FILE_REMOTE_PROTOCOL_INFO);
			break;
		case FileIdInfo:
			if (dwBufferSize >= sizeof(FILE_ID_INFO)) {
				// 128-bit file IDs are something only that only ReFS supports on Windows, which there is no driver for
				// prior to Windows 8. MSVC STL 2025 is going to require this soon, so let's prepare ahead of it.
				BY_HANDLE_FILE_INFORMATION hnd;
				if (!GetFileInformationByHandle(hFile, &hnd))
					return FALSE;
				((PFILE_ID_INFO)lpFileInformation)->VolumeSerialNumber = hnd.dwVolumeSerialNumber;
				// hacky way to emulate a 128-bit file ID from a 64-bit file ID
				memset(&((PFILE_ID_INFO)lpFileInformation)->FileId, 0, 16);
				memcpy(&((PFILE_ID_INFO)lpFileInformation)->FileId, &hnd.nFileIndexHigh, 8);
				return TRUE;				
			}
			SetLastError(ERROR_BAD_LENGTH);
			return FALSE;
		case FileStorageInfo:
			if (dwBufferSize >= sizeof(FILE_STORAGE_INFO)) {
				// Usually only used for databases... but let's add it anyway!
				// TODO: figure out how to get the drive letter from a file handle
				DWORD sectors_per_cluster, bytes_per_sector, clusters_free, clusters_total;
				if (!GetDiskFreeSpaceW(NULL, &sectors_per_cluster, &bytes_per_sector, &clusters_free, &clusters_total))
					return FALSE;
				((PFILE_STORAGE_INFO)lpFileInformation)->LogicalBytesPerSector = bytes_per_sector;
				((PFILE_STORAGE_INFO)lpFileInformation)->PhysicalBytesPerSectorForAtomicity = bytes_per_sector;
				((PFILE_STORAGE_INFO)lpFileInformation)->PhysicalBytesPerSectorForPerformance = bytes_per_sector;
				((PFILE_STORAGE_INFO)lpFileInformation)->FileSystemEffectivePhysicalBytesPerSectorForAtomicity = bytes_per_sector;
				((PFILE_STORAGE_INFO)lpFileInformation)->Flags = 0;
				((PFILE_STORAGE_INFO)lpFileInformation)->ByteOffsetForSectorAlignment = 0xFFFFFFFF;
				((PFILE_STORAGE_INFO)lpFileInformation)->ByteOffsetForPartitionAlignment = 0xFFFFFFFF;
				return TRUE;			
			}
			SetLastError(ERROR_BAD_LENGTH);
			return FALSE;
		case FileAlignmentInfo:
			NtFileInformationClass = FileAlignmentInformation;
			cbMinBufferSize = sizeof(FILE_ALIGNMENT_INFO);
			break;
		default:
			// FileRemoteProtocolInfo - requires SMB2, so we can't implement that
			DbgPrint("GetFileInformationByHandleEx: unhandled parameter %i\n", FileInformationClass);
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
			break;
	}


	if (cbMinBufferSize > dwBufferSize)
	{
		SetLastError(ERROR_BAD_LENGTH);
		return FALSE;
	}
	
	// Emulated 
	if (bNtQueryDirectoryFile)
	{

		Status = NtQueryDirectoryFile(
					hFile,
					NULL,
					NULL,
					NULL,
					&IoStatusBlock,
					lpFileInformation,
					dwBufferSize,
					NtFileInformationClass,
					FALSE,
					NULL,
					RestartScan
					);

		if (STATUS_PENDING == Status)
		{
			if (WaitForSingleObjectEx(hFile, 0, FALSE) == WAIT_FAILED)
			{
				//WaitForSingleObjectEx会设置LastError
				return FALSE;
			}

			Status = IoStatusBlock.Status;
		}
	}
	else
	{
		Status = NtQueryInformationFile(hFile, &IoStatusBlock, lpFileInformation, dwBufferSize, NtFileInformationClass);
	}

	if (Status >= STATUS_SUCCESS)
	{
		if (FileStreamInfo == FileInformationClass && IoStatusBlock.Information == 0)
		{
			SetLastError(ERROR_HANDLE_EOF);
			return FALSE;
		}
		else
		{
			return TRUE;
		}
	}
	else
	{
		BaseSetLastNTError(Status);

		return FALSE;
	}
}

BOOL
WINAPI
SetFileInformationByHandle(
	_In_ HANDLE hFile,
	_In_ FILE_INFO_BY_HANDLE_CLASS FileInformationClass,
	_In_reads_bytes_(dwBufferSize) LPVOID lpFileInformation,
	_In_ DWORD dwBufferSize
)
{
	NTSTATUS Status;
	IO_STATUS_BLOCK IoStatusBlock;
	HANDLE ProcessHeap;
	FILE_INFORMATION_CLASS NtFileInformationClass;
	DWORD cbMinBufferSize;
	BOOLEAN bFreeFileInformation = FALSE;
	FILE_RENAME_INFO *pRenameInfo;
	FILE_RENAME_INFO *NewRenameInfo;
	DWORD dwNewBufferSize;
	DWORD lStatus;
	UNICODE_STRING NtName = {0};

	ProcessHeap = ((TEB*)NtCurrentTeb())->ProcessEnvironmentBlock->ProcessHeap;

	switch (FileInformationClass)
	{
		case FileBasicInfo:
			NtFileInformationClass = FileBasicInformation;
			cbMinBufferSize = sizeof(FILE_BASIC_INFO);
			break;
		case FileRenameInfo:
			NtFileInformationClass = FileRenameInformation;
			cbMinBufferSize = sizeof(FILE_RENAME_INFO);

			if (cbMinBufferSize > dwBufferSize)
			{
				SetLastError(ERROR_BAD_LENGTH);
				return FALSE;
			}

			if (lpFileInformation == NULL)
			{
				SetLastError(ERROR_INVALID_PARAMETER);
				return FALSE;
			}
			else
			{
				pRenameInfo = (FILE_RENAME_INFO*)lpFileInformation;
				if (pRenameInfo->FileNameLength < sizeof(wchar_t) || pRenameInfo->FileName[0] != L':')
				{			
					if (!RtlDosPathNameToNtPathName_U(pRenameInfo->FileName, &NtName, NULL, NULL))
					{
						SetLastError(ERROR_INVALID_PARAMETER);

						return FALSE;
					}

					dwNewBufferSize = sizeof(FILE_RENAME_INFO) + NtName.Length;

					NewRenameInfo = (FILE_RENAME_INFO*)HeapAlloc(ProcessHeap, 0, dwNewBufferSize);
					if (!NewRenameInfo)
					{
						lStatus = GetLastError();
						RtlFreeUnicodeString(&NtName);
						SetLastError(lStatus);
						return FALSE;
					}

					bFreeFileInformation = TRUE;

					NewRenameInfo->ReplaceIfExists = pRenameInfo->ReplaceIfExists;
					NewRenameInfo->RootDirectory = pRenameInfo->RootDirectory;
					NewRenameInfo->FileNameLength = NtName.Length;

					memcpy(NewRenameInfo->FileName, NtName.Buffer, NtName.Length);

					*(wchar_t*)((byte*)NewRenameInfo->FileName + NtName.Length) = L'\0';


					lpFileInformation = NewRenameInfo;
					dwBufferSize = dwNewBufferSize;

					RtlFreeUnicodeString(&NtName);
				}
			}
			break;
		case FileDispositionInfo:
			NtFileInformationClass = FileDispositionInformation;
			cbMinBufferSize = sizeof(FILE_DISPOSITION_INFO);
			break;
		case FileAllocationInfo:
			NtFileInformationClass = FileAllocationInformation;
			cbMinBufferSize = sizeof(FILE_ALLOCATION_INFO);
			break;
		case FileEndOfFileInfo:
			NtFileInformationClass = FileEndOfFileInformation;
			cbMinBufferSize = sizeof(FILE_END_OF_FILE_INFO);
			break;
		case FileIoPriorityHintInfo:
			NtFileInformationClass = FileIoPriorityHintInformation;
			cbMinBufferSize = sizeof(FILE_IO_PRIORITY_HINT_INFO);

			if (cbMinBufferSize > dwBufferSize)
			{
				SetLastError(ERROR_BAD_LENGTH);
				return FALSE;
			}

			if (lpFileInformation == NULL || ((FILE_IO_PRIORITY_HINT_INFO*)lpFileInformation)->PriorityHint >= MaximumIoPriorityHintType)
			{
				SetLastError(ERROR_INVALID_PARAMETER);
				return FALSE;
			}
			// I/O Priority hints are simply not a thing on Windows XP. So therefore the best that we can do in this case
			// is return TRUE.
			return TRUE;
		default:
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		break;
	}

	if (cbMinBufferSize > dwBufferSize)
	{
		if (bFreeFileInformation)
		{
			HeapFree(ProcessHeap, 0, lpFileInformation);
		}

		SetLastError(ERROR_BAD_LENGTH);
		return FALSE;
	}			

	Status = NtSetInformationFile(hFile, &IoStatusBlock, lpFileInformation, dwBufferSize, NtFileInformationClass);

	if (bFreeFileInformation)
	{
		HeapFree(ProcessHeap, 0, lpFileInformation);
	}

	if (Status >= STATUS_SUCCESS)
		return TRUE;
	
	BaseSetLastNTError(Status);
	return FALSE;
}

/***********************************************************************
 *             OpenFileById (KERNEL32.@)
 */
HANDLE WINAPI OpenFileById(
  _In_     HANDLE                hFile,
  _In_     LPFILE_ID_DESCRIPTOR  lpFileID,
  _In_     DWORD                 dwDesiredAccess,
  _In_     DWORD                 dwShareMode,
  _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  _In_     DWORD                 dwFlags
){
	ACCESS_MASK DesiredAccess;
	ULONG CreateOptions;
	HANDLE result;
	OBJECT_ATTRIBUTES attr;
	NTSTATUS status;
	IO_STATUS_BLOCK io;
	UNICODE_STRING objectName;

	if (!lpFileID || (lpFileID->dwSize < sizeof(FILE_ID_DESCRIPTOR)))
	{
	    SetLastError( ERROR_INVALID_PARAMETER );
	    return INVALID_HANDLE_VALUE;
	}
	
	switch(lpFileID->Type)
	{
	case FileIdType:
		objectName.Length = sizeof(LARGE_INTEGER);
		objectName.MaximumLength = sizeof(LARGE_INTEGER);
		objectName.Buffer = (WCHAR *)&lpFileID->FileId;
		break;
	
	case ObjectIdType:
		objectName.Length = sizeof(GUID);
		objectName.MaximumLength = sizeof(GUID);	
		objectName.Buffer = (WCHAR *)&lpFileID->ObjectId;
		break;
		
	default:
		SetLastError( ERROR_INVALID_PARAMETER );
		return INVALID_HANDLE_VALUE;
	}
	
	
	DesiredAccess = dwDesiredAccess | 
		SYNCHRONIZE | FILE_READ_ATTRIBUTES;
	CreateOptions = FILE_OPEN_BY_FILE_ID;
	
	if (dwFlags & FILE_FLAG_WRITE_THROUGH)
		CreateOptions | FILE_WRITE_THROUGH;
	
	if (dwFlags & FILE_FLAG_NO_BUFFERING)
		CreateOptions |= FILE_NO_INTERMEDIATE_BUFFERING;
		
	if (dwFlags & FILE_FLAG_SEQUENTIAL_SCAN) 
		CreateOptions |= FILE_SEQUENTIAL_ONLY;

	if (dwFlags & FILE_FLAG_RANDOM_ACCESS) 
		CreateOptions |= FILE_RANDOM_ACCESS;		
		
	if (dwFlags & FILE_FLAG_BACKUP_SEMANTICS)
	    CreateOptions |= FILE_OPEN_FOR_BACKUP_INTENT;
	
	if (!(dwFlags & FILE_FLAG_OVERLAPPED))
		CreateOptions |= FILE_SYNCHRONOUS_IO_NONALERT;
		
	if(dwFlags & FILE_FLAG_DELETE_ON_CLOSE) {
		DesiredAccess |= DELETE;
		CreateOptions |= FILE_DELETE_ON_CLOSE; }
		
	if(dwFlags & FILE_FLAG_OPEN_REPARSE_POINT)
		CreateOptions |= FILE_OPEN_REPARSE_POINT;
		
	if (dwFlags & FILE_FLAG_OPEN_NO_RECALL)
		CreateOptions |= FILE_OPEN_NO_RECALL;

	attr.Length                   = sizeof(attr);
	attr.RootDirectory            = hFile;
	attr.Attributes               = OBJ_CASE_INSENSITIVE;
	attr.ObjectName               = &objectName;
	attr.SecurityDescriptor       = NULL;
	attr.SecurityQualityOfService = NULL;
	
	status = NtCreateFile( &result, DesiredAccess, &attr, &io, NULL,
	                       0, dwShareMode, FILE_OPEN, CreateOptions, NULL, 0);
	if (status != STATUS_SUCCESS)
	{
		BaseSetLastNTError( status );
	    return INVALID_HANDLE_VALUE;
	}
	return result;
}

// BOOL
// WINAPI
// DECLSPEC_HOTPATCH
// CancelIoEx(
    // HANDLE hFile,
	// LPOVERLAPPED lpOverlapped	
// )
// {
    // NTSTATUS Status;
    // IO_STATUS_BLOCK IoStatusBlock;

    // Status = NtCancelIoFile(hFile, &IoStatusBlock);

    // if ( NT_SUCCESS(Status) ) {
        // return TRUE;
        // }
    // else {
        // BaseSetLastNTError(Status);
        // return FALSE;
        // }

// }

VOID WINAPI CancelIoExInternal(HANDLE dwParam) {
    HANDLE hFile = (HANDLE)dwParam;
    CancelIo(hFile);
}

BOOL WINAPI DECLSPEC_HOTPATCH CancelIoEx(HANDLE hFile, LPOVERLAPPED lpOverlapped) {
    // CancelIoEx can be partially implemented using ntdll APIs to enumerate every thread, plus APCs to
    // call CancelIo on behalf of every thread for this file. However, the lpOverlapped parameter can
    // only be NULL, because there is no known way to iterate all I/O requests without the driver.
    // (This works better on Windows Server 2003 than XP.)
    NTSTATUS Status;
    HANDLE Thread = NULL;
    DWORD Cancelled = 0;
    
    if (lpOverlapped != NULL) {
        TRACE("CancelIoEx: lpOverlapped NOT SUPPORTED! The application might not work as expected!");
        return FALSE;
    }
    
    while (1) {
        Status = NtGetNextThread(NtCurrentProcess(),
            Thread,
            THREAD_SET_CONTEXT,
            0,
            0,
            &Thread);
        if (Status == STATUS_NO_MORE_ENTRIES || Status == STATUS_ACCESS_DENIED) {
            break;
        } else if (!NT_SUCCESS(Status)) {
            return set_ntstatus(Status);
        }
        // queue an APC to call NtCancelIo(hFile) on behalf of thread
        // failures should be fatal only if absolutely no APCs are able to be logged
        if (QueueUserAPC((PAPCFUNC)&CancelIoExInternal, Thread, (ULONG_PTR)hFile))
            Cancelled += 1;
        NtClose(Thread);
    }
    
    if (Cancelled == 0) {
        SetLastError(ERROR_OPERATION_ABORTED);
        return FALSE;
    }
    
    return TRUE;
}

BOOL WINAPI CancelSynchronousIo(HANDLE hThread)
{
    DWORD pid;
	HANDLE hProcess;
	BOOL result;

    if (!hThread)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pid = GetProcessIdOfThread(hThread);
    if (!pid)
        return FALSE;
	
	result = TerminateThread(hThread, (DWORD)-1);

    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess)
        return FALSE;

    result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);

    return result;	
}

BOOL  
WINAPI  
SetStdHandleEx(
  _In_ DWORD  nStdHandle,
  _In_ HANDLE hHandle,
  _In_ PHANDLE oldHandle  
)
{
    if (oldHandle)
        *oldHandle = GetStdHandle(nStdHandle);
    return SetStdHandle(nStdHandle, hHandle);
}

DWORD 
WINAPI 
Wow64SuspendThread(
	HANDLE hThread
)
{ 
#ifdef _M_IX86
	BaseSetLastNTError(STATUS_NOT_IMPLEMENTED);
	return -1;
#elif defined(_M_AMD64)
	return SuspendThread(hThread);
#endif  
}

BOOL 
WINAPI 
Wow64SetThreadContext(
  _In_  HANDLE hThread,
  _In_  const WOW64_CONTEXT *lpContext
)
{ 
#ifdef _M_IX86
	return set_ntstatus( NtSetContextThread( hThread, (const CONTEXT *)lpContext ));
#elif defined(_M_AMD64)
	return SetThreadContext(hThread, (const CONTEXT *)lpContext);
#endif  
}

BOOL 
WINAPI 
Wow64GetThreadContext(
  _In_     HANDLE hThread,
  _Inout_  PWOW64_CONTEXT lpContext
)
{ 
#ifdef _M_IX86
	return set_ntstatus( NtGetContextThread( hThread, (CONTEXT *)lpContext ));
#elif defined(_M_AMD64)
	return GetThreadContext(hThread, (const CONTEXT *)lpContext);
#endif  
}

BOOL 
WINAPI 
Wow64GetThreadSelectorEntry(
  _In_   HANDLE hThread,
  _In_   DWORD dwSelector,
  _Out_  PWOW64_LDT_ENTRY lpSelectorEntry
)
{ 
#ifdef _M_IX86
	BaseSetLastNTError(STATUS_NOT_SUPPORTED);
	return FALSE;
#elif defined(_M_AMD64)
	return GetThreadSelectorEntry(hThread, dwSelector, (LPLDT_ENTRY)lpSelectorEntry);
#endif  
}