/**
 * @file    Os_Port_Stm32_Bringup.c
 * @brief   Minimal hardware bring-up tests for the STM32 OSEK bootstrap port
 * @date    2026-03-14
 *
 * @details Each test proves one hardware primitive in isolation, reports
 *          pass/fail over USART2 (PA2 TX, Nucleo VCP, 115200 baud).
 *
 *          Build with -DOS_BOOTSTRAP_BRINGUP to include these tests.
 *          Call Os_Port_Stm32_BringupAll() from main.c after
 *          Main_Hw_SystemClockInit() but before BSW initialization.
 *
 *          Cortex-M4 architecture makes these tests simpler than TMS570:
 *          - PendSV is a true exception (tail-chains after higher-priority ISRs)
 *          - SysTick is a core timer (no VIM/RTI register setup)
 *          - PSP/MSP hardware split eliminates manual stack switching
 *          - NVIC handles nesting automatically
 *
 *          Tests use the REAL Os_Port_Stm32_Asm.S PendSV handler and
 *          Os_Port_Stm32.c state management — not fake cooperative switches.
 *
 * @note    Safety level: ASIL D bring-up only — not for production
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#ifdef OS_BOOTSTRAP_BRINGUP
#ifdef PLATFORM_STM32

#include "Std_Types.h"
#include "Os.h"
#include "Os_Port_Stm32.h"
#include "Os_Port_TaskBinding.h"

/* UART debug output from cvc_hw_stm32.c (initialized by Main_Hw_SystemClockInit) */
extern void Dbg_Uart_Print(const char *str);

/* HAL tick for delay measurement */
extern uint32 HAL_GetTick(void);

/* ==================================================================
 * Shared helpers
 * ================================================================== */

#define BRINGUP_TASK_A_ID      ((TaskType)0u)
#define BRINGUP_TASK_B_ID      ((TaskType)1u)
#define BRINGUP_ALARM_ID       ((AlarmType)0u)
#define BRINGUP_STACK_SIZE     512u

static uint8 bringup_task_a_stack[BRINGUP_STACK_SIZE] __attribute__((aligned(8)));
static uint8 bringup_task_b_stack[BRINGUP_STACK_SIZE] __attribute__((aligned(8)));
/* Tests 5-6 reuse the original first-task stack (bringup_task_a_stack) for
 * execution, so we need separate stacks for the prepared task contexts to
 * avoid overwriting the live call frames. */
static uint8 bringup_isr_task_a_stack[BRINGUP_STACK_SIZE] __attribute__((aligned(8)));
static uint8 bringup_isr_task_b_stack[BRINGUP_STACK_SIZE] __attribute__((aligned(8)));

static volatile uint32 bringup_task_a_runs = 0u;
static volatile uint32 bringup_task_b_runs = 0u;
static volatile uint32 bringup_pass_count = 0u;
static volatile uint32 bringup_fail_count = 0u;

/** @brief  Print 8-digit hex value to UART */
void bringup_put_hex(uint32 val)
{
    char buf[11];
    uint32 i;
    uint32 nibble;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0u; i < 8u; i++) {
        nibble = (val >> (28u - (i * 4u))) & 0xFu;
        buf[i + 2u] = (nibble < 10u) ? (char)('0' + nibble)
                                      : (char)('A' + nibble - 10u);
    }
    buf[10] = '\0';
    Dbg_Uart_Print(buf);
}

/** @brief  Print decimal uint32 to UART */
static void bringup_put_uint(uint32 val)
{
    char buf[11];
    char *p = &buf[10];
    *p = '\0';
    if (val == 0u) {
        p--;
        *p = '0';
    } else {
        while (val > 0u) {
            p--;
            *p = (char)('0' + (char)(val % 10u));
            val /= 10u;
        }
    }
    Dbg_Uart_Print(p);
}

/** @brief  Busy-wait delay in milliseconds using HAL_GetTick */
static void bringup_delay_ms(uint32 ms)
{
    uint32 start = HAL_GetTick();
    while ((HAL_GetTick() - start) < ms) {
        /* spin */
    }
}

