/**
 * @file    Os_Cfg_Types.h
 * @brief   Aggregate production configuration type for the OSEK kernel
 * @date    2026-07-07
 *
 * @details S-OS-10: production (non-UNIT_TEST) configuration path.
 *          `Os_ConfigType` bundles pointers + counts for every per-area
 *          configuration table the kernel accepts. The per-area struct
 *          types are the existing ones from Os.h — no new field shapes.
 *
 *          Entry-point choice: the kernel's state machine is
 *          "configure areas -> Os_Init (runtime reset) -> StartOS".
 *          `Os_Init(void)` and `StartOS(AppModeType)` keep their OSEK
 *          signatures untouched; the production entry is the new
 *          `Os_Configure(const Os_ConfigType*)`, called once before
 *          StartOS. This mirrors exactly how the Os_TestConfigure*
 *          family drives the kernel (each area applied, then Os_Init),
 *          so no kernel state-machine change was needed.
 *
 *          Notes:
 *          - Counter configuration: the kernel derives its single
 *            counter base (AlarmBaseType) from element 0 of the alarm
 *            table (existing kernel convention); there is no separate
 *            counter area.
 *          - `MemProtTasks` is indexed by TaskID (element i configures
 *            task i); entries with RegionCount == 0 are skipped.
 *          - Validation and error mapping: see Os_Configure() in
 *            Os_Core.c.
 *
 * @safety_req SWR-BSW-050: Static priority task scheduling
 * @traces_to  TSR-OS-001, TSR-OS-002
 * @verified_by test_Os_ConfigValidation.c
 *
 * @standard OSEK/VDX, AUTOSAR_CP_OS, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef OS_CFG_TYPES_H
#define OS_CFG_TYPES_H

#include "Os.h"

typedef struct {
    const Os_TaskConfigType* Tasks;
    uint8 TaskCount;
    const Os_ResourceConfigType* Resources;
    uint8 ResourceCount;
    const Os_AlarmConfigType* Alarms;
    uint8 AlarmCount;
    const Os_ScheduleTableConfigType* ScheduleTables;
    uint8 ScheduleTableCount;
    const Os_ApplicationConfigType* Applications;
    uint8 ApplicationCount;
    const Os_TrustedFunctionConfigType* TrustedFunctions;
    uint8 TrustedFunctionCount;
    const Os_IocConfigType* Iocs;
    uint8 IocCount;
    const Os_StackMonitorConfigType* Stacks;
    uint8 StackCount;
    const Os_MemoryRegionConfigType* MemoryRegions;
    uint8 MemoryRegionCount;
    const Os_MemProtTaskConfigType* MemProtTasks;
    uint8 MemProtTaskCount;
    /* S-OS-31 first-task launch seam: physical per-task stack storage.
     * Optional on host builds; on PLATFORM_STM32/STM32L5/TMS570 every
     * configured task MUST have a stack entry (StartOS launches the
     * first task on its own PSP stack through the port). */
    const Os_TaskStackConfigType* TaskStacks;
    uint8 TaskStackCount;
} Os_ConfigType;

/**
 * @brief   Production configuration entry — apply and validate a full
 *          static OS configuration, then reset kernel runtime state.
 * @param   Config  Aggregate configuration (ROM tables); must outlive
 *                  the OS (the kernel copies per-area tables into its
 *                  own storage, but schedule-table expiry-point arrays
 *                  are referenced, not copied).
 * @return  E_OK on success. On failure the kernel is left UNCONFIGURED
 *          (fail-closed: a subsequent StartOS dispatches nothing) and
 *          one of (documented mapping, see Os_Core.c):
 *          - E_OS_VALUE    invalid pointer/field value incl. null task
 *                          entry (FMEA OS-T-05) and out-of-range ceiling
 *          - E_OS_LIMIT    a per-area count exceeds its OS_MAX_* bound
 *          - E_OS_RESOURCE resource ceiling numerically greater (lower
 *                          priority) than an accessor task's priority
 *                          (FMEA OS-R-04)
 *          - E_OS_NOFUNC   no idle task configured (FMEA OS-C-03)
 *          - E_OS_ID       invalid object cross-reference
 * @note    Not thread-safe; call once from the startup context before
 *          StartOS, never from ISR context.
 */
StatusType Os_Configure(const Os_ConfigType* Config);

#endif /* OS_CFG_TYPES_H */
