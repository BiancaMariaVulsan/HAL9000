#pragma once

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "HAL9000.h"

typedef struct _SEMAPHORE
{
	DWORD Value;
	LOCK MutexLock;
	LIST_ENTRY WaitingList;
} SEMAPHORE, *PSEMAPHORE;

void SemaphoreInit(PSEMAPHORE Semaphore, DWORD InitialValue);
void SemaphoreDown(PSEMAPHORE Semaphore, DWORD Value);
void SemaphoreUp(PSEMAPHORE Semaphore, DWORD Value);

#endif // SEMAPHORE_H
