#include "HAL9000.h"
#include "syscall.h"
#include "gdtmu.h"
#include "syscall_defs.h"
#include "syscall_func.h"
#include "syscall_no.h"
#include "mmu.h"
#include "process_internal.h"
#include "dmp_cpu.h"
#include "thread.h"
#include "thread_internal.h"
#include "io.h"
#include "iomu.h"
#include "vmm.h"

extern void SyscallEntry();

#define SYSCALL_IF_VERSION_KM       SYSCALL_IMPLEMENTED_IF_VERSION

// Userprog 6.
static BOOLEAN syscallsDisabled = TRUE;

// Userprog 7.
#define MAX_NAME_SIZE 256
typedef struct {
    char VariableName[MAX_NAME_SIZE];
    QWORD Value;
} GLOBAL_VARIABLE;
// Define a maximum number of global variables
#define MAX_GLOBAL_VARIABLES 100
// Array to store global variables
GLOBAL_VARIABLE globalVariables[MAX_GLOBAL_VARIABLES];

void
SyscallHandler(
    INOUT   COMPLETE_PROCESSOR_STATE    *CompleteProcessorState
    )
{
    SYSCALL_ID sysCallId;
    PQWORD pSyscallParameters;
    PQWORD pParameters;
    STATUS status;
    REGISTER_AREA* usermodeProcessorState;

    ASSERT(CompleteProcessorState != NULL);

    // It is NOT ok to setup the FMASK so that interrupts will be enabled when the system call occurs
    // The issue is that we'll have a user-mode stack and we wouldn't want to receive an interrupt on
    // that stack. This is why we only enable interrupts here.
    ASSERT(CpuIntrGetState() == INTR_OFF);
    CpuIntrSetState(INTR_ON);

    LOG_TRACE_USERMODE("The syscall handler has been called!\n");

    status = STATUS_SUCCESS;
    pSyscallParameters = NULL;
    pParameters = NULL;
    usermodeProcessorState = &CompleteProcessorState->RegisterArea;

    __try
    {
        if (LogIsComponentTraced(LogComponentUserMode))
        {
            DumpProcessorState(CompleteProcessorState);
        }

        // Check if indeed the shadow stack is valid (the shadow stack is mandatory)
        pParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp];
        status = MmuIsBufferValid(pParameters, SHADOW_STACK_SIZE, PAGE_RIGHTS_READ, GetCurrentProcess());
        if (!SUCCEEDED(status))
        {
            LOG_FUNC_ERROR("MmuIsBufferValid", status);
            __leave;
        }

        sysCallId = usermodeProcessorState->RegisterValues[RegisterR8];

        LOG_TRACE_USERMODE("System call ID is %u\n", sysCallId);

        // The first parameter is the system call ID, we don't care about it => +1
        pSyscallParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp] + 1;

        if (syscallsDisabled) {
            // Dispatch syscalls
            switch (sysCallId)
            {
            case SyscallIdIdentifyVersion:
                status = SyscallValidateInterface((SYSCALL_IF_VERSION)*pSyscallParameters);
                break;
                // STUDENT TODO: implement the rest of the syscalls
            case SyscallIdVirtualAlloc:
                status = SyscallVirtualAlloc(
                    (PVOID)pSyscallParameters[0],
                    (QWORD)pSyscallParameters[1],
                    (VMM_ALLOC_TYPE)pSyscallParameters[2],
                    (PAGE_RIGHTS)pSyscallParameters[3],
                    (UM_HANDLE)pSyscallParameters[4],
                    (QWORD)pSyscallParameters[5],
                    (PVOID*)pSyscallParameters[6]
                );
                break;
            case SyscallIdVirtualFree:
                status = SyscallVirtualFree(
                    (PVOID)pSyscallParameters[0],
                    (QWORD)pSyscallParameters[1],
                    (VMM_FREE_TYPE)pSyscallParameters[2]
                );
                break;
            case SyscallIdFileWrite:
                status = SyscallFileWrite(
                    (UM_HANDLE)pSyscallParameters[0],
                    (PVOID)pSyscallParameters[1],
                    (QWORD)pSyscallParameters[2],
                    (QWORD*)pSyscallParameters[3]
                );
                break;
            case SyscallIdProcessExit:
                status = SyscallProcessExit((STATUS)pSyscallParameters[0]);
                break;
            case SyscallIdThreadExit:
                status = SyscallThreadExit((STATUS)pSyscallParameters[0]);
                break;
            case SyscallIdMemset:
                status = SyscallMemset(
                    (PBYTE)pSyscallParameters[0],
                    (DWORD)pSyscallParameters[1],
                    (BYTE)pSyscallParameters[2]
                );
                break;
            case SyscallIdProcessCreate:
                status = SyscallProcessCreate((char*)pSyscallParameters[0],
                    (QWORD)pSyscallParameters[1], (char*)pSyscallParameters[2],
                    (QWORD)pSyscallParameters[3], (UM_HANDLE*)pSyscallParameters[4]);
                break;
            case SyscallIdDisableSyscalls:
                status = SyscallDisableSyscalls((BOOLEAN)pSyscallParameters[0]);
                break;
            case SyscallIdSetGlobalVariable:
				status = SyscallSetGlobalVariable((char*)pSyscallParameters[0],
					(DWORD)pSyscallParameters[1], (QWORD)pSyscallParameters[2]);
				break;
            case SyscallIdGetGlobalVariable:
                status = SyscallGetGlobalVariable((char*)pSyscallParameters[0],
                    (DWORD)pSyscallParameters[1], (PQWORD)pSyscallParameters[2]);
                break;
            default:
                LOG_ERROR("Unimplemented syscall called from User-space!\n");
                status = STATUS_UNSUPPORTED;
                break;
            }
		}
		else {
            switch (sysCallId)
            {
            case SyscallIdDisableSyscalls:
                status = SyscallDisableSyscalls((BOOLEAN)pSyscallParameters[0]);
                break;
            default:
                LOG_ERROR("Unimplemented syscall called from User-space!\n");
                status = STATUS_UNSUPPORTED;
                break;
            }
        }

    }
    __finally
    {
        LOG_TRACE_USERMODE("Will set UM RAX to 0x%x\n", status);

        usermodeProcessorState->RegisterValues[RegisterRax] = status;

        CpuIntrSetState(INTR_OFF);
    }
}

