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

#if !defined(UNIT_TEST)
#include "HL_sys_vim.h"
#include "HL_reg_rti.h"
#endif

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
static uint32 os_port_tms570_irq_saved_cpsr_stack[OS_PORT_TMS570_IRQ_RETURN_STACK_MAX];
static Os_Port_Tms570_IrqScratchSnapshotType
    os_port_tms570_irq_scratch_stack[OS_PORT_TMS570_IRQ_RETURN_STACK_MAX];
static uintptr_t os_port_tms570_fiq_return_stack[OS_PORT_TMS570_FIQ_RETURN_STACK_MAX];
static uint32 os_port_tms570_fiq_saved_cpsr_stack[OS_PORT_TMS570_FIQ_RETURN_STACK_MAX];
static Os_Port_Tms570_IrqScratchSnapshotType
    os_port_tms570_fiq_scratch_stack[OS_PORT_TMS570_FIQ_RETURN_STACK_MAX];
static uint32 os_port_tms570_irq_context_frame_stack[OS_PORT_TMS570_IRQ_RETURN_STACK_MAX];
static uint32 os_port_tms570_fiq_context_frame_stack[OS_PORT_TMS570_FIQ_RETURN_STACK_MAX];
static uintptr_t os_port_tms570_irq_processing_return_stack
    [OS_PORT_TMS570_IRQ_PROCESSING_RETURN_STACK_MAX];
static uintptr_t os_port_tms570_fiq_processing_return_stack
    [OS_PORT_TMS570_FIQ_PROCESSING_RETURN_STACK_MAX];
static Os_Port_Tms570_InterruptContextType os_port_tms570_pending_irq_save_context;
static Os_Port_Tms570_InterruptContextType os_port_tms570_pending_fiq_save_context;
static Os_Port_Tms570_TaskFrameViewType os_port_tms570_pending_first_task_frame_view;
static Os_Port_Tms570_TaskFrameViewType os_port_tms570_pending_solicited_save_task_frame_view;
static Os_Port_Tms570_TaskFrameViewType os_port_tms570_pending_irq_save_task_frame_view;
static Os_Port_Tms570_TaskFrameViewType os_port_tms570_pending_fiq_save_task_frame_view;
static Os_Port_Tms570_TaskFrameViewType os_port_tms570_pending_irq_restore_task_frame_view;
static Os_Port_Tms570_TaskFrameViewType os_port_tms570_pending_irq_scheduler_return_task_frame_view;
static Os_Port_Tms570_TaskFrameViewType os_port_tms570_pending_fiq_restore_task_frame_view;
static Os_Port_Tms570_TaskFrameViewType os_port_tms570_pending_fiq_scheduler_return_task_frame_view;

