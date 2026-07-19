/*++

Copyright (c) 2022  Shorthorn Project

Module Name:

    synch.c

Abstract:

    This module implements all NTAPI syncronization
    objects.

Author:

    Skulltrail 18-October-2022

Revision History:

--*/

#include <main.h>
#include <config.h>
#include <port.h>

#ifdef _WIN64
#define InterlockedBitTestAndSetPointer(ptr,val) InterlockedBitTestAndSet64((PLONGLONG)ptr,(LONGLONG)val)
#define InterlockedAddPointer(ptr,val) InterlockedAdd64((PLONGLONG)ptr,(LONGLONG)val)
#define InterlockedAndPointer(ptr,val) InterlockedAnd64((PLONGLONG)ptr,(LONGLONG)val)
#define InterlockedOrPointer(ptr,val) InterlockedOr64((PLONGLONG)ptr,(LONGLONG)val)
#else
#define InterlockedBitTestAndSetPointer(ptr,val) InterlockedBitTestAndSet((PLONG)ptr,(LONG)val)
#define InterlockedAddPointer(ptr,val) InterlockedAdd((PLONG)ptr,(LONG)val)
#define InterlockedAndPointer(ptr,val) InterlockedAnd((PLONG)ptr,(LONG)val)
#define InterlockedOrPointer(ptr,val) InterlockedOr((PLONG)ptr,(LONG)val)
#endif

#define InterlockedExchangeAdd16 _InterlockedExchangeAdd16 

#define COND_VAR_UNUSED_FLAG         ((ULONG_PTR)1)
#define COND_VAR_LOCKED_FLAG         ((ULONG_PTR)2)
#define COND_VAR_FLAGS_MASK          ((ULONG_PTR)3)
#define COND_VAR_ADDRESS_MASK        (~COND_VAR_FLAGS_MASK)

#define RtlpWaitOnAddressSpinCount 1024

// a spin-count of 1024 takes a lot longer to execute on debug compiles than a spin-count of 256.
static DWORD ConditionVariableSpinCount = 256;
static DWORD SRWLockSpinCount = 256;

typedef SIZE_T SYNCSTATUS;

#define CVF_Full	7	//唤醒申请已满，全部唤醒
#define CVF_Link	8	//修改链表的操作进行中

typedef struct _COND_VAR_WAIT_ENTRY
{
    /* ListEntry must have an alignment of at least 32-bits, since we
       want COND_VAR_ADDRESS_MASK to cover all of the address. */
    LIST_ENTRY ListEntry;
    PVOID WaitKey;
    BOOLEAN ListRemovalHandled;
} COND_VAR_WAIT_ENTRY, * PCOND_VAR_WAIT_ENTRY;

typedef struct _ADDRESS_WAIT_BLOCK
{
	volatile void* Address;
	//因为Windows 8以及更高版本才支持 ZwWaitForAlertByThreadId，所以我们直接把 ThreadId 砍掉了，反正没鸟用
	//ULONG_PTR            ThreadId;

	// 它是后继
	struct _ADDRESS_WAIT_BLOCK* back;
	// 它是前驱
	struct _ADDRESS_WAIT_BLOCK* notify;
	// 似乎指向Root，但是Root时才指向自己，其余情况为 NULL，这是一种安全性？
	struct _ADDRESS_WAIT_BLOCK* next;
	volatile long         flag;
} ADDRESS_WAIT_BLOCK;

#define CONTAINING_COND_VAR_WAIT_ENTRY(address, field) \
    CONTAINING_RECORD(address, COND_VAR_WAIT_ENTRY, field)
	
#define ADDRESS_GET_BLOCK(AW) ((ADDRESS_WAIT_BLOCK*)((SIZE_T)(AW) & (~(SIZE_T)(0x3))))

//BOOL NTAPI RtlpWaitCouldDeadlock();

static 
NTSTATUS 
RtlpWaitOnAddressWithTimeout(
	ADDRESS_WAIT_BLOCK* pWaitBlock, 
	LARGE_INTEGER *TimeOut
);

static 
void 
RtlpWaitOnAddressRemoveWaitBlock(
	ADDRESS_WAIT_BLOCK* pWaitBlock
);

static void RtlpWaitOnAddressRemoveWaitBlock(ADDRESS_WAIT_BLOCK* pWaitBlock);

/* GLOBALS *******************************************************************/

extern HANDLE GlobalKeyedEventHandle;
static HANDLE WaitOnAddressKeyedEventHandle;
static RTL_RUN_ONCE init_once_woa = RTL_RUN_ONCE_INIT; 

VOID
RtlpInitializeKeyedEvent(VOID)
{
    ASSERT(GlobalKeyedEventHandle == NULL);
    NtCreateKeyedEvent(&GlobalKeyedEventHandle, EVENT_ALL_ACCESS, NULL, 0);
}

VOID
RtlpCloseKeyedEvent(VOID)
{
    ASSERT(GlobalKeyedEventHandle != NULL);
    NtClose(GlobalKeyedEventHandle);
    GlobalKeyedEventHandle = NULL;
}

static DWORD NTAPI
RtlpInitializeWaitOnAddressKeyedEvent( RTL_RUN_ONCE *once, void *param, void **context )
{
    NtCreateKeyedEvent(&WaitOnAddressKeyedEventHandle, GENERIC_READ|GENERIC_WRITE, NULL, 0);
	return TRUE; 
}


static LARGE_INTEGER* __fastcall NtFormatTimeOut(LARGE_INTEGER* Timeout, DWORD dwMilliseconds)
{
    if (dwMilliseconds == INFINITE)
        return NULL;

    Timeout->QuadPart = -10000ll * dwMilliseconds;

    return Timeout;
}

static NTSTATUS NTAPI SecondWaitWorkaroundNtWaitForKeyedEvent(
    IN HANDLE               KeyedEventHandle,
    IN YY_CV_WAIT_BLOCK*              Key,
    IN BOOLEAN              Alertable,
    IN PLARGE_INTEGER       Timeout OPTIONAL
)
{
	NTSTATUS Status;
   // 目前只有Windows XP收到报告说会卡死，所以我们这里判断一下
   if (Timeout == NULL)// && NtCurrentTeb()->ProcessEnvironmentBlock->OSMajorVersion < 6)
   {
       LARGE_INTEGER _nTimeOut;
       NtFormatTimeOut(&_nTimeOut, 0);
       for (; Key->uWakeupThreadId == 0;)
       {
           Status = NtWaitForKeyedEvent(KeyedEventHandle, (PVOID)Key, Alertable, &_nTimeOut);
           if (Status != STATUS_TIMEOUT)
              return STATUS_TIMEOUT;
           }

           // 等5毫秒应该足够唤醒线程调用Release了
           // 这里只是经验假设，可能不能彻底规避问题。但是正常情况下应该足以缓解死等问题。
           NtWaitForKeyedEvent(KeyedEventHandle, Key, Alertable, NtFormatTimeOut(&_nTimeOut, 5));
           return STATUS_SUCCESS;
    }
    return NtWaitForKeyedEvent(KeyedEventHandle, Key, Alertable, Timeout);
}

static NTSTATUS NTAPI SecondWaitWorkaroundNtReleaseKeyedEvent(
	IN HANDLE               KeyedEventHandle,
	IN YY_CV_WAIT_BLOCK*             Key,
	IN BOOLEAN              Alertable,
	IN PLARGE_INTEGER       Timeout OPTIONAL
)
{
   InterlockedExchange((volatile long*)&Key->uWakeupThreadId, HandleToUlong(NtCurrentTeb()->ClientId.UniqueThread));
   return NtReleaseKeyedEvent(KeyedEventHandle, Key, Alertable, Timeout);
}


static ULONG_PTR* GetBlockByWaitOnAddressHashTable(LPVOID Address)
{
	static volatile ULONG_PTR WaitOnAddressHashTable[128];

	size_t Index = ((size_t)Address >> 5) & 0x7F;

	return &WaitOnAddressHashTable[Index];
}

VOID InitializeGlobalKeyedEventHandle()
{
	HANDLE KeyedEventHandle;
	//Windows XP等平台则 使用系统自身的 CritSecOutOfMemoryEvent，Vista或者更高平台 我们直接返回 NULL 即可。
	if (GlobalKeyedEventHandle == NULL)
	{
		const wchar_t Name[] = L"\\KernelObjects\\CritSecOutOfMemoryEvent";

		UNICODE_STRING ObjectName = {sizeof(Name) - sizeof(wchar_t),sizeof(Name) - sizeof(wchar_t) ,(PWSTR)Name };
		OBJECT_ATTRIBUTES attr = { sizeof(attr),0,&ObjectName };

		if (!NT_SUCCESS(NtOpenKeyedEvent(&KeyedEventHandle, MAXIMUM_ALLOWED, &attr)))
		{
			RtlRaiseStatus(STATUS_RESOURCE_NOT_OWNED);
			return;
		}

		if (InterlockedCompareExchange((volatile long *)&GlobalKeyedEventHandle, (size_t)KeyedEventHandle, (size_t)0))
		{
			NtClose(KeyedEventHandle);
		}
	}
}

static void RtlpWaitOnAddressWakeEntireList(ADDRESS_WAIT_BLOCK* pBlock)
{
	for (; pBlock;)
	{
		ADDRESS_WAIT_BLOCK* Tmp = pBlock->back;

		if (InterlockedExchange((volatile long *)&pBlock->flag, 2) == 0)
		{
			NtReleaseKeyedEvent(GlobalKeyedEventHandle, pBlock, 0, 0);
		}

		pBlock = Tmp;
	}
}

static void RtlpOptimizeWaitOnAddressWaitList(volatile ULONG_PTR* ppFirstBlock)
{
	ULONG_PTR Current = *ppFirstBlock;
	ADDRESS_WAIT_BLOCK* pBlock;
	ADDRESS_WAIT_BLOCK* pItem;
	ADDRESS_WAIT_BLOCK* Tmp;
	size_t Last;

	for (;;)
	{
		pBlock = ADDRESS_GET_BLOCK(Current);

		for (pItem = pBlock;;)
		{
			if (pItem->next != 0)
			{
				pBlock->next = pItem->next;
				break;
			}

			Tmp = pItem;
			pItem = pItem->back;
			pItem->notify = Tmp;
		}

		Last = InterlockedCompareExchange((volatile long *)ppFirstBlock, (Current & 1) == 0 ? (size_t)(pBlock) : 0, Current);

		if (Last == Current)
		{
			if(Current & 1)
			{
				RtlpWaitOnAddressWakeEntireList(pBlock);
			}

			return;
		}


		Current = Last;
	}
}

