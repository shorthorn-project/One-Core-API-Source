/*
 * Copyright (C) 2005 Benjamin Cutler
 * Copyright (C) 2008 Dmitry Chapyshev
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */


#include <stdarg.h>

#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winreg.h>
#define NTOS_MODE_USER
#include <ndk/pofuncs.h>
#include <ndk/rtlfuncs.h>
#include <ndk/setypes.h>
#include <powrprof.h>
#include <wine/debug.h>
#include <wine/unicode.h>

WINE_DEFAULT_DEBUG_CHANNEL(powrprof);

typedef PVOID HPOWERNOTIFY, *PHPOWERNOTIFY;

static const WCHAR szPowerCfgSubKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Controls Folder\\PowerCfg";
static const WCHAR szUserPowerConfigSubKey[] = 
    L"Control Panel\\PowerCfg";
static const WCHAR szCurrentPowerPolicies[] = 
    L"CurrentPowerPolicy";
static const WCHAR szPolicies[] = L"Policies";
static const WCHAR szName[] = L"Name";
static const WCHAR szDescription[] = L"Description";
static const WCHAR szSemaphoreName[] = L"PowerProfileRegistrySemaphore";
static const WCHAR szDiskMax[] = L"DiskSpindownMax";
static const WCHAR szDiskMin[] = L"DiskSpindownMin";
static const WCHAR szLastID[] = L"LastID";

typedef enum  { 
  PlatformRoleUnspecified        = 0,
  PlatformRoleDesktop            = 1,
  PlatformRoleMobile             = 2,
  PlatformRoleWorkstation        = 3,
  PlatformRoleEnterpriseServer   = 4,
  PlatformRoleSOHOServer         = 5,
  PlatformRoleAppliancePC        = 6,
  PlatformRolePerformanceServer  = 7,
  PlatformRoleSlate              = 8,
  PlatformRoleMaximum
} POWER_PLATFORM_ROLE;

typedef enum _POWER_DATA_ACCESSOR {
    ACCESS_AC_POWER_SETTING_INDEX,
    ACCESS_DC_POWER_SETTING_INDEX,
    ACCESS_FRIENDLY_NAME,
    ACCESS_DESCRIPTION,
    ACCESS_POSSIBLE_POWER_SETTING,
    ACCESS_POSSIBLE_POWER_SETTING_FRIENDLY_NAME,
    ACCESS_POSSIBLE_POWER_SETTING_DESCRIPTION,
    ACCESS_DEFAULT_AC_POWER_SETTING,
    ACCESS_DEFAULT_DC_POWER_SETTING,
    ACCESS_POSSIBLE_VALUE_MIN,
    ACCESS_POSSIBLE_VALUE_MAX,
    ACCESS_POSSIBLE_VALUE_INCREMENT,
    ACCESS_POSSIBLE_VALUE_UNITS,
    ACCESS_ICON_RESOURCE,
    ACCESS_DEFAULT_SECURITY_DESCRIPTOR,
    ACCESS_ATTRIBUTES,
    ACCESS_SCHEME,
    ACCESS_SUBGROUP,
    ACCESS_INDIVIDUAL_SETTING,
    ACCESS_ACTIVE_SCHEME,
    ACCESS_CREATE_SCHEME,
    ACCESS_AC_POWER_SETTING_MAX,
    ACCESS_DC_POWER_SETTING_MAX,
    ACCESS_AC_POWER_SETTING_MIN,
    ACCESS_DC_POWER_SETTING_MIN,
    ACCESS_PROFILE,
    ACCESS_OVERLAY_SCHEME,
    ACCESS_ACTIVE_OVERLAY_SCHEME,
} POWER_DATA_ACCESSOR, *PPOWER_DATA_ACCESSOR;

