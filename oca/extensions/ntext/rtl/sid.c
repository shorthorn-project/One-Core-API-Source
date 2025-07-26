/*++

Copyright (c) 2025 Shorthorn Project

Module Name:

    sid.c

Abstract:

    This module implements RTL SID APIs

Author:

    Skulltrail 23-Jaunary-2025

Revision History:

--*/
 
#define NDEBUG

#include <main.h>

extern ULONG NtdllBaseTag;

NTSTATUS WINAPI RtlCreateServiceSid(
    PUNICODE_STRING ServiceName,
    PSID ServiceSid,
    PULONG ServiceSidLength) 
{
    NTSTATUS Status;
    UNICODE_STRING UpUnicodeString;
    SHA_CTX Sha1Context;
    SID_IDENTIFIER_AUTHORITY ServiceAuthority = {SECURITY_NT_AUTHORITY};
    ULONG TempSize;
    DWORD Sha1Hash[5];
	PISID Sid = ServiceSid;
    
    if (!ServiceName || !ServiceSidLength)
        return STATUS_INVALID_PARAMETER;
    
    TempSize = *ServiceSidLength;
    *ServiceSidLength = 0x20;
    
    // Check size
    if (TempSize < 0x20) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    
    // Uppercase the service name
    Status = RtlUpcaseUnicodeString(&UpUnicodeString, ServiceName, TRUE);
    if (!NT_SUCCESS(Status))
        return Status;
    
    // Get SHA-1 hash of the Unicode string we created
    A_SHAInit(&Sha1Context);
    A_SHAUpdate(&Sha1Context, (void*)&(UpUnicodeString.Buffer), UpUnicodeString.Length);
    A_SHAFinal(&Sha1Context, (void*)&Sha1Hash);
    RtlFreeUnicodeString(&UpUnicodeString);
    RtlInitializeSid(ServiceSid, &ServiceAuthority, 6);
    
    // Set the authority containing the 0x50 authority and the SHA-1 hash of the
    // uppercase service name.
    Sid->SubAuthority[0] = 0x50;
    Sid->SubAuthority[1] = Sha1Hash[0];
    Sid->SubAuthority[2] = Sha1Hash[1];
    Sid->SubAuthority[3] = Sha1Hash[2];
    Sid->SubAuthority[4] = Sha1Hash[3];
    Sid->SubAuthority[5] = Sha1Hash[4];
    
    return STATUS_SUCCESS;
}	

BOOLEAN 
RtlIsCapabilitySid(PSID CapabilitySidToCheck)
{
  BOOLEAN Verification; // cf@1
  BOOLEAN result; // al@4
  int Compare; // [sp+4h] [bp-Ch]@1

  Verification = (DWORD)CapabilitySidToCheck + 1 < 2u;
  Compare = 0;
  if ( Verification
    || (DWORD)CapabilitySidToCheck != 1
    || RtlCompareMemory((char *)CapabilitySidToCheck + 2, &Compare, 6u) != 6 )
    result = FALSE;
  else
    result = (DWORD)CapabilitySidToCheck == 3;
  return result;
}

#undef RtlCreateSecurityDescriptorRelative

NTSTATUS
RtlpCreateSecurityDescriptorRelative (
    IN PISECURITY_DESCRIPTOR_RELATIVE SecurityDescriptor,
    IN ULONG Revision
    )

/*++

Routine Description:

    This procedure initializes a new "relative format" security descriptor.
    After the procedure call the security descriptor is initialized with no
    system ACL, no discretionary ACL, no owner, no primary group and
    all control flags set to false (null).

Arguments:


    SecurityDescriptor - Supplies the security descriptor to
        initialize.

    Revision - Provides the revision level to assign to the security
        descriptor.  This should be one (1) for this release.

Return Value:

    STATUS_SUCCESS - Indicates the call completed successfully.

    STATUS_UNKNOWN_REVISION - Indicates the revision level provided
        is not supported by this routine.

Note:
    Warning, this code assume the caller allocated a relative security
    descriptor rather than a relative one.  Absolute is larger on systems
    with 64-bit pointers.

--*/

