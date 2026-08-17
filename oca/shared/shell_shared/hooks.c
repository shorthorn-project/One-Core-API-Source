/*++

Copyright (c) 2024  Shorthorn Project

Module Name:

    hooks.c

Abstract:

    Hook native functions 

Author:

    Skulltrail 20-September-2024

Revision History:

--*/

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "objbase.h"
#include "rpcproxy.h"
#include "commdlg.h"
#include "cderr.h"
#include "wine/debug.h"
#include "wine/heap.h"

#define NDEBUG
#include <debug.h>
#include <main.h>
 
WINE_DEFAULT_DEBUG_CHANNEL(hooks);

static PFN_DllGetClassObject_Native pfnDllGetClassObjectNative = NULL;

/*************************************************************************
 * ILLoadFromStream (SHELL32.26)
 *
 * NOTES
 *   the first two bytes are the len, the pidl is following then
 */
HRESULT WINAPI ILLoadFromStream (IStream * pStream, LPITEMIDLIST * ppPidl)
{
    WORD        wLen = 0;
    DWORD       dwBytesRead;
    HRESULT     ret = E_FAIL;


    //TRACE("%p %p\n", pStream ,  ppPidl);

    SHFree(*ppPidl);
    *ppPidl = NULL;

    IStream_AddRef (pStream);

    if (SUCCEEDED(IStream_Read(pStream, &wLen, 2, &dwBytesRead)))
    {
        //TRACE("PIDL length is %d\n", wLen);
        if (wLen != 0)
        {
            *ppPidl = SHAlloc (wLen);
            if (SUCCEEDED(IStream_Read(pStream, *ppPidl , wLen, &dwBytesRead)))
            {
                //TRACE("Stream read OK\n");
                ret = S_OK;
            }
            else
            {
                //WARN("reading pidl failed\n");
                SHFree(*ppPidl);
                *ppPidl = NULL;
            }
        }
        else
        {
            *ppPidl = NULL;
            ret = S_OK;
        }
    }

    /* we are not yet fully compatible */
    if (*ppPidl && !pcheck(*ppPidl))
    {
        WARN("Check failed\n");
#ifndef __REACTOS__ /* We don't know all pidl formats, must allow loading unknown */
        SHFree(*ppPidl);
        *ppPidl = NULL;
#endif
    }

    IStream_Release (pStream);
    TRACE("done\n");
    return ret;
}

/**************************************************************************
 * Default ClassFactory types
 */
/* this table contains all CLSIDs of shell32 objects */
static const struct {
	REFIID			clsid;
	LPFNCREATEINSTANCE	lpfnCI;
} InterfaceTable[] = {

	{&CLSID_ApplicationAssociationRegistration, ApplicationAssociationRegistration_Constructor},
	{&CLSID_ApplicationDestinations, ApplicationDestinations_Constructor},
	{&CLSID_ApplicationDocumentLists, ApplicationDocumentLists_Constructor},
	// {&CLSID_QueryAssociations, QueryAssociations_Constructor},
	//{&CLSID_ShellItem,	IShellItem_Constructor},
	{&CLSID_ShellLink,	IShellLink_Constructor},
	{&CLSID_ExplorerBrowser,ExplorerBrowser_Constructor},
	{&CLSID_KnownFolderManager, KnownFolderManager_Constructor},
	{&CLSID_DestinationList, CustomDestinationList_Constructor},
	{&CLSID_FileOperation, IFileOperation_Constructor},
	{NULL, NULL}
};		   

BOOLEAN 
CheckIfIsOSExec(){
    // Get the current Process ID
    DWORD currentProcessId = GetCurrentProcessId();

    // Abrir o processo
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, currentProcessId);

    // Buffer to save the executable patch
    WCHAR exePath[MAX_PATH];
    
    // Get the path of process 
    DWORD size = GetModuleFileNameExW(hProcess, NULL, exePath, MAX_PATH);
    if (size == 0) {
        return FALSE;
    }

    // Compare executable name with "explorer.exe"
    if ((wcsicmp(PathFindFileNameW(exePath), L"EXPLORER.EXE") == 0) 
		// || wcsicmp(PathFindFileNameW(exePath), L"MSIEXEC.EXE") == 0  
		// || wcsicmp(PathFindFileNameW(exePath), L"Rundll32.EXE") == 0 
		// || wcsicmp(PathFindFileNameW(exePath), L"SYSOCMGR.EXE") == 0
		) {
        return TRUE;
    } else {
		return FALSE;
    }	
}