typedef enum EFFECTIVE_POWER_MODE
 {
     EffectivePowerModeBatterySaver,
     EffectivePowerModeBetterBattery,
     EffectivePowerModeBalanced,
     EffectivePowerModeHighPerformance,
     EffectivePowerModeMaxPerformance,
     EffectivePowerModeGameMode,
     EffectivePowerModeMixedReality,
 } EFFECTIVE_POWER_MODE;

UINT g_LastID = (UINT)-1;

static POWER_PLATFORM_ROLE (WINAPI *pPowerDeterminePlatformRole)(void) = NULL;
static POWER_PLATFORM_ROLE (WINAPI *pPowerDeterminePlatformRoleEx)(ulong) = NULL;
static DWORD (WINAPI *pPowerEnumerate)(HKEY,const GUID*,const GUID*,POWER_DATA_ACCESSOR,ULONG,UCHAR*,DWORD*) = NULL;
static DWORD (WINAPI *pPowerGetActiveScheme)(HKEY,GUID**) = NULL;
static DWORD (WINAPI *pPowerReadACValue)(HKEY,const GUID*,const GUID*,const GUID*,PULONG,UCHAR*,DWORD*) = NULL;
static DWORD (WINAPI *pPowerReadDCValue)(HKEY,const GUID*,const GUID*,const GUID*,PULONG,UCHAR*,DWORD*) = NULL;
static DWORD (WINAPI *pPowerReadACValueIndex)(HKEY,const GUID*,const GUID*,const GUID*,LPDWORD) = NULL;
static DWORD (WINAPI *pPowerReadDCValueIndex)(HKEY,const GUID*,const GUID*,const GUID*,LPDWORD) = NULL;
static DWORD (WINAPI *pPowerReadFriendlyName)(HKEY,const GUID*,const GUID*,const GUID*,UCHAR*,DWORD*) = NULL;
static DWORD (WINAPI *pPowerSetActiveScheme)(HKEY,const GUID*) = NULL;
static DWORD (WINAPI *pPowerRegisterSuspendResumeNotification)(DWORD, HANDLE, PHPOWERNOTIFY) = NULL;
static DWORD (WINAPI *pPowerRegisterForEffectivePowerModeNotifications)(ULONG, PVOID, void *, void *) = NULL;
static DWORD (WINAPI *pPowerUnregisterSuspendResumeNotification)(PHPOWERNOTIFY) = NULL;
static DWORD (WINAPI *pPowerUnregisterFromEffectivePowerModeNotifications)(PVOID) = NULL;
static DWORD (WINAPI *pPowerSettingRegisterNotification)(LPCGUID,DWORD,HANDLE,PHPOWERNOTIFY) = NULL;
static DWORD (WINAPI *pPowerSettingUnregisterNotification)(HPOWERNOTIFY) = NULL;
static DWORD (WINAPI *pPowerWriteACValueIndex)(HKEY,const GUID*,const GUID*,const GUID*,DWORD) = NULL;
static DWORD (WINAPI *pPowerWriteDCValueIndex)(HKEY,const GUID*,const GUID*,const GUID*,DWORD) = NULL;
static DWORD (WINAPI *pPowerWriteSettingAttributes)(const GUID*,const GUID*,DWORD) = NULL;

static HMODULE hPowrProf = NULL;

