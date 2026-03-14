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
 *          The design target remains the GNU Cortex-R5 ThreadX port. For the
 *          local extracted ThreadX tree currently available in this workspace,
 *          some interrupt-ownership slices are cross-checked against the
 *          closest available `ports/arm11/gnu/...` files instead. The repo
 *          notes under `firmware/bsw/os/bootstrap/port/tms570/README.md`
 *          track which exact local files were used for each slice.
 */
#include "Os_Port_Tms570.h"

#if defined(PLATFORM_TMS570)

#define OS_PORT_TMS570_INITIAL_FRAME_BYTES 76u
#define OS_PORT_TMS570_INITIAL_STACK_TYPE  1u
#define OS_PORT_TMS570_INITIAL_CPSR        0x13u
#define OS_PORT_TMS570_IRQ_SYSTEM_STACK_FRAME_BYTES 8u
#define OS_PORT_TMS570_FIQ_SYSTEM_STACK_FRAME_BYTES 8u
#define OS_PORT_TMS570_IRQ_RETURN_STACK_MAX 8u
#define OS_PORT_TMS570_FIQ_RETURN_STACK_MAX 8u
#define OS_PORT_TMS570_IRQ_PROCESSING_RETURN_STACK_MAX 8u
#define OS_PORT_TMS570_FIQ_PROCESSING_RETURN_STACK_MAX 8u
#define OS_PORT_TMS570_VIM_ISR_TABLE_SLOTS 129u

static Os_Port_Tms570_StateType os_port_tms570_state;
static Os_Port_Tms570_TaskContextType os_port_tms570_task_context[OS_MAX_TASKS];
static uintptr_t os_port_tms570_vim_isr_table[OS_PORT_TMS570_VIM_ISR_TABLE_SLOTS];
static uintptr_t os_port_tms570_irq_return_stack[OS_PORT_TMS570_IRQ_RETURN_STACK_MAX];
static uintptr_t os_port_tms570_fiq_return_stack[OS_PORT_TMS570_FIQ_RETURN_STACK_MAX];
static uint32 os_port_tms570_fiq_context_frame_stack[OS_PORT_TMS570_FIQ_RETURN_STACK_MAX];
static uintptr_t os_port_tms570_irq_processing_return_stack
    [OS_PORT_TMS570_IRQ_PROCESSING_RETURN_STACK_MAX];
static uintptr_t os_port_tms570_fiq_processing_return_stack
    [OS_PORT_TMS570_FIQ_PROCESSING_RETURN_STACK_MAX];

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

static void os_port_tms570_service_expired_time_slice(void)
{
    TaskType current_task;

    if (os_port_tms570_state.TimeSliceServicePending == FALSE) {
        return;
    }

    /*
     * Match the local ThreadX timer ISR's separate _tx_thread_time_slice()
     * hook without claiming full round-robin scheduler behavior yet.
     */
    os_port_tms570_state.TimeSliceServiceCount++;
    os_port_tms570_state.TimeSliceServicePending = FALSE;

    current_task = os_port_tms570_state.CurrentTask;
    if ((current_task != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(current_task) == TRUE) &&
        (os_port_tms570_task_context[current_task].Prepared == TRUE)) {
        os_port_tms570_state.CurrentTimeSlice =
            os_port_tms570_task_context[current_task].SavedTimeSlice;
    }
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
        os_port_tms570_task_context[idx].SavedTimeSlice = 0u;
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

static void os_port_tms570_reset_fiq_processing_return_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_FIQ_PROCESSING_RETURN_STACK_MAX; idx++) {
        os_port_tms570_fiq_processing_return_stack[idx] = (uintptr_t)0u;
    }
}

static void os_port_tms570_reset_irq_return_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_irq_return_stack[idx] = (uintptr_t)0u;
    }
}

static void os_port_tms570_reset_fiq_return_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_fiq_return_stack[idx] = (uintptr_t)0u;
    }
}

static void os_port_tms570_reset_fiq_context_frame_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_fiq_context_frame_stack[idx] = 0u;
    }
}

static void os_port_tms570_reset_vim_isr_table(void)
{
    uint32 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_VIM_ISR_TABLE_SLOTS; idx++) {
        os_port_tms570_vim_isr_table[idx] = (uintptr_t)0u;
    }
}

static uint32 os_port_tms570_get_fiq_context_frame_bytes(uint8 SaveAction)
{
    if (SaveAction == OS_PORT_TMS570_FIQ_SAVE_NESTED_FIQ) {
        return OS_PORT_TMS570_FIQ_NESTED_FRAME_BYTES;
    }

    if ((SaveAction == OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY) ||
        (SaveAction == OS_PORT_TMS570_FIQ_SAVE_IDLE_SYSTEM)) {
        return OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES;
    }

    return 0u;
}