/*************************************************************************
 * DllGetClassObject     [SHELL32.@]
 * SHDllGetClassObject   [SHELL32.128]
 */
HRESULT WINAPI DllGetClassObjectHook(REFCLSID rclsid, REFIID iid, LPVOID *ppv)
{
	IClassFactory * pcf = NULL;
	HRESULT	hres;
	int i;
    // HMODULE hShell32 = NULL;	
    // PFNDllGetClassObject pfnDllGetClassObject;	
	
	// TRACE("CLSID:%s,IID:%s\n",shdebugstr_guid(rclsid),shdebugstr_guid(iid));

	if (!ppv) return E_INVALIDARG;
	*ppv = NULL;

	if(!CheckIfIsOSExec()){ //Avoid Explorer
		/* search our internal interface table */
		for(i=0;InterfaceTable[i].clsid;i++) {
			if(IsEqualIID(InterfaceTable[i].clsid, rclsid)) {
				//TRACE("index[%u]\n", i);
				pcf = IDefClF_fnConstructor(InterfaceTable[i].lpfnCI, NULL, NULL);
			}			
		}
	}	

    if (!pcf) {
		// hShell32 = GetModuleHandleW(shellName);
		// if (!hShell32){
			// hShell32 = LoadLibraryW(shellName);
			// if (!hShell32)
			// {
				// return HRESULT_FROM_WIN32(GetLastError());
			// }					
		// }	

		// pfnDllGetClassObject = (PFNDllGetClassObject)
			// GetProcAddress(hShell32, "DllGetClassObjectNative");
		// if (!pfnDllGetClassObject)
		// {
			// OutputDebugStringW(L"DllGetClassObject: não encontrou função nativa em shell32.dll\n");
			// return E_FAIL;
		// }
				
		return DllGetClassObject(rclsid, iid, ppv); 
				
	}

	hres = IClassFactory_QueryInterface(pcf, iid, ppv);
	IClassFactory_Release(pcf);

	//TRACE("-- pointer to class factory: %p\n",*ppv);
	return hres;
}

/*************************************************************************
 * CommandLineToArgvW            [SHCORE.@]
 *
 * We must interpret the quotes in the command line to rebuild the argv
 * array correctly:
 * - arguments are separated by spaces or tabs
 * - quotes serve as optional argument delimiters
 *   '"a b"'   -> 'a b'
 * - escaped quotes must be converted back to '"'
 *   '\"'      -> '"'
 * - consecutive backslashes preceding a quote see their number halved with
 *   the remainder escaping the quote:
 *   2n   backslashes + quote -> n backslashes + quote as an argument delimiter
 *   2n+1 backslashes + quote -> n backslashes + literal quote
 * - backslashes that are not followed by a quote are copied literally:
 *   'a\b'     -> 'a\b'
 *   'a\\b'    -> 'a\\b'
 * - in quoted strings, consecutive quotes see their number divided by three
 *   with the remainder modulo 3 deciding whether to close the string or not.
 *   Note that the opening quote must be counted in the consecutive quotes,
 *   that's the (1+) below:
 *   (1+) 3n   quotes -> n quotes
 *   (1+) 3n+1 quotes -> n quotes plus closes the quoted string
 *   (1+) 3n+2 quotes -> n+1 quotes plus closes the quoted string
 * - in unquoted strings, the first quote opens the quoted string and the
 *   remaining consecutive quotes follow the above rule.
 */
