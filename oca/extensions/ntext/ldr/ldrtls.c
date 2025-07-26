/*++

Copyright (c) 2025  Shorthorn Project

Module Name:

    ldrtls.c

Abstract:

    This module implements loader thread local storage functions.

Author:

    Skulltrail 22-July-2025

Revision History:

--*/

#pragma warning(disable:4214)   // bit field types other than int
#pragma warning(disable:4201)   // nameless struct/union
#pragma warning(disable:4115)   // named type definition in parentheses
#pragma warning(disable:4127)   // condition expression is constant
#pragma warning(disable:4152)   // condition expression is constant

#include <main.h>
#include <ldrtls.h>
#include <tlhelp32.h>

#if defined(_AMD64_)
#define MAXINDEX 0xFFFFFFFFFFFFFFFF
typedef ULONG64 BITMAP_INDEX, *PBITMAP_INDEX;
typedef ULONG64 BITMAP_BUFFER, *PBITMAP_BUFFER;
#define LDRP_BITMAP_INCREMENT (0x23 - sizeof( PVOID ))
#else
#define _BITCOUNT 32
#define MAXINDEX 0xFFFFFFFF
typedef ULONG BITMAP_INDEX, *PBITMAP_INDEX;
typedef ULONG BITMAP_BUFFER, *PBITMAP_BUFFER;	
#define LDRP_BITMAP_INCREMENT (0x27 - sizeof( PVOID ))
#endif

//#define TH32CS_SNAPTHREAD   0x00000004
#define BUFFER_SIZE 64*1024
#define RtlpGetCurrentProcessId() (HandleToUlong(NtCurrentTeb()->ClientId.UniqueProcess))
#define TLS_TAG 3
#define MAKE_TAG(tag)   ( (ULONG)(tag) )

extern ULONG NtdllBaseTag;
LIST_ENTRY LdrpTlsList;
RTL_BITMAP LdrpTlsBitmap;
LONG LdrpActiveThreadCount = 0;
ULONG LdrpPotentialTlsLeaks = 0;
TLS_RECLAIM_TABLE_ENTRY LdrpDelayedTlsReclaimTable[ 16 ];
ULONG LdrpStaticTlsBitmapVector[ 4 ];
ULONG LdrpActualBitmapSize = 0;
RTL_SRWLOCK LdrpTlsLock = {NULL};

HANDLE
WINAPI
LdrCreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID);

BOOL
WINAPI
LdrThread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte);

BOOL
WINAPI
LdrThread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte);

NTSTATUS
NtSetInformationProcessHook(
    __in HANDLE ProcessHandle,
    __in PROCESSINFOCLASS ProcessInformationClass,
    __in_bcount(ProcessInformationLength) PVOID ProcessInformation,
    __in ULONG ProcessInformationLength
);

VOID
LdrpReleaseTlsIndex(
	__in ULONG TlsIndex
	)
{
	RtlClearBit(
		&LdrpTlsBitmap,
		TlsIndex
		);
}

NTSTATUS 
LdrpAcquireTlsIndex(
	PULONG TlsIndex, 
	PBOOLEAN AllocatedBitmap
)
{
  ULONG Length; // esi
  ULONG Index; // eax
  PULONG NewBitmapBuffer; // ebx

  Length = LdrpTlsBitmap.SizeOfBitMap;
  if ( !LdrpTlsBitmap.SizeOfBitMap )
  {
    RtlInitializeBitMap(&LdrpTlsBitmap, LdrpStaticTlsBitmapVector, 4);
    LdrpActualBitmapSize = 1;
LABEL_3:
    RtlClearBits(&LdrpTlsBitmap, Length + 1, 3);
    RtlSetBit(&LdrpTlsBitmap, Length);
    *TlsIndex = Length;
    *AllocatedBitmap = 1;
    return STATUS_SUCCESS;
  }
  Index = RtlFindClearBitsAndSet(&LdrpTlsBitmap, 1, 0);
  if ( Index != -1 )
  {
    *TlsIndex = Index;
    *AllocatedBitmap = 0;
    return STATUS_SUCCESS;
  }
  if ( (LdrpTlsBitmap.SizeOfBitMap + 35) >> 5 <= LdrpActualBitmapSize )
  {
    LdrpTlsBitmap.SizeOfBitMap += 4;
    goto LABEL_3;
  }
  LdrpActualBitmapSize = (Length + 35) >> 5;
  NewBitmapBuffer = (PULONG)RtlAllocateHeap(
                              NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap,
                              NtdllBaseTag + 786432,
                              4 * ((Length + 35) >> 5));
  if ( NewBitmapBuffer )
  {
    memcpy(NewBitmapBuffer, LdrpTlsBitmap.Buffer, (Length + 7) >> 3);
    if (LdrpTlsBitmap.Buffer != LdrpStaticTlsBitmapVector )
      RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, LdrpTlsBitmap.Buffer);
    RtlInitializeBitMap(&LdrpTlsBitmap, NewBitmapBuffer, Length + 4);
    goto LABEL_3;
  }
  return STATUS_NO_MEMORY;
}