static StatusType os_port_tms570_update_running_task_sp(uintptr_t Sp)
{
    TaskType current_task;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (Sp == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    current_task = os_port_tms570_state.CurrentTask;
    if ((current_task == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(current_task) == FALSE) ||
        (os_port_tms570_task_context[current_task].Prepared == FALSE)) {
        return E_OK;
    }

    os_port_tms570_state.CurrentTaskSp = Sp;
    os_port_tms570_task_context[current_task].RuntimeSp = Sp;
    return E_OK;
}

static void os_port_tms570_configure_vim_for_rti_compare0(void)
{
    /* Mirror the local HALCoGen route-to-IRQ and enable sequence for VIM channel 2. */
    os_port_tms570_state.VimChanctrl0 = OS_PORT_TMS570_VIM_CHANCTRL0_DEFAULT;
    os_port_tms570_state.VimFirqpr0 &= ~OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK;
    os_port_tms570_state.VimReqmaskset0 |= OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK;
    os_port_tms570_state.VimRtiCompare0HandlerAddress = (uintptr_t)&Os_Port_Tms570_RtiTickHandler;
    os_port_tms570_vim_isr_table[OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL + 1u] =
        os_port_tms570_state.VimRtiCompare0HandlerAddress;
}

static void os_port_tms570_configure_rti_compare0_tick(void)
{
    /*
     * Mirror the local HALCoGen RTI setup used for the 10 ms Safety Controller tick:
     * - VCLK source selection in GCTRL
     * - compare 0 sourced from counter block 0 in COMPCTRL
     * - compare/update values at 93750 counts
     * - compare 0 interrupt enabled
     * - counter block 0 started
     */
    os_port_tms570_state.RtiGctrl =
        OS_PORT_TMS570_RTI_GCTRL_CLOCK_SOURCE | OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE;
    os_port_tms570_state.RtiCompctrl = OS_PORT_TMS570_RTI_COMPCTRL_DEFAULT;
    os_port_tms570_state.RtiCounter0Value = 0u;
    os_port_tms570_state.RtiCmp0Comp = OS_PORT_TMS570_RTI_COMPARE0_PERIOD;
    os_port_tms570_state.RtiCmp0Udcp = OS_PORT_TMS570_RTI_COMPARE0_PERIOD;
    os_port_tms570_state.RtiSetintena = OS_PORT_TMS570_RTI_COMPARE0_INTFLAG;
    os_port_tms570_state.RtiClearintena = 0u;
    os_port_tms570_state.RtiIntflag = 0u;
    os_port_tms570_state.RtiCompare0AckCount = 0u;
}

static void os_port_tms570_acknowledge_rti_compare0(void)
{
    if ((os_port_tms570_state.RtiSetintena & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG) == 0u) {
        return;
    }

    if ((os_port_tms570_state.RtiIntflag & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG) != 0u) {
        /*
         * Mirror the local HALCoGen / RTI hardware behavior: on each compare 0
         * match, the update compare value is added to the programmed compare.
         */
        os_port_tms570_state.RtiCmp0Comp += os_port_tms570_state.RtiCmp0Udcp;
        os_port_tms570_state.RtiIntflag &= ~OS_PORT_TMS570_RTI_COMPARE0_INTFLAG;
        os_port_tms570_state.RtiCompare0AckCount++;
    }
}

static boolean os_port_tms570_can_deliver_rti_compare0_to_vim(void)
{
    return (boolean)(
        ((os_port_tms570_state.VimReqmaskset0 & OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK) != 0u) &&
        ((os_port_tms570_state.VimFirqpr0 & OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK) == 0u) &&
        ((os_port_tms570_state.RtiGctrl & OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE) != 0u) &&
        ((os_port_tms570_state.RtiSetintena & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG) != 0u) &&
        ((os_port_tms570_state.RtiIntflag & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG) != 0u) &&
        (os_port_tms570_state.VimRtiCompare0HandlerAddress ==
         (uintptr_t)&Os_Port_Tms570_RtiTickHandler));
}

static void os_port_tms570_sync_rti_compare0_vim_request(void)
{
    if (os_port_tms570_can_deliver_rti_compare0_to_vim() == TRUE) {
        os_port_tms570_state.VimIntreq0 |= OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK;
    } else {
        os_port_tms570_state.VimIntreq0 &= ~OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK;
    }
}

static uint32 os_port_tms570_get_highest_pending_irq_channel(void)
{
    uint32 channel;

    if (((os_port_tms570_state.VimIntreq0 & OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK) != 0u) &&
        (os_port_tms570_can_deliver_rti_compare0_to_vim() == TRUE) &&
        (Os_Port_Tms570_ReadMappedChannelForRequest(OS_PORT_TMS570_VIM_RTI_COMPARE0_REQUEST,
                                                    &channel) == E_OK)) {
        return channel;
    }

    return OS_PORT_TMS570_VIM_NO_CHANNEL;
}

static boolean os_port_tms570_has_selected_rti_compare0_irq(void)
{
    return (boolean)(
        (os_port_tms570_state.VimIrqIndex == OS_PORT_TMS570_VIM_RTI_COMPARE0_IRQINDEX) &&
        (os_port_tms570_state.VimIrqVecReg == os_port_tms570_state.VimRtiCompare0HandlerAddress));
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
    if ((os_port_tms570_is_valid_task(os_port_tms570_state.FirstTaskTaskID) == TRUE) &&
        (os_port_tms570_task_context[os_port_tms570_state.FirstTaskTaskID].Prepared == TRUE)) {
        os_port_tms570_state.CurrentTimeSlice =
            os_port_tms570_task_context[os_port_tms570_state.FirstTaskTaskID].SavedTimeSlice;
    } else {
        os_port_tms570_state.CurrentTimeSlice = 0u;
    }
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
        if ((os_port_tms570_state.CurrentTask != INVALID_TASK) &&
            (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == TRUE) &&
            (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == TRUE)) {
            os_port_tms570_state.LastSavedTimeSlice = 0u;
            if (os_port_tms570_state.CurrentTimeSlice > 0u) {
                os_port_tms570_state.LastSavedTimeSlice = os_port_tms570_state.CurrentTimeSlice;
                os_port_tms570_task_context[os_port_tms570_state.CurrentTask].SavedTimeSlice =
                    os_port_tms570_state.CurrentTimeSlice;
            }
        }
        os_port_tms570_state.TaskSwitchCount++;
    }

    os_port_tms570_state.CurrentTask = target_task;
    if ((target_task != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(target_task) == TRUE) &&
        (os_port_tms570_task_context[target_task].Prepared == TRUE)) {
        os_port_tms570_state.CurrentTaskSp = os_port_tms570_task_context[target_task].RuntimeSp;
        os_port_tms570_state.LastRestoredTaskSp = os_port_tms570_task_context[target_task].RuntimeSp;
        os_port_tms570_state.CurrentTimeSlice = os_port_tms570_task_context[target_task].SavedTimeSlice;
    } else {
        os_port_tms570_state.CurrentTimeSlice = 0u;
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
    os_port_tms570_reset_fiq_return_stack();
    os_port_tms570_reset_fiq_context_frame_stack();
    os_port_tms570_reset_vim_isr_table();
    os_port_tms570_reset_irq_processing_return_stack();
    os_port_tms570_reset_fiq_processing_return_stack();
    os_port_tms570_state.TargetInitialized = TRUE;
    os_port_tms570_state.VimConfigured = TRUE;
    os_port_tms570_state.RtiConfigured = TRUE;
    os_port_tms570_state.DispatchRequested = FALSE;
    os_port_tms570_state.DeferredDispatch = FALSE;
    os_port_tms570_state.FirstTaskPrepared = FALSE;
    os_port_tms570_state.FirstTaskStarted = FALSE;
    os_port_tms570_state.TimeSliceServicePending = FALSE;
    os_port_tms570_state.FiqProcessingInterruptsEnabled = FALSE;
    os_port_tms570_state.FiqPreemptDisable = FALSE;
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_THREAD;
    os_port_tms570_state.FiqResumeMode = OS_PORT_TMS570_MODE_THREAD;
    os_port_tms570_state.IrqNesting = 0u;
    os_port_tms570_state.FiqNesting = 0u;
    os_port_tms570_state.IrqContextDepth = 0u;
    os_port_tms570_state.IrqProcessingDepth = 0u;
    os_port_tms570_state.IrqSystemStackFrameDepth = 0u;
    os_port_tms570_state.FiqContextDepth = 0u;
    os_port_tms570_state.FiqProcessingDepth = 0u;
    os_port_tms570_state.FiqSystemStackFrameDepth = 0u;
    os_port_tms570_state.TickInterruptCount = 0u;
    os_port_tms570_state.DispatchRequestCount = 0u;
    os_port_tms570_state.FirstTaskLaunchCount = 0u;
    os_port_tms570_state.TaskSwitchCount = 0u;
    os_port_tms570_state.KernelDispatchObserveCount = 0u;
    os_port_tms570_state.IrqContextSaveCount = 0u;
    os_port_tms570_state.IrqContextRestoreCount = 0u;
    os_port_tms570_state.IrqNestingStartCount = 0u;
    os_port_tms570_state.IrqNestingEndCount = 0u;
    os_port_tms570_state.VimChanctrl0 = 0u;
    os_port_tms570_state.VimFirqpr0 = 0u;
    os_port_tms570_state.VimIntreq0 = 0u;
    os_port_tms570_state.VimIrqIndex = 0u;
    os_port_tms570_state.VimLastIrqIndex = 0u;
    os_port_tms570_state.VimReqmaskset0 = 0u;
    os_port_tms570_state.VimReqmaskclr0 = 0u;
    os_port_tms570_state.VimLastServicedChannel = OS_PORT_TMS570_VIM_NO_CHANNEL;
    os_port_tms570_state.RtiGctrl = 0u;
    os_port_tms570_state.RtiCompctrl = 0u;
    os_port_tms570_state.RtiCounter0Value = 0u;
    os_port_tms570_state.RtiCmp0Comp = 0u;
    os_port_tms570_state.RtiCmp0Udcp = 0u;
    os_port_tms570_state.RtiSetintena = 0u;
    os_port_tms570_state.RtiClearintena = 0u;
    os_port_tms570_state.RtiIntflag = 0u;
    os_port_tms570_state.RtiCompare0AckCount = 0u;
    os_port_tms570_state.IrqSystemStackBytes = 0u;
    os_port_tms570_state.IrqSystemStackPeakBytes = 0u;
    os_port_tms570_state.FiqContextSaveCount = 0u;
    os_port_tms570_state.FiqContextRestoreCount = 0u;
    os_port_tms570_state.FiqNestingStartCount = 0u;
    os_port_tms570_state.FiqNestingEndCount = 0u;
    os_port_tms570_state.FiqInterruptEnableCount = 0u;
    os_port_tms570_state.FiqInterruptDisableCount = 0u;
    os_port_tms570_state.FiqSchedulerReturnCount = 0u;
    os_port_tms570_state.CurrentTimeSlice = 0u;
    os_port_tms570_state.LastSavedTimeSlice = 0u;
    os_port_tms570_state.TimeSliceExpirationCount = 0u;
    os_port_tms570_state.TimeSliceServiceCount = 0u;
    os_port_tms570_state.FiqSystemStackBytes = 0u;
    os_port_tms570_state.FiqSystemStackPeakBytes = 0u;
    os_port_tms570_state.FiqInterruptStackBytes = 0u;
    os_port_tms570_state.FiqInterruptStackPeakBytes = 0u;
    os_port_tms570_state.IrqProcessingEnterCount = 0u;
    os_port_tms570_state.NestedIrqReturnCount = 0u;
    os_port_tms570_state.FiqProcessingEnterCount = 0u;
    os_port_tms570_state.NestedFiqReturnCount = 0u;
    os_port_tms570_state.LastSavedFiqContextBytes = 0u;
    os_port_tms570_state.LastRestoredFiqContextBytes = 0u;
    os_port_tms570_state.FirstTaskTaskID = INVALID_TASK;
    os_port_tms570_state.CurrentTask = INVALID_TASK;
    os_port_tms570_state.IrqCapturedTask = INVALID_TASK;
    os_port_tms570_state.LastSavedTask = INVALID_TASK;
    os_port_tms570_state.LastObservedKernelTask = INVALID_TASK;
    os_port_tms570_state.SelectedNextTask = INVALID_TASK;
    os_port_tms570_state.LastSaveAction = OS_PORT_TMS570_SAVE_NONE;
    os_port_tms570_state.LastSaveContinuationAction = OS_PORT_TMS570_SAVE_CONTINUE_NONE;
    os_port_tms570_state.LastRestoreAction = OS_PORT_TMS570_RESTORE_NONE;
    os_port_tms570_state.LastFiqSaveAction = OS_PORT_TMS570_FIQ_SAVE_NONE;
    os_port_tms570_state.LastFiqSaveContinuationAction = OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NONE;
    os_port_tms570_state.LastFiqRestoreAction = OS_PORT_TMS570_FIQ_RESTORE_NONE;
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
    os_port_tms570_state.VimIrqVecReg = (uintptr_t)0u;
    os_port_tms570_state.VimLastIrqVecReg = (uintptr_t)0u;
    os_port_tms570_state.VimRtiCompare0HandlerAddress = (uintptr_t)0u;
    os_port_tms570_state.CurrentFiqReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastSavedFiqReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastRestoredFiqReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.CurrentFiqProcessingReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastSavedFiqProcessingReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastRestoredFiqProcessingReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.CurrentIrqProcessingReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastSavedIrqProcessingReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastRestoredIrqProcessingReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.InitialCpsr = OS_PORT_TMS570_INITIAL_CPSR;
    os_port_tms570_configure_vim_for_rti_compare0();
    os_port_tms570_configure_rti_compare0_tick();
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

    (void)os_port_tms570_update_running_task_sp(Sp);
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

    if (os_port_tms570_state.FiqNesting < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX) {
        os_port_tms570_fiq_return_stack[os_port_tms570_state.FiqNesting] =
            os_port_tms570_state.CurrentFiqReturnAddress;
    }

    if (os_port_tms570_state.FiqNesting == 0u) {
        os_port_tms570_state.FiqResumeMode = os_port_tms570_state.CurrentExecutionMode;
    }

    os_port_tms570_state.LastSavedFiqReturnAddress =
        os_port_tms570_state.CurrentFiqReturnAddress;
    os_port_tms570_state.FiqNesting++;
    os_port_tms570_state.FiqProcessingInterruptsEnabled = FALSE;
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_FIQ;
}

void Os_Port_Tms570_FiqNestingStart(void)
{
    uint8 frame_index;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FiqContextDepth == 0u) ||
        (os_port_tms570_state.FiqProcessingDepth >= os_port_tms570_state.FiqContextDepth) ||
        (os_port_tms570_state.CurrentExecutionMode != OS_PORT_TMS570_MODE_FIQ)) {
        return;
    }

    frame_index = os_port_tms570_state.FiqSystemStackFrameDepth;
    if (frame_index < OS_PORT_TMS570_FIQ_PROCESSING_RETURN_STACK_MAX) {
        os_port_tms570_fiq_processing_return_stack[frame_index] =
            os_port_tms570_state.CurrentFiqProcessingReturnAddress;
    }

    os_port_tms570_state.FiqProcessingDepth++;
    os_port_tms570_state.FiqSystemStackFrameDepth++;
    os_port_tms570_state.FiqSystemStackBytes += OS_PORT_TMS570_FIQ_SYSTEM_STACK_FRAME_BYTES;
    if (os_port_tms570_state.FiqSystemStackBytes > os_port_tms570_state.FiqSystemStackPeakBytes) {
        os_port_tms570_state.FiqSystemStackPeakBytes = os_port_tms570_state.FiqSystemStackBytes;
    }
    os_port_tms570_state.LastSavedFiqProcessingReturnAddress =
        os_port_tms570_state.CurrentFiqProcessingReturnAddress;
    os_port_tms570_state.FiqProcessingInterruptsEnabled = TRUE;
    os_port_tms570_state.FiqInterruptEnableCount++;
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
    os_port_tms570_state.FiqNestingStartCount++;
}