static uint32 os_port_tms570_get_task_frame_bytes(const Os_Port_Tms570_TaskFrameType* Frame);

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

    if ((os_port_tms570_state.CurrentTask == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == FALSE) ||
        (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == FALSE)) {
        return OS_PORT_TMS570_RESTORE_IDLE_SYSTEM;
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
            os_port_tms570_task_context[current_task].RuntimeFrame.TimeSlice;
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

static void os_port_tms570_clear_task_frame(Os_Port_Tms570_TaskFrameType* Frame)
{
    uint32 idx;

    Frame->Valid = FALSE;
    Frame->Sp = (uintptr_t)0u;
    Frame->ReturnAddress = (uintptr_t)0u;
    Frame->LinkRegister = (uintptr_t)0u;
    Frame->TimeSlice = 0u;
    Frame->Cpsr = 0u;
    Frame->StackType = 0u;
    Frame->TaskLower.R0 = 0u;
    Frame->TaskLower.R1 = 0u;
    Frame->TaskLower.R2 = 0u;
    Frame->TaskLower.R3 = 0u;
    Frame->IrqScratch.R0 = 0u;
    Frame->IrqScratch.R1 = 0u;
    Frame->IrqScratch.R2 = 0u;
    Frame->IrqScratch.R3 = 0u;
    Frame->IrqScratch.R10 = 0u;
    Frame->IrqScratch.R12 = 0u;
    Frame->Preserved.R4 = 0u;
    Frame->Preserved.R5 = 0u;
    Frame->Preserved.R6 = 0u;
    Frame->Preserved.R7 = 0u;
    Frame->Preserved.R8 = 0u;
    Frame->Preserved.R9 = 0u;
    Frame->Preserved.R10 = 0u;
    Frame->Preserved.R11 = 0u;
    Frame->Preserved.R12 = 0u;
    Frame->Vfp.Enabled = FALSE;
    Frame->Vfp.Fpscr = 0u;
    for (idx = 0u; idx < 16u; idx++) {
        Frame->Vfp.D[idx].Low = 0u;
        Frame->Vfp.D[idx].High = 0u;
    }
}

static void os_port_tms570_clear_task_frame_view(Os_Port_Tms570_TaskFrameViewType* View)
{
    os_port_tms570_clear_task_frame(&View->Frame);
    View->FrameBytes = 0u;
}

static void os_port_tms570_clear_task_lower_snapshot(Os_Port_Tms570_TaskLowerSnapshotType* Snapshot)
{
    Snapshot->R0 = 0u;
    Snapshot->R1 = 0u;
    Snapshot->R2 = 0u;
    Snapshot->R3 = 0u;
}

static void os_port_tms570_clear_irq_scratch_snapshot(Os_Port_Tms570_IrqScratchSnapshotType* Snapshot)
{
    Snapshot->R0 = 0u;
    Snapshot->R1 = 0u;
    Snapshot->R2 = 0u;
    Snapshot->R3 = 0u;
    Snapshot->R10 = 0u;
    Snapshot->R12 = 0u;
}

static void os_port_tms570_clear_preserved_snapshot(Os_Port_Tms570_PreservedSnapshotType* Snapshot)
{
    Snapshot->R4 = 0u;
    Snapshot->R5 = 0u;
    Snapshot->R6 = 0u;
    Snapshot->R7 = 0u;
    Snapshot->R8 = 0u;
    Snapshot->R9 = 0u;
    Snapshot->R10 = 0u;
    Snapshot->R11 = 0u;
    Snapshot->R12 = 0u;
}

static void os_port_tms570_clear_vfp_state(Os_Port_Tms570_VfpStateType* State)
{
    uint32 idx;

    State->Enabled = FALSE;
    State->Fpscr = 0u;
    for (idx = 0u; idx < 16u; idx++) {
        State->D[idx].Low = 0u;
        State->D[idx].High = 0u;
    }
}

static void os_port_tms570_clear_interrupt_context(Os_Port_Tms570_InterruptContextType* Context)
{
    Context->Valid = FALSE;
    Context->ReturnAddress = (uintptr_t)0u;
    Context->Cpsr = 0u;
    Context->FrameBytes = 0u;
    os_port_tms570_clear_irq_scratch_snapshot(&Context->Scratch);
}

static void os_port_tms570_set_interrupt_context(
    Os_Port_Tms570_InterruptContextType* Context,
    uintptr_t ReturnAddress,
    uint32 Cpsr,
    uint32 FrameBytes,
    const Os_Port_Tms570_IrqScratchSnapshotType* Scratch)
{
    Context->Valid = TRUE;
    Context->ReturnAddress = ReturnAddress;
    Context->Cpsr = Cpsr;
    Context->FrameBytes = FrameBytes;
    if ((Scratch != NULL_PTR) && (FrameBytes > 0u)) {
        Context->Scratch = *Scratch;
    } else {
        os_port_tms570_clear_irq_scratch_snapshot(&Context->Scratch);
    }
}

static void os_port_tms570_set_task_frame_view(
    Os_Port_Tms570_TaskFrameViewType* View,
    const Os_Port_Tms570_TaskFrameType* Frame,
    uint32 FrameBytes)
{
    if (View == NULL_PTR) {
        return;
    }

    if ((Frame == NULL_PTR) || (Frame->Valid == FALSE)) {
        os_port_tms570_clear_task_frame_view(View);
        return;
    }

    View->Frame = *Frame;
    View->FrameBytes = FrameBytes;
}

static void os_port_tms570_build_task_frame_view_from_interrupt_context(
    Os_Port_Tms570_TaskFrameViewType* View,
    const Os_Port_Tms570_TaskFrameType* Frame,
    const Os_Port_Tms570_InterruptContextType* InterruptContext)
{
    Os_Port_Tms570_TaskFrameType effective_frame;
    uint32 frame_bytes;

    if ((View == NULL_PTR) || (Frame == NULL_PTR) || (Frame->Valid == FALSE)) {
        if (View != NULL_PTR) {
            os_port_tms570_clear_task_frame_view(View);
        }
        return;
    }

    effective_frame = *Frame;
    frame_bytes = os_port_tms570_get_task_frame_bytes(Frame);
    if ((InterruptContext != NULL_PTR) && (InterruptContext->Valid == TRUE)) {
        effective_frame.ReturnAddress = InterruptContext->ReturnAddress;
        effective_frame.Cpsr = InterruptContext->Cpsr;
        effective_frame.IrqScratch = InterruptContext->Scratch;
        frame_bytes = InterruptContext->FrameBytes;
    }

    os_port_tms570_set_task_frame_view(View, &effective_frame, frame_bytes);
}

static void os_port_tms570_publish_last_saved_task_frame_view(void)
{
    os_port_tms570_state.LastSavedTaskSp = os_port_tms570_state.LastSavedTaskFrameView.Frame.Sp;
    os_port_tms570_state.LastSavedTaskFrameBytes = os_port_tms570_state.LastSavedTaskFrameView.FrameBytes;
}

static void os_port_tms570_publish_last_restored_task_frame_view(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = &os_port_tms570_state.LastRestoredTaskFrameView;

    os_port_tms570_state.LastRestoredTaskSp = view->Frame.Sp;
    os_port_tms570_state.LastRestoredTaskReturnAddress = view->Frame.ReturnAddress;
    os_port_tms570_state.LastRestoredTaskLinkRegister = view->Frame.LinkRegister;
    os_port_tms570_state.LastRestoredTaskCpsr = view->Frame.Cpsr;
    os_port_tms570_state.LastRestoredTaskStackType = view->Frame.StackType;
    os_port_tms570_state.LastRestoredTaskLower = view->Frame.TaskLower;
    os_port_tms570_state.LastRestoredTaskIrqScratch = view->Frame.IrqScratch;
    os_port_tms570_state.LastRestoredTaskPreserved = view->Frame.Preserved;
    os_port_tms570_state.LastRestoredTaskVfp = view->Frame.Vfp;
    os_port_tms570_state.LastRestoredTaskFrameBytes = view->FrameBytes;
}

static void os_port_tms570_set_initial_task_frame(Os_Port_Tms570_TaskContextType* Context,
                                                  uintptr_t Sp,
                                                  Os_TaskEntryType Entry)
{
    Context->InitialFrame.Valid = TRUE;
    Context->InitialFrame.Sp = Sp;
    Context->InitialFrame.ReturnAddress = (uintptr_t)Entry;
    Context->InitialFrame.LinkRegister = (uintptr_t)0u;
    Context->InitialFrame.TimeSlice = 0u;
    Context->InitialFrame.Cpsr = OS_PORT_TMS570_INITIAL_CPSR;
    Context->InitialFrame.StackType = OS_PORT_TMS570_INITIAL_STACK_TYPE;
    Context->SavedSp = Sp;
}

static void os_port_tms570_set_runtime_task_sp(Os_Port_Tms570_TaskContextType* Context, uintptr_t Sp)
{
    boolean was_valid = Context->RuntimeFrame.Valid;

    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.Sp = Sp;
    if (Context->RuntimeFrame.Cpsr == 0u) {
        Context->RuntimeFrame.Cpsr = OS_PORT_TMS570_INITIAL_CPSR;
    }
    if (Context->RuntimeFrame.ReturnAddress == (uintptr_t)0u) {
        Context->RuntimeFrame.ReturnAddress = Context->InitialFrame.ReturnAddress;
    }
    if (Context->RuntimeFrame.LinkRegister == (uintptr_t)0u) {
        Context->RuntimeFrame.LinkRegister = Context->InitialFrame.LinkRegister;
    }
    if ((was_valid == FALSE) && (Context->RuntimeFrame.StackType == 0u)) {
        Context->RuntimeFrame.StackType = OS_PORT_TMS570_INITIAL_STACK_TYPE;
    }
    Context->RuntimeSp = Sp;
}

static void os_port_tms570_set_runtime_task_time_slice(Os_Port_Tms570_TaskContextType* Context,
                                                       uint32 TimeSlice)
{
    boolean was_valid = Context->RuntimeFrame.Valid;

    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.TimeSlice = TimeSlice;
    if (Context->RuntimeFrame.Cpsr == 0u) {
        Context->RuntimeFrame.Cpsr = OS_PORT_TMS570_INITIAL_CPSR;
    }
    if ((was_valid == FALSE) && (Context->RuntimeFrame.StackType == 0u)) {
        Context->RuntimeFrame.StackType = OS_PORT_TMS570_INITIAL_STACK_TYPE;
    }
    Context->SavedTimeSlice = TimeSlice;
}

static void os_port_tms570_set_runtime_task_link_register(Os_Port_Tms570_TaskContextType* Context,
                                                          uintptr_t LinkRegister)
{
    boolean was_valid = Context->RuntimeFrame.Valid;

    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.LinkRegister = LinkRegister;
    if (Context->RuntimeFrame.Cpsr == 0u) {
        Context->RuntimeFrame.Cpsr = OS_PORT_TMS570_INITIAL_CPSR;
    }
    if (Context->RuntimeFrame.ReturnAddress == (uintptr_t)0u) {
        Context->RuntimeFrame.ReturnAddress = Context->InitialFrame.ReturnAddress;
    }
    if ((was_valid == FALSE) && (Context->RuntimeFrame.StackType == 0u)) {
        Context->RuntimeFrame.StackType = OS_PORT_TMS570_INITIAL_STACK_TYPE;
    }
}

static void os_port_tms570_set_runtime_task_vfp(Os_Port_Tms570_TaskContextType* Context,
                                                const Os_Port_Tms570_VfpStateType* State)
{
    boolean was_valid;

    if (State == NULL_PTR) {
        return;
    }

    was_valid = Context->RuntimeFrame.Valid;
    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.Vfp = *State;
    if (Context->RuntimeFrame.Cpsr == 0u) {
        Context->RuntimeFrame.Cpsr = OS_PORT_TMS570_INITIAL_CPSR;
    }
    if (Context->RuntimeFrame.ReturnAddress == (uintptr_t)0u) {
        Context->RuntimeFrame.ReturnAddress = Context->InitialFrame.ReturnAddress;
    }
    if ((was_valid == FALSE) && (Context->RuntimeFrame.StackType == 0u)) {
        Context->RuntimeFrame.StackType = OS_PORT_TMS570_INITIAL_STACK_TYPE;
    }
}

static void os_port_tms570_set_runtime_task_lower(Os_Port_Tms570_TaskContextType* Context,
                                                  const Os_Port_Tms570_TaskLowerSnapshotType* Snapshot)
{
    boolean was_valid;

    if (Snapshot == NULL_PTR) {
        return;
    }

    was_valid = Context->RuntimeFrame.Valid;
    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.TaskLower = *Snapshot;
    if (Context->RuntimeFrame.Cpsr == 0u) {
        Context->RuntimeFrame.Cpsr = OS_PORT_TMS570_INITIAL_CPSR;
    }
    if (Context->RuntimeFrame.ReturnAddress == (uintptr_t)0u) {
        Context->RuntimeFrame.ReturnAddress = Context->InitialFrame.ReturnAddress;
    }
    if ((was_valid == FALSE) && (Context->RuntimeFrame.StackType == 0u)) {
        Context->RuntimeFrame.StackType = OS_PORT_TMS570_INITIAL_STACK_TYPE;
    }
}

static void os_port_tms570_capture_runtime_task_irq_metadata(Os_Port_Tms570_TaskContextType* Context,
                                                             uintptr_t ReturnAddress)
{
    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.ReturnAddress = ReturnAddress;
    Context->RuntimeFrame.IrqScratch = os_port_tms570_state.CurrentIrqScratch;
    Context->RuntimeFrame.Cpsr = os_port_tms570_state.CurrentIrqSavedCpsr;
    Context->RuntimeFrame.StackType = OS_PORT_TMS570_INITIAL_STACK_TYPE;
}

static void os_port_tms570_capture_runtime_task_interrupt_context(
    Os_Port_Tms570_TaskContextType* Context,
    const Os_Port_Tms570_InterruptContextType* InterruptContext)
{
    if ((Context == NULL_PTR) || (InterruptContext == NULL_PTR) ||
        (InterruptContext->Valid == FALSE)) {
        return;
    }

    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.ReturnAddress = InterruptContext->ReturnAddress;
    Context->RuntimeFrame.IrqScratch = InterruptContext->Scratch;
    Context->RuntimeFrame.Cpsr = InterruptContext->Cpsr;
    Context->RuntimeFrame.StackType = OS_PORT_TMS570_INITIAL_STACK_TYPE;
}

static void os_port_tms570_capture_runtime_task_from_restored_irq_context(
    Os_Port_Tms570_TaskContextType* Context)
{
    if (os_port_tms570_state.LastRestoredIrqContext.Valid == TRUE) {
        os_port_tms570_capture_runtime_task_interrupt_context(
            Context, &os_port_tms570_state.LastRestoredIrqContext);
    } else {
        os_port_tms570_capture_runtime_task_irq_metadata(
            Context,
            (os_port_tms570_state.CurrentIrqReturnAddress != (uintptr_t)0u)
                ? os_port_tms570_state.CurrentIrqReturnAddress
                : Context->RuntimeFrame.ReturnAddress);
    }
}

static void os_port_tms570_capture_runtime_task_from_restored_fiq_context(
    Os_Port_Tms570_TaskContextType* Context)
{
    if (os_port_tms570_state.LastRestoredFiqContext.Valid == TRUE) {
        os_port_tms570_capture_runtime_task_interrupt_context(
            Context, &os_port_tms570_state.LastRestoredFiqContext);
    } else {
        Context->RuntimeFrame.Valid = TRUE;
        Context->RuntimeFrame.ReturnAddress =
            (os_port_tms570_state.LastRestoredFiqReturnAddress != (uintptr_t)0u)
                ? os_port_tms570_state.LastRestoredFiqReturnAddress
                : ((os_port_tms570_state.CurrentFiqReturnAddress != (uintptr_t)0u)
                       ? os_port_tms570_state.CurrentFiqReturnAddress
                       : Context->RuntimeFrame.ReturnAddress);
        Context->RuntimeFrame.IrqScratch = os_port_tms570_state.CurrentFiqScratch;
        Context->RuntimeFrame.Cpsr = os_port_tms570_state.CurrentFiqSavedCpsr;
        Context->RuntimeFrame.StackType = OS_PORT_TMS570_INITIAL_STACK_TYPE;
    }
}

static void os_port_tms570_capture_runtime_task_preserved(Os_Port_Tms570_TaskContextType* Context)
{
    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.Preserved = os_port_tms570_state.CurrentTaskPreserved;
}

static void os_port_tms570_capture_runtime_task_link_register(Os_Port_Tms570_TaskContextType* Context)
{
    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.LinkRegister = os_port_tms570_state.CurrentTaskLinkRegister;
}

static void os_port_tms570_capture_runtime_task_lower(Os_Port_Tms570_TaskContextType* Context)
{
    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.TaskLower = os_port_tms570_state.CurrentTaskLower;
}

static void os_port_tms570_capture_runtime_task_vfp(Os_Port_Tms570_TaskContextType* Context)
{
    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.Vfp = os_port_tms570_state.CurrentTaskVfp;
}

static void os_port_tms570_capture_solicited_runtime_frame(Os_Port_Tms570_TaskContextType* Context)
{
    uint32 idx;

    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.Sp = os_port_tms570_state.CurrentTaskSp;
    Context->RuntimeFrame.ReturnAddress = os_port_tms570_state.CurrentTaskLinkRegister;
    Context->RuntimeFrame.LinkRegister = os_port_tms570_state.CurrentTaskLinkRegister;
    Context->RuntimeFrame.TimeSlice = os_port_tms570_state.CurrentTimeSlice;
    Context->RuntimeFrame.Cpsr = os_port_tms570_state.CurrentIrqSavedCpsr;
    Context->RuntimeFrame.StackType = OS_PORT_TMS570_SOLICITED_STACK_TYPE;
    os_port_tms570_clear_task_lower_snapshot(&Context->RuntimeFrame.TaskLower);
    os_port_tms570_clear_irq_scratch_snapshot(&Context->RuntimeFrame.IrqScratch);
    Context->RuntimeFrame.Preserved.R4 = os_port_tms570_state.CurrentTaskPreserved.R4;
    Context->RuntimeFrame.Preserved.R5 = os_port_tms570_state.CurrentTaskPreserved.R5;
    Context->RuntimeFrame.Preserved.R6 = os_port_tms570_state.CurrentTaskPreserved.R6;
    Context->RuntimeFrame.Preserved.R7 = os_port_tms570_state.CurrentTaskPreserved.R7;
    Context->RuntimeFrame.Preserved.R8 = os_port_tms570_state.CurrentTaskPreserved.R8;
    Context->RuntimeFrame.Preserved.R9 = os_port_tms570_state.CurrentTaskPreserved.R9;
    Context->RuntimeFrame.Preserved.R10 = os_port_tms570_state.CurrentTaskPreserved.R10;
    Context->RuntimeFrame.Preserved.R11 = os_port_tms570_state.CurrentTaskPreserved.R11;
    Context->RuntimeFrame.Preserved.R12 = 0u;
    os_port_tms570_clear_vfp_state(&Context->RuntimeFrame.Vfp);
    if (os_port_tms570_state.CurrentTaskVfp.Enabled == TRUE) {
        Context->RuntimeFrame.Vfp.Enabled = TRUE;
        Context->RuntimeFrame.Vfp.Fpscr = os_port_tms570_state.CurrentTaskVfp.Fpscr;
        for (idx = 8u; idx < 16u; idx++) {
            Context->RuntimeFrame.Vfp.D[idx] = os_port_tms570_state.CurrentTaskVfp.D[idx];
        }
    }
    Context->RuntimeSp = os_port_tms570_state.CurrentTaskSp;
    Context->SavedTimeSlice = os_port_tms570_state.CurrentTimeSlice;
}

static uint32 os_port_tms570_get_task_frame_bytes(const Os_Port_Tms570_TaskFrameType* Frame)
{
    uint32 bytes;

    if ((Frame == NULL_PTR) || (Frame->Valid == FALSE)) {
        return 0u;
    }

    if (Frame->StackType == OS_PORT_TMS570_SOLICITED_STACK_TYPE) {
        bytes = OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES;
        if (Frame->Vfp.Enabled == TRUE) {
            bytes += OS_PORT_TMS570_SOLICITED_FRAME_VFP_BYTES;
        }
    } else {
        bytes = OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES;
        if (Frame->Vfp.Enabled == TRUE) {
            bytes += OS_PORT_TMS570_INTERRUPT_FRAME_VFP_BYTES;
        }
    }

    return bytes;
}

static uintptr_t os_port_tms570_get_saved_frame_sp(
    uintptr_t RunningSp,
    const Os_Port_Tms570_TaskFrameType* Frame)
{
    uint32 frame_bytes;

    frame_bytes = os_port_tms570_get_task_frame_bytes(Frame);
    if ((RunningSp == (uintptr_t)0u) || (frame_bytes == 0u) ||
        (RunningSp < (uintptr_t)frame_bytes)) {
        return (uintptr_t)0u;
    }

    return (RunningSp - (uintptr_t)frame_bytes);
}

static uintptr_t os_port_tms570_get_saved_sp_for_frame_bytes(uintptr_t RunningSp, uint32 FrameBytes)
{
    if ((RunningSp == (uintptr_t)0u) || (FrameBytes == 0u) || (RunningSp < (uintptr_t)FrameBytes)) {
        return (uintptr_t)0u;
    }

    return (RunningSp - (uintptr_t)FrameBytes);
}

static void os_port_tms570_commit_saved_runtime_frame(Os_Port_Tms570_TaskContextType* Context,
                                                      uintptr_t RunningSp)
{
    if ((Context == NULL_PTR) || (RunningSp == (uintptr_t)0u)) {
        return;
    }

    Context->RuntimeSp = RunningSp;
    Context->RuntimeFrame.Sp = os_port_tms570_get_saved_frame_sp(RunningSp, &Context->RuntimeFrame);
    Context->SavedSp = Context->RuntimeFrame.Sp;
}

static void os_port_tms570_commit_task_frame_view(Os_Port_Tms570_TaskContextType* Context,
                                                  const Os_Port_Tms570_TaskFrameViewType* View)
{
    if ((Context == NULL_PTR) || (View == NULL_PTR) || (View->Frame.Valid == FALSE)) {
        return;
    }

    Context->RuntimeFrame = View->Frame;
    Context->SavedSp = View->Frame.Sp;
    Context->SavedTimeSlice = View->Frame.TimeSlice;
}

static void os_port_tms570_commit_minimal_saved_runtime_sp(Os_Port_Tms570_TaskContextType* Context,
                                                           uintptr_t RunningSp,
                                                           uint32 FrameBytes)
{
    if ((Context == NULL_PTR) || (RunningSp == (uintptr_t)0u)) {
        return;
    }

    Context->RuntimeSp = RunningSp;
    Context->RuntimeFrame.Valid = TRUE;
    Context->RuntimeFrame.Sp = os_port_tms570_get_saved_sp_for_frame_bytes(RunningSp, FrameBytes);
    Context->SavedSp = Context->RuntimeFrame.Sp;
}

static void os_port_tms570_apply_solicited_vfp_restore_state(
    const Os_Port_Tms570_VfpStateType* State)
{
    uint32 idx;

    os_port_tms570_state.CurrentTaskVfp.Enabled = State->Enabled;
    if (State->Enabled == FALSE) {
        return;
    }

    os_port_tms570_state.CurrentTaskVfp.Fpscr = State->Fpscr;
    for (idx = 8u; idx < 16u; idx++) {
        os_port_tms570_state.CurrentTaskVfp.D[idx] = State->D[idx];
    }
}

static void os_port_tms570_apply_interrupt_runtime_frame_to_live_restore_state(
    const Os_Port_Tms570_TaskFrameType* Frame)
{
    os_port_tms570_state.CurrentTaskLower = Frame->TaskLower;
    os_port_tms570_state.CurrentIrqScratch = Frame->IrqScratch;
    os_port_tms570_state.CurrentTaskPreserved = Frame->Preserved;
    os_port_tms570_state.CurrentTaskVfp = Frame->Vfp;
}

static void os_port_tms570_apply_solicited_runtime_frame_to_live_restore_state(
    const Os_Port_Tms570_TaskFrameType* Frame)
{
    os_port_tms570_state.CurrentTaskPreserved.R4 = Frame->Preserved.R4;
    os_port_tms570_state.CurrentTaskPreserved.R5 = Frame->Preserved.R5;
    os_port_tms570_state.CurrentTaskPreserved.R6 = Frame->Preserved.R6;
    os_port_tms570_state.CurrentTaskPreserved.R7 = Frame->Preserved.R7;
    os_port_tms570_state.CurrentTaskPreserved.R8 = Frame->Preserved.R8;
    os_port_tms570_state.CurrentTaskPreserved.R9 = Frame->Preserved.R9;
    os_port_tms570_state.CurrentTaskPreserved.R10 = Frame->Preserved.R10;
    os_port_tms570_state.CurrentTaskPreserved.R11 = Frame->Preserved.R11;
    os_port_tms570_apply_solicited_vfp_restore_state(&Frame->Vfp);
}

static void os_port_tms570_apply_runtime_frame_to_live_restore_state(
    const Os_Port_Tms570_TaskFrameType* Frame)
{
    if ((Frame == NULL_PTR) || (Frame->Valid == FALSE)) {
        return;
    }

    os_port_tms570_state.CurrentTaskSp = Frame->Sp;
    os_port_tms570_state.LastRestoredTaskSp = Frame->Sp;
    os_port_tms570_state.CurrentTimeSlice = Frame->TimeSlice;
    os_port_tms570_state.CurrentTaskLinkRegister = Frame->LinkRegister;
    os_port_tms570_state.CurrentIrqSavedCpsr = Frame->Cpsr;

    if (Frame->StackType == OS_PORT_TMS570_SOLICITED_STACK_TYPE) {
        os_port_tms570_apply_solicited_runtime_frame_to_live_restore_state(Frame);
    } else {
        os_port_tms570_apply_interrupt_runtime_frame_to_live_restore_state(Frame);
    }

    os_port_tms570_set_task_frame_view(&os_port_tms570_state.LastRestoredTaskFrameView,
                                       Frame,
                                       os_port_tms570_get_task_frame_bytes(Frame));
    os_port_tms570_publish_last_restored_task_frame_view();
}

static void os_port_tms570_apply_runtime_frame_to_live_irq_resume_state(
    const Os_Port_Tms570_TaskFrameType* Frame)
{
    const Os_Port_Tms570_InterruptContextType* restored_context = Os_Port_Tms570_PeekRestoredIrqContext();

    if ((Frame == NULL_PTR) || (Frame->Valid == FALSE)) {
        return;
    }

    os_port_tms570_state.CurrentTaskSp = Frame->Sp;
    if ((restored_context != NULL_PTR) && (restored_context->Valid == TRUE)) {
        os_port_tms570_state.CurrentIrqSavedCpsr = restored_context->Cpsr;
        os_port_tms570_state.CurrentIrqScratch = restored_context->Scratch;
    } else {
        os_port_tms570_state.CurrentIrqSavedCpsr = Frame->Cpsr;
        os_port_tms570_state.CurrentIrqScratch = Frame->IrqScratch;
    }

    os_port_tms570_build_task_frame_view_from_interrupt_context(
        &os_port_tms570_state.LastRestoredTaskFrameView, Frame, restored_context);
    os_port_tms570_publish_last_restored_task_frame_view();
}

static void os_port_tms570_apply_runtime_frame_to_live_fiq_resume_state(
    const Os_Port_Tms570_TaskFrameType* Frame)
{
    const Os_Port_Tms570_InterruptContextType* restored_context = Os_Port_Tms570_PeekRestoredFiqContext();

    if ((Frame == NULL_PTR) || (Frame->Valid == FALSE)) {
        return;
    }

    os_port_tms570_state.CurrentTaskSp = Frame->Sp;
    if ((restored_context != NULL_PTR) && (restored_context->Valid == TRUE)) {
        os_port_tms570_state.CurrentFiqSavedCpsr = restored_context->Cpsr;
        os_port_tms570_state.CurrentFiqScratch = restored_context->Scratch;
    } else {
        os_port_tms570_state.CurrentFiqSavedCpsr = Frame->Cpsr;
        os_port_tms570_state.CurrentFiqScratch = Frame->IrqScratch;
    }

    os_port_tms570_build_task_frame_view_from_interrupt_context(
        &os_port_tms570_state.LastRestoredTaskFrameView, Frame, restored_context);
    os_port_tms570_publish_last_restored_task_frame_view();
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
        os_port_tms570_clear_task_frame(&os_port_tms570_task_context[idx].InitialFrame);
        os_port_tms570_clear_task_frame(&os_port_tms570_task_context[idx].RuntimeFrame);
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

static void os_port_tms570_reset_irq_saved_cpsr_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_irq_saved_cpsr_stack[idx] = 0u;
    }
}