static void bringup_report(const char *tag, boolean pass)
{
    Dbg_Uart_Print(tag);
    if (pass) {
        Dbg_Uart_Print(" PASS\r\n");
        bringup_pass_count++;
    } else {
        Dbg_Uart_Print(" FAIL\r\n");
        bringup_fail_count++;
    }
}

/* ==================================================================
 * Test 1: SysTick fires — NVIC routing, tick counter increments
 *
 * Preconditions:
 *   - Main_Hw_SystemClockInit() called (SysTick running at 1ms via HAL)
 *   - Os_PortTargetInit() called (NVIC priorities set)
 *   - SysTick_Handler calls Os_Port_Stm32_TickIsr()
 *
 * Verifies:
 *   - SysTick ISR fires and increments TickInterruptCount
 *   - Os_Port_Stm32_TickIsr() is reachable from SysTick_Handler
 *   - NVIC routing from SysTick exception to our handler works
 * ================================================================== */

static void bringup_test_systick_fires(void)
{
    const Os_Port_Stm32_StateType* state;
    uint32 tickBefore;
    uint32 tickAfter;

    Dbg_Uart_Print("[BRINGUP-1] SysTick fires...\r\n");

    state = Os_Port_Stm32_GetBootstrapState();
    tickBefore = state->TickInterruptCount;

    /* Wait 200ms — expect ~200 ticks at 1ms SysTick */
    bringup_delay_ms(200u);

    state = Os_Port_Stm32_GetBootstrapState();
    tickAfter = state->TickInterruptCount;

    Dbg_Uart_Print("[BRINGUP-1] Ticks: ");
    bringup_put_uint(tickAfter - tickBefore);
    Dbg_Uart_Print(" (expect ~200)\r\n");

    bringup_report("[BRINGUP-1]", (boolean)((tickAfter - tickBefore) > 100u));
}

/* ==================================================================
 * Test 2: First-task launch — PSP switch, initial frame pop, entry
 *
 * Uses Os_Port_Stm32_PrepareFirstTask + Os_PortStartFirstTask to
 * launch a task via PendSV exception. The PendSV handler in
 * Os_Port_Stm32_Asm.S loads the initial frame (R4-R11, EXC_RETURN)
 * and MSR PSP before BX LR (exception return into task).
 *
 * This is a ONE-WAY TRIP — the task entry runs tests 3-6 and then
 * enters an LED blink loop. BringupAll() never returns.
 * ================================================================== */

static void bringup_first_task_entry(void);

static void bringup_test_first_task_launch(void)
{
    uintptr_t stackTop;

    Dbg_Uart_Print("[BRINGUP-2] First-task launch via PendSV...\r\n");

    stackTop = (uintptr_t)(&bringup_task_a_stack[BRINGUP_STACK_SIZE]);

    if (Os_Port_Stm32_PrepareFirstTask(
            BRINGUP_TASK_A_ID,
            (Os_TaskEntryType)bringup_first_task_entry,
            stackTop) != E_OK) {
        bringup_report("[BRINGUP-2]", FALSE);
        return;
    }

    Dbg_Uart_Print("[BRINGUP-2] Frame prepared, starting...\r\n");

    /* One-way trip: PendSV fires, loads initial frame, enters task */
    Os_PortStartFirstTask();

    /* Should never reach here — PendSV takes over */
    bringup_report("[BRINGUP-2]", FALSE);
}

/* ==================================================================
 * Test 3: Same-task ISR return — SysTick saves/restores without corruption
 *
 * Inside the launched task, load sentinel values into R4-R11, enable
 * interrupts, busy-wait while SysTick fires, then verify all registers
 * are preserved. On Cortex-M4, SysTick ISR uses MSP (not PSP), so
 * R4-R11 are only affected if the ISR corrupts them via C code.
 * The hardware auto-saves R0-R3, R12, LR, PC, xPSR to PSP.
 * ================================================================== */

static const uint32 bringup_reg_sentinels[8] = {
    0xDEAD0004u, 0xDEAD0005u, 0xDEAD0006u, 0xDEAD0007u,
    0xDEAD0008u, 0xDEAD0009u, 0xDEAD000Au, 0xDEAD000Bu
};

