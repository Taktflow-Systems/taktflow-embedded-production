/**
 * @file    Os_Scheduler.c
 * @brief   Scheduler and dispatch logic for the OSEK bootstrap kernel
 * @date    2026-03-13
 */
#include "Os_Internal.h"

#if defined(PLATFORM_STM32) || defined(PLATFORM_STM32L5) || defined(PLATFORM_TMS570)
#include "Os_Port.h"
#include "Os_Port_TaskBinding.h"
#endif

uint8 os_isr_cat2_nesting = 0u;
TaskType os_preempted_task_stack[OS_MAX_TASKS];
uint8 os_preempted_task_depth = 0u;

static void os_publish_port_dispatch(TaskType NextTask)
{
#if defined(PLATFORM_STM32) || defined(PLATFORM_STM32L5) || defined(PLATFORM_TMS570)
    Os_Port_ObserveConfiguredDispatch(NextTask);
#else
    (void)NextTask;
#endif
}

static void os_stage_port_dispatch(TaskType PreviousTask, TaskType NextTask)
{
#if defined(PLATFORM_STM32) || defined(PLATFORM_STM32L5) || defined(PLATFORM_TMS570)
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

/**
 * @brief Retire a finished task's TCB state (no current-task advance).
 *
 * Split out of os_complete_running_task for S-OS-31 FIX-10 (memo 8.8): the
 * STM32 termination switchback must retire the terminated task immediately
 * but defer the current-task advance / preempted-stack pop to the PendSV
 * commit (Os_BootstrapCommitDispatch).
 */
static void os_retire_running_task(TaskType CompletedTask)
{
    if (os_tcb[CompletedTask].PendingActivations > 0u) {
        os_tcb[CompletedTask].PendingActivations--;
    }

    if (os_tcb[CompletedTask].PendingActivations > 0u) {
        os_tcb[CompletedTask].State = READY;
        os_tcb[CompletedTask].ReadyStamp = os_ready_stamp_counter++;
    } else {
        os_tcb[CompletedTask].State = SUSPENDED;
        os_tcb[CompletedTask].ReadyStamp = 0u;
    }

    os_tcb[CompletedTask].CurrentPriority = os_task_cfg[CompletedTask].Priority;
    os_tcb[CompletedTask].ResourceCount = 0u;
    os_tcb[CompletedTask].SetEvents = 0u;
    os_tcb[CompletedTask].WaitEvents = 0u;
}

void os_complete_running_task(void)
{
    TaskType completed_task = os_current_task;

    if (completed_task == INVALID_TASK) {
        return;
    }

    os_retire_running_task(completed_task);
    os_restore_preempted_task();
    os_rebuild_ready_bitmap();
}

#if defined(PLATFORM_STM32) || defined(PLATFORM_STM32L5) || defined(PLATFORM_TMS570)
/**
 * @brief   Task-termination switchback (S-OS-31, BRINGUP-6 sequence)
 *
 * @details Called by TerminateTask INSTEAD of the bare
 *          os_complete_running_task when the port PendSV dispatch is live
 *          (Os_Port_IsConfiguredDispatchLive).  On hardware the service
 *          call never returns to the task body: PendSV lands in the
 *          switchback target, and the poisoned initial-frame LR
 *          (0xFFFFFFFF) is never executed.
 *
 *          Wires exactly what the hardware-validated bringup line
 *          (Os_Port_Stm32_Bringup.c BRINGUP-6, 6/6 PASS on STM32G474RE)
 *          did manually after TerminateTask:
 *          - resume target present (preempted-task restore stack top):
 *            port select WITHOUT frame rebuild + context-switch request —
 *            the preempted task's saved context is live, a rebuild would
 *            destroy it;
 *          - a READY task outranking the resume target: fresh dispatch via
 *            Os_Port_RequestConfiguredDispatch (rebuild + select +
 *            request), the same sequence every BRINGUP-5/6 alarm
 *            preemption exercised;
 *          - NEVER a self-redispatch: rebuilding the frame of the stack
 *            still executing this very call corrupts live call frames
 *            (docs/lessons-learned/stm32-bringup-p3.md).  A pending
 *            activation of the terminated task re-enters READY and is
 *            picked up by the next tick's preemption dispatch with a
 *            fresh frame;
 *          - neither: fail closed — report E_OS_STATE and stage nothing
 *            (the parked context starves the watchdog into the safe
 *            state).  Production configurations never reach this branch:
 *            the autostarted idle task never terminates.
 *
 *          PostTaskHook runs first, while the terminating task still owns
 *          the RUNNING identity (OSEK: on leaving RUNNING; the host path
 *          runs it after Entry() returns, which never happens here).
 *
 * @note    Hardware: does not return (parks until PendSV lands in the
 *          target).  UNIT_TEST: returns so the mocked register model can
 *          be asserted.
 */
void os_terminate_switchback(void)
{
    TaskType terminated = os_current_task;
    TaskType resume;
    TaskType ready;

    if (os_post_task_hook != (Os_HookType)0) {
        os_post_task_hook();
    }

    os_stack_monitor_leave_task(terminated);

#if defined(PLATFORM_STM32)
    if (os_commit_dispatch_live == TRUE) {
        /* S-OS-31 FIX-10 (memo 8.8, option A): retire the terminated task's
         * TCB NOW, but do NOT advance os_current_task / pop the preempted
         * stack — that commits inside the PendSV
         * (Os_BootstrapCommitDispatch), paired one-to-one with the physical
         * switch.  os_current_task stays on the terminated (now
         * non-RUNNING) task through the park gap, so a SysTick landing
         * there cannot dispatch a second time (os_maybe_dispatch_preemption
         * declines on State != RUNNING): the 7.2 double-advance race is
         * structurally closed. */
        os_retire_running_task(terminated);
        os_rebuild_ready_bitmap();
    } else {
        os_complete_running_task();
    }
#else
    os_complete_running_task();
#endif

    /* S-OS-31 FIX-04 (F-C): the terminating task's frame is now dead. Suppress
     * its next PendSV save so a re-dispatch that rebuilds a fresh frame in its
     * slot (coalescing race, memo section 7) is not clobbered by a save of the
     * parked/dead context. */
    Os_Port_SuppressTaskSave(terminated);

#if defined(PLATFORM_STM32)
    if (os_commit_dispatch_live == TRUE) {
        resume = (os_preempted_task_depth > 0u)
                     ? os_preempted_task_stack[os_preempted_task_depth - 1u]
                     : INVALID_TASK;
    } else {
        resume = os_current_task;
    }
#else
    resume = os_current_task;
#endif
    ready = os_select_next_ready_task();

    if ((ready != INVALID_TASK) &&
        (ready != terminated) &&
        ((resume == INVALID_TASK) ||
         (os_has_higher_priority(ready, resume) == TRUE))) {
#if defined(PLATFORM_STM32)
        if (os_commit_dispatch_live == TRUE) {
            /* Fresh dispatch of a higher-priority ready task: stage only —
             * the resume-target-in-waiting stays where it already is (on
             * the stack), and the adoption commits in PendSV. */
            (void)Os_Port_RequestConfiguredDispatch(ready);
            Os_Port_ObserveConfiguredDispatch(ready);
        } else
#endif
        {
        /* Fresh dispatch of a higher-priority ready task. */
        if (resume != INVALID_TASK) {
            os_push_preempted_task(resume);
        }

        os_current_task = ready;
        os_tcb[ready].State = RUNNING;
        os_tcb[ready].ReadyStamp = 0u;
        (void)Os_Port_RequestConfiguredDispatch(ready);
        Os_Port_ObserveConfiguredDispatch(ready);

        if (os_task_stack_top_cfg[ready] != (uintptr_t)0u) {
            os_stack_monitor_enter_task(ready, os_task_stack_top_cfg[ready]);
        }

        os_rebuild_ready_bitmap();
        os_dispatch_count++;

        if (os_pre_task_hook != (Os_HookType)0) {
            os_pre_task_hook();
        }
        }
    } else if (resume != INVALID_TASK) {
        /* BRINGUP-6 switchback: resume the preempted task as-is. The port
         * rejects a stale/invalid saved frame (S-OS-31 fail-closed guard,
         * docs/plans/memo-s-os-31-switchback-resume-defect.md); if the resume
         * cannot be staged validly, fail closed rather than let PendSV return
         * into a corrupt frame (INVSTATE HardFault on target). */
        if (Os_Port_StageConfiguredResume(resume) == E_OK) {
            Os_Port_ObserveConfiguredDispatch(resume);
        } else {
            os_report_service_error(OS_DET_API_TERMINATE_TASK, DET_E_PARAM_VALUE,
                                    E_OS_STATE);
        }
    } else {
        os_report_service_error(OS_DET_API_TERMINATE_TASK, DET_E_PARAM_VALUE,
                                E_OS_STATE);
    }

#if !defined(UNIT_TEST)
    for (;;) {
        /* Terminated context: park until PendSV lands in the target.
         * If nothing was staged (fail-closed branch) the watchdog
         * starves and forces the safe state. */
    }
#endif
}

#if defined(PLATFORM_STM32)
/**
 * @brief   Commit the kernel dispatch bookkeeping inside PendSV (FIX-10)
 *
 * @details Called by Os_Port_Stm32_ResolvePendSvTarget AFTER the outgoing
 *          context is physically saved (SavedContextValid TRUE) and the
 *          staged target passed the consume-time gate, with interrupts
 *          disabled.  Performs the push/pop/current-task advance that
 *          os_dispatch_task and os_terminate_switchback used to do
 *          speculatively (memo 8.8, option A):
 *          - the saved (outgoing) task is pushed onto the preempted stack
 *            iff it is still logically RUNNING — a terminated outgoing task
 *            was already retired (SUSPENDED/READY) and must not be pushed;
 *          - an adopted task that is the preempted-stack top is a resume
 *            (pop); anything else is a fresh dispatch (READY -> RUNNING,
 *            stack-monitor enter + PreTaskHook, matching the placements of
 *            the pre-FIX-10 dispatch path).
 *
 *          Push-at-save makes the FIX-10 invariant ("every task on
 *          os_preempted_task_stack has SavedContextValid TRUE") hold by
 *          construction.  On a consume-time gate rejection the port does
 *          NOT call this function: the kernel never advanced, so the
 *          rejected target is simply re-dispatched by a later tick instead
 *          of stranding.
 *
 * @note    PendSV context, interrupts disabled: plain data updates only.
 */
void Os_BootstrapCommitDispatch(TaskType SavedTask, TaskType AdoptedTask)
{
    if ((os_is_valid_task(AdoptedTask) == FALSE) || (AdoptedTask == SavedTask)) {
        return;
    }

    if ((SavedTask != INVALID_TASK) &&
        (os_is_valid_task(SavedTask) == TRUE) &&
        (os_tcb[SavedTask].State == RUNNING)) {
        os_push_preempted_task(SavedTask);
    }

    if ((os_preempted_task_depth > 0u) &&
        (os_preempted_task_stack[os_preempted_task_depth - 1u] == AdoptedTask)) {
        /* Resume of the most recently preempted task (not a fresh dispatch:
         * no rebuild happened, no PreTaskHook, not counted). */
        os_restore_preempted_task();
    } else {
        /* Fresh adoption of a rebuilt initial frame. */
        os_current_task = AdoptedTask;
        os_tcb[AdoptedTask].State = RUNNING;
        os_tcb[AdoptedTask].ReadyStamp = 0u;
        os_dispatch_count++;

        if (os_task_stack_top_cfg[AdoptedTask] != (uintptr_t)0u) {
            os_stack_monitor_enter_task(AdoptedTask, os_task_stack_top_cfg[AdoptedTask]);
        }

        if (os_pre_task_hook != (Os_HookType)0) {
            os_pre_task_hook();
        }
    }

    os_rebuild_ready_bitmap();
}
#endif /* PLATFORM_STM32 */
#endif /* PLATFORM_STM32 || PLATFORM_STM32L5 || PLATFORM_TMS570 */

static void os_dispatch_task(TaskType NextTask)
{
    TaskType previous_task = os_current_task;
    uint8 stack_base_marker = 0u;

#if defined(PLATFORM_STM32)
    /* S-OS-31 FIX-10 (memo 8.8, option A): with the production commit
     * dispatch live, stage only — rebuild+select+request — and let the
     * PendSV commit the kernel push/advance via Os_BootstrapCommitDispatch,
     * paired with the physical save.  This also retires the
     * previous==INVALID Synchronize branch on a live system (the 8.8.1
     * mis-keyed-save leg): every live dispatch now switches contexts
     * through PendSV. */
    if (os_commit_dispatch_live == TRUE) {
        (void)Os_Port_RequestConfiguredDispatch(NextTask);
        os_publish_port_dispatch(NextTask);
        return;
    }
#endif

    if (previous_task != INVALID_TASK) {
        os_push_preempted_task(previous_task);
    }

    os_current_task = NextTask;
    os_tcb[NextTask].State = RUNNING;
    os_tcb[NextTask].ReadyStamp = 0u;
    os_stage_port_dispatch(previous_task, NextTask);
    os_publish_port_dispatch(NextTask);
    /* On ported hardware an ISR-context dispatch is deferred: the task will
     * run on its own configured stack, so &stack_base_marker (an IRQ-stack
     * address) as stack base would make the budget check measure garbage.
     * The synchronous path (host model, direct Entry() call) keeps the
     * marker — there the task really runs on this C stack. */
#if defined(PLATFORM_STM32) || defined(PLATFORM_TMS570)
    if ((Os_PortIsInIsrContext() == TRUE) &&
        (os_task_stack_top_cfg[NextTask] != (uintptr_t)0u)) {
        os_stack_monitor_enter_task(NextTask, os_task_stack_top_cfg[NextTask]);
    } else {
        os_stack_monitor_enter_task(NextTask, (uintptr_t)&stack_base_marker);
    }
#else
    os_stack_monitor_enter_task(NextTask, (uintptr_t)&stack_base_marker);
#endif
    os_rebuild_ready_bitmap();
    os_dispatch_count++;

#if defined(PLATFORM_STM32) || defined(PLATFORM_TMS570)
    /* On hardware, context switch is deferred to PendSV/IRQ exception return.
     * os_stage_port_dispatch already set SelectedNextTask + requested switch.
     * Do NOT call Entry() from ISR context -- PendSV will restore the task.
     *
     * ThreadX ref: tx_timer_interrupt.S -- SysTick never calls task entry,
     * only sets _tx_timer_expired + PENDSVSET.
     *
     * Kernel os_isr_cat2_nesting is already 0 here (decremented by
     * Os_BootstrapExitIsr2 before dispatch).  Os_PortIsInIsrContext checks
     * port-level nesting, decremented later in Os_PortExitIsr2. */
    if (Os_PortIsInIsrContext() == TRUE) {
        return;
    }
#endif

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

#if defined(PLATFORM_STM32)
    /* S-OS-31 FIX-10: with the commit dispatch live os_dispatch_task stages
     * only (os_current_task advances at commit), so the loop condition
     * below would re-stage forever.  Stage at most one dispatch. */
    if (os_commit_dispatch_live == TRUE) {
        return os_dispatch_one();
    }
#endif

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

    if (Os_ServiceProtCheck(OS_ALLOWED_TASK) == FALSE) {
        return E_OS_CALLEVEL;
    }

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

    if (os_tcb[os_current_task].ResourceCount != 0u) {
        os_report_service_error(OS_DET_API_SCHEDULE, DET_E_PARAM_VALUE, E_OS_RESOURCE);
        return E_OS_RESOURCE;
    }

    /* OSEK OS 2.2.3 section 13.2.3.4: Schedule has no influence on a
     * full-preemptive running task, so it succeeds without rescheduling. */
    if (os_is_preemptive_task(os_current_task) == TRUE) {
        return E_OK;
    }

    next_task = os_select_next_ready_task();
    if ((next_task != INVALID_TASK) && (os_has_higher_priority(next_task, os_current_task) == TRUE)) {
        os_dispatch_task(next_task);
    }

    return E_OK;
}
