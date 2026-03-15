/**
 * @file    Os_MemProt.c
 * @brief   AUTOSAR OS memory protection — MPU region management per task
 * @date    2026-03-14
 *
 * @details Kernel-layer memory protection per AUTOSAR OS §7.10 (SC3).
 *          - Per-task MPU region configuration with validation
 *          - Task switch reprograms MPU via port hooks
 *          - Fault handler dispatches to ProtectionHook
 *          Port hooks handle actual MPU hardware programming.
 *
 * @standard AUTOSAR OS §7.10, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Os_Internal.h"

#include "Os_Port.h"

/* --- Per-task config and runtime state --- */
Os_MemProtTaskConfigType os_mem_prot_task_cfg[OS_MAX_TASKS];
boolean os_mem_prot_configured[OS_MAX_TASKS];

/**
 * @brief   Check if a value is a power of 2
 * @param   Value  Value to check (must be > 0)
 * @return  TRUE if power of 2
 */
static boolean os_is_power_of_2(uint32 Value)
{
    if (Value == 0u) {
        return FALSE;
    }

    return ((Value & (Value - 1u)) == 0u) ? TRUE : FALSE;
}

/**
 * @brief   Validate a single MPU region descriptor
 * @param   Region  Pointer to region to validate
 * @return  TRUE if region is valid for MPU programming
 * @note    MPU requires: size >= 32, size is power-of-2, base aligned to size
 */