void Os_Port_Tms570_FiqNestingEnd(void)
{
    uint8 frame_index;
    uintptr_t restored_return_address = (uintptr_t)0u;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FiqProcessingDepth == 0u)) {
        return;
    }

    os_port_tms570_state.FiqProcessingDepth--;
    frame_index = os_port_tms570_state.FiqSystemStackFrameDepth;
    if (os_port_tms570_state.FiqSystemStackFrameDepth > 0u) {
        os_port_tms570_state.FiqSystemStackFrameDepth--;
        frame_index = os_port_tms570_state.FiqSystemStackFrameDepth;
    }
    if (frame_index < OS_PORT_TMS570_FIQ_PROCESSING_RETURN_STACK_MAX) {
        restored_return_address = os_port_tms570_fiq_processing_return_stack[frame_index];
        os_port_tms570_fiq_processing_return_stack[frame_index] = (uintptr_t)0u;
    }
    os_port_tms570_state.CurrentFiqProcessingReturnAddress = restored_return_address;
    os_port_tms570_state.LastRestoredFiqProcessingReturnAddress = restored_return_address;
    if (os_port_tms570_state.FiqSystemStackBytes >= OS_PORT_TMS570_FIQ_SYSTEM_STACK_FRAME_BYTES) {
        os_port_tms570_state.FiqSystemStackBytes -= OS_PORT_TMS570_FIQ_SYSTEM_STACK_FRAME_BYTES;
    } else {
        os_port_tms570_state.FiqSystemStackBytes = 0u;
    }
    os_port_tms570_state.FiqProcessingInterruptsEnabled = FALSE;
    os_port_tms570_state.FiqInterruptDisableCount++;
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_FIQ;
    os_port_tms570_state.FiqNestingEndCount++;
}

