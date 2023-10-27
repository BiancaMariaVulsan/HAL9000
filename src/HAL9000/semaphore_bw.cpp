#include "HAL9000.h"
#include "thread_internal.h"
#include "semaphore_bw.h"

void SemaphoreInit(PSEMAPHORE_BW Semaphore, DWORD InitialValue)
{
	ASSERT(Semaphore != NULL);

	Semaphore->Value = InitialValue;
}

void SemaphoreDown(PSEMAPHORE_BW Semaphore, DWORD Value)
{
	PTHREAD pCurrentThread = GetCurrentThread();

	ASSERT(Semaphore != NULL);
	ASSERT(pCurrentThread != NULL);

	while (Semaphore->Value < Value)
	{
		// Busy-wait until the semaphore value becomes greater or equal to the requested value
	}

	Semaphore->Value -= Value;
}

void SemaphoreUp(PSEMAPHORE_BW Semaphore, DWORD Value)
{
	ASSERT(Semaphore != NULL);

	Semaphore->Value += Value;
}
