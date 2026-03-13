/**
 * @file    Os_Port_Tms570.c
 * @brief   TMS570 Cortex-R5 bootstrap OS port skeleton
 * @date    2026-03-13
 *
 * @details This is the first concrete TMS570LC43x OS port scaffold. It is
 *          not yet linked into the live OS build. Its job is to capture the
 *          port boundary and the bootstrapping state model we will later
 *          wire to real IRQ/FIQ handling, RTI tick interrupt handling, and
 *          first-task launch code.
 *
 *          The local ThreadX archive used for this bootstrap does include
 *          a Cortex-R5 port tree. For TMS570 work we cross-check directly
 *          against the matching GNU Cortex-R5 files:
 *          - threadx-master/ports/cortex_r5/gnu/inc/tx_port.h
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_save.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_save.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_context_restore.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_start.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_end.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_schedule.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_thread_stack_build.S
 *          - threadx-master/ports/cortex_r5/gnu/src/tx_timer_interrupt.S
 */
#include "Os_Port_Tms570.h"

#if defined(PLATFORM_TMS570)

#define OS_PORT_TMS570_INITIAL_FRAME_BYTES 76u
#define OS_PORT_TMS570_INITIAL_STACK_TYPE  1u
#define OS_PORT_TMS570_INITIAL_CPSR        0x13u
#define OS_PORT_TMS570_IRQ_SYSTEM_STACK_FRAME_BYTES 8u
#define OS_PORT_TMS570_IRQ_RETURN_STACK_MAX 8u
#define OS_PORT_TMS570_IRQ_PROCESSING_RETURN_STACK_MAX 8u

static Os_Port_Tms570_StateType os_port_tms570_state;
static Os_Port_Tms570_TaskContextType os_port_tms570_task_context[OS_MAX_TASKS];
static uintptr_t os_port_tms570_irq_return_stack[OS_PORT_TMS570_IRQ_RETURN_STACK_MAX];
static uintptr_t os_port_tms570_irq_processing_return_stack
    [OS_PORT_TMS570_IRQ_PROCESSING_RETURN_STACK_MAX];

static uintptr_t os_port_tms570_align_down(uintptr_t Value, uintptr_t Alignment)
{
    return (Value & ~(Alignment - 1u));
}

static boolean os_port_tms570_is_valid_task(TaskType TaskID)
{
    return (boolean)(TaskID < OS_MAX_TASKS);
}

static uint8 os_port_tms570_get_save_continuation_action(uint8 SaveAction)
{
    if (SaveAction == OS_PORT_TMS570_SAVE_NESTED_IRQ) {
        return OS_PORT_TMS570_SAVE_CONTINUE_NESTED_RETURN;
    }

    if ((SaveAction == OS_PORT_TMS570_SAVE_CAPTURE_CURRENT) ||
        (SaveAction == OS_PORT_TMS570_SAVE_IDLE_SYSTEM)) {
        return OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING;
    }

    return OS_PORT_TMS570_SAVE_CONTINUE_NONE;
}

static uint8 os_port_tms570_get_save_action(void)
{
    TaskType current_task;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return OS_PORT_TMS570_SAVE_NONE;
    }

    if (os_port_tms570_state.IrqContextDepth > 0u) {
        return OS_PORT_TMS570_SAVE_NESTED_IRQ;
    }

    if (os_port_tms570_state.FirstTaskStarted == FALSE) {
        return OS_PORT_TMS570_SAVE_IDLE_SYSTEM;
    }

    current_task = os_port_tms570_state.CurrentTask;
    if ((current_task != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(current_task) == TRUE) &&
        (os_port_tms570_task_context[current_task].Prepared == TRUE)) {
        return OS_PORT_TMS570_SAVE_CAPTURE_CURRENT;
    }

    return OS_PORT_TMS570_SAVE_IDLE_SYSTEM;
}