static boolean bringup_test_same_task_isr_return(void)
{
    uint32 after[8];
    uint32 pspBefore;
    uint32 pspAfter;
    uint32 ticksBefore;
    uint32 ticksAfter;
    const Os_Port_Stm32_StateType* state;
    boolean pass;
    uint32 i;

    Dbg_Uart_Print("[BRINGUP-3] Same-task ISR return + register preservation...\r\n");

    state = Os_Port_Stm32_GetBootstrapState();
    ticksBefore = state->TickInterruptCount;

    /* Load sentinels into R4-R11, capture PSP, busy-wait while SysTick
     * fires, then verify R4-R11 and PSP are unchanged.
     *
     * On Cortex-M4:
     *   - SysTick fires as exception, hardware pushes {R0-R3,R12,LR,PC,xPSR} to PSP
     *   - ISR runs on MSP with its own stack frame
     *   - Exception return restores hardware frame from PSP
     *   - R4-R11 are callee-saved — ISR C code won't corrupt them
     *     (unless the compiler is broken) */
    __asm__ volatile(
        "MRS    %[pspb], PSP        \n\t"
        "LDR    r4, [%[sen], #0]    \n\t"
        "LDR    r5, [%[sen], #4]    \n\t"
        "LDR    r6, [%[sen], #8]    \n\t"
        "LDR    r7, [%[sen], #12]   \n\t"
        "LDR    r8, [%[sen], #16]   \n\t"
        "LDR    r9, [%[sen], #20]   \n\t"
        "LDR    r10, [%[sen], #24]  \n\t"
        "LDR    r11, [%[sen], #28]  \n\t"
        /* Interrupts are already enabled (task runs with PRIMASK=0) */
        /* Busy-wait ~200ms at 170 MHz (each iteration ≈ 3 cycles) */
        "MOV    r0, %[cnt]          \n\t"
        "1:                         \n\t"
        "SUBS   r0, r0, #1          \n\t"
        "BNE    1b                  \n\t"
        /* Read back R4-R11 */
        "STR    r4, [%[aft], #0]    \n\t"
        "STR    r5, [%[aft], #4]    \n\t"
        "STR    r6, [%[aft], #8]    \n\t"
        "STR    r7, [%[aft], #12]   \n\t"
        "STR    r8, [%[aft], #16]   \n\t"
        "STR    r9, [%[aft], #20]   \n\t"
        "STR    r10, [%[aft], #24]  \n\t"
        "STR    r11, [%[aft], #28]  \n\t"
        "MRS    %[pspa], PSP        \n\t"
        : [pspb] "=&r" (pspBefore), [pspa] "=&r" (pspAfter)
        : [sen] "r" (bringup_reg_sentinels), [aft] "r" (after),
          [cnt] "r" ((uint32)11333333u)  /* ~200ms at 170 MHz / 3 cycles per iter */
        : "r0", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11",
          "memory"
    );

    state = Os_Port_Stm32_GetBootstrapState();
    ticksAfter = state->TickInterruptCount;

    /* Verify register preservation */
    pass = TRUE;
    for (i = 0u; i < 8u; i++) {
        if (bringup_reg_sentinels[i] != after[i]) {
            pass = FALSE;
            Dbg_Uart_Print("[BRINGUP-3] R");
            bringup_put_uint(i + 4u);
            Dbg_Uart_Print(" MISMATCH: ");
            bringup_put_hex(bringup_reg_sentinels[i]);
            Dbg_Uart_Print(" -> ");
            bringup_put_hex(after[i]);
            Dbg_Uart_Print("\r\n");
        }
    }

    /* Verify PSP preservation */
    if (pspBefore != pspAfter) {
        pass = FALSE;
        Dbg_Uart_Print("[BRINGUP-3] PSP MISMATCH: ");
        bringup_put_hex(pspBefore);
        Dbg_Uart_Print(" -> ");
        bringup_put_hex(pspAfter);
        Dbg_Uart_Print("\r\n");
    }

    Dbg_Uart_Print("[BRINGUP-3] SysTick ISRs during test: ");
    bringup_put_uint(ticksAfter - ticksBefore);
    Dbg_Uart_Print("\r\n");

    if ((ticksAfter - ticksBefore) == 0u) {
        Dbg_Uart_Print("[BRINGUP-3] FAIL - no SysTick ISRs fired\r\n");
        return FALSE;
    }

    bringup_report("[BRINGUP-3]", pass);
    return pass;
}

