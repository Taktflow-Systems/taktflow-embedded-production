/**
 * @file    Os_Scheduler.c
 * @brief   Scheduler and dispatch logic for the OSEK bootstrap kernel
 * @date    2026-03-13
 */
#include "Os_Internal.h"

#if defined(PLATFORM_STM32) || defined(PLATFORM_TMS570)
#include "Os_Port_TaskBinding.h"
#endif

uint8 os_isr_cat2_nesting = 0u;
TaskType os_preempted_task_stack[OS_MAX_TASKS];
uint8 os_preempted_task_depth = 0u;

static void os_publish_port_dispatch(TaskType NextTask)
{
#if defined(PLATFORM_STM32) || defined(PLATFORM_TMS570)
    Os_Port_ObserveConfiguredDispatch(NextTask);
#else
    (void)NextTask;
#endif
}

static void os_stage_port_dispatch(TaskType PreviousTask, TaskType NextTask)
{
#if defined(PLATFORM_STM32) || defined(PLATFORM_TMS570)
    if (PreviousTask == INVALID_TASK) {
        Os_Port_SynchronizeConfiguredTask(NextTask);
    } else {
        (void)Os_Port_RequestConfiguredDispatch(NextTask);
    }
#else
    (void)PreviousTask;
    (void)NextTask;
#endif
}

static boolean os_has_higher_priority(TaskType CandidateTask, TaskType CurrentTask)
{
    return (boolean)(os_tcb[CandidateTask].CurrentPriority < os_tcb[CurrentTask].CurrentPriority);
}

static void os_push_preempted_task(TaskType TaskID)
{
    if (os_preempted_task_depth < OS_MAX_TASKS) {
        os_preempted_task_stack[os_preempted_task_depth] = TaskID;
        os_preempted_task_depth++;
    }

    os_tcb[TaskID].State = READY;
    os_tcb[TaskID].ReadyStamp = os_ready_stamp_counter++;
}

static void os_restore_preempted_task(void)
{
    TaskType restored_task;

    if (os_preempted_task_depth == 0u) {
        os_current_task = INVALID_TASK;
        return;
    }

    os_preempted_task_depth--;
    restored_task = os_preempted_task_stack[os_preempted_task_depth];
    os_current_task = restored_task;
    os_tcb[restored_task].State = RUNNING;
    os_tcb[restored_task].ReadyStamp = 0u;
}

TaskType os_select_next_ready_task(void)
{
    uint8 priority;

    for (priority = 0u; priority < OS_MAX_PRIORITIES; priority++) {
        uint32 ready_mask = ((uint32)1u << priority);

        if ((os_ready_bitmap & ready_mask) != 0u) {
            TaskType selected_task = INVALID_TASK;
            uint32 best_stamp = 0xFFFFFFFFu;
            uint8 idx;

            for (idx = 0u; idx < os_task_count; idx++) {
                if ((os_tcb[idx].State == READY) &&
                    (os_tcb[idx].CurrentPriority == priority) &&
                    (os_tcb[idx].ReadyStamp < best_stamp)) {
                    selected_task = idx;
                    best_stamp = os_tcb[idx].ReadyStamp;
                }
            }

            if (selected_task != INVALID_TASK) {
                return selected_task;
            }
        }
    }

    return INVALID_TASK;
}

void os_complete_running_task(void)
{
    TaskType completed_task = os_current_task;

    if (completed_task == INVALID_TASK) {
        return;
    }

    if (os_tcb[completed_task].PendingActivations > 0u) {
        os_tcb[completed_task].PendingActivations--;
    }

    if (os_tcb[completed_task].PendingActivations > 0u) {
        os_tcb[completed_task].State = READY;
        os_tcb[completed_task].ReadyStamp = os_ready_stamp_counter++;
    } else {
        os_tcb[completed_task].State = SUSPENDED;
        os_tcb[completed_task].ReadyStamp = 0u;
    }

    os_tcb[completed_task].CurrentPriority = os_task_cfg[completed_task].Priority;
    os_tcb[completed_task].ResourceCount = 0u;
    os_tcb[completed_task].SetEvents = 0u;
    os_tcb[completed_task].WaitEvents = 0u;

    os_restore_preempted_task();
    os_rebuild_ready_bitmap();
}

