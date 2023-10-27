#pragma once

#ifndef SEMAPHORE_BW_H
#define SEMAPHORE_BW_H

#include "HAL9000.h"

typedef struct _SEMAPHORE_BW
{
	volatile DWORD Value;
} SEMAPHORE_BW, *PSEMAPHORE_BW;

void SemaphoreInit(PSEMAPHORE_BW Semaphore, DWORD InitialValue);
void SemaphoreDown(PSEMAPHORE_BW Semaphore, DWORD Value);
void SemaphoreUp(PSEMAPHORE_BW Semaphore, DWORD Value);

#endif // SEMAPHORE_BW_H
