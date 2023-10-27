#include "HAL9000.h"
#include "thread_internal.h"
#include "semaphore.h"

void SemaphoreInit(PSEMAPHORE Semaphore, DWORD InitialValue)
{
	ASSERT(Semaphore != NULL);

	memzero(Semaphore, sizeof(SEMAPHORE));

	LockInit(&Semaphore->MutexLock);
	InitializeListHead(&Semaphore->WaitingList);
	Semaphore->Value = InitialValue;
}

void SemaphoreDown(PSEMAPHORE Semaphore, DWORD Value)
{
	INTR_STATE dummyState;
	INTR_STATE oldState;
	PTHREAD pCurrentThread = GetCurrentThread();

	ASSERT(Semaphore != NULL);
	ASSERT(pCurrentThread != NULL);

	oldState = CpuIntrDisable();

	LockAcquire(&Semaphore->MutexLock, &dummyState);

	while (Semaphore->Value < Value)
	{
		InsertTailList(&Semaphore->WaitingList, &pCurrentThread->ReadyList);
		ThreadTakeBlockLock();
		LockRelease(&Semaphore->MutexLock, dummyState);
		ThreadBlock();
		LockAcquire(&Semaphore->MutexLock, &dummyState);
	}

	Semaphore->Value -= Value;

	LockRelease(&Semaphore->MutexLock, dummyState);

	CpuIntrSetState(oldState);
}

void SemaphoreUp(PSEMAPHORE Semaphore, DWORD Value)
{
	INTR_STATE oldState;
	PLIST_ENTRY pEntry;

	ASSERT(Semaphore != NULL);

	LockAcquire(&Semaphore->MutexLock, &oldState);

	Semaphore->Value += Value;

	pEntry = RemoveHeadList(&Semaphore->WaitingList);

	if (pEntry != &Semaphore->WaitingList)
	{
		PTHREAD pThread = CONTAINING_RECORD(pEntry, THREAD, ReadyList);
		ThreadUnblock(pThread);
	}

	LockRelease(&Semaphore->MutexLock, oldState);
}
