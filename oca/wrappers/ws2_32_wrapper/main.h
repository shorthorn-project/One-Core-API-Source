#define WIN32_NO_STATUS

#include <stdarg.h>
#include <math.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include <windef.h>
#include <winbase.h>

#include <wine/debug.h>
#include <wine/list.h>
#include <wine/unicode.h>
#include <winsock2.h>
/* PSDK and NDK Headers */
#include <winreg.h>
#include <winnls.h>
#include <winuser.h>
#include <ndk/rtlfuncs.h>
#include <pseh/pseh2.h>


#define POLLHUP                    0x0002
#define WS_POLLHUP                 0x0002
#define WS_POLLNVAL                0x0004

#define POLLERR                    0x0001
#define POLLHUP                    0x0002
#define POLLNVAL                   0x0004
#define POLLWRNORM                 0x0010
#define POLLWRBAND                 0x0020
#define POLLRDNORM                 0x0100
#define POLLRDBAND                 0x0200
#define POLLPRI                    0x0400
#define POLLIN                     (POLLRDNORM|POLLRDBAND)
#define POLLOUT                    (POLLWRNORM)

#define WS_POLLERR                 0x0001
#define WS_POLLHUP                 0x0002
#define WS_POLLNVAL                0x0004
#define WS_POLLWRNORM              0x0010
#define WS_POLLWRBAND              0x0020
#define WS_POLLRDNORM              0x0100
#define WS_POLLRDBAND              0x0200
#define WS_POLLPRI                 0x0400
#define WS_POLLIN                  (WS_POLLRDNORM|WS_POLLRDBAND)
#define WS_POLLOUT                 (WS_POLLWRNORM)
#define WSA_NOT_ENOUGH_MEMORY      (ERROR_NOT_ENOUGH_MEMORY)
# define WS_AI_DISABLE_IDN_ENCODING   0x00080000

#define WS_INET6_ADDRSTRLEN     65
#define NS_DNS         12

#define MAP_OPTION(opt) { WS_##opt, opt }

#define WSAEVENT HANDLE

/* getaddrinfo error codes */
#define EAI_AGAIN WSATRY_AGAIN
#define EAI_BADFLAGS WSAEINVAL
#define EAI_FAIL WSANO_RECOVERY
#define EAI_FAMILY WSAEAFNOSUPPORT
#define EAI_MEMORY WSA_NOT_ENOUGH_MEMORY
#define EAI_NODATA EAI_NONAME
#define EAI_NOSECURENAME WSA_SECURE_HOST_NOT_FOUND
#define EAI_NONAME WSAHOST_NOT_FOUND
#define EAI_SERVICE WSATYPE_NOT_FOUND
#define EAI_SOCKTYPE WSAESOCKTNOSUPPORT
#define EAI_IPSECPOLICY WSA_IPSEC_NAME_POLICY_ERROR

#define WSA_INVALID_HANDLE         (ERROR_INVALID_HANDLE)

#define MAX_PROTOCOL_CHAIN 7

#define WSAPROTOCOL_LEN 255

#define WSA_FLAG_NO_HANDLE_INHERIT      0x0080
#define WSA_FLAG_REGISTERED_IO          0x0100

#define WSAAPI FAR PASCAL

#define IPV6_V6ONLY           27
#define IPPROTO_IPV6          41

#define TCP_KEEPALIVE                   3

#define SO_PROTOCOL_INFOA          0x2004

#define IOC_UNIX                       0x00000000
#define IOC_WS2                        0x08000000
#define IOC_PROTOCOL                   0x10000000
#define IOC_VENDOR                     0x18000000

#define _WSAIO(x,y)                    (IOC_VOID|(x)|(y))
#define _WSAIOR(x,y)                   (IOC_OUT|(x)|(y))
#define _WSAIOW(x,y)                   (IOC_IN|(x)|(y))
#define _WSAIORW(x,y)                  (IOC_INOUT|(x)|(y))

#define SIO_BASE_HANDLE 0x48000022
#define SIO_BSP_HANDLE 0x4800001B
#define SIO_BSP_HANDLE_SELECT 0x4800001C
#define SIO_BSP_HANDLE_POLL 0x4800001D
#define SIO_LOOPBACK_FAST_PATH 0x98000010
#define SIO_UDP_NETRESET  _WSAIOW(IOC_VENDOR,15)
#define WSA_WAIT_FAILED ((DWORD)-1L)
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#define WSA_INVALID_EVENT ((WSAEVENT)NULL)

static const unsigned __int64 epoch = ((unsigned __int64) 116444736000000000ULL);

typedef enum _WSC_PROVIDER_INFO_TYPE
{
    ProviderInfoLspCategories,
    ProviderInfoAudit,
} WSC_PROVIDER_INFO_TYPE;

