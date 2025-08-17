/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    input.c

Abstract:

    Implement Message functions

Author:

    Skulltrail 16-August-2025

Revision History:

--*/

#include <main.h>

static const INPUT_MESSAGE_SOURCE msg_source_unavailable = { IMDT_UNAVAILABLE, IMO_UNAVAILABLE };

/***********************************************************************
 *		GetCurrentInputMessageSource (USER32.@)
 */
BOOL WINAPI GetCurrentInputMessageSource(PINPUT_MESSAGE_SOURCE MessageSource)
{
    if (!MessageSource) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    // Most OSS applications I have seen just check this for touch input so it should be fine
    MessageSource->DeviceType = IMDT_UNAVAILABLE;
    // To be better compatible with applications. The only real uses of OriginId I have seen are for not processing injected input
    // (or game anticheat) so IMO_HARDWARE is the most compatible with those applications
    MessageSource->OriginId = IMO_HARDWARE;
    return TRUE;
}

HWND WINAPI GetSendMessageReceiver(DWORD threadId)
{
	DbgPrint("GetSendMessageReceiver is UNIMPLEMENTED\n");  
  //return (HWND)NtUserCallOneParam(threadId, 35); need implement own NtUserCallOneParam
  return NULL;
}