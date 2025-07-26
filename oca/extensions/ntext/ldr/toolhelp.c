/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS system libraries
 * FILE:            dll/win32/kernel32/client/toolhelp.c
 * PURPOSE:         Toolhelp functions
 * PROGRAMMER:      Thomas Weidenmueller <w3seek@reactos.com>
 *                  Robert Dickenson (robd@mok.lvcm.com)
 *
 * NOTES:           Do NOT use the heap functions in here because they
 *                  adulterate the heap statistics!
 *
 * UPDATE HISTORY:
 *                  10/30/2004 Implemented some parts (w3)
 *                             Inspired by the book "Windows NT Native API"
 *                  Created 05 January 2003 (robd)
 */

#include <main.h>
#include <tlhelp32.h>

#define NDEBUG
#include <debug.h>

#define RtlpGetCurrentProcessId() (HandleToUlong(NtCurrentTeb()->ClientId.UniqueProcess))

NTSTATUS
LdrCopyAnsiToUnicode(
    PWCHAR Dest,
    PUCHAR Src,
    USHORT Max)
{
    UNICODE_STRING UnicodeString;
    ANSI_STRING AnsiString;

    UnicodeString.Buffer = Dest;
    UnicodeString.MaximumLength = Max;

    RtlInitAnsiString(&AnsiString, (PCSZ)Src);

    return RtlAnsiStringToUnicodeString( &UnicodeString, &AnsiString, FALSE );
}

/* INTERNAL DEFINITIONS *******************************************************/

typedef struct _RTLP_HEAP_ENTRY
{
    ULONG Size;
    USHORT Flags;
    USHORT Unknown1; /* FIXME */
    ULONG Unknown2; /* FIXME */
    PVOID Address;
} RTLP_HEAP_ENTRY, *PRTLP_HEAP_ENTRY;

#define CHECK_PARAM_SIZE(ptr, siz)                                             \
  if((ptr) == NULL || (ptr)->dwSize != (siz))                                  \
  {                                                                            \
    RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);                                     \
    return FALSE;                                                              \
  }

/*
 * Tests in win showed that the dwSize field can be greater than the actual size
 * of the structure for the ansi functions. I found this out by accidently
 * forgetting to set the dwSize field in a test application and it just didn't
 * work in ros but in win.
 */

#define CHECK_PARAM_SIZEA(ptr, siz)                                            \
  if((ptr) == NULL || (ptr)->dwSize < (siz))                                   \
  {                                                                            \
    RtlSetLastWin32Error(ERROR_INVALID_PARAMETER);                                     \
    return FALSE;                                                              \
  }

#define OffsetToPtr(Snapshot, Offset)                                          \
  ((ULONG_PTR)((Snapshot) + 1) + (ULONG_PTR)(Offset))

typedef struct _TH32SNAPSHOT
{
  /* Heap list */
  ULONG HeapListCount;
  ULONG HeapListIndex;
  ULONG_PTR HeapListOffset;
  /* Module list */
  ULONG ModuleListCount;
  ULONG ModuleListIndex;
  ULONG_PTR ModuleListOffset;
  /* Process list */
  ULONG ProcessListCount;
  ULONG ProcessListIndex;
  ULONG_PTR ProcessListOffset;
  /* Thread list */
  ULONG ThreadListCount;
  ULONG ThreadListIndex;
  ULONG_PTR ThreadListOffset;
} TH32SNAPSHOT, *PTH32SNAPSHOT;

/* INTERNAL FUNCTIONS *********************************************************/

static VOID
TH32FreeAllocatedResources(PRTL_DEBUG_INFORMATION HeapDebug,
                           PRTL_DEBUG_INFORMATION ModuleDebug,
                           PVOID ProcThrdInfo,
                           SIZE_T ProcThrdInfoSize)
{
  if(HeapDebug != NULL)
  {
    RtlDestroyQueryDebugBuffer(HeapDebug);
  }
  if(ModuleDebug != NULL)
  {
    RtlDestroyQueryDebugBuffer(ModuleDebug);
  }

  if(ProcThrdInfo != NULL)
  {
    NtFreeVirtualMemory(NtCurrentProcess(),
                        &ProcThrdInfo,
                        &ProcThrdInfoSize,
                        MEM_RELEASE);
  }
}

