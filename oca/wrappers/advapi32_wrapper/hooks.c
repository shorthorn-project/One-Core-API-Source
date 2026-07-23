/*++

Copyright (c) 2021  Shorthorn Project

Module Name:

    hooks.c

Abstract:

    Hooks native functions to implement new features
	
Author:

    Skulltrail 18-July-2021

Revision History:

--*/

#include "main.h"

WINE_DEFAULT_DEBUG_CHANNEL(advapi32_hooks); 

#define REG_NOTIFY_THREAD_AGNOSTIC   0x10000000

// DWORD
// WINAPI
// SetNamedSecurityInfoWNative(
	// LPWSTR pObjectName,
    // SE_OBJECT_TYPE ObjectType,
    // SECURITY_INFORMATION SecurityInfo,
    // PSID psidOwner,
    // PSID psidGroup,
    // PACL pDacl,
    // PACL pSacl
// );

// DWORD
// WINAPI
// SetSecurityInfoNative(
	// HANDLE handle,
    // SE_OBJECT_TYPE ObjectType,
    // SECURITY_INFORMATION SecurityInfo,
    // PSID psidOwner,
    // PSID psidGroup,
    // PACL pDacl,
    // PACL pSacl
// );

// DWORD
// WINAPI
// GetSecurityInfoNative(
	// HANDLE handle,
    // SE_OBJECT_TYPE ObjectType,
    // SECURITY_INFORMATION SecurityInfo,
    // PSID *ppsidOwner,
    // PSID *ppsidGroup,
    // PACL *ppDacl,
    // PACL *ppSacl,
    // PSECURITY_DESCRIPTOR *ppSecurityDescriptor
// );

// DWORD
// WINAPI
// GetNamedSecurityInfoWNative(
	// LPWSTR pObjectName,
    // SE_OBJECT_TYPE ObjectType,
    // SECURITY_INFORMATION SecurityInfo,
    // PSID *ppsidOwner,
    // PSID *ppsidGroup,
    // PACL *ppDacl,
    // PACL *ppSacl,
    // PSECURITY_DESCRIPTOR *ppSecurityDescriptor
// );

/* ---------------------------
   Dynamic resolver (GetModuleHandleW + GetProcAddress)
   Insert this block after the Native prototypes in hooks.c
   Compatible C89 / WinXP
   --------------------------- */

static HMODULE ghAdvapi32 = NULL;

/* Function pointer declarations matching the real APIs */
static LSTATUS (WINAPI *pRegGetValueW)(
    HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD
) = NULL;

static LSTATUS (WINAPI *pRegNotifyChangeKeyValue)(
    HKEY, BOOL, DWORD, HANDLE, BOOL
) = NULL;

static BOOL (WINAPI *pConvertStringSecurityDescriptorToSecurityDescriptorW)(
    LPCWSTR, DWORD, PSECURITY_DESCRIPTOR*, PULONG
) = NULL;

static DWORD (WINAPI *pSetNamedSecurityInfoW)(
    LPWSTR, SE_OBJECT_TYPE, SECURITY_INFORMATION,
    PSID, PSID, PACL, PACL
) = NULL;

static DWORD (WINAPI *pSetSecurityInfo)(
    HANDLE, SE_OBJECT_TYPE, SECURITY_INFORMATION,
    PSID, PSID, PACL, PACL
) = NULL;

static DWORD (WINAPI *pGetSecurityInfo)(
    HANDLE, SE_OBJECT_TYPE, SECURITY_INFORMATION,
    PSID*, PSID*, PACL*, PACL*, PSECURITY_DESCRIPTOR*
) = NULL;

static DWORD (WINAPI *pGetNamedSecurityInfoW)(
    LPWSTR, SE_OBJECT_TYPE, SECURITY_INFORMATION,
    PSID*, PSID*, PACL*, PACL*, PSECURITY_DESCRIPTOR*
) = NULL;

// /* Initialize pointers using GetModuleHandleW (no LoadLibrary) */
// BOOL InitNativeProcs(void)
// {
    // HMODULE h;
    // if (ghAdvapi32) return TRUE; /* já inicializado */

    // h = GetModuleHandleW(L"advapibase.dll");
    // if (!h) return FALSE;
    // ghAdvapi32 = h;

    // /* getprocaddress - use exact exported names */
    // pRegGetValueW = (void*) GetProcAddress(ghAdvapi32, "RegGetValueW");
    // pRegNotifyChangeKeyValue = (void*) GetProcAddress(ghAdvapi32, "RegNotifyChangeKeyValue");
    // pConvertStringSecurityDescriptorToSecurityDescriptorW = (void*) GetProcAddress(ghAdvapi32, "ConvertStringSecurityDescriptorToSecurityDescriptorW");
    // pSetNamedSecurityInfoW = (void*) GetProcAddress(ghAdvapi32, "SetNamedSecurityInfoW");
    // pSetSecurityInfo = (void*) GetProcAddress(ghAdvapi32, "SetSecurityInfo");
    // pGetSecurityInfo = (void*) GetProcAddress(ghAdvapi32, "GetSecurityInfo");
    // pGetNamedSecurityInfoW = (void*) GetProcAddress(ghAdvapi32, "GetNamedSecurityInfoW");

    // /* It's OK if some pointers are NULL (fallbacks in code may handle) */
    // return TRUE;
