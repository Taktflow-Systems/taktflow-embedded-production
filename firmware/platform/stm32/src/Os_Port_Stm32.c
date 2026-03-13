/**
 * @file    Os_Port_Stm32.c
 * @brief   STM32 Cortex-M4 bootstrap OS port skeleton
 * @date    2026-03-13
 *
 * @details This is the first concrete STM32 OS port scaffold. It is not yet
 *          linked into the live OS build. Its job is to capture the port
 *          boundary and the bootstrapping state model we will later wire to
 *          real PendSV, SysTick, and first-task launch code.
 *
 *          Verified ThreadX references from d:\Compressed\threadx-master.zip:
 *          - threadx-master/ports/cortex_m4/gnu/inc/tx_port.h
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_thread_schedule.S
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_save.S
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_thread_context_restore.S
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_thread_stack_build.S
 *          - threadx-master/ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 *          - threadx-master/ports/cortex_m4/gnu/example_build/tx_initialize_low_level.S
 */
#include "Os_Port_Stm32.h"

#if defined(PLATFORM_STM32)

#define OS_PORT_STM32_INITIAL_FRAME_WORDS        17u
#define OS_PORT_STM32_INITIAL_FRAME_BYTES        (OS_PORT_STM32_INITIAL_FRAME_WORDS * sizeof(uint32))
#define OS_PORT_STM32_XPSR_THUMB                 0x01000000u
#define OS_PORT_STM32_INITIAL_EXC_RETURN         0xFFFFFFFDu
#define OS_PORT_STM32_INITIAL_TASK_LR            0xFFFFFFFFu
#define OS_PORT_STM32_SOFTWARE_RESTORE_WORDS     9u
#define OS_PORT_STM32_SOFTWARE_RESTORE_BYTES     (OS_PORT_STM32_SOFTWARE_RESTORE_WORDS * sizeof(uint32))
#define OS_PORT_STM32_PENDSV_LOWEST_PRIORITY     0xFFu
#define OS_PORT_STM32_SYSTICK_BOOTSTRAP_PRIORITY 0x40u

static Os_Port_Stm32_StateType os_port_stm32_state;
static Os_Port_Stm32_TaskContextType os_port_stm32_task_context[OS_MAX_TASKS];

static uintptr_t os_port_stm32_align_down(uintptr_t Value, uintptr_t Alignment)
{
    return (Value & ~(Alignment - 1u));
}

static boolean os_port_stm32_is_valid_task(TaskType TaskID)
{
    return (boolean)(TaskID < OS_MAX_TASKS);
}

static void os_port_stm32_build_initial_frame(uintptr_t FrameBase, Os_TaskEntryType Entry)
{
    uint32* frame = (uint32*)FrameBase;
    uint32 index;

    frame[0] = OS_PORT_STM32_INITIAL_EXC_RETURN;

    for (index = 1u; index <= 13u; index++) {
        frame[index] = 0u;
    }

    frame[14] = OS_PORT_STM32_INITIAL_TASK_LR;
    frame[15] = (uint32)((uintptr_t)Entry & 0xFFFFFFFFu);
    frame[16] = OS_PORT_STM32_XPSR_THUMB;
}

static uintptr_t os_port_stm32_get_restore_psp(uintptr_t SavedPsp)
{
    if (SavedPsp == (uintptr_t)0u) {
        return (uintptr_t)0u;
    }

    return SavedPsp + (uintptr_t)OS_PORT_STM32_SOFTWARE_RESTORE_BYTES;
}

static uintptr_t os_port_stm32_get_saved_psp(uintptr_t ActivePsp)
{
    if (ActivePsp < (uintptr_t)OS_PORT_STM32_SOFTWARE_RESTORE_BYTES) {
        return (uintptr_t)0u;
    }

    return ActivePsp - (uintptr_t)OS_PORT_STM32_SOFTWARE_RESTORE_BYTES;
}

static uintptr_t os_port_stm32_get_first_task_restore_psp(void)
{
    return os_port_stm32_get_restore_psp(os_port_stm32_state.FirstTaskPsp);
}

static void os_port_stm32_reset_task_contexts(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_TASKS; idx++) {
        os_port_stm32_task_context[idx].Prepared = FALSE;
        os_port_stm32_task_context[idx].TaskID = idx;
        os_port_stm32_task_context[idx].StackTop = (uintptr_t)0u;
        os_port_stm32_task_context[idx].SavedPsp = (uintptr_t)0u;
        os_port_stm32_task_context[idx].RestorePsp = (uintptr_t)0u;
        os_port_stm32_task_context[idx].Entry = (Os_TaskEntryType)0;
    }
}