void
SyscallPreinitSystem(
    void
    )
{

}

STATUS
SyscallInitSystem(
    void
    )
{
    return STATUS_SUCCESS;
}

STATUS
SyscallUninitSystem(
    void
    )
{
    return STATUS_SUCCESS;
}

void
SyscallCpuInit(
    void
    )
{
    IA32_STAR_MSR_DATA starMsr;
    WORD kmCsSelector;
    WORD umCsSelector;

    memzero(&starMsr, sizeof(IA32_STAR_MSR_DATA));

    kmCsSelector = GdtMuGetCS64Supervisor();
    ASSERT(kmCsSelector + 0x8 == GdtMuGetDS64Supervisor());

    umCsSelector = GdtMuGetCS32Usermode();
    /// DS64 is the same as DS32
    ASSERT(umCsSelector + 0x8 == GdtMuGetDS32Usermode());
    ASSERT(umCsSelector + 0x10 == GdtMuGetCS64Usermode());

    // Syscall RIP <- IA32_LSTAR
    __writemsr(IA32_LSTAR, (QWORD) SyscallEntry);

    LOG_TRACE_USERMODE("Successfully set LSTAR to 0x%X\n", (QWORD) SyscallEntry);

    // Syscall RFLAGS <- RFLAGS & ~(IA32_FMASK)
    __writemsr(IA32_FMASK, RFLAGS_INTERRUPT_FLAG_BIT);

    LOG_TRACE_USERMODE("Successfully set FMASK to 0x%X\n", RFLAGS_INTERRUPT_FLAG_BIT);

    // Syscall CS.Sel <- IA32_STAR[47:32] & 0xFFFC
    // Syscall DS.Sel <- (IA32_STAR[47:32] + 0x8) & 0xFFFC
    starMsr.SyscallCsDs = kmCsSelector;

    // Sysret CS.Sel <- (IA32_STAR[63:48] + 0x10) & 0xFFFC
    // Sysret DS.Sel <- (IA32_STAR[63:48] + 0x8) & 0xFFFC
    starMsr.SysretCsDs = umCsSelector;

    __writemsr(IA32_STAR, starMsr.Raw);

    LOG_TRACE_USERMODE("Successfully set STAR to 0x%X\n", starMsr.Raw);
}

// Userprog ex 1.
// SyscallIdIdentifyVersion
STATUS
SyscallValidateInterface(
    IN  SYSCALL_IF_VERSION          InterfaceVersion
)
{
    LOG_TRACE_USERMODE("Will check interface version 0x%x from UM against 0x%x from KM\n",
        InterfaceVersion, SYSCALL_IF_VERSION_KM);

    if (InterfaceVersion != SYSCALL_IF_VERSION_KM)
    {
        LOG_ERROR("Usermode interface 0x%x incompatible with KM!\n", InterfaceVersion);
        return STATUS_INCOMPATIBLE_INTERFACE;
    }

    return STATUS_SUCCESS;
}