static uint8 os_port_tms570_get_restore_action(void)
{
    boolean handoff_pending;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqContextDepth == 0u)) {
        return OS_PORT_TMS570_RESTORE_NONE;
    }

    if (os_port_tms570_state.IrqContextDepth > 1u) {
        return OS_PORT_TMS570_RESTORE_NESTED_RETURN;
    }

    handoff_pending = (boolean)((os_port_tms570_state.DispatchRequested == TRUE) ||
                                (os_port_tms570_state.DeferredDispatch == TRUE));
    if ((handoff_pending == TRUE) &&
        (os_port_tms570_state.SelectedNextTask != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(os_port_tms570_state.SelectedNextTask) == TRUE) &&
        (os_port_tms570_task_context[os_port_tms570_state.SelectedNextTask].Prepared == TRUE) &&
        (os_port_tms570_state.SelectedNextTask != os_port_tms570_state.CurrentTask)) {
        return OS_PORT_TMS570_RESTORE_SWITCH_TASK;
    }

    return OS_PORT_TMS570_RESTORE_RESUME_CURRENT;
}

static void os_port_tms570_build_initial_frame(uintptr_t FrameBase, Os_TaskEntryType Entry)
{
    uint32* frame = (uint32*)FrameBase;
    uint32 index;

    for (index = 0u; index < (uint32)(OS_PORT_TMS570_INITIAL_FRAME_BYTES / sizeof(uint32)); index++) {
        frame[index] = 0u;
    }

    frame[0] = OS_PORT_TMS570_INITIAL_STACK_TYPE;
    frame[1] = OS_PORT_TMS570_INITIAL_CPSR;
    frame[16] = (uint32)((uintptr_t)Entry & 0xFFFFFFFFu);
    frame[17] = 0u;
}

static void os_port_tms570_reset_task_contexts(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_TASKS; idx++) {
        os_port_tms570_task_context[idx].Prepared = FALSE;
        os_port_tms570_task_context[idx].TaskID = idx;
        os_port_tms570_task_context[idx].StackTop = (uintptr_t)0u;
        os_port_tms570_task_context[idx].SavedSp = (uintptr_t)0u;
        os_port_tms570_task_context[idx].RuntimeSp = (uintptr_t)0u;
        os_port_tms570_task_context[idx].Entry = (Os_TaskEntryType)0;
    }
}

static void os_port_tms570_reset_irq_processing_return_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_IRQ_PROCESSING_RETURN_STACK_MAX; idx++) {
        os_port_tms570_irq_processing_return_stack[idx] = (uintptr_t)0u;
    }
}

static void os_port_tms570_reset_irq_return_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_irq_return_stack[idx] = (uintptr_t)0u;
    }
}

uintptr_t Os_Port_Tms570_GetPreparedFirstTaskSp(void)
{
    return os_port_tms570_state.FirstTaskSp;
}

void Os_Port_Tms570_MarkFirstTaskStarted(void)
{
    os_port_tms570_state.FirstTaskStarted = TRUE;
    os_port_tms570_state.DispatchRequested = FALSE;
    os_port_tms570_state.DeferredDispatch = FALSE;
    os_port_tms570_state.CurrentTask = os_port_tms570_state.FirstTaskTaskID;
    os_port_tms570_state.CurrentTaskSp = os_port_tms570_state.FirstTaskSp;
    os_port_tms570_state.LastRestoredTaskSp = os_port_tms570_state.FirstTaskSp;
    os_port_tms570_state.FirstTaskLaunchCount++;
}