/* ==================================================================
 * Test 4: Two-task PendSV context switch
 *
 * Prepares a second task (Task B), triggers PendSV to switch to it,
 * Task B runs and triggers PendSV to switch back to Task A.
 * Verifies both tasks ran and Task A resumes correctly.
 *
 * Uses the REAL PendSV handler from Os_Port_Stm32_Asm.S:
 *   1. Task A: SelectNextTask(B) + RequestContextSwitch() → PENDSVSET
 *   2. PendSV: STMDB {r4-r11,lr} for A, LDMIA {r4-r11,lr} for B
 *   3. Task B runs, sets flag, SelectNextTask(A) + RequestContextSwitch()
 *   4. PendSV: saves B, restores A
 *   5. Task A resumes here
 * ================================================================== */

static volatile boolean bringup_task_b_switch_back = FALSE;

static void bringup_task_b_two_task_entry(void)
{
    Dbg_Uart_Print("[BRINGUP-4] Task B reached!\r\n");
    bringup_task_b_runs++;

    /* Switch back to Task A */
    (void)Os_Port_Stm32_SelectNextTask(BRINGUP_TASK_A_ID);
    bringup_task_b_switch_back = TRUE;
    Os_PortRequestContextSwitch();

    /* PendSV fires after we return to thread mode — but since
     * TerminateTask isn't available in bring-up, we spin.
     * PendSV will preempt this spin and switch to Task A. */
    for (;;) {
        __asm__ volatile("WFI");
    }
}

static boolean bringup_test_two_task_switch(void)
{
    uintptr_t stackTop;
    const Os_Port_Stm32_StateType* state;
    uint32 switchesBefore;
    volatile uint32 timeout;

    Dbg_Uart_Print("[BRINGUP-4] Two-task PendSV switch...\r\n");

    state = Os_Port_Stm32_GetBootstrapState();
    switchesBefore = state->TaskSwitchCount;

    /* Prepare Task B */
    stackTop = (uintptr_t)(&bringup_task_b_stack[BRINGUP_STACK_SIZE]);
    if (Os_Port_Stm32_PrepareTaskContext(
            BRINGUP_TASK_B_ID,
            (Os_TaskEntryType)bringup_task_b_two_task_entry,
            stackTop) != E_OK) {
        bringup_report("[BRINGUP-4]", FALSE);
        return FALSE;
    }

    bringup_task_b_runs = 0u;
    bringup_task_b_switch_back = FALSE;

    Dbg_Uart_Print("[BRINGUP-4] Task A: switching to Task B...\r\n");

    /* Trigger switch to Task B */
    (void)Os_Port_Stm32_SelectNextTask(BRINGUP_TASK_B_ID);
    Os_PortRequestContextSwitch();

    /* PendSV fires here (lowest priority, tail-chains immediately in
     * thread mode). When Task B switches back, we resume here. */

    /* Wait for Task B to signal it ran — PendSV should have already
     * completed by the time we reach this point, but add timeout. */
    timeout = 0u;
    while ((bringup_task_b_switch_back == FALSE) && (timeout < 1000000u)) {
        timeout++;
    }

    Dbg_Uart_Print("[BRINGUP-4] Task A: resumed!\r\n");

    state = Os_Port_Stm32_GetBootstrapState();

    Dbg_Uart_Print("[BRINGUP-4] Task B runs: ");
    bringup_put_uint(bringup_task_b_runs);
    Dbg_Uart_Print(", switches: ");
    bringup_put_uint(state->TaskSwitchCount - switchesBefore);
    Dbg_Uart_Print("\r\n");

    bringup_report("[BRINGUP-4]",
                   (boolean)((bringup_task_b_runs > 0u) &&
                             ((state->TaskSwitchCount - switchesBefore) >= 2u)));
    return (boolean)(bringup_task_b_runs > 0u);
}