void Os_Port_Tms570_ExitFiq(void)
{
    uintptr_t restore_address = (uintptr_t)0u;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return;
    }

    if ((os_port_tms570_state.FiqNesting > 0u) &&
        ((os_port_tms570_state.FiqNesting - 1u) < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX)) {
        restore_address =
            os_port_tms570_fiq_return_stack[os_port_tms570_state.FiqNesting - 1u];
        os_port_tms570_fiq_return_stack[os_port_tms570_state.FiqNesting - 1u] = (uintptr_t)0u;
    }

    os_port_tms570_state.CurrentFiqReturnAddress = restore_address;
    os_port_tms570_state.LastRestoredFiqReturnAddress = restore_address;

    if (os_port_tms570_state.FiqNesting > 0u) {
        os_port_tms570_state.FiqNesting--;
    }

    if (os_port_tms570_state.FiqNesting == 0u) {
        os_port_tms570_state.CurrentExecutionMode = os_port_tms570_state.FiqResumeMode;
        os_port_tms570_state.FiqResumeMode = OS_PORT_TMS570_MODE_THREAD;
        os_port_tms570_state.CurrentFiqReturnAddress = (uintptr_t)0u;
    }

    /*
     * ThreadX study hook:
     * - arm11/gnu/src/tx_thread_fiq_nesting_start.S
     * - arm11/gnu/src/tx_thread_fiq_nesting_end.S
     */
}