static void InitPowrProf(void)
{
    // if (hPowrProf)
        // return;

    hPowrProf = GetModuleHandleW(L"powrprofbase.dll");
    if (!hPowrProf)
        hPowrProf = LoadLibraryW(L"powrprofbase.dll");

    // if (!hPowrProf)
        // return;

    pPowerDeterminePlatformRole =
        (void *)GetProcAddress(hPowrProf, "PowerDeterminePlatformRole");
		
    pPowerDeterminePlatformRoleEx =
        (void *)GetProcAddress(hPowrProf, "PowerDeterminePlatformRoleEx");		

    pPowerEnumerate =
        (void *)GetProcAddress(hPowrProf, "PowerEnumerate");

    pPowerGetActiveScheme =
        (void *)GetProcAddress(hPowrProf, "PowerGetActiveScheme");

    pPowerReadACValue =
        (void *)GetProcAddress(hPowrProf, "PowerReadACValue");

    pPowerReadDCValue =
        (void *)GetProcAddress(hPowrProf, "PowerReadDCValue");

    pPowerReadACValueIndex =
        (void *)GetProcAddress(hPowrProf, "PowerReadACValueIndex");

    pPowerReadDCValueIndex =
        (void *)GetProcAddress(hPowrProf, "PowerReadDCValueIndex");

    pPowerReadFriendlyName =
        (void *)GetProcAddress(hPowrProf, "PowerReadFriendlyName");

    pPowerSetActiveScheme =
        (void *)GetProcAddress(hPowrProf, "PowerSetActiveScheme");
		
    pPowerRegisterSuspendResumeNotification =
        (void *)GetProcAddress(hPowrProf, "PowerRegisterSuspendResumeNotification");
		
    pPowerRegisterForEffectivePowerModeNotifications =
        (void *)GetProcAddress(hPowrProf, "PowerRegisterForEffectivePowerModeNotifications");

    pPowerSettingRegisterNotification =
        (void *)GetProcAddress(hPowrProf, "PowerSettingRegisterNotification");

    pPowerSettingUnregisterNotification =
        (void *)GetProcAddress(hPowrProf, "PowerSettingUnregisterNotification");
		
    pPowerUnregisterSuspendResumeNotification =
        (void *)GetProcAddress(hPowrProf, "PowerUnregisterSuspendResumeNotification");	

    pPowerUnregisterFromEffectivePowerModeNotifications =
        (void *)GetProcAddress(hPowrProf, "PowerUnregisterFromEffectivePowerModeNotifications");			

    pPowerWriteACValueIndex =
        (void *)GetProcAddress(hPowrProf, "PowerWriteACValueIndex");

    pPowerWriteDCValueIndex =
        (void *)GetProcAddress(hPowrProf, "PowerWriteDCValueIndex");

    pPowerWriteSettingAttributes =
        (void *)GetProcAddress(hPowrProf, "PowerWriteSettingAttributes");
}

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv)
{
    switch(fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            InitPowrProf();
            break;
    }

    return TRUE;
}