/* ==================================================================
 * Test 5: ISR preemption — SysTick during task → PendSV at tail-chain
 *
 * Uses the OSEK alarm mechanism: SetRelAlarm activates Task B after
 * N ticks. SysTick fires → TickIsr → ProcessCounterTick → alarm
 * expiry → ActivateTask(B) → RequestContextSwitch → PENDSVSET.
 * PendSV tail-chains after SysTick returns → saves Task A → restores
 * Task B. Verifies the full ISR → PendSV preemption path.
 * ================================================================== */

static volatile boolean bringup_task_b_preempt_ran = FALSE;

static void bringup_task_b_preempt_entry(void)
{
    const Os_Port_Stm32_StateType* st;
    StatusType sel_status;

    Dbg_Uart_Print("[BRINGUP-5] Task B preempted in!\r\n");
    bringup_task_b_preempt_ran = TRUE;

    /* Switch back to Task A */
    sel_status = Os_Port_Stm32_SelectNextTask(BRINGUP_TASK_A_ID);
    Dbg_Uart_Print("[BRINGUP-5] SelectNext=");
    bringup_put_uint((uint32)sel_status);

    st = Os_Port_Stm32_GetBootstrapState();
    Dbg_Uart_Print(" PendSvPending=");
    bringup_put_uint((uint32)st->PendSvPending);
    Dbg_Uart_Print(" Nesting=");
    bringup_put_uint((uint32)st->Isr2Nesting);
    Dbg_Uart_Print(" SelNext=");
    bringup_put_uint((uint32)st->SelectedNextTask);
    Dbg_Uart_Print("\r\n");

    Dbg_Uart_Print("[BRINGUP-5] Requesting ctx switch...\r\n");
    Os_PortRequestContextSwitch();

    st = Os_Port_Stm32_GetBootstrapState();
    Dbg_Uart_Print("[BRINGUP-5] After ReqCtxSw: PendSvPending=");
    bringup_put_uint((uint32)st->PendSvPending);
    Dbg_Uart_Print(" ReqCount=");
    bringup_put_uint(st->PendSvRequestCount);
    Dbg_Uart_Print("\r\n");

    for (;;) {
        __asm__ volatile("WFI");
    }
}

