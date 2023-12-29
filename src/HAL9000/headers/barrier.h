#pragma once

#include "synch.h"

// Threads 6.

typedef struct _BARRIER
{
    DWORD   NoOfParticipants;
    DWORD   Count;
    LOCK    Lock;
} BARRIER, * PBARRIER;

void BarrierInit(
    OUT PBARRIER Barrier,
    IN DWORD NoOfParticipants
    );

void BarrierWait(
    INOUT PBARRIER Barrier
    );