static NTSTATUS
TH32CreateSnapshot(DWORD dwFlags,
                   DWORD th32ProcessID,
                   PRTL_DEBUG_INFORMATION *HeapDebug,
                   PRTL_DEBUG_INFORMATION *ModuleDebug,
                   PVOID *ProcThrdInfo,
                   SIZE_T *ProcThrdInfoSize)
{
  NTSTATUS Status = STATUS_SUCCESS;

  *HeapDebug = NULL;
  *ModuleDebug = NULL;
  *ProcThrdInfo = NULL;
  *ProcThrdInfoSize = 0;

  /*
   * Allocate the debug information for a heap snapshot
   */
  if(dwFlags & TH32CS_SNAPHEAPLIST)
  {
    *HeapDebug = RtlCreateQueryDebugBuffer(0, FALSE);
    if(*HeapDebug != NULL)
    {
      Status = RtlQueryProcessDebugInformation(th32ProcessID,
                                               RTL_DEBUG_QUERY_HEAPS,
                                               *HeapDebug);
    }
    else
      Status = STATUS_UNSUCCESSFUL;
  }

  /*
   * Allocate the debug information for a module snapshot
   */
  if(dwFlags & TH32CS_SNAPMODULE &&
     NT_SUCCESS(Status))
  {
    *ModuleDebug = RtlCreateQueryDebugBuffer(0, FALSE);
    if(*ModuleDebug != NULL)
    {
      Status = RtlQueryProcessDebugInformation(th32ProcessID,
                                               RTL_DEBUG_QUERY_MODULES,
                                               *ModuleDebug);
    }
    else
      Status = STATUS_UNSUCCESSFUL;
  }

  /*
   * Allocate enough memory for the system's process list
   */

  if(dwFlags & (TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD) &&
     NT_SUCCESS(Status))
  {
    for(;;)
    {
      (*ProcThrdInfoSize) += 0x10000;
      Status = NtAllocateVirtualMemory(NtCurrentProcess(),
                                       ProcThrdInfo,
                                       0,
                                       ProcThrdInfoSize,
                                       MEM_COMMIT,
                                       PAGE_READWRITE);
      if(!NT_SUCCESS(Status))
      {
        break;
      }

      Status = NtQuerySystemInformation(SystemProcessInformation,
                                        *ProcThrdInfo,
                                        *ProcThrdInfoSize,
                                        NULL);
      if(Status == STATUS_INFO_LENGTH_MISMATCH)
      {
        NtFreeVirtualMemory(NtCurrentProcess(),
                            ProcThrdInfo,
                            ProcThrdInfoSize,
                            MEM_RELEASE);
        *ProcThrdInfo = NULL;
      }
      else
      {
        break;
      }
    }
  }

  /*
   * Free resources in case of failure!
   */

  if(!NT_SUCCESS(Status))
  {
    TH32FreeAllocatedResources(*HeapDebug,
                               *ModuleDebug,
                               *ProcThrdInfo,
                               *ProcThrdInfoSize);
  }

  return Status;
}