static boolean bringup_test_isr_preemption(void)
{
    uintptr_t stackTop;
    const Os_Port_Stm32_StateType* state;
    volatile uint32 timeout;
    const Os_TaskConfigType task_cfg[] = {
        { "TaskA", (Os_TaskEntryType)bringup_first_task_entry,       2u, 1u, 0u, FALSE, FULL },
        { "TaskB", (Os_TaskEntryType)bringup_task_b_preempt_entry,   1u, 1u, 0u, FALSE, FULL },
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmB", BRINGUP_TASK_B_ID, 0xFFFFu, 1u, 1u }
    };

    Dbg_Uart_Print("[BRINGUP-5] ISR preemption via SysTick alarm...\r\n");

    /* Configure OSEK kernel tasks and alarms */
    Os_TestReset();
    if (Os_TestConfigureTasks(task_cfg, 2u) != E_OK) {
        Dbg_Uart_Print("[BRINGUP-5] Task config failed\r\n");
        bringup_report("[BRINGUP-5]", FALSE);
        return FALSE;
    }
    if (Os_TestConfigureAlarms(alarm_cfg, 1u) != E_OK) {
        Dbg_Uart_Print("[BRINGUP-5] Alarm config failed\r\n");
        bringup_report("[BRINGUP-5]", FALSE);
        return FALSE;
    }

    /* Re-init port and prepare both tasks */
    Os_PortTargetInit();
    /* Use separate stacks — we're still executing on bringup_task_a_stack
     * so PrepareConfiguredFirstTask must NOT write its frame there. */
    stackTop = (uintptr_t)(&bringup_isr_task_a_stack[BRINGUP_STACK_SIZE]);
    if (Os_Port_PrepareConfiguredFirstTask(BRINGUP_TASK_A_ID, stackTop) != E_OK) {
        bringup_report("[BRINGUP-5]", FALSE);
        return FALSE;
    }
    stackTop = (uintptr_t)(&bringup_isr_task_b_stack[BRINGUP_STACK_SIZE]);
    if (Os_Port_PrepareConfiguredTask(BRINGUP_TASK_B_ID, stackTop) != E_OK) {
        bringup_report("[BRINGUP-5]", FALSE);
        return FALSE;
    }

    /* Start kernel and set alarm: activate Task B after 10 ticks (10ms) */
    StartOS(OSDEFAULTAPPMODE);
    bringup_task_b_preempt_ran = FALSE;

    /* We're already running in Task A's thread context (from test 2's
     * Os_PortStartFirstTask).  PortTargetInit reset FirstTaskStarted,
     * so tell the port we're live — otherwise PortRequestContextSwitch
     * will silently return and PendSV will never fire. */
    {
        uintptr_t currentPsp;
        __asm__ volatile("MRS %0, PSP" : "=r"(currentPsp));
        Os_Port_Stm32_MarkFirstTaskStarted(currentPsp);
    }

    /* Register TaskA as RUNNING with the kernel so that
     * os_maybe_dispatch_preemption can preempt it when the alarm
     * activates higher-priority TaskB. */
    (void)ActivateTask(BRINGUP_TASK_A_ID);
    (void)Os_TestSetCurrentTaskRunning(BRINGUP_TASK_A_ID);

    if (SetRelAlarm(BRINGUP_ALARM_ID, 10u, 0u) != E_OK) {
        Dbg_Uart_Print("[BRINGUP-5] SetRelAlarm failed\r\n");
        bringup_report("[BRINGUP-5]", FALSE);
        return FALSE;
    }

    Dbg_Uart_Print("[BRINGUP-5] Alarm set, waiting for preemption...\r\n");

    /* Spin-wait — SysTick fires every 1ms, alarm expires after 10 ticks,
     * PendSV preempts this task, switches to Task B, which switches back. */
    timeout = 0u;
    while ((bringup_task_b_preempt_ran == FALSE) && (timeout < 5000000u)) {
        timeout++;
    }

    state = Os_Port_Stm32_GetBootstrapState();
    Dbg_Uart_Print("[BRINGUP-5] Preempt ran: ");
    Dbg_Uart_Print(bringup_task_b_preempt_ran ? "YES" : "NO");
    Dbg_Uart_Print(", switches: ");
    bringup_put_uint(state->TaskSwitchCount);
    Dbg_Uart_Print("\r\n");

    bringup_report("[BRINGUP-5]", (boolean)(bringup_task_b_preempt_ran == TRUE));
    return (boolean)(bringup_task_b_preempt_ran == TRUE);
}

/* ==================================================================
 * Test 6: Time-slice round-robin — alarm-driven preemption every N ticks
 *
 * Sets a cyclic alarm that activates Task B every 50ms. Task B
 * increments its counter and yields back. After 500ms, verifies
 * that Task B ran ~10 times (50ms period × 10 = 500ms).
 *
 * Proves the full OSEK alarm → ActivateTask → PendSV → context
 * switch cycle works repeatedly without state corruption.
 * ================================================================== */

static volatile uint32 bringup_timeslice_b_count = 0u;

static void bringup_task_b_timeslice_entry(void)
{
    bringup_timeslice_b_count++;

    /* Terminate via kernel: TaskB → SUSPENDED, TaskA restored from
     * preempted stack as os_current_task.  Then switch port context
     * back to TaskA.  The dispatcher rebuilds our frame automatically
     * (Os_Port_RebuildTaskFrame) before the next PendSV dispatch. */
    (void)TerminateTask();
    (void)Os_Port_Stm32_SelectNextTask(BRINGUP_TASK_A_ID);
    Os_PortRequestContextSwitch();

    for (;;) {
        __asm__ volatile("WFI");
    }
}