static void os_port_tms570_reset_irq_scratch_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_irq_scratch_stack[idx]);
    }
}

static void os_port_tms570_reset_fiq_return_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_fiq_return_stack[idx] = (uintptr_t)0u;
    }
}

static void os_port_tms570_reset_fiq_saved_cpsr_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_fiq_saved_cpsr_stack[idx] = 0u;
    }
}

static void os_port_tms570_reset_fiq_scratch_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_fiq_scratch_stack[idx]);
    }
}

static void os_port_tms570_reset_irq_context_frame_stack(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX; idx++) {
        os_port_tms570_irq_context_frame_stack[idx] = 0u;
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

static Os_Port_Tms570_IrqScratchSnapshotType os_port_tms570_get_fiq_context_scratch_snapshot(
    uint8 SaveAction)
{
    Os_Port_Tms570_IrqScratchSnapshotType snapshot = os_port_tms570_state.CurrentFiqScratch;

    if (SaveAction != OS_PORT_TMS570_FIQ_SAVE_NESTED_FIQ) {
        snapshot.R10 = 0u;
        snapshot.R12 = 0u;
    }

    return snapshot;
}

static uint32 os_port_tms570_get_irq_context_frame_bytes(uint8 SaveAction)
{
    if ((SaveAction == OS_PORT_TMS570_SAVE_CAPTURE_CURRENT) ||
        (SaveAction == OS_PORT_TMS570_SAVE_NESTED_IRQ)) {
        return OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES;
    }

    /*
     * The local Cortex-R5 ThreadX idle-system path recovers the temporary
     * r0-r3 push before returning to IRQ processing, so no persistent IRQ
     * interrupt frame remains owned by the interrupted context.
     */
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
    os_port_tms570_set_runtime_task_sp(&os_port_tms570_task_context[current_task], Sp);
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

StatusType Os_Port_Tms570_BeginFirstTaskStart(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskPrepared == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == TRUE)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.FirstTaskStartInProgress = TRUE;
    return E_OK;
}

void Os_Port_Tms570_FinishFirstTaskStart(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStartInProgress == FALSE)) {
        os_port_tms570_state.FirstTaskStartInProgress = FALSE;
        return;
    }

    os_port_tms570_state.FirstTaskStarted = TRUE;
    os_port_tms570_state.FirstTaskStartInProgress = FALSE;
    os_port_tms570_state.DispatchRequested = FALSE;
    os_port_tms570_state.DeferredDispatch = FALSE;
    os_port_tms570_state.CurrentTask = os_port_tms570_state.FirstTaskTaskID;
    if ((os_port_tms570_is_valid_task(os_port_tms570_state.FirstTaskTaskID) == TRUE) &&
        (os_port_tms570_task_context[os_port_tms570_state.FirstTaskTaskID].Prepared == TRUE)) {
        os_port_tms570_apply_runtime_frame_to_live_restore_state(
            &os_port_tms570_task_context[os_port_tms570_state.FirstTaskTaskID].RuntimeFrame);
    } else {
        os_port_tms570_state.CurrentTaskSp = os_port_tms570_state.FirstTaskSp;
        os_port_tms570_state.LastRestoredTaskSp = os_port_tms570_state.FirstTaskSp;
        os_port_tms570_state.CurrentTimeSlice = 0u;
    }
    os_port_tms570_state.FirstTaskLaunchCount++;
}

void Os_Port_Tms570_MarkFirstTaskStarted(void)
{
    (void)Os_Port_Tms570_BeginFirstTaskStart();
    Os_Port_Tms570_FinishFirstTaskStart();
}

void Os_Port_Tms570_CompleteDispatch(void)
{
    TaskType target_task = os_port_tms570_state.CurrentTask;
    uintptr_t running_task_sp = os_port_tms570_state.CurrentTaskSp;

    if ((os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (os_port_tms570_state.DispatchRequested == FALSE)) {
        return;
    }

    os_port_tms570_state.LastSavedTask = os_port_tms570_state.CurrentTask;
    if (os_port_tms570_state.IrqCapturedTask != INVALID_TASK) {
        os_port_tms570_state.LastSavedTask = os_port_tms570_state.IrqCapturedTask;
        running_task_sp =
            os_port_tms570_task_context[os_port_tms570_state.IrqCapturedTask].RuntimeSp;
        if (running_task_sp == (uintptr_t)0u) {
            running_task_sp = os_port_tms570_state.CurrentTaskSp;
        }
    } else if ((os_port_tms570_state.CurrentTask != INVALID_TASK) &&
               (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == TRUE) &&
               (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == TRUE)) {
        os_port_tms570_state.LastSavedTaskSp =
            os_port_tms570_task_context[os_port_tms570_state.CurrentTask].RuntimeFrame.Sp;
    }

    if (os_port_tms570_state.SelectedNextTask != INVALID_TASK) {
        target_task = os_port_tms570_state.SelectedNextTask;
    }

    if (target_task != os_port_tms570_state.CurrentTask) {
        if ((os_port_tms570_state.CurrentTask != INVALID_TASK) &&
            (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == TRUE) &&
            (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == TRUE)) {
            os_port_tms570_capture_runtime_task_from_restored_irq_context(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask]);
            os_port_tms570_capture_runtime_task_lower(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask]);
            os_port_tms570_capture_runtime_task_link_register(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask]);
            os_port_tms570_capture_runtime_task_preserved(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask]);
            os_port_tms570_capture_runtime_task_vfp(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask]);
            os_port_tms570_state.LastSavedTimeSlice = 0u;
            if (os_port_tms570_state.CurrentTimeSlice > 0u) {
                os_port_tms570_state.LastSavedTimeSlice = os_port_tms570_state.CurrentTimeSlice;
                os_port_tms570_set_runtime_task_time_slice(
                    &os_port_tms570_task_context[os_port_tms570_state.CurrentTask],
                    os_port_tms570_state.CurrentTimeSlice);
            }
            os_port_tms570_commit_saved_runtime_frame(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask],
                running_task_sp);
            os_port_tms570_set_task_frame_view(
                &os_port_tms570_state.LastSavedTaskFrameView,
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask].RuntimeFrame,
                os_port_tms570_get_task_frame_bytes(
                    &os_port_tms570_task_context[os_port_tms570_state.CurrentTask].RuntimeFrame));
            os_port_tms570_publish_last_saved_task_frame_view();
        }
        os_port_tms570_state.TaskSwitchCount++;
    }

    os_port_tms570_state.CurrentTask = target_task;
    if ((target_task != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(target_task) == TRUE) &&
        (os_port_tms570_task_context[target_task].Prepared == TRUE)) {
        if (target_task == os_port_tms570_state.LastSavedTask) {
            os_port_tms570_apply_runtime_frame_to_live_irq_resume_state(
                &os_port_tms570_task_context[target_task].RuntimeFrame);
        } else {
            os_port_tms570_apply_runtime_frame_to_live_restore_state(
                &os_port_tms570_task_context[target_task].RuntimeFrame);
        }
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
    uint32 frame_bytes;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        os_port_tms570_state.LastSaveAction = OS_PORT_TMS570_SAVE_NONE;
        os_port_tms570_state.LastSaveContinuationAction = OS_PORT_TMS570_SAVE_CONTINUE_NONE;
        os_port_tms570_state.IrqSaveInProgress = FALSE;
        return OS_PORT_TMS570_SAVE_NONE;
    }

    save_action = os_port_tms570_get_save_action();
    if ((save_action == OS_PORT_TMS570_SAVE_CAPTURE_CURRENT) &&
        (Os_Port_Tms570_SaveCurrentTaskSp(Sp) != E_OK)) {
        os_port_tms570_state.IrqCapturedTask = INVALID_TASK;
        os_port_tms570_state.IrqCapturedTaskSp = (uintptr_t)0u;
        save_action = OS_PORT_TMS570_SAVE_IDLE_SYSTEM;
    }

    frame_bytes = os_port_tms570_get_irq_context_frame_bytes(save_action);
    if (os_port_tms570_state.IrqContextDepth < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX) {
        os_port_tms570_irq_return_stack[os_port_tms570_state.IrqContextDepth] =
            os_port_tms570_state.CurrentIrqReturnAddress;
        os_port_tms570_irq_saved_cpsr_stack[os_port_tms570_state.IrqContextDepth] =
            os_port_tms570_state.CurrentIrqSavedCpsr;
    }

    os_port_tms570_state.LastSaveAction = save_action;
    os_port_tms570_state.LastSaveContinuationAction =
        os_port_tms570_get_save_continuation_action(save_action);
    os_port_tms570_state.LastSavedIrqContextCpsr = os_port_tms570_state.CurrentIrqSavedCpsr;
    if (frame_bytes > 0u) {
        os_port_tms570_state.LastSavedIrqContextScratch = os_port_tms570_state.CurrentIrqScratch;
    } else {
        os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.LastSavedIrqContextScratch);
    }
    os_port_tms570_state.IrqSaveInProgress = (boolean)(save_action != OS_PORT_TMS570_SAVE_NONE);
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_IRQ;
    os_port_tms570_state.LastSavedIrqReturnAddress =
        os_port_tms570_state.CurrentIrqReturnAddress;
    os_port_tms570_state.IrqContextSaveCount++;
    os_port_tms570_state.IrqContextDepth++;
    return save_action;
}

