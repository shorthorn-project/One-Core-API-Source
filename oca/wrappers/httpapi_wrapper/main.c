/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    main.c

Abstract:

    Implement DirectWrite wrapper functions

Author:

    Skulltrail 11-September-2025

Revision History:

--*/

#define COBJMACROS

#include <stdarg.h>
#include <math.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "http.h"

#include "initguid.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dwrite);

#define NtCurrentPeb() (NtCurrentTeb()->Peb)

typedef struct _XP_HTTP_URL_ENTRY
{
    struct _XP_HTTP_URL_ENTRY *Next;
    WCHAR *Url;
    HTTP_URL_CONTEXT Context;
} XP_HTTP_URL_ENTRY;

typedef struct _XP_HTTP_URL_GROUP XP_HTTP_URL_GROUP;
typedef struct _XP_HTTP_SERVER_SESSION XP_HTTP_SERVER_SESSION;

struct _XP_HTTP_URL_GROUP
{
    XP_HTTP_URL_GROUP *Next;
    XP_HTTP_URL_GROUP *SessionNext;

    HTTP_URL_GROUP_ID Id;
    HTTP_SERVER_SESSION_ID ServerSessionId;

    HANDLE RequestQueue;

    XP_HTTP_URL_ENTRY *Urls;
};

struct _XP_HTTP_SERVER_SESSION
{
    XP_HTTP_SERVER_SESSION *Next;

    HTTP_SERVER_SESSION_ID Id;

    XP_HTTP_URL_GROUP *UrlGroups;
};

typedef struct _XP_HTTP_REQUEST_QUEUE
{
    struct _XP_HTTP_REQUEST_QUEUE *Next;

    HANDLE Handle;

} XP_HTTP_REQUEST_QUEUE;

static XP_HTTP_REQUEST_QUEUE *g_RequestQueues = NULL;
static XP_HTTP_SERVER_SESSION *g_ServerSessions = NULL;
static XP_HTTP_URL_GROUP *g_UrlGroups = NULL;

static HTTP_SERVER_SESSION_ID g_NextServerSessionId = 1;
static HTTP_URL_GROUP_ID g_NextUrlGroupId = 1;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( hinstDLL );
        break;
    case DLL_PROCESS_DETACH:
		break;
    }
    return TRUE;
}

static WCHAR *
DuplicateString(PCWSTR String)
{
    SIZE_T Length;
    WCHAR *Copy;

    if (String == NULL)
        return NULL;

    Length = (lstrlenW(String) + 1) * sizeof(WCHAR);

    Copy = HeapAlloc(
        GetProcessHeap(),
        0,
        Length);

    if (Copy == NULL)
        return NULL;

    CopyMemory(
        Copy,
        String,
        Length);

    return Copy;
}

static XP_HTTP_URL_GROUP *
CreateUrlGroup(XP_HTTP_SERVER_SESSION *ServerSession)
{
    XP_HTTP_URL_GROUP *Group;

    Group = HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(XP_HTTP_URL_GROUP));

    if (Group == NULL)
        return NULL;

    Group->Id = g_NextUrlGroupId++;
    Group->ServerSessionId = ServerSession->Id;

    /*
     * Ainda não existe Request Queue associada.
     */
    Group->RequestQueue = NULL;

    Group->Urls = NULL;

    /*
     * Lista global.
     */
    Group->Next = g_UrlGroups;
    g_UrlGroups = Group;

    /*
     * Lista da Server Session.
     */
    Group->SessionNext = ServerSession->UrlGroups;
    ServerSession->UrlGroups = Group;

    return Group;
}

static XP_HTTP_URL_GROUP *
FindUrlGroup(HTTP_URL_GROUP_ID Id)
{
    XP_HTTP_URL_GROUP *Group;

    Group = g_UrlGroups;

    while (Group != NULL)
    {
        if (Group->Id == Id)
            return Group;

        Group = Group->Next;
    }

    return NULL;
}

static XP_HTTP_SERVER_SESSION *
FindServerSession(HTTP_SERVER_SESSION_ID Id)
{
    XP_HTTP_SERVER_SESSION *Session;

    Session = g_ServerSessions;

    while (Session != NULL)
    {
        if (Session->Id == Id)
            return Session;

        Session = Session->Next;
    }

    return NULL;
}

static XP_HTTP_SERVER_SESSION *
CreateServerSession(void)
{
    XP_HTTP_SERVER_SESSION *Session;

    Session = HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(XP_HTTP_SERVER_SESSION));

    if (Session == NULL)
        return NULL;

    Session->Id = g_NextServerSessionId++;
    Session->UrlGroups = NULL;

    Session->Next = g_ServerSessions;
    g_ServerSessions = Session;

    return Session;
}