static boolean os_memprot_validate_region(const Os_MemProtRegionType* Region)
{
    if (Region->Size < OS_MEMPROT_MIN_REGION_SIZE) {
        return FALSE;
    }

    if (os_is_power_of_2(Region->Size) == FALSE) {
        return FALSE;
    }

    /* Base address must be aligned to region size */
    if ((Region->BaseAddress & (MemoryStartAddressType)(Region->Size - 1u)) != 0u) {
        return FALSE;
    }

    if (Region->Access > OS_MEMPROT_RWX) {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief   Initialize memory protection subsystem
 * @note    Called from Os_Init. Enables MPU via port with background region.
 */
void Os_MemProtInit(void)
{
    Os_PortMemProtInit();
}

/**
 * @brief   Configure MPU regions for a specific task
 * @param   TaskID   Task to configure
 * @param   Regions  Array of region descriptors
 * @param   Count    Number of regions
 * @return  E_OK on success, E_OS_ID for invalid task, E_OS_VALUE for invalid regions
 */
StatusType Os_MemProtConfigureTask(TaskType TaskID, const Os_MemProtRegionType* Regions, uint8 Count)
{
    uint8 idx;

    if (TaskID >= OS_MAX_TASKS) {
        return E_OS_ID;
    }

    if (Count > OS_MEMPROT_MAX_TASK_REGIONS) {
        return E_OS_VALUE;
    }

    if ((Count > 0u) && (Regions == NULL_PTR)) {
        return E_OS_VALUE;
    }

    /* Validate each region */
    for (idx = 0u; idx < Count; idx++) {
        if (os_memprot_validate_region(&Regions[idx]) == FALSE) {
            return E_OS_VALUE;
        }
    }

    /* Store config */
    for (idx = 0u; idx < Count; idx++) {
        os_mem_prot_task_cfg[TaskID].Regions[idx].BaseAddress = Regions[idx].BaseAddress;
        os_mem_prot_task_cfg[TaskID].Regions[idx].Size = Regions[idx].Size;
        os_mem_prot_task_cfg[TaskID].Regions[idx].Access = Regions[idx].Access;
    }
    os_mem_prot_task_cfg[TaskID].RegionCount = Count;
    os_mem_prot_configured[TaskID] = TRUE;

    return E_OK;
}

/**
 * @brief   Reprogram MPU for the given task
 * @param   TaskID  Task being switched to
 * @note    Called from context switch path. If task has no MPU config,
 *          loads zero regions (privileged background region remains).
 */
void Os_MemProtSwitchTask(TaskType TaskID)
{
    if (TaskID >= OS_MAX_TASKS) {
        return;
    }

    if (os_mem_prot_configured[TaskID] == FALSE) {
        /* No MPU config — load zero regions (background region only) */
        Os_PortMemProtConfigureRegions(NULL_PTR, 0u);
        return;
    }

    Os_PortMemProtConfigureRegions(
        os_mem_prot_task_cfg[TaskID].Regions,
        os_mem_prot_task_cfg[TaskID].RegionCount);
}

/**
 * @brief   Called by port fault ISR on memory access violation
 * @param   FaultAddress  Address that caused the fault
 * @note    Invokes ProtectionHook(E_OS_PROTECTION_MEMORY). If no hook or
 *          hook returns PRO_SHUTDOWN, calls ShutdownOS.
 *          PRO_TERMINATETASKISR kills the current task.
 */
void Os_MemProtFaultHandler(uintptr_t FaultAddress)
{
    ProtectionReturnType action;

    (void)FaultAddress;

    /* No current task — nothing to protect against */
    if (os_current_task == INVALID_TASK) {
        return;
    }

    if (os_protection_hook == (Os_ProtectionHookType)0) {
        ShutdownOS(E_OS_PROTECTION_MEMORY);
        return;
    }

    action = os_protection_hook(E_OS_PROTECTION_MEMORY);

    if (action == PRO_TERMINATETASKISR) {
        os_complete_running_task();
    } else if (action == PRO_TERMINATEAPPL) {
        /* TODO:SC3 — terminate entire OS-Application */
        os_complete_running_task();
    } else {
        ShutdownOS(E_OS_PROTECTION_MEMORY);
    }
}

/**
 * @brief   Reset all memory protection state
 * @note    Called from os_reset_runtime_state
 */
void Os_MemProtReset(void)
{
    uint8 idx;
    uint8 r;

    for (idx = 0u; idx < OS_MAX_TASKS; idx++) {
        os_mem_prot_configured[idx] = FALSE;
        os_mem_prot_task_cfg[idx].RegionCount = 0u;
        for (r = 0u; r < OS_MEMPROT_MAX_TASK_REGIONS; r++) {
            os_mem_prot_task_cfg[idx].Regions[r].BaseAddress = 0u;
            os_mem_prot_task_cfg[idx].Regions[r].Size = 0u;
            os_mem_prot_task_cfg[idx].Regions[r].Access = OS_MEMPROT_NONE;
        }
    }
}

/* ================================================================== *
 * Generic UNIT_TEST stubs when no platform port is linked             *
 * ================================================================== */
#if defined(UNIT_TEST) && !defined(PLATFORM_STM32) && !defined(PLATFORM_TMS570)

Os_MemProtRegionType os_test_mp_loaded_regions[OS_MEMPROT_MAX_TASK_REGIONS];
uint8 os_test_mp_loaded_count = 0u;
boolean os_test_mp_privileged = FALSE;
boolean os_test_mp_mpu_enabled = FALSE;

void Os_PortMemProtInit(void)
{
    os_test_mp_mpu_enabled = TRUE;
}

void Os_PortMemProtConfigureRegions(const Os_MemProtRegionType* Regions, uint8 Count)
{
    uint8 idx;

    os_test_mp_loaded_count = Count;
    for (idx = 0u; idx < Count; idx++) {
        os_test_mp_loaded_regions[idx].BaseAddress = Regions[idx].BaseAddress;
        os_test_mp_loaded_regions[idx].Size = Regions[idx].Size;
        os_test_mp_loaded_regions[idx].Access = Regions[idx].Access;
    }
}

void Os_PortMemProtEnablePrivileged(void)
{
    os_test_mp_privileged = TRUE;
}

void Os_PortMemProtEnableUnprivileged(void)
{
    os_test_mp_privileged = FALSE;
}

#endif /* UNIT_TEST && !PLATFORM_STM32 && !PLATFORM_TMS570 */