static NTSTATUS
TH32CreateSnapshotSectionInitialize(DWORD dwFlags,
                                    DWORD th32ProcessID,
                                    PRTL_DEBUG_INFORMATION HeapDebug,
                                    PRTL_DEBUG_INFORMATION ModuleDebug,
                                    PVOID ProcThrdInfo,
                                    HANDLE *SectionHandle)
{
  PSYSTEM_PROCESS_INFORMATION ProcessInfo;
  LPHEAPLIST32 HeapListEntry;
  LPMODULEENTRY32W ModuleListEntry;
  LPPROCESSENTRY32W ProcessListEntry;
  LPTHREADENTRY32 ThreadListEntry;
  OBJECT_ATTRIBUTES ObjectAttributes;
  LARGE_INTEGER SSize, SOffset;
  HANDLE hSection;
  PTH32SNAPSHOT Snapshot;
  ULONG_PTR DataOffset;
  SIZE_T ViewSize;
  ULONG i, nProcesses = 0, nThreads = 0, nHeaps = 0, nModules = 0;
  ULONG RequiredSnapshotSize = sizeof(TH32SNAPSHOT);
  PRTL_PROCESS_HEAPS hi = NULL;
  PRTL_PROCESS_MODULES mi = NULL;
  NTSTATUS Status = STATUS_SUCCESS;

  /*
   * Determine the required size for the heap snapshot
   */
  if(dwFlags & TH32CS_SNAPHEAPLIST)
  {
    hi = (PRTL_PROCESS_HEAPS)HeapDebug->Heaps;
    nHeaps = hi->NumberOfHeaps;
    RequiredSnapshotSize += nHeaps * sizeof(HEAPLIST32);
  }

  /*
   * Determine the required size for the module snapshot
   */
  if(dwFlags & TH32CS_SNAPMODULE)
  {
    mi = (PRTL_PROCESS_MODULES)ModuleDebug->Modules;
    nModules = mi->NumberOfModules;
    RequiredSnapshotSize += nModules * sizeof(MODULEENTRY32W);
  }

  /*
   * Determine the required size for the processes and threads snapshot
   */
  if(dwFlags & (TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD))
  {
    ULONG ProcOffset = 0;

    ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)ProcThrdInfo;
    do
    {
      ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)((ULONG_PTR)ProcessInfo + ProcOffset);
      nProcesses++;
      nThreads += ProcessInfo->NumberOfThreads;
      ProcOffset = ProcessInfo->NextEntryOffset;
    } while(ProcOffset != 0);

    if(dwFlags & TH32CS_SNAPPROCESS)
    {
      RequiredSnapshotSize += nProcesses * sizeof(PROCESSENTRY32W);
    }
    if(dwFlags & TH32CS_SNAPTHREAD)
    {
      RequiredSnapshotSize += nThreads * sizeof(THREADENTRY32);
    }
  }

  /*
   * Create and map the section
   */

  SSize.QuadPart = RequiredSnapshotSize;

  InitializeObjectAttributes(&ObjectAttributes,
                             NULL,
                             ((dwFlags & TH32CS_INHERIT) ? OBJ_INHERIT : 0),
			     NULL,
			     NULL);

  Status = NtCreateSection(&hSection,
                           SECTION_ALL_ACCESS,
                           &ObjectAttributes,
                           &SSize,
                           PAGE_READWRITE,
                           SEC_COMMIT,
                           NULL);
  if(!NT_SUCCESS(Status))
  {
    return Status;
  }

  SOffset.QuadPart = 0;
  ViewSize = 0;
  Snapshot = NULL;

  Status = NtMapViewOfSection(hSection,
                              NtCurrentProcess(),
                              (PVOID*)&Snapshot,
                              0,
                              0,
                              &SOffset,
                              &ViewSize,
                              ViewShare,
                              0,
                              PAGE_READWRITE);
  if(!NT_SUCCESS(Status))
  {
    NtClose(hSection);
    return Status;
  }

  RtlZeroMemory(Snapshot, sizeof(TH32SNAPSHOT));
  DataOffset = 0;

  /*
   * Initialize the section data and fill it with all the data we collected
   */

  /* initialize the heap list */
  if(dwFlags & TH32CS_SNAPHEAPLIST)
  {
    Snapshot->HeapListCount = nHeaps;
    Snapshot->HeapListOffset = DataOffset;
    HeapListEntry = (LPHEAPLIST32)OffsetToPtr(Snapshot, DataOffset);
    for(i = 0; i < nHeaps; i++)
    {
      HeapListEntry->dwSize = sizeof(HEAPLIST32);
      HeapListEntry->th32ProcessID = th32ProcessID;
      HeapListEntry->th32HeapID = (ULONG_PTR)hi->Heaps[i].BaseAddress;
      HeapListEntry->dwFlags = hi->Heaps[i].Flags;

      HeapListEntry++;
    }

    DataOffset += hi->NumberOfHeaps * sizeof(HEAPLIST32);
  }

  /* initialize the module list */
  if(dwFlags & TH32CS_SNAPMODULE)
  {
    Snapshot->ModuleListCount = nModules;
    Snapshot->ModuleListOffset = DataOffset;
    ModuleListEntry = (LPMODULEENTRY32W)OffsetToPtr(Snapshot, DataOffset);
    for(i = 0; i < nModules; i++)
    {
      ModuleListEntry->dwSize = sizeof(MODULEENTRY32W);
      ModuleListEntry->th32ModuleID = 1; /* no longer used, always set to one! */
      ModuleListEntry->th32ProcessID = th32ProcessID;
      ModuleListEntry->GlblcntUsage = mi->Modules[i].LoadCount;
      ModuleListEntry->ProccntUsage = mi->Modules[i].LoadCount;
      ModuleListEntry->modBaseAddr = (BYTE*)mi->Modules[i].ImageBase;
      ModuleListEntry->modBaseSize = mi->Modules[i].ImageSize;
      ModuleListEntry->hModule = (HMODULE)mi->Modules[i].ImageBase;

      LdrCopyAnsiToUnicode(ModuleListEntry->szModule,
                          &mi->Modules[i].FullPathName[mi->Modules[i].OffsetToFileName],
                          sizeof(ModuleListEntry->szModule) / sizeof(ModuleListEntry->szModule[0]));

      LdrCopyAnsiToUnicode(ModuleListEntry->szExePath,
						  mi->Modules[i].FullPathName,
                          sizeof(ModuleListEntry->szExePath) / sizeof(ModuleListEntry->szExePath[0]));

      ModuleListEntry++;
    }

    DataOffset += mi->NumberOfModules * sizeof(MODULEENTRY32W);
  }

  /* initialize the process list */
  if(dwFlags & TH32CS_SNAPPROCESS)
  {
    ULONG ProcOffset = 0;

    Snapshot->ProcessListCount = nProcesses;
    Snapshot->ProcessListOffset = DataOffset;
    ProcessListEntry = (LPPROCESSENTRY32W)OffsetToPtr(Snapshot, DataOffset);
    ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)ProcThrdInfo;
    do
    {
      ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)((ULONG_PTR)ProcessInfo + ProcOffset);

      ProcessListEntry->dwSize = sizeof(PROCESSENTRY32W);
      ProcessListEntry->cntUsage = 0; /* no longer used */
      ProcessListEntry->th32ProcessID = (ULONG_PTR)ProcessInfo->UniqueProcessId;
      ProcessListEntry->th32DefaultHeapID = 0; /* no longer used */
      ProcessListEntry->th32ModuleID = 0; /* no longer used */
      ProcessListEntry->cntThreads = ProcessInfo->NumberOfThreads;
      ProcessListEntry->th32ParentProcessID = (ULONG_PTR)ProcessInfo->InheritedFromUniqueProcessId;
      ProcessListEntry->pcPriClassBase = ProcessInfo->BasePriority;
      ProcessListEntry->dwFlags = 0; /* no longer used */
      if(ProcessInfo->ImageName.Buffer != NULL)
      {
        ULONG ExeFileLength = min(ProcessInfo->ImageName.Length,
                                  sizeof(ProcessListEntry->szExeFile) - sizeof(WCHAR));
        RtlCopyMemory(ProcessListEntry->szExeFile,
                      ProcessInfo->ImageName.Buffer,
                      ExeFileLength);
        ProcessListEntry->szExeFile[ExeFileLength / sizeof(WCHAR)] = UNICODE_NULL;
      }
      else
      {
        wcscpy(ProcessListEntry->szExeFile, L"[System Process]");
      }

      ProcessListEntry++;

      ProcOffset = ProcessInfo->NextEntryOffset;
    } while(ProcOffset != 0);

    DataOffset += nProcesses * sizeof(PROCESSENTRY32W);
  }

  /* initialize the thread list */
  if(dwFlags & TH32CS_SNAPTHREAD)
  {
    ULONG ProcOffset = 0;

    Snapshot->ThreadListCount = nThreads;
    Snapshot->ThreadListOffset = DataOffset;
    ThreadListEntry = (LPTHREADENTRY32)OffsetToPtr(Snapshot, DataOffset);
    ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)ProcThrdInfo;
    do
    {
      PSYSTEM_THREAD_INFORMATION ThreadInfo;
      ULONG n;

      ProcessInfo = (PSYSTEM_PROCESS_INFORMATION)((ULONG_PTR)ProcessInfo + ProcOffset);
      ThreadInfo = (PSYSTEM_THREAD_INFORMATION)(ProcessInfo + 1);

      for(n = 0; n < ProcessInfo->NumberOfThreads; n++)
      {
        ThreadListEntry->dwSize = sizeof(THREADENTRY32);
        ThreadListEntry->cntUsage = 0; /* no longer used */
        ThreadListEntry->th32ThreadID = (ULONG_PTR)ThreadInfo->ClientId.UniqueThread;
        ThreadListEntry->th32OwnerProcessID = (ULONG_PTR)ThreadInfo->ClientId.UniqueProcess;
        ThreadListEntry->tpBasePri = ThreadInfo->BasePriority;
        ThreadListEntry->tpDeltaPri = 0; /* no longer used */
        ThreadListEntry->dwFlags = 0; /* no longer used */

        ThreadInfo++;
        ThreadListEntry++;
      }

      ProcOffset = ProcessInfo->NextEntryOffset;
    } while(ProcOffset != 0);
  }

  /*
   * We're done, unmap the view and return the section handle
   */

  Status = NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)Snapshot);

  if(NT_SUCCESS(Status))
  {
    *SectionHandle = hSection;
  }
  else
  {
    NtClose(hSection);
  }

  return Status;
}