void Os_Port_Tms570_CompleteDispatch(void)
{
    TaskType target_task = os_port_tms570_state.CurrentTask;

    if ((os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (os_port_tms570_state.DispatchRequested == FALSE)) {
        return;
    }

    os_port_tms570_state.LastSavedTask = os_port_tms570_state.CurrentTask;
    if (os_port_tms570_state.IrqCapturedTask != INVALID_TASK) {
        os_port_tms570_state.LastSavedTask = os_port_tms570_state.IrqCapturedTask;
        os_port_tms570_state.LastSavedTaskSp = os_port_tms570_state.IrqCapturedTaskSp;
    } else if ((os_port_tms570_state.CurrentTask != INVALID_TASK) &&
               (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == TRUE) &&
               (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == TRUE)) {
        os_port_tms570_state.LastSavedTaskSp =
            os_port_tms570_task_context[os_port_tms570_state.CurrentTask].RuntimeSp;
    }

    if (os_port_tms570_state.SelectedNextTask != INVALID_TASK) {
        target_task = os_port_tms570_state.SelectedNextTask;
    }

    if (target_task != os_port_tms570_state.CurrentTask) {
        os_port_tms570_state.TaskSwitchCount++;
    }

    os_port_tms570_state.CurrentTask = target_task;
    if ((target_task != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(target_task) == TRUE) &&
        (os_port_tms570_task_context[target_task].Prepared == TRUE)) {
        os_port_tms570_state.CurrentTaskSp = os_port_tms570_task_context[target_task].RuntimeSp;
        os_port_tms570_state.LastRestoredTaskSp = os_port_tms570_task_context[target_task].RuntimeSp;
    }
    os_port_tms570_state.SelectedNextTask = INVALID_TASK;
    os_port_tms570_state.DispatchRequested = FALSE;
    os_port_tms570_state.DeferredDispatch = FALSE;
}

uint8 Os_Port_Tms570_PeekSaveAction(void)
{
    return os_port_tms570_get_save_action();
}

uint8 Os_Port_Tms570_PeekSaveContinuationAction(void)
{
    return os_port_tms570_get_save_continuation_action(os_port_tms570_get_save_action());
}

uint8 Os_Port_Tms570_BeginIrqContextSave(uintptr_t Sp)
{
    uint8 save_action;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        os_port_tms570_state.LastSaveAction = OS_PORT_TMS570_SAVE_NONE;
        os_port_tms570_state.LastSaveContinuationAction = OS_PORT_TMS570_SAVE_CONTINUE_NONE;
        return OS_PORT_TMS570_SAVE_NONE;
    }

    save_action = os_port_tms570_get_save_action();
    if ((save_action == OS_PORT_TMS570_SAVE_CAPTURE_CURRENT) &&
        (Os_Port_Tms570_SaveCurrentTaskSp(Sp) != E_OK)) {
        os_port_tms570_state.IrqCapturedTask = INVALID_TASK;
        os_port_tms570_state.IrqCapturedTaskSp = (uintptr_t)0u;
        save_action = OS_PORT_TMS570_SAVE_IDLE_SYSTEM;
    }

    if (os_port_tms570_state.IrqContextDepth < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX) {
        os_port_tms570_irq_return_stack[os_port_tms570_state.IrqContextDepth] =
            os_port_tms570_state.CurrentIrqReturnAddress;
    }

    os_port_tms570_state.LastSaveAction = save_action;
    os_port_tms570_state.LastSaveContinuationAction =
        os_port_tms570_get_save_continuation_action(save_action);
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_IRQ;
    os_port_tms570_state.LastSavedIrqReturnAddress =
        os_port_tms570_state.CurrentIrqReturnAddress;
    os_port_tms570_state.IrqContextSaveCount++;
    os_port_tms570_state.IrqContextDepth++;
    return save_action;
}

void Os_Port_Tms570_FinishIrqContextSave(uint8 SaveAction)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (SaveAction == OS_PORT_TMS570_SAVE_NONE)) {
        return;
    }

    if (os_port_tms570_state.LastSaveContinuationAction ==
        OS_PORT_TMS570_SAVE_CONTINUE_NESTED_RETURN) {
        os_port_tms570_state.NestedIrqReturnCount++;
    } else if (os_port_tms570_state.LastSaveContinuationAction ==
               OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING) {
        os_port_tms570_state.IrqProcessingEnterCount++;
    }

    Os_PortEnterIsr2();
}