void Os_Port_Tms570_FinishIrqContextSave(uint8 SaveAction)
{
    uint8 frame_index;
    uint32 frame_bytes;
    TaskType current_task = os_port_tms570_state.CurrentTask;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (SaveAction == OS_PORT_TMS570_SAVE_NONE)) {
        os_port_tms570_state.IrqSaveInProgress = FALSE;
        return;
    }

    frame_bytes = os_port_tms570_get_irq_context_frame_bytes(SaveAction);
    if (os_port_tms570_state.IrqContextDepth > 0u) {
        frame_index = (uint8)(os_port_tms570_state.IrqContextDepth - 1u);
        if (frame_index < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX) {
            os_port_tms570_irq_context_frame_stack[frame_index] = frame_bytes;
            if (frame_bytes > 0u) {
                os_port_tms570_irq_scratch_stack[frame_index] = os_port_tms570_state.CurrentIrqScratch;
            } else {
                os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_irq_scratch_stack[frame_index]);
            }
        }
    }
    os_port_tms570_state.LastSavedIrqContextBytes = frame_bytes;
    if (frame_bytes > 0u) {
        os_port_tms570_state.LastSavedIrqContextScratch = os_port_tms570_state.CurrentIrqScratch;
    } else {
        os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.LastSavedIrqContextScratch);
    }
    os_port_tms570_set_interrupt_context(
        &os_port_tms570_state.LastSavedIrqContext,
        os_port_tms570_state.LastSavedIrqReturnAddress,
        os_port_tms570_state.LastSavedIrqContextCpsr,
        frame_bytes,
        (frame_bytes > 0u) ? &os_port_tms570_state.LastSavedIrqContextScratch : NULL_PTR);
    if ((SaveAction == OS_PORT_TMS570_SAVE_CAPTURE_CURRENT) &&
        (current_task != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(current_task) == TRUE) &&
        (os_port_tms570_task_context[current_task].Prepared == TRUE)) {
        os_port_tms570_capture_runtime_task_interrupt_context(
            &os_port_tms570_task_context[current_task],
            &os_port_tms570_state.LastSavedIrqContext);
        os_port_tms570_capture_runtime_task_lower(
            &os_port_tms570_task_context[current_task]);
        os_port_tms570_capture_runtime_task_link_register(
            &os_port_tms570_task_context[current_task]);
        os_port_tms570_capture_runtime_task_preserved(
            &os_port_tms570_task_context[current_task]);
        os_port_tms570_capture_runtime_task_vfp(
            &os_port_tms570_task_context[current_task]);
    }
    os_port_tms570_state.IrqInterruptStackBytes += frame_bytes;
    if (os_port_tms570_state.IrqInterruptStackBytes > os_port_tms570_state.IrqInterruptStackPeakBytes) {
        os_port_tms570_state.IrqInterruptStackPeakBytes = os_port_tms570_state.IrqInterruptStackBytes;
    }

    if (os_port_tms570_state.LastSaveContinuationAction ==
        OS_PORT_TMS570_SAVE_CONTINUE_NESTED_RETURN) {
        os_port_tms570_state.NestedIrqReturnCount++;
    } else if (os_port_tms570_state.LastSaveContinuationAction ==
               OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING) {
        os_port_tms570_state.IrqProcessingEnterCount++;
    }

    Os_PortEnterIsr2();
    os_port_tms570_state.IrqSaveInProgress = FALSE;
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
    uint32 frame_bytes = 0u;
    uintptr_t restore_address = (uintptr_t)0u;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqContextDepth == 0u)) {
        os_port_tms570_state.LastRestoreAction = OS_PORT_TMS570_RESTORE_NONE;
        os_port_tms570_state.IrqRestoreInProgress = FALSE;
        return OS_PORT_TMS570_RESTORE_NONE;
    }

    restore_action = os_port_tms570_get_restore_action();
    restore_index = (uint8)(os_port_tms570_state.IrqContextDepth - 1u);
    if (restore_index < OS_PORT_TMS570_IRQ_RETURN_STACK_MAX) {
        restore_address = os_port_tms570_irq_return_stack[restore_index];
        os_port_tms570_irq_return_stack[restore_index] = (uintptr_t)0u;
        frame_bytes = os_port_tms570_irq_context_frame_stack[restore_index];
        os_port_tms570_irq_context_frame_stack[restore_index] = 0u;
        os_port_tms570_state.CurrentIrqSavedCpsr = os_port_tms570_irq_saved_cpsr_stack[restore_index];
        os_port_tms570_irq_saved_cpsr_stack[restore_index] = 0u;
        if (frame_bytes > 0u) {
            os_port_tms570_state.CurrentIrqScratch = os_port_tms570_irq_scratch_stack[restore_index];
            os_port_tms570_state.LastRestoredIrqContextScratch =
                os_port_tms570_irq_scratch_stack[restore_index];
        } else {
            os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.LastRestoredIrqContextScratch);
        }
        os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_irq_scratch_stack[restore_index]);
    }
    os_port_tms570_state.LastRestoreAction = restore_action;
    os_port_tms570_state.IrqRestoreInProgress = (boolean)(restore_action != OS_PORT_TMS570_RESTORE_NONE);
    os_port_tms570_state.LastRestoredIrqContextBytes = frame_bytes;
    os_port_tms570_state.LastRestoredIrqContextCpsr = os_port_tms570_state.CurrentIrqSavedCpsr;
    os_port_tms570_set_interrupt_context(
        &os_port_tms570_state.LastRestoredIrqContext,
        restore_address,
        os_port_tms570_state.LastRestoredIrqContextCpsr,
        frame_bytes,
        (frame_bytes > 0u) ? &os_port_tms570_state.LastRestoredIrqContextScratch : NULL_PTR);
    if (os_port_tms570_state.IrqInterruptStackBytes >= frame_bytes) {
        os_port_tms570_state.IrqInterruptStackBytes -= frame_bytes;
    } else {
        os_port_tms570_state.IrqInterruptStackBytes = 0u;
    }
    os_port_tms570_state.CurrentIrqReturnAddress = restore_address;
    os_port_tms570_state.LastRestoredIrqReturnAddress = restore_address;
    os_port_tms570_state.IrqContextRestoreCount++;
    os_port_tms570_state.IrqContextDepth--;
    return restore_action;
}

static void os_port_tms570_begin_irq_scheduler_return(void)
{
    TaskType saved_task = os_port_tms570_state.CurrentTask;
    uintptr_t running_task_sp = os_port_tms570_state.CurrentTaskSp;

    if ((os_port_tms570_state.FirstTaskStarted == TRUE) &&
        (os_port_tms570_state.DispatchRequested == TRUE)) {
        os_port_tms570_state.LastSavedTask = os_port_tms570_state.CurrentTask;
        if (os_port_tms570_state.IrqCapturedTask != INVALID_TASK) {
            os_port_tms570_state.LastSavedTask = os_port_tms570_state.IrqCapturedTask;
            saved_task = os_port_tms570_state.IrqCapturedTask;
            running_task_sp =
                os_port_tms570_task_context[os_port_tms570_state.IrqCapturedTask].RuntimeSp;
            if (running_task_sp == (uintptr_t)0u) {
                running_task_sp = os_port_tms570_state.CurrentTaskSp;
            }
        } else if ((os_port_tms570_state.CurrentTask != INVALID_TASK) &&
                   (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == TRUE) &&
                   (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == TRUE)) {
            os_port_tms570_state.LastSavedTaskSp =
                os_port_tms570_task_context[os_port_tms570_state.CurrentTask].RuntimeFrame.Sp;
        }

        if ((saved_task != INVALID_TASK) &&
            (os_port_tms570_is_valid_task(saved_task) == TRUE) &&
            (os_port_tms570_task_context[saved_task].Prepared == TRUE)) {
            os_port_tms570_capture_runtime_task_from_restored_irq_context(
                &os_port_tms570_task_context[saved_task]);
            os_port_tms570_state.LastSavedTimeSlice = 0u;
            if (os_port_tms570_state.CurrentTimeSlice > 0u) {
                os_port_tms570_state.LastSavedTimeSlice = os_port_tms570_state.CurrentTimeSlice;
                os_port_tms570_set_runtime_task_time_slice(
                    &os_port_tms570_task_context[saved_task],
                    os_port_tms570_state.CurrentTimeSlice);
            }
            os_port_tms570_commit_saved_runtime_frame(
                &os_port_tms570_task_context[saved_task],
                running_task_sp);
            os_port_tms570_set_task_frame_view(
                &os_port_tms570_state.LastSavedTaskFrameView,
                &os_port_tms570_task_context[saved_task].RuntimeFrame,
                os_port_tms570_get_task_frame_bytes(
                    &os_port_tms570_task_context[saved_task].RuntimeFrame));
            os_port_tms570_publish_last_saved_task_frame_view();
        }
    }

    os_port_tms570_state.CurrentTask = INVALID_TASK;
    os_port_tms570_state.CurrentTaskSp = (uintptr_t)0u;
    os_port_tms570_state.CurrentTaskLinkRegister = (uintptr_t)0u;
    os_port_tms570_state.CurrentTimeSlice = 0u;
    os_port_tms570_state.IrqSchedulerReturnInProgress = TRUE;
}

static void os_port_tms570_begin_fiq_scheduler_return(uint8 RestoreAction)
{
    TaskType saved_task = os_port_tms570_state.CurrentTask;
    uintptr_t running_task_sp = os_port_tms570_state.CurrentTaskSp;

    if ((RestoreAction == OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER) &&
        (os_port_tms570_state.FirstTaskStarted == TRUE) &&
        (saved_task != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(saved_task) == TRUE) &&
        (os_port_tms570_task_context[saved_task].Prepared == TRUE)) {
        os_port_tms570_state.LastSavedTask = saved_task;
        os_port_tms570_capture_runtime_task_from_restored_fiq_context(
            &os_port_tms570_task_context[saved_task]);
        os_port_tms570_state.LastSavedTimeSlice = 0u;
        if (os_port_tms570_state.CurrentTimeSlice > 0u) {
            os_port_tms570_state.LastSavedTimeSlice = os_port_tms570_state.CurrentTimeSlice;
            os_port_tms570_set_runtime_task_time_slice(
                &os_port_tms570_task_context[saved_task],
                os_port_tms570_state.CurrentTimeSlice);
        }
        os_port_tms570_commit_saved_runtime_frame(
            &os_port_tms570_task_context[saved_task],
            running_task_sp);
        os_port_tms570_set_task_frame_view(
            &os_port_tms570_state.LastSavedTaskFrameView,
            &os_port_tms570_task_context[saved_task].RuntimeFrame,
            os_port_tms570_get_task_frame_bytes(
                &os_port_tms570_task_context[saved_task].RuntimeFrame));
        os_port_tms570_publish_last_saved_task_frame_view();
    }

    os_port_tms570_state.CurrentTask = INVALID_TASK;
    os_port_tms570_state.CurrentTaskSp = (uintptr_t)0u;
    os_port_tms570_state.CurrentTaskLinkRegister = (uintptr_t)0u;
    os_port_tms570_state.CurrentTimeSlice = 0u;
    os_port_tms570_state.FiqSchedulerReturnInProgress = TRUE;
}

void Os_Port_Tms570_FinishIrqContextRestore(uint8 RestoreAction)
{
    boolean switch_task_pending = (boolean)(RestoreAction == OS_PORT_TMS570_RESTORE_SWITCH_TASK);

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (RestoreAction == OS_PORT_TMS570_RESTORE_NONE)) {
        os_port_tms570_state.IrqRestoreInProgress = FALSE;
        return;
    }

    Os_PortExitIsr2();

    if ((switch_task_pending == FALSE) &&
        (os_port_tms570_state.DispatchRequested == TRUE) &&
        (os_port_tms570_state.SelectedNextTask != INVALID_TASK) &&
        (os_port_tms570_is_valid_task(os_port_tms570_state.SelectedNextTask) == TRUE) &&
        (os_port_tms570_task_context[os_port_tms570_state.SelectedNextTask].Prepared == TRUE) &&
        (os_port_tms570_state.SelectedNextTask != os_port_tms570_state.CurrentTask)) {
        switch_task_pending = TRUE;
    }

    if (switch_task_pending == TRUE) {
        os_port_tms570_state.LastRestoreAction = OS_PORT_TMS570_RESTORE_SWITCH_TASK;
        os_port_tms570_state.IrqSchedulerReturnCount++;
        os_port_tms570_begin_irq_scheduler_return();
    } else if ((RestoreAction == OS_PORT_TMS570_RESTORE_RESUME_CURRENT) &&
               (os_port_tms570_state.CurrentTask != INVALID_TASK) &&
               (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == TRUE) &&
               (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == TRUE)) {
        os_port_tms570_apply_runtime_frame_to_live_irq_resume_state(
            &os_port_tms570_task_context[os_port_tms570_state.CurrentTask].RuntimeFrame);
        if ((os_port_tms570_state.DispatchRequested == TRUE) &&
            ((os_port_tms570_state.SelectedNextTask == INVALID_TASK) ||
             (os_port_tms570_state.SelectedNextTask == os_port_tms570_state.CurrentTask))) {
            os_port_tms570_state.DispatchRequested = FALSE;
            os_port_tms570_state.DeferredDispatch = FALSE;
            os_port_tms570_state.SelectedNextTask = INVALID_TASK;
        }
    } else if (RestoreAction == OS_PORT_TMS570_RESTORE_IDLE_SYSTEM) {
        os_port_tms570_state.IrqIdleSystemReturnCount++;
        os_port_tms570_begin_irq_scheduler_return();
    }

    if ((RestoreAction == OS_PORT_TMS570_RESTORE_NESTED_RETURN) ||
        (switch_task_pending == TRUE) ||
        (RestoreAction == OS_PORT_TMS570_RESTORE_IDLE_SYSTEM)) {
        os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
    } else {
        os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_THREAD;
    }
    os_port_tms570_state.CurrentIrqReturnAddress = (uintptr_t)0u;

    if (os_port_tms570_state.IrqContextDepth == 0u) {
        os_port_tms570_state.IrqCapturedTask = INVALID_TASK;
        os_port_tms570_state.IrqCapturedTaskSp = (uintptr_t)0u;
    }
    os_port_tms570_state.IrqRestoreInProgress = FALSE;
}

void Os_Port_Tms570_FinishIrqSchedulerReturn(void)
{
    TaskType target_task = INVALID_TASK;
    TaskType saved_task = os_port_tms570_state.LastSavedTask;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqSchedulerReturnInProgress == FALSE)) {
        os_port_tms570_state.IrqSchedulerReturnInProgress = FALSE;
        return;
    }

    if (os_port_tms570_state.DispatchRequested == TRUE) {
        if (os_port_tms570_state.SelectedNextTask != INVALID_TASK) {
            target_task = os_port_tms570_state.SelectedNextTask;
        }

        os_port_tms570_state.CurrentTask = target_task;
        if ((target_task != INVALID_TASK) &&
            (os_port_tms570_is_valid_task(target_task) == TRUE) &&
            (os_port_tms570_task_context[target_task].Prepared == TRUE)) {
            os_port_tms570_apply_runtime_frame_to_live_restore_state(
                &os_port_tms570_task_context[target_task].RuntimeFrame);
            os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_THREAD;
        } else {
            os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
            os_port_tms570_state.CurrentTimeSlice = 0u;
        }

        if (target_task != saved_task) {
            os_port_tms570_state.TaskSwitchCount++;
        }
        os_port_tms570_state.SelectedNextTask = INVALID_TASK;
        os_port_tms570_state.DispatchRequested = FALSE;
        os_port_tms570_state.DeferredDispatch = FALSE;
    } else {
        os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
    }

    os_port_tms570_state.IrqSchedulerReturnInProgress = FALSE;
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