static void RtlpAddWaitBlockToWaitList(ADDRESS_WAIT_BLOCK* pWaitBlock)
{
	ULONG_PTR* ppFirstBlock = GetBlockByWaitOnAddressHashTable((LPVOID)pWaitBlock->Address);
	ULONG_PTR Current = *ppFirstBlock;	
	size_t New;
	ADDRESS_WAIT_BLOCK* back;
	size_t Last;

	for (;;)
	{
		New = (size_t)(pWaitBlock) | ((size_t)(Current) & 0x3);

		back = ADDRESS_GET_BLOCK(Current);
	    pWaitBlock->back = back;
		if (back)
		{
			New |= 0x2;

			pWaitBlock->next = 0;
		}
		else
		{
			pWaitBlock->next = pWaitBlock;
		}

		Last = InterlockedCompareExchange((volatile long *)ppFirstBlock, New, Current);

		if (Last == Current)
		{
			//0x2状态发生变化 才需要重新优化锁。
			if ((Current ^ New) & 0x2)
			{
				RtlpOptimizeWaitOnAddressWaitList(ppFirstBlock);
			}

			return;
		}

		Current = Last;
	}
}

/***********************************************************************
 *           RtlWaitOnAddress   (NTDLL.@)
 */

static 
NTSTATUS 
RtlpWaitOnAddressWithTimeout(
	ADDRESS_WAIT_BLOCK* pWaitBlock, 
	LARGE_INTEGER *TimeOut
)
{
	NTSTATUS Status;
	ULONG SpinCount;
	//单核 我们无需自旋，直接进入等待过程即可
	if (NtCurrentTeb()->ProcessEnvironmentBlock->NumberOfProcessors > 1 && RtlpWaitOnAddressSpinCount)
	{
		for (SpinCount = 0; SpinCount < RtlpWaitOnAddressSpinCount;++SpinCount)
		{
			if ((pWaitBlock->flag & 1) == 0)
			{
				//自旋过程中，等到了信号改变
				return STATUS_SUCCESS;
			}

			YieldProcessor();
		}
	}

	if (!_interlockedbittestandreset((volatile long *)&pWaitBlock->flag, 0))
	{
		//本来我是拒绝的，但是运气好，状态已经发生了反转
		return STATUS_SUCCESS;
	}

	Status = NtWaitForKeyedEvent(GlobalKeyedEventHandle, pWaitBlock, 0, TimeOut);

	if (Status == STATUS_TIMEOUT)
	{
		if (InterlockedExchange((volatile long *)&pWaitBlock->flag, 4) == 2)
		{
			Status = NtWaitForKeyedEvent(GlobalKeyedEventHandle, pWaitBlock, 0, 0);
		}
		else
		{
			RtlpWaitOnAddressRemoveWaitBlock(pWaitBlock);
		}
	}

	return Status;
}	
	
static void RtlpWaitOnAddressRemoveWaitBlock(ADDRESS_WAIT_BLOCK* pWaitBlock)
{
	ULONG_PTR* ppFirstBlock = GetBlockByWaitOnAddressHashTable((LPVOID)pWaitBlock->Address);
	ULONG_PTR Current = *ppFirstBlock;
	size_t Last;
	size_t New;
	ADDRESS_WAIT_BLOCK* pBlock;
	ADDRESS_WAIT_BLOCK* pItem;
	ADDRESS_WAIT_BLOCK* pNotify;
	ADDRESS_WAIT_BLOCK* Tmp;
	BOOL bFind;

	for (; Current; Current = Last)
	{
		if (Current & 2)
		{
			Last = InterlockedCompareExchange((volatile long *)ppFirstBlock, Current | 1, Current);

			if (Last == Current)
			{
				break;
			}
		}else{
			New = Current | 0x2;
			Last = InterlockedCompareExchange((volatile long *)ppFirstBlock, New, Current);

			if (Last == Current)
			{
				Current = New;

				bFind = FALSE;

							//同步成功！
				pBlock = ADDRESS_GET_BLOCK(New);
				pItem = pBlock;

				pNotify = pBlock->notify;				

				do
				{
					Tmp = pBlock->back;

					if (pBlock != pWaitBlock)
					{
						pBlock->notify = pNotify;
						pNotify = pBlock;


						pBlock = Tmp;
						Tmp = pItem;
						continue;
					}

					bFind = TRUE;


					if (pBlock != pItem)
					{
						pNotify->back = Tmp;
						if (Tmp)
							Tmp->notify = pNotify;
						else
							pNotify->next = pNotify;

						pBlock = Tmp;
						Tmp = pItem;
						continue;
					}

					New = (size_t)(pBlock->back);
					if (Tmp)
					{
						New = (size_t)(Tmp) ^ (Current ^ (size_t)(Tmp)) & 0x3;
					}

					Last = InterlockedCompareExchange((volatile long *)ppFirstBlock, New, Current);

					if (Last == Current)
					{
						if (New == 0)
							return;

						Tmp->notify = 0;
						pBlock = Tmp;
					}else{
						Current = Last;

						Tmp = pBlock = ADDRESS_GET_BLOCK(Current);
						pNotify = pBlock->notify;
					}


					pItem = Tmp;
				} while (pBlock);
							

				if (bFind == FALSE && InterlockedExchange((volatile long *)&pWaitBlock->flag, 0) != 2)
				{
					NtWaitForKeyedEvent(GlobalKeyedEventHandle, pWaitBlock, 0, 0);
				}

				Tmp->next = pNotify;

				for (;;)
				{
					Last = InterlockedCompareExchange((volatile long *)ppFirstBlock, (Current & 1) == 0 ? (size_t)(ADDRESS_GET_BLOCK(Current)) : 0, Current);

					if (Last == Current)
						break;

					Current = Last;
				}

				if (Current & 1)
					RtlpWaitOnAddressWakeEntireList(ADDRESS_GET_BLOCK(Current));


					return;
			}
		}
	}

	if (InterlockedExchange((volatile long *)&pWaitBlock->flag, 1) == 2)
		return;

	RtlpWaitOnAddressWithTimeout(pWaitBlock, 0);
}  
								  
// NTSTATUS 
// NTAPI
// RtlWaitOnAddress( 
	// const void *Address, 
	// const void *CompareAddress, 
	// SIZE_T AddressSize,
    // const LARGE_INTEGER *TimeOut )	
// {		
	// BOOL bSame;
	// NTSTATUS Status;
	// ADDRESS_WAIT_BLOCK WaitBlock;
			
	// if (AddressSize > 8 || AddressSize == 0 || ((AddressSize - 1) & AddressSize) != 0)
	// {
		// return STATUS_INVALID_PARAMETER;
	// }
	
	// WaitBlock.Address = Address;
	// WaitBlock.back = 0;
	// WaitBlock.notify = 0;
	// WaitBlock.next = 0;
	// WaitBlock.flag = 1;

	// RtlpAddWaitBlockToWaitList(&WaitBlock);
			
	// switch (AddressSize)
	// {
		// case 1:
			// bSame = *(volatile byte*)Address == *(volatile byte*)CompareAddress;
			// break;
		// case 2:
			// bSame = *(volatile WORD*)Address == *(volatile WORD*)CompareAddress;
			// break;
		// case 4:
			// bSame = *(volatile DWORD*)Address == *(volatile DWORD*)CompareAddress;
			// break;
		// default:
			// //case 8:
// #if _WIN64
			// //64位自身能保证操作的原子性
			// bSame = *(volatile unsigned long long*)Address == *(volatile unsigned long long*)CompareAddress;
// #else
			// bSame = InterlockedCompareExchange64((volatile long long*)Address, 0, 0) == *(volatile long long*)CompareAddress;
// #endif
			// break;
	// }

	// if (!bSame)
	// {
		// //结果不相同，我们从等待队列移除
		// RtlpWaitOnAddressRemoveWaitBlock(&WaitBlock);
		// return TRUE;
	// }			

	// Status = RtlpWaitOnAddressWithTimeout(&WaitBlock, TimeOut);

	// return Status;	
// }

// /***********************************************************************
 // *           RtlWakeAddressAll    (NTDLL.@)
 // */
// void NTAPI RtlWakeAddressAll( const void *addr )
// {
    // LARGE_INTEGER now;

    // RtlRunOnceExecuteOnce( &init_once_woa, RtlpInitializeWaitOnAddressKeyedEvent, NULL, NULL );
    // NtQuerySystemTime( &now );
    // while (NtReleaseKeyedEvent( GlobalKeyedEventHandle, addr, 0, &now ) == STATUS_SUCCESS) {}
// }

// /***********************************************************************
 // *           RtlWakeAddressSingle (NTDLL.@)
 // */
// void NTAPI RtlWakeAddressSingle( const void *addr )
// {
    // LARGE_INTEGER now;

    // RtlRunOnceExecuteOnce( &init_once_woa, RtlpInitializeWaitOnAddressKeyedEvent, NULL, NULL );
    // NtQuerySystemTime( &now );
    // NtReleaseKeyedEvent( GlobalKeyedEventHandle, addr, 0, &now );
// }  

/******************************************************************
 *              RtlRunOnceBeginInitialize (NTDLL.@)
 */