/* PUBLIC FUNCTIONS ***********************************************************/

/*
 * @implemented
 */
BOOL
WINAPI
Heap32First(LPHEAPENTRY32 lphe, DWORD th32ProcessID, DWORD th32HeapID)
{
  PRTL_DEBUG_INFORMATION DebugInfo;
  PRTL_HEAP_INFORMATION Heap;
  PRTLP_HEAP_ENTRY Block, LastBlock;
  ULONG i;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lphe, sizeof(HEAPENTRY32));

  DebugInfo = RtlCreateQueryDebugBuffer(0,
                                        FALSE);
  if (DebugInfo == NULL)
  {
    RtlSetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
    return FALSE;
  }

  Status = RtlQueryProcessDebugInformation(th32ProcessID,
                                           RTL_DEBUG_QUERY_HEAPS | RTL_DEBUG_QUERY_HEAP_BLOCKS,
                                           DebugInfo);

  if (NT_SUCCESS(Status))
  {
    Status = STATUS_NO_MORE_FILES;

    for (i = 0;
         i != DebugInfo->Heaps->NumberOfHeaps;
         i++)
    {
      Heap = &DebugInfo->Heaps->Heaps[i];

      if ((ULONG_PTR)Heap->BaseAddress == th32HeapID)
      {
        lphe->hHandle = (HANDLE)Heap->BaseAddress;
        lphe->dwAddress = 0;
        lphe->dwBlockSize = 0;
        lphe->dwFlags = 0;
        lphe->dwLockCount = 0;
        lphe->dwResvd = 0;
        lphe->th32ProcessID = th32ProcessID;
        lphe->th32HeapID = (ULONG_PTR)Heap->BaseAddress;

        Block = (PRTLP_HEAP_ENTRY)Heap->Entries;
        LastBlock = Block + Heap->NumberOfEntries;

        while (Block != LastBlock && (Block->Flags & PROCESS_HEAP_UNCOMMITTED_RANGE))
        {
          lphe->dwResvd++;
          lphe->dwAddress = (ULONG_PTR)((ULONG_PTR)Block->Address + Heap->EntryOverhead);
          Block++;
        }

        if (Block != LastBlock && lphe->dwResvd != 0)
        {
          lphe->dwBlockSize =  Block->Size;

          if (Block->Flags & 0x2F1) /* FIXME */
            lphe->dwFlags = LF32_FIXED;
          else if (Block->Flags & 0x20) /* FIXME */
            lphe->dwFlags = LF32_MOVEABLE;
          else if (Block->Flags & 0x100) /* FIXME */
            lphe->dwFlags = LF32_FREE;

          Status = STATUS_SUCCESS;
        }

        break;
      }
    }
  }

  RtlDestroyQueryDebugBuffer(DebugInfo);

  if (!NT_SUCCESS(Status))
  {
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
    return FALSE;
  }

  return TRUE;
}


