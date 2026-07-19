/*++

Copyright (c) 2022 Shorthorn Project

Module Name:

    socket.c

Abstract:

    This module implements Socket APIs

Author:

    Skulltrail 25-September-2022

Revision History:

--*/
 
#include "main.h"
#include "stubs.h"
#include "stdio.h"

WINE_DEFAULT_DEBUG_CHANNEL(socket);

static inline char *strdupWtoA(const WCHAR *str)
{
    char *ret = NULL;

    if(str) {
        size_t size = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
        ret = malloc(size);
        if(ret)
            WideCharToMultiByte(CP_ACP, 0, str, -1, ret, size, NULL, NULL);
    }

    return ret;
}

static int hexval(unsigned c)
{
    if (c-'0'<10) return c-'0';
    c |= 32;
    if (c-'a'<6) return c-'a'+10;
    return -1;
}

/***********************************************************************
 *              inet_ntop                      (WS2_32.@)
 */
PCSTR WSAAPI WS_inet_ntop(INT af, const VOID *a0, PSTR s, size_t l)
{
    const unsigned char *a = a0;
    int i, j, max, best;
    char buf[100];
    if (!a0 || !s) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    switch (af) {
    case AF_INET:
        if (snprintf(s, l, "%d.%d.%d.%d", a[0],a[1],a[2],a[3]) < l)
            return s;
        break;
    case AF_INET6:
        if (memcmp(a, "\0\0\0\0\0\0\0\0\0\0\377\377", 12))
            snprintf(buf, 100,
                "%x:%x:%x:%x:%x:%x:%x:%x",
                256*a[0]+a[1],256*a[2]+a[3],
                256*a[4]+a[5],256*a[6]+a[7],
                256*a[8]+a[9],256*a[10]+a[11],
                256*a[12]+a[13],256*a[14]+a[15]);
        else
            snprintf(buf, 100,
                "%x:%x:%x:%x:%x:%x:%d.%d.%d.%d",
                256*a[0]+a[1],256*a[2]+a[3],
                256*a[4]+a[5],256*a[6]+a[7],
                256*a[8]+a[9],256*a[10]+a[11],
                a[12],a[13],a[14],a[15]);
        /* Replace longest /(^0|:)[:0]{2,}/ with "::" */
        for (i=best=0, max=2; buf[i]; i++) {
            if (i && buf[i] != ':') continue;
            j = strspn(buf+i, ":0");
            if (j>max) best=i, max=j;
        }
        if (max>3) {
            buf[best] = buf[best+1] = ':';
            memmove(buf+best+2, buf+best+max, i-best-max+1);
        }
        if (strlen(buf) < l) {
            strcpy(s, buf);
            return s;
        }
        break;
    default:
        WSASetLastError(WSAEAFNOSUPPORT);
        return 0;
    }
    WSASetLastError(ERROR_INVALID_PARAMETER);
    return 0;
}

/***********************************************************************
*              inet_pton                      (WS2_32.@)
*/
int WSAAPI WS_inet_pton(INT af, PCSTR s, PVOID a0)
{
    UINT16 ip[8];
    unsigned char *a = a0;
    int i, j, v, d, brk=-1, need_v4=0;
    if (!s || !a0) {
        SetLastError(WSAEFAULT);
        return -1;
    }
    if (af==AF_INET) {
        for (i=0; i<4; i++) {
            for (v=j=0; j<3 && isdigit(s[j]); j++)
                v = 10*v + s[j]-'0';
            if (j==0 || (j>1 && s[0]=='0') || v>255) return 0;
            a[i] = v;
            if (s[j]==0 && i==3) return 1;
            if (s[j]!='.') return 0;
            s += j+1;
        }
        return 0;
    } else if (af!=AF_INET6) {
        SetLastError(WSAEAFNOSUPPORT);
        return -1;
    }

    if (*s==':' && *++s!=':') return 0;

    for (i=0; ; i++) {
        if (s[0]==':' && brk<0) {
            brk=i;
            ip[i&7]=0;
            if (!*++s) break;
            if (i==7) return 0;
            continue;
        }
        for (v=j=0; j<4 && (d=hexval(s[j]))>=0; j++)
            v=16*v+d;
        if (j==0) return 0;
        ip[i&7] = v;
        if (!s[j] && (brk>=0 || i==7)) break;
        if (i==7) return 0;
        if (s[j]!=':') {
            if (s[j]!='.' || (i<6 && brk<0)) return 0;
            need_v4=1;
            i++;
            ip[i&7]=0;
            break;
        }
        s += j+1;
    }
    if (brk>=0) {
        memmove(ip+brk+7-i, ip+brk, 2*(i+1-brk));
        for (j=0; j<7-i; j++) ip[brk+j] = 0;
    }
    for (j=0; j<8; j++) {
        *a++ = ip[j]>>8;
        *a++ = ip[j];
    }
    if (need_v4 && WS_inet_pton(AF_INET, (void *)s, a-4) <= 0) return 0;
    return 1;
}

