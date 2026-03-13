/**
 * @file    Os_Alarm.c
 * @brief   Alarm and counter services for the OSEK bootstrap kernel
 * @date    2026-03-13
 */
#include "Os_Internal.h"

#if defined(PLATFORM_STM32) || defined(PLATFORM_TMS570)
#include "Os_Port_TaskBinding.h"
#endif

static TickType os_counter_increment_one(TickType Value)
{
    if (Value >= os_counter_base.maxallowedvalue) {
        return 0u;
    }

    return Value + 1u;
}

static TickType os_counter_add_ticks(TickType Value, TickType Delta)
{
    TickType result = Value;

    while (Delta > 0u) {
        result = os_counter_increment_one(result);
        Delta--;
    }

    return result;
}

static TickType os_counter_ticks_until(TickType Target)
{
    TickType current = os_counter_value;
    TickType ticks = 0u;

    while (current != Target) {
        current = os_counter_increment_one(current);
        ticks++;
    }

    return ticks;
}

static boolean os_alarm_cycle_is_valid(TickType Cycle)
{
    if (Cycle == 0u) {
        return TRUE;
    }

    if (Cycle < os_counter_base.mincycle) {
        return FALSE;
    }

    if (Cycle > os_counter_base.maxallowedvalue) {
        return FALSE;
    }

    return TRUE;
}

static void os_alarm_process_expiry(AlarmType AlarmID)
{
    (void)os_activate_task_internal(os_alarm_cfg[AlarmID].TaskID, FALSE);

    if (os_alarm_cb[AlarmID].Cycle == 0u) {
        os_alarm_cb[AlarmID].Active = FALSE;
        return;
    }

    os_alarm_cb[AlarmID].AlarmTime =
        os_counter_add_ticks(os_counter_value, os_alarm_cb[AlarmID].Cycle);
}

static void os_alarm_process_current_tick(void)
{
    AlarmType idx;

    for (idx = 0u; idx < os_alarm_count; idx++) {
        if ((os_alarm_cb[idx].Active == TRUE) &&
            (os_alarm_cb[idx].AlarmTime == os_counter_value)) {
            os_alarm_process_expiry(idx);
        }
    }
}

static boolean os_bootstrap_ready_task_requires_dispatch(void)
{
    TaskType next_task = os_select_next_ready_task();

    if (next_task == INVALID_TASK) {
        return FALSE;
    }

    if (os_current_task == INVALID_TASK) {
        return TRUE;
    }

    if (os_tcb[os_current_task].State != RUNNING) {
        return TRUE;
    }

    if (os_is_preemptive_task(os_current_task) == FALSE) {
        return FALSE;
    }

    return (boolean)(os_tcb[next_task].CurrentPriority < os_tcb[os_current_task].CurrentPriority);
}

static StatusType os_bootstrap_complete_port_dispatches(void)
{
    StatusType status = E_OS_NOFUNC;

#if defined(PLATFORM_STM32) || defined(PLATFORM_TMS570)
    while (Os_Port_CompleteConfiguredDispatch() == E_OK) {
        status = E_OK;
    }
#endif

    return status;
}

static void os_bootstrap_drain_to_idle(void)
{
    boolean progressed;

    do {
        progressed = FALSE;

        if (os_run_ready_tasks() == E_OK) {
            progressed = TRUE;
        }

        if (os_bootstrap_complete_port_dispatches() == E_OK) {
            progressed = TRUE;
        }
    } while (progressed == TRUE);
}