/*
 * @implemented
 */
BOOL
WINAPI
Heap32Next(LPHEAPENTRY32 lphe)
{
  PRTL_DEBUG_INFORMATION DebugInfo;
  PRTL_HEAP_INFORMATION Heap;
  PRTLP_HEAP_ENTRY Block, LastBlock;
  BOOLEAN FoundUncommitted = FALSE;
  ULONG i;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lphe, sizeof(HEAPENTRY32));

  DebugInfo = RtlCreateQueryDebugBuffer(0,
                                        FALSE);
  if (DebugInfo == NULL)
  {
    RtlSetLastWin32Error(ERROR_NOT_ENOUGH_MEMORY);
    return FALSE;
  }

  Status = RtlQueryProcessDebugInformation(lphe->th32ProcessID,
                                           RTL_DEBUG_QUERY_HEAPS | RTL_DEBUG_QUERY_HEAP_BLOCKS,
                                           DebugInfo);

  if (NT_SUCCESS(Status))
  {
    Status = STATUS_NO_MORE_FILES;

    for (i = 0;
         i != DebugInfo->Heaps->NumberOfHeaps;
         i++)
    {
      Heap = &DebugInfo->Heaps->Heaps[i];

      if ((ULONG_PTR)Heap->BaseAddress == lphe->th32HeapID)
      {
        if (++lphe->dwResvd < Heap->NumberOfEntries)
        {
          lphe->dwFlags = 0;

          Block = (PRTLP_HEAP_ENTRY)Heap->Entries + lphe->dwResvd;
          LastBlock = (PRTLP_HEAP_ENTRY)Heap->Entries + Heap->NumberOfEntries;

          while (Block < LastBlock && (Block->Flags & PROCESS_HEAP_UNCOMMITTED_RANGE))
          {
            lphe->dwResvd++;
            lphe->dwAddress = (ULONG_PTR)((ULONG_PTR)Block->Address + Heap->EntryOverhead);
            FoundUncommitted = TRUE;
            Block++;
          }

          if (Block < LastBlock)
          {
            if (!FoundUncommitted)
              lphe->dwAddress += lphe->dwBlockSize;

            lphe->dwBlockSize =  Block->Size;

            if (Block->Flags & 0x2F1) /* FIXME */
              lphe->dwFlags = LF32_FIXED;
            else if (Block->Flags & 0x20) /* FIXME */
              lphe->dwFlags = LF32_MOVEABLE;
            else if (Block->Flags & 0x100) /* FIXME */
              lphe->dwFlags = LF32_FREE;

            Status = STATUS_SUCCESS;
          }
        }

        break;
      }
    }
  }

  RtlDestroyQueryDebugBuffer(DebugInfo);

  if (!NT_SUCCESS(Status))
  {
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
    return FALSE;
  }

  return TRUE;

}


/*
 * @implemented
 */
BOOL
WINAPI
Heap32ListFirst(HANDLE hSnapshot, LPHEAPLIST32 lphl)
{
  PTH32SNAPSHOT Snapshot;
  LARGE_INTEGER SOffset;
  SIZE_T ViewSize;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lphl, sizeof(HEAPLIST32));

  SOffset.QuadPart = 0;
  ViewSize = 0;
  Snapshot = NULL;

  Status = NtMapViewOfSection(hSnapshot,
                              NtCurrentProcess(),
                              (PVOID*)&Snapshot,
                              0,
                              0,
                              &SOffset,
                              &ViewSize,
                              ViewShare,
                              0,
                              PAGE_READWRITE);
  if(NT_SUCCESS(Status))
  {
    BOOL Ret;

    if(Snapshot->HeapListCount > 0)
    {
      LPHEAPLIST32 Entries = (LPHEAPLIST32)OffsetToPtr(Snapshot, Snapshot->HeapListOffset);
      Snapshot->HeapListIndex = 1;
      RtlCopyMemory(lphl, &Entries[0], sizeof(HEAPLIST32));
      Ret = TRUE;
    }
    else
    {
      RtlSetLastWin32Error(ERROR_NO_MORE_FILES);
      Ret = FALSE;
    }

    NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)Snapshot);
    return Ret;
  }

  RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
  return FALSE;
}


/*
 * @implemented
 */
