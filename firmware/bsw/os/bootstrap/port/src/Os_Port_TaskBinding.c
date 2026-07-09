/**
 * @file    Os_Port_TaskBinding.c
 * @brief   Bootstrap binding from configured OS tasks into target port contexts
 * @date    2026-03-13
 */
#include "Os_Internal.h"
#include "Os_Port_TaskBinding.h"

#if defined(PLATFORM_STM32)
#include "Os_Port_Stm32.h"
#elif defined(PLATFORM_STM32L5)
#include "Os_Port_Stm32L5.h"
#elif defined(PLATFORM_TMS570)
#include "Os_Port_Tms570.h"
#endif

StatusType Os_Port_PrepareConfiguredTask(TaskType TaskID, uintptr_t StackTop)
{
    if ((os_is_valid_task(TaskID) == FALSE) || (StackTop == (uintptr_t)0u)) {
        return E_OS_VALUE;
    }

#if defined(PLATFORM_STM32)
    return Os_Port_Stm32_PrepareTaskContext(TaskID, os_task_cfg[TaskID].Entry, StackTop);
#elif defined(PLATFORM_STM32L5)
    return Os_Port_Stm32L5_PrepareTaskContext(TaskID, os_task_cfg[TaskID].Entry, StackTop, (uintptr_t)0u);
#elif defined(PLATFORM_TMS570)
    return Os_Port_Tms570_PrepareTaskContext(TaskID, os_task_cfg[TaskID].Entry, StackTop);
#else
    (void)TaskID;
    (void)StackTop;
    return E_OS_STATE;
#endif
}

StatusType Os_Port_PrepareConfiguredFirstTask(TaskType TaskID, uintptr_t StackTop)
{
    if ((os_is_valid_task(TaskID) == FALSE) || (StackTop == (uintptr_t)0u)) {
        return E_OS_VALUE;
    }

#if defined(PLATFORM_STM32)
    return Os_Port_Stm32_PrepareFirstTask(TaskID, os_task_cfg[TaskID].Entry, StackTop);
#elif defined(PLATFORM_STM32L5)
    return Os_Port_Stm32L5_PrepareFirstTask(TaskID, os_task_cfg[TaskID].Entry, StackTop, (uintptr_t)0u);
#elif defined(PLATFORM_TMS570)
    return Os_Port_Tms570_PrepareFirstTask(TaskID, os_task_cfg[TaskID].Entry, StackTop);
#else
    (void)TaskID;
    (void)StackTop;
    return E_OS_STATE;
#endif
}

StatusType Os_Port_SelectConfiguredTask(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return E_OS_VALUE;
    }

#if defined(PLATFORM_STM32)
    return Os_Port_Stm32_SelectNextTask(TaskID);
#elif defined(PLATFORM_STM32L5)
    return Os_Port_Stm32L5_SelectNextTask(TaskID);
#elif defined(PLATFORM_TMS570)
    return Os_Port_Tms570_SelectNextTask(TaskID);
#else
    (void)TaskID;
    return E_OS_STATE;
#endif
}

void Os_Port_SynchronizeConfiguredTask(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return;
    }

#if defined(PLATFORM_STM32)
    Os_Port_Stm32_SynchronizeCurrentTask(TaskID);
#elif defined(PLATFORM_STM32L5)
    Os_Port_Stm32L5_SynchronizeCurrentTask(TaskID);
#elif defined(PLATFORM_TMS570)
    Os_Port_Tms570_SynchronizeCurrentTask(TaskID);
#else
    (void)TaskID;
#endif
}

StatusType Os_Port_RebuildTaskFrame(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return E_OS_VALUE;
    }

#if defined(PLATFORM_STM32)
    {
        const Os_Port_Stm32_TaskContextType* ctx = Os_Port_Stm32_GetTaskContext(TaskID);
        if ((ctx != (const Os_Port_Stm32_TaskContextType*)0) && (ctx->Prepared == TRUE)) {
            return Os_Port_Stm32_PrepareTaskContext(
                TaskID, os_task_cfg[TaskID].Entry, ctx->StackTop);
        }
        return E_OS_STATE;
    }
#else
    (void)TaskID;
    return E_OS_STATE;
#endif
}

#if defined(UNIT_TEST)
/* Distinguishes a staged RESUME (preempted task's saved context restored
 * as-is) from a staged FRESH dispatch (rebuilt initial frame) so the host
 * mock in Os_Port_CompleteConfiguredDispatch knows whether to invoke the
 * task entry.  On hardware the distinction is physical: PendSV restores
 * whatever frame the staging left in place. */
static boolean os_port_binding_resume_staged = FALSE;
#endif

StatusType Os_Port_RequestConfiguredDispatch(TaskType TaskID)
{
    StatusType status;

    /* On hardware, a task that previously ran and terminated has stale
     * saved context.  Always rebuild the initial frame so PendSV
     * restores a clean entry point. */
    (void)Os_Port_RebuildTaskFrame(TaskID);

    status = Os_Port_SelectConfiguredTask(TaskID);

    if (status != E_OK) {
        return status;
    }

#if defined(UNIT_TEST)
    os_port_binding_resume_staged = FALSE;
#endif
    Os_PortRequestContextSwitch();
    return E_OK;
}