WCHAR** WINAPI CommandLineToArgvW(const WCHAR *cmdline, int *numargs)
{
    int qcount, bcount;
    const WCHAR *s;
    WCHAR **argv;
    DWORD argc;
    WCHAR *d;

    if (!numargs)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (*cmdline == 0)
    {
        /* Return the path to the executable */
        DWORD len, deslen = MAX_PATH, size;

        size = sizeof(WCHAR *) * 2 + deslen * sizeof(WCHAR);
        for (;;)
        {
            if (!(argv = LocalAlloc(LMEM_FIXED, size))) return NULL;
            len = GetModuleFileNameW(0, (WCHAR *)(argv + 2), deslen);
            if (!len)
            {
                LocalFree(argv);
                return NULL;
            }
            if (len < deslen) break;
            deslen *= 2;
            size = sizeof(WCHAR *) * 2 + deslen * sizeof(WCHAR);
            LocalFree(argv);
        }
        argv[0] = (WCHAR *)(argv + 2);
        argv[1] = NULL;
        *numargs = 1;

        return argv;
    }

    /* --- First count the arguments */
    argc = 1;
    s = cmdline;
    /* The first argument, the executable path, follows special rules */
    if (*s == '"')
    {
        /* The executable path ends at the next quote, no matter what */
        s++;
        while (*s)
            if (*s++ == '"')
                break;
    }
    else
    {
        /* The executable path ends at the next space, no matter what */
        while (*s && *s != ' ' && *s != '\t')
            s++;
    }
    /* skip to the first argument, if any */
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s)
        argc++;

    /* Analyze the remaining arguments */
    qcount = bcount = 0;
    while (*s)
    {
        if ((*s == ' ' || *s == '\t') && qcount == 0)
        {
            /* skip to the next argument and count it if any */
            while (*s == ' ' || *s == '\t')
                s++;
            if (*s)
                argc++;
            bcount = 0;
        }
        else if (*s == '\\')
        {
            /* '\', count them */
            bcount++;
            s++;
        }
        else if (*s == '"')
        {
            /* '"' */
            if ((bcount & 1) == 0)
                qcount++; /* unescaped '"' */
            s++;
            bcount = 0;
            /* consecutive quotes, see comment in copying code below */
            while (*s == '"')
            {
                qcount++;
                s++;
            }
            qcount = qcount % 3;
            if (qcount == 2)
                qcount = 0;
        }
        else
        {
            /* a regular character */
            bcount = 0;
            s++;
        }
    }

    /* Allocate in a single lump, the string array, and the strings that go
     * with it. This way the caller can make a single LocalFree() call to free
     * both, as per MSDN.
     */
    argv = LocalAlloc(LMEM_FIXED, (argc + 1) * sizeof(WCHAR *) + (lstrlenW(cmdline) + 1) * sizeof(WCHAR));
    if (!argv)
        return NULL;

    /* --- Then split and copy the arguments */
    argv[0] = d = lstrcpyW((WCHAR *)(argv + argc + 1), cmdline);
    argc = 1;
    /* The first argument, the executable path, follows special rules */
    if (*d == '"')
    {
        /* The executable path ends at the next quote, no matter what */
        s = d + 1;
        while (*s)
        {
            if (*s == '"')
            {
                s++;
                break;
            }
            *d++ = *s++;
        }
    }
    else
    {
        /* The executable path ends at the next space, no matter what */
        while (*d && *d != ' ' && *d != '\t')
            d++;
        s = d;
        if (*s)
            s++;
    }
    /* close the executable path */
    *d++ = 0;
    /* skip to the first argument and initialize it if any */
    while (*s == ' ' || *s == '\t')
        s++;
    if (!*s)
    {
        /* There are no parameters so we are all done */
        argv[argc] = NULL;
        *numargs = argc;
        return argv;
    }

    /* Split and copy the remaining arguments */
    argv[argc++] = d;
    qcount = bcount = 0;
    while (*s)
    {
        if ((*s == ' ' || *s == '\t') && qcount == 0)
        {
            /* close the argument */
            *d++ = 0;
            bcount = 0;

            /* skip to the next one and initialize it if any */
            do {
                s++;
            } while (*s == ' ' || *s == '\t');
            if (*s)
                argv[argc++] = d;
        }
        else if (*s=='\\')
        {
            *d++ = *s++;
            bcount++;
        }
        else if (*s == '"')
        {
            if ((bcount & 1) == 0)
            {
                /* Preceded by an even number of '\', this is half that
                 * number of '\', plus a quote which we erase.
                 */
                d -= bcount / 2;
                qcount++;
            }
            else
            {
                /* Preceded by an odd number of '\', this is half that
                 * number of '\' followed by a '"'
                 */
                d = d - bcount / 2 - 1;
                *d++ = '"';
            }
            s++;
            bcount = 0;
            /* Now count the number of consecutive quotes. Note that qcount
             * already takes into account the opening quote if any, as well as
             * the quote that lead us here.
             */
            while (*s == '"')
            {
                if (++qcount == 3)
                {
                    *d++ = '"';
                    qcount = 0;
                }
                s++;
            }
            if (qcount == 2)
                qcount = 0;
        }
        else
        {
            /* a regular character */
            *d++ = *s++;
            bcount = 0;
        }
    }
    *d = '\0';
    argv[argc] = NULL;
    *numargs = argc;

    return argv;
}