BOOL
WINAPI
Heap32ListNext(HANDLE hSnapshot, LPHEAPLIST32 lphl)
{
  PTH32SNAPSHOT Snapshot;
  LARGE_INTEGER SOffset;
  SIZE_T ViewSize;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lphl, sizeof(HEAPLIST32));

  SOffset.QuadPart = 0;
  ViewSize = 0;
  Snapshot = NULL;

  Status = NtMapViewOfSection(hSnapshot,
                              NtCurrentProcess(),
                              (PVOID*)&Snapshot,
                              0,
                              0,
                              &SOffset,
                              &ViewSize,
                              ViewShare,
                              0,
                              PAGE_READWRITE);
  if(NT_SUCCESS(Status))
  {
    BOOL Ret;

    if(Snapshot->HeapListCount > 0 &&
       Snapshot->HeapListIndex < Snapshot->HeapListCount)
    {
      LPHEAPLIST32 Entries = (LPHEAPLIST32)OffsetToPtr(Snapshot, Snapshot->HeapListOffset);
      RtlCopyMemory(lphl, &Entries[Snapshot->HeapListIndex++], sizeof(HEAPLIST32));
      Ret = TRUE;
    }
    else
    {
      RtlSetLastWin32Error(ERROR_NO_MORE_FILES);
      Ret = FALSE;
    }

    NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)Snapshot);
    return Ret;
  }

  RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
  return FALSE;
}

/*
 * @implemented
 */
BOOL
WINAPI
Module32FirstW(HANDLE hSnapshot, LPMODULEENTRY32W lpme)
{
  PTH32SNAPSHOT Snapshot;
  LARGE_INTEGER SOffset;
  SIZE_T ViewSize;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lpme, sizeof(MODULEENTRY32W));

  SOffset.QuadPart = 0;
  ViewSize = 0;
  Snapshot = NULL;

  Status = NtMapViewOfSection(hSnapshot,
                              NtCurrentProcess(),
                              (PVOID*)&Snapshot,
                              0,
                              0,
                              &SOffset,
                              &ViewSize,
                              ViewShare,
                              0,
                              PAGE_READWRITE);
  if(NT_SUCCESS(Status))
  {
    BOOL Ret;

    if(Snapshot->ModuleListCount > 0)
    {
      LPMODULEENTRY32W Entries = (LPMODULEENTRY32W)OffsetToPtr(Snapshot, Snapshot->ModuleListOffset);
      Snapshot->ModuleListIndex = 1;
      RtlCopyMemory(lpme, &Entries[0], sizeof(MODULEENTRY32W));
      Ret = TRUE;
    }
    else
    {
      RtlSetLastWin32Error(ERROR_NO_MORE_FILES);
      Ret = FALSE;
    }

    NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)Snapshot);
    return Ret;
  }

  RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
  return FALSE;
}

/*
 * @implemented
 */
BOOL
WINAPI
Module32NextW(HANDLE hSnapshot, LPMODULEENTRY32W lpme)
{
  PTH32SNAPSHOT Snapshot;
  LARGE_INTEGER SOffset;
  SIZE_T ViewSize;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lpme, sizeof(MODULEENTRY32W));

  SOffset.QuadPart = 0;
  ViewSize = 0;
  Snapshot = NULL;

  Status = NtMapViewOfSection(hSnapshot,
                              NtCurrentProcess(),
                              (PVOID*)&Snapshot,
                              0,
                              0,
                              &SOffset,
                              &ViewSize,
                              ViewShare,
                              0,
                              PAGE_READWRITE);
  if(NT_SUCCESS(Status))
  {
    BOOL Ret;

    if((Snapshot->ModuleListCount > 0) &&
       (Snapshot->ModuleListIndex < Snapshot->ModuleListCount))
    {
      LPMODULEENTRY32W Entries = (LPMODULEENTRY32W)OffsetToPtr(Snapshot, Snapshot->ModuleListOffset);
      RtlCopyMemory(lpme, &Entries[Snapshot->ModuleListIndex++], sizeof(MODULEENTRY32W));
      Ret = TRUE;
    }
    else
    {
      RtlSetLastWin32Error(ERROR_NO_MORE_FILES);
      Ret = FALSE;
    }

    NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)Snapshot);
    return Ret;
  }

  RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
  return FALSE;
}

/*
 * @implemented
 */
BOOL
WINAPI
LdrProcess32First(HANDLE hSnapshot, LPPROCESSENTRY32W lppe)
{
  PTH32SNAPSHOT Snapshot;
  LARGE_INTEGER SOffset;
  SIZE_T ViewSize;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lppe, sizeof(PROCESSENTRY32W));

  SOffset.QuadPart = 0;
  ViewSize = 0;
  Snapshot = NULL;

  Status = NtMapViewOfSection(hSnapshot,
                              NtCurrentProcess(),
                              (PVOID*)&Snapshot,
                              0,
                              0,
                              &SOffset,
                              &ViewSize,
                              ViewShare,
                              0,
                              PAGE_READWRITE);
  if(NT_SUCCESS(Status))
  {
    BOOL Ret;

    if(Snapshot->ProcessListCount > 0)
    {
      LPPROCESSENTRY32W Entries = (LPPROCESSENTRY32W)OffsetToPtr(Snapshot, Snapshot->ProcessListOffset);

      Snapshot->ProcessListIndex = 1;
      RtlCopyMemory(lppe, &Entries[0], sizeof(PROCESSENTRY32W));
      Ret = TRUE;
    }
    else
    {

      RtlSetLastWin32Error(ERROR_NO_MORE_FILES);
      Ret = FALSE;
    }

    NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)Snapshot);
    return Ret;
  }

  RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
  return FALSE;
}

