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
#ifdef OS_BOOTSTRAP_BRINGUP
#include "sc_os_cfg.h"
extern void sc_sci_puts(const char* str);
extern void sc_sci_put_uint(uint32 value);
extern uint32 Os_Port_Tms570_BringupGetTickEntryCount(void);
#endif

extern uint32 sc_hw_cycle_time_us(void);
static uint8 os_tms570_isr_depth;
static uint32 os_tms570_tp_start_us;

/* ====================================================================
 * Extern declarations for Target.c functions (HALCoGen-side).
 * Types are ABI-compatible: uint8 = unsigned char in both domains.
 * ==================================================================== */

extern uint8 Os_Port_Tms570_TargetPrepareTask(
    uint8 taskId, void (*entry)(void), uintptr_t stackTop);

extern uint8 Os_Port_Tms570_TargetPrepareFirstTask(
    uint8 taskId, void (*entry)(void), uintptr_t stackTop);

extern void Os_Port_Tms570_TargetSetNextTask(uint8 taskId);
extern void Os_Port_Tms570_TargetMarkSwitchPending(void);
extern void Os_Port_Tms570_TargetEnableRtiIrq(void);

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
    Os_Port_Tms570_CooperativeContextType* save;
    Os_Port_Tms570_CooperativeContextType* restore;

    /* Record every staged dispatch before selecting the hardware path. */
    os_hw_state.DispatchRequested = TRUE;
    os_hw_state.DispatchRequestCount++;

    /* TerminateTask requests switchback from task context. TMS570 has no
     * PendSV equivalent, so use the validated cooperative switch directly.
     * RTI-originated preemption remains owned by the RTI assembly handler. */
    if (os_tms570_isr_depth == 0u) {
        save = Os_Port_Tms570_GetPendingSaveCoopCtx();
        restore = Os_Port_Tms570_GetPendingRestoreCoopCtx();
        if ((save != NULL_PTR) && (restore != NULL_PTR)) {
            Os_Port_Tms570_SwitchContextAsm(save, restore);
        }
    } else {
        Os_Port_Tms570_TargetMarkSwitchPending();
    }
}

void Os_PortEnterIsr2(void)
{
    os_tms570_isr_depth++;
    Os_BootstrapEnterIsr2();
}

void Os_Port_Tms570_EnableRtiTick(void)
{
#ifdef OS_BOOTSTRAP_BRINGUP
    sc_sci_puts("[OSEK-TICK] wrapper enter\r\n");
#endif
    Os_Port_Tms570_TargetEnableRtiIrq();
#ifdef OS_BOOTSTRAP_BRINGUP
    sc_sci_puts("[OSEK-TICK] wrapper exit\r\n");
#endif
}

#ifdef OS_BOOTSTRAP_BRINGUP
void Os_Port_Tms570_BringupObserveKernelState(void)
{
    static uint32 last_isr_count = 0xFFFFFFFFu;
    uint32 isr_count = Os_Port_Tms570_BringupGetTickEntryCount();

    /* Emit the armed state once and the first observed ISR transition once.
     * Do not stream at 100 Hz: UART must not perturb the scheduling seam. */
    if ((last_isr_count == 0xFFFFFFFFu) ||
        ((last_isr_count == 0u) && (isr_count > 0u))) {
        TickType remaining = 0u;
        StatusType alarm_status = GetAlarm(SC_ALARM_MAIN_ID, &remaining);
        last_isr_count = isr_count;
        sc_sci_puts("[OSEK-KERNEL] isr="); sc_sci_put_uint(isr_count);
        sc_sci_puts(" counter="); sc_sci_put_uint(os_counter_value);
        sc_sci_puts(" alarm_status="); sc_sci_put_uint((uint32)alarm_status);
        sc_sci_puts(" remaining="); sc_sci_put_uint(remaining);
        sc_sci_puts("\r\n");
    }
}
#endif

void Os_PortExitIsr2(void)
{
    Os_BootstrapExitIsr2();
    if (os_tms570_isr_depth > 0u) {
        os_tms570_isr_depth--;
    }
}

boolean Os_PortIsInIsrContext(void)
{
    return (boolean)(os_tms570_isr_depth > 0u);
}

void Os_PortTimingProtArmBudget(uint32 BudgetUs)
{
    (void)BudgetUs;
    os_tms570_tp_start_us = sc_hw_cycle_time_us();
}

void Os_PortTimingProtDisarm(void)
{
    os_tms570_tp_start_us = sc_hw_cycle_time_us();
}

uint32 Os_PortTimingProtElapsedUs(void)
{
    return sc_hw_cycle_time_us() - os_tms570_tp_start_us;
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
