/**
 * @file    Os_Port_Stm32.c
 * @brief   STM32 Cortex-M4/M33 OSEK bootstrap OS port
 * @date    2026-03-14
 *
 * @details Phase 1 (P1) of OSEK STM32 port — plan-osek-stm32-port.md.
 *          Provides the C-side of the PendSV context switch, NVIC priority
 *          setup, and SysTick tick ISR wiring for all STM32 variants
 *          (G474RE, F413ZH, L552ZE, F4).
 *
 *          Assembly counterpart: Os_Port_Stm32_Asm.S
 *          ThreadX Cortex-M4 port used as structural reference.
 *
 * @standard OSEK/VDX, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
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

/* ==================================================================
 * NVIC / SCB register access (hardware builds only)
 * ================================================================== */

#if !defined(UNIT_TEST)
/* SHPR3: PendSV priority [23:16], SysTick priority [31:24] */
#define OS_PORT_SCB_SHPR3       (*(volatile uint32*)0xE000ED20u)
/* ICSR: Interrupt Control and State Register */
#define OS_PORT_SCB_ICSR        (*(volatile uint32*)0xE000ED04u)
#define OS_PORT_ICSR_PENDSVSET  ((uint32)1u << 28u)
#endif

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

/**
 * @brief Build initial task stack frame in STMDB {r4-r11, lr} order.
 *
 * Layout (17 words, low address first):
 *   [0..7]  R4-R11          (software-saved, zeroed)
 *   [8]     LR(EXC_RETURN)  (0xFFFFFFFD = thread mode, PSP, no FPU)
 *   [9..12] R0-R3           (hardware-saved, zeroed)
 *   [13]    R12             (hardware-saved, zeroed)
 *   [14]    LR              (poisoned 0xFFFFFFFF)
 *   [15]    PC              (task entry address)
 *   [16]    xPSR            (Thumb bit set)
 *
 * This layout matches ARM LDMIA/STMDB {r4-r11, lr} register ordering
 * so the PendSV handler uses a single instruction pair for save/restore.
 */