// }

/* ---------------------------
   Wrappers for Native functions
   These keep existing call-sites untouched.
   --------------------------- */

// LSTATUS 
// WINAPI 
// RegGetValueWNative(
    // HKEY hkey, 
    // LPCWSTR lpSubKey, 
    // LPCWSTR lpValue, 
    // DWORD dwFlags, 
    // LPDWORD pdwType, 
    // PVOID pvData, 
    // LPDWORD pcbData
// )
// {
    // if (!InitNativeProcs() || !pRegGetValueW) {
        // SetLastError(ERROR_PROC_NOT_FOUND);
        // return ERROR_PROC_NOT_FOUND; /* LSTATUS is LONG - returning an error code */
    // }
    // return pRegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
// }

// LSTATUS 
// WINAPI 
// RegNotifyChangeKeyValueNative(
    // HKEY   hKey,
    // BOOL   bWatchSubtree,
    // DWORD  dwNotifyFilter,
    // HANDLE hEvent, 
    // BOOL   fAsynchronous
// )
// {
    // if (!InitNativeProcs() || !pRegNotifyChangeKeyValue) {
        // SetLastError(ERROR_PROC_NOT_FOUND);
        // return ERROR_PROC_NOT_FOUND;
    // }
    // return pRegNotifyChangeKeyValue(hKey, bWatchSubtree, dwNotifyFilter, hEvent, fAsynchronous);
// }

// BOOL 
// WINAPI
// DECLSPEC_HOTPATCH 
// ConvertStringSecurityDescriptorToSecurityDescriptorWNative(
        // const WCHAR *string, DWORD revision, PSECURITY_DESCRIPTOR *sd, ULONG *ret_size )
// {
    // if (!InitNativeProcs() || !pConvertStringSecurityDescriptorToSecurityDescriptorW) {
        // SetLastError(ERROR_PROC_NOT_FOUND);
        // return FALSE;
    // }
    // return pConvertStringSecurityDescriptorToSecurityDescriptorW(string, revision, sd, ret_size);
// }

// DWORD
// WINAPI
// SetNamedSecurityInfoWNative(
    // LPWSTR pObjectName,
    // SE_OBJECT_TYPE ObjectType,
    // SECURITY_INFORMATION SecurityInfo,
    // PSID psidOwner,
    // PSID psidGroup,
    // PACL pDacl,
    // PACL pSacl
// )
// {
    // if (!InitNativeProcs() || !pSetNamedSecurityInfoW) {
        // return ERROR_PROC_NOT_FOUND;
    // }
    // return pSetNamedSecurityInfoW(pObjectName, ObjectType, SecurityInfo, psidOwner, psidGroup, pDacl, pSacl);
// }

// DWORD
// WINAPI
// SetSecurityInfoNative(
    // HANDLE handle,
    // SE_OBJECT_TYPE ObjectType,
    // SECURITY_INFORMATION SecurityInfo,
    // PSID psidOwner,
    // PSID psidGroup,
    // PACL pDacl,
    // PACL pSacl
// )
// {
    // if (!InitNativeProcs() || !pSetSecurityInfo) {
        // return ERROR_PROC_NOT_FOUND;
    // }
    // return pSetSecurityInfo(handle, ObjectType, SecurityInfo, psidOwner, psidGroup, pDacl, pSacl);
// }

// DWORD
// WINAPI
// GetSecurityInfoNative(
    // HANDLE handle,
    // SE_OBJECT_TYPE ObjectType,
    // SECURITY_INFORMATION SecurityInfo,
    // PSID *ppsidOwner,
    // PSID *ppsidGroup,
    // PACL *ppDacl,
    // PACL *ppSacl,
    // PSECURITY_DESCRIPTOR *ppSecurityDescriptor
// )
// {
    // if (!InitNativeProcs() || !pGetSecurityInfo) {
        // return ERROR_PROC_NOT_FOUND;
    // }
    // return pGetSecurityInfo(handle, ObjectType, SecurityInfo, ppsidOwner, ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor);
// }