NTSTATUS 
LdrpAllocateTlsEntry(
    PIMAGE_TLS_DIRECTORY TlsDirectory,
    PLDR_DATA_TABLE_ENTRY ModuleEntry,
    PULONG TlsIndex,
    PBOOLEAN AllocatedBitmap,
    PTLS_ENTRY *TlsEntry
)
{
	PTLS_ENTRY Entry; // ebx MAPDST
	ULONG NewIndex; // eax
	NTSTATUS Status; // [esp+1Ch] [ebp-1Ch]

	Entry = (PTLS_ENTRY)RtlAllocateHeap(
							NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap,
							MAKE_TAG( TLS_TAG ),
							sizeof( TLS_ENTRY ));
	if ( !Entry )
		return STATUS_NO_MEMORY;
	
	RtlCopyMemory(&Entry->TlsDirectory, TlsDirectory, sizeof(Entry->TlsDirectory));
	
	if ( Entry->TlsDirectory.EndAddressOfRawData < Entry->TlsDirectory.StartAddressOfRawData )
	{
		RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, Entry);
		return STATUS_INVALID_IMAGE_FORMAT;
	}
	
	Entry->ModuleEntry = ModuleEntry;
	
	//
	// Insert the entry into our list.
	//

	InsertTailList(
		&LdrpTlsList,
		&Entry->TlsEntryLinks
		);
	if ( AllocatedBitmap )
	{
		Status = LdrpAcquireTlsIndex(TlsIndex, AllocatedBitmap);
		if ( !NT_SUCCESS(Status) )
		{
			RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, Entry);
			return Status;			
		}
		NewIndex = *TlsIndex;
	}else{
		NewIndex = (*TlsIndex)++;
	}
	
	//
	// We reuse the 'Characteristics' field for the real TLS index.
	//
	Entry->TlsDirectory.Characteristics = NewIndex;
	*(PULONG)Entry->TlsDirectory.AddressOfIndex = NewIndex;
	if ( TlsEntry )
		*TlsEntry = Entry;
	return STATUS_SUCCESS;
}

PVOID *
FASTCALL
LdrpGetNewTlsVector(
	__in ULONG TlsBitmapLength
	)
{
	PTLS_VECTOR TlsVector;

	TlsVector = (PTLS_VECTOR)RtlAllocateHeap(
		RtlProcessHeap(),
		MAKE_TAG( TLS_TAG ),
		sizeof( TLS_VECTOR ) + (sizeof( PVOID ) * TlsBitmapLength) -
			sizeof( PVOID )
		);

	if (!TlsVector)
		return 0;

	TlsVector->Length = TlsBitmapLength;

	RtlZeroMemory(
		TlsVector->ModuleTlsData,
		TlsBitmapLength * sizeof( PVOID )
		);

	return TlsVector->ModuleTlsData;
}

PTLS_ENTRY
FASTCALL
LdrpFindTlsEntry(
	__in PLDR_DATA_TABLE_ENTRY ModuleEntry
	)
{
	PTLS_ENTRY  TlsEntry;
	PLIST_ENTRY ListHead;

	ListHead = &LdrpTlsList;

	for (TlsEntry = CONTAINING_RECORD(
	         LdrpTlsList.Flink,
			 TLS_ENTRY,
			 TlsEntryLinks
			 );
	     &TlsEntry->TlsEntryLinks != ListHead;
		 TlsEntry = CONTAINING_RECORD(
	         TlsEntry->TlsEntryLinks.Flink,
			 TLS_ENTRY,
			 TlsEntryLinks
			 )
	    )
	{
		if (TlsEntry->ModuleEntry == ModuleEntry)
			return TlsEntry;
	}

	return 0;
}

NTSTATUS
LdrpReleaseTlsEntry(
	__in PLDR_DATA_TABLE_ENTRY ModuleEntry
	)
{
	PTLS_ENTRY TlsEntry;

	//
	// Find the corresponding TLS_ENTRY for this module entry.
	//

	TlsEntry = LdrpFindTlsEntry(
		ModuleEntry
		);

	if (!TlsEntry)
		return STATUS_NOT_FOUND;

	//
	// Remove it from the global list of outstanding TLS entries.
	//

	RemoveEntryList(
		&TlsEntry->TlsEntryLinks
		);

	//
	// Deallocate the TLS index.
	//

	LdrpReleaseTlsIndex(
		TlsEntry->TlsDirectory.Characteristics
		);

	//
	// Deallocate the TLS_ENTRY object itself.
	//

	RtlFreeHeap(
		RtlProcessHeap(),
		0,
		TlsEntry
		);

	//
	// We're done.
	//

	return STATUS_SUCCESS;
}

VOID
LdrpQueueDeferredTlsData(
	__inout PVOID TlsVector,
	__inout PVOID ThreadId
	)
{
	PTLS_VECTOR              RealTlsVector;
	PTLS_RECLAIM_TABLE_ENTRY ReclaimEntry;

	RealTlsVector = CONTAINING_RECORD(
		TlsVector,
		TLS_VECTOR,
		ModuleTlsData
		);

	RealTlsVector->ThreadId = ThreadId;

	ReclaimEntry = &LdrpDelayedTlsReclaimTable[ ((ULONG_PTR)(ThreadId) >> 2) & 0xF ];

	RtlAcquireSRWLockExclusive(
		&ReclaimEntry->Lock
		);

	RealTlsVector->PreviousDeferredTlsVector = ReclaimEntry->TlsVector;
	ReclaimEntry->TlsVector = RealTlsVector;

	RtlReleaseSRWLockExclusive(
		&ReclaimEntry->Lock
		);
}