typedef struct pollfd {
  SOCKET fd;
  SHORT events;
  SHORT revents;
} WSAPOLLFD, *PWSAPOLLFD, FAR *LPWSAPOLLFD;

int poll( struct pollfd *fds, unsigned int count, int timeout );

typedef struct addrinfoexW {
  int                ai_flags;
  int                ai_family;
  int                ai_socktype;
  int                ai_protocol;
  size_t             ai_addrlen;
  PWSTR              ai_canonname;
  struct sockaddr    *ai_addr;
  void               *ai_blob;
  size_t             ai_bloblen;
  LPGUID             ai_provider;
  struct addrinfoexW *ai_next;
} ADDRINFOEXW, *PADDRINFOEXW, *LPADDRINFOEXW;

// typedef struct _WSAOVERLAPPED {
  // DWORD    Internal;
  // DWORD    InternalHigh;
  // DWORD    Offset;
  // DWORD    OffsetHigh;
  // WSAEVENT hEvent;
// } WSAOVERLAPPED, *LPWSAOVERLAPPED;

struct timezone {
    int tz_minuteswest; /* minutes W of Greenwich */
    int tz_dsttime;     /* type of dst correction */
};

typedef
void
(CALLBACK * LPLOOKUPSERVICE_COMPLETION_ROUTINE)(
    _In_      DWORD    dwError,
    _In_      DWORD    dwBytes,
    _In_      LPWSAOVERLAPPED lpOverlapped
    );
	
struct getaddrinfo_args
{
    OVERLAPPED *overlapped;
    LPLOOKUPSERVICE_COMPLETION_ROUTINE completion_routine;
    ADDRINFOEXW **result;
    char *nodename;
    char *servname;
};	

// typedef struct addrinfo {
  // int             ai_flags;
  // int             ai_family;
  // int             ai_socktype;
  // int             ai_protocol;
  // size_t          ai_addrlen;
  // char            *ai_canonname;
  // struct sockaddr *ai_addr;
  // struct addrinfo *ai_next;
// } ADDRINFOA, *PADDRINFOA;

// typedef struct addrinfoW {
  // int ai_flags;
  // int ai_family;
  // int ai_socktype;
  // int ai_protocol;
  // size_t ai_addrlen;
  // PWSTR ai_canonname;
  // struct sockaddr *ai_addr;
  // struct addrinfoW *ai_next;
// } ADDRINFOW, *PADDRINFOW;

typedef struct addrinfoexA {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    SIZE_T ai_addrlen;
    char *ai_canonname;
    struct sockaddr *ai_addr;
    void *ai_blob;
    SIZE_T ai_bloblen;
    GUID *ai_provider;
    struct addrinfoexA *ai_next;
} ADDRINFOEXA, *PADDRINFOEXA, *LPADDRINFOEXA;

void WINAPI FreeAddrInfoExW(ADDRINFOEXW*);

INT
GetAddrInfoW(
  _In_opt_ PCWSTR pNodeName,
  _In_opt_ PCWSTR pServiceName,
  _In_opt_ const ADDRINFOW *pHints,
  _Outptr_ PADDRINFOW *ppResult);
  
INT
WINAPI
getaddrinfo(
  _In_opt_ PCSTR pNodeName,
  _In_opt_ PCSTR pServiceName,
  _In_opt_ const ADDRINFOA *pHints,
  _Outptr_ PADDRINFOA *ppResult);  
  
VOID
WINAPI
freeaddrinfo(
  _In_opt_ PADDRINFOA pAddrInfo); 
  
WINAPI IdnToAscii(DWORD,LPCWSTR,INT,LPWSTR,INT);
WINAPI TrySubmitThreadpoolCallback(PTP_SIMPLE_CALLBACK,void*,TP_CALLBACK_ENVIRON*);

struct gethostname_params
{
    char *name;
    unsigned int size;
};

typedef void (CALLBACK *LPWSAOVERLAPPED_COMPLETION_ROUTINE)
(
    DWORD dwError,
    DWORD cbTransferred,
    LPWSAOVERLAPPED lpOverlapped,
    DWORD dwFlags
);

// typedef struct _WSABUF {
  // ULONG len;
  // CHAR FAR *buf;
// } WSABUF, FAR * LPWSABUF;

// typedef struct _WSAMSG {
  // LPSOCKADDR name;
  // INT namelen;
  // LPWSABUF lpBuffers;
// #if (_WIN32_WINNT >= 0x0600)
  // ULONG dwBufferCount;
// #else
  // DWORD dwBufferCount;
// #endif
  // WSABUF Control;
// #if (_WIN32_WINNT >= 0x0600)
  // ULONG dwFlags;
// #else
  // DWORD dwFlags;
// #endif
// } WSAMSG, *PWSAMSG, *FAR LPWSAMSG;

// typedef struct _WSAPROTOCOLCHAIN {
  // int ChainLen;
  // DWORD ChainEntries[MAX_PROTOCOL_CHAIN];