static boolean bringup_test_timeslice(void)
{
    uintptr_t stackTop;
    const Os_Port_Stm32_StateType* state;
    uint32 startTick;
    const Os_TaskConfigType task_cfg[] = {
        { "TaskA", (Os_TaskEntryType)bringup_first_task_entry,       2u, 1u, 0u, FALSE, FULL },
        { "TaskB", (Os_TaskEntryType)bringup_task_b_timeslice_entry, 1u, 1u, 0u, FALSE, FULL },
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmB", BRINGUP_TASK_B_ID, 0xFFFFu, 1u, 1u }
    };

    Dbg_Uart_Print("[BRINGUP-6] Time-slice round-robin (50ms cyclic)...\r\n");

    /* Configure OSEK kernel */
    Os_TestReset();
    if (Os_TestConfigureTasks(task_cfg, 2u) != E_OK) {
        bringup_report("[BRINGUP-6]", FALSE);
        return FALSE;
    }
    if (Os_TestConfigureAlarms(alarm_cfg, 1u) != E_OK) {
        bringup_report("[BRINGUP-6]", FALSE);
        return FALSE;
    }

    /* Re-init port and prepare both tasks.
     * Use separate stacks — we're still executing on bringup_task_a_stack. */
    Os_PortTargetInit();
    stackTop = (uintptr_t)(&bringup_isr_task_a_stack[BRINGUP_STACK_SIZE]);
    if (Os_Port_PrepareConfiguredFirstTask(BRINGUP_TASK_A_ID, stackTop) != E_OK) {
        bringup_report("[BRINGUP-6]", FALSE);
        return FALSE;
    }
    stackTop = (uintptr_t)(&bringup_isr_task_b_stack[BRINGUP_STACK_SIZE]);
    if (Os_Port_PrepareConfiguredTask(BRINGUP_TASK_B_ID, stackTop) != E_OK) {
        bringup_report("[BRINGUP-6]", FALSE);
        return FALSE;
    }

    StartOS(OSDEFAULTAPPMODE);
    bringup_timeslice_b_count = 0u;

    /* Mark port as running — we're already in task context from test 2 */
    {
        uintptr_t currentPsp;
        __asm__ volatile("MRS %0, PSP" : "=r"(currentPsp));
        Os_Port_Stm32_MarkFirstTaskStarted(currentPsp);
    }

    /* Register TaskA with kernel so preemption logic works */
    (void)ActivateTask(BRINGUP_TASK_A_ID);
    (void)Os_TestSetCurrentTaskRunning(BRINGUP_TASK_A_ID);

    /* Cyclic alarm: first expiry at 50 ticks, repeat every 50 ticks */
    if (SetRelAlarm(BRINGUP_ALARM_ID, 50u, 50u) != E_OK) {
        Dbg_Uart_Print("[BRINGUP-6] SetRelAlarm failed\r\n");
        bringup_report("[BRINGUP-6]", FALSE);
        return FALSE;
    }

    /* Wait 500ms — expect ~10 preemptions */
    startTick = HAL_GetTick();
    while ((HAL_GetTick() - startTick) < 500u) {
        /* Spin in Task A while alarms fire and preempt */
        __asm__ volatile("NOP");
    }

    /* Cancel cyclic alarm */
    (void)CancelAlarm(BRINGUP_ALARM_ID);

    state = Os_Port_Stm32_GetBootstrapState();
    Dbg_Uart_Print("[BRINGUP-6] Task B preemptions: ");
    bringup_put_uint(bringup_timeslice_b_count);
    Dbg_Uart_Print(", total switches: ");
    bringup_put_uint(state->TaskSwitchCount);
    Dbg_Uart_Print("\r\n");

    /* Expect at least 5 preemptions in 500ms with 50ms period */
    bringup_report("[BRINGUP-6]",
                   (boolean)(bringup_timeslice_b_count >= 5u));
    return (boolean)(bringup_timeslice_b_count >= 5u);
}

/* ==================================================================
 * First-task entry — runs tests 3-6, then LED blink loop
 *
 * Entered via PendSV exception return from Os_Port_Stm32_Asm.S.
 * Runs in thread mode on PSP with interrupts enabled.
 * ================================================================== */

