/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    notification.c

Abstract:

    Implement Notification functions

Author:

    Skulltrail 16-August-2025

Revision History:

--*/

#include "main.h"

WINE_DEFAULT_DEBUG_CHANNEL(notification);

typedef DWORD CONFIGRET;

HDEVNOTIFY 
WINAPI 
RegisterDeviceNotificationWorker(HANDLE hRecipient, LPVOID NotificationFilter, DWORD Flags)
{
  CONFIGRET receive; // edi@1
  HDEVNOTIFY resp; // ebx@1
  HMODULE module; // eax@1
  HMODULE otherModule; // esi@1
  FARPROC proc; // eax@2
  DWORD error; // [sp-4h] [bp-14h]@12
  PULONG compose; // [sp+Ch] [bp-4h]@1

  compose = NULL;
  receive = 19;
  resp = 0;
  module = LoadLibraryW(L"SETUPAPI.DLL");
  otherModule = module;
  if ( !module )
    goto LABEL_19;
  proc = GetProcAddress(module, "CMP_RegisterNotification");
  if ( proc )
    receive = ((CONFIGRET (__stdcall *)(HANDLE, LPVOID, DWORD, PULONG))proc)(hRecipient, NotificationFilter, Flags, compose);
  FreeLibrary(otherModule);
  if ( receive )
  {
LABEL_19:
    switch ( receive )
    {
      case 2:
        error = 8;
        break;
      case 3:
        error = 87;
        break;
      case 31:
        error = 13;
        break;
      default:
        error = 1066;
        break;
    }
    SetLastError(error);
  }
  if ( compose && compose != (PULONG)-1 )
    resp = (HDEVNOTIFY)compose;
  return resp;
}

HPOWERNOTIFY 
WINAPI 
RegisterPowerSettingNotification(
  _In_  HANDLE hRecipient,
  _In_  LPCGUID PowerSettingGuid,
  _In_  DWORD Flags
)
{
	return (HPOWERNOTIFY)0xdeadbeef;	
  // LPVOID v4; // [sp+8h] [bp-20h]@1
  // int v5; // [sp+Ch] [bp-1Ch]@1
  // int v6; // [sp+10h] [bp-18h]@1
  // DWORD v7; // [sp+14h] [bp-14h]@1
  // int v8; // [sp+18h] [bp-10h]@1
  // int v9; // [sp+1Ch] [bp-Ch]@1
  // int v10; // [sp+20h] [bp-8h]@1

  // v6 = 0;
  // v4 = (LPVOID)28;
  // v5 = -2147483647;
  // v7 = PowerSettingGuid->Data1;
  // v8 = *(DWORD *)&PowerSettingGuid->Data2;
  // v9 = *(DWORD *)&PowerSettingGuid->Data4[0];
  // v10 = *(DWORD *)&PowerSettingGuid->Data4[4];
  // return RegisterDeviceNotificationWorker(hRecipient, v4, Flags);
}

BOOL WINAPI UnregisterDeviceNotification(HDEVNOTIFY Handle)
{
  int verification; // edi@1
  HMODULE module; // eax@1
  HMODULE receiveModule; // esi@1
  FARPROC proc; // eax@2
  DWORD error; // [sp-4h] [bp-Ch]@8

  verification = 0;
  module = LoadLibraryW(L"SETUPAPI.DLL");
  receiveModule = module;
  if ( module )
  {
    proc = GetProcAddress(module, "CMP_UnregisterNotification");
    if ( proc )
      verification = ((int (__stdcall *)(HDEVNOTIFY))proc)(Handle);
    FreeLibrary(receiveModule);
    if ( verification )
    {
      if ( verification == 3 )
      {
        error = 87;
      }
      else
      {
        if ( verification == 31 )
          error = 13;
        else
          error = 1066;
      }
      SetLastError(error);
    }
  }
  return verification == 0;
}

BOOL WINAPI UnregisterPowerSettingNotification(HDEVNOTIFY Handle)
{
  //return UnregisterDeviceNotification(Handle);
   return TRUE;
}

HDEVNOTIFY WINAPI RegisterDeviceNotificationW(HANDLE hRecipient, LPVOID NotificationFilter, DWORD Flags)
{
  HDEVNOTIFY result; // eax@2

  if ( *((DWORD *)NotificationFilter + 1) == -2147483647 )
  {
    SetLastError(0xDu);
    result = NULL;
  }
  else
  {
    result = (HDEVNOTIFY)RegisterDeviceNotificationWorker(hRecipient, NotificationFilter, Flags);
  }
  return result;
}


/**********************************************************************
 * RegisterSuspendResumeNotification (USER32.@)
 */
HPOWERNOTIFY WINAPI RegisterSuspendResumeNotification(HANDLE recipient, DWORD flags)
{
    FIXME("%p, %#lx: stub.\n", recipient, flags);
    return (HPOWERNOTIFY)0xdeadbeef;
}

/**********************************************************************
 * UnregisterSuspendResumeNotification (USER32.@)
 */
BOOL WINAPI UnregisterSuspendResumeNotification(HPOWERNOTIFY handle)
{
    FIXME("%p: stub.\n", handle);
    return TRUE;
}