// DWORD
// WINAPI
// GetNamedSecurityInfoWNative(
    // LPWSTR pObjectName,
    // SE_OBJECT_TYPE ObjectType,
    // SECURITY_INFORMATION SecurityInfo,
    // PSID *ppsidOwner,
    // PSID *ppsidGroup,
    // PACL *ppDacl,
    // PACL *ppSacl,
    // PSECURITY_DESCRIPTOR *ppSecurityDescriptor
// )
// {
    // if (!InitNativeProcs() || !pGetNamedSecurityInfoW) {
        // return ERROR_PROC_NOT_FOUND;
    // }
    // return pGetNamedSecurityInfoW(pObjectName, ObjectType, SecurityInfo, ppsidOwner, ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor);
// }

// /* End of dynamic resolver block */

// LSTATUS 
// WINAPI 
// RegNotifyChangeKeyValueNative(
    // HKEY   hKey,
	// BOOL   bWatchSubtree,
	// DWORD  dwNotifyFilter,
	// HANDLE hEvent, 
	// BOOL   fAsynchronous
// );

// BOOL 
// WINAPI
// DECLSPEC_HOTPATCH 
// ConvertStringSecurityDescriptorToSecurityDescriptorWNative(
        // const WCHAR *string, DWORD revision, PSECURITY_DESCRIPTOR *sd, ULONG *ret_size );

static LPWSTR SERV_dup( LPCSTR str )
{
    UINT len;
    LPWSTR wstr;

    if( !str )
        return NULL;
    len = MultiByteToWideChar( CP_ACP, 0, str, -1, NULL, 0 );
    wstr = heap_alloc( len*sizeof (WCHAR) );
    MultiByteToWideChar( CP_ACP, 0, str, -1, wstr, len );
    return wstr;
}

BOOL
APIENTRY
GetTokenInformationInternal (
    HANDLE TokenHandle,
    TOKEN_INFORMATION_CLASS TokenInformationClass,
    PVOID TokenInformation,
    DWORD TokenInformationLength,
    PDWORD ReturnLength
    )
{
    NTSTATUS Status;
    PTOKEN_GROUPS InformationBuffer = (PTOKEN_GROUPS)TokenInformation;
    PTOKEN_GROUPS GroupBuffer;
    DWORD dwReturnLength = *ReturnLength;
    int i, index=0;
	char* ptr;
    // 
    if(TokenInformationClass == TokenLogonSid){
		if (TokenInformationLength == 0) { // Chrome 98+ sandbox needs this.
			*ReturnLength = sizeof(TOKEN_GROUPS) + sizeof(PVOID) + sizeof(DWORD) + SECURITY_MAX_SID_SIZE;
			return FALSE;
		}		
        Status = NtQueryInformationToken(TokenHandle,
                                         TokenGroups,
                                         0,
                                         0,
                                         (PULONG)&dwReturnLength);
        if (Status == STATUS_BUFFER_TOO_SMALL) {
            // allocate requested buffer for temporary group buffer
            GroupBuffer = RtlAllocateHeap(RtlGetProcessHeap(), 0, dwReturnLength);
            Status = NtQueryInformationToken(TokenHandle,
                                         TokenGroups,
                                         GroupBuffer,
                                         dwReturnLength,
                                         (PULONG)&dwReturnLength);
        }
		
		if (Status != 0) {
				RtlFreeHeap(RtlGetProcessHeap(), 0, GroupBuffer);
				RtlSetLastWin32ErrorAndNtStatusFromNtStatus(Status);
				return FALSE;
		}		
		
        // Return it.
        //InformationBuffer->Groups = (SIZE_T)(InformationBuffer) + sizeof(DWORD) + sizeof(PVOID);
        for (i = 0; i < GroupBuffer->GroupCount; i++){
            if ((GroupBuffer->Groups[i].Attributes & SE_GROUP_LOGON_ID) == 0)
            {
                // Copy SID and return, assumes that buffer allocated
                InformationBuffer->Groups[0].Attributes = GroupBuffer->Groups[i].Attributes;
                InformationBuffer->Groups[0].Sid = &(InformationBuffer->Groups[1]);
                CopySid(GetLengthSid(GroupBuffer->Groups[i].Sid), &InformationBuffer->Groups[1], GroupBuffer->Groups[i].Sid);
                index++;
                break;
            }
        }
        InformationBuffer->GroupCount = index;
		ptr = (void*)InformationBuffer; // ugly hack, chrome sandbox of 98-109 requires different format
#ifdef _M_IX86
	    ptr[11] |= 0xC0; // OR the 11th byte with 0xC0;
#elif defined(_M_AMD64)
	    ptr[19] |= 0xC0; // OR the 12th byte, or 16th on x64 with 0xC0
#endif		
		*ReturnLength = sizeof(TOKEN_GROUPS) + sizeof(PVOID) + sizeof(DWORD) + SECURITY_MAX_SID_SIZE;
        // Free temp buffer.
        RtlFreeHeap(RtlGetProcessHeap(), 0, GroupBuffer);
        return TRUE;
    }
	
	if(TokenInformationClass == TokenAppContainerSid ){
        *ReturnLength = sizeof(PSID);
        if(TokenInformationLength < sizeof(PSID))
           return FALSE;
        TokenInformation = NULL;
        return TRUE;
	}
	
	if(TokenInformationClass == TokenElevationType ){
		TokenInformation = (PVOID)2;
		TokenInformationLength = sizeof(ULONG);
		return TRUE;
	}	
	
    if(TokenInformationClass == TokenIntegrityLevel || 
       TokenInformationClass == TokenElevation || 
       TokenInformationClass == TokenLinkedToken || 
       TokenInformationClass == TokenElevation){
        
        DbgPrint("GetTokenInformationInternal:: Unhandled Vista Token Case: %i\n", TokenInformationClass);
        
        Status = NtQueryInformationToken(TokenHandle,
                                         TokenInformationClass,
                                         TokenInformation,
                                         TokenInformationLength,
                                         (PULONG)ReturnLength);
        if (!NT_SUCCESS(Status))
        {
            //DbgPrint("GetTokenInformationInternal:: NtQueryInformationToken returned Status: 0x%08lx\n", Status);
            SetLastError(RtlNtStatusToDosError(Status));
            return FALSE;
        }
        
        
        return TRUE;
    }

    Status = NtQueryInformationToken(TokenHandle,
                                     TokenInformationClass,
                                     TokenInformation,
                                     TokenInformationLength,
                                     (PULONG)ReturnLength);
    if (!NT_SUCCESS(Status))
    {
        SetLastError(RtlNtStatusToDosError(Status));
        return FALSE;
    }

    return TRUE;
}