DWORD 
NTAPI 
RtlRunOnceBeginInitialize( 
	RTL_RUN_ONCE *once, 
	ULONG flags, 
	void **context 
)
{
    if (flags & RTL_RUN_ONCE_CHECK_ONLY)
    {
        ULONG_PTR val = (ULONG_PTR)once->Ptr;

        if (flags & RTL_RUN_ONCE_ASYNC) return STATUS_INVALID_PARAMETER;
        if ((val & 3) != 2) return STATUS_UNSUCCESSFUL;
        if (context) *context = (void *)(val & ~3);
        return STATUS_SUCCESS;
    } 

    for (;;)
    {
        ULONG_PTR next, val = (ULONG_PTR)once->Ptr;

        switch (val & 3)
        {
        case 0:  /* first time */
            if (!interlocked_cmpxchg_ptr( &once->Ptr,
                                          (flags & RTL_RUN_ONCE_ASYNC) ? (void *)3 : (void *)1, 0 ))
                return STATUS_PENDING;
            break;

        case 1:  /* in progress, wait */			
            if (flags & RTL_RUN_ONCE_ASYNC) return STATUS_INVALID_PARAMETER;
            next = val & ~3;
            if (interlocked_cmpxchg_ptr( &once->Ptr, (void *)((ULONG_PTR)&next | 1),
                                         (void *)val ) == (void *)val)
                NtWaitForKeyedEvent( GlobalKeyedEventHandle, &next, FALSE, NULL );
            break;

        case 2:  /* done */
            if (context) *context = (void *)(val & ~3);
            return STATUS_SUCCESS;

        case 3:  /* in progress, async */
            if (!(flags & RTL_RUN_ONCE_ASYNC)) return STATUS_INVALID_PARAMETER;
            return STATUS_PENDING;
        }
    }
}

/*
 * @implemented - need test
 */
/******************************************************************
 *              RtlRunOnceComplete (NTDLL.@)
 */
DWORD
NTAPI
RtlRunOnceComplete( 
	PRTL_RUN_ONCE once, 
	ULONG flags, 
	PVOID context 
)
{
    if ((ULONG_PTR)context & 3) return STATUS_INVALID_PARAMETER;

    if (flags & RTL_RUN_ONCE_INIT_FAILED)
    {
        if (context) return STATUS_INVALID_PARAMETER;
        if (flags & RTL_RUN_ONCE_ASYNC) return STATUS_INVALID_PARAMETER;
    }
    else context = (void *)((ULONG_PTR)context | 2);
	
    for (;;)
    {
        ULONG_PTR val = (ULONG_PTR)once->Ptr;

        switch (val & 3)
        {
        case 1:  /* in progress */
            if (interlocked_cmpxchg_ptr( &once->Ptr, context, (void *)val ) != (void *)val) break;
            val &= ~3;
            while (val)
            {
                ULONG_PTR next = *(ULONG_PTR *)val;
                NtReleaseKeyedEvent( GlobalKeyedEventHandle, (void *)val, FALSE, NULL );
                val = next;
            }
            return STATUS_SUCCESS;

        case 3:  /* in progress, async */
            if (!(flags & RTL_RUN_ONCE_ASYNC)) return STATUS_INVALID_PARAMETER;
            if (interlocked_cmpxchg_ptr( &once->Ptr, context, (void *)val ) != (void *)val) break;
            return STATUS_SUCCESS;

        default:
            return STATUS_UNSUCCESSFUL;
        }
    }
}

/******************************************************************
  *              RtlRunOnceExecuteOnce (NTDLL.@)
  */
DWORD 
NTAPI 
RtlRunOnceExecuteOnce( 
	RTL_RUN_ONCE *once, 
	PRTL_RUN_ONCE_INIT_FN func,
    void *param, void **context 
)
{
     DWORD ret = RtlRunOnceBeginInitialize( once, 0, context );
 
     if (ret != STATUS_PENDING) return ret;
 
     if (!func( once, param, context ))
     {
         RtlRunOnceComplete( once, RTL_RUN_ONCE_INIT_FAILED, NULL );
         return STATUS_UNSUCCESSFUL;
     } 
     return RtlRunOnceComplete( once, 0, context ? *context : NULL );
}

/******************************************************************
 *              RtlRunOnceInitialize (NTDLL.@)
 */
void 
NTAPI 
RtlRunOnceInitialize( 
	RTL_RUN_ONCE *once 
)
{
    once->Ptr = NULL;
}

// BOOL NTAPI RtlpWaitCouldDeadlock()
// {
	// //byte_77F978A8极有可能是LdrpShutdownInProgress
	// //进程退出时，各种资源即将被销毁，继续等待将会出现错误的结果
	// return RtlDllShutdownInProgress()!=0;
// }

//New ConditionVariable API
void NTAPI RtlpInitConditionVariable(PEB* pPeb)
{
	
	if (pPeb->NumberOfProcessors == 1) { // on singleprocessor, spincount is meaningless
        ConditionVariableSpinCount = 0;
        SRWLockSpinCount = 0;
	}
}
/***********************************************************************
 *           RtlInitializeConditionVariable   (NTDLL.@)
 *
 * Initializes the condition variable with NULL.
 *
 * PARAMS
 *  variable [O] condition variable
 *
 * RETURNS
 *  Nothing.
 */
void NTAPI RtlInitializeConditionVariable( RTL_CONDITION_VARIABLE *variable )
{
    variable->Ptr = NULL;
}

//将等待块插入 SRWLock 中
static BOOL __fastcall RtlpQueueWaitBlockToSRWLock(YY_CV_WAIT_BLOCK* pBolck, RTL_SRWLOCK *SRWLock, ULONG SRWLockMark)
{
	size_t shareCount;
	size_t Current;
	size_t New;
	USHORT backoff = 0;
				
	for (;;)
	{
		Current = *(volatile size_t*)SRWLock;

		if ((Current & 0x1) == 0)
			break;


		if (SRWLockMark == 0)
		{
			pBolck->flag |= 0x1;
		}
		else if ((Current & 0x2) == 0 && YY_SRWLOCK_GET_BLOCK(Current))
		{
			return FALSE;
		}

		pBolck->next = NULL;					

		if (Current & 0x2)
		{
			pBolck->notify = NULL;
			pBolck->shareCount = 0;

			//_YY_CV_WAIT_BLOCK 结构体跟 _YY_SRWLOCK_WAIT_BLOCK兼容，所以能这样强转
			pBolck->back = (YY_CV_WAIT_BLOCK*)YY_SRWLOCK_GET_BLOCK(Current);

			New = (size_t)(pBolck) | (Current & YY_CV_MASK);
		}
		else
		{
			shareCount = Current >> 4;

			pBolck->shareCount = shareCount;
			pBolck->notify = pBolck;
			New = shareCount <= 1 ? (size_t)(pBolck) | 0x3 : (size_t)(pBolck) | 0xB;
		}

		//清泠 发现的Bug，我们应该返回 TRUE，减少必要的内核等待。
		if ((size_t)InterlockedCompareExchangePointer((void *volatile *)SRWLock, (volatile long*)New, (volatile long*)Current) == Current)
			return TRUE;

		RtlBackoff(&backoff);
		//YieldProcessor();
	}

	return FALSE;
}

static void __fastcall RtlpWakeConditionVariable(RTL_CONDITION_VARIABLE *ConditionVariable, size_t ConditionVariableStatus, size_t WakeCount)
{
	//v16
	YY_CV_WAIT_BLOCK* notify = NULL;
	YY_CV_WAIT_BLOCK* pWake = NULL;
	YY_CV_WAIT_BLOCK* pWaitBlock;
	YY_CV_WAIT_BLOCK* pBlock;
	YY_CV_WAIT_BLOCK* tmp;
	YY_CV_WAIT_BLOCK* next;
	YY_CV_WAIT_BLOCK* back;
	YY_CV_WAIT_BLOCK** ppInsert = &pWake;
	size_t LastStatus;
	size_t MaxWakeCount;
	size_t Count = 0;

	for (;;)
	{
		pWaitBlock = YY_CV_GET_BLOCK(ConditionVariableStatus);

		if ((ConditionVariableStatus & 0x7) == 0x7)
		{
			ConditionVariableStatus = (size_t)InterlockedExchangePointer((void *volatile *)ConditionVariable, 0);

			*ppInsert = YY_CV_GET_BLOCK(ConditionVariableStatus);

			break;
		}

		MaxWakeCount = WakeCount + (ConditionVariableStatus & 7);

		pBlock = pWaitBlock;

		for (; pBlock->notify == NULL;)
		{
			tmp = pBlock;
			pBlock = pBlock->back;
			pBlock->next = tmp;
		}

		if (MaxWakeCount <= Count)
		{
			LastStatus = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)(pWaitBlock), (volatile long*)ConditionVariableStatus);

			if (LastStatus == ConditionVariableStatus)
			{
				break;
			}

			ConditionVariableStatus = LastStatus;
		}
		else
		{
			notify = pBlock->notify;

			for (; MaxWakeCount > Count && notify->next;)
			{
				++Count;
				*ppInsert = notify;
				notify->back = NULL;

				next = notify->next;

				pWaitBlock->notify = next;
				next->back = NULL;

				ppInsert = &notify->back;

				notify = next;

			}

			if (MaxWakeCount <= Count)
			{
				LastStatus = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)(pWaitBlock), (volatile long*)ConditionVariableStatus);

				if (LastStatus == ConditionVariableStatus)
				{
					break;
				}

				ConditionVariableStatus = LastStatus;
			}
			else
			{
				LastStatus = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)0, (volatile long*)ConditionVariableStatus);


				if (LastStatus == ConditionVariableStatus)
				{
					*ppInsert = notify;
					notify->back = 0;

					break;
				}

				ConditionVariableStatus = LastStatus;
			}
		}
	}

	for (; pWake;)
	{
		back = pWake->back;

		if (!InterlockedBitTestAndReset((volatile LONG*)&pWake->flag, 1))
		{
			if (pWake->SRWLock == NULL || RtlpQueueWaitBlockToSRWLock(pWake, pWake->SRWLock, (pWake->flag >> 2) & 0x1) == FALSE)
			{
				SecondWaitWorkaroundNtReleaseKeyedEvent(GlobalKeyedEventHandle, pWake, 0, NULL);
			}
		}

		pWake = back;
	}

	return;
}

VOID
NTAPI
RtlWakeConditionVariable(
	_Inout_ RTL_CONDITION_VARIABLE *ConditionVariable
)
{
	size_t Current;
	size_t Last;

	Current = *(volatile size_t*)ConditionVariable;

	for (; Current; Current = Last)
	{
		if (Current & 0x8)
		{
			if ((Current & 0x7) == 0x7)
			{
				return;
			}

			Last = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)(Current + 1), (volatile long*)Current);
			if (Last == Current)
				return;
		}
		else
		{
			Last = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)(Current | 0x8), (volatile long*)Current);
			if (Last == Current)
			{
				RtlpWakeConditionVariable(ConditionVariable, Current + 8, 1);
				return;
			}
		}
	}
}
		