// STUDENT TODO: implement the rest of the syscalls
// Userprog 1.
STATUS
SyscallThreadExit(
    IN  STATUS                      ExitStatus
)
{
    LOG_TRACE_USERMODE("Will exit thread with status 0x%x\n", ExitStatus);
    ThreadExit(ExitStatus);
    return STATUS_SUCCESS;
}

// Userprog 1.
STATUS
SyscallProcessExit(
    IN  STATUS                      ExitStatus
)
{
    LOG_TRACE_USERMODE("Will exit process with status 0x%x\n", ExitStatus);
    PPROCESS crtProcess = GetCurrentProcess();
    if (crtProcess == NULL) {
        return STATUS_UNSUCCESSFUL;
	}
    ProcessTerminate(crtProcess);
    return STATUS_SUCCESS;
}

// Userprog 2.
STATUS
SyscallFileWrite(
    IN  UM_HANDLE                   FileHandle,
    IN_READS_BYTES(BytesToWrite)
    PVOID                           Buffer,
    IN  QWORD                       BytesToWrite,
    OUT QWORD* BytesWritten
)
{
    if (BytesWritten == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    if (FileHandle == UM_FILE_HANDLE_STDOUT) {
        *BytesWritten = BytesToWrite;
        LOG("[%s]:[%s]\n", ProcessGetName(NULL), Buffer);
        return STATUS_SUCCESS;
    }

    *BytesWritten = BytesToWrite;
    return STATUS_SUCCESS;
}

// Userprog 4.
STATUS
SyscallMemset(
    OUT_WRITES(BytesToWrite)    PBYTE   Address,
    IN                          DWORD   BytesToWrite,
    IN                          BYTE    ValueToWrite
)
{
    STATUS status = MmuIsBufferValid(Address, sizeof(Address), PAGE_RIGHTS_WRITE, GetCurrentProcess());
    if (!SUCCEEDED(status))
    {
        return STATUS_INVALID_PARAMETER1;
    }

	memset(Address, ValueToWrite, BytesToWrite);
	return STATUS_SUCCESS;
}

// Userprog 5.
STATUS
SyscallProcessCreate(
    IN_READS_Z(PathLength)
    char*                           ProcessPath,
    IN          QWORD               PathLength,
    IN_READS_OPT_Z(ArgLength)
    char*                           Arguments,
    IN          QWORD               ArgLength,
    OUT         UM_HANDLE*          ProcessHandle
)
{
    PPROCESS pProcess;
    STATUS status;
    INTR_STATE oldState;

    status = MmuIsBufferValid((const PVOID)ProcessPath, PathLength, PAGE_RIGHTS_READ, GetCurrentProcess());
    if (!SUCCEEDED(status))
    {
        return STATUS_INVALID_PARAMETER1;
    }

    if (PathLength <= 0) {
        return STATUS_INVALID_PARAMETER2;
    }

    if (Arguments != NULL) {
        status = MmuIsBufferValid((const PVOID)Arguments, ArgLength, PAGE_RIGHTS_READ, GetCurrentProcess());
        if (!SUCCEEDED(status))
        {
            return STATUS_INVALID_PARAMETER3;
        }
        if (ArgLength < 0) {
            return STATUS_INVALID_PARAMETER3;
        }
    }

    status = MmuIsBufferValid(ProcessHandle, sizeof(UM_HANDLE), PAGE_RIGHTS_WRITE, GetCurrentProcess());
    if (!SUCCEEDED(status))
    {
        return STATUS_INVALID_PARAMETER4;
    }

    char resultPath[MAX_PATH];

    if (strncmp(IomuGetSystemPartitionPath(), ProcessPath, 3) != 0) {
        snprintf(resultPath, MAX_PATH, "%sApplications\\%s", IomuGetSystemPartitionPath(), ProcessPath);
    }
    else {
        snprintf(resultPath, MAX_PATH, "%s", ProcessPath);
    }

    // ProcessCreate is used to create a child (pProcess) of the currently running process
    status = ProcessCreate(
        resultPath,
        Arguments,
        &pProcess
    );
    if (!SUCCEEDED(status)) {
        *ProcessHandle = UM_INVALID_HANDLE_VALUE;
        return status;
    }

    // Use the PID as handle
    *ProcessHandle = pProcess->Id;

    // Add the child to the list of children of the current process
    LockAcquire(&pProcess->ProcessChildrenLock, &oldState);
    InsertTailList(&GetCurrentProcess()->ProcessChildrenHead, &pProcess->ProcessChildrenElem);
    LockRelease(&pProcess->ProcessChildrenLock, oldState);

    return STATUS_SUCCESS;
}

// Userprog 6.
STATUS
SyscallDisableSyscalls(
    IN      BOOLEAN     Disable
    )
{
    syscallsDisabled = Disable;
    return STATUS_SUCCESS;
}

// Userprog 7.
STATUS
SyscallSetGlobalVariable(
    IN_READS_Z(VarLength)           char*   VariableName,
    IN                              DWORD   VarLength,
    IN                              QWORD   Value
    )
{
    // Check for valid parameters
    if (VarLength >= sizeof(globalVariables[0].VariableName) || VarLength == 0) {
        return STATUS_INVALID_PARAMETER2;
    }

    // Search for the variable in the array
    for (int i = 0; i < MAX_GLOBAL_VARIABLES; ++i) {
        if (strncmp(globalVariables[i].VariableName, VariableName, VarLength) == 0) {
            // Variable found, update its value
            globalVariables[i].Value = Value;

            return STATUS_SUCCESS;
        }
    }

    // Variable not found, add it to the array
    for (int i = 0; i < MAX_GLOBAL_VARIABLES; ++i) {
        if (globalVariables[i].VariableName[0] == '\0') {
            // Found an empty slot, add the variable
            strncpy(globalVariables[i].VariableName, VariableName, VarLength);
            globalVariables[i].Value = Value;

            return STATUS_SUCCESS;
        }
    }

    // No empty slot found
    return STATUS_NO_MORE_OBJECTS;
}

// Userprog 7.
STATUS
SyscallGetGlobalVariable(
    IN_READS_Z(VarLength)           char*   VariableName,
    IN                              DWORD   VarLength,
    OUT                             PQWORD  Value
    )
{
    // Check for valid parameters
    if (VarLength >= sizeof(globalVariables[0].VariableName) || VarLength == 0 || Value == NULL) {
        return STATUS_INVALID_PARAMETER3;
    }

    // Search for the variable in the array
    for (int i = 0; i < MAX_GLOBAL_VARIABLES; ++i) {
        if (strncmp(globalVariables[i].VariableName, VariableName, VarLength) == 0) {
            // Variable found, return its value
            *Value = globalVariables[i].Value;

            return STATUS_SUCCESS;
        }
    }

    // Variable not found
    return STATUS_UNSUCCESSFUL;
}

// Virtual Memory 3. Implement the basic SyscallIdVirtualAlloc system call ignoring the Key parameter.
STATUS
SyscallVirtualAlloc(
    IN_OPT      PVOID                   BaseAddress,
    IN          QWORD                   Size,
    IN          VMM_ALLOC_TYPE          AllocType,
    IN          PAGE_RIGHTS             PageRights,
    IN_OPT      UM_HANDLE               FileHandle,
    IN_OPT      QWORD                   Key,
    OUT         PVOID * AllocatedAddress
     )
{
    UNREFERENCED_PARAMETER(FileHandle);
    UNREFERENCED_PARAMETER(Key);
    
    PPROCESS currentProcess = GetCurrentProcess();
    
    if (currentProcess == NULL) {
        return STATUS_UNSUCCESSFUL;
    }
    
    *AllocatedAddress = VmmAllocRegionEx(
            BaseAddress,
            Size,
            AllocType,
            PageRights,
            FALSE,
            NULL,
            currentProcess->VaSpace,
            currentProcess->PagingData,
            NULL
            );

    return STATUS_SUCCESS;
}

STATUS
SyscallVirtualFree(
    IN          PVOID                   Address,
    _When_(VMM_FREE_TYPE_RELEASE == FreeType, _Reserved_)
     _When_(VMM_FREE_TYPE_RELEASE != FreeType, IN)
     QWORD                   Size,
    IN          VMM_FREE_TYPE           FreeType
     )
{
    VmmFreeRegionEx(
            Address,
            Size,
            FreeType,
            TRUE,
            GetCurrentProcess()->VaSpace,
            GetCurrentProcess()->PagingData
         );
    return STATUS_SUCCESS;
}

// Virtual Memory 7. Implement two new system calls SyscallIdMapZeroPage and SyscallIdUnmapZeroPage:

// The first system call should mark the first page as valid, but not map it, i.e. a #PF should still be generated on NULL access.
STATUS
SyscallMapZeroPage(
    void
)
{
    PPROCESS pProcess = GetCurrentProcess();

    if (pProcess == NULL) {
		return STATUS_UNSUCCESSFUL;
	}

    // Mark the first page as usable
    VmmMarkPageAsUsable(
        pProcess->VaSpace
		);
    return STATUS_SUCCESS;
}

// the first page is no longer valid
STATUS
SyscallUnmapZeroPage(
    void
)
{
    PPROCESS pProcess = GetCurrentProcess();

    if (pProcess == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    // Mark the first page as not usable
    VmmMarkPageAsNotUsable(
        pProcess->VaSpace
    );
    return STATUS_SUCCESS;
}