BOOL
APIENTRY
SetTokenInformationInternal (
    HANDLE TokenHandle,
    TOKEN_INFORMATION_CLASS TokenInformationClass,
    PVOID TokenInformation,
    DWORD TokenInformationLength
    )
{ 
    NTSTATUS Status;
	
    if(TokenInformationClass == TokenIntegrityLevel || 
       TokenInformationClass == TokenElevationType || 
       TokenInformationClass == TokenLinkedToken || 
       TokenInformationClass == TokenElevation ||
       TokenInformationClass == TokenLogonSid){
                 DbgPrint("SetTokenInformationInternal:: Unhandled Vista Token Case: %i\n", TokenInformationClass);
        Status = NtSetInformationToken(TokenHandle,
                                       TokenInformationClass,
                                       TokenInformation,
                                       TokenInformationLength);
        if (!NT_SUCCESS(Status))
        {        
            SetLastError(RtlNtStatusToDosError(Status));
            return FALSE;
        }

        return TRUE;
    }else{
		Status = NtSetInformationToken(TokenHandle,
									   TokenInformationClass,
									   TokenInformation,
									   TokenInformationLength);
		if (!NT_SUCCESS(Status))
		{
			SetLastError(RtlNtStatusToDosError(Status));
			return FALSE;
		}

		return TRUE;	
	}						  
}

BOOL 
WINAPI 
GetKernelObjectSecurityInternal(
  _In_      HANDLE               Handle,
  _In_      SECURITY_INFORMATION RequestedInformation,
  _Out_opt_ PSECURITY_DESCRIPTOR pSecurityDescriptor,
  _In_      DWORD                nLength,
  _Out_     LPDWORD              lpnLengthNeeded
)
{
	NTSTATUS Status;
	//This is a hack, for now is enabled because need a truly implementation of LABEL_SECURITY_INFORMATION (for Chrome and Chromium Framework)
	if(RequestedInformation & LABEL_SECURITY_INFORMATION)
	{	
		Status = NtQuerySecurityObject(Handle, RequestedInformation, pSecurityDescriptor,
                                               nLength, lpnLengthNeeded );
		
		if(!NT_SUCCESS(Status)){
			//DbgPrint("GetKernelObjectSecurityInternal::NtQuerySecurityObject returned Status: 0x%08lx\n", Status);	
			RequestedInformation = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION;
			goto tryAgain;
		}

		return TRUE;
	}
	
tryAgain:	
    return set_ntstatus( NtQuerySecurityObject(Handle, RequestedInformation, pSecurityDescriptor,
                                               nLength, lpnLengthNeeded ));
}


