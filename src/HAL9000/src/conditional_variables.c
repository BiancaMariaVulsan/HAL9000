#include "HAL9000.h"
#include "conditional_variables.h"
#include "thread_internal.h"

// Thread 7.

void 
CondVariableInit(
    OUT PCONDITIONAL_VARIABLE CondVariable
    )
{
    InitializeListHead(&CondVariable->WaiterList);
}

void 
CondVariableWait(
    INOUT PCONDITIONAL_VARIABLE CondVariable, 
    INOUT PMUTEX Lock
    )
{
    LIST_ENTRY currentThreadEntry;
    INTR_STATE oldState;

    LockAcquire(&Lock->MutexLock, &oldState);

    // Add the current thread to the waiter list
    InitializeListHead(&currentThreadEntry);
    InsertTailList(&CondVariable->WaiterList, &currentThreadEntry);

    LockRelease(&Lock->MutexLock, oldState);

    // Release the lock and block the current thread
    ThreadBlock();

    // Reacquire the lock after being unblocked
    LockAcquire(&Lock->MutexLock, &oldState);
}

void 
CondVariableSignal(
    INOUT PCONDITIONAL_VARIABLE CondVariable, 
    INOUT PMUTEX Lock
    )
{
    LIST_ENTRY* pWaiterEntry = NULL;
    INTR_STATE oldState;

    LockAcquire(&Lock->MutexLock, &oldState);

    // Check if there are any threads waiting
    if (!IsListEmpty(&CondVariable->WaiterList))
    {
        // Signal the first waiting thread
        pWaiterEntry = RemoveHeadList(&CondVariable->WaiterList);
        ThreadUnblock(CONTAINING_RECORD(pWaiterEntry, THREAD, ReadyList));
    }

    LockRelease(&Lock->MutexLock, oldState);
}

void CondVariableBroadcast(
    INOUT PCONDITIONAL_VARIABLE CondVariable, 
    INOUT PMUTEX Lock
    )
{
    LIST_ENTRY* pWaiterEntry = NULL;
    INTR_STATE oldState;

    LockAcquire(&Lock->MutexLock, &oldState);

    // Wake up all waiting threads
    while (!IsListEmpty(&CondVariable->WaiterList))
    {
        pWaiterEntry = RemoveHeadList(&CondVariable->WaiterList);
        ThreadUnblock(CONTAINING_RECORD(pWaiterEntry, THREAD, ReadyList));
    }

    LockRelease(&Lock->MutexLock, oldState);
}
