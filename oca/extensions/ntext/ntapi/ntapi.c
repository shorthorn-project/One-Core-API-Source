/*++

Copyright (c) 2018 Shorthorn Project

Module Name:

    ntapi.c

Abstract:

    Implement User Mode Nt functions

Author:

    Skulltrail 06-March-2018

Revision History:

--*/

#include <main.h>

static volatile LONG g_FlushCounter = 0;

static NTSTATUS NTAPI NtLoadKeyToFixOffice2013Installer(POBJECT_ATTRIBUTES TargetKey, POBJECT_ATTRIBUTES SourceFile) {
    return NtLoadKey(TargetKey, SourceFile);
}
NTSTATUS 
NTAPI
NtLoadKeyEx(
    IN POBJECT_ATTRIBUTES TargetKey,
    IN POBJECT_ATTRIBUTES SourceFile,
    IN ULONG Flags,
    IN HANDLE TrustClassKey
) 
{
	DbgPrint("NtOpenKeyEx:: parameters ignored %i , %p\n", Flags, TrustClassKey);
    return NtLoadKeyToFixOffice2013Installer(TargetKey, SourceFile);
}

NTSTATUS 
NTAPI
NtQueryOpenSubKeysEx(
	IN POBJECT_ATTRIBUTES TargetKey,
	IN ULONG BufferLength,
	IN PVOID Buffer,
	IN PULONG RequiredSize
)
{
	return NtQueryOpenSubKeys(TargetKey, RequiredSize);
}

NTSTATUS 
WINAPI 
NtUnmapViewOfSectionEx(
	HANDLE handle, 
	PVOID MemoryCache, 
	ULONG number)
{
	return NtUnmapViewOfSection(handle, MemoryCache);	
}

NTSTATUS
NTAPI
NtCreateMutantEx(
  OUT PHANDLE             MutantHandle,
  IN ACCESS_MASK          DesiredAccess,
  IN POBJECT_ATTRIBUTES   ObjectAttributes OPTIONAL,
  IN BOOLEAN              InitialOwner, 
  IN ULONG 				  Flags
)
{
	return NtCreateMutant(MutantHandle, DesiredAccess, ObjectAttributes, InitialOwner);
}

NTSTATUS 
NTAPI 
NtCreateIoCompletionEx(
	__out PHANDLE IoCompletionHandle, 
	__in ACCESS_MASK DesiredAccess, 
	__in_opt POBJECT_ATTRIBUTES ObjectAttributes, 
	__in_opt ULONG Count
)
{
	return NtCreateIoCompletion(IoCompletionHandle, DesiredAccess, ObjectAttributes, Count);
} 

// NTSTATUS 
// NTAPI
// NtRemoveIoCompletionEx(
    // __in HANDLE IoCompletionHandle,
    // __out FILE_IO_COMPLETION_INFORMATION *IoCompletionInformation,
    // __in ULONG Count,
    // __out ULONG *NumEntriesRemoved,
    // __in_opt PLARGE_INTEGER Timeout,
    // __in BOOLEAN Alertable
// )
// {
	// PVOID CompletionKey;
	// PVOID CompletionValue;
	// IO_STATUS_BLOCK IoStatusBlock;
	// NTSTATUS Status;
	// ULONG i = 0;
	
    // for (;;)
    // {
        // while (i < Count)
        // {
			// Status = NtRemoveIoCompletion(IoCompletionHandle,
										 // &CompletionKey,
										 // &CompletionValue,
										 // &IoStatusBlock,
										  // Timeout);
                // if (NT_SUCCESS(Status))
                // {
                    // IoCompletionInformation[i].KeyContext             = CompletionKey;
                    // IoCompletionInformation[i].ApcContext             = CompletionValue;
                    // IoCompletionInformation[i].IoStatusBlock.Information = IoStatusBlock.Information;
                    // IoCompletionInformation[i].IoStatusBlock.Status    = IoStatusBlock.Status;
                // }
            // if (Status != STATUS_SUCCESS) break;
            // ++i;
        // }
        // if (i || Status != STATUS_PENDING)
        // {
            // if (Status == STATUS_PENDING) Status = STATUS_SUCCESS;
            // break;
        // }
        // Status = NtWaitForSingleObject( IoCompletionHandle, Alertable, Timeout );
        // if (Status != WAIT_OBJECT_0) break;
    // }
    // *NumEntriesRemoved = i ? i : 1;
    // return Status;