{
    //
    // Check the requested revision
    //

    if (Revision == SECURITY_DESCRIPTOR_REVISION) {

        //
        // Typecast to the opaque SECURITY_DESCRIPTOR structure.
        //

        RtlZeroMemory( SecurityDescriptor, sizeof(SECURITY_DESCRIPTOR_RELATIVE));

        SecurityDescriptor->Revision = SECURITY_DESCRIPTOR_REVISION;

        return STATUS_SUCCESS;
    }

    return STATUS_UNKNOWN_REVISION;
}

NTSTATUS 
NTAPI
RtlCheckTokenCapability(
	HANDLE TokenHandle, 
	PSID CapabilitySidToCheck, 
	PBOOL HasCapability
)
{
  PISECURITY_DESCRIPTOR_RELATIVE SecurityDescriptor; // edi@1 MAPDST
  NTSTATUS Status; // esi@4
  int SidSize; // ecx@7
  int Size; // esi@7
  PVOID CurrentHeap; // ST14_4@7
  struct _SECURITY_DESCRIPTOR_RELATIVE *DescriptorRelative; // eax@7
  OBJECT_ATTRIBUTES ObjectAttributes; // [sp+Ch] [bp-98h]@28
  ACCESS_MASK GrantedAccess; // [sp+28h] [bp-7Ch]@15
  ULONG SizeACL; // [sp+30h] [bp-74h]@7
  ULONG PrivilegeSetLength; // [sp+34h] [bp-70h]@15
  NTSTATUS AccessStatus; // [sp+38h] [bp-6Ch]@15
  ULONG ReturnLength; // [sp+44h] [bp-60h]@4
  HANDLE NewTokenHandle; // [sp+48h] [bp-5Ch]@1
  GENERIC_MAPPING GenericMapping; // [sp+4Ch] [bp-58h]@1
  ULONG SecurityQualityOfService; // [sp+5Ch] [bp-48h]@28
  PRIVILEGE_SET PrivilegeSet; // [sp+68h] [bp-3Ch]@15

  NewTokenHandle = NULL;
  SecurityDescriptor = NULL;
  GenericMapping.GenericRead = 0x20001;
  GenericMapping.GenericWrite = 0x30000;
  GenericMapping.GenericExecute = 0x20000;
  GenericMapping.GenericAll = 0x1F0001;
  *HasCapability = FALSE;
  if ( !RtlIsCapabilitySid(CapabilitySidToCheck) )
  {
    Status = STATUS_INVALID_PARAMETER;
    goto Finalizing;
  }
  if ( TokenHandle )
  {
    NewTokenHandle = TokenHandle;
Working:
    ReturnLength = 0;
    Status = ZwQueryInformationToken(TokenHandle, TokenUser, 0, 0, &ReturnLength);
    if ( Status != STATUS_BUFFER_TOO_SMALL )
      goto Finalizing;
    SecurityDescriptor = (PISECURITY_DESCRIPTOR_RELATIVE)RtlAllocateHeap(
                                                           NtCurrentPeb()->ProcessHeap,
                                                           NtdllBaseTag + 1310720,
                                                           ReturnLength);
    if ( !SecurityDescriptor )
      goto Insuficiente_resources;
    Status = ZwQueryInformationToken(NewTokenHandle, TokenUser, SecurityDescriptor, ReturnLength, &ReturnLength);
    if ( Status < 0 )
      goto Finalizing;
    SidSize = (DWORD)CapabilitySidToCheck + 1 + 3 * (DWORD)&SecurityDescriptor->Revision + 1;
    Size = 4 * SidSize + 76;
    CurrentHeap = NtCurrentPeb()->ProcessHeap;
    SizeACL = 4 * SidSize + 76;
    DescriptorRelative = (struct _SECURITY_DESCRIPTOR_RELATIVE *)RtlAllocateHeap(
                                                                   CurrentHeap,
                                                                   NtdllBaseTag + 1310720,
                                                                   Size);
    SecurityDescriptor = DescriptorRelative;
    if ( DescriptorRelative )
    {
      memset(DescriptorRelative, 0, Size);
      Status = RtlpCreateSecurityDescriptorRelative(SecurityDescriptor, 1);
      if ( Status >= 0 )
      {
        Status = RtlSetOwnerSecurityDescriptor(SecurityDescriptor, *(PSID *)&SecurityDescriptor->Revision, 0);
        if ( Status >= 0 )
        {
          Status = RtlSetGroupSecurityDescriptor(SecurityDescriptor, *(PSID *)&SecurityDescriptor->Revision, 0);
          if ( Status >= 0 )
          {
            Status = RtlCreateAcl((PACL)&SecurityDescriptor[1], SizeACL - 20, 2u);
            if ( Status >= 0 )
            {
              Status = RtlAddAccessAllowedAce(
                         (PACL)&SecurityDescriptor[1],
                         2u,
                         0x10001u,
                         *(PSID *)&SecurityDescriptor->Revision);
              if ( Status >= 0 )
              {
                Status = RtlAddAccessAllowedAce((PACL)&SecurityDescriptor[1], 2u, 0x10001u, CapabilitySidToCheck);
                if ( Status >= 0 )
                {
                  Status = RtlSetDaclSecurityDescriptor(
                             (PSECURITY_DESCRIPTOR)SecurityDescriptor,
                             1u,
                             (PACL)&SecurityDescriptor[1],
                             0);
                  if ( Status >= 0 )
                  {
                    PrivilegeSetLength = 56;
                    Status = NtAccessCheck(
                               (PSECURITY_DESCRIPTOR)SecurityDescriptor,
                               NewTokenHandle,
                               0x10001u,
                               &GenericMapping,
                               &PrivilegeSet,
                               &PrivilegeSetLength,
                               &GrantedAccess,
                               &AccessStatus);
                    if ( Status >= 0 )
                    {
                      if ( !AccessStatus && GrantedAccess == 65537 )
                        *HasCapability = TRUE;
                      Status = STATUS_SUCCESS;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    else
    {
Insuficiente_resources:
      Status = STATUS_INSUFFICIENT_RESOURCES;
    }
Finalizing:
    if ( TokenHandle )
      goto Free_memory;
    goto Close_token;
  }
  Status = NtOpenThreadTokenEx((HANDLE)-2, 8, 1, 0, &NewTokenHandle);
  if ( Status == STATUS_NO_TOKEN )
  {
    Status = ZwOpenProcessTokenEx((HANDLE)-1, 10, 0, &TokenHandle);
    if ( Status < STATUS_SUCCESS )
      goto Close_token;
    ObjectAttributes.SecurityQualityOfService = &SecurityQualityOfService;
    ObjectAttributes.Length = 24;
    ObjectAttributes.RootDirectory = 0;
    ObjectAttributes.Attributes = 0;
    ObjectAttributes.ObjectName = 0;
    ObjectAttributes.SecurityDescriptor = 0;
    SecurityQualityOfService = 12;
    Status = NtDuplicateToken(TokenHandle, 8u, &ObjectAttributes, 0, TokenImpersonation, &NewTokenHandle);
    NtClose(TokenHandle);
  }
  if ( Status >= STATUS_SUCCESS )
  {
    TokenHandle = NewTokenHandle;
    goto Working;
  }
Close_token:
  if ( NewTokenHandle )
    NtClose(NewTokenHandle);
Free_memory:
  if ( SecurityDescriptor )
    RtlFreeHeap(NtCurrentPeb()->ProcessHeap, 0, SecurityDescriptor);
  return Status;
}

NTSTATUS 
NTAPI 
RtlCheckTokenMembershipEx(
	HANDLE TokenHandle, 
	PSID SidToCheck, 
	DWORD Flas, 
	PBOOL IsMember
)
{
	DbgPrint("UNIMPLEMENTED: RtlCheckTokenMembershipEx");
	return STATUS_SUCCESS;
}	