NTSTATUS
LdrpHandleTlsData(
	__inout PLDR_DATA_TABLE_ENTRY ModuleEntry
	)
{
	PIMAGE_TLS_DIRECTORY      TlsDirectory;
	ULONG                     DirectorySize;
	ULONG                     TlsIndex;
	HANDLE                    Heap;
	PPROCESS_TLS_INFORMATION  TlsInfo;
	PROCESS_TLS_INFORMATION   OneThreadTlsInfo;
	NTSTATUS                  Status;
	BOOLEAN                   AllocatedBitmap;
	PTLS_ENTRY                TlsEntry;
	ULONG                     TlsBitmapLength;
	SIZE_T                    TlsRawDataLength;
	ULONG                     ThreadIndex;
	PVOID                     TlsData = 0;
	PVOID                    *TlsVector;
	PTHREAD_TLS_INFORMATION   ResultTlsInformation;
	ULONG                     ThreadsCleanedUp;
	
	DbgPrint("LdrpHandleTlsData called\n");

	if (LdrpActiveThreadCount == 0)
	{
		return STATUS_SUCCESS;
	}

	//
	// Discover the TLS directory address for this module.
	//
#if defined (AMD64)
	 if(!NT_SUCCESS(RtlpImageDirectoryEntryToDataEx(
		ModuleEntry->DllBase,
		TRUE,
		IMAGE_DIRECTORY_ENTRY_TLS,
		&DirectorySize,
		&TlsDirectory
		)) || !TlsDirectory)
		{
			return STATUS_SUCCESS;
		};	
#else
	TlsDirectory = (PIMAGE_TLS_DIRECTORY)RtlImageDirectoryEntryToData(
		ModuleEntry->DllBase,
		TRUE,
		IMAGE_DIRECTORY_ENTRY_TLS,
		&DirectorySize
		);
		
	//
	// If we've got no TLS directory then we're done.
	//

	if (!TlsDirectory)
		return STATUS_SUCCESS;		
#endif	



	//
	// We'll be using the process heap.
	//

	Heap = RtlProcessHeap();

	//
	// We've got an optimization for one active thread, which is the case for
	// traditional static-link DLLs that use __declspec(thread).
	//

	if (LdrpActiveThreadCount == 1)
		TlsInfo = &OneThreadTlsInfo;
	else
	{
		//
		// Otherwise, allocate memory for our thread data block.
		//

		TlsInfo = (PPROCESS_TLS_INFORMATION)RtlAllocateHeap(
			Heap,
			MAKE_TAG( TLS_TAG ),
			LdrpActiveThreadCount * sizeof( THREAD_TLS_INFORMATION ) +
				sizeof( PROCESS_TLS_INFORMATION ) - sizeof( THREAD_TLS_INFORMATION )
			);

		if (!TlsInfo)
		{
			DbgPrint("LdrpHandleTlsData:: cannot allocate TlsInfo\n");
			return STATUS_NO_MEMORY;
		}
	}

	do
	{
		//
		// Allocate a TLS index (or a new TLS bitmap).
		//

		TlsBitmapLength = LdrpTlsBitmap.SizeOfBitMap;

		Status = LdrpAllocateTlsEntry(
			TlsDirectory,
			ModuleEntry,
			&TlsIndex,
			&AllocatedBitmap,
			&TlsEntry
			);

		if (!NT_SUCCESS( Status ))
		{
			DbgPrint("LdrpHandleTlsData:: LdrpAllocateTlsEntry returned %08lx\n", Status);
			break;
		}
			

		TlsInfo->ThreadDataCount = LdrpActiveThreadCount;

		if (AllocatedBitmap)
		{
			TlsInfo->OperationType   = ProcessTlsReplaceVector;
			TlsInfo->TlsVectorLength = TlsBitmapLength;

			TlsBitmapLength = LdrpTlsBitmap.SizeOfBitMap;
		}
		else
		{
			TlsInfo->OperationType = ProcessTlsReplaceIndex;
			TlsInfo->TlsIndex      = TlsIndex;
		}

		Status           = STATUS_SUCCESS;
		ThreadsCleanedUp = 0;

		//
		// Calculate the size of the raw TLS data for this module.
		//

		TlsRawDataLength = TlsEntry->TlsDirectory.EndAddressOfRawData -
			TlsEntry->TlsDirectory.StartAddressOfRawData;

		//
		// Prepare data for each running thread.
		//

		for (ThreadIndex = 0;
		     ThreadIndex < TlsInfo->ThreadDataCount;
		     ThreadIndex += 1)
		{
			//
			// Allocate the TLS memory block for this thread...
			//

			TlsData = RtlAllocateHeap(
				Heap,
				MAKE_TAG( TLS_TAG ),
				TlsRawDataLength
				);

			if (!TlsData)
			{
				DbgPrint("LdrpHandleTlsData:: cannot allocate TlsData\n");
				Status = STATUS_NO_MEMORY;
				break;
			}

			//
			// Copy the initializer raw data into it.
			//

			__try
			{
				RtlCopyMemory(
					TlsData,
					(PVOID)TlsEntry->TlsDirectory.StartAddressOfRawData,
					TlsRawDataLength
					);
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				Status = GetExceptionCode();
			}

			if (!NT_SUCCESS( Status ))
			{
				DbgPrint("LdrpHandleTlsData:: Excepetion when trying copying TlsData %08lx\n", Status);
				RtlFreeHeap(
					Heap,
					0,
					TlsData
					);

				break;
			}

			if (AllocatedBitmap)
			{
				TlsVector = LdrpGetNewTlsVector(
					TlsBitmapLength
					);

				if (!TlsVector)
				{
					RtlFreeHeap(
						Heap,
						0,
						TlsData
						);

					break;
				}
				
				TlsVector[ TlsIndex ] = TlsData;

				TlsInfo->ThreadData[ ThreadIndex ].TlsVector = TlsVector;
			}
			else
			{
				TlsInfo->ThreadData[ ThreadIndex ].TlsModulePointer = TlsData;
			}

			TlsInfo->ThreadData[ ThreadIndex ].Flags = 0;
		}

		//
		// This is awkward; all the 'break' above really are either goto or
		// __leave, but we aren't using those.  This is really supposed to
		// just happen on normal for loop exit.
		//

		if (ThreadIndex >= TlsInfo->ThreadDataCount)
		{
			TlsInfo->Reserved = 0;

			//
			// Perform the actual work of swapping the thread TLS data.
			//
			Status = NtSetInformationProcessHook(
				NtCurrentProcess(),
				ProcessTlsInformation,
				TlsInfo,
				TlsInfo->ThreadDataCount * sizeof( THREAD_TLS_INFORMATION ) +
					sizeof( PROCESS_TLS_INFORMATION ) - sizeof( THREAD_TLS_INFORMATION )
				);

			if(!NT_SUCCESS(Status)){
				DbgPrint("LdrpHandleTlsData:: NtSetInformationProcess Status %08lx\n", Status);					
			}
		}
		
        do 
        {
			--ThreadIndex;
			
            ResultTlsInformation = &TlsInfo->ThreadData[ThreadIndex];

            if (ResultTlsInformation->Flags & 0x1)
            {
                LdrpPotentialTlsLeaks += 1;
              
                // I think we are supposed to increment this here but the windows loader doesn't seem to do it so ¯\_(ツ)_/¯

                continue;
            }

            if (!ResultTlsInformation->TlsVector)
                continue;

            if ((ResultTlsInformation->Flags & 0x2) && AllocatedBitmap)
            {
                // If you've been following along we kind of hard leak memory during this process
              
                // Windows devs realised this and came up with the following solution
              
                // We essentially add a reference to the entry to a global structure and the entry is only freed when we are 100% sure the thread is dead
              
                // So this is exactly what this function does
              
                LdrpQueueDeferredTlsData(ResultTlsInformation->TlsVector, ResultTlsInformation->ThreadId);

                continue;
            }

            if (!ResultTlsInformation->Flags)
            {
                ThreadsCleanedUp++;

                if (AllocatedBitmap)
                {
                    // Free the old TLS memory block
                  
                    RtlFreeHeap(RtlProcessHeap(), 0, ResultTlsInformation->TlsVector[TlsIndex]);

                    // Free the whole TLS vector allocated with LdrpGetNewTlsVector
                  
                    RtlFreeHeap(RtlProcessHeap(), 0, CONTAINING_RECORD(ResultTlsInformation->TlsVector, TLS_VECTOR, ModuleTlsData));
                }
            }

            if (!AllocatedBitmap)
            {
                RtlFreeHeap(RtlProcessHeap(), 0, ResultTlsInformation->TlsModulePointer);
            }
        }while (ThreadIndex > 0);


		if (!NT_SUCCESS( Status ))
		{
			DbgPrint("LdrpHandleTlsData:: General error 1 with Status %08lx\n", Status);
			LdrpReleaseTlsEntry(
				ModuleEntry
				);

			if (AllocatedBitmap)
				LdrpTlsBitmap.SizeOfBitMap -= 4;
		}
		else if (ThreadsCleanedUp > 0)
		{
			LdrpActiveThreadCount -= ThreadsCleanedUp;
		}
	} while (0) ;

	if (TlsInfo != &OneThreadTlsInfo)
	{
		RtlFreeHeap(
			Heap,
			0,
			TlsInfo
			);
	}

	if (!NT_SUCCESS( Status ))
	{
		DbgPrint("LdrpHandleTlsData:: General error 2 with Status %08lx\n", Status);
		return Status;
	}
		

	ModuleEntry->TlsIndex = 0xFFFF;

	return STATUS_SUCCESS;
}

