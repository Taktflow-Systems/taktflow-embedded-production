/**
 * @file    Os_TimingProt.c
 * @brief   AUTOSAR OS timing protection — execution budgets and inter-arrival
 * @date    2026-03-14
 *
 * @details Kernel-layer timing protection per AUTOSAR OS §7.10.
 *          - Per-task execution budget: arm hardware timer on dispatch,
 *            disarm on preemption/termination, ProtectionHook on overrun.
 *          - Per-task inter-arrival time: reject ActivateTask if too soon.
 *          Port hooks handle the actual timer hardware.
 *
 * @standard AUTOSAR OS §7.10, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Os_Internal.h"

#include "Os_Port.h"

/* --- Per-task config and runtime state --- */
Os_TimingProtConfigType os_timing_prot_cfg[OS_MAX_TASKS];
boolean os_timing_prot_configured[OS_MAX_TASKS];
uint32 os_timing_prot_last_activation_us[OS_MAX_TASKS];

/* --- Protection hook (set via Os_TestSetProtectionHook or static config) --- */
Os_ProtectionHookType os_protection_hook = (Os_ProtectionHookType)0;

/**
 * @brief   Configure timing protection for a specific task
 * @param   TaskID  Task to configure
 * @param   Config  Pointer to timing protection config
 */
void Os_TimingProtConfigure(TaskType TaskID, const Os_TimingProtConfigType* Config)
{
    if ((TaskID >= OS_MAX_TASKS) || (Config == NULL_PTR)) {
        return;
    }

    os_timing_prot_cfg[TaskID].ExecutionBudgetUs = Config->ExecutionBudgetUs;
    os_timing_prot_cfg[TaskID].InterArrivalTimeUs = Config->InterArrivalTimeUs;
    os_timing_prot_configured[TaskID] = TRUE;
}

/**
 * @brief   Arm budget timer when a task starts or resumes execution
 * @param   TaskID  Task being dispatched
 * @note    Called from the dispatch path (os_dispatch_task)
 */
void Os_TimingProtStart(TaskType TaskID)
{
    if ((TaskID >= OS_MAX_TASKS) || (os_timing_prot_configured[TaskID] == FALSE)) {
        return;
    }

    if (os_timing_prot_cfg[TaskID].ExecutionBudgetUs == 0u) {
        return;
    }

    Os_PortTimingProtArmBudget(os_timing_prot_cfg[TaskID].ExecutionBudgetUs);
}

/**
 * @brief   Disarm budget timer when a task stops execution
 * @param   TaskID  Task being preempted or terminated
 * @note    Called from preemption and termination paths
 */
void Os_TimingProtStop(TaskType TaskID)
{
    if ((TaskID >= OS_MAX_TASKS) || (os_timing_prot_configured[TaskID] == FALSE)) {
        return;
    }

    if (os_timing_prot_cfg[TaskID].ExecutionBudgetUs == 0u) {
        return;
    }

    Os_PortTimingProtDisarm();
}

/**
 * @brief   Called by port ISR when execution budget timer expires
 * @note    Invokes ProtectionHook(E_OS_PROTECTION_TIME). If no hook or
 *          hook returns PRO_SHUTDOWN, calls ShutdownOS.
 *          PRO_TERMINATETASKISR kills the current task.
 */
void Os_TimingProtBudgetExpired(void)
{
    ProtectionReturnType action;

    /* No current task or task has no budget configured → ignore */
    if (os_current_task == INVALID_TASK) {
        return;
    }

    if ((os_timing_prot_configured[os_current_task] == FALSE) ||
        (os_timing_prot_cfg[os_current_task].ExecutionBudgetUs == 0u)) {
        return;
    }

    if (os_protection_hook == (Os_ProtectionHookType)0) {
        ShutdownOS(E_OS_PROTECTION_TIME);
        return;
    }

    action = os_protection_hook(E_OS_PROTECTION_TIME);

    if (action == PRO_TERMINATETASKISR) {
        os_complete_running_task();
    } else if (action == PRO_TERMINATEAPPL) {
        /* TODO:SC3 — terminate entire OS-Application */
        os_complete_running_task();
    } else {
        ShutdownOS(E_OS_PROTECTION_TIME);
    }
}

/**
 * @brief   Check inter-arrival time constraint before task activation
 * @param   TaskID  Task about to be activated
 * @return  E_OK if allowed, E_OS_PROTECTION_ARRIVAL if too soon
 */
StatusType Os_TimingProtCheckInterArrival(TaskType TaskID)
{
    uint32 elapsed;

    if ((TaskID >= OS_MAX_TASKS) || (os_timing_prot_configured[TaskID] == FALSE)) {
        return E_OK;
    }

    if (os_timing_prot_cfg[TaskID].InterArrivalTimeUs == 0u) {
        return E_OK;
    }

    elapsed = Os_PortTimingProtElapsedUs();

    if (elapsed < os_timing_prot_cfg[TaskID].InterArrivalTimeUs) {
        ProtectionReturnType action;

        if (os_protection_hook == (Os_ProtectionHookType)0) {
            ShutdownOS(E_OS_PROTECTION_ARRIVAL);
            return E_OS_PROTECTION_ARRIVAL;
        }

        action = os_protection_hook(E_OS_PROTECTION_ARRIVAL);

        if (action == PRO_TERMINATETASKISR) {
            os_complete_running_task();
        } else if (action == PRO_TERMINATEAPPL) {
            /* TODO:SC3 — terminate entire OS-Application */
            os_complete_running_task();
        } else {
            ShutdownOS(E_OS_PROTECTION_ARRIVAL);
        }

        return E_OS_PROTECTION_ARRIVAL;
    }

    /* Record activation time */
    os_timing_prot_last_activation_us[TaskID] = elapsed;
    return E_OK;
}

/**
 * @brief   Reset all timing protection state
 * @note    Called from os_reset_runtime_state
 */
void Os_TimingProtReset(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_TASKS; idx++) {
        os_timing_prot_cfg[idx].ExecutionBudgetUs = 0u;
        os_timing_prot_cfg[idx].InterArrivalTimeUs = 0u;
        os_timing_prot_configured[idx] = FALSE;
        os_timing_prot_last_activation_us[idx] = 0u;
    }

    Os_PortTimingProtDisarm();
}

/* ================================================================== *
 * Generic UNIT_TEST stubs when no platform port is linked             *
 * ================================================================== */
#if defined(UNIT_TEST) && !defined(PLATFORM_STM32) && !defined(PLATFORM_TMS570)

uint32 os_test_tp_armed_budget_us = 0u;
boolean os_test_tp_armed = FALSE;
uint32 os_test_tp_elapsed_us = 0u;

void Os_PortTimingProtArmBudget(uint32 BudgetUs)
{
    os_test_tp_armed_budget_us = BudgetUs;
    os_test_tp_armed = TRUE;
}

void Os_PortTimingProtDisarm(void)
{
    os_test_tp_armed = FALSE;
    os_test_tp_armed_budget_us = 0u;
}

uint32 Os_PortTimingProtElapsedUs(void)
{
    return os_test_tp_elapsed_us;
}

#endif /* UNIT_TEST && !PLATFORM_STM32 && !PLATFORM_TMS570 */