VOID
NTAPI
RtlWakeAllConditionVariable(
	_Inout_ RTL_CONDITION_VARIABLE *ConditionVariable
)
{
	size_t Current = (size_t)ConditionVariable;
	size_t Last;
	YY_CV_WAIT_BLOCK* pBlock;
	YY_CV_WAIT_BLOCK* Tmp;

	for (; Current && (Current & 0x7) != 0x7; Current = Last)
	{
		if (Current & 0x8)
		{
			Last = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)(Current | 0x7), (volatile long*)Current);
			if (Last == Current)
				return;
		}
		else
		{
			Last = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)0, (volatile long*)Current);
			if (Last == Current)
			{

				for (pBlock = YY_CV_GET_BLOCK(Current); pBlock;)
				{
					Tmp = pBlock->back;

					if (!InterlockedBitTestAndReset((volatile LONG*)&pBlock->flag, 1))
					{
						SecondWaitWorkaroundNtReleaseKeyedEvent(GlobalKeyedEventHandle, pBlock, FALSE, NULL);
					}

					pBlock = Tmp;
				}

				return;
			}
		}
	}
}
		
static void __fastcall RtlpOptimizeConditionVariableWaitList(RTL_CONDITION_VARIABLE *ConditionVariable, size_t ConditionVariableStatus)
{
    YY_CV_WAIT_BLOCK *pWaitBlock;
    YY_CV_WAIT_BLOCK *pItem;
    YY_CV_WAIT_BLOCK *temp;
    size_t LastStatus;
                
    for (;;)
    {
        pWaitBlock = YY_CV_GET_BLOCK(ConditionVariableStatus);
        pItem = pWaitBlock;

        for (; pItem->notify == NULL;)
        {
            temp = pItem;
            pItem = pItem->back;
            pItem->next = temp;
        }

        pWaitBlock->notify = pItem->notify;

        LastStatus = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)pWaitBlock, (volatile long*)ConditionVariableStatus);

        if (LastStatus == ConditionVariableStatus)
            return;

        if (LastStatus & 7)
        {
            RtlpWakeConditionVariable(ConditionVariable, LastStatus, 0);
            return;
        }
        
        ConditionVariableStatus = LastStatus;
    }
}

static BOOL __fastcall RtlpWakeSingle(RTL_CONDITION_VARIABLE *ConditionVariable, YY_CV_WAIT_BLOCK* pBlock)
{
	size_t Current = (size_t)ConditionVariable;
	YY_CV_WAIT_BLOCK *pWaitBlock;
	YY_CV_WAIT_BLOCK *pSuccessor;
	size_t Last;
	size_t New;
	size_t back;
	YY_CV_WAIT_BLOCK* notify;
	BOOL bRet;				

	for (; Current && (Current & 0x7) != 0x7;)
	{
		if (Current & 0x8)
		{
			Last = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)(Current | 0x7), (volatile long*)Current);

			if (Last == Current)
				return FALSE;

			Current = Last;
		}
		else
		{
			New = Current | 0x8;

			Last = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)New, (volatile long*)Current);

			if (Last == Current)
			{
				Current = New;

				notify = NULL;
				bRet = FALSE;

				pWaitBlock = YY_CV_GET_BLOCK(Current);
				pSuccessor = pWaitBlock;

				if (pWaitBlock)
				{
					for (; pWaitBlock;)
					{
						if (pWaitBlock == pBlock)
						{
							if (notify)
							{
								pWaitBlock = pWaitBlock->back;
								bRet = TRUE;

								notify->back = pWaitBlock;

								if (!pWaitBlock)
									break;

								pWaitBlock->next = notify;
							}
							else
							{
								back = (size_t)(pWaitBlock->back);

								New = back == 0 ? back : back ^ ((New ^ back) & 0xF);

								Last = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)New, (volatile long*)Current);

								if (Last == Current)
								{
									Current = New;
									if (back == 0)
										return TRUE;

									bRet = TRUE;
								}
								else
								{
									Current = Last;
								}

								pSuccessor = pWaitBlock = YY_CV_GET_BLOCK(Current);
								notify = NULL;
							}
						}
						else
						{
							pWaitBlock->next = notify;
							notify = pWaitBlock;
							pWaitBlock = pWaitBlock->back;
						}
					}

					if (pSuccessor)
						pSuccessor->notify = notify;
				}

				RtlpWakeConditionVariable(ConditionVariable, Current, 0);
				return bRet;
			}

			Current = Last;
		}
	}

	return FALSE;
}			

NTSTATUS
NTAPI
RtlSleepConditionVariableCS(
	_Inout_ RTL_CONDITION_VARIABLE *ConditionVariable,
	_Inout_ PRTL_CRITICAL_SECTION   CriticalSection,
	_In_    const LARGE_INTEGER *   dwMilliseconds
)
{
	YY_CV_WAIT_BLOCK StackWaitBlock;
	size_t OldConditionVariable;
	size_t NewConditionVariable;
	size_t LastConditionVariable;
	size_t SpinCount;
	NTSTATUS Status = STATUS_SUCCESS;

	StackWaitBlock.next = NULL;
	StackWaitBlock.flag = 2;
	StackWaitBlock.SRWLock = NULL;
	StackWaitBlock.uWakeupThreadId = 0;
	OldConditionVariable = *(size_t*)ConditionVariable;			

	for (;;)
	{
		NewConditionVariable = (size_t)(&StackWaitBlock) | (OldConditionVariable & YY_CV_MASK);
		StackWaitBlock.back = YY_CV_GET_BLOCK(OldConditionVariable);

		if (StackWaitBlock.back)
		{
			StackWaitBlock.notify = NULL;

			NewConditionVariable |= 0x8;
		}
		else
		{
			StackWaitBlock.notify = &StackWaitBlock;
		}

		LastConditionVariable = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)NewConditionVariable, (volatile long*)OldConditionVariable);

		if (LastConditionVariable == OldConditionVariable)
			break;

		OldConditionVariable = LastConditionVariable;
	}

	RtlLeaveCriticalSection(CriticalSection);

	//0x8 标记新增时，才进行优化 ConditionVariableWaitList
	if ((OldConditionVariable ^ NewConditionVariable) & 0x8)
	{
		RtlpOptimizeConditionVariableWaitList(ConditionVariable, NewConditionVariable);
	}
	
	InitializeGlobalKeyedEventHandle();

	//自旋
	for (SpinCount = ConditionVariableSpinCount; SpinCount; --SpinCount)
	{
		if (!(StackWaitBlock.flag & 2))
			break;

		YieldProcessor();
	}			

	if (InterlockedBitTestAndReset((volatile LONG*)&StackWaitBlock.flag, 1))
	{	
		Status = NtWaitForKeyedEvent(GlobalKeyedEventHandle, (PVOID)&StackWaitBlock, 0, dwMilliseconds);

		if (Status == STATUS_TIMEOUT && RtlpWakeSingle(ConditionVariable, &StackWaitBlock) == FALSE)
		{
			SecondWaitWorkaroundNtWaitForKeyedEvent(GlobalKeyedEventHandle, &StackWaitBlock, 0, NULL);
			//NtWaitForKeyedEvent(GlobalKeyedEventHandle, (PVOID)&StackWaitBlock, 0, NULL);
			Status = STATUS_SUCCESS;
		}
	}

	RtlEnterCriticalSection(CriticalSection);

	return Status;
}	


NTSTATUS
NTAPI
RtlSleepConditionVariableSRW(
	_Inout_ RTL_CONDITION_VARIABLE *ConditionVariable,
	_Inout_ RTL_SRWLOCK *SRWLock,
	_In_ PLARGE_INTEGER dwMilliseconds,
	_In_ ULONG Flags
)
{
	size_t SpinCount;
	YY_CV_WAIT_BLOCK StackWaitBlock;			
	size_t Current;
	size_t New;
	size_t Last;
	NTSTATUS Status = STATUS_SUCCESS;
	
	do{
		if (Flags & ~RTL_CONDITION_VARIABLE_LOCKMODE_SHARED)
		{
			break;
		}		

		StackWaitBlock.next = NULL;
		StackWaitBlock.flag = 2;
		StackWaitBlock.SRWLock = NULL;
		StackWaitBlock.uWakeupThreadId = 0;

		if (Flags& RTL_CONDITION_VARIABLE_LOCKMODE_SHARED)
		{
			StackWaitBlock.flag |= 0x4;
		}

		Current = *(volatile size_t*)ConditionVariable;

		for (;;)
		{
			New = (size_t)(&StackWaitBlock) | (Current & YY_CV_MASK);

			if (StackWaitBlock.back = YY_CV_GET_BLOCK(Current))
			{
				StackWaitBlock.notify = NULL;

				New |= 0x8;
			}
			else
			{
				StackWaitBlock.notify = &StackWaitBlock;
			}

			Last = (size_t)InterlockedCompareExchangePointer((void *volatile *)ConditionVariable, (volatile long*)New, (volatile long*)Current);

			if (Last == Current)
			{
				break;
			}

			Current = Last;
		}

		if (Flags& RTL_CONDITION_VARIABLE_LOCKMODE_SHARED)
			RtlReleaseSRWLockShared(SRWLock);
		else
			RtlReleaseSRWLockExclusive(SRWLock);

		if ((Current ^ New) & 0x8)
		{
			//新增0x8 标记位才调用 RtlpOptimizeConditionVariableWaitList
			RtlpOptimizeConditionVariableWaitList(ConditionVariable, New);
		}
		
		InitializeGlobalKeyedEventHandle();

		//自旋
		for (SpinCount = ConditionVariableSpinCount; SpinCount; --SpinCount)
		{
			if (!(StackWaitBlock.flag & 2))
				break;

			YieldProcessor();
		}

		if (InterlockedBitTestAndReset((volatile LONG*)&StackWaitBlock.flag, 1))
		{
			Status = NtWaitForKeyedEvent(GlobalKeyedEventHandle, (PVOID)&StackWaitBlock, 0, dwMilliseconds);

			if (Status == STATUS_TIMEOUT && RtlpWakeSingle(ConditionVariable, &StackWaitBlock) == FALSE)
			{
				SecondWaitWorkaroundNtWaitForKeyedEvent(GlobalKeyedEventHandle, (PVOID)&StackWaitBlock, 0, NULL);
				Status = STATUS_SUCCESS;
			}
		}

		if (Flags& RTL_CONDITION_VARIABLE_LOCKMODE_SHARED)
			RtlAcquireSRWLockShared(SRWLock);
		else
			RtlAcquireSRWLockExclusive(SRWLock);
	} while (FALSE);

	return Status;
}

