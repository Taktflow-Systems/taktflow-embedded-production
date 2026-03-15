/**
 * @file    Os_Port_Stm32L5.c
 * @brief   STM32L5 Cortex-M33 bootstrap OS port
 * @date    2026-03-15
 *
 * @details Cortex-M33 OS port extending the M4 port with:
 *          - FPU lazy stacking: conditional s16-s31 save/restore
 *          - PSPLIM: per-task stack limit register
 *          - Variable frame size: 9 words (no FPU) or 25 words (with FPU)
 *
 *          Verified ThreadX references:
 *          - threadx-master/ports/cortex_m33/ac6/src/tx_thread_stack_build.S
 *          - threadx-master/ports/cortex_m33/ac6/src/tx_thread_context_save.S
 *          - threadx-master/ports/cortex_m33/ac6/src/tx_thread_context_restore.S
 */
#include "Os_Port_Stm32L5.h"

#if defined(PLATFORM_STM32L5)

/* Software frame: EXC_RETURN + r4-r11 = 9 words */
#define OS_PORT_M33_SW_FRAME_WORDS          9u
#define OS_PORT_M33_SW_FRAME_BYTES          (OS_PORT_M33_SW_FRAME_WORDS * sizeof(uint32))

/* Software frame with FPU: EXC_RETURN + r4-r11 + s16-s31 = 25 words */
#define OS_PORT_M33_SW_FPU_FRAME_WORDS      25u
#define OS_PORT_M33_SW_FPU_FRAME_BYTES      (OS_PORT_M33_SW_FPU_FRAME_WORDS * sizeof(uint32))

/* Initial frame for first launch: 9 software + 8 hardware = 17 words (no FPU on first launch) */
#define OS_PORT_M33_INITIAL_FRAME_WORDS     17u
#define OS_PORT_M33_INITIAL_FRAME_BYTES     (OS_PORT_M33_INITIAL_FRAME_WORDS * sizeof(uint32))

#define OS_PORT_M33_XPSR_THUMB              0x01000000u

/*
 * EXC_RETURN for Cortex-M33 (no TrustZone):
 * 0xFFFFFFFD = return to thread mode, use PSP, no FPU frame
 * Bit[4] = 1 means no FPU context stacked
 */
#define OS_PORT_M33_INITIAL_EXC_RETURN      0xFFFFFFFDu

/* EXC_RETURN bit[4]: 0 = FPU frame stacked, 1 = no FPU frame */
#define OS_PORT_M33_EXC_RETURN_FPU_BIT      0x00000010u

#define OS_PORT_M33_INITIAL_TASK_LR         0xFFFFFFFFu
#define OS_PORT_M33_PENDSV_LOWEST_PRIORITY  0xFFu
#define OS_PORT_M33_SYSTICK_BOOTSTRAP_PRIO  0x40u

static Os_Port_Stm32L5_StateType os_port_m33_state;
static Os_Port_Stm32L5_TaskContextType os_port_m33_task_context[OS_MAX_TASKS];

static uintptr_t os_port_m33_align_down(uintptr_t Value, uintptr_t Alignment)
{
    return (Value & ~(Alignment - 1u));
}

static boolean os_port_m33_is_valid_task(TaskType TaskID)
{
    return (boolean)(TaskID < OS_MAX_TASKS);
}

/**
 * @brief   Build synthetic exception frame for first task launch
 * @param   FrameBase  Aligned address where software frame starts
 * @param   Entry      Task entry point
 *
 * @note    Frame layout (17 words, no FPU on initial launch):
 *          [0]  EXC_RETURN (0xFFFFFFFD)
 *          [1-8]  r4-r11 (zeroed)
 *          [9-12] r0-r3 (zeroed — hardware frame)
 *          [13]   r12 (zeroed)
 *          [14]   lr (0xFFFFFFFF — task should not return)
 *          [15]   pc (Entry)
 *          [16]   xPSR (Thumb bit set)
 */
static void os_port_m33_build_initial_frame(uintptr_t FrameBase, Os_TaskEntryType Entry)
{
    uint32* frame = (uint32*)FrameBase;
    uint32 index;

    frame[0] = OS_PORT_M33_INITIAL_EXC_RETURN;

    for (index = 1u; index <= 13u; index++) {
        frame[index] = 0u;
    }

    frame[14] = OS_PORT_M33_INITIAL_TASK_LR;
    frame[15] = (uint32)((uintptr_t)Entry & 0xFFFFFFFFu);
    frame[16] = OS_PORT_M33_XPSR_THUMB;
}