uint8 Os_Port_Tms570_PeekFiqSaveAction(void)
{
    TaskType current_task;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return OS_PORT_TMS570_FIQ_SAVE_NONE;
    }

    if (os_port_tms570_state.FiqNesting > 0u) {
        return OS_PORT_TMS570_FIQ_SAVE_NESTED_FIQ;
    }

    current_task = os_port_tms570_state.CurrentTask;
    if ((os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (current_task == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(current_task) == FALSE) ||
        (os_port_tms570_task_context[current_task].Prepared == FALSE)) {
        return OS_PORT_TMS570_FIQ_SAVE_IDLE_SYSTEM;
    }

    return OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY;
}

uint8 Os_Port_Tms570_PeekFiqSaveContinuationAction(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NONE;
    }

    if (os_port_tms570_state.FiqNesting > 0u) {
        return OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NESTED_RETURN;
    }

    return OS_PORT_TMS570_FIQ_SAVE_CONTINUE_PROCESSING;
}

uint8 Os_Port_Tms570_BeginFiqContextSave(void)
{
    uint8 save_action;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        os_port_tms570_state.LastFiqSaveAction = OS_PORT_TMS570_FIQ_SAVE_NONE;
        os_port_tms570_state.LastFiqSaveContinuationAction = OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NONE;
        return OS_PORT_TMS570_FIQ_SAVE_NONE;
    }

    save_action = Os_Port_Tms570_PeekFiqSaveAction();
    if ((save_action == OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY) &&
        (os_port_tms570_update_running_task_sp(os_port_tms570_state.CurrentTaskSp) != E_OK)) {
        save_action = OS_PORT_TMS570_FIQ_SAVE_IDLE_SYSTEM;
    }
    os_port_tms570_state.LastFiqSaveAction = save_action;
    os_port_tms570_state.LastFiqSaveContinuationAction =
        Os_Port_Tms570_PeekFiqSaveContinuationAction();
    return save_action;
}

void Os_Port_Tms570_FinishFiqContextSave(uint8 SaveAction)
{
    uint32 frame_bytes;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (SaveAction == OS_PORT_TMS570_FIQ_SAVE_NONE)) {
        return;
    }

    frame_bytes = os_port_tms570_get_fiq_context_frame_bytes(SaveAction);
    if (os_port_tms570_state.FiqContextDepth < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX) {
        os_port_tms570_fiq_context_frame_stack[os_port_tms570_state.FiqContextDepth] = frame_bytes;
    }
    os_port_tms570_state.LastSavedFiqContextBytes = frame_bytes;
    os_port_tms570_state.FiqInterruptStackBytes += frame_bytes;
    if (os_port_tms570_state.FiqInterruptStackBytes > os_port_tms570_state.FiqInterruptStackPeakBytes) {
        os_port_tms570_state.FiqInterruptStackPeakBytes = os_port_tms570_state.FiqInterruptStackBytes;
    }
    os_port_tms570_state.FiqContextSaveCount++;
    os_port_tms570_state.FiqContextDepth++;
    if (SaveAction == OS_PORT_TMS570_FIQ_SAVE_NESTED_FIQ) {
        os_port_tms570_state.NestedFiqReturnCount++;
    } else {
        os_port_tms570_state.FiqProcessingEnterCount++;
    }
    Os_Port_Tms570_EnterFiq();
}

uint8 Os_Port_Tms570_PeekFiqRestoreAction(void)
{
    TaskType current_task;
    boolean handoff_pending;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FiqContextDepth == 0u)) {
        return OS_PORT_TMS570_FIQ_RESTORE_NONE;
    }

    if (os_port_tms570_state.FiqContextDepth > 1u) {
        return OS_PORT_TMS570_FIQ_RESTORE_NESTED_RETURN;
    }

    if (os_port_tms570_state.FiqResumeMode == OS_PORT_TMS570_MODE_IRQ) {
        return OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE;
    }

    current_task = os_port_tms570_state.CurrentTask;
    if ((os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (current_task == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(current_task) == FALSE) ||
        (os_port_tms570_task_context[current_task].Prepared == FALSE)) {
        return OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM;
    }

    if (os_port_tms570_state.FiqPreemptDisable == TRUE) {
        return OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE;
    }

    handoff_pending = (boolean)((os_port_tms570_state.DispatchRequested == TRUE) ||
                                (os_port_tms570_state.DeferredDispatch == TRUE));
    if ((handoff_pending == TRUE) &&
        (os_port_tms570_state.SelectedNextTask != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(os_port_tms570_state.SelectedNextTask) == TRUE) &&
        (os_port_tms570_task_context[os_port_tms570_state.SelectedNextTask].Prepared == TRUE) &&
        (os_port_tms570_state.SelectedNextTask != current_task)) {
        return OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER;
    }

    return OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE;
}

