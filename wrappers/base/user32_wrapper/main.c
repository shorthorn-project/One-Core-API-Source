/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    main.c

Abstract:

    Implement Main User functions

Author:

    Skulltrail 16-August-2025

Revision History:

--*/

#include <main.h>

WINE_DEFAULT_DEBUG_CHANNEL(main);

HMODULE userBaseHinst;
 
BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv)
{
    switch(fdwReason)
    {
        case DLL_PROCESS_ATTACH:
			userBaseHinst = GetModuleHandleA("userbase.dll");
			if(userBaseHinst){
				PrivateRegisterICSProcAddr = TryGetProcedure("PrivateRegisterICSProc");
				if(PrivateRegisterICSProcAddr){
					IsNativePNGConversor = TRUE;					
				}				
			}
			//pUpdateLayeredWindowIndirect = (void *)GetProcAddress(huser32, "UpdateLayeredWindowIndirect");
            break;
    }

    return TRUE;
}

BOOL WINAPI RemoveClipboardFormatListener(
  _In_  HWND hwnd
)
{
	DbgPrint("RemoveClipboardFormatListener is UNIMPLEMENTED\n");  
	hwnd = NULL;
	return TRUE;
}

int WINAPI VTagOutput()
{
  return 0;
}

int WINAPI VRipOutput()
{
  return 0;
}
/*
BOOL WINAPI SoundSentry()
{
	return _UserSoundSentry();
}*/

BOOL WINAPI UnregisterSessionPort()
{
	DbgPrint("UnregisterSessionPort is UNIMPLEMENTED\n");  
	return TRUE;
}

BOOL WINAPI SetMirrorRendering(HANDLE h1, HANDLE h2)
{
	DbgPrint("SetMirrorRendering is UNIMPLEMENTED\n");  
	return TRUE;
}

PVOID TryGetProcedure(char* procedureName){
	PVOID procedureAddress;
	
	if(userBaseHinst){
		procedureAddress = GetProcAddress(userBaseHinst, procedureName);
		if(procedureAddress){
			//FreeLibrary(userbaseModule);
			return procedureAddress;
		}else{
			DbgPrint("TryGetProcedure:: failed to get procedureAddress\n");
		}
	}else{
		DbgPrint("TryGetProcedure:: failed to get userBaseHinst\n");
	}
	//FreeLibrary(userbaseModule);
	return NULL;
}

DWORD WINAPI ConsoleControl(int a1, PVOID a2, int a3)
{
	DbgPrint("ConsoleControl is UNIMPLEMENTED\n"); 
	return 0;
}

BOOL WINAPI DoSoundConnect()
{
	DbgPrint("DoSoundConnect is UNIMPLEMENTED\n"); 
	return TRUE;
}

BOOL WINAPI DoSoundDisconnect()
{
	DbgPrint("DoSoundDisconnect is UNIMPLEMENTED\n"); 
	return TRUE;
}

DWORD WINAPI RegisterErrorReportingDialog(HWND a1, DWORD a2)
{
	DbgPrint("DoSoundDisconnect is UNIMPLEMENTED. Need implement NtUserCallTwoParam");	
	return 0;
}

DWORD WINAPI RegisterSessionPort(HANDLE a1, HANDLE a2)
{
	DbgPrint("RegisterSessionPort is UNIMPLEMENTED\n"); 
	return 0;
}

// BOOL WINAPI SkipPointerFrameMessages(
  // _In_  UINT32 pointerId
// )
// {
	// DbgPrint("SkipPointerFrameMessages is UNIMPLEMENTED\n"); 
	// return TRUE;
// }

BOOL WINAPI GetCIMSSM(INPUT_MESSAGE_SOURCE *inputMessageSource)
{
   SetLastError(87);
   return FALSE;
}

DWORD WINAPI MBToWCSEx(
		WORD  	CodePage,
		LPSTR  	MBString,
		LONG  	MBSize,
		LPWSTR *  	UnicodeString,
		LONG  	UnicodeSize,
		BOOL  	Allocate 
)
{
    DWORD Size;
    if (MBSize == -1)
    {
        MBSize = strlen(MBString)+1;
    }
    if (UnicodeSize == -1)
    {
        if (!Allocate)
        {
            return 0;
        }
        UnicodeSize = MBSize;
    }
    if (Allocate)
    {
        LPWSTR SafeString = RtlAllocateHeap(GetProcessHeap(), 0, UnicodeSize);
        if (SafeString == NULL)
            return 0;
        *UnicodeString = SafeString;
    }
    UnicodeSize *= sizeof(WCHAR);
    if (CodePage == 0)
    {
        RtlMultiByteToUnicodeN(*UnicodeString,UnicodeSize,&Size,MBString,MBSize);
    }
    else
    {
        Size = MultiByteToWideChar(CodePage,0,MBString,MBSize,*UnicodeString,UnicodeSize);
    }
    return Size;
}

// UWP stuff. Needs to be in user32.
BOOL WINAPI IsImmersiveProcess(HANDLE hProcess)
{
    SetLastError(0);
    return FALSE;
}

SIZE_T WINAPI IsProcess16Bit()
{
  return (NtCurrentTeb()->Win32ClientInfo[0] >> 2) & 1;
}