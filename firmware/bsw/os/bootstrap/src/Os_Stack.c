/**
 * @file    Os_Stack.c
 * @brief   Bootstrap stack-budget monitoring helpers
 * @date    2026-03-13
 */
#include "Os_Internal.h"

static uint16 os_stack_usage_bytes(uintptr_t BaseAddress, uintptr_t CurrentAddress)
{
    uintptr_t delta;

    if ((BaseAddress == (uintptr_t)0u) || (CurrentAddress == (uintptr_t)0u)) {
        return 0u;
    }

    if (BaseAddress >= CurrentAddress) {
        delta = BaseAddress - CurrentAddress;
    } else {
        delta = CurrentAddress - BaseAddress;
    }

    if (delta > 0xFFFFu) {
        return 0xFFFFu;
    }

    return (uint16)delta;
}

void os_stack_monitor_enter_task(TaskType TaskID, uintptr_t BaseAddress)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return;
    }

    os_tcb[TaskID].StackBaseAddr = BaseAddress;
}

void os_stack_monitor_leave_task(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return;
    }

    os_tcb[TaskID].StackBaseAddr = (uintptr_t)0u;
}

void os_stack_monitor_sample(uintptr_t CurrentAddress, uint8 ApiId)
{
    TaskType current_task;
    uint16 budget;
    uint16 usage;

    current_task = os_current_task;
    if ((current_task == INVALID_TASK) || (os_is_valid_task(current_task) == FALSE)) {
        return;
    }

    if (os_tcb[current_task].State != RUNNING) {
        return;
    }

    usage = os_stack_usage_bytes(os_tcb[current_task].StackBaseAddr, CurrentAddress);
    if (usage > os_tcb[current_task].PeakStackUsage) {
        os_tcb[current_task].PeakStackUsage = usage;
    }

    budget = os_stack_budget_cfg[current_task];
    if ((budget == 0u) || (os_tcb[current_task].StackViolation == TRUE) || (usage <= budget)) {
        return;
    }

    os_tcb[current_task].StackViolation = TRUE;
    os_report_service_error(ApiId, DET_E_PARAM_VALUE, E_OS_LIMIT);
    ShutdownOS(E_OS_LIMIT);
}