void Os_Port_Tms570_IrqNestingStart(void)
{
    uint8 frame_index;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqContextDepth == 0u) ||
        (os_port_tms570_state.IrqProcessingDepth >= os_port_tms570_state.IrqContextDepth) ||
        (os_port_tms570_state.CurrentExecutionMode != OS_PORT_TMS570_MODE_IRQ)) {
        return;
    }

    frame_index = os_port_tms570_state.IrqSystemStackFrameDepth;
    if (frame_index < OS_PORT_TMS570_IRQ_PROCESSING_RETURN_STACK_MAX) {
        os_port_tms570_irq_processing_return_stack[frame_index] =
            os_port_tms570_state.CurrentIrqProcessingReturnAddress;
    }

    os_port_tms570_state.IrqProcessingDepth++;
    os_port_tms570_state.IrqSystemStackFrameDepth++;
    os_port_tms570_state.IrqSystemStackBytes += OS_PORT_TMS570_IRQ_SYSTEM_STACK_FRAME_BYTES;
    if (os_port_tms570_state.IrqSystemStackBytes > os_port_tms570_state.IrqSystemStackPeakBytes) {
        os_port_tms570_state.IrqSystemStackPeakBytes = os_port_tms570_state.IrqSystemStackBytes;
    }
    os_port_tms570_state.LastSavedIrqProcessingReturnAddress =
        os_port_tms570_state.CurrentIrqProcessingReturnAddress;
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
    os_port_tms570_state.IrqNestingStartCount++;
}

void Os_Port_Tms570_IrqNestingEnd(void)
{
    uint8 frame_index;
    uintptr_t restored_return_address = (uintptr_t)0u;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqProcessingDepth == 0u)) {
        return;
    }

    os_port_tms570_state.IrqProcessingDepth--;
    frame_index = os_port_tms570_state.IrqSystemStackFrameDepth;
    if (os_port_tms570_state.IrqSystemStackFrameDepth > 0u) {
        os_port_tms570_state.IrqSystemStackFrameDepth--;
        frame_index = os_port_tms570_state.IrqSystemStackFrameDepth;
    }
    if (frame_index < OS_PORT_TMS570_IRQ_PROCESSING_RETURN_STACK_MAX) {
        restored_return_address = os_port_tms570_irq_processing_return_stack[frame_index];
        os_port_tms570_irq_processing_return_stack[frame_index] = (uintptr_t)0u;
    }
    os_port_tms570_state.CurrentIrqProcessingReturnAddress = restored_return_address;
    os_port_tms570_state.LastRestoredIrqProcessingReturnAddress = restored_return_address;
    if (os_port_tms570_state.IrqSystemStackBytes >= OS_PORT_TMS570_IRQ_SYSTEM_STACK_FRAME_BYTES) {
        os_port_tms570_state.IrqSystemStackBytes -= OS_PORT_TMS570_IRQ_SYSTEM_STACK_FRAME_BYTES;
    } else {
        os_port_tms570_state.IrqSystemStackBytes = 0u;
    }
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_IRQ;
    os_port_tms570_state.IrqNestingEndCount++;
}

uint8 Os_Port_Tms570_PeekRestoreAction(void)
{
    return os_port_tms570_get_restore_action();
}

uint8 Os_Port_Tms570_BeginIrqContextRestore(void)
{
    uint8 restore_action;
    uint8 restore_index;
    uintptr_t restore_address = (uintptr_t)0u;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqContextDepth == 0u)) {
        os_port_tms570_state.LastRestoreAction = OS_PORT_TMS570_RESTORE_NONE;
        return OS_PORT_TMS570_RESTORE_NONE;
    }

    restore_action = os_port_tms570_get_restore_action();
    restore_index = (uint8)(os_port_tms570_state.IrqContextDepth - 1u);
    if (restore_index < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX) {
        restore_address = os_port_tms570_irq_return_stack[restore_index];
        os_port_tms570_irq_return_stack[restore_index] = (uintptr_t)0u;
    }
    os_port_tms570_state.LastRestoreAction = restore_action;
    os_port_tms570_state.CurrentIrqReturnAddress = restore_address;
    os_port_tms570_state.LastRestoredIrqReturnAddress = restore_address;
    os_port_tms570_state.IrqContextRestoreCount++;
    os_port_tms570_state.IrqContextDepth--;
    return restore_action;
}