BOOL WINAPI ShellExecuteExAHook(
    SHELLEXECUTEINFOA *pExecInfo
)
{
    // static PFN_ShellExecuteExA_Native pfnNative = NULL;
    // HMODULE hShell32 = NULL;
    // FARPROC proc = NULL;
    BOOL result = FALSE;
    const char prefix[] = "\\\\?\\";
    const char *originalFile = NULL;
    char *heapCleanPath = NULL;

    if (!pExecInfo) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // /* Se a função nativa ainda não foi resolvida, tenta resolver agora */
    // if (pfnNative == NULL) {
        // /* Primeiro tenta obter o módulo já carregado */
        // hShell32 = GetModuleHandleW(shellName);
        // if (hShell32) {
            // proc = GetProcAddress(hShell32, "ShellExecuteExANative");
            // if (proc) pfnNative = (PFN_ShellExecuteExA_Native)proc;
        // }

        // /* Se não encontrou, tenta carregar shell32 e resolver */
        // if (pfnNative == NULL) {
            // hShell32 = LoadLibraryW(shellName);
            // if (hShell32) {
                // proc = GetProcAddress(hShell32, "ShellExecuteExANative");
                // if (proc) pfnNative = (PFN_ShellExecuteExA_Native)proc;
                // /* Note: não liberamos o module handle carregado com LoadLibrary
                   // porque a shell32 é uma dll de sistema — deixar o refcount
                   // não causa problema em geral. Se quiser, pode FreeLibrary aqui,
                   // mas então o ponteiro seria inválido. */
            // }
        // }

        // /* Se não conseguiu resolver, falha com erro apropriado */
        // if (pfnNative == NULL) {
            // /* Não foi possível achar a implementação nativa */
            // /* Para depuração: */
            // OutputDebugStringA("ShellExecuteExA: não encontrou ShellExecuteExANative em shell32.dll\n");
            // SetLastError(ERROR_PROC_NOT_FOUND);
            // return FALSE;
        // }
    // }

    /* Se houver path com prefixo \\?\ - aloca um buffer em heap e usa-o */
    originalFile = pExecInfo->lpFile;
    if (originalFile && lstrlenA(originalFile) >= 4 && strncmp(originalFile, prefix, 4) == 0) {
        SIZE_T needed = (lstrlenA(originalFile + 4) + 1);
        heapCleanPath = (char *)HeapAlloc(GetProcessHeap(), 0, needed);
        if (heapCleanPath == NULL) {
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
        /* copia a versão "limpa" para o heap */
        lstrcpyA(heapCleanPath, originalFile + 4);
        /* aponta para o buffer alocado */
        pExecInfo->lpFile = heapCleanPath;
        /* opcional para debug */
        // OutputDebugStringA("ShellExecuteExA: usando caminho limpo alocado no heap\n");
    }

    /* Chama a função nativa */
    result = ShellExecuteExA(pExecInfo);

    /* Restaura e libera o buffer heap se tivermos alocado */
    if (heapCleanPath) {
        /* restaura o ponteiro original para não mudar o comportamento do chamador */
        pExecInfo->lpFile = originalFile;
        HeapFree(GetProcessHeap(), 0, heapCleanPath);
    }

    return result;
}

//Intl.cpl require this hook with original name, don't accept alternate name
BOOL WINAPI ShellExecuteExWHook(
    SHELLEXECUTEINFOW *pExecInfo
)
{
    // static PFN_ShellExecuteExW_Native pfnNative = NULL;
    // HMODULE hShell32 = NULL;
    // FARPROC proc = NULL;
    BOOL result = FALSE;
    const wchar_t prefix[] = L"\\\\?\\";
    const wchar_t *originalFile = NULL;
    wchar_t *heapCleanPath = NULL;

    if (!pExecInfo) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // /* Resolver função nativa uma única vez */
    // if (pfnNative == NULL) {
        // hShell32 = GetModuleHandleW(shellName);
        // if (hShell32) {
            // proc = GetProcAddress(hShell32, "ShellExecuteExWNative");
            // if (proc) pfnNative = (PFN_ShellExecuteExW_Native)proc;
        // }

        // if (pfnNative == NULL) {
            // if (!hShell32)
                // hShell32 = LoadLibraryW(shellName);
            // if (hShell32) {
                // proc = GetProcAddress(hShell32, "ShellExecuteExWNative");
                // if (proc)
                    // pfnNative = (PFN_ShellExecuteExW_Native)proc;
                // else {
                    // /* Fallback para ShellExecuteExW normal */
                    // proc = GetProcAddress(hShell32, "ShellExecuteExW");
                    // if (proc)
                        // pfnNative = (PFN_ShellExecuteExW_Native)proc;
                // }
            // }
        // }

        // if (pfnNative == NULL) {
            // OutputDebugStringW(L"ShellExecuteExW: não encontrou ShellExecuteExWNative em shell32.dll\n");
            // SetLastError(ERROR_PROC_NOT_FOUND);
            // return FALSE;
        // }
    // }

    /* Limpa o prefixo \\?\ se presente */
    originalFile = pExecInfo->lpFile;
    if (originalFile && wcsncmp(originalFile, prefix, 4) == 0) {
        SIZE_T needed = (lstrlenW(originalFile + 4) + 1) * sizeof(wchar_t);
        heapCleanPath = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, needed);
        if (!heapCleanPath) {
            SetLastError(ERROR_OUTOFMEMORY);
            return FALSE;
        }
        lstrcpyW(heapCleanPath, originalFile + 4);
        pExecInfo->lpFile = heapCleanPath;
        // OutputDebugStringW(L"ShellExecuteExW: usando caminho limpo no heap\n");
    }

    /* Chama a função real */
    result = ShellExecuteExW(pExecInfo);

    /* Restaura e libera */
    if (heapCleanPath) {
        pExecInfo->lpFile = originalFile;
        HeapFree(GetProcessHeap(), 0, heapCleanPath);
    }

    return result;
}

