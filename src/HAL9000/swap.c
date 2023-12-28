#include "HAL9000.h"
#include "bitmap.h"
#include "iomu.h"
#include "mmu.h"
#include "process_internal.h"
#include "synch.h"
#include "swap.h"

typedef struct _SWAP_SYSTEM_DATA
{
    DWORD           SwapMemSlots;

    LOCK            SwapBitmapLock;

    _Guarded_by_(SwapBitmapLock)
        BITMAP          SwapAvailabilityBitmap;
    _Guarded_by_(SwapBitmapLock)
        PVOID           SwapBitmapData;
} SWAP_SYSTEM_DATA, *PSWAP_SYSTEM_DATA;

static SWAP_SYSTEM_DATA m_swapSyatemData;

void
SwapSystemPreinit(
    void
)
{
    memzero(&m_swapSyatemData, sizeof(SWAP_SYSTEM_DATA));

    LockInit(&m_swapSyatemData.SwapBitmapLock);
}

void
SwapSystemInit(
    void
    )
{
    INTR_STATE      oldState;
    QWORD           SwapFileSize;
    DWORD           SwapBitmapSize = 0;

    SwapFileSize = IomuGetSwapFileSize();
    LockAcquire(&m_swapSyatemData.SwapBitmapLock, &oldState);

    SwapBitmapSize = BitmapPreinit(&m_swapSyatemData.SwapAvailabilityBitmap, (DWORD)SwapFileSize / PAGE_SIZE);
    m_swapSyatemData.SwapBitmapData
        = ExAllocatePoolWithTag(PoolAllocatePanicIfFail, SwapBitmapSize, HEAP_IOMU_TAG, 0);
    ASSERT(NULL != m_swapSyatemData.SwapBitmapData);
    BitmapInit(&m_swapSyatemData.SwapAvailabilityBitmap, m_swapSyatemData.SwapBitmapData);

    LockRelease(&m_swapSyatemData.SwapBitmapLock, oldState);
    LOG("SWAP system initialized with success!\n");
}

STATUS
SwapAllocSwapPage(
    OUT     PSWAP_ADDRESS      SwapAddress
)
{
    DWORD           SwapIndex;
    INTR_STATE      oldState;
    QWORD           SwapFileSize;

    ASSERT(NULL != SwapAddress);

    SwapFileSize = IomuGetSwapFileSize();
    LockAcquire(&m_swapSyatemData.SwapBitmapLock, &oldState);
    SwapIndex = BitmapScanAndFlip(&m_swapSyatemData.SwapAvailabilityBitmap, 1, FALSE);
    if (SwapIndex >= MAX_DWORD || SwapIndex < 0 || SwapIndex >(SwapFileSize / PAGE_SIZE))
    {
        LockRelease(&m_swapSyatemData.SwapBitmapLock, oldState);
        LOG_ERROR("Swap file is full!\n");
        return STATUS_UNSUCCESSFUL;
    }
    LockRelease(&m_swapSyatemData.SwapBitmapLock, oldState);

    *SwapAddress = _SwapIndexToAddr(SwapIndex);
    return STATUS_SUCCESS;
}

void
SwapFreeSwapPage(
    IN     SWAP_ADDRESS      SwapAddress
)
{
    INTR_STATE oldState;
    DWORD SwapPageIndex = _SwapAddrToIndex(SwapAddress);

    LockAcquire(&m_swapSyatemData.SwapBitmapLock, &oldState);
    BitmapClearBit(&m_swapSyatemData.SwapAvailabilityBitmap, SwapPageIndex);
    LockRelease(&m_swapSyatemData.SwapBitmapLock, oldState);
}


STATUS
SwapOut(
    IN          PVOID               VirtualAddress,
    OUT         PSWAP_ADDRESS       SwapAddress
    )
{
    STATUS          status;
    SWAP_ADDRESS    SwapPageAddress;
    PFILE_OBJECT    SwapFile;
    QWORD           BytesWritenInSwap;

    if (!IsAddressAligned(VirtualAddress, PAGE_SIZE))
    {
        LOG_ERROR("Address 0x%x not page aligned\n", VirtualAddress);
        return STATUS_INVALID_PARAMETER1;
    }
    LOG("Address 0x%x is aligned\n", VirtualAddress);

    status = MmuIsBufferValid(VirtualAddress, PAGE_SIZE, PAGE_RIGHTS_READ, GetCurrentProcess());
    if (!SUCCEEDED(status))
    {
        LOG_ERROR("No rights to read address 0x%x\n", VirtualAddress);
        return STATUS_INVALID_PARAMETER1;
    }
    LOG("Address 0x%x is a valid READ buffer\n", VirtualAddress);

    if (NULL == SwapAddress)
    {
        return STATUS_INVALID_PARAMETER2;
    }

    status = SwapAllocSwapPage(&SwapPageAddress);
    if (!SUCCEEDED(status))
    {
        return STATUS_UNSUCCESSFUL;
    }
    LOG("Allocated swap page (0x%x) with success\n", SwapPageAddress);

    SwapFile = IomuGetSwapFile();
    if (NULL == SwapFile)
    {
        LOG_ERROR("No swap file could be retreived\n");
        goto __ret_with_free_swap_page;
    }
    LOG("Obtained swap file from iomu\n");
    LOG("Trying to write swap file from offset %u until %u out of %u\n", SwapPageAddress, SwapPageAddress + PAGE_SIZE, IomuGetSwapFileSize());

    status = IoWriteFile(SwapFile, PAGE_SIZE, &SwapPageAddress, VirtualAddress, &BytesWritenInSwap);
    LOG("Returned from IoWriteFile\n");
    if (!SUCCEEDED(status))
    {
        LOG_ERROR("Failed to write in swap!\n");
        goto __ret_with_free_swap_page;
    }
    if (PAGE_SIZE != BytesWritenInSwap)
    {
        LOG_ERROR("Wrote %u bytes instead of %u", BytesWritenInSwap, PAGE_SIZE);
        goto __ret_with_free_swap_page;
    }
    LOG("Succesfully wrote %u bytes out of %u bytes pf page\n", BytesWritenInSwap, PAGE_SIZE);

    *SwapAddress = SwapPageAddress;
    return STATUS_SUCCESS;

__ret_with_free_swap_page:
    SwapFreeSwapPage(SwapPageAddress);
    return STATUS_UNSUCCESSFUL;
}