ULONG WINAPI
HttpCreateUrlGroup(
    HTTP_SERVER_SESSION_ID ServerSessionId,
    PHTTP_URL_GROUP_ID UrlGroupId,
    ULONG Reserved)
{
    XP_HTTP_SERVER_SESSION *ServerSession;
    XP_HTTP_URL_GROUP *Group;

    if (Reserved != 0)
        return ERROR_INVALID_PARAMETER;

    if (UrlGroupId == NULL)
        return ERROR_INVALID_PARAMETER;

    ServerSession = FindServerSession(ServerSessionId);

    if (ServerSession == NULL)
        return ERROR_INVALID_PARAMETER;

    Group = CreateUrlGroup(ServerSession);

    if (Group == NULL)
        return ERROR_OUTOFMEMORY;

    *UrlGroupId = Group->Id;

    return NO_ERROR;
}

ULONG WINAPI
HttpAddUrlToUrlGroup(
    HTTP_URL_GROUP_ID UrlGroupId,
    PCWSTR pFullyQualifiedUrl,
    HTTP_URL_CONTEXT UrlContext,
    ULONG Reserved)
{
    XP_HTTP_URL_GROUP *Group;
    XP_HTTP_URL_ENTRY *Entry;
    ULONG Status;

    if (Reserved != 0)
        return ERROR_INVALID_PARAMETER;

    if (pFullyQualifiedUrl == NULL)
        return ERROR_INVALID_PARAMETER;

    Group = FindUrlGroup(UrlGroupId);

    if (Group == NULL)
        return ERROR_INVALID_PARAMETER;

    if (Group->RequestQueue == NULL)
        return ERROR_INVALID_PARAMETER;

    /*
     * Evita registrar exatamente a mesma URL duas vezes
     * dentro do nosso grupo.
     */
    Entry = Group->Urls;

    while (Entry != NULL)
    {
        if (lstrcmpW(Entry->Url, pFullyQualifiedUrl))
            return ERROR_ALREADY_EXISTS;

        Entry = Entry->Next;
    }

    /*
     * Primeiro registra na HTTP API 1.0 do XP.
     */
    Status = HttpAddUrl(
        Group->RequestQueue,
        pFullyQualifiedUrl,
        (PVOID)UrlContext);

    if (Status != NO_ERROR)
        return Status;

    /*
     * Cria nossa representação da URL.
     */
    Entry = HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(XP_HTTP_URL_ENTRY));

    if (Entry == NULL)
        return ERROR_OUTOFMEMORY;

    Entry->Url = DuplicateString(pFullyQualifiedUrl);

    if (Entry->Url == NULL)
    {
        HeapFree(GetProcessHeap(), 0, Entry);
        return ERROR_OUTOFMEMORY;
    }

    Entry->Context = UrlContext;

    Entry->Next = Group->Urls;
    Group->Urls = Entry;

    return NO_ERROR;
}

ULONG WINAPI
HttpCreateServerSession(
    HTTPAPI_VERSION Version,
    PHTTP_SERVER_SESSION_ID ServerSessionId,
    ULONG Reserved)
{
    XP_HTTP_SERVER_SESSION *Session;

    if (ServerSessionId == NULL)
        return ERROR_INVALID_PARAMETER;

    if (Reserved != 0)
        return ERROR_INVALID_PARAMETER;

    if (Version.HttpApiMajorVersion != 2 ||
        Version.HttpApiMinorVersion != 0)
    {
        return ERROR_REVISION_MISMATCH;
    }

    Session = CreateServerSession();

    if (Session == NULL)
        return ERROR_OUTOFMEMORY;

    *ServerSessionId = Session->Id;

    return NO_ERROR;
}

ULONG WINAPI
HttpRemoveUrlFromUrlGroup(
    HTTP_URL_GROUP_ID UrlGroupId,
    PCWSTR pFullyQualifiedUrl,
    ULONG Flags)
{
    XP_HTTP_URL_GROUP *Group;
    XP_HTTP_URL_ENTRY *Entry;
    XP_HTTP_URL_ENTRY *Previous;
    ULONG Status;

    /*
     * Atualmente não há flags suportadas.
     */
    if (Flags != 0)
        return ERROR_INVALID_PARAMETER;

    if (pFullyQualifiedUrl == NULL)
        return ERROR_INVALID_PARAMETER;

    Group = FindUrlGroup(UrlGroupId);

    if (Group == NULL)
        return ERROR_INVALID_PARAMETER;

    if (Group->RequestQueue == NULL)
        return ERROR_INVALID_PARAMETER;

    Previous = NULL;
    Entry = Group->Urls;

    while (Entry != NULL)
    {
        if (lstrcmpW(Entry->Url, pFullyQualifiedUrl) == 0)
            break;

        Previous = Entry;
        Entry = Entry->Next;
    }

    if (Entry == NULL)
        return ERROR_FILE_NOT_FOUND;

    /*
     * Primeiro remove a URL da HTTP API 1.0.
     */
    Status = HttpRemoveUrl(
        Group->RequestQueue,
        pFullyQualifiedUrl);

    if (Status != NO_ERROR)
        return Status;

    /*
     * Remove da nossa lista.
     */
    if (Previous != NULL)
        Previous->Next = Entry->Next;
    else
        Group->Urls = Entry->Next;

    /*
     * Libera a string e a entrada.
     */
    if (Entry->Url != NULL)
    {
        HeapFree(
            GetProcessHeap(),
            0,
            Entry->Url);
    }

    HeapFree(
        GetProcessHeap(),
        0,
        Entry);

    return NO_ERROR;
}

