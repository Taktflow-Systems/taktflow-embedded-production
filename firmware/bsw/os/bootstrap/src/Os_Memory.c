/**
 * @file    Os_Memory.c
 * @brief   Bootstrap memory-access checks for OS-Applications
 * @date    2026-03-13
 */
#include "Os_Internal.h"

static boolean os_range_fits_region(MemoryStartAddressType Address,
                                    MemorySizeType Size,
                                    MemoryStartAddressType RegionStart,
                                    MemorySizeType RegionSize)
{
    MemoryStartAddressType end_address;
    MemoryStartAddressType region_end;

    if ((Size == 0u) || (RegionSize == 0u)) {
        return FALSE;
    }

    end_address = Address + (MemoryStartAddressType)(Size - 1u);
    region_end = RegionStart + (MemoryStartAddressType)(RegionSize - 1u);

    if ((end_address < Address) || (region_end < RegionStart)) {
        return FALSE;
    }

    if ((Address < RegionStart) || (end_address > region_end)) {
        return FALSE;
    }

    return TRUE;
}

AccessType CheckTaskMemoryAccess(TaskType TaskID, MemoryStartAddressType Address, MemorySizeType Size)
{
    ApplicationType owner_app;
    uint8 idx;

    if ((os_is_valid_task(TaskID) == FALSE) || (Address == (MemoryStartAddressType)0u) || (Size == 0u)) {
        return NO_ACCESS;
    }

    if (os_application_count == 0u) {
        return ACCESS;
    }

    owner_app = CheckObjectOwnership(OBJECT_TASK, TaskID);
    if (owner_app == INVALID_OSAPPLICATION) {
        return NO_ACCESS;
    }

    if (os_application_cfg[owner_app].Trusted == TRUE) {
        return ACCESS;
    }

    for (idx = 0u; idx < os_memory_region_count; idx++) {
        if (os_memory_region_cfg[idx].Application != owner_app) {
            continue;
        }

        if (os_range_fits_region(Address,
                                 Size,
                                 os_memory_region_cfg[idx].StartAddress,
                                 os_memory_region_cfg[idx].Size) == TRUE) {
            return ACCESS;
        }
    }

    return NO_ACCESS;
}
