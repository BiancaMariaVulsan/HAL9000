#include "HAL9000.h"
#include "barrier.h"
#include "smp.h"
#include "thread_internal.h"

// Threads 6.

void 
BarrierInit(
    OUT PBARRIER Barrier,
    IN DWORD NoOfParticipants
    )
{
    ASSERT(Barrier != NULL);
    ASSERT(NoOfParticipants > 0);

    memzero(Barrier, sizeof(BARRIER));

    Barrier->NoOfParticipants = NoOfParticipants;
}

void 
BarrierWait(
    INOUT PBARRIER Barrier
    )
{
    ASSERT(Barrier != NULL);
    INTR_STATE oldState;

    LockAcquire(&Barrier->Lock, &oldState);

    Barrier->Count++;

    if (Barrier->Count < Barrier->NoOfParticipants)
    {
        // Release the lock and busy-wait
        LockRelease(&Barrier->Lock, oldState);
        while (Barrier->Count < Barrier->NoOfParticipants)
        {
            // Busy-wait
            ThreadYield();
        }
    }
    else
    {
        // Last participant reached the barrier, reset the count
        Barrier->Count = 0;
        LockRelease(&Barrier->Lock, oldState);
    }
}
