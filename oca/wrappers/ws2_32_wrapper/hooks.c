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
	}else if (dwIoControlCode == SIO_LOOPBACK_FAST_PATH) {
        // Let Java and other applications handle fast TCP loopback not being supported. 
        return WSAEOPNOTSUPP;
    } else if (dwIoControlCode == SIO_GET_EXTENSION_FUNCTION_POINTER) {
		// Despite WSASendMsg being introduced as a function on Vista, some things query the WSAID_WSASENDMSG function
		// for no reason at all. Maybe some proprietary driver defines this?
		
		// Anyways, Chromium needs this in order to access many UDP sockets.
		
		if ((res = WSAIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine))) {
			if (lpvInBuffer && lpvOutBuffer && cbInBuffer >= sizeof(GUID) && cbOutBuffer >= sizeof(PVOID)) {
				if (memcmp(&WSASendMsg_GUID, lpvInBuffer, sizeof(GUID)) == 0) {
					lpvOutBuffer = &WSASendMsg_GUID;
					return 0;
				}
			}
		}
		return res;
	}
    // fall back into original function
    return WSAIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine); 
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

/*
 * Fixups for modern async-I/O clients (Node.js/libuv, undici). The 2003/XP winsock
 * stack lacks the per-socket TCP_KEEPALIVE option and advertises IFS handles, which
 * drives libuv down the Vista-only SetFileCompletionNotificationModes path
 * (unsupported here) and breaks both connect and streaming. setsockoptInternal
 * translates TCP_KEEPALIVE to the supported SIO_KEEPALIVE_VALS ioctl;
 * getsockoptInternal reports the base provider as non-IFS.
 */
#ifndef SO_PROTOCOL_INFOW
#define SO_PROTOCOL_INFOW   0x2005
#endif
#ifndef XP1_IFS_HANDLES
#define XP1_IFS_HANDLES     0x00020000
#endif
#ifndef SIO_KEEPALIVE_VALS
#define SIO_KEEPALIVE_VALS  0x98000004
#endif
#ifndef IPPROTO_TCP
#define IPPROTO_TCP         6
#endif

/* Layout of struct tcp_keepalive (mstcpip.h); named locally to avoid header clashes. */
typedef struct _OCA_TCP_KEEPALIVE {
    ULONG onoff;
    ULONG keepalivetime;      /* milliseconds */
    ULONG keepaliveinterval;  /* milliseconds */
} OCA_TCP_KEEPALIVE;

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

	/*
	 * TCP_KEEPALIVE is a Vista+ option; the 2003/XP afd rejects it with
	 * WSAENOPROTOOPT, which kills libuv/undici sockets on setKeepAlive. Map it
	 * to SIO_KEEPALIVE_VALS, which the stack does support. optval carries the
	 * idle time in SECONDS (Node passes ~~(delay/1000)); the ioctl wants ms.
	 */
	if (level == IPPROTO_TCP && optname == TCP_KEEPALIVE) {
		OCA_TCP_KEEPALIVE ka;
		DWORD bytesReturned = 0;
		ka.onoff = 1;
		ka.keepalivetime = (optval != NULL && optlen >= (INT)sizeof(ULONG))
		                       ? (*(const ULONG*)optval) * 1000 : 7200000;
		ka.keepaliveinterval = 1000;
		if (WSAIoctl(s, SIO_KEEPALIVE_VALS, &ka, sizeof(ka),
		             NULL, 0, &bytesReturned, NULL, NULL) == 0)
			return 0;
		return SOCKET_ERROR; /* WSAIoctl already set the last error */
	}

	return setsockopt(s, level, optname, optval, optlen);
}

INT WINAPI getsockoptInternal(SOCKET s, int level, int optname, char *optval, int *optlen) {
    // Unity 6 games require this hook for network connectivity.
    if (level == IPPROTO_IPV6 && optname == IPV6_V6ONLY) {
        *(PDWORD)optval = TRUE; // no dual-stack support :(
        return 0;
    }
    /*
     * Report the base provider as non-IFS by clearing XP1_IFS_HANDLES from the
     * returned protocol info. libuv/undici test this bit to decide whether to call
     * SetFileCompletionNotificationModes (Vista+, unsupported on the 2003 kernel);
     * with it clear they skip that path and use plain IOCP, so connect and streaming
     * work. dwServiceFlags1 is the first field of WSAPROTOCOL_INFO[A/W].
     */
    if (level == SOL_SOCKET &&
        (optname == SO_PROTOCOL_INFOA || optname == SO_PROTOCOL_INFOW)) {
        INT ret = getsockopt(s, level, optname, optval, optlen);
        if (ret == 0 && optval != NULL && optlen != NULL &&
            *optlen >= (INT)sizeof(DWORD)) {
            *(PDWORD)optval &= ~XP1_IFS_HANDLES;
        }
        return ret;
    }
    return getsockopt(s, level, optname, optval, optlen);
}