const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekSavedIrqContext(void)
{
    const Os_Port_Tms570_InterruptContextType* pending_context =
        Os_Port_Tms570_PeekPendingIrqSaveContext();

    if (pending_context != NULL_PTR) {
        return pending_context;
    }

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.LastSavedIrqContext.Valid == FALSE)) {
        return NULL_PTR;
    }

    return &os_port_tms570_state.LastSavedIrqContext;
}

const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekSavedFiqContext(void)
{
    const Os_Port_Tms570_InterruptContextType* pending_context =
        Os_Port_Tms570_PeekPendingFiqSaveContext();

    if (pending_context != NULL_PTR) {
        return pending_context;
    }

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.LastSavedFiqContext.Valid == FALSE)) {
        return NULL_PTR;
    }

    return &os_port_tms570_state.LastSavedFiqContext;
}

const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekPendingIrqSaveContext(void)
{
    uint8 save_action;
    uint32 frame_bytes;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqSaveInProgress == FALSE)) {
        return NULL_PTR;
    }

    save_action = os_port_tms570_state.LastSaveAction;
    if (save_action == OS_PORT_TMS570_SAVE_NONE) {
        return NULL_PTR;
    }

    frame_bytes = os_port_tms570_get_irq_context_frame_bytes(save_action);
    os_port_tms570_set_interrupt_context(
        &os_port_tms570_pending_irq_save_context,
        os_port_tms570_state.LastSavedIrqReturnAddress,
        os_port_tms570_state.LastSavedIrqContextCpsr,
        frame_bytes,
        (frame_bytes > 0u) ? &os_port_tms570_state.LastSavedIrqContextScratch : NULL_PTR);
    return &os_port_tms570_pending_irq_save_context;
}

const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekPendingFiqSaveContext(void)
{
    uint8 save_action;
    uint32 frame_bytes;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FiqSaveInProgress == FALSE)) {
        return NULL_PTR;
    }

    save_action = os_port_tms570_state.LastFiqSaveAction;
    if (save_action == OS_PORT_TMS570_FIQ_SAVE_NONE) {
        return NULL_PTR;
    }

    frame_bytes = os_port_tms570_get_fiq_context_frame_bytes(save_action);
    os_port_tms570_set_interrupt_context(
        &os_port_tms570_pending_fiq_save_context,
        os_port_tms570_state.LastSavedFiqReturnAddress,
        os_port_tms570_state.LastSavedFiqContextCpsr,
        frame_bytes,
        (frame_bytes > 0u) ? &os_port_tms570_state.LastSavedFiqContextScratch : NULL_PTR);
    return &os_port_tms570_pending_fiq_save_context;
}

const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekPendingSaveInterruptContext(void)
{
    const Os_Port_Tms570_InterruptContextType* pending_context =
        Os_Port_Tms570_PeekPendingIrqSaveContext();

    if (pending_context != NULL_PTR) {
        return pending_context;
    }

    return Os_Port_Tms570_PeekPendingFiqSaveContext();
}

const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekRestoredIrqContext(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.LastRestoredIrqContext.Valid == FALSE)) {
        return NULL_PTR;
    }

    return &os_port_tms570_state.LastRestoredIrqContext;
}

const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekRestoredFiqContext(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.LastRestoredFiqContext.Valid == FALSE)) {
        return NULL_PTR;
    }

    return &os_port_tms570_state.LastRestoredFiqContext;
}

const Os_Port_Tms570_InterruptContextType* Os_Port_Tms570_PeekPendingRestoreInterruptContext(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        ((os_port_tms570_state.IrqRestoreInProgress == FALSE) &&
         (os_port_tms570_state.FiqRestoreInProgress == FALSE))) {
        return NULL_PTR;
    }

    if (os_port_tms570_state.IrqRestoreInProgress == TRUE) {
        return &os_port_tms570_state.LastRestoredIrqContext;
    }

    return &os_port_tms570_state.LastRestoredFiqContext;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekSavedTaskFrameView(void)
{
    const Os_Port_Tms570_TaskFrameViewType* pending_view =
        Os_Port_Tms570_PeekPendingSaveTaskFrameView();
    if (pending_view != NULL_PTR) {
        return pending_view;
    }

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.LastSavedTaskFrameView.Frame.Valid == FALSE)) {
        return NULL_PTR;
    }

    return &os_port_tms570_state.LastSavedTaskFrameView;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingSaveTaskFrameView(void)
{
    const Os_Port_Tms570_TaskFrameViewType* pending_view =
        Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView();

    if (pending_view != NULL_PTR) {
        return pending_view;
    }

    pending_view = Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView();
    if (pending_view != NULL_PTR) {
        return pending_view;
    }

    return Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView();
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingFirstTaskFrameView(void)
{
    TaskType task_id;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStartInProgress == FALSE)) {
        return NULL_PTR;
    }

    task_id = os_port_tms570_state.FirstTaskTaskID;
    if ((task_id == INVALID_TASK) || (os_port_tms570_is_valid_task(task_id) == FALSE) ||
        (os_port_tms570_task_context[task_id].Prepared == FALSE)) {
        return NULL_PTR;
    }

    os_port_tms570_set_task_frame_view(
        &os_port_tms570_pending_first_task_frame_view,
        &os_port_tms570_task_context[task_id].RuntimeFrame,
        os_port_tms570_get_task_frame_bytes(&os_port_tms570_task_context[task_id].RuntimeFrame));
    return &os_port_tms570_pending_first_task_frame_view;
}

const Os_Port_Tms570_TaskLowerSnapshotType* Os_Port_Tms570_PeekSavedTaskLower(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? &view->Frame.TaskLower : NULL_PTR;
}

const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekSavedTaskIrqScratch(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? &view->Frame.IrqScratch : NULL_PTR;
}

const Os_Port_Tms570_PreservedSnapshotType* Os_Port_Tms570_PeekSavedTaskPreserved(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? &view->Frame.Preserved : NULL_PTR;
}

const Os_Port_Tms570_VfpStateType* Os_Port_Tms570_PeekSavedTaskVfp(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? &view->Frame.Vfp : NULL_PTR;
}

uintptr_t Os_Port_Tms570_PeekSavedTaskSp(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Sp : (uintptr_t)0u;
}

uintptr_t Os_Port_Tms570_PeekSavedTaskReturnAddress(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.ReturnAddress : (uintptr_t)0u;
}

uintptr_t Os_Port_Tms570_PeekSavedTaskLinkRegister(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.LinkRegister : (uintptr_t)0u;
}

uint32 Os_Port_Tms570_PeekSavedTaskCpsr(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Cpsr : 0u;
}

uint32 Os_Port_Tms570_PeekSavedTaskStackType(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.StackType : 0u;
}

uint32 Os_Port_Tms570_PeekSavedTaskFrameBytes(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekSavedTaskFrameView();

    return (view != NULL_PTR) ? view->FrameBytes : 0u;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.SolicitedSaveInProgress == FALSE)) {
        return NULL_PTR;
    }

    if (os_port_tms570_pending_solicited_save_task_frame_view.Frame.Valid == FALSE) {
        return NULL_PTR;
    }

    return &os_port_tms570_pending_solicited_save_task_frame_view;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView(void)
{
    TaskType task_id;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqSaveInProgress == FALSE) ||
        (os_port_tms570_state.LastSaveAction != OS_PORT_TMS570_SAVE_CAPTURE_CURRENT)) {
        return NULL_PTR;
    }

    task_id = os_port_tms570_state.CurrentTask;
    if ((task_id == INVALID_TASK) || (os_port_tms570_is_valid_task(task_id) == FALSE) ||
        (os_port_tms570_task_context[task_id].Prepared == FALSE)) {
        return NULL_PTR;
    }

    os_port_tms570_build_task_frame_view_from_interrupt_context(
        &os_port_tms570_pending_irq_save_task_frame_view,
        &os_port_tms570_task_context[task_id].RuntimeFrame,
        Os_Port_Tms570_PeekPendingIrqSaveContext());
    if (os_port_tms570_state.CurrentTaskSp != (uintptr_t)0u) {
        os_port_tms570_pending_irq_save_task_frame_view.Frame.Sp =
            os_port_tms570_get_saved_sp_for_frame_bytes(
                os_port_tms570_state.CurrentTaskSp,
                os_port_tms570_pending_irq_save_task_frame_view.FrameBytes);
    }
    return &os_port_tms570_pending_irq_save_task_frame_view;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView(void)
{
    TaskType task_id;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FiqSaveInProgress == FALSE) ||
        (os_port_tms570_state.LastFiqSaveAction != OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY)) {
        return NULL_PTR;
    }

    task_id = os_port_tms570_state.CurrentTask;
    if ((task_id == INVALID_TASK) || (os_port_tms570_is_valid_task(task_id) == FALSE) ||
        (os_port_tms570_task_context[task_id].Prepared == FALSE)) {
        return NULL_PTR;
    }

    os_port_tms570_build_task_frame_view_from_interrupt_context(
        &os_port_tms570_pending_fiq_save_task_frame_view,
        &os_port_tms570_task_context[task_id].RuntimeFrame,
        Os_Port_Tms570_PeekPendingFiqSaveContext());
    if (os_port_tms570_state.CurrentTaskSp != (uintptr_t)0u) {
        os_port_tms570_pending_fiq_save_task_frame_view.Frame.Sp =
            os_port_tms570_get_saved_sp_for_frame_bytes(
                os_port_tms570_state.CurrentTaskSp,
                os_port_tms570_pending_fiq_save_task_frame_view.FrameBytes);
    }
    return &os_port_tms570_pending_fiq_save_task_frame_view;
}

const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekPendingIrqSaveTaskIrqScratch(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView();

    return (view != NULL_PTR) ? &view->Frame.IrqScratch : NULL_PTR;
}

const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekPendingFiqSaveTaskIrqScratch(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView();

    return (view != NULL_PTR) ? &view->Frame.IrqScratch : NULL_PTR;
}

uintptr_t Os_Port_Tms570_PeekPendingIrqSaveTaskSp(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Sp : (uintptr_t)0u;
}

uintptr_t Os_Port_Tms570_PeekPendingFiqSaveTaskSp(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Sp : (uintptr_t)0u;
}

uintptr_t Os_Port_Tms570_PeekPendingIrqSaveTaskReturnAddress(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.ReturnAddress : (uintptr_t)0u;
}

uintptr_t Os_Port_Tms570_PeekPendingFiqSaveTaskReturnAddress(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.ReturnAddress : (uintptr_t)0u;
}

uint32 Os_Port_Tms570_PeekPendingIrqSaveTaskCpsr(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Cpsr : 0u;
}

uint32 Os_Port_Tms570_PeekPendingFiqSaveTaskCpsr(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Cpsr : 0u;
}

uint32 Os_Port_Tms570_PeekPendingIrqSaveTaskFrameBytes(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqSaveTaskFrameView();

    return (view != NULL_PTR) ? view->FrameBytes : 0u;
}

uint32 Os_Port_Tms570_PeekPendingFiqSaveTaskFrameBytes(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqSaveTaskFrameView();

    return (view != NULL_PTR) ? view->FrameBytes : 0u;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekRestoredTaskFrameView(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.LastRestoredTaskFrameView.Frame.Valid == FALSE)) {
        return NULL_PTR;
    }

    return &os_port_tms570_state.LastRestoredTaskFrameView;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView(void)
{
    uint8 restore_action;
    TaskType task_id;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqRestoreInProgress == FALSE)) {
        return NULL_PTR;
    }

    restore_action = os_port_tms570_state.LastRestoreAction;
    if ((restore_action != OS_PORT_TMS570_RESTORE_RESUME_CURRENT) &&
        (restore_action != OS_PORT_TMS570_RESTORE_SWITCH_TASK)) {
        return NULL_PTR;
    }

    task_id = os_port_tms570_state.CurrentTask;
    if (restore_action == OS_PORT_TMS570_RESTORE_SWITCH_TASK) {
        task_id = os_port_tms570_state.SelectedNextTask;
    }

    if ((task_id == INVALID_TASK) || (os_port_tms570_is_valid_task(task_id) == FALSE) ||
        (os_port_tms570_task_context[task_id].Prepared == FALSE)) {
        return NULL_PTR;
    }

    if (restore_action == OS_PORT_TMS570_RESTORE_RESUME_CURRENT) {
        os_port_tms570_build_task_frame_view_from_interrupt_context(
            &os_port_tms570_pending_irq_restore_task_frame_view,
            &os_port_tms570_task_context[task_id].RuntimeFrame,
            Os_Port_Tms570_PeekRestoredIrqContext());
    } else {
        os_port_tms570_set_task_frame_view(
            &os_port_tms570_pending_irq_restore_task_frame_view,
            &os_port_tms570_task_context[task_id].RuntimeFrame,
            os_port_tms570_get_task_frame_bytes(&os_port_tms570_task_context[task_id].RuntimeFrame));
    }

    return &os_port_tms570_pending_irq_restore_task_frame_view;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingIrqSchedulerReturnTaskFrameView(void)
{
    TaskType task_id;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.IrqSchedulerReturnInProgress == FALSE) ||
        (os_port_tms570_state.DispatchRequested == FALSE)) {
        return NULL_PTR;
    }

    task_id = os_port_tms570_state.SelectedNextTask;
    if ((task_id == INVALID_TASK) || (os_port_tms570_is_valid_task(task_id) == FALSE) ||
        (os_port_tms570_task_context[task_id].Prepared == FALSE)) {
        return NULL_PTR;
    }

    os_port_tms570_set_task_frame_view(
        &os_port_tms570_pending_irq_scheduler_return_task_frame_view,
        &os_port_tms570_task_context[task_id].RuntimeFrame,
        os_port_tms570_get_task_frame_bytes(&os_port_tms570_task_context[task_id].RuntimeFrame));
    return &os_port_tms570_pending_irq_scheduler_return_task_frame_view;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingFiqSchedulerReturnTaskFrameView(void)
{
    TaskType task_id;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FiqSchedulerReturnInProgress == FALSE) ||
        (os_port_tms570_state.DispatchRequested == FALSE)) {
        return NULL_PTR;
    }

    task_id = os_port_tms570_state.SelectedNextTask;
    if ((task_id == INVALID_TASK) || (os_port_tms570_is_valid_task(task_id) == FALSE) ||
        (os_port_tms570_task_context[task_id].Prepared == FALSE)) {
        return NULL_PTR;
    }

    os_port_tms570_set_task_frame_view(
        &os_port_tms570_pending_fiq_scheduler_return_task_frame_view,
        &os_port_tms570_task_context[task_id].RuntimeFrame,
        os_port_tms570_get_task_frame_bytes(&os_port_tms570_task_context[task_id].RuntimeFrame));
    return &os_port_tms570_pending_fiq_scheduler_return_task_frame_view;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView(void)
{
    uint8 restore_action;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FiqRestoreInProgress == FALSE)) {
        return NULL_PTR;
    }

    restore_action = os_port_tms570_state.LastFiqRestoreAction;
    if ((restore_action != OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE) ||
        (os_port_tms570_state.FiqResumeMode == OS_PORT_TMS570_MODE_IRQ) ||
        (os_port_tms570_state.CurrentTask == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == FALSE) ||
        (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == FALSE)) {
        return NULL_PTR;
    }

    os_port_tms570_build_task_frame_view_from_interrupt_context(
        &os_port_tms570_pending_fiq_restore_task_frame_view,
        &os_port_tms570_task_context[os_port_tms570_state.CurrentTask].RuntimeFrame,
        Os_Port_Tms570_PeekRestoredFiqContext());
    return &os_port_tms570_pending_fiq_restore_task_frame_view;
}