BOOL 
WINAPI 
SetKernelObjectSecurityInternal(
  _In_ HANDLE               Handle,
  _In_ SECURITY_INFORMATION SecurityInformation,
  _In_ PSECURITY_DESCRIPTOR SecurityDescriptor
)
{
	NTSTATUS Status;
	//This is a hack, for now is enabled because need a truly implementation of LABEL_SECURITY_INFORMATION (for Chrome and Chromium Framework)
	if(SecurityInformation & LABEL_SECURITY_INFORMATION)
	{

		// Status = NtSetSecurityObject(Handle, SecurityInformation, SecurityDescriptor);
		
		// if(!NT_SUCCESS(Status)){
			// //DbgPrint("SetKernelObjectSecurityInternal::NtSetSecurityObject returned Status: 0x%08lx\n", Status);
			// SecurityInformation = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION;
			// goto tryAgain;			
		// }

		return TRUE;
	}

//tryAgain:	

    Status = NtSetSecurityObject(Handle,
                                 SecurityInformation,
                                 SecurityDescriptor);
    if (!NT_SUCCESS(Status))
    {
        SetLastError(RtlNtStatusToDosError(Status));
        return FALSE;
    }

    return TRUE;
}

/**********************************************************************
 * SetNamedSecurityInfoW			EXPORTED
 *
 * @implemented
 */
DWORD
WINAPI
SetNamedSecurityInfoWInternal(
	LPWSTR pObjectName,
    SE_OBJECT_TYPE ObjectType,
    SECURITY_INFORMATION SecurityInfo,
    PSID psidOwner,
    PSID psidGroup,
    PACL pDacl,
    PACL pSacl)
{
	DWORD ret;

	//This is a hack, for now is enabled because need a truly implementation of LABEL_SECURITY_INFORMATION (for Chrome and Chromium Framework)
	if(SecurityInfo & LABEL_SECURITY_INFORMATION)
	{
		
		//SecurityInfo = SACL_SECURITY_INFORMATION;
		
		ret = SetNamedSecurityInfoW(pObjectName,
									 ObjectType,
									 SecurityInfo,
									 psidOwner,
									 psidGroup,
									 pDacl,
									 pSacl);
		
		if(ret != ERROR_SUCCESS){
			//DbgPrint("SetNamedSecurityInfoWInternal::SetNamedSecurityInfoW returned ret: 0x%08lx\n", ret);	
			SecurityInfo = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION;
			goto tryAgain;			
		}

		return ERROR_SUCCESS;
	}
	
tryAgain:	
	return SetNamedSecurityInfoW(pObjectName,
								 ObjectType,
								 SecurityInfo,
								 psidOwner,
								 psidGroup,
								 pDacl,
								 pSacl);	
}

/**********************************************************************
 * SetSecurityInfo			EXPORTED
 *
 * @implemented
 */
DWORD
WINAPI
SetSecurityInfoInternal(
	HANDLE handle,
    SE_OBJECT_TYPE ObjectType,
    SECURITY_INFORMATION SecurityInfo,
    PSID psidOwner,
    PSID psidGroup,
    PACL pDacl,
    PACL pSacl)
{
	DWORD resp;
	//This is a hack, for now is enabled because need a truly implementation of LABEL_SECURITY_INFORMATION (for Chrome and Chromium Framework)
	if(SecurityInfo & LABEL_SECURITY_INFORMATION)
	{
		resp = SetSecurityInfo(handle,
							   ObjectType,
							   SecurityInfo,
							   psidOwner,
							   psidGroup,
							   pDacl,
							   pSacl);
							   
		if(resp != ERROR_SUCCESS)
		{		
			//DbgPrint("SetSecurityInfoInternal::SetSecurityInfo return: %d\n", resp);	
			SecurityInfo = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION;
			goto tryAgain;			
		}			
	}	
	
tryAgain:						   
	return SetSecurityInfo(handle,
						   ObjectType,
						   SecurityInfo,
						   psidOwner,
						   psidGroup,
						   pDacl,
						   pSacl);					   
}

DWORD
WINAPI
GetSecurityInfoInternal(
	HANDLE handle,
    SE_OBJECT_TYPE ObjectType,
    SECURITY_INFORMATION SecurityInfo,
    PSID *ppsidOwner,
    PSID *ppsidGroup,
    PACL *ppDacl,
    PACL *ppSacl,
    PSECURITY_DESCRIPTOR *ppSecurityDescriptor
)
{
	DWORD resp;
	//This is a hack, for now is enabled because need a truly implementation of LABEL_SECURITY_INFORMATION (for Chrome and Chromium Framework)
	if(SecurityInfo & LABEL_SECURITY_INFORMATION)
	{
		resp = GetSecurityInfo(handle,
						   ObjectType,
						   SecurityInfo,
						   ppsidOwner,
						   ppsidGroup,
						   ppDacl,
						   ppSacl,
						   ppSecurityDescriptor);		
		
		if(resp != ERROR_SUCCESS)
		{		
			//DbgPrint("GetSecurityInfoInternal::GetSecurityInfo return: %d\n", resp);
			SecurityInfo = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION;
			goto tryAgain;				
		}
		return resp;		
	}	
		
tryAgain:		
	return GetSecurityInfo(handle,
						   ObjectType,
						   SecurityInfo,
						   ppsidOwner,
						   ppsidGroup,
						   ppDacl,
						   ppSacl,
						   ppSecurityDescriptor);						   
}