static const int ws_poll_map[][2] =
{
    MAP_OPTION( POLLERR ),
    MAP_OPTION( POLLHUP ),
    MAP_OPTION( POLLNVAL ),
    MAP_OPTION( POLLWRNORM ),
    MAP_OPTION( POLLWRBAND ),
    MAP_OPTION( POLLRDNORM ),
    { WS_POLLRDBAND, POLLPRI }
};

static int convert_poll_u2w(int events)
{
    int i, ret;
    for (i = ret = 0; events && i < sizeof(ws_poll_map) / sizeof(ws_poll_map[0]); i++)
    {
        if (ws_poll_map[i][1] & events)
        {
            ret |= ws_poll_map[i][0];
            events &= ~ws_poll_map[i][1];
        }
    }

    if (events)
        FIXME("Unsupported poll() flags 0x%x\n", events);
    return ret;
}

static int convert_poll_w2u(int events)
{
    int i, ret;
    for (i = ret = 0; events && i < sizeof(ws_poll_map) / sizeof(ws_poll_map[0]); i++)
    {
        if (ws_poll_map[i][0] & events)
        {
            ret |= ws_poll_map[i][1];
            events &= ~ws_poll_map[i][0];
        }
    }

    if (events)
        FIXME("Unsupported WSAPoll() flags 0x%x\n", events);
    return ret;
}

static int poll_compat(LPWSAPOLLFD *fds, ULONG nfds, int timeout)
{
    DWORD to = (timeout >= 0) ? (DWORD)timeout : INFINITE;
	DWORD ret = WSA_WAIT_FAILED;
	WSAEVENT *evts;	
	unsigned count = 0;
	unsigned i;
	struct timeval tv = { 0, 0 };

    if (nfds == 0)
    {    /* WSAWaitForMultipleEvents() does not allow zero events */
        if (SleepEx(to, TRUE))
        {
            errno = EINTR;
            return -1;
        }
        return 0;
    }

    evts = malloc(nfds * sizeof (WSAEVENT));
    if (evts == NULL)
        return -1; /* ENOMEM */

    
    for (i = 0; i < nfds; i++)
    {
        SOCKET fd = fds[i]->fd;
        long mask = FD_CLOSE;
        fd_set rdset, wrset, exset;

        FD_ZERO(&rdset);
        FD_ZERO(&wrset);
        FD_ZERO(&exset);
        FD_SET(fd, &exset);

        if (fds[i]->events & POLLRDNORM)
        {
            mask |= FD_READ | FD_ACCEPT;
            FD_SET(fd, &rdset);
        }
        if (fds[i]->events & POLLWRNORM)
        {
            mask |= FD_WRITE | FD_CONNECT;
            FD_SET(fd, &wrset);
        }
        if (fds[i]->events & POLLPRI)
            mask |= FD_OOB;

        fds[i]->revents = 0;

        evts[i] = WSACreateEvent();
        if (evts[i] == WSA_INVALID_EVENT)
        {
            while (i > 0)
                WSACloseEvent(evts[--i]);
            free(evts);
            errno = ENOMEM;
            return -1;
        }

        if (WSAEventSelect(fds[i]->fd, evts[i], mask)
         && WSAGetLastError() == WSAENOTSOCK)
            fds[i]->revents |= POLLNVAL;
        
        /* By its horrible design, WSAEnumNetworkEvents() only enumerates
         * events that were not already signaled (i.e. it is edge-triggered).
         * WSAPoll() would be better in this respect, but worse in others.
         * So use WSAEnumNetworkEvents() after manually checking for pending
         * events. */
        if (select(0, &rdset, &wrset, &exset, &tv) > 0)
        {
            if (FD_ISSET(fd, &rdset))
                fds[i]->revents |= fds[i]->events & POLLRDNORM;
            if (FD_ISSET(fd, &wrset))
                fds[i]->revents |= fds[i]->events & POLLWRNORM;
            if (FD_ISSET(fd, &exset))
                /* To add pain to injury, POLLERR and POLLPRI cannot be
                 * distinguished here. */
                fds[i]->revents |= POLLERR | (fds[i]->events & POLLPRI);
        }

        if (fds[i]->revents != 0 && ret == WSA_WAIT_FAILED)
            ret = WSA_WAIT_EVENT_0 + i;
    }

    if (ret == WSA_WAIT_FAILED)
        ret = WSAWaitForMultipleEvents(nfds, evts, FALSE, to, TRUE);
    
    for (i = 0; i < nfds; i++)
    {
        WSANETWORKEVENTS ne;

        if (WSAEnumNetworkEvents(fds[i]->fd, evts[i], &ne))
            memset(&ne, 0, sizeof (ne));
        WSAEventSelect(fds[i]->fd, evts[i], 0);
        WSACloseEvent(evts[i]);

        if (ne.lNetworkEvents & FD_CONNECT)
        {
            fds[i]->revents |= POLLWRNORM;
            if (ne.iErrorCode[FD_CONNECT_BIT] != 0)
                fds[i]->revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_CLOSE)
        {
            fds[i]->revents |= (fds[i]->events & POLLRDNORM) | POLLHUP;
            if (ne.iErrorCode[FD_CLOSE_BIT] != 0)
                fds[i]->revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_ACCEPT)
        {
            fds[i]->revents |= POLLRDNORM;
            if (ne.iErrorCode[FD_ACCEPT_BIT] != 0)
                fds[i]->revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_OOB)
        {
            fds[i]->revents |= POLLPRI;
            if (ne.iErrorCode[FD_OOB_BIT] != 0)
                fds[i]->revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_READ)
        {
            fds[i]->revents |= POLLRDNORM;
            if (ne.iErrorCode[FD_READ_BIT] != 0)
                fds[i]->revents |= POLLERR;
        }
        if (ne.lNetworkEvents & FD_WRITE)
        {
            fds[i]->revents |= POLLWRNORM;
            if (ne.iErrorCode[FD_WRITE_BIT] != 0)
                fds[i]->revents |= POLLERR;
        }
        count += fds[i]->revents != 0;
    }

    free(evts);

    if (count == 0 && ret == WSA_WAIT_IO_COMPLETION)
    {
        errno = EINTR;
        return -1;
    }
    return count;
}