const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekPendingIrqRestoreTaskIrqScratch(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView();

    return (view != NULL_PTR) ? &view->Frame.IrqScratch : NULL_PTR;
}

const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekPendingFiqRestoreTaskIrqScratch(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView();

    return (view != NULL_PTR) ? &view->Frame.IrqScratch : NULL_PTR;
}

uintptr_t Os_Port_Tms570_PeekPendingIrqRestoreTaskSp(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Sp : (uintptr_t)0u;
}

uintptr_t Os_Port_Tms570_PeekPendingFiqRestoreTaskSp(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Sp : (uintptr_t)0u;
}

uintptr_t Os_Port_Tms570_PeekPendingIrqRestoreTaskReturnAddress(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.ReturnAddress : (uintptr_t)0u;
}

uintptr_t Os_Port_Tms570_PeekPendingFiqRestoreTaskReturnAddress(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.ReturnAddress : (uintptr_t)0u;
}

uint32 Os_Port_Tms570_PeekPendingIrqRestoreTaskCpsr(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Cpsr : 0u;
}

uint32 Os_Port_Tms570_PeekPendingFiqRestoreTaskCpsr(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView();

    return (view != NULL_PTR) ? view->Frame.Cpsr : 0u;
}

uint32 Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameBytes(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView();

    return (view != NULL_PTR) ? view->FrameBytes : 0u;
}

uint32 Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameBytes(void)
{
    const Os_Port_Tms570_TaskFrameViewType* view = Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView();

    return (view != NULL_PTR) ? view->FrameBytes : 0u;
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
    os_port_tms570_reset_irq_saved_cpsr_stack();
    os_port_tms570_reset_irq_scratch_stack();
    os_port_tms570_reset_fiq_return_stack();
    os_port_tms570_reset_fiq_saved_cpsr_stack();
    os_port_tms570_reset_fiq_scratch_stack();
    os_port_tms570_reset_irq_context_frame_stack();
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
    os_port_tms570_state.FirstTaskStartInProgress = FALSE;
    os_port_tms570_state.FirstTaskStarted = FALSE;
    os_port_tms570_state.TimeSliceServicePending = FALSE;
    os_port_tms570_state.SolicitedSaveInProgress = FALSE;
    os_port_tms570_state.IrqSaveInProgress = FALSE;
    os_port_tms570_state.FiqSaveInProgress = FALSE;
    os_port_tms570_state.IrqRestoreInProgress = FALSE;
    os_port_tms570_state.IrqSchedulerReturnInProgress = FALSE;
    os_port_tms570_state.FiqRestoreInProgress = FALSE;
    os_port_tms570_state.FiqSchedulerReturnInProgress = FALSE;
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
    os_port_tms570_state.IrqIdleSystemReturnCount = 0u;
    os_port_tms570_state.IrqSchedulerReturnCount = 0u;
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
    os_port_tms570_state.IrqInterruptStackBytes = 0u;
    os_port_tms570_state.IrqInterruptStackPeakBytes = 0u;
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
    os_port_tms570_state.LastSavedIrqContextBytes = 0u;
    os_port_tms570_state.LastRestoredIrqContextBytes = 0u;
    os_port_tms570_state.LastSavedFiqContextCpsr = 0u;
    os_port_tms570_state.LastRestoredFiqContextCpsr = 0u;
    os_port_tms570_state.LastSavedFiqContextBytes = 0u;
    os_port_tms570_state.LastRestoredFiqContextBytes = 0u;
    os_port_tms570_state.LastSavedTaskFrameBytes = 0u;
    os_port_tms570_state.LastRestoredTaskFrameBytes = 0u;
    os_port_tms570_state.LastSavedIrqContextCpsr = 0u;
    os_port_tms570_state.LastRestoredIrqContextCpsr = 0u;
    os_port_tms570_state.CurrentIrqSavedCpsr = OS_PORT_TMS570_INITIAL_CPSR;
    os_port_tms570_state.CurrentFiqSavedCpsr = OS_PORT_TMS570_INITIAL_CPSR;
    os_port_tms570_state.LastRestoredTaskCpsr = 0u;
    os_port_tms570_state.LastRestoredTaskStackType = 0u;
    os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.CurrentIrqScratch);
    os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.LastSavedIrqContextScratch);
    os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.LastRestoredIrqContextScratch);
    os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.CurrentFiqScratch);
    os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.LastSavedFiqContextScratch);
    os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.LastRestoredFiqContextScratch);
    os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_state.LastRestoredTaskIrqScratch);
    os_port_tms570_clear_task_lower_snapshot(&os_port_tms570_state.CurrentTaskLower);
    os_port_tms570_clear_task_lower_snapshot(&os_port_tms570_state.LastRestoredTaskLower);
    os_port_tms570_clear_preserved_snapshot(&os_port_tms570_state.CurrentTaskPreserved);
    os_port_tms570_clear_preserved_snapshot(&os_port_tms570_state.LastRestoredTaskPreserved);
    os_port_tms570_clear_vfp_state(&os_port_tms570_state.CurrentTaskVfp);
    os_port_tms570_clear_vfp_state(&os_port_tms570_state.LastRestoredTaskVfp);
    os_port_tms570_clear_interrupt_context(&os_port_tms570_pending_irq_save_context);
    os_port_tms570_clear_interrupt_context(&os_port_tms570_pending_fiq_save_context);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_first_task_frame_view);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_solicited_save_task_frame_view);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_irq_save_task_frame_view);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_fiq_save_task_frame_view);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_irq_restore_task_frame_view);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_fiq_restore_task_frame_view);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_irq_scheduler_return_task_frame_view);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_fiq_scheduler_return_task_frame_view);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_state.LastSavedTaskFrameView);
    os_port_tms570_clear_task_frame_view(&os_port_tms570_state.LastRestoredTaskFrameView);
    os_port_tms570_clear_interrupt_context(&os_port_tms570_state.LastSavedIrqContext);
    os_port_tms570_clear_interrupt_context(&os_port_tms570_state.LastRestoredIrqContext);
    os_port_tms570_clear_interrupt_context(&os_port_tms570_state.LastSavedFiqContext);
    os_port_tms570_clear_interrupt_context(&os_port_tms570_state.LastRestoredFiqContext);
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
    os_port_tms570_state.CurrentTaskLinkRegister = (uintptr_t)0u;
    os_port_tms570_state.LastSavedTaskSp = (uintptr_t)0u;
    os_port_tms570_state.LastRestoredTaskSp = (uintptr_t)0u;
    os_port_tms570_state.LastRestoredTaskReturnAddress = (uintptr_t)0u;
    os_port_tms570_state.LastRestoredTaskLinkRegister = (uintptr_t)0u;
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
    os_port_tms570_task_context[TaskID].Entry = Entry;
    os_port_tms570_set_initial_task_frame(&os_port_tms570_task_context[TaskID], prepared_sp, Entry);
    os_port_tms570_set_runtime_task_sp(&os_port_tms570_task_context[TaskID], prepared_sp);
    os_port_tms570_set_runtime_task_time_slice(&os_port_tms570_task_context[TaskID], 0u);
    os_port_tms570_clear_task_lower_snapshot(
        &os_port_tms570_task_context[TaskID].RuntimeFrame.TaskLower);
    os_port_tms570_set_runtime_task_link_register(&os_port_tms570_task_context[TaskID], (uintptr_t)0u);
    os_port_tms570_clear_vfp_state(&os_port_tms570_task_context[TaskID].RuntimeFrame.Vfp);
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
    os_port_tms570_state.FirstTaskSp = os_port_tms570_task_context[TaskID].RuntimeFrame.Sp;
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

StatusType Os_Port_Tms570_BeginSolicitedSystemReturn(void)
{
    TaskType current_task;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (os_port_tms570_state.DispatchRequested == TRUE) ||
        (os_port_tms570_state.IrqContextDepth != 0u) ||
        (os_port_tms570_state.FiqContextDepth != 0u) ||
        (os_port_tms570_state.CurrentExecutionMode != OS_PORT_TMS570_MODE_THREAD)) {
        return E_OS_STATE;
    }

    current_task = os_port_tms570_state.CurrentTask;
    if ((current_task == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(current_task) == FALSE) ||
        (os_port_tms570_task_context[current_task].Prepared == FALSE)) {
        return E_OS_STATE;
    }

    os_port_tms570_capture_solicited_runtime_frame(&os_port_tms570_task_context[current_task]);
    os_port_tms570_set_task_frame_view(
        &os_port_tms570_pending_solicited_save_task_frame_view,
        &os_port_tms570_task_context[current_task].RuntimeFrame,
        os_port_tms570_get_task_frame_bytes(&os_port_tms570_task_context[current_task].RuntimeFrame));
    if (os_port_tms570_pending_solicited_save_task_frame_view.Frame.Valid == TRUE) {
        os_port_tms570_pending_solicited_save_task_frame_view.Frame.Sp =
            os_port_tms570_get_saved_sp_for_frame_bytes(
                os_port_tms570_state.CurrentTaskSp,
                os_port_tms570_pending_solicited_save_task_frame_view.FrameBytes);
    }
    os_port_tms570_state.SolicitedSaveInProgress = TRUE;
    return E_OK;
}

void Os_Port_Tms570_FinishSolicitedSystemReturn(void)
{
    TaskType current_task;
    const Os_Port_Tms570_TaskFrameViewType* pending_view;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.SolicitedSaveInProgress == FALSE)) {
        os_port_tms570_state.SolicitedSaveInProgress = FALSE;
        return;
    }

    current_task = os_port_tms570_state.CurrentTask;
    if ((current_task == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(current_task) == FALSE) ||
        (os_port_tms570_task_context[current_task].Prepared == FALSE)) {
        os_port_tms570_state.SolicitedSaveInProgress = FALSE;
        os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_solicited_save_task_frame_view);
        return;
    }

    pending_view = Os_Port_Tms570_PeekPendingSolicitedSaveTaskFrameView();
    os_port_tms570_commit_task_frame_view(
        &os_port_tms570_task_context[current_task],
        pending_view);
    os_port_tms570_set_task_frame_view(
        &os_port_tms570_state.LastSavedTaskFrameView,
        (pending_view != NULL_PTR) ? &pending_view->Frame
                                   : &os_port_tms570_task_context[current_task].RuntimeFrame,
        (pending_view != NULL_PTR)
            ? pending_view->FrameBytes
            : os_port_tms570_get_task_frame_bytes(
                  &os_port_tms570_task_context[current_task].RuntimeFrame));
    os_port_tms570_state.LastSavedTask = current_task;
    os_port_tms570_publish_last_saved_task_frame_view();
    os_port_tms570_state.LastSavedTimeSlice =
        (pending_view != NULL_PTR) ? pending_view->Frame.TimeSlice : os_port_tms570_state.CurrentTimeSlice;
    os_port_tms570_state.CurrentTimeSlice = 0u;
    os_port_tms570_state.CurrentTask = INVALID_TASK;
    os_port_tms570_state.CurrentTaskSp = (uintptr_t)0u;
    os_port_tms570_state.CurrentTaskLinkRegister = (uintptr_t)0u;
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
    os_port_tms570_state.DispatchRequested = TRUE;
    os_port_tms570_state.DispatchRequestCount++;
    os_port_tms570_state.SolicitedSaveInProgress = FALSE;
    os_port_tms570_clear_task_frame_view(&os_port_tms570_pending_solicited_save_task_frame_view);
}

StatusType Os_Port_Tms570_SolicitedSystemReturn(void)
{
    StatusType status = Os_Port_Tms570_BeginSolicitedSystemReturn();

    if (status != E_OK) {
        return status;
    }

    Os_Port_Tms570_FinishSolicitedSystemReturn();
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
    os_port_tms570_state.CurrentTaskSp = os_port_tms570_task_context[TaskID].RuntimeFrame.Sp;
    os_port_tms570_state.SelectedNextTask = INVALID_TASK;
    os_port_tms570_state.DispatchRequested = FALSE;
    os_port_tms570_state.DeferredDispatch = FALSE;
    os_port_tms570_state.IrqSchedulerReturnInProgress = FALSE;
    os_port_tms570_state.FiqSchedulerReturnInProgress = FALSE;
    os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_THREAD;
}