/*
 * @implemented
 */
BOOL
WINAPI
LdrProcess32Next(HANDLE hSnapshot, LPPROCESSENTRY32W lppe)
{
  PTH32SNAPSHOT Snapshot;
  LARGE_INTEGER SOffset;
  SIZE_T ViewSize;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lppe, sizeof(PROCESSENTRY32W));

  SOffset.QuadPart = 0;
  ViewSize = 0;
  Snapshot = NULL;

  Status = NtMapViewOfSection(hSnapshot,
                              NtCurrentProcess(),
                              (PVOID*)&Snapshot,
                              0,
                              0,
                              &SOffset,
                              &ViewSize,
                              ViewShare,
                              0,
                              PAGE_READWRITE);
  if(NT_SUCCESS(Status))
  {
    BOOL Ret;

    if(Snapshot->ProcessListCount > 0 &&
       Snapshot->ProcessListIndex < Snapshot->ProcessListCount)
    {
      LPPROCESSENTRY32W Entries = (LPPROCESSENTRY32W)OffsetToPtr(Snapshot, Snapshot->ProcessListOffset);
      RtlCopyMemory(lppe, &Entries[Snapshot->ProcessListIndex++], sizeof(PROCESSENTRY32W));
      Ret = TRUE;
    }
    else
    {
      RtlSetLastWin32Error(ERROR_NO_MORE_FILES);
      Ret = FALSE;
    }

    NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)Snapshot);
    return Ret;
  }

  RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
  return FALSE;
}

/*
 * @implemented
 */
BOOL
WINAPI
LdrThread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte)
{
  PTH32SNAPSHOT Snapshot;
  LARGE_INTEGER SOffset;
  SIZE_T ViewSize;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lpte, sizeof(THREADENTRY32));

  SOffset.QuadPart = 0;
  ViewSize = 0;
  Snapshot = NULL;

  Status = NtMapViewOfSection(hSnapshot,
                              NtCurrentProcess(),
                              (PVOID*)&Snapshot,
                              0,
                              0,
                              &SOffset,
                              &ViewSize,
                              ViewShare,
                              0,
                              PAGE_READWRITE);
  if(NT_SUCCESS(Status))
  {
    BOOL Ret;

    if(Snapshot->ThreadListCount > 0)
    {
      LPTHREADENTRY32 Entries = (LPTHREADENTRY32)OffsetToPtr(Snapshot, Snapshot->ThreadListOffset);
      Snapshot->ThreadListIndex = 1;
      RtlCopyMemory(lpte, &Entries[0], sizeof(THREADENTRY32));
      Ret = TRUE;
    }
    else
    {
      RtlSetLastWin32Error(ERROR_NO_MORE_FILES);
      Ret = FALSE;
    }

    NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)Snapshot);
    return Ret;
  }

  RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
  return FALSE;
}


/*
 * @implemented
 */
BOOL
WINAPI
LdrThread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte)
{
  PTH32SNAPSHOT Snapshot;
  LARGE_INTEGER SOffset;
  SIZE_T ViewSize;
  NTSTATUS Status;

  CHECK_PARAM_SIZE(lpte, sizeof(THREADENTRY32));

  SOffset.QuadPart = 0;
  ViewSize = 0;
  Snapshot = NULL;

  Status = NtMapViewOfSection(hSnapshot,
                              NtCurrentProcess(),
                              (PVOID*)&Snapshot,
                              0,
                              0,
                              &SOffset,
                              &ViewSize,
                              ViewShare,
                              0,
                              PAGE_READWRITE);
  if(NT_SUCCESS(Status))
  {
    BOOL Ret;

    if(Snapshot->ThreadListCount > 0 &&
       Snapshot->ThreadListIndex < Snapshot->ThreadListCount)
    {
      LPTHREADENTRY32 Entries = (LPTHREADENTRY32)OffsetToPtr(Snapshot, Snapshot->ThreadListOffset);
      RtlCopyMemory(lpte, &Entries[Snapshot->ThreadListIndex++], sizeof(THREADENTRY32));
      Ret = TRUE;
    }
    else
    {
      RtlSetLastWin32Error(ERROR_NO_MORE_FILES);
      Ret = FALSE;
    }

    NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)Snapshot);
    return Ret;
  }

  RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
  return FALSE;
}

/*
 * @implemented
 */