void Os_Port_Tms570_FinishIrqContextRestore(uint8 RestoreAction)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (RestoreAction == OS_PORT_TMS570_RESTORE_NONE)) {
        return;
    }

    Os_PortExitIsr2();

    if ((RestoreAction == OS_PORT_TMS570_RESTORE_SWITCH_TASK) ||
        (os_port_tms570_state.DispatchRequested == TRUE)) {
        Os_Port_Tms570_CompleteDispatch();
    }

    if (RestoreAction == OS_PORT_TMS570_RESTORE_NESTED_RETURN) {
        os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
    } else {
        os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_THREAD;
    }
    os_port_tms570_state.CurrentIrqReturnAddress = (uintptr_t)0u;

    if (os_port_tms570_state.IrqContextDepth == 0u) {
        os_port_tms570_state.IrqCapturedTask = INVALID_TASK;
        os_port_tms570_state.IrqCapturedTaskSp = (uintptr_t)0u;
    }
}

const Os_Port_Tms570_StateType* Os_Port_Tms570_GetBootstrapState(void)
{
    return &os_port_tms570_state;
}

const Os_Port_Tms570_TaskContextType* Os_Port_Tms570_GetTaskContext(TaskType TaskID)
{
    if (os_port_tms570_is_valid_task(TaskID) == FALSE) {
        return (const Os_Port_Tms570_TaskContextType*)0;
    }

    return &os_port_tms570_task_context[TaskID];
}

void Os_PortTargetInit(void)
{
    /*
     * ThreadX study hook:
     * - tx_port.h for ARM-R port assumptions
     * - tx_initialize_low_level.S for low-level timer and vector setup split
     *
     * Later work:
     * - configure VIM routing
     * - configure RTI as the system counter source
     * - define IRQ/FIQ ownership and ISR2 entry rules
     */
    os_port_tms570_reset_task_contexts();
    os_port_tms570_reset_irq_return_stack();
    os_port_tms570_reset_irq_processing_return_stack();
    os_port_tms570_state.TargetInitialized = TRUE;
    os_port_tms570_state.VimConfigured = TRUE;
    os_port_tms570_state.RtiConfigured = TRUE;
    os_port_tms570_state.DispatchRequested = FALSE;
    os_port_tms570_state.DeferredDispatch = FALSE;
    os_port_tms570_state.FirstTaskPrepared = FALSE;
    os_port_tms570_state.FirstTaskStarted = FALSE;
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_THREAD;
    os_port_tms570_state.IrqNesting = 0u;
    os_port_tms570_state.FiqNesting = 0u;
    os_port_tms570_state.IrqContextDepth = 0u;
    os_port_tms570_state.IrqProcessingDepth = 0u;
    os_port_tms570_state.IrqSystemStackFrameDepth = 0u;
    os_port_tms570_state.FiqContextDepth = 0u;
    os_port_tms570_state.TickInterruptCount = 0u;
    os_port_tms570_state.DispatchRequestCount = 0u;
    os_port_tms570_state.FirstTaskLaunchCount = 0u;
    os_port_tms570_state.TaskSwitchCount = 0u;
    os_port_tms570_state.KernelDispatchObserveCount = 0u;
    os_port_tms570_state.IrqContextSaveCount = 0u;
    os_port_tms570_state.IrqContextRestoreCount = 0u;
    os_port_tms570_state.IrqNestingStartCount = 0u;
    os_port_tms570_state.IrqNestingEndCount = 0u;
    os_port_tms570_state.IrqSystemStackBytes = 0u;
    os_port_tms570_state.IrqSystemStackPeakBytes = 0u;
    os_port_tms570_state.FiqContextSaveCount = 0u;
    os_port_tms570_state.FiqContextRestoreCount = 0u;
    os_port_tms570_state.IrqProcessingEnterCount = 0u;
    os_port_tms570_state.NestedIrqReturnCount = 0u;
    os_port_tms570_state.FirstTaskTaskID = INVALID_TASK;
    os_port_tms570_state.CurrentTask = INVALID_TASK;
    os_port_tms570_state.IrqCapturedTask = INVALID_TASK;
    os_port_tms570_state.LastSavedTask = INVALID_TASK;
    os_port_tms570_state.LastObservedKernelTask = INVALID_TASK;
    os_port_tms570_state.SelectedNextTask = INVALID_TASK;
    os_port_tms570_state.LastSaveAction = OS_PORT_TMS570_SAVE_NONE;
    os_port_tms570_state.LastSaveContinuationAction = OS_PORT_TMS570_SAVE_CONTINUE_NONE;
    os_port_tms570_state.LastRestoreAction = OS_PORT_TMS570_RESTORE_NONE;
    os_port_tms570_state.FirstTaskEntryAddress = (uintptr_t)0u;
    os_port_tms570_state.FirstTaskStackTop = (uintptr_t)0u;
    os_port_tms570_state.FirstTaskSp = (uintptr_t)0u;
    os_port_tms570_state.IrqCapturedTaskSp = (uintptr_t)0u;
    os_port_tms570_state.CurrentTaskSp = (uintptr_t)0u;
    os_port_tms570_state.LastSavedTaskSp = (uintptr_t)0u;
    os_port_tms570_state.LastRestoredTaskSp = (uintptr_t)0u;
    os_port_tms570_state.CurrentIrqReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastSavedIrqReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastRestoredIrqReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.CurrentIrqProcessingReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastSavedIrqProcessingReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastRestoredIrqProcessingReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.InitialCpsr = OS_PORT_TMS570_INITIAL_CPSR;
}