// } WSAPROTOCOLCHAIN, *LPWSAPROTOCOLCHAIN;

// typedef struct _WSAPROTOCOL_INFOA {
  // DWORD dwServiceFlags1;
  // DWORD dwServiceFlags2;
  // DWORD dwServiceFlags3;
  // DWORD dwServiceFlags4;
  // DWORD dwProviderFlags;
  // GUID ProviderId;
  // DWORD dwCatalogEntryId;
  // WSAPROTOCOLCHAIN ProtocolChain;
  // int iVersion;
  // int iAddressFamily;
  // int iMaxSockAddr;
  // int iMinSockAddr;
  // int iSocketType;
  // int iProtocol;
  // int iProtocolMaxOffset;
  // int iNetworkByteOrder;
  // int iSecurityScheme;
  // DWORD dwMessageSize;
  // DWORD dwProviderReserved;
  // CHAR szProtocol[WSAPROTOCOL_LEN+1];
// } WSAPROTOCOL_INFOA, *LPWSAPROTOCOL_INFOA;

// typedef struct _WSAPROTOCOL_INFOW {
  // DWORD dwServiceFlags1;
  // DWORD dwServiceFlags2;
  // DWORD dwServiceFlags3;
  // DWORD dwServiceFlags4;
  // DWORD dwProviderFlags;
  // GUID ProviderId;
  // DWORD dwCatalogEntryId;
  // WSAPROTOCOLCHAIN ProtocolChain;
  // int iVersion;
  // int iAddressFamily;
  // int iMaxSockAddr;
  // int iMinSockAddr;
  // int iSocketType;
  // int iProtocol;
  // int iProtocolMaxOffset;
  // int iNetworkByteOrder;
  // int iSecurityScheme;
  // DWORD dwMessageSize;
  // DWORD dwProviderReserved;
  // WCHAR szProtocol[WSAPROTOCOL_LEN+1];
// } WSAPROTOCOL_INFOW, * LPWSAPROTOCOL_INFOW;

// #ifdef UNICODE
// typedef WSAPROTOCOL_INFOW WSAPROTOCOL_INFO;
// typedef LPWSAPROTOCOL_INFOW LPWSAPROTOCOL_INFO;
// #else
// typedef WSAPROTOCOL_INFOA WSAPROTOCOL_INFO;
// typedef LPWSAPROTOCOL_INFOA LPWSAPROTOCOL_INFO;
// #endif

// typedef struct in6_addr {
  // union {
    // UCHAR  Byte[16];
    // USHORT Word[8];
  // } u;
// } IN6_ADDR, *PIN6_ADDR, *LPIN6_ADDR;

struct sockaddr_in6 {
        short   sin6_family;
        u_short sin6_port;
        u_long  sin6_flowinfo;
        struct  in6_addr sin6_addr;
        u_long  sin6_scope_id;
};

typedef unsigned int GROUP;

typedef struct sockaddr_in6 SOCKADDR_IN6;

SOCKET
WINAPI 
WSASocketA(
	int af, 
	int type, 
	int protocol,
    LPWSAPROTOCOL_INFOA lpProtocolInfo,
    GROUP g, 
	DWORD dwFlags
);

WINAPI 
WSASocketW(
	int af, 
	int type, 
	int protocol,
    LPWSAPROTOCOL_INFOW lpProtocolInfo,
    GROUP g, 
	DWORD dwFlags
);

INT
WSAAPI
WSASendTo(IN SOCKET s,
          IN LPWSABUF lpBuffers,
          IN DWORD dwBufferCount,
          OUT LPDWORD lpNumberOfBytesSent,
          IN DWORD dwFlags,
          IN CONST struct sockaddr *lpTo,
          IN INT iToLen,
          IN LPWSAOVERLAPPED lpOverlapped,
          IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
		  
INT
WSAAPI
WSAIoctl(IN SOCKET s,
         IN DWORD dwIoControlCode,
         IN LPVOID lpvInBuffer,
         IN DWORD cbInBuffer,
         OUT LPVOID lpvOutBuffer,
         IN DWORD cbOutBuffer,
         OUT LPDWORD lpcbBytesReturned,
         IN LPWSAOVERLAPPED lpOverlapped,
         IN LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);		  
		 
int
WSAAPI
WSASendMsg(
  IN SOCKET Handle,
  IN LPWSAMSG lpMsg,
  IN DWORD dwFlags,
  OUT OPTIONAL LPDWORD lpNumberOfBytesSent,
  IN OUT OPTIONAL LPWSAOVERLAPPED lpOverlapped,
  IN OPTIONAL LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);		 
  
WSAEVENT
WSAAPI
WSACreateEvent(void);  

BOOL
WSAAPI
WSACloseEvent(
  _In_ WSAEVENT hEvent);