/**********************************************************************
 * GetNamedSecurityInfoW			EXPORTED
 *
 * @implemented
 */
DWORD
WINAPI
GetNamedSecurityInfoWInternal(
	LPWSTR pObjectName,
    SE_OBJECT_TYPE ObjectType,
    SECURITY_INFORMATION SecurityInfo,
    PSID *ppsidOwner,
    PSID *ppsidGroup,
    PACL *ppDacl,
    PACL *ppSacl,
    PSECURITY_DESCRIPTOR *ppSecurityDescriptor
)
{
	//DWORD resp;	
	//This is a hack, for now is enabled because need a truly implementation of LABEL_SECURITY_INFORMATION (for Chrome and Chromium Framework)
	if(SecurityInfo & LABEL_SECURITY_INFORMATION)
	{
		SecurityInfo = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION;
	}	
	
//tryAgain:	
	return GetNamedSecurityInfoW(pObjectName,
								 ObjectType,
								 SecurityInfo,
								 ppsidOwner,
								 ppsidGroup,
								 ppDacl,
								 ppSacl,
								 ppSecurityDescriptor);								 
}

/******************************************************************************
 *     ConvertStringSecurityDescriptorToSecurityDescriptorW   (sechost.@)
 */
BOOL 
WINAPI
DECLSPEC_HOTPATCH 
ConvertStringSecurityDescriptorToSecurityDescriptorWInternal(
        const WCHAR *string, DWORD revision, PSECURITY_DESCRIPTOR *sd, ULONG *ret_size )
{
    return ConvertStringSecurityDescriptorToSecurityDescriptorWImpl(string, revision, sd, ret_size);
}

/******************************************************************************
 * ConvertStringSecurityDescriptorToSecurityDescriptorA [ADVAPI32.@]
 */
BOOL 
WINAPI 
ConvertStringSecurityDescriptorToSecurityDescriptorAInternal(
        LPCSTR StringSecurityDescriptor,
        DWORD StringSDRevision,
        PSECURITY_DESCRIPTOR* SecurityDescriptor,
        PULONG SecurityDescriptorSize)
{
    BOOL ret;
    LPWSTR StringSecurityDescriptorW;

    TRACE("%s, %u, %p, %p\n", debugstr_a(StringSecurityDescriptor), StringSDRevision,
          SecurityDescriptor, SecurityDescriptorSize);

    if(!StringSecurityDescriptor)
        return FALSE;

    StringSecurityDescriptorW = strdupAW(StringSecurityDescriptor);
    ret = ConvertStringSecurityDescriptorToSecurityDescriptorWInternal(StringSecurityDescriptorW,
                                                               StringSDRevision, SecurityDescriptor,
                                                               SecurityDescriptorSize);
    heap_free(StringSecurityDescriptorW);

    return ret;
}	

/******************************************************************************
 *     ConvertStringSidToSidW   (sechost.@)
 */
BOOL 
WINAPI 
DECLSPEC_HOTPATCH 
ConvertStringSidToSidWInternal( const WCHAR *string, PSID *sid )
{
    return ConvertStringSidToSidWImpl(string, sid);
}

/******************************************************************************
 * ConvertStringSidToSidA [ADVAPI32.@]
 */
BOOL WINAPI ConvertStringSidToSidAInternal(LPCSTR StringSid, PSID* Sid)
{
    BOOL bret = FALSE;

    TRACE("%s, %p\n", debugstr_a(StringSid), Sid);
    if (GetVersion() & 0x80000000)
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    else if (!StringSid || !Sid)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        WCHAR *wStringSid = SERV_dup(StringSid);
        bret = ConvertStringSidToSidWInternal(wStringSid, Sid);
        heap_free(wStringSid);
    }
    return bret;
}

/******************************************************************************
 * CreateWellKnownSid   (kernelex.@)
 */
BOOL WINAPI CreateWellKnownSidInternal( WELL_KNOWN_SID_TYPE type, PSID domain, PSID sid, DWORD *size )
{
    return CreateWellKnownSidImpl(type, domain, sid, size);
}

/******************************************************************************
 * IsWellKnownSid   (kernelex.@)
 */