int WINAPI WSAPoll(LPWSAPOLLFD fds, ULONG nfds, INT timeout)
{
    if (timeout < 0)
    {
        /* HACK: In some cases, we lose some events because events are
         * destroyed and recreated only when we need to poll. In order to work
         * arround this issue, we try to call the poll compat function every
         * 100ms (in case of infinite timeout). */
        int ret;
        while ((ret = poll_compat(&fds, nfds, 100)) == 0);
        return ret;
    }
    else
        return poll_compat(&fds, nfds, timeout);
}

/***********************************************************************
 *     WSAPoll
 */
    // WSAAPI
    // WSAPoll(
        // _Inout_ WSAPOLLFD *fdArray,
        // _In_ ULONG fds,
        // _In_ INT timeout
        // )
    // {
        // // if (auto const pWSAPoll = try_get_WSAPoll())
        // // {
            // // return pWSAPoll(fdArray, fds, timeout);
        // // }

        // fd_set        readfds;
        // fd_set        writefds;
        // fd_set        exceptfds;
		// ULONG i;
        // TIMEVAL* __ptimeout = NULL;
		// int result;

        // TIMEVAL time_out;		

        // FD_ZERO(&readfds);
        // FD_ZERO(&writefds);
        // FD_ZERO(&exceptfds);

        // for (i = 0; i < fds; i++)
        // {
            // WSAPOLLFD fd = fdArray[i];

            // //Read (in) socket
            // if (fd.events & (POLLRDNORM | POLLRDBAND | POLLPRI))
            // {
                // FD_SET(fd.fd, &readfds);
            // }

            // //Write (out) socket
            // if (fd.events & (POLLWRNORM | POLLWRBAND))
            // {
                // FD_SET(fd.fd, &writefds);
            // }

            // //异常
            // if (fd.events & (POLLERR | POLLHUP | POLLNVAL))
            // {
                // FD_SET(fd.fd, &exceptfds);
            // }
        // }


        // /*
        // timeout  < 0 ，无限等待
        // timeout == 0 ，马上回来
        // timeout  >0  ，最长等这个时间
        // */

        // if (timeout >= 0)
        // {
            // time_out.tv_sec = timeout / 1000;
            // time_out.tv_usec = (timeout % 1000) * 1000;
            // __ptimeout = &time_out;
        // }

        // result = select(1, &readfds, &writefds, &exceptfds, __ptimeout);

        // if (result > 0)
        // {
            // for (i = 0; i < fds; i++)
            // {
                // WSAPOLLFD fd = fdArray[i];

                // fd.revents = 0;
                // if (FD_ISSET(fd.fd, &readfds))
                    // fd.revents |= fd.events & (POLLRDNORM | POLLRDBAND | POLLPRI);

                // if (FD_ISSET(fd.fd, &writefds))
                    // fd.revents |= fd.events & (POLLWRNORM | POLLWRBAND);

                // if (FD_ISSET(fd.fd, &exceptfds))
                    // fd.revents |= fd.events & (POLLERR | POLLHUP | POLLNVAL);
            // }
        // }
    
        // return result;
    // }