const Os_Port_Stm32_TaskContextType* Os_Port_Stm32_GetTaskContext(TaskType TaskID)
{
    if (os_port_stm32_is_valid_task(TaskID) == FALSE) {
        return (const Os_Port_Stm32_TaskContextType*)0;
    }

    return &os_port_stm32_task_context[TaskID];
}

uintptr_t Os_Port_Stm32_GetPreparedFirstTaskPsp(void)
{
    return os_port_stm32_state.FirstTaskPsp;
}

uint32 Os_Port_Stm32_IsFirstTaskStarted(void)
{
    return (os_port_stm32_state.FirstTaskStarted != FALSE) ? 1u : 0u;
}

uintptr_t Os_Port_Stm32_ResolvePendSvTarget(uintptr_t CurrentActivePsp)
{
    uintptr_t current_saved_psp;
    uintptr_t target_saved_psp;
    TaskType target_task;

    if (os_port_stm32_state.FirstTaskStarted == FALSE) {
        return (uintptr_t)0u;
    }

    current_saved_psp = os_port_stm32_get_saved_psp(CurrentActivePsp);
    if (current_saved_psp == (uintptr_t)0u) {
        return (uintptr_t)0u;
    }

    target_saved_psp = current_saved_psp;
    target_task = os_port_stm32_state.CurrentTask;
    if (os_port_stm32_state.SelectedNextTaskPsp != (uintptr_t)0u) {
        target_saved_psp = os_port_stm32_state.SelectedNextTaskPsp;
    }

    if (os_port_stm32_state.SelectedNextTask != INVALID_TASK) {
        target_task = os_port_stm32_state.SelectedNextTask;
    }

    os_port_stm32_state.LastSavedPsp = current_saved_psp;
    os_port_stm32_state.LastSavedTask = os_port_stm32_state.CurrentTask;
    if (target_saved_psp != current_saved_psp) {
        os_port_stm32_state.TaskSwitchCount++;
    }

    os_port_stm32_state.CurrentTask = target_task;
    os_port_stm32_state.SelectedNextTask = INVALID_TASK;
    os_port_stm32_state.SelectedNextTaskPsp = (uintptr_t)0u;
    return os_port_stm32_get_restore_psp(target_saved_psp);
}

void Os_Port_Stm32_MarkFirstTaskStarted(uintptr_t ActivePsp)
{
    os_port_stm32_state.FirstTaskStarted = TRUE;
    os_port_stm32_state.PendSvPending = FALSE;
    os_port_stm32_state.DeferredPendSv = FALSE;
    os_port_stm32_state.ActivePsp = ActivePsp;
    os_port_stm32_state.CurrentTask = os_port_stm32_state.FirstTaskTaskID;
    os_port_stm32_state.FirstTaskLaunchCount++;
}

void Os_Port_Stm32_MarkPendSvComplete(uintptr_t ActivePsp)
{
    if (os_port_stm32_state.FirstTaskStarted == FALSE) {
        return;
    }

    os_port_stm32_state.PendSvPending = FALSE;
    os_port_stm32_state.DeferredPendSv = FALSE;
    os_port_stm32_state.ActivePsp = ActivePsp;
    os_port_stm32_state.PendSvCompleteCount++;
}

static void os_port_stm32_reset_state(void)
{
    os_port_stm32_reset_task_contexts();
    os_port_stm32_state.TargetInitialized = FALSE;
    os_port_stm32_state.SysTickConfigured = FALSE;
    os_port_stm32_state.PendSvPending = FALSE;
    os_port_stm32_state.FirstTaskPrepared = FALSE;
    os_port_stm32_state.FirstTaskStarted = FALSE;
    os_port_stm32_state.DeferredPendSv = FALSE;
    os_port_stm32_state.Isr2Nesting = 0u;
    os_port_stm32_state.PendSvPriority = OS_PORT_STM32_PENDSV_LOWEST_PRIORITY;
    os_port_stm32_state.SysTickPriority = OS_PORT_STM32_SYSTICK_BOOTSTRAP_PRIORITY;
    os_port_stm32_state.TickInterruptCount = 0u;
    os_port_stm32_state.PendSvRequestCount = 0u;
    os_port_stm32_state.FirstTaskLaunchCount = 0u;
    os_port_stm32_state.PendSvCompleteCount = 0u;
    os_port_stm32_state.TaskSwitchCount = 0u;
    os_port_stm32_state.KernelDispatchObserveCount = 0u;
    os_port_stm32_state.FirstTaskTaskID = INVALID_TASK;
    os_port_stm32_state.CurrentTask = INVALID_TASK;
    os_port_stm32_state.LastSavedTask = INVALID_TASK;
    os_port_stm32_state.LastObservedKernelTask = INVALID_TASK;
    os_port_stm32_state.SelectedNextTask = INVALID_TASK;
    os_port_stm32_state.FirstTaskEntryAddress = (uintptr_t)0u;
    os_port_stm32_state.FirstTaskStackTop = (uintptr_t)0u;
    os_port_stm32_state.FirstTaskPsp = (uintptr_t)0u;
    os_port_stm32_state.LastSavedPsp = (uintptr_t)0u;
    os_port_stm32_state.SelectedNextTaskPsp = (uintptr_t)0u;
    os_port_stm32_state.ActivePsp = (uintptr_t)0u;
    os_port_stm32_state.InitialXpsr = OS_PORT_STM32_XPSR_THUMB;
}