StatusType GetAlarmBase(AlarmType AlarmID, AlarmBaseRefType Info)
{
    OS_STACK_SAMPLE(OS_DET_API_GET_ALARM_BASE);

    if (Info == NULL_PTR) {
        os_report_service_error(OS_DET_API_GET_ALARM_BASE, DET_E_PARAM_POINTER, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_is_valid_alarm(AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_ALARM_BASE, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_ALARM, AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_ALARM_BASE, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    Info->maxallowedvalue = os_alarm_cfg[AlarmID].MaxAllowedValue;
    Info->ticksperbase = os_alarm_cfg[AlarmID].TicksPerBase;
    Info->mincycle = os_alarm_cfg[AlarmID].MinCycle;
    return E_OK;
}

StatusType GetAlarm(AlarmType AlarmID, TickRefType Tick)
{
    OS_STACK_SAMPLE(OS_DET_API_GET_ALARM);

    if (Tick == NULL_PTR) {
        os_report_service_error(OS_DET_API_GET_ALARM, DET_E_PARAM_POINTER, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_is_valid_alarm(AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_ALARM, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_ALARM, AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_GET_ALARM, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if (os_alarm_cb[AlarmID].Active == FALSE) {
        os_report_service_error(OS_DET_API_GET_ALARM, DET_E_PARAM_VALUE, E_OS_NOFUNC);
        return E_OS_NOFUNC;
    }

    *Tick = os_counter_ticks_until(os_alarm_cb[AlarmID].AlarmTime);
    return E_OK;
}

StatusType SetRelAlarm(AlarmType AlarmID, TickType increment, TickType cycle)
{
    OS_STACK_SAMPLE(OS_DET_API_SET_REL_ALARM);

    if (os_started == FALSE) {
        os_report_service_error(OS_DET_API_SET_REL_ALARM, DET_E_UNINIT, E_OS_STATE);
        return E_OS_STATE;
    }

    if (os_is_valid_alarm(AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_SET_REL_ALARM, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_ALARM, AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_SET_REL_ALARM, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if ((increment == 0u) || (increment > os_counter_base.maxallowedvalue)) {
        os_report_service_error(OS_DET_API_SET_REL_ALARM, DET_E_PARAM_VALUE, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_alarm_cycle_is_valid(cycle) == FALSE) {
        os_report_service_error(OS_DET_API_SET_REL_ALARM, DET_E_PARAM_VALUE, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_alarm_cb[AlarmID].Active == TRUE) {
        os_report_service_error(OS_DET_API_SET_REL_ALARM, DET_E_PARAM_VALUE, E_OS_STATE);
        return E_OS_STATE;
    }

    os_alarm_cb[AlarmID].Active = TRUE;
    os_alarm_cb[AlarmID].AlarmTime = os_counter_add_ticks(os_counter_value, increment);
    os_alarm_cb[AlarmID].Cycle = cycle;
    return E_OK;
}

StatusType SetAbsAlarm(AlarmType AlarmID, TickType start, TickType cycle)
{
    OS_STACK_SAMPLE(OS_DET_API_SET_ABS_ALARM);

    if (os_started == FALSE) {
        os_report_service_error(OS_DET_API_SET_ABS_ALARM, DET_E_UNINIT, E_OS_STATE);
        return E_OS_STATE;
    }

    if (os_is_valid_alarm(AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_SET_ABS_ALARM, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_ALARM, AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_SET_ABS_ALARM, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if (start > os_counter_base.maxallowedvalue) {
        os_report_service_error(OS_DET_API_SET_ABS_ALARM, DET_E_PARAM_VALUE, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_alarm_cycle_is_valid(cycle) == FALSE) {
        os_report_service_error(OS_DET_API_SET_ABS_ALARM, DET_E_PARAM_VALUE, E_OS_VALUE);
        return E_OS_VALUE;
    }

    if (os_alarm_cb[AlarmID].Active == TRUE) {
        os_report_service_error(OS_DET_API_SET_ABS_ALARM, DET_E_PARAM_VALUE, E_OS_STATE);
        return E_OS_STATE;
    }

    os_alarm_cb[AlarmID].Active = TRUE;
    os_alarm_cb[AlarmID].AlarmTime = start;
    os_alarm_cb[AlarmID].Cycle = cycle;
    return E_OK;
}

StatusType CancelAlarm(AlarmType AlarmID)
{
    OS_STACK_SAMPLE(OS_DET_API_CANCEL_ALARM);

    if (os_started == FALSE) {
        os_report_service_error(OS_DET_API_CANCEL_ALARM, DET_E_UNINIT, E_OS_STATE);
        return E_OS_STATE;
    }

    if (os_is_valid_alarm(AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_CANCEL_ALARM, DET_E_PARAM_VALUE, E_OS_ID);
        return E_OS_ID;
    }

    if (os_current_application_has_access(OBJECT_ALARM, AlarmID) == FALSE) {
        os_report_service_error(OS_DET_API_CANCEL_ALARM, DET_E_PARAM_VALUE, E_OS_ACCESS);
        return E_OS_ACCESS;
    }

    if (os_alarm_cb[AlarmID].Active == FALSE) {
        os_report_service_error(OS_DET_API_CANCEL_ALARM, DET_E_PARAM_VALUE, E_OS_NOFUNC);
        return E_OS_NOFUNC;
    }

    os_alarm_cb[AlarmID].Active = FALSE;
    os_alarm_cb[AlarmID].Cycle = 0u;
    os_alarm_cb[AlarmID].AlarmTime = 0u;
    return E_OK;
}

boolean Os_BootstrapProcessCounterTick(void)
{
    os_counter_value = os_counter_increment_one(os_counter_value);
    os_alarm_process_current_tick();
    return os_bootstrap_ready_task_requires_dispatch();
}

static void os_bootstrap_advance_counter(TickType Ticks)
{
    while (Ticks > 0u) {
        (void)Os_BootstrapProcessCounterTick();

        if (os_current_task != INVALID_TASK) {
            (void)os_maybe_dispatch_preemption();
        }

        /* Keep timer-driven bootstrap tests quiescent across host and port models. */
        os_bootstrap_drain_to_idle();

        Ticks--;
    }
}

#if defined(UNIT_TEST)
void Os_TestAdvanceCounter(TickType Ticks)
{
    os_bootstrap_advance_counter(Ticks);
}
#endif