// /***********************************************************************
// *              InetPtonW                      (WS2_32.@)
// */
// INT WSAAPI InetPtonW(INT family, PCWSTR addr, PVOID buffer)
// {
    // char *addrA;
    // int len;
    // INT ret;

    // TRACE("family %d, addr %s, buffer (%p)\n", family, debugstr_w(addr), buffer);

    // if (!addr)
    // {
        // SetLastError(WSAEFAULT);
        // return SOCKET_ERROR;
    // }

    // len = WideCharToMultiByte(CP_ACP, 0, addr, -1, NULL, 0, NULL, NULL);
    // if (!(addrA = HeapAlloc(GetProcessHeap(), 0, len)))
    // {
        // SetLastError(WSA_NOT_ENOUGH_MEMORY);
        // return SOCKET_ERROR;
    // }
    // WideCharToMultiByte(CP_ACP, 0, addr, -1, addrA, len, NULL, NULL);

    // ret = WS_inet_pton(family, addrA, buffer);

    // HeapFree(GetProcessHeap(), 0, addrA);
    // return ret;
// }

// /***********************************************************************
 // *              InetNtopW                      (WS2_32.@)
 // */
// PCWSTR WSAAPI InetNtopW(INT family, PVOID addr, PWSTR buffer, SIZE_T len)
// {
    // char bufferA[WS_INET6_ADDRSTRLEN];
    // PWSTR ret = NULL;

    // TRACE("family %d, addr (%p), buffer (%p), len %ld\n", family, addr, buffer, len);

    // if (WS_inet_ntop(family, addr, bufferA, sizeof(bufferA)))
    // {
        // if (MultiByteToWideChar(CP_ACP, 0, bufferA, -1, buffer, len))
            // ret = buffer;
        // else
            // SetLastError(ERROR_INVALID_PARAMETER);
    // }
    // return ret;
// }

static ADDRINFOEXW *addrinfo_AtoW(const PADDRINFOA ai)
{
    ADDRINFOEXW *ret;

    if (!(ret = HeapAlloc(GetProcessHeap(), 0, sizeof(ADDRINFOEXW)))) return NULL;
    ret->ai_flags     = ai->ai_flags;
    ret->ai_family    = ai->ai_family;
    ret->ai_socktype  = ai->ai_socktype;
    ret->ai_protocol  = ai->ai_protocol;
    ret->ai_addrlen   = ai->ai_addrlen;
    ret->ai_canonname = NULL;
    ret->ai_addr      = NULL;
    ret->ai_blob      = NULL;
    ret->ai_bloblen   = 0;
    ret->ai_provider  = NULL;
    ret->ai_next      = NULL;
    if (ai->ai_canonname)
    {
        int len = MultiByteToWideChar(CP_ACP, 0, ai->ai_canonname, -1, NULL, 0);
        if (!(ret->ai_canonname = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR))))
        {
            HeapFree(GetProcessHeap(), 0, ret);
            return NULL;
        }
        MultiByteToWideChar(CP_ACP, 0, ai->ai_canonname, -1, ret->ai_canonname, len);
    }
    if (ai->ai_addr)
    {
        if (!(ret->ai_addr = HeapAlloc(GetProcessHeap(), 0, ai->ai_addrlen)))
        {
            HeapFree(GetProcessHeap(), 0, ret->ai_canonname);
            HeapFree(GetProcessHeap(), 0, ret);
            return NULL;
        }
        memcpy(ret->ai_addr, ai->ai_addr, ai->ai_addrlen);
    }
    return ret;
}

static ADDRINFOEXW *addrinfo_list_AtoW(PADDRINFOA info)
{
    ADDRINFOEXW *ret, *infoW;

    if (!(ret = infoW = addrinfo_AtoW(info))) return NULL;
    while (info->ai_next)
    {
        if (!(infoW->ai_next = addrinfo_AtoW(info->ai_next)))
        {
            FreeAddrInfoExW(ret);
            return NULL;
        }
        infoW = infoW->ai_next;
        info = info->ai_next;
    }
    return ret;
}