ULONG WINAPI
HttpSetUrlGroupProperty(
    HTTP_URL_GROUP_ID UrlGroupId,
    HTTP_SERVER_PROPERTY Property,
    PVOID PropertyInformation,
    ULONG PropertyInformationLength)
{
    XP_HTTP_URL_GROUP *Group;
    HTTP_BINDING_INFO *BindingInfo;

    if (PropertyInformation == NULL)
        return ERROR_INVALID_PARAMETER;

    if (PropertyInformationLength == 0)
        return ERROR_INVALID_PARAMETER;

    Group = FindUrlGroup(UrlGroupId);

    if (Group == NULL)
        return ERROR_INVALID_PARAMETER;

    switch (Property)
    {
        case HttpServerBindingProperty:

            if (PropertyInformationLength !=
                sizeof(HTTP_BINDING_INFO))
            {
                return ERROR_INVALID_PARAMETER;
            }

            BindingInfo =
                (HTTP_BINDING_INFO *)PropertyInformation;

            /*
             * RequestQueueHandle == NULL remove o binding
             * existente.
             */
            if (BindingInfo->RequestQueueHandle == NULL)
            {
                Group->RequestQueue = NULL;
                return NO_ERROR;
            }

            /*
             * Associa o URL Group à Request Queue.
             */
            Group->RequestQueue =
                BindingInfo->RequestQueueHandle;

            return NO_ERROR;

        default:

            /*
             * Ainda não implementamos as outras propriedades.
             */
            return ERROR_INVALID_PARAMETER;
    }
}

ULONG WINAPI
HttpCreateRequestQueue(
    HTTPAPI_VERSION Version,
    PCWSTR Name,
    PSECURITY_ATTRIBUTES SecurityAttributes,
    ULONG Flags,
    PHANDLE RequestQueueHandle)
{
    XP_HTTP_REQUEST_QUEUE *Queue;
    HANDLE Handle;
    ULONG Status;

    if (RequestQueueHandle == NULL)
        return ERROR_INVALID_PARAMETER;

    *RequestQueueHandle = NULL;

    if (Version.HttpApiMajorVersion != 2 ||
        Version.HttpApiMinorVersion != 0)
    {
        return ERROR_REVISION_MISMATCH;
    }

    if (Flags != 0)
        return ERROR_INVALID_PARAMETER;

    /*
     * Name e SecurityAttributes não são utilizados pela
     * implementação baseada na HTTP API 1.0.
     */
    (void)Name;
    (void)SecurityAttributes;

    Status = HttpCreateHttpHandle(
        &Handle,
        0);

    if (Status != NO_ERROR)
        return Status;

    Queue = HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(XP_HTTP_REQUEST_QUEUE));

    if (Queue == NULL)
    {
        CloseHandle(Handle);
        return ERROR_OUTOFMEMORY;
    }

    Queue->Handle = Handle;

    Queue->Next = g_RequestQueues;
    g_RequestQueues = Queue;

    *RequestQueueHandle = Handle;

    return NO_ERROR;
}

ULONG WINAPI
HttpShutdownRequestQueue(
    HANDLE RequestQueueHandle)
{
    XP_HTTP_REQUEST_QUEUE *Queue;
    XP_HTTP_REQUEST_QUEUE *Previous;
    XP_HTTP_URL_GROUP *Group;

    if (RequestQueueHandle == NULL)
        return ERROR_INVALID_HANDLE;

    Queue = g_RequestQueues;
    Previous = NULL;

    while (Queue != NULL)
    {
        if (Queue->Handle == RequestQueueHandle)
            break;

        Previous = Queue;
        Queue = Queue->Next;
    }

    if (Queue == NULL)
        return ERROR_INVALID_HANDLE;

    /*
     * Desassocia a queue de todos os URL Groups.
     */
    Group = g_UrlGroups;

    while (Group != NULL)
    {
        if (Group->RequestQueue == RequestQueueHandle)
            Group->RequestQueue = NULL;

        Group = Group->Next;
    }

    /*
     * Remove da lista.
     */
    if (Previous != NULL)
        Previous->Next = Queue->Next;
    else
        g_RequestQueues = Queue->Next;

    CloseHandle(Queue->Handle);

    HeapFree(
        GetProcessHeap(),
        0,
        Queue);

    return NO_ERROR;
}

// ULONG WINAPI
// HttpWaitForDisconnectEx(
	// IN HANDLE             RequestQueueHandle,
    // IN HTTP_CONNECTION_ID ConnectionId,
    // IN ULONG   Reserved   OPTIONAL
    // IN LPOVERLAPPED       Overlapped OPTIONAL)
// {
	// return HttpWaitForDisconnect(RequestQueueHandle, ConnectionId, Overlapped);
// }