HANDLE
WINAPI
LdrCreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID)
{
  PRTL_DEBUG_INFORMATION HeapDebug, ModuleDebug;
  PVOID ProcThrdInfo;
  SIZE_T ProcThrdInfoSize;
  NTSTATUS Status;
  HANDLE hSnapShotSection = NULL;

  if(th32ProcessID == 0)
  {
    th32ProcessID = RtlpGetCurrentProcessId();
  }

  /*
   * Get all information required for the snapshot
   */
  Status = TH32CreateSnapshot(dwFlags,
                              th32ProcessID,
                              &HeapDebug,
                              &ModuleDebug,
                              &ProcThrdInfo,
                              &ProcThrdInfoSize);
  if(!NT_SUCCESS(Status))
  {
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
    return NULL;
  }

  /*
   * Create a section handle and initialize the collected information
   */
  Status = TH32CreateSnapshotSectionInitialize(dwFlags,
                                               th32ProcessID,
                                               HeapDebug,
                                               ModuleDebug,
                                               ProcThrdInfo,
                                               &hSnapShotSection);

  /*
   * Free the temporarily allocated memory which is no longer needed
   */
  TH32FreeAllocatedResources(HeapDebug,
                             ModuleDebug,
                             ProcThrdInfo,
                             ProcThrdInfoSize);

  if(!NT_SUCCESS(Status))
  {
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
    return NULL;
  }

  return hSnapShotSection;
}

NTSTATUS
NTAPI
NtGetNextThread (
    __in HANDLE ProcessHandle,
    __in HANDLE ThreadHandle,
    __in ACCESS_MASK DesiredAccess,
    __in ULONG HandleAttributes,
    __in ULONG Flags,
    __out PHANDLE NewThreadHandle
    )
{
    HANDLE hSnapshot;
    THREADENTRY32 te32;
    DWORD currentThreadId;
    DWORD nextThreadId;
    BOOL found;
    BOOL hasThread;
    HANDLE hThread;
	CLIENT_ID ClientId;
	OBJECT_ATTRIBUTES Obja;
	NTSTATUS Status;

    currentThreadId = 0;
    nextThreadId = 0;
    found = FALSE;
	
	UNREFERENCED_PARAMETER(ProcessHandle);
	UNREFERENCED_PARAMETER(ThreadHandle);
	UNREFERENCED_PARAMETER(DesiredAccess);
	UNREFERENCED_PARAMETER(HandleAttributes);
	UNREFERENCED_PARAMETER(Flags);

    hSnapshot = LdrCreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return STATUS_UNSUCCESSFUL;
    }

    te32.dwSize = sizeof(THREADENTRY32);
    hasThread = LdrThread32First(hSnapshot, &te32);

    while (hasThread) {
        if (te32.th32OwnerProcessID == HandleToUlong(NtCurrentTeb()->ClientId.UniqueProcess) && te32.th32ThreadID > currentThreadId) {
            if (!found || te32.th32ThreadID < nextThreadId) {
                nextThreadId = te32.th32ThreadID;
                found = TRUE;
            }
        }
        hasThread = LdrThread32Next(hSnapshot, &te32);
    }

    //CloseHandle(hSnapshot);
	NtClose(hSnapshot);

    if (!found) {
        return STATUS_NO_MORE_ENTRIES;
    }

	ClientId.UniqueThread = (HANDLE)LongToHandle(te32.th32ThreadID);
	ClientId.UniqueProcess = (HANDLE)NULL;

	InitializeObjectAttributes(&Obja,
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

    // hThread = OpenThread(DesiredAccess, (HandleAttributes & OBJ_INHERIT) != 0, nextThreadId);
    // if (hThread == NULL) {
        // return STATUS_ACCESS_DENIED;
    // }

    *NewThreadHandle = hThread;
    return STATUS_SUCCESS;
}

NTSTATUS 
NTAPI
NtGetNextProcess(
    HANDLE ProcessHandle,
    ACCESS_MASK DesiredAccess,
    ULONG HandleAttributes,
    ULONG Flags,
    PHANDLE NewProcessHandle
) {
    HANDLE hSnapshot;
    PROCESSENTRY32W pe32;
    DWORD currentPid;
    DWORD nextPid;
    BOOL found;
    BOOL hasProcess;
    HANDLE hProcess;
	CLIENT_ID cid;
	OBJECT_ATTRIBUTES objAttr;
	NTSTATUS status;

    currentPid = 0;
    nextPid = 0;
    found = FALSE;
	
	UNREFERENCED_PARAMETER(ProcessHandle);
	UNREFERENCED_PARAMETER(Flags);

    hSnapshot = LdrCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return STATUS_UNSUCCESSFUL;
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);
    hasProcess = LdrProcess32First(hSnapshot, &pe32);

    while (hasProcess) {
        if (pe32.th32ProcessID > currentPid) {
            if (!found || pe32.th32ProcessID < nextPid) {
                nextPid = pe32.th32ProcessID;
                found = TRUE;
            }
        }
        hasProcess = LdrProcess32Next(hSnapshot, &pe32);
    }

    NtClose(hSnapshot);

    if (!found) {
        return STATUS_NO_MORE_ENTRIES;
    }


    // Abre o novo processo usando NtOpenProcess
    cid.UniqueProcess = (HANDLE)(ULONG_PTR)nextPid;
    cid.UniqueThread = NULL;
    InitializeObjectAttributes(&objAttr, NULL, HandleAttributes, NULL, NULL);

    status = NtOpenProcess(&hProcess, DesiredAccess, &objAttr, &cid);
    if (status != STATUS_SUCCESS)
        return STATUS_ACCESS_DENIED;

    *NewProcessHandle = hProcess;
    return STATUS_SUCCESS;
}
/* EOF */