//通过延时来暂时退避竞争
void
NTAPI
RtlBackoff(USHORT* const pBackoff)
{
    USHORT Backoff = *pBackoff;
	USHORT i;
	
    if (Backoff != 0)
    {
        if (Backoff + 1 < (1u << 13))
            Backoff *= 2;
    }
    else
    {
        /* No need to do back-off on UP */
        if (IsUniprocessorMachine)
            return;

        Backoff = 64;
    }

    *pBackoff = Backoff;

    Backoff += (Backoff - 1) & __rdtsc();

    for (i = 0; i < Backoff; ++i)
        YieldProcessor();
}


typedef struct _KEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK *PKEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK;

typedef struct _KEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK {
	//
	// The address that the thread is waiting on.
	//
	PVOID								Address;

	//
	// The event handle upon which this thread is waiting.
	//
	HANDLE								EventHandle;

	//
	// Links to the next and previous RTL_WAIT_ON_ADDRESS_WAIT_BLOCK structure
	// in the linked list.
	//
	PKEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK Next;
	PKEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK Previous;
} KEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK;

typedef struct _KEX_RTL_WAIT_ON_ADDRESS_HASH_BUCKET {
	//
	// Locks the hash bucket. Threads calling WoA or one of the wake functions
	// for a particular address (range) will be blocked until all pending linked
	// list operations are complete.
	//
	RTL_SRWLOCK							Lock;

	//
	// This item will be NULL if no threads are waiting on the addresses that
	// fall under this hash bucket.
	//
	PKEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK	WaitBlocks;
} KEX_RTL_WAIT_ON_ADDRESS_HASH_BUCKET, *PKEX_RTL_WAIT_ON_ADDRESS_HASH_BUCKET;

//
// 128 entries is what's used in Windows 8.
// Perhaps it's a little excessive.
// The size of this array can be freely adjusted here. The code that uses it
// will automatically adapt to the changed size.
// If you change it, it must remain a power of two. Otherwise, the code that
// hashes addresses will become larger and slower by more than a factor of 2.
// demo: https://godbolt.org/z/K9q9KheYj
//
#define KexRtlWoaHashEntries 128 // Must be a power of two.
static KEX_RTL_WAIT_ON_ADDRESS_HASH_BUCKET KexRtlWaitOnAddressHashTable[KexRtlWoaHashEntries] = {0};

#pragma warning(disable:4715) // not all control paths return a value
static inline BOOLEAN KexRtlpEqualVolatileMemory(
	IN	volatile VOID	*Address1,
	IN	PVOID			Address2,
	IN	SIZE_T			Size)
{
	switch (Size) {
	case 1:		return (*(PUCHAR) Address1 == *(PUCHAR) Address2);
	case 2:		return (*(PUSHORT) Address1 == *(PUSHORT) Address2);
	case 4:		return (*(PULONG) Address1 == *(PULONG) Address2);
	case 8:		return (*(PULONGLONG) Address1 == *(PULONGLONG) Address2);
	default:	return FALSE;
	}
}
#pragma warning(default:4715)

static FORCEINLINE PKEX_RTL_WAIT_ON_ADDRESS_HASH_BUCKET KexRtlpGetWoaHashBucket(
	IN	volatile VOID	*Address)
{
    return &KexRtlWaitOnAddressHashTable[
        (((ULONG_PTR) Address) >> 4) & (KexRtlWoaHashEntries - 1)]; 
// Improved from VxKex by using & operation instead of % operation, improving performance for debug builds and likely ancient compilers.
}

//
// This function must be called while the hash bucket is locked.
//
static inline VOID KexRtlpRemoveWoaWaitBlock(
	IN	PKEX_RTL_WAIT_ON_ADDRESS_HASH_BUCKET	HashBucket,
	IN	PKEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK		WaitBlock)
{
	if (WaitBlock->Previous == NULL) {
		// this is a signal that we do NOT touch anything
		return;
	} else if (WaitBlock->Next == WaitBlock) {
		// this wait block is the only entry in the list
		HashBucket->WaitBlocks = NULL;
	} else {
		// there's more than one wait block in the list so we have to
		// remove it "properly"

		if (WaitBlock == HashBucket->WaitBlocks) {
			// this wait block is at the beginning so we have to update
			// the pointer in the hash bucket
			HashBucket->WaitBlocks = WaitBlock->Next;
		}

		WaitBlock->Previous->Next = WaitBlock->Next;
		WaitBlock->Next->Previous = WaitBlock->Previous;
	}
}

#  define PopulationCount16 __popcnt16
#  define PopulationCount __popcnt
#  define PopulationCount64 __popcnt64

//
// This function is the implementation of the WaitOnAddress extended API.
// See WaitOnAddress in KxBase\synch.c and the MSDN docs.
//
NTSTATUS NTAPI RtlWaitOnAddress(
	IN	volatile VOID	*Address,
	IN	PVOID			CompareAddress,
	IN	SIZE_T			AddressSize,
	IN	const LARGE_INTEGER*	Timeout OPTIONAL)
{
	NTSTATUS Status;
	PKEX_RTL_WAIT_ON_ADDRESS_HASH_BUCKET HashBucket;
	KEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK WaitBlock;

	//ASSERT (PopulationCount(ARRAYSIZE(KexRtlWaitOnAddressHashTable)) == 1);
	// ASSERT (Address != NULL);
	// ASSERT (CompareAddress != NULL);
	
	if (Address == NULL || CompareAddress == NULL)
	    return STATUS_INVALID_PARAMETER;

	// ASSERT (AddressSize == 1 || AddressSize == 2 ||
			// AddressSize == 4 || AddressSize == 8);

    if (AddressSize != 4 && AddressSize != 2 &&
        AddressSize != 1 && AddressSize != 8) {

        return STATUS_INVALID_PARAMETER;
    }

	//
	// Figure out which hash bucket we belong in.
	//

	HashBucket = KexRtlpGetWoaHashBucket(Address);

	RtlAcquireSRWLockExclusive(&HashBucket->Lock);

	//
	// Check that the values at *Address and *CompareAddress are the same
	// before continuing.
	//

	if (!KexRtlpEqualVolatileMemory(Address, CompareAddress, AddressSize)) {
		// Values are different, so we can return straight away.
		RtlReleaseSRWLockExclusive(&HashBucket->Lock);
		return STATUS_SUCCESS;
	}

	//
	// The values are different.
	// Create the event upon which we will wait.
	//

	Status = NtCreateEvent(
		&WaitBlock.EventHandle,
		SYNCHRONIZE | EVENT_MODIFY_STATE,
		NULL,
		NotificationEvent,
		FALSE);

	//ASSERT (NT_SUCCESS(Status));

	if (!NT_SUCCESS(Status)) {
		RtlReleaseSRWLockExclusive(&HashBucket->Lock);
		return Status;
	}

	//
	// Add ourselves into the linked list.
	//

	WaitBlock.Address = (PVOID) Address;

	if (HashBucket->WaitBlocks == NULL) {
		// No existing wait blocks.
		WaitBlock.Previous = &WaitBlock;
		WaitBlock.Next = &WaitBlock;
		HashBucket->WaitBlocks = &WaitBlock;
	} else {
		// One or more wait blocks already exist.
		// Add ourselves to the end of the list.
		WaitBlock.Previous = HashBucket->WaitBlocks->Previous;
		WaitBlock.Next = HashBucket->WaitBlocks;
		HashBucket->WaitBlocks->Previous->Next = &WaitBlock;
		HashBucket->WaitBlocks->Previous = &WaitBlock;
	}

	//
	// Wait.
	//

	RtlReleaseSRWLockExclusive(&HashBucket->Lock);

	Status = NtWaitForSingleObject(
		WaitBlock.EventHandle,
		FALSE,
		Timeout);

	//ASSERT (NT_SUCCESS(Status));

	//
	// The thread that woke us up is in charge of removing us from the
	// list. However, if we timed out, there is no such thread, so if the
	// status from NtWaitForSingleObject is STATUS_TIMEOUT or some other
	// error code, we have to do that ourselves.
	//

	if (Status == STATUS_TIMEOUT || !NT_SUCCESS(Status)) {
		RtlAcquireSRWLockExclusive(&HashBucket->Lock);
		KexRtlpRemoveWoaWaitBlock(HashBucket, &WaitBlock);
		RtlReleaseSRWLockExclusive(&HashBucket->Lock);
	}

	NtClose(WaitBlock.EventHandle);
	return Status;
}

//
// This function is the implementation of the WakeByAddressSingle and
// WakeByAddressAll extended APIs.
// See KexRtlWakeByAddressSingle, KexRtlWakeByAddressAll, and the MSDN
// docs.
//
static VOID RtlpWakeByAddress(
	IN	PVOID			Address,
	IN	BOOLEAN			WakeAll)
{
	PKEX_RTL_WAIT_ON_ADDRESS_HASH_BUCKET HashBucket;
	PKEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK WaitBlock;

	HashBucket = KexRtlpGetWoaHashBucket(Address);

	RtlAcquireSRWLockExclusive(&HashBucket->Lock);

	if (HashBucket->WaitBlocks == NULL) {
		RtlReleaseSRWLockExclusive(&HashBucket->Lock);
		return;
	}

	//
	// Traverse the list starting from the beginning.
	// The API documentation from MS states that threads are woken starting
	// from the one that first started waiting.
	//

	WaitBlock = HashBucket->WaitBlocks;

	while (TRUE) {
		PKEX_RTL_WAIT_ON_ADDRESS_WAIT_BLOCK NextWaitBlock;

		NextWaitBlock = WaitBlock->Next;

		if (WaitBlock->Address == Address) {
			NTSTATUS Status;

			KexRtlpRemoveWoaWaitBlock(HashBucket, WaitBlock);

			//
			// Signal to future invocations of KexRtlpRemoveWoaWaitBlock to tell it
			// that we've already removed this block from the list.
			// We need to do this due to the possibility of the following situation:
			//   1. A thread calls WaitOnAddress with a timeout.
			//   2. NtWaitForSingleObject returns with STATUS_TIMEOUT.
			//   3. In between the STATUS_TIMEOUT return and when that thread removes
			//      itself from the list, someone calls WakeByAddress and we get to
			//      this point in the code.
			//   4. We remove the list entry from the list.
			//   5. The other thread that timed out removes the list entry from the
			//      list a 2nd time and potentially causes corruption.
			//
			// It's a rare edge case but the cost to eliminate it is luckily very small.
			//

			WaitBlock->Previous = NULL;
			
			//
			// Wake up the thread.
			//

			Status = NtSetEvent(WaitBlock->EventHandle, NULL);
			//ASSERT (NT_SUCCESS(Status));

			//
			// After the call to NtSetEvent, the contents of WaitBlock should be
			// considered undefined, since when the KexRtlWaitOnAddress call returns
			// the contents of the stack are no longer defined.
			//

			if (!WakeAll) {
				// we only want to wake this one
				break;
			}
		}

		if (HashBucket->WaitBlocks == NULL || NextWaitBlock == HashBucket->WaitBlocks) {
			break;
		}

		WaitBlock = NextWaitBlock;
	}

	RtlReleaseSRWLockExclusive(&HashBucket->Lock);
}