StatusType Os_Port_Tms570_PrepareTaskContext(TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop)
{
    uintptr_t prepared_sp;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((os_port_tms570_is_valid_task(TaskID) == FALSE) || (Entry == (Os_TaskEntryType)0) ||
        (StackTop < (uintptr_t)OS_PORT_TMS570_INITIAL_FRAME_BYTES)) {
        return E_OS_VALUE;
    }

    prepared_sp =
        os_port_tms570_align_down(StackTop - (uintptr_t)OS_PORT_TMS570_INITIAL_FRAME_BYTES, (uintptr_t)8u);
    if (prepared_sp == (uintptr_t)0u) {
        return E_OS_VALUE;
    }

    os_port_tms570_task_context[TaskID].Prepared = TRUE;
    os_port_tms570_task_context[TaskID].TaskID = TaskID;
    os_port_tms570_task_context[TaskID].StackTop = StackTop;
    os_port_tms570_task_context[TaskID].SavedSp = prepared_sp;
    os_port_tms570_task_context[TaskID].RuntimeSp = prepared_sp;
    os_port_tms570_task_context[TaskID].Entry = Entry;
    os_port_tms570_build_initial_frame(prepared_sp, Entry);
    return E_OK;
}

StatusType Os_Port_Tms570_PrepareFirstTask(TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop)
{
    StatusType status = Os_Port_Tms570_PrepareTaskContext(TaskID, Entry, StackTop);

    if (status != E_OK) {
        return status;
    }

    os_port_tms570_state.DispatchRequested = FALSE;
    os_port_tms570_state.DeferredDispatch = FALSE;
    os_port_tms570_state.FirstTaskPrepared = TRUE;
    os_port_tms570_state.FirstTaskStarted = FALSE;
    os_port_tms570_state.FirstTaskTaskID = TaskID;
    os_port_tms570_state.FirstTaskEntryAddress = (uintptr_t)Entry;
    os_port_tms570_state.FirstTaskStackTop = StackTop;
    os_port_tms570_state.FirstTaskSp = os_port_tms570_task_context[TaskID].SavedSp;
    os_port_tms570_state.InitialCpsr = OS_PORT_TMS570_INITIAL_CPSR;
    return E_OK;
}