DWORD WINAPI
PowerGetActiveScheme(
	HKEY UserRootPowerKey,
	GUID **polguid
)
{
    if(pPowerGetActiveScheme)
	{
		return pPowerGetActiveScheme(UserRootPowerKey, polguid);
	}
	
    return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
WINAPI 
PowerSetActiveScheme(
  _In_opt_       HKEY UserRootPowerKey,
  _In_     const GUID *SchemeGuid
)
{
    if(pPowerSetActiveScheme)
	{
		return PowerSetActiveScheme(UserRootPowerKey, SchemeGuid);
	}
	
    return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
WINAPI
PowerReadDCValue(
	HKEY RootPowerKey, 
	const GUID *Scheme, 
	const GUID *SubGroup, 
	const GUID *PowerSettings, 
	PULONG Type, 
	PUCHAR Buffer, 
	DWORD *BufferSize
)
{
    if(pPowerReadDCValue)
	{
		return pPowerReadDCValue(RootPowerKey, 
		                        Scheme,
								SubGroup,
								PowerSettings,
								Type,
								Buffer,
								BufferSize);
	}
	
    return ERROR_CALL_NOT_IMPLEMENTED;
}

//Need more implementation
POWER_PLATFORM_ROLE 
WINAPI 
PowerDeterminePlatformRole(void)
{
    if(pPowerDeterminePlatformRole)
	{
		return pPowerDeterminePlatformRole();
	}
	
	return PlatformRoleDesktop;
}

//Need more implementation
POWER_PLATFORM_ROLE 
WINAPI 
PowerDeterminePlatformRoleEx(
  _In_ ULONG Version
)
{
    if(pPowerDeterminePlatformRoleEx)
	{
		return pPowerDeterminePlatformRoleEx(Version);
	}
	
	return PlatformRoleDesktop;
}

DWORD 
WINAPI 
PowerReadACValue(
  _In_opt_          HKEY    RootPowerKey,
  _In_opt_    const GUID    *SchemeGuid,
  _In_opt_    const GUID    *SubGroupOfPowerSettingsGuid,
  _In_opt_    const GUID    *PowerSettingGuid,
  _Out_opt_         PULONG  Type,
  _Out_opt_         LPBYTE  Buffer,
  _Inout_opt_       LPDWORD BufferSize
)
{
    if(pPowerReadACValue)
	{
		return pPowerReadACValue(RootPowerKey, 
		                        SchemeGuid,
								SubGroupOfPowerSettingsGuid,
								PowerSettingGuid,
								Type,
								Buffer,
								BufferSize);
	}
	
    return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
WINAPI 
PowerSettingRegisterNotification(
	const GUID *setting, 
	DWORD flags, 
	HANDLE recipient, 
	PHPOWERNOTIFY handle)
{
    DbgPrint("(%s,0x%08lx,%p,%p) stub!\n", setting, flags, recipient, handle);
	
    if(pPowerSettingRegisterNotification)
	{
		return pPowerSettingRegisterNotification(setting, 
												 flags,
												 recipient,
												 handle);
	}	
	
    *handle = (PHPOWERNOTIFY)0xdeadbeef;
    return ERROR_SUCCESS;
}

DWORD WINAPI PowerSettingUnregisterNotification(HPOWERNOTIFY handle)
{
    DbgPrint("(%p) stub!\n", handle);
	
    if(pPowerSettingUnregisterNotification)
	{
		return pPowerSettingUnregisterNotification(handle);
	}		
	
    return ERROR_SUCCESS;
}

DWORD 
WINAPI 
PowerWriteDCValueIndex(
  _In_opt_       HKEY  RootSystemPowerKey,
  _In_     const GUID  *SchemePersonalityGuid,
  _In_opt_ const GUID  *SubGroupOfPowerSettingsGuid,
  _In_     const GUID  *PowerSettingGuid,
  _In_           DWORD DefaultDcIndex
)
{
    if(pPowerWriteDCValueIndex)
	{
		return pPowerWriteDCValueIndex(RootSystemPowerKey,
									   SchemePersonalityGuid,
									   SubGroupOfPowerSettingsGuid,
									   PowerSettingGuid,
									   DefaultDcIndex);
	}	
    return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
WINAPI 
PowerReadFriendlyName(
  _In_opt_        HKEY    RootPowerKey,
  _In_opt_  const GUID    *SchemeGuid,
  _In_opt_  const GUID    *SubGroupOfPowerSettingsGuid,
  _In_opt_  const GUID    *PowerSettingGuid,
  _Out_opt_       PUCHAR  Buffer,
  _Inout_         LPDWORD BufferSize
)
{
    if(pPowerReadFriendlyName)
	{
		return pPowerReadFriendlyName(RootPowerKey,
	                                  SchemeGuid,
							          SubGroupOfPowerSettingsGuid,
							          PowerSettingGuid,
							          Buffer,
							          BufferSize);
	}
	
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD 
WINAPI 
PowerEnumerate(
	HKEY key, 
	const GUID *scheme, 
	const GUID *subgroup, 
	POWER_DATA_ACCESSOR flags,
    ULONG index, 
	UCHAR *buffer, 
	DWORD *buffer_size)
{
    if(pPowerEnumerate)
	{
		return pPowerEnumerate(key,
	                           scheme,
							   subgroup,
							   flags,
							   index,
							   buffer,
							   buffer_size);
	}
	
	return ERROR_CALL_NOT_IMPLEMENTED;
}


DWORD 
WINAPI 
PowerRegisterSuspendResumeNotification(DWORD flags, HANDLE recipient, PHPOWERNOTIFY handle)
{
    DbgPrint("(0x%08lx,%p,%p) stub!\n", flags, recipient, handle);
	
    if(pPowerRegisterSuspendResumeNotification)
	{
		return pPowerRegisterSuspendResumeNotification(flags,
													   recipient,
													   handle);
	}	
	
    *handle = (HPOWERNOTIFY)0xdeadbeef;
    return ERROR_SUCCESS;
}

DWORD 
WINAPI 
PowerUnregisterSuspendResumeNotification(
	HPOWERNOTIFY handle
	)
{
    DbgPrint("(%p) stub!\n", handle);
	
    if(pPowerUnregisterSuspendResumeNotification)
	{
		return pPowerUnregisterSuspendResumeNotification(handle);
	}		

    return ERROR_SUCCESS;
}

DWORD 
WINAPI 
PowerWriteSettingAttributes(
	const GUID *SubGroupGuid, 
	const GUID *PowerSettingGuid, 
	DWORD Attributes)
{
    if(pPowerWriteSettingAttributes)
	{
		return pPowerWriteSettingAttributes(SubGroupGuid,
											PowerSettingGuid,
											Attributes);
	}	
	
	return ERROR_SUCCESS;
}

DWORD 
WINAPI 
PowerWriteACValueIndex(
	HKEY key, 
	const GUID *scheme, 
	const GUID *subgroup, 
	const GUID *setting, 
	DWORD index)
{	
    if(pPowerWriteACValueIndex)
	{
		return pPowerWriteACValueIndex(key,
	                           scheme,
							   subgroup,
							   setting,
							   index);
	}
	
	DbgPrint("(%p,%s,%s,%s,0x%08lx) stub!\n", key, (scheme), (subgroup), (setting), index);	
	
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD
WINAPI
PowerReadACValueIndex(
  HKEY       RootPowerKey,
  const GUID *SchemeGuid,
  const GUID *SubGroupOfPowerSettingsGuid,
  const GUID *PowerSettingGuid,
  LPDWORD    AcValueIndex
)
{
    if(pPowerReadACValueIndex)
	{
		return pPowerReadACValueIndex(RootPowerKey,
	                                  SchemeGuid,
									  SubGroupOfPowerSettingsGuid,
							          PowerSettingGuid,
							          AcValueIndex);
	}
	
	return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD
WINAPI
PowerReadDCValueIndex(
	HKEY       RootPowerKey,
	const GUID *SchemeGuid,
	const GUID *SubGroupOfPowerSettingsGuid,
	const GUID *PowerSettingGuid,
	LPDWORD    DcValueIndex
)
{	
    //DbgPrint("(%p,%s,%s,%s,0x%08lx) stub!\n", RootPowerKey, (SchemeGuid), (SubGroupOfPowerSettingsGuid), (PowerSettingGuid), DcValueIndex);
    if(pPowerReadDCValueIndex)
	{
		return pPowerReadDCValueIndex(RootPowerKey,
	                                  SchemeGuid,
									  SubGroupOfPowerSettingsGuid,
							          PowerSettingGuid,
							          DcValueIndex);
	}
	
	return ERROR_CALL_NOT_IMPLEMENTED;
}

// Win 2019 LTSC APIs - required by new Edge on 1809+ modes
HRESULT 
WINAPI 
PowerRegisterForEffectivePowerModeNotifications(
	ULONG version, 
	PVOID callback, 
	void *context, 
	void **handle
	)
 {
    if(pPowerRegisterForEffectivePowerModeNotifications)
	{
		return pPowerRegisterForEffectivePowerModeNotifications(version,
																callback,
																context,
																handle);
	}	 
	 
     return S_OK;
 }

HRESULT 
WINAPI 
PowerUnregisterFromEffectivePowerModeNotifications(
	PVOID RegistrationHandle
	) 
{
    if(pPowerUnregisterFromEffectivePowerModeNotifications)
	{
		return pPowerUnregisterFromEffectivePowerModeNotifications(RegistrationHandle);
	}		
    return S_OK;
}