static void WSAAPI getaddrinfo_callback(TP_CALLBACK_INSTANCE *instance, void *context)
{
    struct getaddrinfo_args *args = context;
    OVERLAPPED *overlapped = args->overlapped;
    HANDLE event = overlapped->hEvent;
    LPLOOKUPSERVICE_COMPLETION_ROUTINE completion_routine = args->completion_routine;
    PADDRINFOA res;
    int ret;

    ret = getaddrinfo(args->nodename, args->servname, NULL, &res);
    if (res)
    {
        *args->result = addrinfo_list_AtoW(res);
        overlapped->u.Pointer = args->result;
        freeaddrinfo(res);
    }

    HeapFree(GetProcessHeap(), 0, args->nodename);
    HeapFree(GetProcessHeap(), 0, args->servname);
    HeapFree(GetProcessHeap(), 0, args);

    overlapped->Internal = ret;
    if (completion_routine) completion_routine(ret, 0, (LPWSAOVERLAPPED)overlapped);
    if (event) SetEvent(event);
}

static int WS_getaddrinfoW(const WCHAR *nodename, const WCHAR *servname, const PADDRINFOA hints, ADDRINFOEXW **res,
                           OVERLAPPED *overlapped, LPLOOKUPSERVICE_COMPLETION_ROUTINE completion_routine)
{
    int ret = EAI_MEMORY, len, i;
    char *nodenameA = NULL, *servnameA = NULL;
    PADDRINFOA resA;
    WCHAR *local_nodenameW = (WCHAR *)nodename;

    *res = NULL;
    if (nodename)
    {
        /* Is this an IDN? Most likely if any char is above the Ascii table, this
         * is the simplest validation possible, further validation will be done by
         * the native getaddrinfo() */
        for (i = 0; nodename[i]; i++)
        {
            if (nodename[i] > 'z')
                break;
        }
        if (nodename[i])
        {
            if (hints && (hints->ai_flags & WS_AI_DISABLE_IDN_ENCODING))
            {
                /* Name requires conversion but it was disabled */
                ret = WSAHOST_NOT_FOUND;
                WSASetLastError(ret);
                goto end;
            }

            len = IdnToAscii(0, nodename, -1, NULL, 0);
            if (!len)
            {
                ERR("Failed to convert %s to punycode\n", debugstr_w(nodename));
                ret = EAI_FAIL;
                goto end;
            }
            if (!(local_nodenameW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR)))) goto end;
            IdnToAscii(0, nodename, -1, local_nodenameW, len);
        }
    }
    if (local_nodenameW)
    {
        len = WideCharToMultiByte(CP_ACP, 0, local_nodenameW, -1, NULL, 0, NULL, NULL);
        if (!(nodenameA = HeapAlloc(GetProcessHeap(), 0, len))) goto end;
        WideCharToMultiByte(CP_ACP, 0, local_nodenameW, -1, nodenameA, len, NULL, NULL);
    }
    if (servname)
    {
        len = WideCharToMultiByte(CP_ACP, 0, servname, -1, NULL, 0, NULL, NULL);
        if (!(servnameA = HeapAlloc(GetProcessHeap(), 0, len))) goto end;
        WideCharToMultiByte(CP_ACP, 0, servname, -1, servnameA, len, NULL, NULL);
    }

    if (overlapped)
    {
        struct getaddrinfo_args *args;

        if (overlapped->hEvent && completion_routine)
        {
            ret = WSAEINVAL;
            goto end;
        }

        if (!(args = HeapAlloc(GetProcessHeap(), 0, sizeof(*args)))) goto end;
        args->overlapped = overlapped;
        args->completion_routine = completion_routine;
        args->result = res;
        args->nodename = nodenameA;
        args->servname = servnameA;

        overlapped->Internal = WSAEINPROGRESS;
        if (!TrySubmitThreadpoolCallback(getaddrinfo_callback, args, NULL))
        {
            HeapFree(GetProcessHeap(), 0, args);
            ret = GetLastError();
            goto end;
        }

        if (local_nodenameW != nodename)
            HeapFree(GetProcessHeap(), 0, local_nodenameW);
        WSASetLastError(ERROR_IO_PENDING);
        return ERROR_IO_PENDING;
    }

    ret = getaddrinfo(nodenameA, servnameA, hints, &resA);
    if (!ret)
    {
        *res = addrinfo_list_AtoW(resA);
        freeaddrinfo(resA);
    }