void Os_Port_Tms570_ObserveKernelDispatch(TaskType TaskID)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE)) {
        return;
    }

    os_port_tms570_state.LastObservedKernelTask = TaskID;
    os_port_tms570_state.KernelDispatchObserveCount++;

    if ((os_port_tms570_state.IrqSchedulerReturnInProgress == TRUE) &&
        (os_port_tms570_state.SelectedNextTask == TaskID)) {
        Os_Port_Tms570_FinishIrqSchedulerReturn();
    } else if ((os_port_tms570_state.FiqSchedulerReturnInProgress == TRUE) &&
               (os_port_tms570_state.SelectedNextTask == TaskID)) {
        Os_Port_Tms570_FinishFiqSchedulerReturn();
    }
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
    os_port_tms570_capture_runtime_task_irq_metadata(
        &os_port_tms570_task_context[current_task],
        os_port_tms570_state.CurrentIrqReturnAddress);
    os_port_tms570_commit_minimal_saved_runtime_sp(
        &os_port_tms570_task_context[current_task], Sp, OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES);
    os_port_tms570_state.IrqCapturedTask = current_task;
    os_port_tms570_state.IrqCapturedTaskSp =
        os_port_tms570_task_context[current_task].RuntimeFrame.Sp;
    return E_OK;
}

uintptr_t Os_Port_Tms570_PeekRestoreTaskSp(void)
{
    const Os_Port_Tms570_TaskFrameType* frame = Os_Port_Tms570_PeekRestoreTaskFrame();

    if ((frame == NULL_PTR) || (frame->Valid == FALSE)) {
        return (uintptr_t)0u;
    }

    return frame->Sp;
}

const Os_Port_Tms570_TaskFrameType* Os_Port_Tms570_PeekRestoreTaskFrame(void)
{
    const Os_Port_Tms570_TaskFrameViewType* pending_view =
        Os_Port_Tms570_PeekPendingRestoreTaskFrameView();
    TaskType target_task = os_port_tms570_state.CurrentTask;

    if (pending_view != NULL_PTR) {
        return &pending_view->Frame;
    }

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return NULL_PTR;
    }

    if (os_port_tms570_state.SelectedNextTask != INVALID_TASK) {
        target_task = os_port_tms570_state.SelectedNextTask;
    }

    if ((target_task == INVALID_TASK) ||
        (os_port_tms570_is_valid_task(target_task) == FALSE) ||
        (os_port_tms570_task_context[target_task].Prepared == FALSE)) {
        return NULL_PTR;
    }

    return &os_port_tms570_task_context[target_task].RuntimeFrame;
}

const Os_Port_Tms570_TaskFrameViewType* Os_Port_Tms570_PeekPendingRestoreTaskFrameView(void)
{
    const Os_Port_Tms570_TaskFrameViewType* pending_view =
        Os_Port_Tms570_PeekPendingFirstTaskFrameView();

    if (pending_view != NULL_PTR) {
        return pending_view;
    }

    pending_view = Os_Port_Tms570_PeekPendingIrqRestoreTaskFrameView();
    if (pending_view != NULL_PTR) {
        return pending_view;
    }

    pending_view = Os_Port_Tms570_PeekPendingIrqSchedulerReturnTaskFrameView();
    if (pending_view != NULL_PTR) {
        return pending_view;
    }

    pending_view = Os_Port_Tms570_PeekPendingFiqRestoreTaskFrameView();
    if (pending_view != NULL_PTR) {
        return pending_view;
    }

    return Os_Port_Tms570_PeekPendingFiqSchedulerReturnTaskFrameView();
}

const Os_Port_Tms570_TaskLowerSnapshotType* Os_Port_Tms570_PeekRestoreTaskLower(void)
{
    const Os_Port_Tms570_TaskFrameType* frame = Os_Port_Tms570_PeekRestoreTaskFrame();

    if ((frame == NULL_PTR) || (frame->Valid == FALSE)) {
        return NULL_PTR;
    }

    return &frame->TaskLower;
}

const Os_Port_Tms570_IrqScratchSnapshotType* Os_Port_Tms570_PeekRestoreTaskIrqScratch(void)
{
    const Os_Port_Tms570_TaskFrameType* frame = Os_Port_Tms570_PeekRestoreTaskFrame();

    if ((frame == NULL_PTR) || (frame->Valid == FALSE)) {
        return NULL_PTR;
    }

    return &frame->IrqScratch;
}

const Os_Port_Tms570_PreservedSnapshotType* Os_Port_Tms570_PeekRestoreTaskPreserved(void)
{
    const Os_Port_Tms570_TaskFrameType* frame = Os_Port_Tms570_PeekRestoreTaskFrame();

    if ((frame == NULL_PTR) || (frame->Valid == FALSE)) {
        return NULL_PTR;
    }

    return &frame->Preserved;
}

const Os_Port_Tms570_VfpStateType* Os_Port_Tms570_PeekRestoreTaskVfp(void)
{
    const Os_Port_Tms570_TaskFrameType* frame = Os_Port_Tms570_PeekRestoreTaskFrame();

    if ((frame == NULL_PTR) || (frame->Valid == FALSE)) {
        return NULL_PTR;
    }

    return &frame->Vfp;
}

uintptr_t Os_Port_Tms570_PeekRestoreTaskReturnAddress(void)
{
    const Os_Port_Tms570_TaskFrameType* frame = Os_Port_Tms570_PeekRestoreTaskFrame();

    if ((frame == NULL_PTR) || (frame->Valid == FALSE)) {
        return (uintptr_t)0u;
    }

    return frame->ReturnAddress;
}

uintptr_t Os_Port_Tms570_PeekRestoreTaskLinkRegister(void)
{
    const Os_Port_Tms570_TaskFrameType* frame = Os_Port_Tms570_PeekRestoreTaskFrame();

    if ((frame == NULL_PTR) || (frame->Valid == FALSE)) {
        return (uintptr_t)0u;
    }

    return frame->LinkRegister;
}

uint32 Os_Port_Tms570_PeekRestoreTaskCpsr(void)
{
    const Os_Port_Tms570_TaskFrameType* frame = Os_Port_Tms570_PeekRestoreTaskFrame();

    if ((frame == NULL_PTR) || (frame->Valid == FALSE)) {
        return 0u;
    }

    return frame->Cpsr;
}

uint32 Os_Port_Tms570_PeekRestoreTaskStackType(void)
{
    const Os_Port_Tms570_TaskFrameType* frame = Os_Port_Tms570_PeekRestoreTaskFrame();

    if ((frame == NULL_PTR) || (frame->Valid == FALSE)) {
        return 0u;
    }

    return frame->StackType;
}

uint32 Os_Port_Tms570_PeekRestoreTaskFrameBytes(void)
{
    const Os_Port_Tms570_TaskFrameViewType* pending_view =
        Os_Port_Tms570_PeekPendingRestoreTaskFrameView();

    if (pending_view != NULL_PTR) {
        return pending_view->FrameBytes;
    }

    return os_port_tms570_get_task_frame_bytes(Os_Port_Tms570_PeekRestoreTaskFrame());
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

    if (Os_Port_Tms570_BeginFirstTaskStart() != E_OK) {
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
    Os_Port_Tms570_IrqScratchSnapshotType saved_scratch;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        os_port_tms570_state.LastFiqSaveAction = OS_PORT_TMS570_FIQ_SAVE_NONE;
        os_port_tms570_state.LastFiqSaveContinuationAction = OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NONE;
        os_port_tms570_state.FiqSaveInProgress = FALSE;
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
    saved_scratch = os_port_tms570_get_fiq_context_scratch_snapshot(save_action);
    os_port_tms570_state.LastSavedFiqContextCpsr = os_port_tms570_state.CurrentFiqSavedCpsr;
    os_port_tms570_state.LastSavedFiqContextScratch = saved_scratch;
    os_port_tms570_state.FiqSaveInProgress =
        (boolean)(save_action != OS_PORT_TMS570_FIQ_SAVE_NONE);
    os_port_tms570_state.LastSavedFiqReturnAddress = os_port_tms570_state.CurrentFiqReturnAddress;
    return save_action;
}

void Os_Port_Tms570_FinishFiqContextSave(uint8 SaveAction)
{
    uint32 frame_bytes;
    Os_Port_Tms570_IrqScratchSnapshotType saved_scratch;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (SaveAction == OS_PORT_TMS570_FIQ_SAVE_NONE)) {
        os_port_tms570_state.FiqSaveInProgress = FALSE;
        return;
    }

    frame_bytes = os_port_tms570_get_fiq_context_frame_bytes(SaveAction);
    saved_scratch = os_port_tms570_get_fiq_context_scratch_snapshot(SaveAction);
    if (os_port_tms570_state.FiqContextDepth < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX) {
        os_port_tms570_fiq_context_frame_stack[os_port_tms570_state.FiqContextDepth] = frame_bytes;
        os_port_tms570_fiq_saved_cpsr_stack[os_port_tms570_state.FiqContextDepth] =
            os_port_tms570_state.CurrentFiqSavedCpsr;
        os_port_tms570_fiq_scratch_stack[os_port_tms570_state.FiqContextDepth] = saved_scratch;
    }
    os_port_tms570_state.LastSavedFiqContextCpsr = os_port_tms570_state.CurrentFiqSavedCpsr;
    os_port_tms570_state.LastSavedFiqContextBytes = frame_bytes;
    os_port_tms570_state.LastSavedFiqContextScratch = saved_scratch;
    os_port_tms570_set_interrupt_context(
        &os_port_tms570_state.LastSavedFiqContext,
        os_port_tms570_state.LastSavedFiqReturnAddress,
        os_port_tms570_state.LastSavedFiqContextCpsr,
        frame_bytes,
        (frame_bytes > 0u) ? &os_port_tms570_state.LastSavedFiqContextScratch : NULL_PTR);
    os_port_tms570_state.FiqInterruptStackBytes += frame_bytes;
    if (os_port_tms570_state.FiqInterruptStackBytes > os_port_tms570_state.FiqInterruptStackPeakBytes) {
        os_port_tms570_state.FiqInterruptStackPeakBytes = os_port_tms570_state.FiqInterruptStackBytes;
    }
    os_port_tms570_state.FiqContextSaveCount++;
    os_port_tms570_state.FiqContextDepth++;
    if (SaveAction == OS_PORT_TMS570_FIQ_SAVE_NESTED_FIQ) {
        os_port_tms570_state.NestedFiqReturnCount++;
    } else {
        if ((SaveAction == OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY) &&
            (os_port_tms570_state.CurrentTask != INVALID_TASK) &&
            (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == TRUE) &&
            (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == TRUE)) {
            os_port_tms570_capture_runtime_task_interrupt_context(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask],
                &os_port_tms570_state.LastSavedFiqContext);
            os_port_tms570_capture_runtime_task_lower(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask]);
            os_port_tms570_capture_runtime_task_link_register(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask]);
            os_port_tms570_capture_runtime_task_preserved(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask]);
            os_port_tms570_capture_runtime_task_vfp(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask]);
            os_port_tms570_commit_minimal_saved_runtime_sp(
                &os_port_tms570_task_context[os_port_tms570_state.CurrentTask],
                os_port_tms570_state.CurrentTaskSp,
                OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES);
        }
        os_port_tms570_state.FiqProcessingEnterCount++;
    }
    Os_Port_Tms570_EnterFiq();
    os_port_tms570_state.FiqSaveInProgress = FALSE;
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
    uintptr_t restore_address = (uintptr_t)0u;

    os_port_tms570_state.LastFiqRestoreAction = restore_action;
    os_port_tms570_state.FiqRestoreInProgress = (boolean)(restore_action != OS_PORT_TMS570_FIQ_RESTORE_NONE);
    if (restore_action != OS_PORT_TMS570_FIQ_RESTORE_NONE) {
        frame_index = (uint8)(os_port_tms570_state.FiqContextDepth - 1u);
        if (frame_index < OS_PORT_TMS570_FIQ_RETURN_STACK_MAX) {
            restore_address = os_port_tms570_fiq_return_stack[frame_index];
            frame_bytes = os_port_tms570_fiq_context_frame_stack[frame_index];
            os_port_tms570_fiq_context_frame_stack[frame_index] = 0u;
            os_port_tms570_state.CurrentFiqSavedCpsr = os_port_tms570_fiq_saved_cpsr_stack[frame_index];
            os_port_tms570_fiq_saved_cpsr_stack[frame_index] = 0u;
            os_port_tms570_state.CurrentFiqScratch = os_port_tms570_fiq_scratch_stack[frame_index];
            os_port_tms570_state.LastRestoredFiqContextScratch =
                os_port_tms570_fiq_scratch_stack[frame_index];
            os_port_tms570_clear_irq_scratch_snapshot(&os_port_tms570_fiq_scratch_stack[frame_index]);
        }
        os_port_tms570_state.LastRestoredFiqContextCpsr = os_port_tms570_state.CurrentFiqSavedCpsr;
        os_port_tms570_state.LastRestoredFiqContextBytes = frame_bytes;
        os_port_tms570_state.LastRestoredFiqReturnAddress = restore_address;
        os_port_tms570_set_interrupt_context(
            &os_port_tms570_state.LastRestoredFiqContext,
            restore_address,
            os_port_tms570_state.LastRestoredFiqContextCpsr,
            frame_bytes,
            (frame_bytes > 0u) ? &os_port_tms570_state.LastRestoredFiqContextScratch : NULL_PTR);
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
    uint8 resume_mode = os_port_tms570_state.FiqResumeMode;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (RestoreAction == OS_PORT_TMS570_FIQ_RESTORE_NONE)) {
        os_port_tms570_state.FiqRestoreInProgress = FALSE;
        return;
    }

    Os_Port_Tms570_ExitFiq();

    if ((RestoreAction == OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM) ||
        (RestoreAction == OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER)) {
        os_port_tms570_begin_fiq_scheduler_return(RestoreAction);
        os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
        os_port_tms570_state.FiqSchedulerReturnCount++;
    } else if ((RestoreAction == OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE) &&
               (resume_mode != OS_PORT_TMS570_MODE_IRQ) &&
               (os_port_tms570_state.CurrentTask != INVALID_TASK) &&
               (os_port_tms570_is_valid_task(os_port_tms570_state.CurrentTask) == TRUE) &&
               (os_port_tms570_task_context[os_port_tms570_state.CurrentTask].Prepared == TRUE)) {
        os_port_tms570_apply_runtime_frame_to_live_fiq_resume_state(
            &os_port_tms570_task_context[os_port_tms570_state.CurrentTask].RuntimeFrame);
    }
    os_port_tms570_state.FiqRestoreInProgress = FALSE;
}