/**
 * @brief   Compute restore PSP from saved PSP (skip software frame)
 * @param   SavedPsp   Stack pointer after software context push
 * @param   FpuActive  TRUE if FPU registers were saved
 * @return  PSP pointing to hardware frame (for exception return)
 */
static uintptr_t os_port_m33_get_restore_psp(uintptr_t SavedPsp, boolean FpuActive)
{
    uintptr_t sw_bytes;

    if (SavedPsp == (uintptr_t)0u) {
        return (uintptr_t)0u;
    }

    sw_bytes = (FpuActive != FALSE)
        ? (uintptr_t)OS_PORT_M33_SW_FPU_FRAME_BYTES
        : (uintptr_t)OS_PORT_M33_SW_FRAME_BYTES;

    return SavedPsp + sw_bytes;
}

static void os_port_m33_reset_task_contexts(void)
{
    uint8 idx;

    for (idx = 0u; idx < OS_MAX_TASKS; idx++) {
        os_port_m33_task_context[idx].Prepared = FALSE;
        os_port_m33_task_context[idx].TaskID = idx;
        os_port_m33_task_context[idx].StackTop = (uintptr_t)0u;
        os_port_m33_task_context[idx].StackLimit = (uintptr_t)0u;
        os_port_m33_task_context[idx].SavedPsp = (uintptr_t)0u;
        os_port_m33_task_context[idx].RestorePsp = (uintptr_t)0u;
        os_port_m33_task_context[idx].Entry = (Os_TaskEntryType)0;
        os_port_m33_task_context[idx].FpuActive = FALSE;
    }
}

const Os_Port_Stm32L5_TaskContextType* Os_Port_Stm32L5_GetTaskContext(TaskType TaskID)
{
    if (os_port_m33_is_valid_task(TaskID) == FALSE) {
        return (const Os_Port_Stm32L5_TaskContextType*)0;
    }

    return &os_port_m33_task_context[TaskID];
}

uintptr_t Os_Port_Stm32L5_GetPreparedFirstTaskPsp(void)
{
    return os_port_m33_state.FirstTaskPsp;
}

uint32 Os_Port_Stm32L5_IsFirstTaskStarted(void)
{
    return (os_port_m33_state.FirstTaskStarted != FALSE) ? 1u : 0u;
}

/**
 * @brief   Get stack limit (PSPLIM) for a task
 * @param   TaskID  Task identifier
 * @return  Stack limit address, or 0 if invalid
 * @note    Called from PendSV assembly to set PSPLIM on context restore
 */
uintptr_t Os_Port_Stm32L5_GetTaskStackLimit(TaskType TaskID)
{
    if ((os_port_m33_is_valid_task(TaskID) == FALSE) ||
        (os_port_m33_task_context[TaskID].Prepared == FALSE)) {
        return (uintptr_t)0u;
    }

    return os_port_m33_task_context[TaskID].StackLimit;
}

/**
 * @brief   Get stack limit for the current task
 * @return  PSPLIM value for the current task, or 0 if invalid
 * @note    Called from PendSV assembly — avoids hardcoded struct offsets
 */
uintptr_t Os_Port_Stm32L5_GetCurrentTaskStackLimit(void)
{
    return Os_Port_Stm32L5_GetTaskStackLimit(os_port_m33_state.CurrentTask);
}

void Os_Port_Stm32L5_MarkFirstTaskStarted(uintptr_t ActivePsp)
{
    os_port_m33_state.FirstTaskStarted = TRUE;
    os_port_m33_state.PendSvPending = FALSE;
    os_port_m33_state.DeferredPendSv = FALSE;
    os_port_m33_state.ActivePsp = ActivePsp;
    os_port_m33_state.CurrentTask = os_port_m33_state.FirstTaskTaskID;
    os_port_m33_state.FirstTaskLaunchCount++;
}

void Os_Port_Stm32L5_MarkPendSvComplete(uintptr_t ActivePsp)
{
    if (os_port_m33_state.FirstTaskStarted == FALSE) {
        return;
    }

    os_port_m33_state.PendSvPending = FALSE;
    os_port_m33_state.DeferredPendSv = FALSE;
    os_port_m33_state.ActivePsp = ActivePsp;
    os_port_m33_state.PendSvCompleteCount++;
}

/**
 * @brief   Save current task's PSP and FPU state after software context push
 * @param   SavedPsp   Task stack pointer after software context push
 * @param   ExcReturn  EXC_RETURN value (bit[4] indicates FPU state)
 * @note    Called from PendSV assembly after STMDB (and optional VSTMDB)
 */
