/**
 * @file    Os_TaskMap.c
 * @brief   COM/RTE → OSEK task mapping implementation (Phase 5)
 * @date    2026-03-15
 *
 * @details Iterates the registered task map and calls each BSW main
 *          function whose MappedTask matches the currently running task.
 *          Each migrated ECU main registers its table once via
 *          Os_TaskMap_SetTable() before StartOS (S-OS-22 cutover
 *          pattern); until then the map is empty and dispatch is a
 *          no-op.
 *
 *          Design note: the original weak-symbol override pattern
 *          (weak-empty os_task_map[] in this TU, strong table per ECU)
 *          was replaced by this registration API. With definition and
 *          consumer in one TU the compiler constant-folded the weak
 *          count of 0 into the dispatch loop (observed at -Og/-O2,
 *          arm-none-eabi-gcc 13.3.0) so a strong override was silently
 *          never read, and MinGW COFF hosts do not link weak data at
 *          all. Regression guard: test/test_Os_TaskMap.c (built -O2).
 *
 * @standard AUTOSAR OS §10, AUTOSAR SchM
 * @copyright Taktflow Systems 2026
 */
#include "Os_TaskMap.h"

static const Os_TaskMapEntryType* os_task_map_table = NULL_PTR;
static uint8 os_task_map_count = 0u;

void Os_TaskMap_SetTable(const Os_TaskMapEntryType* Table, uint8 Count)
{
    if ((Table == NULL_PTR) || (Count == 0u)) {
        /* Fail-closed: reject inconsistent registration -> empty map */
        os_task_map_table = NULL_PTR;
        os_task_map_count = 0u;
        return;
    }

    os_task_map_table = Table;
    os_task_map_count = Count;
}

void Os_TaskMap_RunMappedFunctions(TaskType TaskID)
{
    uint8 idx;

    if (os_task_map_table == NULL_PTR) {
        return;
    }

    for (idx = 0u; idx < os_task_map_count; idx++) {
        if ((os_task_map_table[idx].MappedTask == TaskID) &&
            (os_task_map_table[idx].MainFunction != NULL_PTR)) {
            os_task_map_table[idx].MainFunction();
        }
    }
}