end:
    if (local_nodenameW != nodename)
        HeapFree(GetProcessHeap(), 0, local_nodenameW);
    HeapFree(GetProcessHeap(), 0, nodenameA);
    HeapFree(GetProcessHeap(), 0, servnameA);
    return ret;
}

/***********************************************************************
 *		GetAddrInfoExW		(WS2_32.@)
 */
int WSAAPI GetAddrInfoExW(const WCHAR *name, const WCHAR *servname, DWORD namespace, GUID *namespace_id,
        const ADDRINFOEXW *hints, ADDRINFOEXW **result, struct WS_timeval *timeout, OVERLAPPED *overlapped,
        LPLOOKUPSERVICE_COMPLETION_ROUTINE completion_routine, HANDLE *handle)
{
    int ret;

    TRACE("(%s %s %x %s %p %p %p %p %p %p)\n", debugstr_w(name), debugstr_w(servname), namespace,
          debugstr_guid(namespace_id), hints, result, timeout, overlapped, completion_routine, handle);

    if (namespace != NS_DNS)
        FIXME("Unsupported namespace %u\n", namespace);
    if (namespace_id)
        FIXME("Unsupported naemspace_id %s\n", debugstr_guid(namespace_id));
    if (hints)
        FIXME("Unsupported hints\n");
    if (timeout)
        FIXME("Unsupported timeout\n");
    if (handle)
        FIXME("Unsupported cancel handle\n");

    ret = WS_getaddrinfoW(name, servname, NULL, result, overlapped, completion_routine);
    if (ret) return ret;
    if (handle) *handle = (HANDLE)0xdeadbeef;
    return 0;
}

/***********************************************************************
 *      FreeAddrInfoExW      (WS2_32.@)
 */
void WSAAPI FreeAddrInfoExW(ADDRINFOEXW *ai)
{
    TRACE("(%p)\n", ai);

    while (ai)
    {
        ADDRINFOEXW *next;
        HeapFree(GetProcessHeap(), 0, ai->ai_canonname);
        HeapFree(GetProcessHeap(), 0, ai->ai_addr);
        next = ai->ai_next;
        HeapFree(GetProcessHeap(), 0, ai);
        ai = next;
    }
}

/***********************************************************************
 *              WSCGetProviderInfo
 */
INT WSAAPI WSCGetProviderInfo( LPGUID provider, WSC_PROVIDER_INFO_TYPE info_type,
                               PBYTE info, size_t* len, DWORD flags, LPINT errcode )
{
    FIXME( "(%s 0x%08x %p %p 0x%08x %p) Stub!\n",
           debugstr_guid(provider), info_type, info, len, flags, errcode );

    if (!errcode)
        return SOCKET_ERROR;

    if (!provider) {
        *errcode = WSAEFAULT;
        return SOCKET_ERROR;
    }

    *errcode = WSANO_RECOVERY;
    return SOCKET_ERROR;
}

/***********************************************************************
 *      FreeAddrInfoEx      (WS2_32.@)
 */
void WSAAPI FreeAddrInfoEx(ADDRINFOEXA *ai)
{
    TRACE("(%p)\n", ai);

    while (ai)
    {
        ADDRINFOEXA *next;
        HeapFree(GetProcessHeap(), 0, ai->ai_canonname);
        HeapFree(GetProcessHeap(), 0, ai->ai_addr);
        next = ai->ai_next;
        HeapFree(GetProcessHeap(), 0, ai);
        ai = next;
    }
}

/***********************************************************************
 *		GetAddrInfoExOverlappedResult  (WS2_32.@)
 */
int WINAPI GetAddrInfoExOverlappedResult( OVERLAPPED *overlapped )
{
    TRACE( "(%p)\n", overlapped );

    if (!overlapped)
        return WSAEINVAL;

    return overlapped->Internal;
}

/***********************************************************************
 *		GetAddrInfoExCancel     (WS2_32.@)
 */
int WSAAPI GetAddrInfoExCancel(HANDLE *handle)
{
    FIXME("(%p)\n", handle);
    return WSA_INVALID_HANDLE;
}

/***********************************************************************
 *     WSASendMsg
 */
int WINAPI WSASendMsg( SOCKET s, LPWSAMSG msg, DWORD dwFlags, LPDWORD lpNumberOfBytesSent,
                       LPWSAOVERLAPPED lpOverlapped,
                       LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    if (!msg)
    {
        SetLastError( WSAEFAULT );
        return SOCKET_ERROR;
    }

    return WSASendTo( s, msg->lpBuffers, msg->dwBufferCount, lpNumberOfBytesSent,
                       dwFlags, msg->name, msg->namelen,
                       lpOverlapped, lpCompletionRoutine );
}