uint8 Os_Port_Tms570_BeginFiqContextRestore(void)
{
    uint8 restore_action = Os_Port_Tms570_PeekFiqRestoreAction();
    uint8 frame_index;
    uint32 frame_bytes = 0u;

    os_port_tms570_state.LastFiqRestoreAction = restore_action;
    if (restore_action != OS_PORT_TMS570_FIQ_RESTORE_NONE) {
        frame_index = (uint8)(os_port_tms570_state.FiqContextDepth - 1u);
        if (frame_index < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX) {
            frame_bytes = os_port_tms570_fiq_context_frame_stack[frame_index];
            os_port_tms570_fiq_context_frame_stack[frame_index] = 0u;
        }
        os_port_tms570_state.LastRestoredFiqContextBytes = frame_bytes;
        if (os_port_tms570_state.FiqInterruptStackBytes >= frame_bytes) {
            os_port_tms570_state.FiqInterruptStackBytes -= frame_bytes;
        } else {
            os_port_tms570_state.FiqInterruptStackBytes = 0u;
        }
        os_port_tms570_state.FiqContextRestoreCount++;
        os_port_tms570_state.FiqContextDepth--;
    }
    return restore_action;
}

void Os_Port_Tms570_FinishFiqContextRestore(uint8 RestoreAction)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (RestoreAction == OS_PORT_TMS570_FIQ_RESTORE_NONE)) {
        return;
    }

    Os_Port_Tms570_ExitFiq();

    if ((RestoreAction == OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM) ||
        (RestoreAction == OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER)) {
        if ((RestoreAction == OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER) &&
            (os_port_tms570_state.CurrentTask != INVALID_TASK) &&
            (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == TRUE) &&
            (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == TRUE)) {
            os_port_tms570_state.LastSavedTask = os_port_tms570_state.CurrentTask;
            os_port_tms570_state.LastSavedTaskSp =
                os_port_tms570_task_context[os_port_tms570_state.CurrentTask].RuntimeSp;
            os_port_tms570_state.LastSavedTimeSlice = 0u;
            if (os_port_tms570_state.CurrentTimeSlice > 0u) {
                os_port_tms570_state.LastSavedTimeSlice = os_port_tms570_state.CurrentTimeSlice;
                os_port_tms570_task_context[os_port_tms570_state.CurrentTask].SavedTimeSlice =
                    os_port_tms570_state.CurrentTimeSlice;
                os_port_tms570_state.CurrentTimeSlice = 0u;
            }
            os_port_tms570_state.CurrentTask = INVALID_TASK;
            os_port_tms570_state.CurrentTaskSp = (uintptr_t)0u;
        }
        os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
        os_port_tms570_state.FiqSchedulerReturnCount++;
    }
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

    /* Match the local ThreadX timer ISR countdown without overclaiming
     * round-robin behavior we do not model yet in this bootstrap.
     */
    if (os_port_tms570_state.CurrentTimeSlice > 0u) {
        os_port_tms570_state.CurrentTimeSlice--;
        if (os_port_tms570_state.CurrentTimeSlice == 0u) {
            os_port_tms570_state.TimeSliceExpirationCount++;
            os_port_tms570_state.TimeSliceServicePending = TRUE;
        }
    }

    if (Os_BootstrapProcessCounterTick() == TRUE) {
        Os_PortRequestContextSwitch();
    }

    os_port_tms570_service_expired_time_slice();
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
    uint8 save_action;

    save_action = Os_Port_Tms570_BeginFiqContextSave();
    if (save_action == OS_PORT_TMS570_FIQ_SAVE_NONE) {
        return;
    }

    Os_Port_Tms570_FinishFiqContextSave(save_action);
}

void Os_Port_Tms570_FiqContextRestore(void)
{
    uint8 restore_action = Os_Port_Tms570_BeginFiqContextRestore();

    if (restore_action == OS_PORT_TMS570_FIQ_RESTORE_NONE) {
        return;
    }

    Os_Port_Tms570_FinishFiqContextRestore(restore_action);
}

void Os_Port_Tms570_RtiTickServiceCore(void)
{
    os_port_tms570_acknowledge_rti_compare0();
    Os_Port_Tms570_TickIsr();
}

void Os_Port_Tms570_RtiTickHandler(void)
{
    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();
    Os_Port_Tms570_RtiTickServiceCore();
    Os_Port_Tms570_IrqNestingEnd();
    Os_Port_Tms570_IrqContextRestore();
}

void Os_Port_Tms570_FiqProcessingStart(void)
{
    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqNestingStart();
}

void Os_Port_Tms570_FiqProcessingEnd(void)
{
    Os_Port_Tms570_FiqNestingEnd();
    Os_Port_Tms570_FiqContextRestore();
}

StatusType Os_Port_Tms570_TestSetIrqReturnAddress(uintptr_t Address)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Address == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentIrqReturnAddress = Address;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetFiqReturnAddress(uintptr_t Address)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Address == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentFiqReturnAddress = Address;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetFiqProcessingReturnAddress(uintptr_t Address)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Address == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentFiqProcessingReturnAddress = Address;
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