// }

NTSTATUS 
NTAPI 
NtCreateSemaphoreEx(
  IN OPTIONAL   HANDLE lpSemaphoreAttributes,
  IN       LONG                  lInitialCount,
  IN       LONG                  lMaximumCount,
  IN OPTIONAL  LPCTSTR               lpName,
  IN 	   DWORD                 dwFlags,
  IN       DWORD                 dwDesiredAccess
)
{
	return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI 
NtCreateKeyTransacted(
	__out PHANDLE KeyHandle, 
	__in ACCESS_MASK DesiredAccess, 
	__in POBJECT_ATTRIBUTES ObjectAttributes, 
	ULONG TitleIndex, 
	__in_opt PUNICODE_STRING Class, 
	__in ULONG CreateOptions, 
	__in HANDLE TransactionHandle, 
	__out_opt PULONG Disposition)
{
	return NtCreateKey(KeyHandle, 
					   DesiredAccess, 
					   ObjectAttributes, 
					   TitleIndex, 
					   Class, 
					   CreateOptions, 
					   Disposition);
}

NTSTATUS 
NTAPI 
NtOpenKeyTransacted(
	__out PHANDLE KeyHandle, 
	__in ACCESS_MASK DesiredAccess, 
	__in POBJECT_ATTRIBUTES ObjectAttributes, 
	__in HANDLE TransactionHandle)
{
	return NtOpenKey(KeyHandle, 
					 DesiredAccess, 
					 ObjectAttributes);
}

/* Section Unimplemented*/
NTSTATUS
NTAPI
NtOpenPrivateNamespace(
  __out PHANDLE NamespaceHandle,
  __in ACCESS_MASK DesiredAccess,
  __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
  __in PVOID BoundaryDescriptor
)
{
	return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI 
NtSetInformationVirtualMemory(HANDLE hProcess,
							  BOOL access,
							  ULONG_PTR NumberOfEntries, 
							  PMEMORY_RANGE_ENTRY VirtualAddresses, 
							  PULONG Flags,
							  DWORD other
)
{
	return STATUS_SUCCESS;
}	

NTSTATUS 
NTAPI 	
NtReplacePartitionUnit(__in PUNICODE_STRING TargetInstancePath, 
					   __in PUNICODE_STRING SpareInstancePath,
					   __in ULONG Flags)
{
	return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI 
NtDeletePrivateNamespace(
  __in HANDLE NamespaceHandle
)
{
	return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI 
NtCreatePrivateNamespace(
  __out PHANDLE NamespaceHandle,
  __in ACCESS_MASK DesiredAccess,
  __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
  __in PVOID BoundaryDescriptor
)
{
	return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
NtTraceControl(
	ULONG FunctionCode, 
	PVOID InBuffer, 
	ULONG InBufferLen, 
	PVOID OutBuffer, 
	ULONG OutBufferLen, 
	PULONG ReturnLength
)
{
	return STATUS_UNSUCCESSFUL;
}

VOID
NTAPI
NtFlushProcessWriteBuffers(VOID)
{
    SYSTEM_BASIC_INFORMATION sbi;
    NTSTATUS status;
    LONG current;
    ULONG i;

    status = ZwQuerySystemInformation(SystemBasicInformation,
                                      &sbi,
                                      sizeof(sbi),
                                      NULL);

    if (!NT_SUCCESS(status) || sbi.NumberOfProcessors <= 1)
    {
#if defined(_M_IX86)
        __asm { lock add dword ptr [esp], 0 }
#elif defined(_M_AMD64)
        _mm_mfence();
#else
        MemoryBarrier();
#endif
        return;
    }

    current = InterlockedIncrement(&g_FlushCounter);

    for (i = 0; i < sbi.NumberOfProcessors; i++)
    {
#if defined(_M_IX86)
        __asm { lock add dword ptr [esp], 0 }
#elif defined(_M_AMD64)
        _mm_mfence();
#else
        MemoryBarrier();
#endif
    }

#if defined(_M_IX86)
    __asm { lock add dword ptr [esp], 0 }
#elif defined(_M_AMD64)
    _mm_mfence();
#else
    MemoryBarrier();
#endif
}

NTAPI 
NtSetSystemPowerState( 	
	IN POWER_ACTION  	SystemAction,
	IN SYSTEM_POWER_STATE  	MinSystemState,
	IN ULONG  	Flags 
)
{
	return STATUS_SUCCESS;
} 	

// NTSTATUS 
// WINAPI 
// NtCreateThreadEx(PHANDLE hThread,
    // ACCESS_MASK DesiredAccess,
    // POBJECT_ATTRIBUTES ObjectAttributes,
    // HANDLE ProcessHandle,
    // LPTHREAD_START_ROUTINE lpStartAddress,
    // LPVOID lpParameter,
    // ULONG CreateFlags,
    // ULONG StackZeroBits,
    // ULONG SizeOfStackCommit,
    // ULONG SizeOfStackReserve,
    // LPVOID lpBytesBuffer)
// {
    // CLIENT_ID clientId;
    
    // NTSTATUS status = RtlCreateUserThread(ProcessHandle, 
                                          // ObjectAttributes ? ObjectAttributes->SecurityDescriptor : NULL,
                                          // CreateFlags & THREAD_CREATE_FLAGS_CREATE_SUSPENDED,
                                          // StackZeroBits,
                                          // SizeOfStackReserve,
                                          // SizeOfStackCommit,
                                          // lpStartAddress, 
                                          // lpParameter, 
                                          // hThread, 
                                          // &clientId);
    
    
    // return status;
// }

NTSTATUS
NTAPI
RtlpCreateUserStack(IN HANDLE ProcessHandle,
                    IN SIZE_T StackReserve OPTIONAL,
                    IN SIZE_T StackCommit OPTIONAL,
                    IN ULONG StackZeroBits OPTIONAL,
                    OUT PINITIAL_TEB InitialTeb)
{
    NTSTATUS Status;
    SYSTEM_BASIC_INFORMATION SystemBasicInfo;
    PIMAGE_NT_HEADERS Headers;
    ULONG_PTR Stack;
    BOOLEAN UseGuard;
    ULONG Dummy;
    SIZE_T MinimumStackCommit, GuardPageSize;

    /* Get some memory information */
    Status = ZwQuerySystemInformation(SystemBasicInformation,
                                      &SystemBasicInfo,
                                      sizeof(SYSTEM_BASIC_INFORMATION),
                                      NULL);
    if (!NT_SUCCESS(Status)) return Status;

    /* Use the Image Settings if we are dealing with the current Process */
    if (ProcessHandle == NtCurrentProcess())
    {
        /* Get the Image Headers */
        Headers = RtlImageNtHeader(NtCurrentPeb()->ImageBaseAddress);
        if (!Headers) return STATUS_INVALID_IMAGE_FORMAT;

        /* If we didn't get the parameters, find them ourselves */
        if (StackReserve == 0)
            StackReserve = Headers->OptionalHeader.SizeOfStackReserve;
        if (StackCommit == 0)
            StackCommit = Headers->OptionalHeader.SizeOfStackCommit;

        MinimumStackCommit = NtCurrentPeb()->MinimumStackCommit;
        if ((MinimumStackCommit != 0) && (StackCommit < MinimumStackCommit))
        {
            StackCommit = MinimumStackCommit;
        }
    }
    else
    {
        /* Use the System Settings if needed */
        if (StackReserve == 0)
            StackReserve = SystemBasicInfo.AllocationGranularity;
        if (StackCommit == 0)
            StackCommit = SystemBasicInfo.PageSize;
    }

    /* Check if the commit is higher than the reserve */
    if (StackCommit >= StackReserve)
    {
        /* Grow the reserve beyond the commit, up to 1MB alignment */
        StackReserve = ROUND_UP(StackCommit, 1024 * 1024);
    }

    /* Align everything to Page Size */
    StackCommit = ROUND_UP(StackCommit, SystemBasicInfo.PageSize);
    StackReserve = ROUND_UP(StackReserve, SystemBasicInfo.AllocationGranularity);

    /* Reserve memory for the stack */
    Stack = 0;
    Status = ZwAllocateVirtualMemory(ProcessHandle,
                                     (PVOID*)&Stack,
                                     StackZeroBits,
                                     &StackReserve,
                                     MEM_RESERVE,
                                     PAGE_READWRITE);
    if (!NT_SUCCESS(Status)) return Status;

    /* Now set up some basic Initial TEB Parameters */
    InitialTeb->AllocatedStackBase = (PVOID)Stack;
    InitialTeb->StackBase = (PVOID)(Stack + StackReserve);
    InitialTeb->PreviousStackBase = NULL;
    InitialTeb->PreviousStackLimit = NULL;

    /* Update the stack position */
    Stack += StackReserve - StackCommit;

    /* Check if we can add a guard page */
    if (StackReserve >= StackCommit + SystemBasicInfo.PageSize)
    {
        Stack -= SystemBasicInfo.PageSize;
        StackCommit += SystemBasicInfo.PageSize;
        UseGuard = TRUE;
    }
    else
    {
        UseGuard = FALSE;
    }

    /* Allocate memory for the stack */
    Status = ZwAllocateVirtualMemory(ProcessHandle,
                                     (PVOID*)&Stack,
                                     0,
                                     &StackCommit,
                                     MEM_COMMIT,
                                     PAGE_READWRITE);
    if (!NT_SUCCESS(Status))
    {
        GuardPageSize = 0;
        ZwFreeVirtualMemory(ProcessHandle, (PVOID*)&Stack, &GuardPageSize, MEM_RELEASE);
        return Status;
    }

    /* Now set the current Stack Limit */
    InitialTeb->StackLimit = (PVOID)Stack;

    /* Create a guard page if needed */
    if (UseGuard)
    {
        GuardPageSize = SystemBasicInfo.PageSize;
        Status = ZwProtectVirtualMemory(ProcessHandle,
                                        (PVOID*)&Stack,
                                        &GuardPageSize,
                                        PAGE_GUARD | PAGE_READWRITE,
                                        &Dummy);
        if (!NT_SUCCESS(Status)) return Status;

        /* Update the Stack Limit keeping in mind the Guard Page */
        InitialTeb->StackLimit = (PVOID)((ULONG_PTR)InitialTeb->StackLimit +
                                         GuardPageSize);
    }

    /* We are done! */
    return STATUS_SUCCESS;
}

VOID
NTAPI
RtlpFreeUserStack(IN HANDLE ProcessHandle,
                  IN PINITIAL_TEB InitialTeb)
{
    SIZE_T Dummy = 0;

    /* Free the Stack */
    ZwFreeVirtualMemory(ProcessHandle,
                        &InitialTeb->AllocatedStackBase,
                        &Dummy,
                        MEM_RELEASE);

    /* Clear the initial TEB */
    RtlZeroMemory(InitialTeb, sizeof(*InitialTeb));
}

NTSTATUS
NTAPI
RtlCreateUserThreadInternal(IN HANDLE ProcessHandle,
					IN DWORD DesiredAccess,
                    IN POBJECT_ATTRIBUTES ThreadObjectAttributes OPTIONAL,
                    IN BOOLEAN CreateSuspended,
                    IN ULONG StackZeroBits OPTIONAL,
                    IN SIZE_T StackReserve OPTIONAL,
                    IN SIZE_T StackCommit OPTIONAL,
                    IN PTHREAD_START_ROUTINE StartAddress,
                    IN PVOID Parameter OPTIONAL,
                    OUT PHANDLE ThreadHandle OPTIONAL,
                    OUT PCLIENT_ID ClientId OPTIONAL)
{
    NTSTATUS Status;
    HANDLE Handle;
    CLIENT_ID ThreadCid;
    INITIAL_TEB InitialTeb;
    OBJECT_ATTRIBUTES ObjectAttributes;
    CONTEXT Context;

    /* First, we'll create the Stack */
    Status = RtlpCreateUserStack(ProcessHandle,
                                 StackReserve,
                                 StackCommit,
                                 StackZeroBits,
                                 &InitialTeb);
    if (!NT_SUCCESS(Status)) return Status;

    /* Next, we'll set up the Initial Context */
    RtlInitializeContext(ProcessHandle,
                         &Context,
                         Parameter,
                         StartAddress,
                         InitialTeb.StackBase);
	
    /* We are now ready to create the Kernel Thread Object */
	if (ThreadObjectAttributes) {
		memmove(&ObjectAttributes, ThreadObjectAttributes, sizeof(OBJECT_ATTRIBUTES));
	} else {
		// matches NT5 ntdll.dll
		InitializeObjectAttributes(&ObjectAttributes,
                               NULL,
                               OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);
	}
	
    Status = ZwCreateThread(&Handle,
                            DesiredAccess,
                            &ObjectAttributes,
                            ProcessHandle,
                            &ThreadCid,
                            &Context,
                            &InitialTeb,
                            CreateSuspended);
    if (!NT_SUCCESS(Status))
    {
        /* Free the stack */
        RtlpFreeUserStack(ProcessHandle, &InitialTeb);
    }
    else
    {
        /* Return thread data */
        if (ThreadHandle)
            *ThreadHandle = Handle;
        else
            NtClose(Handle);
        if (ClientId) *ClientId = ThreadCid;
    }

    /* Return success or the previous failure */
    return Status;
}

//Implements the AttributeList required by api monitor's usages

NTSTATUS 
WINAPI 
NtCreateThreadEx(PHANDLE hThread,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE ProcessHandle,
    LPTHREAD_START_ROUTINE lpStartAddress,
    LPVOID lpParameter,
    ULONG CreateFlags,
    ULONG StackZeroBits,
    ULONG SizeOfStackCommit,
    ULONG SizeOfStackReserve,
    PPS_ATTRIBUTE_LIST AttributeList)
{
	CLIENT_ID clientId;
	PPS_ATTRIBUTE attr;
	THREAD_BASIC_INFORMATION BaseInformation;
	ULONG ReturnLength;
	NTSTATUS status2;
	int i;
	NTSTATUS status;
	
    status = RtlCreateUserThreadInternal(
										ProcessHandle, 
										DesiredAccess,
                                        ObjectAttributes,
                                        CreateFlags & THREAD_CREATE_FLAGS_CREATE_SUSPENDED,
                                        StackZeroBits,
										SizeOfStackReserve,
                                        SizeOfStackCommit,
                                        lpStartAddress, 
                                        lpParameter, 
                                        hThread, 
                                        &clientId);
	
	DbgPrint("NtCreateThreadEx status: %p\n", status);
	
	if (NT_SUCCESS(status) && AttributeList) {
		for (i = 0; i < (AttributeList->TotalLength - sizeof(SIZE_T)) / sizeof(PS_ATTRIBUTE); i++) {
			attr = &AttributeList->Attributes[i];
			DbgPrint("NtCreateThreadEx: attempting to handle attribute %i!\n", attr->Attribute);
			switch (attr->Attribute) {
				case PS_ATTRIBUTE_CLIENT_ID: // 65539
					DbgPrint("CLIENT_ID Size: %i Copying To %p\n", attr->Size, attr->ValuePtr);
					if (attr->Size >= sizeof(CLIENT_ID)) 
						memmove(attr->ValuePtr, &clientId, sizeof(CLIENT_ID));
					attr->ReturnLength = (SIZE_T*)sizeof(CLIENT_ID);
				case PS_ATTRIBUTE_TEB_ADDRESS: // 65540
					status2 = NtQueryInformationThread(*hThread, ThreadBasicInformation, &BaseInformation, sizeof(BaseInformation), &ReturnLength);
					DbgPrint("PS_ATTRIBUTE_TEB_ADDRESS At %p, Status %p\n", BaseInformation.TebBaseAddress, status2);
					if (NT_SUCCESS(status2)) {
						((PVOID*)attr->ValuePtr) = BaseInformation.TebBaseAddress;
						attr->ReturnLength = (SIZE_T*)sizeof(PVOID);
					}
			}
		}
	}
	
	DbgPrint("Good to go!\n");
	
    return status;
}

NTSTATUS 
NTAPI
NtCreateTransaction(
  _Out_    PHANDLE            TransactionHandle,
  _In_     ACCESS_MASK        DesiredAccess,
  _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
  _In_opt_ LPGUID             Uow,
  _In_opt_ HANDLE             TmHandle,
  _In_opt_ ULONG              CreateOptions,
  _In_opt_ ULONG              IsolationLevel,
  _In_opt_ ULONG              IsolationFlags,
  _In_opt_ PLARGE_INTEGER     Timeout,
  _In_opt_ PUNICODE_STRING    Description
)
{
	*TransactionHandle = NULL;
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS 
NTAPI
NtQueryInformationTransaction(
  _In_      HANDLE                        TransactionHandle,
  _In_      TRANSACTION_INFORMATION_CLASS TransactionInformationClass,
  _Out_     PVOID                         TransactionInformation,
  _In_      ULONG                         TransactionInformationLength,
  _Out_opt_ PULONG                        ReturnLength
)
{
	TransactionInformation = NULL;
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS 
NTAPI
NtSetInformationTransaction(
  _In_ HANDLE                        TransactionHandle,
  _In_ TRANSACTION_INFORMATION_CLASS TransactionInformationClass,
  _In_ PVOID                         TransactionInformation,
  _In_ ULONG                         TransactionInformationLength
)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS 
NTAPI
NtRollbackTransaction(
  _In_ HANDLE  TransactionHandle,
  _In_ BOOLEAN Wait
)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS 
NTAPI
NtCommitTransaction(
  _In_ HANDLE  TransactionHandle,
  _In_ BOOLEAN Wait
)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS 
NTAPI
NtOpenTransaction(
  _Out_    PHANDLE            TransactionHandle,
  _In_     ACCESS_MASK        DesiredAccess,
  _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
  _In_     LPGUID             Uow,
  _In_opt_ HANDLE             TmHandle
)
{
	*TransactionHandle = NULL;
	return STATUS_NOT_IMPLEMENTED;
}

static 
NTSTATUS 
NTAPI 
NtOpenKeyToFixOffice2013Installer(PHANDLE retkey, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr) {
    return NtOpenKey( retkey, access, attr);
}

NTSTATUS 
WINAPI 
NtOpenKeyEx( 
	PHANDLE retkey, 
	ACCESS_MASK access, 
	const OBJECT_ATTRIBUTES *attr, 
	ULONG options 
)
{
	DbgPrint("NtOpenKeyEx:: parameters ignored %i , %p\n", options, retkey);
    return NtOpenKeyToFixOffice2013Installer( retkey, access, attr);
}

/**************************************************************************
 *           NtCancelIoFileEx   (NTDLL.@)
 */
NTSTATUS 
WINAPI 
NtCancelIoFileEx( 
	HANDLE handle, 
	IO_STATUS_BLOCK *io, 
	IO_STATUS_BLOCK *io_status 
)
{
	/*Ignoring io parameter for now*/
	UNREFERENCED_PARAMETER(io);
	return NtCancelIoFile(handle, io_status);
}

/***********************************************************************
 *             NtRemoveIoCompletionEx (NTDLL.@)
 */
NTSTATUS WINAPI NtRemoveIoCompletionEx( HANDLE handle, FILE_IO_COMPLETION_INFORMATION *info, ULONG count,
                                        ULONG *written, LARGE_INTEGER *timeout, BOOLEAN alertable )
{
    NTSTATUS status;
    ULONG i = 0;
	PVOID CompletionKey = 0;
	PVOID CompletionValue = 0;
	PIO_STATUS_BLOCK IoStatusBlock = {0};

    for (;;)
    {
        while (i < count)
        {
			status = NtRemoveIoCompletion(handle, CompletionKey, CompletionValue, IoStatusBlock, timeout);			
            if(status == STATUS_SUCCESS)
			{
				info[i].KeyContext             = CompletionKey;
				info[i].ApcContext             = CompletionValue;
				info[i].IoStatusBlock.Information = IoStatusBlock->Information;
				info[i].IoStatusBlock.Status    = IoStatusBlock->Status;   
			}				
			if (status != STATUS_SUCCESS) break;
            ++i;
        }
        if (i || status != STATUS_PENDING)
        {
            if (status == STATUS_PENDING) status = STATUS_SUCCESS;
            break;
        }
        status = NtWaitForSingleObject( handle, alertable, timeout );
        if (status != WAIT_OBJECT_0) break;
    }
    *written = i ? i : 1;
    return status;
}