VOID NTAPI RtlWakeAddressSingle(
	IN	PVOID			Address)
{
	RtlpWakeByAddress(Address, FALSE);
}

VOID NTAPI RtlWakeAddressAll(
	IN	PVOID			Address)
{
	RtlpWakeByAddress(Address, TRUE);
}


//* Pointer-sized interlocked helpers for user mode. */
#ifndef InterlockedAndPointer
 #if defined(_WIN64)
  #define InterlockedAndPointer(ptr, val) (PVOID)InterlockedAnd64((volatile LONG64*)(ptr), (LONG64)(val))
 #else
  #define InterlockedAndPointer(ptr, val) (PVOID)InterlockedAnd((volatile LONG*)(ptr), (LONG)(val))
 #endif
#endif

#ifndef InterlockedExchangeAddPointer
 #if defined(_WIN64)
  #define InterlockedExchangeAddPointer(ptr, val) (PVOID)InterlockedExchangeAdd64((volatile LONG64*)(ptr), (LONG64)(val))
 #else
  #define InterlockedExchangeAddPointer(ptr, val) (PVOID)InterlockedExchangeAdd((volatile LONG*)(ptr), (LONG)(val))
 #endif
#endif

/* Bit layout for internal SRW state and wait list flags. */
#if defined(_WIN64)
 #define SRWM_ITEM  0xFFFFFFFFFFFFFFF0ULL
#else
 #define SRWM_ITEM  0xFFFFFFF0U
#endif
#define SRWM_FLAG  (~SRWM_ITEM)

#define SRW_COUNT_BIT   4
#define SRW_HOLD_BIT    0
#define NODE_SPIN_BIT   1

#define NODEF_EXCL      0x01
#define NODEF_SPIN      0x02

typedef struct _SRW_WAIT_NODE
{
    struct _SRW_WAIT_NODE* prev;
    struct _SRW_WAIT_NODE* head;
    struct _SRW_WAIT_NODE* next;
    ULONG shareSnapshot;    /* shared count snapshot */
    ULONG flags;            /* NODEF_EXCL | NODEF_SPIN */
    PRTL_SRWLOCK lock;      /* reserved */
} SRW_WAIT_NODE;

/* SRW internal state is encoded in the pointer-sized value of SRWLock->Ptr. */
typedef ULONG_PTR SRW_STATE;

/* Safely set bit 0 of a pointer-sized value; returns previous bit value (0/1). */
__forceinline LONG RtlpInterlockedBit0SetPointer(volatile PVOID* Target)
{
#if defined(_M_X64)
    return _interlockedbittestandset64((volatile LONG64*)Target, 0);
#elif defined(_M_IX86)
    return _interlockedbittestandset((volatile LONG*)Target, 0);
#else
    PVOID OldValue = *Target;
    for (;;)
    {
        ULONG_PTR OldBits = (ULONG_PTR)OldValue;
        if (OldBits & SRWF_Hold) return 1; /* bit was already set */
		
        PVOID NewValue = (PVOID)(OldBits | SRWF_Hold);
        PVOID Prev = InterlockedCompareExchangePointer((PVOID*)Target, NewValue, OldValue);
		
        if (Prev == OldValue) return 0;     /* successfully set from 0 to 1 */
		
        OldValue = Prev;
    }
#endif
}

static VOID NTAPI RtlpBackoffExp(ULONG* pCount)
{
    ULONG n = *pCount;
    if (n == 0)
    {
        if (NtCurrentTeb()->ProcessEnvironmentBlock->NumberOfProcessors == 1) return;
        n = 64;
    }
    else if (n < 0x2000)
    {
        n <<= 1;
    }
    *pCount = n;
    while (n--) YieldProcessor();
}

__forceinline BOOLEAN RtlpWaitCouldDeadlock(void)
{
    return FALSE;
}

/* Helpers to test node attributes; avoid raw bit-twiddling at callsites. */
__forceinline BOOLEAN SrwNodeIsExclusive(const SRW_WAIT_NODE* n) { return (n->flags & NODEF_EXCL) != 0; }
__forceinline BOOLEAN SrwNodeIsSpinning(const SRW_WAIT_NODE* n)  { return (n->flags & NODEF_SPIN) != 0; }

static VOID NTAPI RtlpSrwWake(PRTL_SRWLOCK SRWLock, SRW_STATE OldStatus)
{
    SRW_STATE CurrStatus;
    SRW_WAIT_NODE* tail;
    SRW_WAIT_NODE* first;

    while (1)
    {
        if (OldStatus & SRWF_Hold)
        {
            do
            {
                CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)(OldStatus - SRWF_Link), (PVOID)OldStatus);
                if (CurrStatus == OldStatus) return;
                OldStatus = (SRW_STATE)CurrStatus;
            } while (OldStatus & SRWF_Hold);
        }

        tail = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
        first = tail->head;
        if (first == NULL)
        {
            SRW_WAIT_NODE* curr = tail;
            do
            {
                curr->prev->next = curr;
                curr = curr->prev;
                first = curr->head;
            } while (first == NULL);
            if (tail != curr) tail->head = first;
        }

        if ((first->next != NULL) && SrwNodeIsExclusive(first))
        {
            tail->head = first->next;
            first->next = NULL;
            InterlockedAndPointer(&SRWLock->Ptr, (PVOID)(~SRWF_Link));
            break;
        }
        else
        {
            CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, NULL, (PVOID)OldStatus);
            if ((SRW_STATE)CurrStatus == OldStatus) break;
            tail->head = first;
            OldStatus = (SRW_STATE)CurrStatus;
        }
    }

    do
    {
        SRW_WAIT_NODE* next = first->next;
        if (InterlockedBitTestAndReset((PLONG)&first->flags, NODE_SPIN_BIT) == 0)
        {
            NtReleaseKeyedEvent(GlobalKeyedEventHandle, first, FALSE, NULL);
        }
        first = next;
    } while (first != NULL);
}

static VOID NTAPI RtlpSrwCompressQueue(PRTL_SRWLOCK SRWLock, SRW_STATE OldStatus)
{
    SRW_STATE CurrStatus;
    if (OldStatus & SRWF_Hold)
    {
        do
        {
            SRW_WAIT_NODE* tail = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
            if (tail != NULL)
            {
                SRW_WAIT_NODE* curr = tail;
                while (curr->head == NULL)
                {
                    curr->prev->next = curr;
                    curr = curr->prev;
                }
                tail->head = curr->head;
            }
            CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)(OldStatus - SRWF_Link), (PVOID)OldStatus);
            if (CurrStatus == OldStatus) return;
            OldStatus = (SRW_STATE)CurrStatus;
        } while (OldStatus & SRWF_Hold);
    }
    RtlpSrwWake(SRWLock, OldStatus);
}

VOID NTAPI RtlInitializeSRWLock(PRTL_SRWLOCK SRWLock)
{
    SRWLock->Ptr = NULL;
}

VOID NTAPI RtlAcquireSRWLockExclusive(PRTL_SRWLOCK SRWLock)
{
    __ALIGNED(16) SRW_WAIT_NODE node;
    BOOLEAN IsOptimize;
    SRW_STATE NewStatus;
    ULONG backoff = 0;
    SRW_STATE CurrStatus;
    SRW_STATE OldStatus;
    int i;

    if (RtlpInterlockedBit0SetPointer(&SRWLock->Ptr) == 0)
        return;

    OldStatus = (SRW_STATE)SRWLock->Ptr;

    while (1)
    {
        if (OldStatus & SRWF_Hold)
        {
            if (RtlpWaitCouldDeadlock()) NtTerminateProcess((HANDLE)-1, STATUS_THREAD_IS_TERMINATING);

            node.flags = NODEF_EXCL | NODEF_SPIN;
            node.next = NULL;
            IsOptimize = FALSE;

            if (OldStatus & SRWF_Wait)
            {
                node.head = NULL;
                node.shareSnapshot = 0;
                node.prev = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
                NewStatus = (SRW_STATE)&node | (OldStatus & SRWF_Many) | (SRWF_Link | SRWF_Wait | SRWF_Hold);
                if (!(OldStatus & SRWF_Link)) IsOptimize = TRUE;
            }
            else
            {
                node.head = &node;
                node.shareSnapshot = (ULONG)(OldStatus >> SRW_COUNT_BIT);
                if (node.shareSnapshot > 1)
                    NewStatus = (SRW_STATE)&node | (SRWF_Many | SRWF_Wait | SRWF_Hold);
                else
                    NewStatus = (SRW_STATE)&node | (SRWF_Wait | SRWF_Hold);
            }

            CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            if (CurrStatus == OldStatus)
            {
                if (IsOptimize) RtlpSrwCompressQueue(SRWLock, NewStatus);
                for (i = SRWLockSpinCount; i > 0; --i)
                {
                    if (!SrwNodeIsSpinning(&node)) break;
                    YieldProcessor();
                }
                if (InterlockedBitTestAndReset((PLONG)&node.flags, NODE_SPIN_BIT))
                    NtWaitForKeyedEvent(GlobalKeyedEventHandle, &node, FALSE, NULL);
                OldStatus = CurrStatus;
            }
            else
            {
                RtlpBackoffExp(&backoff);
                OldStatus = (SRW_STATE)SRWLock->Ptr;
            }
        }
        else
        {
            CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)(OldStatus + SRWF_Hold), (PVOID)OldStatus);
            if (CurrStatus == OldStatus) return;
            RtlpBackoffExp(&backoff);
            OldStatus = (SRW_STATE)SRWLock->Ptr;
        }
    }
}

