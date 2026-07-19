/*++

Copyright (c) 2024 Shorthorn Project

Module Name:

    hooks.c

Abstract:

    This module implements Hooks of Native Windows Sockets 2 APIs

Author:

    Skulltrail 23-October-2024

Revision History:

--*/
 
#include "main.h"
#include "stubs.h"
#include "stdio.h"

#define IOC_WS2                        0x08000000
#define _WSAIORW(x,y)                  (IOC_INOUT|(x)|(y))
#define SIO_GET_EXTENSION_FUNCTION_POINTER  _WSAIORW(IOC_WS2,6)

#define WSAID_WSASENDMSG /* a441e712-754f-43ca-84a7-0dee44cf606d */ \
    {0xa441e712,0x754f,0x43ca,{0x84,0xa7,0x0d,0xee,0x44,0xcf,0x60,0x6d}}
	
const GUID WSASendMsg_GUID = WSAID_WSASENDMSG;	

// /*
 // * @implemented
 // */
// INT
// WSAAPI
// WSAIoctl(IN SOCKET s,
         // IN DWORD dwIoControlCode,
         // IN LPVOID lpvInBuffer,
         // IN DWORD cbInBuffer,
         // OUT LPVOID lpvOutBuffer,
         // IN DWORD cbOutBuffer,
         // OUT LPDWORD lpcbBytesReturned,
         // IN LPWSAOVERLAPPED lpOverlapped,
         // IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
// )
// {
    // PWSSOCKET Socket;
    // INT Status;
    // INT ErrorCode;
    // LPWSATHREADID ThreadId;
	
    // DPRINT("WSAIoctl: %lx, %lx\n", s, dwIoControlCode);
	
	// if (dwIoControlCode == SIO_BASE_HANDLE
        // || dwIoControlCode == SIO_BSP_HANDLE
        // || dwIoControlCode == SIO_BSP_HANDLE_SELECT
        // || dwIoControlCode == SIO_BSP_HANDLE_POLL) { 
		// if (lpvOutBuffer == NULL || lpcbBytesReturned == NULL) {
			// WSASetLastError(WSAEFAULT);
			// return SOCKET_ERROR;
		// }

		// *(SOCKET*)lpvOutBuffer = s;
		// *lpcbBytesReturned = sizeof(SOCKET);
		// return 0;
	// }


    // /* Check for WSAStartup */
    // if ((ErrorCode = WsQuickPrologTid(&ThreadId)) == ERROR_SUCCESS)
    // {
        // /* Get the Socket Context */
        // if ((Socket = WsSockGetSocket(s)))
        // {
            // /* Make the call */
            // Status = Socket->Provider->Service.lpWSPIoctl(s,
                                                  // dwIoControlCode,
                                                  // lpvInBuffer,
                                                  // cbInBuffer,
                                                  // lpvOutBuffer,
                                                  // cbOutBuffer,
                                                  // lpcbBytesReturned,
                                                  // lpOverlapped,
                                                  // lpCompletionRoutine,
                                                  // ThreadId,
                                                  // &ErrorCode);

            // /* Deference the Socket Context */
            // WsSockDereference(Socket);

            // /* Return Provider Value */
            // if (Status == ERROR_SUCCESS) return Status;
        // }
        // else
        // {
            // /* No Socket Context Found */
            // ErrorCode = WSAENOTSOCK;
        // }
    // }

    // /* Return with an Error */
    // SetLastError(ErrorCode);
    // return SOCKET_ERROR;	
// }

