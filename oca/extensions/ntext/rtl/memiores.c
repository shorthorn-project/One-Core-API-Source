/*++

Copyright (c) 2026 Shorthorn Project

Module Name:

    ldrrsrc.c

Abstract:

    This module implements RTL Encode/Decode Memory In/Out Resource APIs

Author:

    Murak 01-July-2028

Revision History:

--*/
 
#define NDEBUG

#include <main.h>

typedef struct _CM_PARTIAL_RESOURCE_DESCRIPTOR_OCA {
    UCHAR Type;
    UCHAR ShareDisposition;
    USHORT Flags;
    union {
	struct {
	    PHYSICAL_ADDRESS Start;
	    ULONG Length;
	} Generic;
	struct {
	    PHYSICAL_ADDRESS Start;
	    ULONG Length;
	} Port;
	struct {
	    ULONG Level;
	    ULONG Vector;
	    KAFFINITY Affinity;
	} Interrupt;
	struct {
	    union {
		struct {
		    USHORT Reserved;
		    USHORT MessageCount;
		    ULONG MessageData;
		    ULONG_PTR MessageAddress;
		} Raw;
		struct {
		    ULONG Level;
		    ULONG Vector;
		    KAFFINITY Affinity;
		} Translated;
	    };
	} MessageInterrupt;
	struct {
	    PHYSICAL_ADDRESS Start;
	    ULONG Length;
	} Memory;
	struct {
	    ULONG Channel;
	    ULONG Port;
	    ULONG Reserved1;
	} Dma;
	struct {
	    ULONG Data[3];
	} DevicePrivate;
	struct {
	    ULONG Start;
	    ULONG Length;
	    ULONG Reserved;
	} BusNumber;
	struct {
	    ULONG DataSize;
	    ULONG Reserved1;
	    ULONG Reserved2;
	} DeviceSpecificData;
	struct {
	    PHYSICAL_ADDRESS Start;
	    ULONG Length40;
	} Memory40;
	struct {
	    PHYSICAL_ADDRESS Start;
	    ULONG Length48;
	} Memory48;
	struct {
	    PHYSICAL_ADDRESS Start;
	    ULONG Length64;
	} Memory64;
    }u;
} CM_PARTIAL_RESOURCE_DESCRIPTOR_OCA, *PCM_PARTIAL_RESOURCE_DESCRIPTOR_OCA;

// implementation from Vista SP1
ULONGLONG NTAPI RtlCmDecodeMemIoResource(PCM_PARTIAL_RESOURCE_DESCRIPTOR_OCA Descriptor, PULONGLONG Start)
{
	ULONGLONG length;
	ULONG flags;
	
	if (Descriptor->Type == CmResourceTypeMemory || Descriptor->Type == CmResourceTypePort) {
		length = Descriptor->u.Generic.Length;
	} else { // it is Descriptor->Type == CmResourceTypeMemoryLarge here
		flags = Descriptor->Flags;
		
		if (flags & CM_RESOURCE_MEMORY_LARGE_40) {
			length = ((ULONGLONG)Descriptor->u.Memory40.Length40) << 8;
		} else if (flags & CM_RESOURCE_MEMORY_LARGE_48) {
			length = ((ULONGLONG)Descriptor->u.Memory48.Length48) << 16;
		} else if (flags & CM_RESOURCE_MEMORY_LARGE_64) {
			length = ((ULONGLONG)Descriptor->u.Memory64.Length64) << 32;
		} else {
			length = 0;
		}
	}
	
	if (Start)
		*Start = Descriptor->u.Generic.Start.QuadPart;
		
	return length;
}

// implementation from Vista SP1
NTSTATUS NTAPI RtlCmEncodeMemIoResource(PCM_PARTIAL_RESOURCE_DESCRIPTOR_OCA Descriptor, UCHAR Type, ULONGLONG Length, ULONGLONG Start) {
	if (Type == CmResourceTypeMemory || Type == CmResourceTypeMemoryLarge) {
		Descriptor->Flags &= ~CM_RESOURCE_MEMORY_LARGE;
		Descriptor->u.Generic.Start.QuadPart = Start;
		if (Length <= 0xFFFFFFFF) { // standard CmResourceTypeMemory
			Descriptor->Type = CmResourceTypeMemory;
			Descriptor->u.Generic.Length = Length;
		} else if (Length <= 0xFFFFFFFFFFLL) { // CmResourceTypeMemoryLarge, Memory40
			if (Length & 0xFF) // can't be represented in a Memory40
				return STATUS_UNSUCCESSFUL;
			Descriptor->Type = CmResourceTypeMemoryLarge;
			Descriptor->Flags |= CM_RESOURCE_MEMORY_LARGE_40;
			Descriptor->u.Generic.Length = Length >> 8;
		} else if (Length <= 0xFFFFFFFFFFFFLL) { // CmResourceTypeMemoryLarge, Memory48
			if (Length & 0xFFFF) // can't be represented in a Memory48
				return STATUS_UNSUCCESSFUL;
			Descriptor->Type = CmResourceTypeMemoryLarge;
			Descriptor->Flags |= CM_RESOURCE_MEMORY_LARGE_48;
			Descriptor->u.Generic.Length = Length >> 16;
		} else { // CmResourceTypeMemoryLarge, Memory64
			if (Length & 0xFFFFFFFF) // can't be represented in a Memory64
				return STATUS_UNSUCCESSFUL;
			Descriptor->Type = CmResourceTypeMemoryLarge;
			Descriptor->Flags |= CM_RESOURCE_MEMORY_LARGE_64;
			Descriptor->u.Generic.Length = Length >> 32;
		}
		
		return STATUS_SUCCESS;
	} else if (Type == CmResourceTypePort) {
		if (Length > 0xFFFFFFFF)
			return STATUS_UNSUCCESSFUL;
		
		Descriptor->u.Generic.Start.QuadPart = Start;
		Descriptor->u.Generic.Length = Length;
		Descriptor->Type = CmResourceTypePort;	
	}
	
	return STATUS_UNSUCCESSFUL;
}