VOID NTAPI RtlAcquireSRWLockShared(PRTL_SRWLOCK SRWLock)
{
    __ALIGNED(16) SRW_WAIT_NODE node;
    BOOLEAN IsOptimize;
    ULONG backoff = 0;
    int i;

    SRW_STATE NewStatus;
    SRW_STATE CurrStatus;
    SRW_STATE OldStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)((1 << SRW_COUNT_BIT) | SRWF_Hold), NULL);
    if (OldStatus == 0) return;

    while (1)
    {
    if ((OldStatus & SRWF_Hold) && ((OldStatus & SRWF_Wait) || ((OldStatus & SRWM_ITEM) == (SRW_STATE)NULL)))
        {
            if (RtlpWaitCouldDeadlock()) NtTerminateProcess((HANDLE)-1, STATUS_THREAD_IS_TERMINATING);

            node.flags = NODEF_SPIN;
            node.shareSnapshot = 0;
            IsOptimize = FALSE;
            node.next = NULL;

            if (OldStatus & SRWF_Wait)
            {
                node.prev = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
                NewStatus = (SRW_STATE)&node | (OldStatus & (SRWF_Many | SRWF_Hold)) | (SRWF_Link | SRWF_Wait);
                node.head = NULL;
                if (!(OldStatus & SRWF_Link)) IsOptimize = TRUE;
            }
            else
            {
                node.head = &node;
                NewStatus = (SRW_STATE)&node | (SRWF_Wait | SRWF_Hold);
            }

            CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            if (CurrStatus == OldStatus)
            {
                if (IsOptimize) RtlpSrwCompressQueue(SRWLock, NewStatus);
                for (i = SRWLockSpinCount; i > 0; --i)
                {
                    if (!SrwNodeIsSpinning(&node)) break;
                    YieldProcessor();
                }
                if (InterlockedBitTestAndReset((PLONG)&node.flags, NODE_SPIN_BIT))
                    NtWaitForKeyedEvent(GlobalKeyedEventHandle, &node, FALSE, NULL);
                OldStatus = CurrStatus;
            }
            else
            {
                RtlpBackoffExp(&backoff);
                OldStatus = (SRW_STATE)SRWLock->Ptr;
            }
        }
        else
        {
            if (OldStatus & SRWF_Wait)
                NewStatus = OldStatus + SRWF_Hold;
            else
                NewStatus = (OldStatus + (1 << SRW_COUNT_BIT)) | SRWF_Hold;
            CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            if (CurrStatus == OldStatus) return;
            RtlpBackoffExp(&backoff);
            OldStatus = (SRW_STATE)SRWLock->Ptr;
        }
    }
}

VOID NTAPI RtlReleaseSRWLockExclusive(PRTL_SRWLOCK SRWLock)
{
    SRW_STATE CurrStatus;
    SRW_STATE OldStatus = (SRW_STATE)InterlockedExchangeAddPointer(&SRWLock->Ptr, (PVOID)(-(LONG_PTR)SRWF_Hold));
    if ((OldStatus & SRWF_Wait) && !(OldStatus & SRWF_Link))
    {
        OldStatus -= SRWF_Hold;
        CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)(OldStatus + SRWF_Link), (PVOID)OldStatus);
        if (CurrStatus == OldStatus) RtlpSrwWake(SRWLock, OldStatus + SRWF_Link);
    }
}

VOID NTAPI RtlReleaseSRWLockShared(PRTL_SRWLOCK SRWLock)
{
    SRW_STATE CurrStatus, NewStatus;
    ULONG count;
    SRW_STATE OldStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, NULL, (PVOID)(((1 << SRW_COUNT_BIT) | SRWF_Hold)));
    if (OldStatus == ((1 << SRW_COUNT_BIT) | SRWF_Hold)) return;

    if (!(OldStatus & SRWF_Wait))
    {
        do
        {
            if ((OldStatus & (SRWM_ITEM)) <= (1 << SRW_COUNT_BIT)) NewStatus = 0;
            else NewStatus = OldStatus - (1 << SRW_COUNT_BIT);
            CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            if (CurrStatus == OldStatus) return;
            OldStatus = CurrStatus;
        } while (!(OldStatus & SRWF_Wait));
    }

    if (OldStatus & SRWF_Many)
    {
    SRW_WAIT_NODE* curr = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
    while (curr->head == NULL) curr = curr->prev;
    curr = curr->head;
    count = InterlockedDecrement((PLONG)&curr->shareSnapshot);
        if (count > 0) return;
    }

    while (1)
    {
        NewStatus = OldStatus & (~(SRWF_Many | SRWF_Hold));
        if (OldStatus & SRWF_Link)
        {
            CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            if (CurrStatus == OldStatus) return;
        }
        else
        {
            NewStatus |= SRWF_Link;
            CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            if (CurrStatus == OldStatus)
            {
                RtlpSrwWake(SRWLock, NewStatus);
                return;
            }
        }
        OldStatus = CurrStatus;
    }
}

BOOLEAN NTAPI RtlTryAcquireSRWLockExclusive(PRTL_SRWLOCK SRWLock)
{
    return !(RtlpInterlockedBit0SetPointer(&SRWLock->Ptr) == TRUE);
}

BOOLEAN NTAPI RtlTryAcquireSRWLockShared(PRTL_SRWLOCK SRWLock)
{
    ULONG backoff = 0;
    SRW_STATE NewStatus;
    SRW_STATE CurrStatus;
    SRW_STATE OldStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)((1 << SRW_COUNT_BIT) | SRWF_Hold), NULL);
    if (OldStatus == 0) return TRUE;
    while (1)
    {
        if ((OldStatus & SRWF_Hold) && ((OldStatus & SRWF_Wait) || (OldStatus & SRWM_ITEM) == (SRW_STATE)NULL))
            return FALSE;
        if (OldStatus & SRWF_Wait) NewStatus = OldStatus + SRWF_Hold;
        else NewStatus = OldStatus + (1 << SRW_COUNT_BIT);
        CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
        if (CurrStatus == OldStatus) return TRUE;
        RtlpBackoffExp(&backoff);
        OldStatus = (SRW_STATE)SRWLock->Ptr;
    }
}

// static __forceinline LONG RtlpMarkSrwHeld(volatile PVOID* Target)
// {
// #if defined(_M_X64)
    // return _interlockedbittestandset64((volatile LONG64*)Target, 0);
// #elif defined(_M_IX86)
    // return _interlockedbittestandset((volatile LONG*)Target, 0);
// #else
    // PVOID OldValue = *Target;
    // for (;;)
    // {
        // ULONG_PTR OldBits = (ULONG_PTR)OldValue;
        // if (OldBits & SRWF_Hold) return 1; /* bit was already set */
		
        // PVOID NewValue = (PVOID)(OldBits | SRWF_Hold);
        // PVOID Prev = InterlockedCompareExchangePointer((PVOID*)Target, NewValue, OldValue);
		
        // if (Prev == OldValue) return 0;     /* successfully set from 0 to 1 */
		
        // OldValue = Prev;
    // }
// #endif
// }

// static VOID NTAPI RtlpBackoffExp(ULONG* pCount)
// {
    // ULONG n = *pCount;
    // if (n == 0)
    // {
        // if (NtCurrentTeb()->ProcessEnvironmentBlock->NumberOfProcessors == 1) return;
        // n = 64;
    // }
    // else if (n < 0x2000)
    // {
        // n <<= 1;
    // }
    // *pCount = n;
    // while (n--) YieldProcessor();
// }

// /* Helpers to test node attributes; avoid raw bit-twiddling at callsites. */
// static __forceinline BOOLEAN SrwNodeIsExclusive(const SRW_WAIT_NODE* n) { return (n->flags & NODEF_EXCL) != 0; }
// static __forceinline BOOLEAN SrwNodeIsSpinning(const SRW_WAIT_NODE* n)  { return (n->flags & NODEF_SPIN) != 0; }

// static VOID NTAPI RtlpSrwWake(PRTL_SRWLOCK SRWLock, SRW_STATE OldStatus)
// {
    // SRW_STATE CurrStatus;
    // SRW_WAIT_NODE* tail;
    // SRW_WAIT_NODE* first;

    // while (1)
    // {
        // while (OldStatus & SRWF_Hold)
        // {
            // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)(OldStatus & ~(SRW_STATE)SRWF_Link), (PVOID)OldStatus);
            // if (CurrStatus == OldStatus) return;
            // OldStatus = (SRW_STATE)CurrStatus;
        // }
		
        // tail = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
        // first = tail->head;
        // if (first == NULL)
        // {
            // SRW_WAIT_NODE* curr = tail;
            // do
            // {
                // curr->prev->next = curr;
                // curr = curr->prev;
                // first = curr->head;
            // } while (first == NULL);
            // if (tail != curr) tail->head = first;
        // }

        // if ((first->next != NULL) && SrwNodeIsExclusive(first))
        // {
            // tail->head = first->next;
            // first->next = NULL;
            // InterlockedAndPointer(&SRWLock->Ptr, (PVOID)(~((SRW_STATE)SRWF_Link)));
            // break;
        // }
        // else
        // {
            // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, NULL, (PVOID)OldStatus);
            // if (CurrStatus == OldStatus) break;
            // tail->head = first;
            // OldStatus = (SRW_STATE)CurrStatus;
        // }
    // }
	
    // do
    // {
        // SRW_WAIT_NODE* next = first->next;
        // if (InterlockedBitTestAndReset((PLONG)&first->flags, NODE_SPIN_BIT) == 0)
        // {
            // NtReleaseKeyedEvent(GlobalKeyedEventHandle, first, FALSE, NULL);
        // }
        // first = next;
    // } while (first != NULL);
// }