// Virtually every single 100% Rust-based application needs this to make any HTTP requests.
// While supported in Vista+, since Windows 8, those four I/O control codes simply return back the original socket, thus considered 'deprecated'
int WSAAPI WSAIoctlInternal(
  SOCKET                             s,
  DWORD                              dwIoControlCode,
  LPVOID                             lpvInBuffer,
  DWORD                              cbInBuffer,
  LPVOID                             lpvOutBuffer,
  DWORD                              cbOutBuffer,
  LPDWORD                            lpcbBytesReturned,
  LPWSAOVERLAPPED                    lpOverlapped,
  LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
) {
	int res = 0;
	
	// Rust applications require SIO_BASE* functions, for whatever reason.
    if (dwIoControlCode == SIO_BASE_HANDLE
            || dwIoControlCode == SIO_BSP_HANDLE
            || dwIoControlCode == SIO_BSP_HANDLE_SELECT
            || dwIoControlCode == SIO_BSP_HANDLE_POLL) { 
        if (lpvOutBuffer == NULL || lpcbBytesReturned == NULL) {
            WSASetLastError(WSAEFAULT);
            return SOCKET_ERROR;
        }

        *(SOCKET*)lpvOutBuffer = s;
        *lpcbBytesReturned = sizeof(SOCKET);
        return 0;
	} else if (dwIoControlCode == SIO_LOOPBACK_FAST_PATH) {
        // Let Java and other applications handle fast TCP loopback not being supported. 
        return WSAEOPNOTSUPP;
    } else if (dwIoControlCode == SIO_GET_EXTENSION_FUNCTION_POINTER) {
		// Despite WSASendMsg being introduced as a function on Vista, some things query the WSAID_WSASENDMSG function
		// for no reason at all. Maybe some proprietary driver defines this?
		
		// Anyways, Chromium needs this in order to access many UDP sockets.
		
		if ((res = WSAIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine))) {
			if (lpvInBuffer && lpvOutBuffer && cbInBuffer >= sizeof(GUID) && cbOutBuffer >= sizeof(PVOID)) {
				if (memcmp(&WSASendMsg_GUID, lpvInBuffer, sizeof(GUID)) == 0) {
					*(LPVOID*)lpvOutBuffer = (LPVOID)WSASendMsg; // has to be a pointer to WSASendMsg function itself.
					return 0;
				}
			}
		}
		return res;
	} else if (dwIoControlCode == SIO_UDP_NETRESET || dwIoControlCode == SIO_UDP_CONNRESET) {
		// MSDN claims that they are supported on Windows XP, but in reality, there's reason to doubt this claim as they both are not present in 2003 SP1 DDK.
		// Needed by many Golang applications: https://github.com/shorthorn-project/One-Core-API-Binaries/issues/563
		
		return 0;
	}
	
    // fall back into original function
    res = WSAIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine); 
	
	// Suppress WSAEINVAL for unsupported/unknown IOCTLs (in case there are more and more of them.)
    if (res == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEINVAL) {
            DbgPrint("WSAIoctlInternal: unsupported IOCTL %lx. If this is encountered report to OCA so it is properly supported.\n", dwIoControlCode);
            return 0; // Return success to the caller
        }
	}
	
	return res;
}

SOCKET WSAAPI WSASocketAInternal(int af, int type, int protocol,
                         LPWSAPROTOCOL_INFOA lpProtocolInfo,
                         GROUP g, DWORD dwFlags)
{
    SOCKET curSock;
    if(dwFlags & WSA_FLAG_NO_HANDLE_INHERIT){
                
        dwFlags ^= WSA_FLAG_NO_HANDLE_INHERIT;
                curSock = WSASocketA(af, type, protocol, lpProtocolInfo, g, dwFlags);
                if (curSock != INVALID_SOCKET) {
                    SetHandleInformation((HANDLE)curSock, HANDLE_FLAG_INHERIT, 0);
                }
        DbgPrint("WSASocketWInternal: flag is WSA_FLAG_NO_HANDLE_INHERIT\n");
                return curSock;
    }
    return WSASocketA(af, type, protocol, lpProtocolInfo, g, dwFlags);
}

SOCKET WSAAPI WSASocketWInternal(int af, int type, int protocol,
                         LPWSAPROTOCOL_INFOW lpProtocolInfo,
                         GROUP g, DWORD dwFlags)
{
        SOCKET curSock;
    if(dwFlags & WSA_FLAG_NO_HANDLE_INHERIT){
                
        dwFlags ^= WSA_FLAG_NO_HANDLE_INHERIT;
                curSock = WSASocketW(af, type, protocol, lpProtocolInfo, g, dwFlags);
                if (curSock != INVALID_SOCKET) {
                    SetHandleInformation((HANDLE)curSock, HANDLE_FLAG_INHERIT, 0);
                }
        DbgPrint("WSASocketWInternal: flag is WSA_FLAG_NO_HANDLE_INHERIT\n");
                return curSock;
    }
    return WSASocketW(af, type, protocol, lpProtocolInfo, g, dwFlags);
}

INT
WSAAPI
setsockoptInternal(
	IN SOCKET s,
    IN INT level,
    IN INT optname,
    IN CONST CHAR FAR* optval,
    IN INT optlen)
{
	if(level == IPPROTO_IPV6 && optname == IPV6_V6ONLY){
		return S_OK;
	}
	
	return setsockopt(s, level, optname, optval, optlen);
}

INT WINAPI getsockoptInternal(SOCKET s, int level, int optname, char *optval, int *optlen) {
    // Unity 6 games require this hook for network connectivity.
    if (level == IPPROTO_IPV6 && optname == IPV6_V6ONLY) {
        *(PDWORD)optval = TRUE; // no dual-stack support :(
        return 0;
    }
    return getsockopt(s, level, optname, optval, optlen);
}