static void bringup_first_task_entry(void)
{
    uint32 control;
    /* Read CONTROL register to verify PSP is active */
    __asm__ volatile("MRS %0, CONTROL" : "=r"(control));

    Dbg_Uart_Print("[BRINGUP-2] First task entry reached!\r\n");
    Dbg_Uart_Print("[BRINGUP-2] CONTROL = ");
    bringup_put_hex(control);
    Dbg_Uart_Print(" (SPSEL=");
    bringup_put_uint((control >> 1u) & 1u);
    Dbg_Uart_Print(")\r\n");

    /* CONTROL.SPSEL (bit 1) = 1 means PSP is active */
    if ((control & 2u) == 0u) {
        Dbg_Uart_Print("[BRINGUP-2] FAIL - not using PSP!\r\n");

    }

    bringup_report("[BRINGUP-2]", (boolean)((control & 2u) != 0u));
    bringup_task_a_runs++;

    /* Test 3: same-task ISR return */
    if (bringup_test_same_task_isr_return() == FALSE) {

    }

    /* Test 4: two-task PendSV switch */
    if (bringup_test_two_task_switch() == FALSE) {

    }

    /* Test 5: ISR preemption via alarm
     * NOTE: Tests 5-6 reinitialize the kernel, so they must come last.
     * After test 5, this task context is no longer valid for the
     * port state machine. We report pass/fail before reinit. */
    if (bringup_test_isr_preemption() == FALSE) {

    }

    /* Test 6: time-slice round-robin */
    if (bringup_test_timeslice() == FALSE) {

    }

    /* Final summary */
    Dbg_Uart_Print("\r\n=== STM32 Bring-up: ");
    bringup_put_uint(bringup_pass_count);
    Dbg_Uart_Print("/");
    bringup_put_uint(bringup_pass_count + bringup_fail_count);
    Dbg_Uart_Print(bringup_fail_count == 0u ? " ALL PASS" : " SOME FAILED");
    Dbg_Uart_Print(" ===\r\n\r\n");

    /* Stay alive: polled LED blink via SysTick (LD2 = PA5 on Nucleo G474RE) */
    {
        uint32 lastBlink = HAL_GetTick();
        volatile uint32 *gpioa_bsrr = (volatile uint32 *)0x48000018u; /* GPIOA BSRR */
        boolean ledOn = FALSE;

        for (;;) {
            if ((HAL_GetTick() - lastBlink) >= 500u) {
                lastBlink = HAL_GetTick();
                if (ledOn) {
                    *gpioa_bsrr = (uint32)(1u << (5u + 16u));  /* PA5 reset (LED off) */
                } else {
                    *gpioa_bsrr = (uint32)(1u << 5u);          /* PA5 set (LED on) */
                }
                ledOn = (boolean)(!ledOn);
            }
        }
    }
}

/* ==================================================================
 * Public entry point
 * ================================================================== */

/**
 * @brief  Run all STM32 OSEK bootstrap bring-up tests
 *
 * Call from main.c after Main_Hw_SystemClockInit() but before BSW init.
 * SysTick must already be running (HAL_Init configures it at 1ms).
 *
 * @note   This function never returns — test 2 launches a task via
 *         PendSV that takes over as the new execution context.
 */
void Os_Port_Stm32_BringupAll(void)
{
    Dbg_Uart_Print("\r\n=== OS Bootstrap Bring-up Tests (STM32 G474RE) ===\r\n");

    bringup_pass_count = 0u;
    bringup_fail_count = 0u;

    /* Initialize OSEK port — NVIC priorities for PendSV and SysTick */
    Os_PortTargetInit();

    /* Test 1: verify SysTick fires and tick counter increments */
    bringup_test_systick_fires();

    /* Test 2: first-task launch via PendSV (one-way trip).
     * Tests 3-6 run inside the launched task. */
    bringup_test_first_task_launch();

    /* Should never reach here */
    Dbg_Uart_Print("=== Bring-up FAILED (test 2 did not launch) ===\r\n");
}

#endif /* PLATFORM_STM32 */
#endif /* OS_BOOTSTRAP_BRINGUP */