void Os_Port_Stm32L5_PendSvSaveContext(uintptr_t SavedPsp, uint32 ExcReturn)
{
    TaskType current = os_port_m33_state.CurrentTask;

    if ((current < OS_MAX_TASKS) &&
        (os_port_m33_task_context[current].Prepared == TRUE)) {
        os_port_m33_task_context[current].SavedPsp = SavedPsp;
        /* Bit[4] = 0 means FPU frame was stacked */
        os_port_m33_task_context[current].FpuActive =
            (boolean)((ExcReturn & OS_PORT_M33_EXC_RETURN_FPU_BIT) == 0u);
    }
}

/**
 * @brief   Get next task's SavedPsp for context restore
 * @return  SavedPsp of next task (or current if no switch), 0 on error
 * @note    Called from PendSV assembly before LDMIA
 */
uintptr_t Os_Port_Stm32L5_PendSvGetNextContext(void)
{
    TaskType next = os_port_m33_state.SelectedNextTask;
    TaskType current = os_port_m33_state.CurrentTask;

    if ((next != INVALID_TASK) && (next < OS_MAX_TASKS) &&
        (os_port_m33_task_context[next].Prepared == TRUE) &&
        (next != current)) {
        os_port_m33_state.LastSavedTask = current;
        os_port_m33_state.LastSavedPsp =
            (current < OS_MAX_TASKS) ? os_port_m33_task_context[current].SavedPsp : (uintptr_t)0u;
        os_port_m33_state.CurrentTask = next;
        os_port_m33_state.SelectedNextTask = INVALID_TASK;
        os_port_m33_state.SelectedNextTaskPsp = (uintptr_t)0u;
        os_port_m33_state.TaskSwitchCount++;
        return os_port_m33_task_context[next].SavedPsp;
    }

    os_port_m33_state.SelectedNextTask = INVALID_TASK;
    os_port_m33_state.SelectedNextTaskPsp = (uintptr_t)0u;

    if ((current < OS_MAX_TASKS) &&
        (os_port_m33_task_context[current].Prepared == TRUE)) {
        return os_port_m33_task_context[current].SavedPsp;
    }

    return (uintptr_t)0u;
}

/**
 * @brief   Get EXC_RETURN for next task (encodes FPU state)
 * @return  EXC_RETURN with bit[4] reflecting next task's FPU state
 * @note    Called from PendSV assembly to set lr before exception return
 */
uint32 Os_Port_Stm32L5_PendSvGetNextExcReturn(void)
{
    TaskType current = os_port_m33_state.CurrentTask;

    if ((current < OS_MAX_TASKS) &&
        (os_port_m33_task_context[current].Prepared == TRUE) &&
        (os_port_m33_task_context[current].FpuActive != FALSE)) {
        /* Clear bit[4] to indicate FPU frame present */
        return OS_PORT_M33_INITIAL_EXC_RETURN & ~OS_PORT_M33_EXC_RETURN_FPU_BIT;
    }

    return OS_PORT_M33_INITIAL_EXC_RETURN;
}

static void os_port_m33_reset_state(void)
{
    os_port_m33_reset_task_contexts();
    os_port_m33_state.TargetInitialized = FALSE;
    os_port_m33_state.SysTickConfigured = FALSE;
    os_port_m33_state.PendSvPending = FALSE;
    os_port_m33_state.FirstTaskPrepared = FALSE;
    os_port_m33_state.FirstTaskStarted = FALSE;
    os_port_m33_state.DeferredPendSv = FALSE;
    os_port_m33_state.Isr2Nesting = 0u;
    os_port_m33_state.PendSvPriority = OS_PORT_M33_PENDSV_LOWEST_PRIORITY;
    os_port_m33_state.SysTickPriority = OS_PORT_M33_SYSTICK_BOOTSTRAP_PRIO;
    os_port_m33_state.TickInterruptCount = 0u;
    os_port_m33_state.PendSvRequestCount = 0u;
    os_port_m33_state.FirstTaskLaunchCount = 0u;
    os_port_m33_state.PendSvCompleteCount = 0u;
    os_port_m33_state.TaskSwitchCount = 0u;
    os_port_m33_state.KernelDispatchObserveCount = 0u;
    os_port_m33_state.FirstTaskTaskID = INVALID_TASK;
    os_port_m33_state.CurrentTask = INVALID_TASK;
    os_port_m33_state.LastSavedTask = INVALID_TASK;
    os_port_m33_state.LastObservedKernelTask = INVALID_TASK;
    os_port_m33_state.SelectedNextTask = INVALID_TASK;
    os_port_m33_state.FirstTaskEntryAddress = (uintptr_t)0u;
    os_port_m33_state.FirstTaskStackTop = (uintptr_t)0u;
    os_port_m33_state.FirstTaskPsp = (uintptr_t)0u;
    os_port_m33_state.LastSavedPsp = (uintptr_t)0u;
    os_port_m33_state.SelectedNextTaskPsp = (uintptr_t)0u;
    os_port_m33_state.ActivePsp = (uintptr_t)0u;
    os_port_m33_state.InitialXpsr = OS_PORT_M33_XPSR_THUMB;
}

