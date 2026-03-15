/**
 * @file    Os_TaskMap.c
 * @brief   COM/RTE → OSEK task mapping implementation (Phase 5)
 * @date    2026-03-15
 *
 * @details Iterates the statically configured task map and calls each BSW
 *          main function whose MappedTask matches the currently running task.
 *          The task map is provided per-ECU in Os_Cfg or equivalent config.
 *
 * @standard AUTOSAR OS §10, AUTOSAR SchM
 * @copyright Taktflow Systems 2026
 */
#include "Os_TaskMap.h"

/**
 * @brief   Per-ECU task map (weak symbol, overridden by ECU config).
 *
 * Each ECU provides its own task map array and count. The weak defaults
 * here allow the module to link even if no ECU-specific config is provided.
 */
__attribute__((weak)) const Os_TaskMapEntryType os_task_map[] = { { NULL_PTR, NULL_PTR, INVALID_TASK, 0u } };
__attribute__((weak)) const uint8 os_task_map_count = 0u;

void Os_TaskMap_RunMappedFunctions(TaskType TaskID)
{
    uint8 idx;

    for (idx = 0u; idx < os_task_map_count; idx++) {
        if ((os_task_map[idx].MappedTask == TaskID) &&
            (os_task_map[idx].MainFunction != NULL_PTR)) {
            os_task_map[idx].MainFunction();
        }
    }
}