StatusType Os_Port_Tms570_TestSetRtiIntFlag(uint32 Flags)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    os_port_tms570_state.RtiIntflag = Flags;
    os_port_tms570_sync_rti_compare0_vim_request();
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetVimChannelEnabled(uint32 Channel, boolean Enabled)
{
    uint32 mask;

    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Channel >= 32u)) {
        return E_OS_STATE;
    }

    mask = ((uint32)1u << Channel);
    if (Enabled == TRUE) {
        os_port_tms570_state.VimReqmaskset0 |= mask;
        os_port_tms570_state.VimReqmaskclr0 &= ~mask;
    } else {
        os_port_tms570_state.VimReqmaskset0 &= ~mask;
        os_port_tms570_state.VimReqmaskclr0 |= mask;
    }

    if (Channel == OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL) {
        os_port_tms570_sync_rti_compare0_vim_request();
    }

    return E_OK;
}

StatusType Os_Port_Tms570_TestInvokeVimChannel(uint32 Channel)
{
    if ((Channel == os_port_tms570_get_highest_pending_irq_channel()) &&
        (Channel == OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL)) {
        return Os_Port_Tms570_DispatchPendingIrq();
    }

    return E_OS_NOFUNC;
}

StatusType Os_Port_Tms570_SelectPendingIrq(void)
{
    uintptr_t vector_address;
    uint32 channel;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    channel = os_port_tms570_get_highest_pending_irq_channel();
    if (channel == OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL) {
        os_port_tms570_state.VimIrqIndex = OS_PORT_TMS570_VIM_RTI_COMPARE0_IRQINDEX;
        if (Os_Port_Tms570_ReadActiveIrqVector(&vector_address) != E_OK) {
            os_port_tms570_state.VimIrqIndex = 0u;
            return E_OS_NOFUNC;
        }
        os_port_tms570_state.VimIrqVecReg = vector_address;
        return E_OK;
    }

    os_port_tms570_state.VimIrqIndex = 0u;
    os_port_tms570_state.VimIrqVecReg = (uintptr_t)0u;
    return E_OS_NOFUNC;
}

StatusType Os_Port_Tms570_ReadMappedChannelForRequest(uint32 Request, uint32* Channel)
{
    uint32 encoded_request;
    uint32 slot;
    uint32 shift;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if (Channel == NULL_PTR) {
        return E_OS_VALUE;
    }

    if (Request > 0xFFu) {
        return E_OS_NOFUNC;
    }

    for (slot = 0u; slot < 4u; slot++) {
        shift = (3u - slot) * 8u;
        encoded_request = (os_port_tms570_state.VimChanctrl0 >> shift) & 0xFFu;
        if (encoded_request == Request) {
            *Channel = slot;
            return E_OK;
        }
    }

    return E_OS_NOFUNC;
}

StatusType Os_Port_Tms570_ServiceActiveIrq(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if (os_port_tms570_has_selected_rti_compare0_irq() == FALSE) {
        return E_OS_NOFUNC;
    }

    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();
    (void)Os_Port_Tms570_ServiceActiveIrqCore();
    Os_Port_Tms570_IrqNestingEnd();
    Os_Port_Tms570_IrqContextRestore();

    return E_OK;
}

StatusType Os_Port_Tms570_ServiceActiveIrqCore(void)
{
    uint32 channel;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((Os_Port_Tms570_ReadActiveIrqChannel(&channel) != E_OK) ||
        (channel != OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL) ||
        (os_port_tms570_has_selected_rti_compare0_irq() == FALSE)) {
        return E_OS_NOFUNC;
    }

    if (Os_Port_Tms570_PulseActiveIrqMask() != E_OK) {
        return E_OS_NOFUNC;
    }
    if (Os_Port_Tms570_InvokeActiveIrqVectorCore() != E_OK) {
        return E_OS_NOFUNC;
    }
    os_port_tms570_state.VimLastServicedChannel = OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL;
    os_port_tms570_state.VimLastIrqIndex = os_port_tms570_state.VimIrqIndex;
    os_port_tms570_state.VimLastIrqVecReg = os_port_tms570_state.VimIrqVecReg;
    os_port_tms570_state.VimIntreq0 &= ~OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK;
    os_port_tms570_state.VimIrqIndex = 0u;
    os_port_tms570_state.VimIrqVecReg = (uintptr_t)0u;

    return E_OK;
}

StatusType Os_Port_Tms570_ReadActiveIrqChannel(uint32* Channel)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if (Channel == NULL_PTR) {
        return E_OS_VALUE;
    }

    if (os_port_tms570_state.VimIrqIndex == 0u) {
        return E_OS_NOFUNC;
    }

    *Channel = os_port_tms570_state.VimIrqIndex - 1u;
    return E_OK;
}

StatusType Os_Port_Tms570_ReadActiveIrqVector(uintptr_t* VectorAddress)
{
    uint32 channel;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if (VectorAddress == NULL_PTR) {
        return E_OS_VALUE;
    }

    if (Os_Port_Tms570_ReadActiveIrqChannel(&channel) != E_OK) {
        return E_OS_NOFUNC;
    }

    if ((channel + 1u) >= OS_PORT_TMS570_VIM_ISR_TABLE_SLOTS) {
        return E_OS_NOFUNC;
    }

    if (os_port_tms570_vim_isr_table[channel + 1u] == (uintptr_t)0u) {
        return E_OS_NOFUNC;
    }

    *VectorAddress = os_port_tms570_vim_isr_table[channel + 1u];
    return E_OK;
}