NTSTATUS
LdrpAllocateTls (
    VOID
    )
{
    PTEB Teb;
    PLIST_ENTRY Head, Next;
    PTLS_ENTRY TlsEntry;
	PTLS_VECTOR TlsVectorEntry;
    PVOID *TlsVector;
    HANDLE ProcessHeap;
	ULONG SizeOfBitMap;
	SIZE_T TlsRawDataLength;
	ULONG i;
	
	SizeOfBitMap = LdrpTlsBitmap.SizeOfBitMap;
	Teb = NtCurrentTeb();
	
	if(SizeOfBitMap == 0){
		Teb->ThreadLocalStoragePointer = &Teb->ThreadLocalStoragePointer;
		return STATUS_SUCCESS;
	}

    //
    // Allocate the array of thread local storage pointers
    //

    if (SizeOfBitMap) {

        
        ProcessHeap = Teb->ProcessEnvironmentBlock->ProcessHeap;

        TlsVector = LdrpGetNewTlsVector(SizeOfBitMap);

        if (!TlsVector) {
            return STATUS_NO_MEMORY;
        }
        
        Head = &LdrpTlsList;
        Next = Head->Flink;

        while (Next != Head) {
            TlsEntry = CONTAINING_RECORD(Next, TLS_ENTRY, TlsEntryLinks);
            Next = Next->Flink;
			//
			// Calculate the size of the raw TLS data for this module.
			//

			TlsRawDataLength = TlsEntry->TlsDirectory.EndAddressOfRawData -
				TlsEntry->TlsDirectory.StartAddressOfRawData;

			TlsVectorEntry = RtlAllocateHeap(ProcessHeap,
                                             MAKE_TAG( TLS_TAG ),
                                             TlsRawDataLength
                                             );	
            if (!TlsVectorEntry ) {
				for ( i = 0; i < SizeOfBitMap; ++i )
				{
					if ( TlsVector[i] )
					RtlFreeHeap(ProcessHeap, 0, TlsVector[i]);
				}
				RtlFreeHeap(ProcessHeap, 0, TlsVector - 2);		
                return STATUS_NO_MEMORY;
            }
											 
            TlsVector[TlsEntry->TlsDirectory.Characteristics] = TlsVectorEntry;

            //
            // Do the TLS Callouts
            //

            RtlCopyMemory (
                TlsVector[TlsEntry->TlsDirectory.Characteristics],
                (PVOID)TlsEntry->TlsDirectory.StartAddressOfRawData,
                TlsRawDataLength
            );
        }
		
		Teb->ThreadLocalStoragePointer = TlsVector;
    }
	
    return STATUS_SUCCESS;
}