// static VOID NTAPI RtlpSrwCompressQueue(PRTL_SRWLOCK SRWLock, SRW_STATE OldStatus)
// {
    // SRW_STATE CurrStatus;
    // while (OldStatus & SRWF_Hold)
    // {
        // SRW_WAIT_NODE* tail = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
        // if (tail != NULL)
        // {
            // SRW_WAIT_NODE* curr = tail;
            // while (curr->head == NULL)
            // {
                // curr->prev->next = curr;
                // curr = curr->prev;
            // }
            // tail->head = curr->head;
        // }
        // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)(OldStatus & ~(SRW_STATE)SRWF_Link), (PVOID)OldStatus);
        // if (CurrStatus == OldStatus) return;
        // OldStatus = (SRW_STATE)CurrStatus;
    // }
    // RtlpSrwWake(SRWLock, OldStatus);
// }

// VOID NTAPI RtlInitializeSRWLock(PRTL_SRWLOCK SRWLock)
// {
    // SRWLock->Ptr = NULL;
// }

// VOID NTAPI RtlAcquireSRWLockExclusive(PRTL_SRWLOCK SRWLock)
// {
    // __ALIGNED(16) SRW_WAIT_NODE node;
    // BOOLEAN IsOptimize;
    // SRW_STATE NewStatus;
    // ULONG backoff = 0;
    // SRW_STATE CurrStatus;
    // SRW_STATE OldStatus;
    // int i;

    // if (RtlpMarkSrwHeld(&SRWLock->Ptr) == 0)
        // return;

    // OldStatus = (SRW_STATE)SRWLock->Ptr;

    // while (1)
    // {
        // if (OldStatus & SRWF_Hold)
        // {
            // if (RtlpWaitCouldDeadlock()) NtTerminateProcess((HANDLE)-1, STATUS_THREAD_IS_TERMINATING);

            // node.flags = NODEF_EXCL | NODEF_SPIN;
            // node.next = NULL;
            // IsOptimize = FALSE;

            // if (OldStatus & SRWF_Wait)
            // {
                // node.head = NULL;
                // node.shareSnapshot = 0;
                // node.prev = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
                // NewStatus = (SRW_STATE)&node | (OldStatus & SRWF_Many) | (SRWF_Link | SRWF_Wait | SRWF_Hold);
                // if (!(OldStatus & SRWF_Link)) IsOptimize = TRUE;
            // }
            // else
            // {
                // node.head = &node;
                // node.shareSnapshot = (ULONG)(OldStatus >> SRW_COUNT_BIT);
                // if (node.shareSnapshot > 1)
                    // NewStatus = (SRW_STATE)&node | (SRWF_Many | SRWF_Wait | SRWF_Hold);
                // else
                    // NewStatus = (SRW_STATE)&node | (SRWF_Wait | SRWF_Hold);
            // }

            // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            // if (CurrStatus == OldStatus)
            // {
                // if (IsOptimize) RtlpSrwCompressQueue(SRWLock, NewStatus);
                // for (i = SRWLockSpinCount; i > 0; --i)
                // {
                    // if (!SrwNodeIsSpinning(&node)) break;
                    // YieldProcessor();
                // }
                // if (InterlockedBitTestAndReset((PLONG)&node.flags, NODE_SPIN_BIT))
                    // NtWaitForKeyedEvent(GlobalKeyedEventHandle, &node, FALSE, NULL);
                // OldStatus = CurrStatus;
            // }
            // else
            // {
                // RtlpBackoffExp(&backoff);
                // OldStatus = (SRW_STATE)SRWLock->Ptr;
            // }
        // }
        // else
        // {
            // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)(OldStatus | SRWF_Hold), (PVOID)OldStatus);
            // if (CurrStatus == OldStatus) return;
            // RtlpBackoffExp(&backoff);
            // OldStatus = (SRW_STATE)SRWLock->Ptr;
        // }
    // }
// }

// VOID NTAPI RtlAcquireSRWLockShared(PRTL_SRWLOCK SRWLock)
// {
    // __ALIGNED(16) SRW_WAIT_NODE node;
    // BOOLEAN IsOptimize;
    // ULONG backoff = 0;
    // int i;

    // SRW_STATE NewStatus;
    // SRW_STATE CurrStatus;
    // SRW_STATE OldStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)((1 << SRW_COUNT_BIT) | SRWF_Hold), NULL);
    // if (OldStatus == 0) return;

    // while (1)
    // {
    // if ((OldStatus & SRWF_Hold) && ((OldStatus & SRWF_Wait) || ((OldStatus & SRWM_ITEM) == (SRW_STATE)NULL)))
        // {
            // if (RtlpWaitCouldDeadlock()) NtTerminateProcess((HANDLE)-1, STATUS_THREAD_IS_TERMINATING);

            // node.flags = NODEF_SPIN;
            // node.shareSnapshot = 0;
            // IsOptimize = FALSE;
            // node.next = NULL;

            // if (OldStatus & SRWF_Wait)
            // {
                // node.prev = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
                // NewStatus = (SRW_STATE)&node | (OldStatus & (SRWF_Many | SRWF_Hold)) | (SRWF_Link | SRWF_Wait);
                // node.head = NULL;
                // if (!(OldStatus & SRWF_Link)) IsOptimize = TRUE;
            // }
            // else
            // {
                // node.head = &node;
                // NewStatus = (SRW_STATE)&node | (SRWF_Wait | SRWF_Hold);
            // }

            // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            // if (CurrStatus == OldStatus)
            // {
                // if (IsOptimize) RtlpSrwCompressQueue(SRWLock, NewStatus);
                // for (i = SRWLockSpinCount; i > 0; --i)
                // {
                    // if (!SrwNodeIsSpinning(&node)) break;
                    // YieldProcessor();
                // }
                // if (InterlockedBitTestAndReset((PLONG)&node.flags, NODE_SPIN_BIT))
                    // NtWaitForKeyedEvent(GlobalKeyedEventHandle, &node, FALSE, NULL);
                // OldStatus = CurrStatus;
            // }
            // else
            // {
                // RtlpBackoffExp(&backoff);
                // OldStatus = (SRW_STATE)SRWLock->Ptr;
            // }
        // }
        // else
        // {
            // if (OldStatus & SRWF_Wait)
                // NewStatus = OldStatus | SRWF_Hold;
            // else
                // NewStatus = (OldStatus + (1 << SRW_COUNT_BIT)) | SRWF_Hold;
            // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            // if (CurrStatus == OldStatus) return;
            // RtlpBackoffExp(&backoff);
            // OldStatus = (SRW_STATE)SRWLock->Ptr;
        // }
    // }
// }

// VOID NTAPI RtlReleaseSRWLockExclusive(PRTL_SRWLOCK SRWLock)
// {
    // SRW_STATE CurrStatus;
    // SRW_STATE OldStatus = (SRW_STATE)InterlockedExchangeAddPointer(&SRWLock->Ptr, (PVOID)(-(LONG_PTR)SRWF_Hold));
    // if ((OldStatus & SRWF_Wait) && !(OldStatus & SRWF_Link))
    // {
        // OldStatus &= ~(SRW_STATE)SRWF_Hold;
        // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)(OldStatus | SRWF_Link), (PVOID)OldStatus);
        // if (CurrStatus == OldStatus) RtlpSrwWake(SRWLock, OldStatus | SRWF_Link);
    // }
// }

// VOID NTAPI RtlReleaseSRWLockShared(PRTL_SRWLOCK SRWLock)
// {
    // SRW_STATE CurrStatus, NewStatus;
    // ULONG count;
    // SRW_STATE OldStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, NULL, (PVOID)(((1 << SRW_COUNT_BIT) | SRWF_Hold)));
    // if (OldStatus == ((1 << SRW_COUNT_BIT) | SRWF_Hold)) return;

    // while (!(OldStatus & SRWF_Wait))
    // {
        // if ((OldStatus & (SRWM_ITEM)) <= (1 << SRW_COUNT_BIT)) NewStatus = 0;
        // else NewStatus = OldStatus - (1 << SRW_COUNT_BIT);
        // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
        // if (CurrStatus == OldStatus) return;
        // OldStatus = CurrStatus;
    // }

    // if (OldStatus & SRWF_Many)
    // {
        // SRW_WAIT_NODE* curr = (SRW_WAIT_NODE*)(OldStatus & SRWM_ITEM);
        // while (curr->head == NULL) curr = curr->prev;
        // curr = curr->head;
        // count = InterlockedDecrement((PLONG)&curr->shareSnapshot);
        // if (count > 0) return;
    // }

    // while (1)
    // {
        // NewStatus = OldStatus & (~(SRW_STATE)(SRWF_Many | SRWF_Hold));
        // if (OldStatus & SRWF_Link)
        // {
            // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            // if (CurrStatus == OldStatus) return;
        // }
        // else
        // {
            // NewStatus |= SRWF_Link;
            // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
            // if (CurrStatus == OldStatus)
            // {
                // RtlpSrwWake(SRWLock, NewStatus);
                // return;
            // }
        // }
        // OldStatus = CurrStatus;
    // }
// }

// BOOLEAN NTAPI RtlTryAcquireSRWLockExclusive(PRTL_SRWLOCK SRWLock)
// {
    // return RtlpMarkSrwHeld(&SRWLock->Ptr) == 0;
// }

// BOOLEAN NTAPI RtlTryAcquireSRWLockShared(PRTL_SRWLOCK SRWLock)
// {
    // ULONG backoff = 0;
    // SRW_STATE NewStatus;
    // SRW_STATE CurrStatus;
    // SRW_STATE OldStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)((1 << SRW_COUNT_BIT) | SRWF_Hold), NULL);
    // if (OldStatus == 0) return TRUE;
    // while (1)
    // {
        // if ((OldStatus & SRWF_Hold) && ((OldStatus & SRWF_Wait) || (OldStatus & SRWM_ITEM) == (SRW_STATE)NULL))
            // return FALSE;
        // if (OldStatus & SRWF_Wait) NewStatus = OldStatus | SRWF_Hold;
        // else NewStatus = OldStatus + (1 << SRW_COUNT_BIT);
        // CurrStatus = (SRW_STATE)InterlockedCompareExchangePointer(&SRWLock->Ptr, (PVOID)NewStatus, (PVOID)OldStatus);
        // if (CurrStatus == OldStatus) return TRUE;
        // RtlpBackoffExp(&backoff);
        // OldStatus = (SRW_STATE)SRWLock->Ptr;
    // }
// }