// Prior to Windows Vista, SHGetFolderPath's Wow64 CSIDL's do not work on 32-bit systems.
// This affects Java installers after around JDK 8u152, plus tons and tons of other installers.
// Some programs handle it by only passing the X86 csidl's on 64-bit systems, but not all of them.
NTSTATUS WINAPI SHGetFolderPathWHook(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPWSTR pszPath) {
	// DbgPrint("SHGetFolderPathWHook called\n");
	// DbgPrint("SHGetFolderPathWHook::csidl %i\n", csidl);
	// DbgPrint("SHGetFolderPathWHook::flags %d\n", dwFlags);
//#ifdef _M_IX86
    if (csidl & 0xFF == CSIDL_PROGRAM_FILES_COMMONX86) {
        csidl &= ~0xFF;
        DbgPrint("csidl CSIDL_PROGRAM_FILES_COMMONX86, flags %i\n", csidl);
        csidl |= CSIDL_PROGRAM_FILES_COMMON;
    } else if (csidl & 0xFF == CSIDL_PROGRAM_FILESX86) {
        csidl &= ~0xFF;
        DbgPrint("csidl CSIDL_PROGRAM_FILESX86, flags %i\n", csidl);
        csidl |= CSIDL_PROGRAM_FILES;
    } else if (csidl & 0xFF == CSIDL_SYSTEMX86) {
        csidl &= ~0xFF;
        DbgPrint("csidl CSIDL_SYSTEMX86, flags %i\n", csidl);
        csidl |= CSIDL_SYSTEM;
    }
//#endif
    return SHGetFolderPathW(hwnd, csidl, hToken, dwFlags, pszPath);
}