StatusType Os_Port_Tms570_SelectNextTask(TaskType TaskID)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE)) {
        return E_OS_VALUE;
    }

    os_port_tms570_state.SelectedNextTask = TaskID;
    return E_OK;
}

void Os_Port_Tms570_SynchronizeCurrentTask(TaskType TaskID)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE)) {
        return;
    }

    os_port_tms570_state.CurrentTask = TaskID;
    os_port_tms570_state.CurrentTaskSp = os_port_tms570_task_context[TaskID].RuntimeSp;
    os_port_tms570_state.SelectedNextTask = INVALID_TASK;
}

void Os_Port_Tms570_ObserveKernelDispatch(TaskType TaskID)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE)) {
        return;
    }

    os_port_tms570_state.LastObservedKernelTask = TaskID;
    os_port_tms570_state.KernelDispatchObserveCount++;
}

StatusType Os_Port_Tms570_SaveCurrentTaskSp(uintptr_t Sp)
{
    TaskType current_task;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (Sp == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    current_task = os_port_tms570_state.CurrentTask;
    if ((os_port_tms570_state.IrqContextDepth != 0u) ||
        (current_task == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(current_task) == FALSE) ||
        (os_port_tms570_task_context[current_task].Prepared == FALSE)) {
        return E_OK;
    }

    os_port_tms570_state.CurrentTaskSp = Sp;
    os_port_tms570_task_context[current_task].RuntimeSp = Sp;
    os_port_tms570_state.IrqCapturedTask = current_task;
    os_port_tms570_state.IrqCapturedTaskSp = Sp;
    return E_OK;
}

uintptr_t Os_Port_Tms570_PeekRestoreTaskSp(void)
{
    TaskType target_task = os_port_tms570_state.CurrentTask;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return (uintptr_t)0u;
    }

    if (os_port_tms570_state.SelectedNextTask != INVALID_TASK) {
        target_task = os_port_tms570_state.SelectedNextTask;
    }

    if ((target_task == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(target_task) == FALSE) ||
        (os_port_tms570_task_context[target_task].Prepared == FALSE)) {
        return (uintptr_t)0u;
    }

    return os_port_tms570_task_context[target_task].RuntimeSp;
}

void Os_PortStartFirstTask(void)
{
    /*
     * ThreadX study hook:
     * - tx_thread_schedule.S
     * - tx_thread_stack_build.S
     *
     * Later work:
     * - branch into the first runnable task context
     * - load the initial ARM-R mode and saved register frame
     */
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskPrepared == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == TRUE)) {
        return;
    }

    Os_Port_Tms570_StartFirstTaskAsm();
}

void Os_PortRequestContextSwitch(void)
{
    /*
     * ThreadX study hook:
     * - tx_thread_context_save.S
     * - tx_thread_context_restore.S
     *
     * Later work:
     * - request deferred dispatch from IRQ return
     * - keep the real register save/restore path in assembly
     */
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == FALSE)) {
        return;
    }

    if (os_port_tms570_state.DispatchRequested == TRUE) {
        return;
    }

    if (os_port_tms570_state.IrqNesting > 0u) {
        os_port_tms570_state.DeferredDispatch = TRUE;
        return;
    }

    os_port_tms570_state.DispatchRequested = TRUE;
    os_port_tms570_state.DispatchRequestCount++;
}

void Os_PortEnterIsr2(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return;
    }

    os_port_tms570_state.IrqNesting++;
    Os_BootstrapEnterIsr2();
}

