@ stdcall CreateFileMappingFromApp(long ptr long int64 wstr) kernel32.CreateFileMappingFromApp
@ stdcall CreateFileMappingW(long ptr long long long wstr) kernel32.CreateFileMappingW
@ stdcall DiscardVirtualMemory(ptr long) kernel32.DiscardVirtualMemory
@ stdcall FlushViewOfFile(ptr long) kernel32.FlushViewOfFile
@ stdcall GetLargePageMinimum() kernel32.GetLargePageMinimum
@ stdcall GetProcessWorkingSetSizeEx(long ptr ptr ptr) kernel32.GetProcessWorkingSetSizeEx
@ stdcall GetWriteWatch(long ptr long ptr ptr ptr) kernel32.GetWriteWatch
@ stdcall MapViewOfFile(long long long long long) kernel32.MapViewOfFile
@ stdcall MapViewOfFileEx(long long long long long ptr) kernel32.MapViewOfFileEx
@ stdcall MapViewOfFileFromApp(long long int64 long) kernel32.MapViewOfFileFromApp
@ stub OfferVirtualMemory
@ stdcall OpenFileMappingFromApp(long long wstr) kernel32.OpenFileMappingFromApp
@ stdcall OpenFileMappingW(long long wstr) kernel32.OpenFileMappingW
@ stdcall ReadProcessMemory(long ptr ptr long ptr) kernel32.ReadProcessMemory
@ stub ReclaimVirtualMemory
@ stdcall ResetWriteWatch(ptr long) kernel32.ResetWriteWatch
@ stub SetProcessValidCallTargets
@ stdcall SetProcessWorkingSetSizeEx(long long long long) kernel32.SetProcessWorkingSetSizeEx
@ stdcall UnmapViewOfFile(ptr) kernel32.UnmapViewOfFile
@ stdcall UnmapViewOfFileEx(ptr long) kernel32.UnmapViewOfFileEx
@ stdcall VirtualAlloc(ptr long long long) kernel32.VirtualAlloc
@ stub VirtualAllocFromApp
@ stdcall VirtualFree(ptr long long) kernel32.VirtualFree
@ stdcall VirtualFreeEx(long ptr long long) kernel32.VirtualFreeEx
@ stdcall VirtualLock(ptr long) kernel32.VirtualLock
@ stdcall VirtualProtect(ptr long long ptr) kernel32.VirtualProtect
@ stub VirtualProtectFromApp
@ stdcall VirtualQuery(ptr ptr long) kernel32.VirtualQuery
@ stdcall VirtualQueryEx(long ptr ptr long) kernel32.VirtualQueryEx
@ stdcall VirtualUnlock(ptr long) kernel32.VirtualUnlock
@ stdcall WriteProcessMemory(long ptr ptr long ptr) kernel32.WriteProcessMemory