NTSTATUS WINAPI SHGetFolderPathAHook(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath) {
	DbgPrint("SHGetFolderPathAHook called\n");
//#ifdef _M_IX86
	if (csidl & (~CSIDL_FLAG_MASK) == CSIDL_PROGRAM_FILES_COMMONX86)
		csidl = (csidl & CSIDL_FLAG_MASK) | CSIDL_PROGRAM_FILES_COMMON;
	else if (csidl & (~CSIDL_FLAG_MASK) == CSIDL_PROGRAM_FILESX86)
		csidl = (csidl & CSIDL_FLAG_MASK) | CSIDL_PROGRAM_FILES;
	else if (csidl & (~CSIDL_FLAG_MASK) == CSIDL_SYSTEMX86)
		csidl = (csidl & CSIDL_FLAG_MASK) | CSIDL_SYSTEM;
//#endif
	return SHGetFolderPathA(hwnd, csidl, hToken, dwFlags, pszPath);
}

// Because SHGetSpecialFolderPath just calls SHGetFolderPath, we must hook those as well, because tons of things
// still call SHGetSpecialFolderPath instead of SHGetFolderPath despite the docs calling it unsupported.
BOOL WINAPI SHGetSpecialFolderPathWHook(HWND hwnd, LPWSTR pszPath, int csidl, BOOL create) {
	// create flag is equalivent to CSIDL_FLAG_CREATE flag.
	return SHGetFolderPathWHook(hwnd, create ? csidl | CSIDL_FLAG_CREATE : csidl, NULL, 0, pszPath) == 0;
}

BOOL WINAPI SHGetSpecialFolderPathAHook(HWND hwnd, LPSTR pszPath, int csidl, BOOL create) {
	return SHGetFolderPathAHook(hwnd, create ? csidl | CSIDL_FLAG_CREATE : csidl, NULL, 0, pszPath) == 0;
}

// /*************************************************************************
 // * ShellExecuteA            [SHELL32.290]
 // */
// HINSTANCE WINAPI ShellExecuteAInternal(HWND hWnd, LPCSTR lpVerb, LPCSTR lpFile,
                               // LPCSTR lpParameters, LPCSTR lpDirectory, INT iShowCmd)
// {
	// // char converted[MAX_PATH];
	// // //PathCchCanonicalize(lpFile, MAX_PATH, lpFile);
	// // remove_extended_prefix(lpFile, converted ,MAX_PATH);
	// // DbgPrint("ShellExecuteWInternal:: original file: %ws\n", lpFile);
	// // DbgPrint("ShellExecuteWInternal:: converted file: %ws\n", converted);
	// return ShellExecuteANative(hWnd, lpVerb, lpFile, lpParameters, lpDirectory, iShowCmd);
// }

// /*************************************************************************
 // * ShellExecuteW			[SHELL32.294]
 // * from shellapi.h
 // * WINSHELLAPI HINSTANCE APIENTRY ShellExecuteW(HWND hwnd, LPCWSTR lpVerb,
 // * LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd);
 // */
// HINSTANCE WINAPI ShellExecuteWInternal(HWND hwnd, LPCWSTR lpVerb, LPCWSTR lpFile,
                               // LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd)
// {
	// // wchar_t converted[MAX_PATH];
	// // //PathCchCanonicalize(lpFile, MAX_PATH, lpFile);
	// // remove_extended_prefix_w(lpFile, converted ,MAX_PATH);
	// // DbgPrint("ShellExecuteWInternal:: original file: %ws\n", lpFile);
	// // DbgPrint("ShellExecuteWInternal:: converted file: %ws\n", converted);
	// return ShellExecuteWNative(hwnd, lpVerb, lpFile, lpParameters, lpDirectory, nShowCmd);
// }