/***********************************************************************
 *          WSAConnectByNameA      (WS2_32.@)
 */
BOOL WINAPI WSAConnectByNameA(SOCKET s, const char *node_name, const char *service_name,
                              DWORD *local_addr_len, struct sockaddr *local_addr,
                              DWORD *remote_addr_len, struct sockaddr *remote_addr,
                              const struct timeval *timeout, WSAOVERLAPPED *reserved)
{
    WSAPROTOCOL_INFOA proto_info;
    WSAPOLLFD pollout;
    struct addrinfo *service, hints;
    int ret, proto_len, sockaddr_size, sockname_size, sock_err, int_len;

    TRACE("socket %#Ix, node_name %s, service_name %s, local_addr_len %p, local_addr %p, \
          remote_addr_len %p, remote_addr %p, timeout %p, reserved %p\n",
          s, debugstr_a(node_name), debugstr_a(service_name), local_addr_len, local_addr,
          remote_addr_len, remote_addr, timeout, reserved );

    if (!node_name || !service_name || reserved)
    {
        SetLastError(WSAEINVAL);
        return FALSE;
    }

    if (!s)
    {
        SetLastError(WSAENOTSOCK);
        return FALSE;
    }

    if (timeout)
        FIXME("WSAConnectByName timeout stub\n");

    proto_len = sizeof(WSAPROTOCOL_INFOA);
    ret = getsockopt(s, SOL_SOCKET, SO_PROTOCOL_INFOA, (char *)&proto_info, &proto_len);
    if (ret)
        return FALSE;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = proto_info.iSocketType;
    hints.ai_family = proto_info.iAddressFamily;
    hints.ai_protocol = proto_info.iProtocol;
    ret = getaddrinfo(node_name, service_name, &hints, &service);
    if (ret)
        return FALSE;

    if (proto_info.iSocketType != SOCK_STREAM)
    {
        freeaddrinfo(service);
        SetLastError(WSAEFAULT);
        return FALSE;
    }

    switch (proto_info.iAddressFamily)
    {
    case AF_INET:
        sockaddr_size = sizeof(SOCKADDR_IN);
        break;
    case AF_INET6:
        sockaddr_size = sizeof(SOCKADDR_IN6);
        break;
    default:
        freeaddrinfo(service);
        SetLastError(WSAENOTSOCK);
        return FALSE;
    }

    ret = connect(s, service->ai_addr, sockaddr_size);
    if (ret)
    {
        freeaddrinfo(service);
        return FALSE;
    }

    pollout.fd = s;
    pollout.events = POLLWRNORM;
    ret = WSAPoll(&pollout, 1, -1);
    if (ret == SOCKET_ERROR)
    {
        freeaddrinfo(service);
        return FALSE;
    }
    if (pollout.revents & (POLLERR | POLLHUP | POLLNVAL))
    {
        freeaddrinfo(service);
        int_len = sizeof(int);
        ret = getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&sock_err, &int_len);
        if (ret == SOCKET_ERROR)
            return FALSE;
        SetLastError(sock_err);
        return FALSE;
    }

    if (remote_addr_len && remote_addr)
    {
        if (*remote_addr_len >= sockaddr_size)
        {
            memcpy(remote_addr, service->ai_addr, sockaddr_size);
            *remote_addr_len = sockaddr_size;
        }
        else
        {
            freeaddrinfo(service);
            SetLastError(WSAEFAULT);
            return FALSE;
        }
    }

    freeaddrinfo(service);

    if (local_addr_len && local_addr)
    {
        if (*local_addr_len >= sockaddr_size)
        {
            sockname_size = sockaddr_size;
            ret = getsockname(s, local_addr, &sockname_size);
            if (ret)
                return FALSE;
            if (proto_info.iAddressFamily == AF_INET6)
                ((SOCKADDR_IN6 *)local_addr)->sin6_port = 0;
            else
                ((SOCKADDR_IN *)local_addr)->sin_port = 0;
            *local_addr_len = sockaddr_size;
        }
        else
        {
            SetLastError(WSAEFAULT);
            return FALSE;
        }
    }

    return TRUE;
}

/***********************************************************************
 *          WSAConnectByNameW      (WS2_32.@)
 */