void Os_Port_Tms570_FinishFiqSchedulerReturn(void)
{
    TaskType target_task = INVALID_TASK;
    TaskType saved_task = os_port_tms570_state.LastSavedTask;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FiqSchedulerReturnInProgress == FALSE)) {
        os_port_tms570_state.FiqSchedulerReturnInProgress = FALSE;
        return;
    }

    if (os_port_tms570_state.DispatchRequested == TRUE) {
        if (os_port_tms570_state.SelectedNextTask != INVALID_TASK) {
            target_task = os_port_tms570_state.SelectedNextTask;
        }

        os_port_tms570_state.CurrentTask = target_task;
        if ((target_task != INVALID_TASK) &&
            (os_port_tms570_is_valid_task(target_task) == TRUE) &&
            (os_port_tms570_task_context[target_task].Prepared == TRUE)) {
            os_port_tms570_apply_runtime_frame_to_live_restore_state(
                &os_port_tms570_task_context[target_task].RuntimeFrame);
            os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_THREAD;
        } else {
            os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
            os_port_tms570_state.CurrentTimeSlice = 0u;
        }

        if (target_task != saved_task) {
            os_port_tms570_state.TaskSwitchCount++;
        }
        os_port_tms570_state.SelectedNextTask = INVALID_TASK;
        os_port_tms570_state.DispatchRequested = FALSE;
        os_port_tms570_state.DeferredDispatch = FALSE;
    } else {
        os_port_tms570_state.CurrentExecutionMode = OS_PORT_TMS570_MODE_SYSTEM;
    }

    os_port_tms570_state.FiqSchedulerReturnInProgress = FALSE;
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
    if (Os_Port_Tms570_PeekRestoreTaskSp() == (uintptr_t)0u) {
        return;
    }

    Os_Port_Tms570_FinishFirstTaskStart();
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
    if (os_port_tms570_state.IrqSchedulerReturnInProgress == TRUE) {
        Os_Port_Tms570_FinishIrqSchedulerReturn();
    }
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


/* ---------------------------------------------------------------------------
 * HALCoGen bring-up glue
 *
 * On target, these functions will call through to HALCoGen-generated code
 * (vimInit, vimChannelMap, rtiInit, etc.).  In UNIT_TEST mode they configure
 * the bootstrap model's VIM/RTI state so that the same lifecycle tests work
 * without real hardware.
 * --------------------------------------------------------------------------- */

StatusType Os_Port_Tms570_HalVimInit(void)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

#if defined(UNIT_TEST)
    os_port_tms570_reset_vim_isr_table();
    os_port_tms570_state.VimReqmaskset0 = 0u;
    os_port_tms570_state.VimChanctrl0 = 0u;
    os_port_tms570_state.VimConfigured = TRUE;
#else
    vimInit();
    os_port_tms570_state.VimConfigured = TRUE;
#endif

    return E_OK;
}

StatusType Os_Port_Tms570_HalVimMapTickChannel(uint32 VimChannel, uint32 RequestId)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.VimConfigured == FALSE) ||
        (VimChannel < 2u) || (VimChannel >= 96u)) {
        return E_OS_STATE;
    }

#if defined(UNIT_TEST)
    /* Model: store handler address and set CHANCTRL mapping */
    os_port_tms570_vim_isr_table[VimChannel + 1u] =
        (uintptr_t)&Os_Port_Tms570_RtiTickHandler;
    os_port_tms570_state.VimRtiCompare0HandlerAddress =
        (uintptr_t)&Os_Port_Tms570_RtiTickHandler;
    (void)RequestId;
#else
    vimChannelMap(RequestId, VimChannel,
                  (t_isrFuncPTR)&Os_Port_Tms570_RtiTickHandler);
    os_port_tms570_state.VimRtiCompare0HandlerAddress =
        (uintptr_t)&Os_Port_Tms570_RtiTickHandler;
#endif

    return E_OK;
}

StatusType Os_Port_Tms570_HalVimEnableChannel(uint32 VimChannel)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.VimConfigured == FALSE) ||
        (VimChannel >= 32u)) {
        return E_OS_STATE;
    }

#if defined(UNIT_TEST)
    os_port_tms570_state.VimReqmaskset0 |= ((uint32)1u << VimChannel);
#else
    vimEnableInterrupt(VimChannel, SYS_IRQ);
#endif

    return E_OK;
}

StatusType Os_Port_Tms570_HalVimDisableChannel(uint32 VimChannel)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.VimConfigured == FALSE) ||
        (VimChannel >= 32u)) {
        return E_OS_STATE;
    }

#if defined(UNIT_TEST)
    os_port_tms570_state.VimReqmaskset0 &= ~((uint32)1u << VimChannel);
#else
    vimDisableInterrupt(VimChannel);
#endif

    return E_OK;
}

StatusType Os_Port_Tms570_HalRtiInit(uint32 Compare0Period)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

#if defined(UNIT_TEST)
    os_port_tms570_state.RtiCmp0Comp = Compare0Period;
    os_port_tms570_state.RtiCmp0Udcp = Compare0Period;
    os_port_tms570_state.RtiIntflag = 0u;
    os_port_tms570_state.RtiSetintena = 0u;
    os_port_tms570_state.RtiConfigured = TRUE;
#else
    rtiREG1->CMP[0u].COMPx = Compare0Period;
    rtiREG1->CMP[0u].UDCPx = Compare0Period;
    rtiREG1->INTFLAG = (uint32)1u;  /* Clear pending compare0 flag */
    rtiREG1->CLEARINTENA = (uint32)1u;  /* Disable compare0 until start */
    os_port_tms570_state.RtiConfigured = TRUE;
#endif

    return E_OK;
}

StatusType Os_Port_Tms570_HalRtiStart(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.RtiConfigured == FALSE)) {
        return E_OS_STATE;
    }

#if defined(UNIT_TEST)
    os_port_tms570_state.RtiGctrl |= OS_PORT_TMS570_RTI_GCTRL_COUNTER0_ENABLE;
    os_port_tms570_state.RtiSetintena |= OS_PORT_TMS570_RTI_COMPARE0_INTFLAG;
#else
    rtiREG1->GCTRL |= (uint32)1u;   /* Enable counter block 0 */
    rtiREG1->SETINTENA = (uint32)1u; /* Enable compare0 interrupt */
#endif

    return E_OK;
}

StatusType Os_Port_Tms570_HalRtiAcknowledgeCompare0(void)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.RtiConfigured == FALSE)) {
        return E_OS_STATE;
    }

#if defined(UNIT_TEST)
    os_port_tms570_acknowledge_rti_compare0();
#else
    rtiREG1->INTFLAG = (uint32)1u;   /* Acknowledge compare0 interrupt */
#endif

    return E_OK;
}
StatusType Os_Port_Tms570_TestSetIrqReturnAddress(uintptr_t Address)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Address == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentIrqReturnAddress = Address;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(uint32 Cpsr)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentIrqSavedCpsr = Cpsr;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetCurrentIrqScratch(
    const Os_Port_Tms570_IrqScratchSnapshotType* Snapshot)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Snapshot == NULL_PTR)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentIrqScratch = *Snapshot;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetCurrentTaskLower(
    const Os_Port_Tms570_TaskLowerSnapshotType* Snapshot)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Snapshot == NULL_PTR)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentTaskLower = *Snapshot;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetCurrentTaskPreserved(
    const Os_Port_Tms570_PreservedSnapshotType* Snapshot)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Snapshot == NULL_PTR)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentTaskPreserved = *Snapshot;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetCurrentTaskVfp(
    const Os_Port_Tms570_VfpStateType* State)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (State == NULL_PTR)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentTaskVfp = *State;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetCurrentTaskLinkRegister(uintptr_t LinkRegister)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (os_port_tms570_state.CurrentTask == INVALID_TASK) ||
        (LinkRegister == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentTaskLinkRegister = LinkRegister;
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

StatusType Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(uint32 Cpsr)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentFiqSavedCpsr = Cpsr;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetCurrentFiqScratch(
    const Os_Port_Tms570_IrqScratchSnapshotType* Snapshot)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) || (Snapshot == NULL_PTR)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentFiqScratch = *Snapshot;
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
    if (channel != OS_PORT_TMS570_VIM_NO_CHANNEL) {
        os_port_tms570_state.VimIrqIndex = channel + 1u;
        if (Os_Port_Tms570_ReadActiveIrqVector(&vector_address) != E_OK) {
            os_port_tms570_state.VimIrqIndex = 0u;
            os_port_tms570_state.VimIrqVecReg = (uintptr_t)0u;
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

    if ((os_port_tms570_state.VimIrqIndex == 0u) ||
        (os_port_tms570_state.VimIrqVecReg == (uintptr_t)0u)) {
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
    uintptr_t vector_address;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((Os_Port_Tms570_ReadActiveIrqChannel(&channel) != E_OK) ||
        (Os_Port_Tms570_ReadActiveIrqVector(&vector_address) != E_OK)) {
        return E_OS_NOFUNC;
    }

    if (Os_Port_Tms570_PulseActiveIrqMask() != E_OK) {
        return E_OS_NOFUNC;
    }
    if (Os_Port_Tms570_ServiceActiveIrqChannelCore(channel, vector_address) != E_OK) {
        return E_OS_NOFUNC;
    }
    os_port_tms570_state.VimLastServicedChannel = channel;
    os_port_tms570_state.VimLastIrqIndex = os_port_tms570_state.VimIrqIndex;
    os_port_tms570_state.VimLastIrqVecReg = os_port_tms570_state.VimIrqVecReg;
    os_port_tms570_state.VimIntreq0 &= ~((uint32)1u << channel);
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

StatusType Os_Port_Tms570_ServiceActiveIrqChannelCore(uint32 Channel, uintptr_t VectorAddress)
{
    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    switch (Channel) {
        case OS_PORT_TMS570_VIM_RTI_COMPARE0_CHANNEL:
            if (VectorAddress != os_port_tms570_state.VimRtiCompare0HandlerAddress) {
                return E_OS_NOFUNC;
            }
            Os_Port_Tms570_RtiTickServiceCore();
            return E_OK;

        default:
            return E_OS_NOFUNC;
    }
}

StatusType Os_Port_Tms570_InvokeActiveIrqVectorCore(void)
{
    uint32 channel;
    uintptr_t vector_address;

    if (os_port_tms570_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((Os_Port_Tms570_ReadActiveIrqChannel(&channel) != E_OK) ||
        (Os_Port_Tms570_ReadActiveIrqVector(&vector_address) != E_OK)) {
        return E_OS_NOFUNC;
    }

    return Os_Port_Tms570_ServiceActiveIrqChannelCore(channel, vector_address);
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
    TaskType current_task;

    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_state.FirstTaskStarted == FALSE) ||
        (os_port_tms570_state.CurrentTask == INVALID_TASK) ||
        (Sp == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_state.CurrentTaskSp = Sp;

    if ((os_port_tms570_state.IrqContextDepth != 0u) ||
        (os_port_tms570_state.FiqContextDepth != 0u)) {
        return E_OK;
    }

    current_task = os_port_tms570_state.CurrentTask;
    if ((os_port_tms570_is_valid_task(current_task) == FALSE) ||
        (os_port_tms570_task_context[current_task].Prepared == FALSE)) {
        return E_OK;
    }

    os_port_tms570_set_runtime_task_sp(&os_port_tms570_task_context[current_task], Sp);
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

    os_port_tms570_set_runtime_task_time_slice(&os_port_tms570_task_context[TaskID], TimeSlice);
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetTaskSavedCpsr(TaskType TaskID, uint32 Cpsr)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE)) {
        return E_OS_STATE;
    }

    os_port_tms570_task_context[TaskID].RuntimeFrame.Valid = TRUE;
    os_port_tms570_task_context[TaskID].RuntimeFrame.Cpsr = Cpsr;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetTaskSavedReturnAddress(TaskType TaskID, uintptr_t ReturnAddress)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE) ||
        (ReturnAddress == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_task_context[TaskID].RuntimeFrame.Valid = TRUE;
    os_port_tms570_task_context[TaskID].RuntimeFrame.ReturnAddress = ReturnAddress;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetTaskSavedIrqScratch(
    TaskType TaskID,
    const Os_Port_Tms570_IrqScratchSnapshotType* Snapshot)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE) ||
        (Snapshot == NULL_PTR)) {
        return E_OS_STATE;
    }

    os_port_tms570_task_context[TaskID].RuntimeFrame.Valid = TRUE;
    os_port_tms570_task_context[TaskID].RuntimeFrame.IrqScratch = *Snapshot;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetTaskSavedLower(
    TaskType TaskID,
    const Os_Port_Tms570_TaskLowerSnapshotType* Snapshot)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE) ||
        (Snapshot == NULL_PTR)) {
        return E_OS_STATE;
    }

    os_port_tms570_set_runtime_task_lower(&os_port_tms570_task_context[TaskID], Snapshot);
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetTaskSavedStackType(TaskType TaskID, uint32 StackType)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE) ||
        ((StackType != OS_PORT_TMS570_SOLICITED_STACK_TYPE) &&
         (StackType != OS_PORT_TMS570_INITIAL_STACK_TYPE))) {
        return E_OS_STATE;
    }

    os_port_tms570_task_context[TaskID].RuntimeFrame.Valid = TRUE;
    os_port_tms570_task_context[TaskID].RuntimeFrame.StackType = StackType;
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetTaskSavedVfp(
    TaskType TaskID,
    const Os_Port_Tms570_VfpStateType* State)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE) ||
        (State == NULL_PTR)) {
        return E_OS_STATE;
    }

    os_port_tms570_set_runtime_task_vfp(&os_port_tms570_task_context[TaskID], State);
    return E_OK;
}

StatusType Os_Port_Tms570_TestSetTaskSavedLinkRegister(TaskType TaskID, uintptr_t LinkRegister)
{
    if ((os_port_tms570_state.TargetInitialized == FALSE) ||
        (os_port_tms570_is_valid_task(TaskID) == FALSE) ||
        (os_port_tms570_task_context[TaskID].Prepared == FALSE) ||
        (LinkRegister == (uintptr_t)0u)) {
        return E_OS_STATE;
    }

    os_port_tms570_set_runtime_task_link_register(&os_port_tms570_task_context[TaskID],
                                                  LinkRegister);
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