void Os_PortExitIsr2(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return;
    }

    Os_BootstrapExitIsr2();

    if (os_port_tms570_state.IrqNesting > 0u) {
        os_port_tms570_state.IrqNesting--;
    }

    if ((os_port_tms570_state.IrqNesting == 0u) &&
        (os_port_tms570_state.DeferredDispatch == TRUE) &&
        (os_port_tms570_state.FirstTaskStarted == TRUE)) {
        os_port_tms570_state.DeferredDispatch = FALSE;
        os_port_tms570_state.DispatchRequested = TRUE;
        os_port_tms570_state.DispatchRequestCount++;
    }

    /*
     * ThreadX study hook:
     * - tx_thread_irq_nesting_start.S
     * - tx_thread_irq_nesting_end.S
     *
     * Later work:
     * - dispatch on final IRQ exit if a higher-priority task became ready
     */
}

void Os_Port_Tms570_EnterFiq(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return;
    }

    os_port_tms570_state.FiqNesting++;
}

void Os_Port_Tms570_ExitFiq(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return;
    }

    if (os_port_tms570_state.FiqNesting > 0u) {
        os_port_tms570_state.FiqNesting--;
    }

    /*
     * ThreadX study hook:
     * - cortex_r5/gnu/src/tx_thread_fiq_nesting_start.S
     * - cortex_r5/gnu/src/tx_thread_fiq_nesting_end.S
     */
}

void Os_Port_Tms570_TickIsr(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return;
    }

    /*
     * Later work:
     * - acknowledge RTI compare interrupt
     * - advance OSEK counter/alarm processing
     * - request dispatch on final interrupt exit if needed
     */
    os_port_tms570_state.TickInterruptCount++;

    if (Os_BootstrapProcessCounterTick() == TRUE) {
        Os_PortRequestContextSwitch();
    }
}

#if defined(UNIT_TEST)
void Os_Port_Tms570_StartFirstTaskAsm(void)
{
    Os_Port_Tms570_MarkFirstTaskStarted();
}

void Os_Port_Tms570_IrqContextSave(void)
{
    uint8 save_action;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        os_port_tms570_state.LastSaveAction = OS_PORT_TMS570_SAVE_NONE;
        os_port_tms570_state.LastSaveContinuationAction = OS_PORT_TMS570_SAVE_CONTINUE_NONE;
        return;
    }

    save_action = Os_Port_Tms570_BeginIrqContextSave(os_port_tms570_state.CurrentTaskSp);
    if (save_action == OS_PORT_TMS570_SAVE_NONE) {
        return;
    }

    Os_Port_Tms570_FinishIrqContextSave(save_action);
}

void Os_Port_Tms570_IrqContextRestore(void)
{
    uint8 restore_action;

    restore_action = Os_Port_Tms570_BeginIrqContextRestore();
    if (restore_action == OS_PORT_TMS570_RESTORE_NONE) {
        return;
    }

    Os_Port_Tms570_FinishIrqContextRestore(restore_action);
}

void Os_Port_Tms570_FiqContextSave(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return;
    }

    os_port_tms570_state.FiqContextSaveCount++;
    os_port_tms570_state.FiqContextDepth++;
    Os_Port_Tms570_EnterFiq();
}

void Os_Port_Tms570_FiqContextRestore(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FiqContextDepth == 0u)) {
        return;
    }

    os_port_tms570_state.FiqContextRestoreCount++;
    os_port_tms570_state.FiqContextDepth--;
    Os_Port_Tms570_ExitFiq();
}

void Os_Port_Tms570_RtiTickHandler(void)
{
    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();
    Os_Port_Tms570_TickIsr();
    Os_Port_Tms570_IrqNestingEnd();
    Os_Port_Tms570_IrqContextRestore();
}

StatusType Os_Port_Tms570_TestSetIrqReturnAddress(uintptr_t Address)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Address == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentIrqReturnAddress = Address;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetIrqProcessingReturnAddress(uintptr_t Address)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Address == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentIrqProcessingReturnAddress = Address;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetCurrentTaskSp(uintptr_t Sp)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (os_port_tms570_state.CurrentTask == INVALID_TASK) ||
        (Sp == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentTaskSp = Sp;
    return E_OK;
}
#endif

#endif
