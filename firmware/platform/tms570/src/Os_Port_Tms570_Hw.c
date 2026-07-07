/**
 * @file    Os_Port_Tms570_Hw.c
 * @brief   Hardware port bridge — BSW-side functions for TMS570 OS integration
 * @date    2026-03-14
 *
 * @details Implements the functions that the kernel and Os_Port_TaskBinding.c
 *          call on the PLATFORM_TMS570 path.  Uses BSW headers only (no
 *          HALCoGen) to avoid the boolean typedef conflict.  Delegates
 *          hardware operations to Os_Port_Tms570_Target.c via extern.
 *
 *          NOT compiled in UNIT_TEST builds — the model tests use
 *          Os_Port_Tms570.c instead (full state machine).
 *
 * @note    Safety level: bootstrap — not production
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#ifdef PLATFORM_TMS570
#ifndef UNIT_TEST

#include "Os_Port_Tms570.h"   /* BSW types via Os_Port.h → Os.h → Std_Types.h */
#include "Os_Internal.h"      /* os_select_next_ready_task, os_task_cfg */

/* ====================================================================
 * Extern declarations for Target.c functions (HALCoGen-side).
 * Types are ABI-compatible: uint8 = unsigned char in both domains.
 * ==================================================================== */

extern uint8 Os_Port_Tms570_TargetPrepareTask(
    uint8 taskId, void (*entry)(void), uintptr_t stackTop);

extern uint8 Os_Port_Tms570_TargetPrepareFirstTask(
    uint8 taskId, void (*entry)(void), uintptr_t stackTop);

extern void Os_Port_Tms570_TargetSetNextTask(uint8 taskId);

/* Assembly entry point for first-task launch */
extern void Os_Port_Tms570_StartFirstTaskAsm(void);

/* ====================================================================
 * Static state — minimal subset of Os_Port_Tms570_StateType for
 * GetBootstrapState.  The binding layer checks a few boolean fields
 * in CompleteConfiguredDispatch; on hardware these paths are not used
 * (context switches happen via RTI ISR assembly, not deferred dispatch).
 * ==================================================================== */

static Os_Port_Tms570_StateType os_hw_state;

/* ====================================================================
 * Functions called by Os_Port_TaskBinding.c
 * ==================================================================== */

StatusType Os_Port_Tms570_PrepareTaskContext(
    TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop)
{
    return (StatusType)Os_Port_Tms570_TargetPrepareTask(
        TaskID, Entry, StackTop);
}

StatusType Os_Port_Tms570_PrepareFirstTask(
    TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop)
{
    StatusType status;

    status = (StatusType)Os_Port_Tms570_TargetPrepareFirstTask(
        TaskID, Entry, StackTop);

    if (status == E_OK) {
        os_hw_state.FirstTaskPrepared = TRUE;
        os_hw_state.FirstTaskTaskID = TaskID;
    }

    return status;
}

StatusType Os_Port_Tms570_SelectNextTask(TaskType TaskID)
{
    os_hw_state.SelectedNextTask = TaskID;
    Os_Port_Tms570_TargetSetNextTask(TaskID);
    return E_OK;
}

void Os_Port_Tms570_SynchronizeCurrentTask(TaskType TaskID)
{
    os_hw_state.CurrentTask = TaskID;
    os_hw_state.FirstTaskStartInProgress = TRUE;
}

void Os_Port_Tms570_ObserveKernelDispatch(TaskType TaskID)
{
    os_hw_state.LastObservedKernelTask = TaskID;
    os_hw_state.KernelDispatchObserveCount++;
}

const Os_Port_Tms570_StateType* Os_Port_Tms570_GetBootstrapState(void)
{
    return &os_hw_state;
}

/* ====================================================================
 * Stubs — test harness paths not used on real hardware.
 * CompleteConfiguredDispatch checks these but the conditions are never
 * true on hardware (IrqSchedulerReturnInProgress etc. stay FALSE).
 * ==================================================================== */

void Os_Port_Tms570_FinishIrqSchedulerReturn(void)
{
    /* Not used on hardware — IRQ return handled by assembly */
}

void Os_Port_Tms570_FinishFiqSchedulerReturn(void)
{
    /* Not used on hardware — FIQ return handled by assembly */
}

void Os_Port_Tms570_IrqContextSave(void)
{
    /* Not used on hardware — IRQ context saved by assembly */
}

void Os_Port_Tms570_IrqContextRestore(void)
{
    /* Not used on hardware — IRQ context restored by assembly */
}

/* ====================================================================
 * Os_Port.h functions — called by kernel and binding layer
 * ==================================================================== */

void Os_PortTargetInit(void)
{
    /* HALCoGen already initializes VIM/RTI via systemInit + rtiInit.
     * Zero out our state struct. */
    uint8 i;
    uint8* p = (uint8*)&os_hw_state;

    for (i = 0u; i < (uint8)sizeof(os_hw_state); i++) {
        p[i] = 0u;
    }
}

void Os_PortStartFirstTask(void)
{
    os_hw_state.FirstTaskStarted = TRUE;
    os_hw_state.FirstTaskLaunchCount++;
    Os_Port_Tms570_StartFirstTaskAsm();
}

void Os_PortRequestContextSwitch(void)
{
    /* Synchronous dispatch model — the kernel calls Entry() directly
     * from os_dispatch_task.  No deferred context switch needed.
     * Mark DispatchRequested for state consistency. */
    os_hw_state.DispatchRequested = TRUE;
    os_hw_state.DispatchRequestCount++;
}

void Os_PortEnterIsr2(void)
{
    Os_BootstrapEnterIsr2();
}

void Os_PortExitIsr2(void)
{
    Os_BootstrapExitIsr2();
}

/* ====================================================================
 * Helper for Target.c — allows querying the kernel's ready bitmap
 * without including Os_Internal.h from the HALCoGen-side file.
 * ==================================================================== */

uint8 Os_Port_Tms570_HwSelectNextTask(void)
{
    return os_select_next_ready_task();
}

#endif /* !UNIT_TEST */
#endif /* PLATFORM_TMS570 */