static void os_port_stm32_build_initial_frame(uintptr_t FrameBase, Os_TaskEntryType Entry)
{
    uint32* frame = (uint32*)FrameBase;
    uint32 index;

    /* R4-R11 (indices 0..7) */
    for (index = 0u; index <= 7u; index++) {
        frame[index] = 0u;
    }

    /* EXC_RETURN at index 8 — matches STMDB {r4-r11, lr} where lr=R14 is highest */
    frame[8] = OS_PORT_STM32_INITIAL_EXC_RETURN;

    /* Hardware auto-saved frame: R0-R3, R12 (zeroed) */
    for (index = 9u; index <= 13u; index++) {
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

#if defined(UNIT_TEST)
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
#endif

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

/**
 * @brief Resolve PendSV context switch target.
 *
 * @param CurrentSavedPsp  PSP of current task after STMDB {r4-r11, lr}.
 *                         This is where the software frame starts.
 * @return SavedPsp of the next task (for LDMIA {r4-r11, lr}), or 0 on error.
 *
 * Called from PendSV assembly handler with interrupts disabled.
 * Saves current task PSP to task context, selects next task, updates state.
 */
uintptr_t Os_Port_Stm32_ResolvePendSvTarget(uintptr_t CurrentSavedPsp)
{
    uintptr_t target_saved_psp;
    TaskType current_task;
    TaskType target_task;

    if (os_port_stm32_state.FirstTaskStarted == FALSE) {
        return (uintptr_t)0u;
    }

    current_task = os_port_stm32_state.CurrentTask;

    /* Save current task's PSP back to its context */
    if (os_port_stm32_is_valid_task(current_task) &&
        (os_port_stm32_task_context[current_task].Prepared == TRUE)) {
        os_port_stm32_task_context[current_task].SavedPsp = CurrentSavedPsp;
        os_port_stm32_task_context[current_task].RestorePsp =
            CurrentSavedPsp + (uintptr_t)OS_PORT_STM32_SOFTWARE_RESTORE_BYTES;
    }

    /* Select target: next task if selected, else stay on current */
    target_saved_psp = CurrentSavedPsp;
    target_task = current_task;

    if (os_port_stm32_state.SelectedNextTask != INVALID_TASK) {
        target_task = os_port_stm32_state.SelectedNextTask;
        target_saved_psp = os_port_stm32_task_context[target_task].SavedPsp;
    }

    /* State bookkeeping */
    os_port_stm32_state.LastSavedPsp = CurrentSavedPsp;
    os_port_stm32_state.LastSavedTask = current_task;
    if (target_saved_psp != CurrentSavedPsp) {
        os_port_stm32_state.TaskSwitchCount++;
    }

    os_port_stm32_state.CurrentTask = target_task;
    os_port_stm32_state.SelectedNextTask = INVALID_TASK;
    os_port_stm32_state.SelectedNextTaskPsp = (uintptr_t)0u;
    os_port_stm32_state.PendSvPending = FALSE;
    os_port_stm32_state.DeferredPendSv = FALSE;
    os_port_stm32_state.PendSvCompleteCount++;
    os_port_stm32_state.ActivePsp =
        target_saved_psp + (uintptr_t)OS_PORT_STM32_SOFTWARE_RESTORE_BYTES;

    return target_saved_psp;
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
    os_port_stm32_reset_state();

#if !defined(UNIT_TEST)
    {
        /* Configure NVIC exception priorities via SHPR3 (0xE000ED20):
         *   [23:16] = PendSV priority  → 0xFF (lowest, tail-chains after all ISRs)
         *   [31:24] = SysTick priority → 0x40 (above PendSV, below fault handlers)
         * Reference: ThreadX tx_initialize_low_level.S lines 128-137 */
        uint32 shpr3 = OS_PORT_SCB_SHPR3;
        shpr3 &= ~((uint32)0xFFFF0000u);
        shpr3 |= ((uint32)OS_PORT_STM32_SYSTICK_BOOTSTRAP_PRIORITY << 24u)
                | ((uint32)OS_PORT_STM32_PENDSV_LOWEST_PRIORITY << 16u);
        OS_PORT_SCB_SHPR3 = shpr3;
    }
#endif

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
    if ((os_port_stm32_state.TargetInitialized == FALSE) ||
        (os_port_stm32_state.FirstTaskPrepared == FALSE) ||
        (os_port_stm32_state.FirstTaskStarted == TRUE)) {
        return;
    }

    Os_Port_Stm32_StartFirstTaskAsm();
}

void Os_PortRequestContextSwitch(void)
{
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

#if !defined(UNIT_TEST)
    /* Pend PendSV — will tail-chain after current ISR at lowest priority */
    OS_PORT_SCB_ICSR = OS_PORT_ICSR_PENDSVSET;
#endif
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
#if !defined(UNIT_TEST)
        OS_PORT_SCB_ICSR = OS_PORT_ICSR_PENDSVSET;
#endif
    }
}

boolean Os_PortIsInIsrContext(void)
{
    return (boolean)(os_port_stm32_state.Isr2Nesting > 0u);
}

void Os_Port_Stm32_TickIsr(void)
{
    if (os_port_stm32_state.TargetInitialized == FALSE) {
        return;
    }

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
    uintptr_t current_saved_psp;

    if ((os_port_stm32_state.FirstTaskStarted == FALSE) ||
        (os_port_stm32_state.PendSvPending == FALSE)) {
        return;
    }

    /* Simulate assembly: ActivePsp - SOFTWARE_RESTORE_BYTES = SavedPsp */
    current_saved_psp = os_port_stm32_get_saved_psp(os_port_stm32_state.ActivePsp);

    /* ResolvePendSvTarget now handles all state management internally */
    (void)Os_Port_Stm32_ResolvePendSvTarget(current_saved_psp);
}

void Os_Port_Stm32_SysTickHandler(void)
{
    Os_PortEnterIsr2();
    Os_Port_Stm32_TickIsr();
    Os_PortExitIsr2();
}
#endif

#endif