BOOL 
WINAPI 
WSAConnectByNameW(
	SOCKET s, 
	LPWSTR node_name, 
	LPWSTR service_name,
    DWORD *local_addr_len, 
	struct sockaddr *local_addr,
    DWORD *remote_addr_len, 
	struct sockaddr *remote_addr,
    const struct timeval *timeout, 
	WSAOVERLAPPED *reserved)
{
    char *node_nameA, *service_nameA;
    BOOL ret;

    if (!node_name || !service_name)
    {
        SetLastError(WSAEINVAL);
        return FALSE;
    }

    node_nameA = strdupWtoA(node_name);
    service_nameA = strdupWtoA(service_name);
    if (!node_nameA || !service_nameA)
    {
        SetLastError(WSAENOBUFS);
        return FALSE;
    }

    ret = WSAConnectByNameA(s, node_nameA, service_nameA, local_addr_len, local_addr,
                             remote_addr_len, remote_addr, timeout, reserved);
    free(node_nameA);
    free(service_nameA);
    return ret;
}

PCSTR
WINAPI
inet_ntop(
    _In_ INT Family,
    _In_ const VOID *pAddr,
    _Out_writes_(StringBufSize) PSTR pStringBuf,
    _In_ size_t StringBufSize)
{
    NTSTATUS Status;
    ULONG BufSize = StringBufSize;

    switch (Family)
    {
        case AF_INET:
            Status = RtlIpv4AddressToStringExA(pAddr, 0, pStringBuf, &BufSize);
            break;
        case AF_INET6:
            Status = RtlIpv6AddressToStringExA(pAddr, 0, 0, pStringBuf, &BufSize);
            break;
        default:
            SetLastError(WSAEAFNOSUPPORT);
            return NULL;
    }

    if (!NT_SUCCESS(Status)) 
    {
        SetLastError(WSAEINVAL);
        return NULL;
    }

    return pStringBuf;
}

PCWSTR
WINAPI
InetNtopW(
    _In_ INT Family,
    _In_ const VOID *pAddr,
    _Out_writes_(StringBufSize) PWSTR pStringBuf,
    _In_ size_t StringBufSize)
{
    NTSTATUS Status;
    ULONG BufSize = StringBufSize;

    switch (Family)
    {
        case AF_INET:
            Status = RtlIpv4AddressToStringExW(pAddr, 0, pStringBuf, &BufSize);
            break;
        case AF_INET6:
            Status = RtlIpv6AddressToStringExW(pAddr, 0, 0, pStringBuf, &BufSize);
            break;
        default:
            SetLastError(WSAEAFNOSUPPORT);
            return NULL;
    }

    if (!NT_SUCCESS(Status)) 
    {
        SetLastError(WSAEINVAL);
        return NULL;
    }

    return pStringBuf;
}

INT
WINAPI
inet_pton(
    _In_ INT Family,
    _In_ PCSTR pszAddrString,
    _Out_writes_bytes_(sizeof(IN_ADDR6)) PVOID pAddrBuf)
{
    NTSTATUS Status;
    PCSTR ch;

    if (!pszAddrString || !pAddrBuf)
    {
        SetLastError(WSAEFAULT);
        return -1;
    }

    switch (Family)
    {
        case AF_INET:
            Status = RtlIpv4StringToAddressA(pszAddrString, TRUE, &ch, pAddrBuf);
            break;
        case AF_INET6:
            Status = RtlIpv6StringToAddressA(pszAddrString, &ch, pAddrBuf);
            break;
        default:
            SetLastError(WSAEAFNOSUPPORT);
            return -1;
    }

    if (!NT_SUCCESS(Status) || (*ch != 0)) 
    {
        return 0;
    }

    return 1;
}

INT
WINAPI
InetPtonW(
    _In_ INT Family,
    _In_ PCWSTR pszAddrString,
    _Out_writes_bytes_(sizeof(IN_ADDR6)) PVOID pAddrBuf)
{
    NTSTATUS Status;
    PCWSTR ch;

    if (!pszAddrString || !pAddrBuf)
    {
        SetLastError(WSAEFAULT);
        return -1;
    }

    switch (Family)
    {
        case AF_INET:
            Status = RtlIpv4StringToAddressW(pszAddrString, TRUE, &ch, pAddrBuf);
            break;
        case AF_INET6:
            Status = RtlIpv6StringToAddressW(pszAddrString, &ch, pAddrBuf);
            break;
        default:
            SetLastError(WSAEAFNOSUPPORT);
            return -1;
    }

    if (!NT_SUCCESS(Status) || (*ch != 0)) 
    {
        SetLastError(WSAEINVAL); /* Only unicode version sets this error */
        return 0;
    }

    return 1;
}