static void os_dispatch_task(TaskType NextTask)
{
    TaskType previous_task = os_current_task;
    uint8 stack_base_marker = 0u;

    if (previous_task != INVALID_TASK) {
        os_push_preempted_task(previous_task);
    }

    os_current_task = NextTask;
    os_tcb[NextTask].State = RUNNING;
    os_tcb[NextTask].ReadyStamp = 0u;
    os_stage_port_dispatch(previous_task, NextTask);
    os_publish_port_dispatch(NextTask);
    os_stack_monitor_enter_task(NextTask, (uintptr_t)&stack_base_marker);
    os_rebuild_ready_bitmap();
    os_dispatch_count++;

    if (os_pre_task_hook != (Os_HookType)0) {
        os_pre_task_hook();
    }

    os_task_cfg[NextTask].Entry();

    if (os_post_task_hook != (Os_HookType)0) {
        os_post_task_hook();
    }

    os_stack_monitor_leave_task(NextTask);

    if ((os_current_task == NextTask) && (os_tcb[NextTask].State == RUNNING)) {
        os_complete_running_task();
    } else if ((os_current_task == INVALID_TASK) && (os_tcb[NextTask].State == WAITING)) {
        os_restore_preempted_task();
        os_rebuild_ready_bitmap();
    }
}

StatusType os_dispatch_one(void)
{
    TaskType next_task = os_select_next_ready_task();

    if (next_task == INVALID_TASK) {
        return E_OS_NOFUNC;
    }

    os_dispatch_task(next_task);
    return E_OK;
}

StatusType os_run_ready_tasks(void)
{
    StatusType status = E_OS_NOFUNC;

    while ((os_shutdown_requested == FALSE) &&
           (os_current_task == INVALID_TASK) &&
           (os_dispatch_one() == E_OK)) {
        status = E_OK;
    }

    return status;
}

StatusType os_maybe_dispatch_preemption(void)
{
    TaskType next_task;

    if (os_isr_cat2_nesting != 0u) {
        return E_OS_NOFUNC;
    }

    if (os_current_task == INVALID_TASK) {
        return E_OS_NOFUNC;
    }

    if (os_tcb[os_current_task].State != RUNNING) {
        return E_OS_NOFUNC;
    }

    if (os_is_preemptive_task(os_current_task) == FALSE) {
        return E_OS_NOFUNC;
    }

    next_task = os_select_next_ready_task();
    if (next_task == INVALID_TASK) {
        return E_OS_NOFUNC;
    }

    if (os_has_higher_priority(next_task, os_current_task) == FALSE) {
        return E_OS_NOFUNC;
    }

    os_dispatch_task(next_task);
    return E_OK;
}

StatusType Schedule(void)
{
    TaskType next_task;
    OS_STACK_SAMPLE(OS_DET_API_SCHEDULE);

    if (os_started == FALSE) {
        os_report_service_error(OS_DET_API_SCHEDULE, DET_E_UNINIT, E_OS_STATE);
        return E_OS_STATE;
    }

    if (os_current_task == INVALID_TASK) {
        return os_run_ready_tasks();
    }

    if (os_tcb[os_current_task].State != RUNNING) {
        os_report_service_error(OS_DET_API_SCHEDULE, DET_E_PARAM_VALUE, E_OS_CALLEVEL);
        return E_OS_CALLEVEL;
    }

    if (os_is_preemptive_task(os_current_task) == TRUE) {
        os_report_service_error(OS_DET_API_SCHEDULE, DET_E_PARAM_VALUE, E_OS_CALLEVEL);
        return E_OS_CALLEVEL;
    }

    if (os_tcb[os_current_task].ResourceCount != 0u) {
        os_report_service_error(OS_DET_API_SCHEDULE, DET_E_PARAM_VALUE, E_OS_RESOURCE);
        return E_OS_RESOURCE;
    }

    next_task = os_select_next_ready_task();
    if ((next_task != INVALID_TASK) && (os_has_higher_priority(next_task, os_current_task) == TRUE)) {
        os_dispatch_task(next_task);
    }

    return E_OK;
}