StatusType Os_Port_StageConfiguredResume(TaskType TaskID)
{
    StatusType status;

    /* BRINGUP-6-validated switchback staging: select WITHOUT frame
     * rebuild — the resumed task was preempted and its saved context is
     * live; rebuilding would destroy it (docs/lessons-learned/
     * stm32-bringup-p3.md records when rebuild is required instead). */
    status = Os_Port_SelectConfiguredTask(TaskID);

    if (status != E_OK) {
        return status;
    }

#if defined(UNIT_TEST)
    os_port_binding_resume_staged = TRUE;
#endif
    Os_PortRequestContextSwitch();
    return E_OK;
}

void Os_Port_SuppressTaskSave(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return;
    }

#if defined(PLATFORM_STM32)
    Os_Port_Stm32_SuppressTaskSave(TaskID);
#else
    /* STM32L5/TMS570: the coalescing save-clobber (S-OS-31 FIX-04) has not been
     * reproduced on those ports; no suppression machinery yet. */
    (void)TaskID;
#endif
}

StatusType Os_Port_CompleteConfiguredDispatch(void)
{
#if defined(PLATFORM_STM32)
    const Os_Port_Stm32_StateType* state = Os_Port_Stm32_GetBootstrapState();
    TaskType target;
#if defined(UNIT_TEST)
    boolean resume_staged;
#endif

    if (state->PendSvPending == FALSE) {
        return E_OS_NOFUNC;
    }

#if defined(UNIT_TEST)
    resume_staged = os_port_binding_resume_staged;
    os_port_binding_resume_staged = FALSE;
#endif

    Os_Port_Stm32_PendSvHandler();

#if defined(UNIT_TEST)
    /* A resume lands mid-body in the restored task on hardware: the mock
     * must NOT re-invoke the task entry from the top. */
    if (resume_staged == TRUE) {
        return E_OK;
    }
#endif

    /* On hardware, PendSV restores the task context and branches to its
     * saved PC (Entry for a new task).  In unit tests, simulate this by
     * calling Entry() + post-Entry cleanup for the current kernel task.
     *
     * ThreadX ref: tx_thread_schedule.S -- PendSV pops the thread context
     * and does BX LR to resume execution at the saved PC. */
    target = os_current_task;
    if ((target != INVALID_TASK) &&
        (os_tcb[target].State == RUNNING) &&
        (os_task_cfg[target].Entry != (Os_TaskEntryType)0)) {
        os_task_cfg[target].Entry();

        /* Post-Entry: task completed -- mirror os_dispatch_task cleanup */
        if ((os_current_task == target) &&
            (os_tcb[target].State == RUNNING)) {
            os_complete_running_task();
        }
    }

    return E_OK;
#elif defined(PLATFORM_STM32L5)
    const Os_Port_Stm32L5_StateType* state = Os_Port_Stm32L5_GetBootstrapState();

    if (state->PendSvPending == FALSE) {
        return E_OS_NOFUNC;
    }

    Os_Port_Stm32L5_PendSvHandler();
    return E_OK;
#elif defined(PLATFORM_TMS570)
    const Os_Port_Tms570_StateType* state = Os_Port_Tms570_GetBootstrapState();

    if (state->IrqSchedulerReturnInProgress == TRUE) {
        Os_Port_Tms570_FinishIrqSchedulerReturn();
        return E_OK;
    }

    if (state->FiqSchedulerReturnInProgress == TRUE) {
        Os_Port_Tms570_FinishFiqSchedulerReturn();
        return E_OK;
    }

    if (state->DispatchRequested == FALSE) {
        return E_OS_NOFUNC;
    }

    if (state->IrqContextDepth > 0u) {
        Os_Port_Tms570_IrqContextRestore();
    } else {
        Os_Port_Tms570_IrqContextSave();
        Os_Port_Tms570_IrqContextRestore();
    }
    return E_OK;
#else
    return E_OS_STATE;
#endif
}

boolean Os_Port_IsConfiguredDispatchLive(void)
{
#if defined(PLATFORM_STM32)
    return Os_Port_Stm32_GetBootstrapState()->FirstTaskStarted;
#elif defined(PLATFORM_STM32L5)
    return Os_Port_Stm32L5_GetBootstrapState()->FirstTaskStarted;
#elif defined(PLATFORM_TMS570)
    return Os_Port_Tms570_GetBootstrapState()->FirstTaskStarted;
#else
    return FALSE;
#endif
}

void Os_Port_ObserveConfiguredDispatch(TaskType TaskID)
{
    if (os_is_valid_task(TaskID) == FALSE) {
        return;
    }

#if defined(PLATFORM_STM32)
    Os_Port_Stm32_ObserveKernelDispatch(TaskID);
#elif defined(PLATFORM_STM32L5)
    Os_Port_Stm32L5_ObserveKernelDispatch(TaskID);
#elif defined(PLATFORM_TMS570)
    Os_Port_Tms570_ObserveKernelDispatch(TaskID);
#else
    (void)TaskID;
#endif
}