StatusType Os_Port_Tms570_PulseActiveIrqMask(void)
{
    uint32 channel;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if (Os_Port_Tms570_ReadActiveIrqChannel(&channel) != E_OK) {
        return E_OS_NOFUNC;
    }

    if (channel == OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL) {
        os_port_tms570_state.VimReqmaskclr0 |= OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK;
        os_port_tms570_state.VimReqmaskset0 |= OS_PORT_TMS570_VIM_RTI_COMPARE0_MASK;
        return E_OK;
    }

    return E_OS_NOFUNC;
}

StatusType Os_Port_Tms570_InvokeActiveIrqVectorCore(void)
{
    uintptr_t vector_address;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if (Os_Port_Tms570_ReadActiveIrqVector(&vector_address) != E_OK) {
        return E_OS_NOFUNC;
    }

    if (vector_address == os_port_tms570_state.VimRtiCompare0HandlerAddress) {
        Os_Port_Tms570_RtiTickServiceCore();
        return E_OK;
    }

    return E_OS_NOFUNC;
}

StatusType Os_Port_Tms570_DispatchPendingIrq(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((os_port_tms570_state.VimIrqIndex == 0u) &&
        (os_port_tms570_state.VimIrqVecReg == (uintptr_t)0u) &&
        (Os_Port_Tms570_SelectPendingIrq() != E_OK)) {
        return E_OS_NOFUNC;
    }

    return Os_Port_Tms570_VimIrqEntry();
}

StatusType Os_Port_Tms570_VimIrqEntryCore(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((os_port_tms570_state.VimIrqIndex == 0u) ||
        (os_port_tms570_state.VimIrqVecReg == (uintptr_t)0u)) {
        if (Os_Port_Tms570_SelectPendingIrq() != E_OK) {
            return E_OS_NOFUNC;
        }
    }

    return Os_Port_Tms570_ServiceActiveIrqCore();
}

StatusType Os_Port_Tms570_VimIrqEntry(void)
{
    StatusType service_status;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();
    service_status = Os_Port_Tms570_VimIrqEntryCore();
    Os_Port_Tms570_IrqNestingEnd();
    Os_Port_Tms570_IrqContextRestore();

    return service_status;
}

StatusType Os_Port_Tms570_TestSelectPendingIrq(void)
{
    return Os_Port_Tms570_SelectPendingIrq();
}

StatusType Os_Port_Tms570_TestServiceActiveIrq(void)
{
    return Os_Port_Tms570_ServiceActiveIrq();
}

StatusType Os_Port_Tms570_TestDispatchPendingIrq(void)
{
    return Os_Port_Tms570_DispatchPendingIrq();
}

StatusType Os_Port_Tms570_TestSetRtiCompare0NotificationEnabled(boolean Enabled)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if (Enabled == TRUE) {
        /* Mirror HALCoGen rtiEnableNotification(): clear pending flag then set enable. */
        os_port_tms570_state.RtiIntflag &= ~OS_PORT_TMS570_RTI_COMPARE0_INTFLAG;
        os_port_tms570_state.RtiSetintena |= OS_PORT_TMS570_RTI_COMPARE0_INTFLAG;
        os_port_tms570_state.RtiClearintena &= ~OS_PORT_TMS570_RTI_COMPARE0_INTFLAG;
    } else {
        /* Mirror HALCoGen rtiDisableNotification(): latch clear-enable write and clear enable. */
        os_port_tms570_state.RtiClearintena |= OS_PORT_TMS570_RTI_COMPARE0_INTFLAG;
        os_port_tms570_state.RtiSetintena &= ~OS_PORT_TMS570_RTI_COMPARE0_INTFLAG;
    }

    os_port_tms570_sync_rti_compare0_vim_request();

    return E_OK;
}

StatusType Os_Port_Tms570_TestSetRtiCounter0Enabled(boolean Enabled)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if (Enabled == TRUE) {
        os_port_tms570_state.RtiGctrl |= OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE;
    } else {
        os_port_tms570_state.RtiGctrl &= ~OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE;
    }

    os_port_tms570_sync_rti_compare0_vim_request();

    return E_OK;
}

StatusType Os_Port_Tms570_TestRaiseRtiCompare0Interrupt(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    os_port_tms570_state.RtiIntflag |= OS_PORT_TMS570_RTI_COMPARE0_INTFLAG;
    os_port_tms570_sync_rti_compare0_vim_request();

    return E_OK;
}

StatusType Os_Port_Tms570_TestAdvanceRtiCounter0(uint32 Ticks)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if (((os_port_tms570_state.RtiGctrl & OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE) == 0u) ||
        (Ticks == 0u)) {
        return E_OK;
    }

    os_port_tms570_state.RtiCounter0Value += Ticks;
    if ((os_port_tms570_state.RtiCounter0Value >= os_port_tms570_state.RtiCmp0Comp) &&
        ((os_port_tms570_state.RtiIntflag & OS_PORT_TMS570_RTI_COMPARE0_INTFLAG) == 0u)) {
        return Os_Port_Tms570_TestRaiseRtiCompare0Interrupt();
    }

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

StatusType Os_Port_Tms570_TestSetCurrentTimeSlice(uint32 TimeSlice)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentTimeSlice = TimeSlice;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetTaskSavedTimeSlice(TaskType TaskID, uint32 TimeSlice)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE)) {
        return E_OS_STATE;
    }

    os_port_tms570_task_context[TaskID].SavedTimeSlice = TimeSlice;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetPreemptDisable(boolean Enabled)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    os_port_tms570_state.FiqPreemptDisable = Enabled;
    return E_OK;
}

StatusType Os_Port_Tms570_TestInvokeFiq(Os_TestIsrHandlerType Handler)
{
    if (Handler == (Os_TestIsrHandlerType)0) {
        return E_OS_VALUE;
    }

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    Os_Port_Tms570_FiqProcessingStart();
    Handler();
    Os_Port_Tms570_FiqProcessingEnd();
    return E_OK;
}
#endif

#endif