const Os_Port_Stm32L5_StateType* Os_Port_Stm32L5_GetBootstrapState(void)
{
    return &os_port_m33_state;
}

void Os_PortTargetInit(void)
{
    os_port_m33_reset_state();
    os_port_m33_state.TargetInitialized = TRUE;
    os_port_m33_state.SysTickConfigured = TRUE;
}

StatusType Os_Port_Stm32L5_PrepareTaskContext(
    TaskType TaskID,
    Os_TaskEntryType Entry,
    uintptr_t StackTop,
    uintptr_t StackLimit)
{
    uintptr_t prepared_psp;

    if (os_port_m33_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((os_port_m33_is_valid_task(TaskID) == FALSE) || (Entry == (Os_TaskEntryType)0) ||
        (StackTop < (uintptr_t)OS_PORT_M33_INITIAL_FRAME_BYTES)) {
        return E_OS_VALUE;
    }

    if (StackLimit >= StackTop) {
        return E_OS_VALUE;
    }

    prepared_psp =
        os_port_m33_align_down(StackTop - (uintptr_t)OS_PORT_M33_INITIAL_FRAME_BYTES, (uintptr_t)8u);
    if (prepared_psp < StackLimit) {
        return E_OS_VALUE;
    }

    os_port_m33_task_context[TaskID].Prepared = TRUE;
    os_port_m33_task_context[TaskID].TaskID = TaskID;
    os_port_m33_task_context[TaskID].StackTop = StackTop;
    os_port_m33_task_context[TaskID].StackLimit = StackLimit;
    os_port_m33_task_context[TaskID].SavedPsp = prepared_psp;
    os_port_m33_task_context[TaskID].RestorePsp =
        os_port_m33_get_restore_psp(prepared_psp, FALSE);
    os_port_m33_task_context[TaskID].Entry = Entry;
    os_port_m33_task_context[TaskID].FpuActive = FALSE;
    os_port_m33_build_initial_frame(prepared_psp, Entry);
    return E_OK;
}

StatusType Os_Port_Stm32L5_PrepareFirstTask(
    TaskType TaskID,
    Os_TaskEntryType Entry,
    uintptr_t StackTop,
    uintptr_t StackLimit)
{
    StatusType status = Os_Port_Stm32L5_PrepareTaskContext(TaskID, Entry, StackTop, StackLimit);

    if (status != E_OK) {
        return status;
    }

    os_port_m33_state.FirstTaskPrepared = TRUE;
    os_port_m33_state.FirstTaskStarted = FALSE;
    os_port_m33_state.PendSvPending = FALSE;
    os_port_m33_state.DeferredPendSv = FALSE;
    os_port_m33_state.ActivePsp = (uintptr_t)0u;
    os_port_m33_state.FirstTaskTaskID = TaskID;
    os_port_m33_state.FirstTaskEntryAddress = (uintptr_t)Entry;
    os_port_m33_state.FirstTaskStackTop = StackTop;
    os_port_m33_state.FirstTaskPsp = os_port_m33_task_context[TaskID].SavedPsp;
    os_port_m33_state.InitialXpsr = OS_PORT_M33_XPSR_THUMB;
    return E_OK;
}

StatusType Os_Port_Stm32L5_SelectNextTask(TaskType TaskID)
{
    if (os_port_m33_state.TargetInitialized == FALSE) {
        return E_OS_STATE;
    }

    if ((os_port_m33_is_valid_task(TaskID) == FALSE) ||
        (os_port_m33_task_context[TaskID].Prepared == FALSE)) {
        return E_OS_VALUE;
    }

    os_port_m33_state.SelectedNextTask = TaskID;
    os_port_m33_state.SelectedNextTaskPsp = os_port_m33_task_context[TaskID].SavedPsp;
    return E_OK;
}

void Os_Port_Stm32L5_SynchronizeCurrentTask(TaskType TaskID)
{
    if ((os_port_m33_state.TargetInitialized == FALSE) ||
        (os_port_m33_state.FirstTaskStarted == FALSE) ||
        (os_port_m33_is_valid_task(TaskID) == FALSE) ||
        (os_port_m33_task_context[TaskID].Prepared == FALSE)) {
        return;
    }

    os_port_m33_state.CurrentTask = TaskID;
    os_port_m33_state.SelectedNextTask = INVALID_TASK;
    os_port_m33_state.SelectedNextTaskPsp = (uintptr_t)0u;
}

void Os_Port_Stm32L5_ObserveKernelDispatch(TaskType TaskID)
{
    if ((os_port_m33_state.TargetInitialized == FALSE) ||
        (os_port_m33_is_valid_task(TaskID) == FALSE)) {
        return;
    }

    os_port_m33_state.LastObservedKernelTask = TaskID;
    os_port_m33_state.KernelDispatchObserveCount++;
}

void Os_PortStartFirstTask(void)
{
    if ((os_port_m33_state.TargetInitialized == FALSE) ||
        (os_port_m33_state.FirstTaskPrepared == FALSE) ||
        (os_port_m33_state.FirstTaskStarted == TRUE)) {
        return;
    }

    Os_Port_Stm32L5_StartFirstTaskAsm();
}

void Os_PortRequestContextSwitch(void)
{
    if ((os_port_m33_state.TargetInitialized == FALSE) ||
        (os_port_m33_state.FirstTaskStarted == FALSE)) {
        return;
    }

    if (os_port_m33_state.PendSvPending == TRUE) {
        return;
    }

    if (os_port_m33_state.Isr2Nesting > 0u) {
        os_port_m33_state.DeferredPendSv = TRUE;
        return;
    }

    os_port_m33_state.PendSvPending = TRUE;
    os_port_m33_state.PendSvRequestCount++;
}

void Os_PortEnterIsr2(void)
{
    if (os_port_m33_state.TargetInitialized == FALSE) {
        return;
    }

    os_port_m33_state.Isr2Nesting++;
    Os_BootstrapEnterIsr2();
}

void Os_PortExitIsr2(void)
{
    if (os_port_m33_state.TargetInitialized == FALSE) {
        return;
    }

    Os_BootstrapExitIsr2();

    if (os_port_m33_state.Isr2Nesting > 0u) {
        os_port_m33_state.Isr2Nesting--;
    }

    if ((os_port_m33_state.Isr2Nesting == 0u) &&
        (os_port_m33_state.DeferredPendSv == TRUE) &&
        (os_port_m33_state.FirstTaskStarted == TRUE)) {
        os_port_m33_state.DeferredPendSv = FALSE;
        os_port_m33_state.PendSvPending = TRUE;
        os_port_m33_state.PendSvRequestCount++;
    }
}

void Os_Port_Stm32L5_TickIsr(void)
{
    if (os_port_m33_state.TargetInitialized == FALSE) {
        return;
    }

    os_port_m33_state.TickInterruptCount++;

    if (Os_BootstrapProcessCounterTick() == TRUE) {
        Os_PortRequestContextSwitch();
    }
}

#if defined(UNIT_TEST)
void Os_Port_Stm32L5_StartFirstTaskAsm(void)
{
    uintptr_t restore_psp = os_port_m33_get_restore_psp(
        os_port_m33_state.FirstTaskPsp, FALSE);
    Os_Port_Stm32L5_MarkFirstTaskStarted(restore_psp);
}

void Os_Port_Stm32L5_PendSvHandler(void)
{
    uintptr_t next_psp;

    if (os_port_m33_state.FirstTaskStarted == FALSE) {
        uintptr_t restore_psp = os_port_m33_get_restore_psp(
            os_port_m33_state.FirstTaskPsp, FALSE);
        Os_Port_Stm32L5_MarkFirstTaskStarted(restore_psp);
        return;
    }

    if (os_port_m33_state.PendSvPending == FALSE) {
        return;
    }

    /* Simulate save: store current task's ActivePsp as SavedPsp (no FPU in test) */
    Os_Port_Stm32L5_PendSvSaveContext(
        os_port_m33_state.ActivePsp, OS_PORT_M33_INITIAL_EXC_RETURN);

    next_psp = Os_Port_Stm32L5_PendSvGetNextContext();
    if (next_psp == (uintptr_t)0u) {
        return;
    }

    Os_Port_Stm32L5_MarkPendSvComplete(next_psp);
}

void Os_Port_Stm32L5_SysTickHandler(void)
{
    Os_PortEnterIsr2();
    Os_Port_Stm32L5_TickIsr();
    Os_PortExitIsr2();
}
#endif /* UNIT_TEST */

#endif /* PLATFORM_STM32L5 */