BOOL WINAPI IsWellKnownSidInternal( PSID sid, WELL_KNOWN_SID_TYPE type )
{
    return IsWellKnownSidImpl(sid, type);
}

// Prior to Windows 8.1, RegGetValueW does not support using REG_EXPAND_SZ without also using REG_NOEXPAND.
// While it's a minor thing, the launcher for Python 3.11 and above will fail to detect any Python installations.
// Work around this bug.
LSTATUS 
WINAPI 
RegGetValueWInternal(
    HKEY hkey, 
    LPCWSTR lpSubKey, 
    LPCWSTR lpValue, 
    DWORD dwFlags, 
    LPDWORD pdwType, 
    PVOID pvData, 
    LPDWORD pcbData
) 
{
    // First, check if the flags conflict. This is completely unsupported prior to Windows 8.1. While it's possible
    // to remove the check, it WILL result in issues.
    if ((dwFlags & RRF_RT_REG_EXPAND_SZ) && !(dwFlags & RRF_NOEXPAND)) {
        // Call RegGetValueW from PythonWin7, which is confirmed to fix this exact sceneraio.
        return Py_RegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
    }
	
	// InitNativeProcs();
	
	// //Check if original advapi32 has RegGetValueW, then, return original
    // if (pRegGetValueW){
		// return pRegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
	// }
	
    // Otherwise, call RegGetValueW like normal.	
    return RegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

LSTATUS 
WINAPI 
RegNotifyChangeKeyValueInternal(
    HKEY   hKey,
	BOOL   bWatchSubtree,
	DWORD  dwNotifyFilter,
	HANDLE hEvent, 
	BOOL   fAsynchronous
) 
{
	//For fix Dns Error on Electron
    return RegNotifyChangeKeyValue(hKey, bWatchSubtree, dwNotifyFilter & ~REG_NOTIFY_THREAD_AGNOSTIC, hEvent, fAsynchronous);
}

BOOLEAN
IsHklmWindowsNT(HANDLE hKey)
{
    BYTE buffer[1024];
    ULONG ret;
    NTSTATUS st;
    PKEY_NAME_INFORMATION info;
    ULONG chars;
    static const WCHAR TargetPrefix[] =
        L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT";
    ULONG prefixLen;

    st = NtQueryKey(hKey,
                    KeyNameInformation,
                    buffer,
                    sizeof(buffer),
                    &ret);

    if (!NT_SUCCESS(st))
        return FALSE;

    info = (PKEY_NAME_INFORMATION)buffer;

    chars = info->NameLength / sizeof(WCHAR);
    prefixLen = (ULONG)wcslen(TargetPrefix);

    if (chars < prefixLen)
        return FALSE;

    if (_wcsnicmp(info->Name, TargetPrefix, prefixLen) == 0)
        return TRUE;

    return FALSE;
}

/* Assume que os prototypes e imports necess?rios existem:
   NtQueryValueKey, NtQueryKey, RtlNtStatusToDosError, MapDefaultKey, ClosePredefKey,
   IsHKCRKey, QueryHKCRValue, ReadEmulatedVersion, GetProcessHeap, HeapAlloc, HeapFree,
   KEY_VALUE_PARTIAL_INFORMATION, KEY_NAME_INFORMATION, etc.
*/

#define EMULATED_BUF_CHARS 64
#define LOCAL_QUERY_BUFFER 256
#define INFO_BASE offsetof(KEY_VALUE_PARTIAL_INFORMATION, Data)

LSTATUS WINAPI RegQueryValueExWInternal(
    HKEY hKey,
    LPCWSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
)
{
    PPEB Peb;
    WCHAR versionBuf[32];
    SIZE_T need;
    DWORD cb;

    UNREFERENCED_PARAMETER(lpReserved);

    Peb = NtCurrentPeb();

    /* S� intercepta HKLM\Software\Microsoft\Windows NT\CurrentVersion */
    if (!IsHklmWindowsNT(hKey))
        return RegQueryValueExW(
            hKey, lpValueName, lpReserved, lpType, lpData, lpcbData
        );

    /*  intercepta CurrentVersion */
    if (!lpValueName || _wcsicmp(lpValueName, L"CurrentVersion") != 0)
        return RegQueryValueExW(
            hKey, lpValueName, lpReserved, lpType, lpData, lpcbData
        );

    /*
     * Compoe string diretamente do PEB
     * Ex: "6.1", "10.0"
     */
    swprintf(
        versionBuf,
        L"%lu.%lu",
        Peb->OSMajorVersion,
        Peb->OSMinorVersion
    );

    cb = (DWORD)(lstrlenW(versionBuf) + 1);
    need = cb * sizeof(WCHAR);

    if (lpType)
        *lpType = REG_SZ;

    if (lpcbData)
    {
        if (!lpData)
        {
            /* Apenas informa o tamanho necessário */
            *lpcbData = (DWORD)need;
            return ERROR_SUCCESS;
        }

        if (*lpcbData < need)
        {
            *lpcbData = (DWORD)need;
            return ERROR_MORE_DATA;
        }

        memcpy(lpData, versionBuf, need);
        *lpcbData = (DWORD)need;
    }

    return ERROR_SUCCESS;
}


LSTATUS WINAPI RegQueryValueExAInternal(
    HKEY hKey,
    LPCSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
)
{
    PPEB  Peb;
    CHAR  versionBuf[32];
    SIZE_T need;
    DWORD cb;

    UNREFERENCED_PARAMETER(lpReserved);

    Peb = NtCurrentPeb();

    /* Só intercepta HKLM\Software\Microsoft\Windows NT\CurrentVersion */
    if (!IsHklmWindowsNT(hKey))
        return RegQueryValueExA(
            hKey, lpValueName, lpReserved, lpType, lpData, lpcbData
        );

    /* Só intercepta CurrentVersion */
    if (!lpValueName || _stricmp(lpValueName, "CurrentVersion") != 0)
        return RegQueryValueExA(
            hKey, lpValueName, lpReserved, lpType, lpData, lpcbData
        );

    /*
     * Ex: "6.1", "10.0"
     */
    sprintf(
        versionBuf,
        "%lu.%lu",
        Peb->OSMajorVersion,
        Peb->OSMinorVersion
    );

    cb   = (DWORD)(lstrlenA(versionBuf) + 1);
    need = cb * sizeof(CHAR);

    if (lpType)
        *lpType = REG_SZ;

    if (lpcbData)
    {
        if (!lpData)
        {
            *lpcbData = (DWORD)need;
            return ERROR_SUCCESS;
        }

        if (*lpcbData < need)
        {
            *lpcbData = (DWORD)need;
            return ERROR_MORE_DATA;
        }

        memcpy(lpData, versionBuf, need);
        *lpcbData = (DWORD)need;
    }

    return ERROR_SUCCESS;
}

LSTATUS WINAPI RegQueryValueWInternal(
    HKEY hKey,
    LPCWSTR lpSubKey,
    LPWSTR lpData,
    PLONG lpcbData
)
{
    PPEB  Peb;
    WCHAR versionBuf[32];
    LONG  need;

    Peb = NtCurrentPeb();

    if (!IsHklmWindowsNT(hKey))
        return RegQueryValueW(hKey, lpSubKey, lpData, lpcbData);

    if (!lpSubKey || _wcsicmp(lpSubKey, L"CurrentVersion") != 0)
        return RegQueryValueW(hKey, lpSubKey, lpData, lpcbData);

    swprintf(
        versionBuf,
        L"%lu.%lu",
        Peb->OSMajorVersion,
        Peb->OSMinorVersion
    );

    need = (LONG)((lstrlenW(versionBuf) + 1) * sizeof(WCHAR));

    if (!lpcbData)
        return ERROR_SUCCESS;

    if (!lpData)
    {
        *lpcbData = need;
        return ERROR_SUCCESS;
    }

    if (*lpcbData < need)
    {
        *lpcbData = need;
        return ERROR_MORE_DATA;
    }

    memcpy(lpData, versionBuf, need);
    *lpcbData = need;

    return ERROR_SUCCESS;
}

LSTATUS WINAPI RegQueryValueAInternal(
    HKEY hKey,
    LPCSTR lpSubKey,
    LPSTR lpData,
    PLONG lpcbData
)
{
    PPEB  Peb;
    CHAR  versionBuf[32];
    LONG  need;

    Peb = NtCurrentPeb();

    if (!IsHklmWindowsNT(hKey))
        return RegQueryValueA(hKey, lpSubKey, lpData, lpcbData);

    if (!lpSubKey || _stricmp(lpSubKey, "CurrentVersion") != 0)
        return RegQueryValueA(hKey, lpSubKey, lpData, lpcbData);

    sprintf(
        versionBuf,
        "%lu.%lu",
        Peb->OSMajorVersion,
        Peb->OSMinorVersion
    );

    need = (LONG)(lstrlenA(versionBuf) + 1);

    if (!lpcbData)
        return ERROR_SUCCESS;

    if (!lpData)
    {
        *lpcbData = need;
        return ERROR_SUCCESS;
    }

    if (*lpcbData < need)
    {
        *lpcbData = need;
        return ERROR_MORE_DATA;
    }

    memcpy(lpData, versionBuf, need);
    *lpcbData = need;

    return ERROR_SUCCESS;
}