NTSTATUS
LdrpInitializeTls (
    VOID
    )
{
    PLDR_DATA_TABLE_ENTRY Entry;
    PLIST_ENTRY Head,Next;
    PIMAGE_TLS_DIRECTORY TlsImage;
    ULONG TlsSize;
	NTSTATUS Status;
	ULONG TlsBitmapSize;
	PVOID TlsStaticVector;
	ULONG TlsActualBitmapSize;
	ULONG TlsIndex = 0;
            
	LdrpActiveThreadCount = 1;
    InitializeListHead (&LdrpTlsList);

    //
    // Walk through the loaded modules and look for TLS. If we find TLS,
    // lock in the module and add to the TLS chain.
    //

    Head = &NtCurrentPeb()->Ldr->InLoadOrderModuleList;
    Next = Head->Flink;

    while (Next != Head) {

        Entry = CONTAINING_RECORD(Next, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        Next = Next->Flink;
			   
#if defined (AMD64)
	 if(!NT_SUCCESS(RtlpImageDirectoryEntryToDataEx(
		Entry->DllBase,
		TRUE,
		IMAGE_DIRECTORY_ENTRY_TLS,
		&TlsSize,
		&TlsImage
		)))
		{
			TlsImage = 0;
		};	
#else
	TlsImage = (PIMAGE_TLS_DIRECTORY)RtlImageDirectoryEntryToData(
		Entry->DllBase,
		TRUE,
		IMAGE_DIRECTORY_ENTRY_TLS,
		&TlsSize
		);	
#endif							   

        if (TlsImage) {

            Status = LdrpAllocateTlsEntry(TlsImage, Entry, &TlsIndex, NULL, NULL);
            if ( !NT_SUCCESS(Status) ) {
                return STATUS_NO_MEMORY;
            }

            //
            // Mark this as having thread local storage
            //

            Entry->TlsIndex = (USHORT)0xffff;
        }
    }
	
	if ( TlsIndex )
	{
		TlsBitmapSize = TlsIndex + 4;
		if ( TlsBitmapSize > 32 )
		{
			TlsActualBitmapSize = (TlsIndex + LDRP_BITMAP_INCREMENT) >> 5;
			TlsStaticVector = (PVOID)RtlAllocateHeap(
									 NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap,
									 MAKE_TAG( TLS_TAG ),
									 4 * TlsActualBitmapSize);
			if ( !TlsStaticVector )
				return STATUS_NO_MEMORY;
			LdrpActualBitmapSize = TlsActualBitmapSize;
		}
		else
		{
			TlsStaticVector = &LdrpStaticTlsBitmapVector;
			LdrpActualBitmapSize = 1;
		}
		RtlInitializeBitMap(&LdrpTlsBitmap, TlsStaticVector, TlsBitmapSize);
		RtlSetBits(&LdrpTlsBitmap, 0, TlsIndex);
		RtlClearBits(&LdrpTlsBitmap, TlsIndex, 4);
	}
	else
	{
		RtlInitializeBitMap(&LdrpTlsBitmap, 0, 0);
		LdrpActualBitmapSize = 0;
	}	

    //
    // We now have walked through all static DLLs and know
    // all DLLs that reference thread local storage. Now we
    // just have to allocate the thread local storage for the current
    // thread and for all subsequent threads.
    //

    return LdrpAllocateTls ();
}

VOID 
LdrpFreeTls(
	VOID
)
{
  ULONG Index; // ebx
  PVOID *TlsVector; // eax MAPDST
  PVOID *OldTlsVector; // esi
  ULONG Size; // [esp+8h] [ebp-8h]
  HANDLE ProcessHeap; // [esp+Ch] [ebp-4h]

  TlsVector = &NtCurrentTeb()->ThreadLocalStoragePointer;
  OldTlsVector = (PVOID *)*TlsVector;
  if ( *TlsVector )
  {
    if ( OldTlsVector != TlsVector )
    {
      ProcessHeap = NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap;
      Index = 0;
      Size = (ULONG)(ULONG_PTR)*(OldTlsVector - 2);
      if ( Size )
      {
        do
        {
          if ( OldTlsVector[Index] )
            RtlFreeHeap(ProcessHeap, 0, OldTlsVector[Index]);
          ++Index;
        }
        while ( Index < Size );
      }
      RtlFreeHeap(ProcessHeap, 0, OldTlsVector - 2);
    }
    *TlsVector = 0;
  }
}

// VOID
// LdrpCallTlsInitializers (
    // IN ULONG Reason,
	// IN PLDR_DATA_TABLE_ENTRY ModuleEntry
    // )
// {
    // PTLS_ENTRY TlsEntry;
    // PIMAGE_TLS_CALLBACK *CallBackArray;
    // PIMAGE_TLS_CALLBACK InitRoutine;

    // TlsEntry = (PTLS_ENTRY)LdrpFindTlsEntry(ModuleEntry);

    // if (TlsEntry) {

        // try {
            // CallBackArray = (PIMAGE_TLS_CALLBACK *)TlsEntry->TlsDirectory.AddressOfCallBacks;
            // if ( CallBackArray ) {
                // if (ShowSnaps) {
                    // DbgPrint(
							// "LDR: Tls Callbacks Found. Imagebase %p Tls %p CallBacks %p\n",
							// ModuleEntry->DllBase,
							// &TlsEntry->TlsDirectory,
							// CallBackArray);
                // }

                // while (*CallBackArray) {

                    // InitRoutine = *CallBackArray++;

                    // if (ShowSnaps) {
                        // DbgPrint( "LDR: Calling Tls Callback Imagebase %p Function %p\n",
                                    // ModuleEntry->DllBase,
                                    // InitRoutine
                                // );
                    // }

                    // LdrpCallInitRoutine((PDLL_INIT_ROUTINE)InitRoutine,
                                        // ModuleEntry->DllBase,
                                        // Reason,
                                        // 0);
                // }
            // }
        // }

        // except (LdrpGenericExceptionFilter(GetExceptionInformation(), __FUNCTION__)) {
            // DbgPrintEx(
                // DPFLTR_LDR_ID,
                // LDR_ERROR_DPFLTR,
                // "LDR: %s - caught exception %08lx calling TLS callbacks\n",
                // __FUNCTION__,
                // GetExceptionCode());
        // }
    // }
// }

VOID LdrpCleanupThreadTlsData()
{
	PVOID ThreadId; // ebx
	PTLS_RECLAIM_TABLE_ENTRY ReclaimEntry; // esi
	PTLS_VECTOR RealTlsVector; // edi MAPDST
	struct _TLS_VECTOR *PreviousDeferredTlsVector; // ecx
	struct _TLS_VECTOR *RealTlsVectorPreviousDeferredTls; // esi
	HANDLE ProcessHeap; // [esp+Ch] [ebp-Ch]
	PTLS_VECTOR PreviusRealTlsVector; // [esp+14h] [ebp-4h]

	ThreadId = NtCurrentTeb()->ClientId.UniqueThread;
	ProcessHeap = NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap;
	ReclaimEntry = &LdrpDelayedTlsReclaimTable[((ULONG_PTR)(ThreadId) >> 2) & 0xF];
	RealTlsVector = 0;
	PreviusRealTlsVector = 0;
	RtlAcquireSRWLockExclusive(&ReclaimEntry->Lock);
	RealTlsVector = ReclaimEntry->TlsVector;
	if ( ReclaimEntry->TlsVector )
	{
		do
		{
			PreviousDeferredTlsVector = RealTlsVector->PreviousDeferredTlsVector;
			if ( RealTlsVector->ThreadId == ThreadId )
			{
				if ( PreviusRealTlsVector )
					PreviusRealTlsVector->PreviousDeferredTlsVector = PreviousDeferredTlsVector;
				else
					ReclaimEntry->TlsVector = PreviousDeferredTlsVector;
				RealTlsVector->PreviousDeferredTlsVector = RealTlsVector;
				RealTlsVector = PreviusRealTlsVector;
			}
			PreviusRealTlsVector = RealTlsVector;
			RealTlsVector = PreviousDeferredTlsVector;
		}
		while ( PreviousDeferredTlsVector );
	}
	RtlReleaseSRWLockExclusive(&ReclaimEntry->Lock);
	if ( RealTlsVector )
	{
		do
		{
			RealTlsVectorPreviousDeferredTls = RealTlsVector->PreviousDeferredTlsVector;
			RtlFreeHeap(ProcessHeap, 0, RealTlsVector);
			RealTlsVector = RealTlsVectorPreviousDeferredTls;
		}while ( RealTlsVectorPreviousDeferredTls );
	}
}

NTSTATUS
NtSetInformationProcessHook(
    __in HANDLE ProcessHandle,
    __in PROCESSINFOCLASS ProcessInformationClass,
    __in_bcount(ProcessInformationLength) PVOID ProcessInformation,
    __in ULONG ProcessInformationLength
)
{
	PPROCESS_TLS_INFORMATION   TlsInfo;
	PPROCESS_TLS_INFORMATION   UserTlsInfo;
	PROCESS_TLS_INFORMATION    OneThreadTlsInfo;
	ULONG                      ThreadIndex;
	NTSTATUS                   Status;
	PVOID                    **AddrOfCurrentTlsBlock = 0;
	PVOID                      OldModuleTlsData = 0;
	PVOID                     *CurrentTlsBlock = 0;
	PVOID                     *ThreadTlsVector;
	HANDLE                     UniqueThread;
	PTEB Teb;
	ULONG Alignment = 0;
	HANDLE Snapshot;
	THREADENTRY32 threadEntry;
    OBJECT_ATTRIBUTES Obja;
    HANDLE hThread;
    CLIENT_ID ClientId;	
	
	UNREFERENCED_PARAMETER(ProcessInformationClass);

		//
		// Wow, what a hack - should open it...
		//

		if (ProcessHandle != NtCurrentProcess())
			return STATUS_INVALID_PARAMETER;

		//
		// We must be at least able to contain a PROCESS_TLS_INFORMATION object.
		//

		if (ProcessInformationLength < sizeof( PROCESS_TLS_INFORMATION ))
		{
			DbgPrint("NtSetInformationProcess:: ProcessTlsInformation STATUS_INFO_LENGTH_MISMATCH 1\n");
			return STATUS_INFO_LENGTH_MISMATCH;
		}	

			

		//
		// Copy the caller's buffer into kernel storage.  We'll use the stack if
		// there happens to be just one thread.
		//

		if (ProcessInformationLength == sizeof( OneThreadTlsInfo ))
		{
			TlsInfo = &OneThreadTlsInfo;
		}
		else
		{				
			TlsInfo = (PPROCESS_TLS_INFORMATION)RtlAllocateHeap(
						NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap,
						'slTP',
						ProcessInformationLength
			);			

			if (!TlsInfo)
				return STATUS_INSUFFICIENT_RESOURCES;
		}

		__try
		{
			RtlCopyMemory(
				TlsInfo,
				ProcessInformation,
				ProcessInformationLength
				);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			//
			// Assumed, didn't check for sure.
			//

			if (TlsInfo != &OneThreadTlsInfo)
				RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, TlsInfo);

			return (NTSTATUS)GetExceptionCode();
		}

		//
		// Perform further validation...
		//

		//
		// Well that's the totally wrong status code to return, but oh well.
		//

		if (TlsInfo->OperationType > MaxProcessTlsOperation)
		{
			if (TlsInfo != &OneThreadTlsInfo)
				RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, TlsInfo);
			
			DbgPrint("NtSetInformationProcess:: ProcessTlsInformation STATUS_INFO_LENGTH_MISMATCH 3\n");
			return STATUS_INFO_LENGTH_MISMATCH;
		}

		if (TlsInfo->Reserved & ~0x1)
		{
			if (TlsInfo != &OneThreadTlsInfo)
				RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, TlsInfo);
			DbgPrint("NtSetInformationProcess:: ProcessTlsInformation STATUS_INFO_LENGTH_MISMATCH 4\n");
			return STATUS_INFO_LENGTH_MISMATCH;
		}

		if (TlsInfo->ThreadDataCount < 1)
		{
			if (TlsInfo != &OneThreadTlsInfo)
				RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, TlsInfo);

			DbgPrint("NtSetInformationProcess:: ProcessTlsInformation STATUS_INFO_LENGTH_MISMATCH 5\n");
			return STATUS_INFO_LENGTH_MISMATCH;
		}

		//
		// Flags must be zero on the call.  Validate the flags field on each thread
		// data entry now.
		//

		for (ThreadIndex = 0;
			 ThreadIndex < TlsInfo->ThreadDataCount;
			 ThreadIndex += 1)
		{
			if (TlsInfo->ThreadData[ ThreadIndex ].Flags != 0)
			{
				if (TlsInfo != &OneThreadTlsInfo)
					RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, TlsInfo);

				return STATUS_INVALID_PARAMETER;
			}
		}

		//
		// Bizzare.  We check for ~1 and then 1.  Why not just check for nonzereo?
		//

		if (TlsInfo->Reserved & 0x1)
		{
			if (TlsInfo != &OneThreadTlsInfo)
				RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, TlsInfo);

			return STATUS_INVALID_PARAMETER;
		}

		Status             = STATUS_SUCCESS;
		UserTlsInfo        = (PPROCESS_TLS_INFORMATION)ProcessInformation;
		//Thread             = 0;
		ThreadIndex 	   = 0;
		Alignment		   = 4;

				Snapshot = LdrCreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );				
			

				threadEntry.dwSize = sizeof(THREADENTRY32);

					if(LdrThread32First(Snapshot, &threadEntry)){
						do{
							if (threadEntry.th32OwnerProcessID == HandleToUlong(NtCurrentTeb()->ClientId.UniqueProcess)) {
								// Obter identificador de thread
								//HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, threadEntry.th32ThreadID);

								ClientId.UniqueThread = (HANDLE)LongToHandle(threadEntry.th32ThreadID);
								ClientId.UniqueProcess = (HANDLE)NULL;

								InitializeObjectAttributes(
									&Obja,
									NULL,
									0,
									NULL,
									NULL
									);
								Status = NtOpenThread(
											&hThread,
											(ACCESS_MASK)THREAD_QUERY_INFORMATION,
											&Obja,
											&ClientId
											);
								if ( !NT_SUCCESS(Status) ) {
									return Status;
								}
								
								if (hThread != NULL) {
									// Obter informações básicas sobre a thread
									THREAD_BASIC_INFORMATION threadInfo;

									Status = NtQueryInformationThread(hThread, ThreadBasicInformation, &threadInfo, sizeof(threadInfo), NULL);

									if (!NT_SUCCESS(Status)) {
										return Status;
									}

									Teb = (PTEB)threadInfo.TebBaseAddress;
									
									if (ThreadIndex >= TlsInfo->ThreadDataCount)
										break;

									//
									// Get the current thread's TLS block.
									//

									__try
									{				
										AddrOfCurrentTlsBlock   = (PVOID **)&Teb->ThreadLocalStoragePointer;
										OldModuleTlsData        = (PVOID)&Teb->ThreadLocalStoragePointer;
										CurrentTlsBlock         = Teb->ThreadLocalStoragePointer; 
									}
									__except (EXCEPTION_EXECUTE_HANDLER)
									{
										// XXX: CHECK ME!! important!!!! - scope 0x17

										//
										// Assumed, didn't check for sure.
										//

										Status = (NTSTATUS)GetExceptionCode();
									}

									//
									// If we had an exception occur or we got a null TLS block, then we'll
									// need to either bail out or continue on to the next entry.
									//

									if (!NT_SUCCESS( Status ) || !CurrentTlsBlock)
									{
										//ExReleaseRundownProtection( &Thread->RundownProtect );

										if (NT_SUCCESS( Status ))
											continue;

										//ObDereferenceObject( Thread );

										if (TlsInfo != &OneThreadTlsInfo)
											RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, TlsInfo);

										return Status;
									}

									if (TlsInfo->OperationType != ProcessTlsReplaceVector)
									{
										//
										// This thread didn't expand the TLS bitmap.
										//

										__try
										{
											UserTlsInfo->ThreadData[ ThreadIndex ].Flags |= 0x1;
										}
										__except (EXCEPTION_EXECUTE_HANDLER)
										{
											Status = (NTSTATUS)GetExceptionCode();

											goto checkstatus1; // Ugh
										}

										__try
										{
											//
											// Get the old TLS pointer.
											//

											OldModuleTlsData = CurrentTlsBlock[ TlsInfo->TlsIndex ];

											//
											// Store the new module TLS pointer.
											//

											CurrentTlsBlock[ TlsInfo->TlsIndex ] =
												TlsInfo->ThreadData[ ThreadIndex ].TlsModulePointer;
										}
										__except (EXCEPTION_EXECUTE_HANDLER)
										{
											// XXX: CHECK ME!! important!!!! - scope 0x1E ( 0x1D )

											//
											// Assumed, didn't check for sure.
											//

											Status = (NTSTATUS)GetExceptionCode();

											goto checkstatus1; // Ugh
										}

							checkstatus1:
										if (!NT_SUCCESS( Status ))
										{
											//
											// If we failed, unset the successful-for-not-TLS-bitmap flag.
											//

											__try
											{
												UserTlsInfo->ThreadData[ ThreadIndex ].Flags &= ~0x1;
											}
											__except (EXCEPTION_EXECUTE_HANDLER)
											{
												Status = (NTSTATUS)GetExceptionCode();

												goto checkstatus1a; // Ugh
											}
										}
										else
										{
											//
											// Otherwise, tell the user the old TLS vector pointer and note
											// that we were successful.  (Hooray.)
											//

											__try
											{
												UserTlsInfo->ThreadData[ ThreadIndex ].TlsVector  = (PVOID *)OldModuleTlsData;
												UserTlsInfo->ThreadData[ ThreadIndex ].Flags     ^= 0x3;

												ThreadIndex += 1;
											}
											__except (EXCEPTION_EXECUTE_HANDLER)
											{
												Status = (NTSTATUS)GetExceptionCode();

												goto checkstatus1a; // Ugh
											}
										}

							checkstatus1a:
										if (!NT_SUCCESS( Status ))
											return Status;

										//
										// Next loop iteration continues...
										//
									}
									else
									{
										if (CurrentTlsBlock == (PVOID *)&Teb->ThreadLocalStoragePointer)
										{
											CurrentTlsBlock = 0;
										}
										else
										{
											//
											// The actual implementation saves the address, but I see
											// no reason to dereference it again.
											//

											ThreadTlsVector = TlsInfo->ThreadData[ ThreadIndex ].TlsVector;

											__try
											{
												RtlCopyMemory(
													ThreadTlsVector,
													CurrentTlsBlock,
													TlsInfo->TlsVectorLength * sizeof( PVOID )
													);
											}
											__except (EXCEPTION_EXECUTE_HANDLER)
											{
												Status = (NTSTATUS)GetExceptionCode();

												goto checkstatus2;
											}
										}

										//
										// Set flags accordingly as we're going to set the vector for this
										// thread.
										//

										__try
										{
											UserTlsInfo->ThreadData[ ThreadIndex ].Flags |= 0x1;
										}
										__except (EXCEPTION_EXECUTE_HANDLER)
										{
											Status = (NTSTATUS)GetExceptionCode();

											goto checkstatus2;
										}

										//
										// Set the vector for this thread.
										//

										__try
										{
											*AddrOfCurrentTlsBlock = TlsInfo->ThreadData[ ThreadIndex ].TlsVector;
										}
										__except (EXCEPTION_EXECUTE_HANDLER)
										{
											Status = (NTSTATUS)GetExceptionCode();

											goto checkstatus2;
										}

							checkstatus2:
										if (NT_SUCCESS( Status ))
										{
											UniqueThread = (HANDLE)NtCurrentTeb()->ClientId.UniqueThread;

											__try
											{
												UserTlsInfo->ThreadData[ ThreadIndex ].ThreadId   = UniqueThread;
												UserTlsInfo->ThreadData[ ThreadIndex ].TlsVector  = CurrentTlsBlock;
												UserTlsInfo->ThreadData[ ThreadIndex ].Flags     ^= 0x3;

												ThreadIndex += 1;
											}
											__except (EXCEPTION_EXECUTE_HANDLER)
											{
												Status = (NTSTATUS)GetExceptionCode();

												goto checkstatus3; // Ugh
											}
										}
										else
										{
											//
											// If we failed, unset the successful-for-not-TLS-bitmap flag.
											//

											__try
											{
												UserTlsInfo->ThreadData[ ThreadIndex ].Flags &= ~0x1;
											}
											__except (EXCEPTION_EXECUTE_HANDLER)
											{
												Status = (NTSTATUS)GetExceptionCode();

												goto checkstatus3; // Ugh
											}
										}

							checkstatus3:
										//ExReleaseRundownProtection( &Thread->RundownProtect );

										if (!NT_SUCCESS( Status ))
											return Status;

										//
										// Next loop iteration continues...
										//
									}									

									NtClose(hThread);
								}
							}
						} while (LdrThread32Next(Snapshot, &threadEntry));	
					}

	if (TlsInfo != &OneThreadTlsInfo)
		RtlFreeHeap(NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, TlsInfo);

	return STATUS_SUCCESS;
}

VOID
NTAPI
RtlClearBit(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_ BITMAP_INDEX BitNumber)
{
    BitMapHeader->Buffer[BitNumber / _BITCOUNT] &= ~(1 << (BitNumber & (_BITCOUNT - 1)));
}

VOID
NTAPI
RtlSetBit(
    _In_ PRTL_BITMAP BitMapHeader,
    _In_range_(<, BitMapHeader->SizeOfBitMap) BITMAP_INDEX BitNumber)
{
    BitMapHeader->Buffer[BitNumber / _BITCOUNT] |= ((BITMAP_INDEX)1 << (BitNumber & (_BITCOUNT - 1)));
}