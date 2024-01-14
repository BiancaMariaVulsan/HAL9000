#pragma once


#include "list.h"
#include "synch.h"
#include "mutex.h"

// Thread 7.

typedef struct _CONDITIONAL_VARIABLE
{
    LIST_ENTRY WaiterList;
} CONDITIONAL_VARIABLE, * PCONDITIONAL_VARIABLE;

//******************************************************************************
// Function:     CondVariableInit
// Description:  Initializes condition variable CondVariable
// Returns:      void
// Parameter:    OUT PCONDITIONAL_VARIABLE CondVariable
//******************************************************************************
void 
CondVariableInit(
    OUT PCONDITIONAL_VARIABLE CondVariable
    );

//******************************************************************************
// Function:     CondVariableWait
// Description:  Atomically releases Lock and waits for CondVariable to be signaled by
// some other piece of code. After CondVariable is signaled, Lock is
// reacquired before returning. The lock must be held before calling
// this function.
// Returns:      void
// Parameter:    INOUT PCONDITIONAL_VARIABLE CondVariable
// Parameter:    INOUT PMUTEX Lock
//******************************************************************************
void 
CondVariableWait(
    INOUT PCONDITIONAL_VARIABLE CondVariable, 
    INOUT PMUTEX Lock
    );

//******************************************************************************
// Function:     CondVariableSignal
// Description:  If any threads are waiting on CondVariable (protected by Lock), then
// this function signals one of them to wake up from its wait.
// Returns:      void
// Parameter:    INOUT PCONDITIONAL_VARIABLE CondVariable
// Parameter:    INOUT PMUTEX Lock
//******************************************************************************
void 
CondVariableSignal(
    INOUT PCONDITIONAL_VARIABLE CondVariable, 
    INOUT PMUTEX Lock
    );

//******************************************************************************
// Function:     CondVariableBroadcast
// Description:  Wakes up all threads, if any, waiting on CondVariable (protected by
// Lock). Lock must be held before calling this function.
// Returns:      void
// Parameter:    INOUT PCONDITIONAL_VARIABLE CondVariable
// Parameter:    INOUT PMUTEX Lock
//******************************************************************************
void 
CondVariableBroadcast(
    INOUT PCONDITIONAL_VARIABLE CondVariable, 
    INOUT PMUTEX Lock
    );