// BOOL WINAPI Shell_NotifyIconWInternal(DWORD dwMessage, PNOTIFYICONDATAW lpData) {
    // if (lpData != NULL && lpData->cbSize > NOTIFYICONDATAW_V3_SIZE) {
        // NOTIFYICONDATAW lpXPData;
        // memcpy(&lpXPData, lpData, NOTIFYICONDATAW_V3_SIZE);
        // lpXPData.cbSize = NOTIFYICONDATAW_V3_SIZE;
        // // Remove Vista flags.
        // if (lpXPData.uFlags & 0x80) // NIF_SHOWTIP
            // lpXPData.uFlags ^= 0x80;
        // if (lpXPData.uFlags & 0x40) // NIF_REALTIME
            // lpXPData.uFlags ^= 0x40;
        // if (lpXPData.uFlags & 0x20) // NIF_GUID
            // lpXPData.uFlags ^= 0x20;
        
        // // & 0x20 is "reserved", we do not want to mess with it normally, but since this is conditionally defined, it's fair game.
        // if (lpXPData.dwInfoFlags & 0x20) {
            // // I hope it picks the right icon.
            // lpXPData.dwInfoFlags ^= 0x20;
        // }
        // if (lpXPData.dwInfoFlags & 0x80) {
            // lpXPData.dwInfoFlags ^= 0x80;
        // }
        // if (lpXPData.uVersion > 3)
            // lpXPData.uVersion = 3;
        // memset(&(lpXPData.guidItem), 0, sizeof(GUID));
        // return Shell_NotifyIconWNative(dwMessage, &lpXPData);
    // }
    // return Shell_NotifyIconWNative(dwMessage, lpData);
// }

// BOOL WINAPI Shell_NotifyIconWInternal(DWORD dwMessage, PNOTIFYICONDATAW lpData) {
    // // Verifica se a estrutura é válida e se é de uma versão superior à V3
    // if (lpData != NULL && lpData->cbSize > NOTIFYICONDATAW_V3_SIZE) {
        // // Faz uma cópia para manipulação, sem afetar a original
        // NOTIFYICONDATAW lpXPData = {0};
        // memcpy(&lpXPData, lpData, sizeof(NOTIFYICONDATAW));
        
        // // Corrige a cbSize para V3
        // lpXPData.cbSize = NOTIFYICONDATAW_V3_SIZE;

        // // Remove flags específicas do Vista que não existem na V3
        // lpXPData.uFlags &= ~(0x80 | 0x40 | 0x20); // NIF_SHOWTIP, NIF_REALTIME, NIF_GUID
        // lpXPData.dwInfoFlags &= ~(0x20 | 0x80);   // Valores específicos que causam problemas
        // lpXPData.uVersion = min(lpXPData.uVersion, 3);

        // // Zera GUID para evitar uso incorreto (já que o campo foi removido)
        // ZeroMemory(&(lpXPData.guidItem), sizeof(GUID));

        // // Chama a versão nativa com a estrutura compatível
        // return Shell_NotifyIconWNative(dwMessage, &lpXPData);
    // }

    // // Estrutura compatível, pode chamar diretamente
    // return Shell_NotifyIconWNative(dwMessage, lpData);
// }


// BOOL WINAPI Shell_NotifyIconAInternal(DWORD dwMessage, PNOTIFYICONDATAA lpData) {
    // // // if (lpData->cbSize > NOTIFYICONDATAA_V2_SIZE) {
        // // // NOTIFYICONDATAA lpXPData;
        // // // memcpy(&lpXPData, lpData, NOTIFYICONDATAA_V2_SIZE);
        // // // lpXPData.cbSize = NOTIFYICONDATAW_V2_SIZE;
        // // // // Remove Vista flags.
        // // // if (lpXPData.uFlags & 0x80) { // NIF_SHOWTIP
            // // // lpXPData.uFlags ^= 0x80;
        // // // }
        // // // if (lpXPData.uFlags & 0x40) { // NIF_REALTIME
            // // // lpXPData.uFlags ^= 0x40;
        // // // }
        // // // if (lpXPData.dwInfoFlags & 0x20) {
            // // // // I hope it picks the right icon.
            // // // lpXPData.dwInfoFlags ^= 0x20;
        // // // }
        // // // if (lpXPData.dwInfoFlags & 0x80) {
            // // // lpXPData.dwInfoFlags ^= 0x80;
        // // // }
        // // // if (lpXPData.uVersion > 3)
            // // // lpXPData.uVersion = 3;
        // // // return Shell_NotifyIconANative(dwMessage, &lpXPData);
    // // // }
    // return Shell_NotifyIconA(dwMessage, lpData);
// }