const Os_Port_Stm32_StateType* Os_Port_Stm32_GetBootstrapState(void)
{
    return &os_port_stm32_state;
}

void Os_PortTargetInit(void)
{
    /*
     * ThreadX study hook:
     * - tx_port.h shows what should live in the port boundary
     * - tx_initialize_low_level.S shows low-level timer/vector bring-up split
     *
     * Later work:
     * - configure SysTick or GPT-backed system tick
     * - configure PendSV priority to lowest exception level
     * - prepare first-task launch contract with the portable scheduler
     */
    os_port_stm32_reset_state();
    os_port_stm32_state.TargetInitialized = TRUE;
    os_port_stm32_state.SysTickConfigured = TRUE;
}

StatusType Os_Port_Stm32_PrepareTaskContext(
    TaskType TaskID,
    Os_TaskEntryType Entry,
    uintptr_t StackTop)
{
    uintptr_t prepared_psp;

    if (os_port_stm32_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((os_port_stm32_is_valid_task(TaskID) == FALSE) || (Entry == (Os_TaskEntryType)0) ||
        (StackTop < (uintptr_t)OS_PORT_STM32_INITIAL_FRAME_BYTES)) {
        return E_OS_VALUE;
    }

    prepared_psp =
        os_port_stm32_align_down(StackTop - (uintptr_t)OS_PORT_STM32_INITIAL_FRAME_BYTES, (uintptr_t)8u);
    if (prepared_psp == (uintptr_t)0u) {
        return E_OS_VALUE;
    }

    os_port_stm32_task_context[TaskID].Prepared = TRUE;
    os_port_stm32_task_context[TaskID].TaskID = TaskID;
    os_port_stm32_task_context[TaskID].StackTop = StackTop;
    os_port_stm32_task_context[TaskID].SavedPsp = prepared_psp;
    os_port_stm32_task_context[TaskID].RestorePsp = os_port_stm32_get_restore_psp(prepared_psp);
    os_port_stm32_task_context[TaskID].Entry = Entry;
    os_port_stm32_build_initial_frame(prepared_psp, Entry);
    return E_OK;
}

StatusType Os_Port_Stm32_PrepareFirstTask(TaskType TaskID, Os_TaskEntryType Entry, uintptr_t StackTop)
{
    StatusType status = Os_Port_Stm32_PrepareTaskContext(TaskID, Entry, StackTop);

    if (status != E_OK) {
        return status;
    }

    os_port_stm32_state.FirstTaskPrepared = TRUE;
    os_port_stm32_state.FirstTaskStarted = FALSE;
    os_port_stm32_state.PendSvPending = FALSE;
    os_port_stm32_state.DeferredPendSv = FALSE;
    os_port_stm32_state.ActivePsp = (uintptr_t)0u;
    os_port_stm32_state.FirstTaskTaskID = TaskID;
    os_port_stm32_state.FirstTaskEntryAddress = (uintptr_t)Entry;
    os_port_stm32_state.FirstTaskStackTop = StackTop;
    os_port_stm32_state.FirstTaskPsp = os_port_stm32_task_context[TaskID].SavedPsp;
    os_port_stm32_state.InitialXpsr = OS_PORT_STM32_XPSR_THUMB;
    return E_OK;
}

StatusType Os_Port_Stm32_SelectNextTask(TaskType TaskID)
{
    if (os_port_stm32_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((os_port_stm32_is_valid_task(TaskID) == FALSE) ||
        (os_port_stm32_task_context[TaskID].Prepared == FALSE)) {
        return E_OS_VALUE;
    }

    os_port_stm32_state.SelectedNextTask = TaskID;
    os_port_stm32_state.SelectedNextTaskPsp = os_port_stm32_task_context[TaskID].SavedPsp;
    return E_OK;
}

void Os_Port_Stm32_SynchronizeCurrentTask(TaskType TaskID)
{
    if ((os_port_stm32_state.TargetInitialized == FALSE) ||
        (os_port_stm32_state.FirstTaskStarted == FALSE) ||
        (os_port_stm32_is_valid_task(TaskID) == FALSE) ||
        (os_port_stm32_task_context[TaskID].Prepared == FALSE)) {
        return;
    }

    os_port_stm32_state.CurrentTask = TaskID;
    os_port_stm32_state.SelectedNextTask = INVALID_TASK;
    os_port_stm32_state.SelectedNextTaskPsp = (uintptr_t)0u;
}

void Os_Port_Stm32_ObserveKernelDispatch(TaskType TaskID)
{
    if ((os_port_stm32_state.TargetInitialized == FALSE) ||
        (os_port_stm32_is_valid_task(TaskID) == FALSE)) {
        return;
    }

    os_port_stm32_state.LastObservedKernelTask = TaskID;
    os_port_stm32_state.KernelDispatchObserveCount++;
}

void Os_PortStartFirstTask(void)
{
    /*
     * ThreadX study hook:
     * - tx_thread_schedule.S
     * - tx_thread_stack_build.S
     *
     * Later work:
     * - build the first synthetic exception frame
     * - switch from MSP bootstrap context to PSP thread context
     * - branch into the first runnable OSEK task
     */
    if ((os_port_stm32_state.TargetInitialized == FALSE) ||
        (os_port_stm32_state.FirstTaskPrepared == FALSE) ||
        (os_port_stm32_state.FirstTaskStarted == TRUE)) {
        return;
    }

    Os_Port_Stm32_StartFirstTaskAsm();
}

void Os_PortRequestContextSwitch(void)
{
    /*
     * ThreadX study hook:
     * - tx_thread_context_save.S
     * - tx_thread_context_restore.S
     *
     * Later work:
     * - pend PendSV
     * - leave actual register save/restore in assembly
     */
    if ((os_port_stm32_state.TargetInitialized == FALSE) ||
        (os_port_stm32_state.FirstTaskStarted == FALSE)) {
        return;
    }

    if (os_port_stm32_state.PendSvPending == TRUE) {
        return;
    }

    if (os_port_stm32_state.Isr2Nesting > 0u) {
        os_port_stm32_state.DeferredPendSv = TRUE;
        return;
    }

    os_port_stm32_state.PendSvPending = TRUE;
    os_port_stm32_state.PendSvRequestCount++;
}

void Os_PortEnterIsr2(void)
{
    if (os_port_stm32_state.TargetInitialized == FALSE) {
        return;
    }

    os_port_stm32_state.Isr2Nesting++;
    Os_BootstrapEnterIsr2();
}

void Os_PortExitIsr2(void)
{
    if (os_port_stm32_state.TargetInitialized == FALSE) {
        return;
    }

    Os_BootstrapExitIsr2();

    if (os_port_stm32_state.Isr2Nesting > 0u) {
        os_port_stm32_state.Isr2Nesting--;
    }

    if ((os_port_stm32_state.Isr2Nesting == 0u) &&
        (os_port_stm32_state.DeferredPendSv == TRUE) &&
        (os_port_stm32_state.FirstTaskStarted == TRUE)) {
        os_port_stm32_state.DeferredPendSv = FALSE;
        os_port_stm32_state.PendSvPending = TRUE;
        os_port_stm32_state.PendSvRequestCount++;
    }

    /*
     * ThreadX study hook:
     * - tx_timer_interrupt.S
     * - tx_thread_schedule.S
     *
     * Later work:
     * - trigger deferred dispatch after ISR exit if a higher-priority task is ready
     */
}

void Os_Port_Stm32_TickIsr(void)
{
    if (os_port_stm32_state.TargetInitialized == FALSE) {
        return;
    }

    /*
     * Later work:
     * - acknowledge SysTick or GPT source
     * - advance the OSEK system counter
     * - request dispatch if an alarm/task became ready
     */
    os_port_stm32_state.TickInterruptCount++;

    if (Os_BootstrapProcessCounterTick() == TRUE) {
        Os_PortRequestContextSwitch();
    }
}

#if defined(UNIT_TEST)
void Os_Port_Stm32_StartFirstTaskAsm(void)
{
    Os_Port_Stm32_MarkFirstTaskStarted(os_port_stm32_get_first_task_restore_psp());
}

void Os_Port_Stm32_PendSvHandler(void)
{
    uintptr_t next_active_psp;

    if ((os_port_stm32_state.FirstTaskStarted == FALSE) ||
        (os_port_stm32_state.PendSvPending == FALSE)) {
        return;
    }

    next_active_psp = Os_Port_Stm32_ResolvePendSvTarget(os_port_stm32_state.ActivePsp);
    if (next_active_psp == (uintptr_t)0u) {
        return;
    }

    Os_Port_Stm32_MarkPendSvComplete(next_active_psp);
}

void Os_Port_Stm32_SysTickHandler(void)
{
    Os_PortEnterIsr2();
    Os_Port_Stm32_TickIsr();
    Os_PortExitIsr2();
}
#endif

#endif
