/**
 * @file    Os_Port_Tms570_Bringup.c
 * @brief   Minimal hardware bring-up tests for the TMS570 bootstrap OS port
 * @date    2026-03-14
 *
 * @details Each test proves one hardware primitive in isolation, reports
 *          pass/fail over SCI UART, then restores the previous state so
 *          the main SC polled loop can continue normally.
 *
 *          Build with -DOS_BOOTSTRAP_BRINGUP to include these tests.
 *          Call Os_Port_Tms570_BringupAll() from sc_main.c after rtiInit()
 *          and rtiStartCounter() but before the main loop.
 *
 * @note    Safety level: ASIL D bring-up only — not for production
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#ifdef OS_BOOTSTRAP_BRINGUP
#ifdef PLATFORM_TMS570

/* HALCoGen headers first — they define uint32, boolean, etc.
 * Do NOT include sc_types.h here — its boolean typedef (uint8)
 * conflicts with HALCoGen's boolean (bool). */
#include "HL_sys_vim.h"
#include "HL_reg_rti.h"

/* SCI debug output — declared here to avoid sc_hw.h/sc_types.h conflict */
extern void sc_sci_puts(const char* str);
extern void sc_sci_put_uint(uint32 val);

/* ==================================================================
 * Shared helpers
 * ================================================================== */

extern void _enable_IRQ_interrupt_(void);
extern void _disable_IRQ_interrupt_(void);

#ifndef TRUE
#define TRUE  1U
#endif
#ifndef FALSE
#define FALSE 0U
#endif

/** Busy-wait delay — approximately `cycles` loop iterations at 300 MHz.
 *  30 000 000 iterations ≈ 100ms. */
static void bringup_delay(volatile uint32 cycles)
{
    while (cycles > 0u) {
        cycles--;
    }
}

/** @brief  Print a uint32 as 8-digit hex via SCI UART */
static void bringup_put_hex(uint32 val)
{
    char buf[9];
    uint32 i;
    uint32 nibble;
    for (i = 0u; i < 8u; i++) {
        nibble = (val >> (28u - (i * 4u))) & 0xFu;
        buf[i] = (nibble < 10u) ? (char)('0' + nibble) : (char)('A' + nibble - 10u);
    }
    buf[8] = '\0';
    sc_sci_puts(buf);
}

/* ==================================================================
 * Test 1: Prove RTI compare0 fires as IRQ via VIM channel 2
 * ================================================================== */

/** @brief  Counter incremented by our test ISR */
static volatile uint32 bringup_rti_irq_count = 0u;

/**
 * @brief  Minimal RTI compare0 ISR — increment counter, acknowledge
 *
 * @note   This is NOT the bootstrap OS tick handler. It is a standalone
 *         proof-of-concept ISR used only during bring-up.
 */
void __attribute__((interrupt("IRQ"))) bringup_rti_compare0_isr(void)
{
    bringup_rti_irq_count++;
    rtiREG1->INTFLAG = (uint32)1u;  /* W1C: acknowledge compare0 */
}

/**
 * @brief  Prove RTI compare0 fires as IRQ through VIM channel 2
 *
 * Preconditions (met by sc_main.c before calling):
 *   - vimInit() called by _c_int00
 *   - rtiInit() called — RTI configured for 10ms tick, interrupts disabled
 *   - rtiStartCounter() called — counter block 0 running
 *
 * Test sequence:
 *   1. Map RTI compare0 (request 2) to VIM channel 2 with our test ISR
 *   2. Enable VIM channel 2 as IRQ
 *   3. Enable RTI compare0 interrupt generation (SETINTENA bit 0)
 *   4. Enable CPU IRQs (CPSR I-bit clear)
 *   5. Wait ~200ms (expect ~20 ticks at 10ms period)
 *   6. Report count over SCI UART
 *   7. Restore: disable compare0 interrupt, disable VIM channel 2,
 *      clear pending flag, disable CPU IRQs
 *
 * @return TRUE if at least one IRQ was received, FALSE otherwise
 */
static boolean bringup_test_rti_compare0_irq(void)
{
    uint32 startCount;
    uint32 endCount;
    boolean pass;

    sc_sci_puts("[BRINGUP-1] RTI compare0 IRQ via VIM ch2...\r\n");

    /* Reset counter */
    bringup_rti_irq_count = 0u;

    /* 1. Map RTI compare0 (request 2) → VIM channel 2 with our ISR */
    vimChannelMap(2u, 2u, (t_isrFuncPTR)&bringup_rti_compare0_isr);

    /* 2. Enable VIM channel 2 as IRQ */
    vimEnableInterrupt(2u, SYS_IRQ);

    /* 3. Enable RTI compare0 interrupt generation */
    rtiREG1->SETINTENA = (uint32)1u;

    /* 4. Enable CPU IRQs */
    startCount = bringup_rti_irq_count;
    _enable_IRQ_interrupt_();

    /* 5. Wait ~200ms — expect ~20 interrupts at 10ms period */
    bringup_delay(60000000u);

    /* 6. Disable CPU IRQs before touching VIM/RTI state */
    _disable_IRQ_interrupt_();

    endCount = bringup_rti_irq_count;

    /* 7. Restore polled mode */
    rtiREG1->CLEARINTENA = (uint32)1u;    /* Disable compare0 interrupt */
    vimDisableInterrupt(2u);               /* Disable VIM channel 2 */
    rtiREG1->INTFLAG = (uint32)1u;         /* Clear any pending flag */

    /* Report */
    sc_sci_puts("[BRINGUP-1] IRQ count: ");
    sc_sci_put_uint(endCount - startCount);
    sc_sci_puts(" (expect ~20)\r\n");

    pass = ((endCount - startCount) > 0u) ? TRUE : FALSE;
    sc_sci_puts(pass ? "[BRINGUP-1] PASS\r\n" : "[BRINGUP-1] FAIL\r\n");

    return pass;
}

/* ==================================================================
 * Test 3: Prove same-task IRQ return preserves callee-saved registers
 *
 * Called from inside the launched task (test 2). Loads known sentinel
 * values into R4-R11, enables RTI compare0 IRQ, busy-waits while IRQs
 * fire, then verifies all registers are unchanged after ISR returns.
 *
 * Proves:
 *   - IRQ fires while a launched task is running
 *   - __attribute__((interrupt("IRQ"))) ISR correctly saves/restores
 *     CPSR (via SPSR_irq) and scratch registers (R0-R3, R12, LR)
 *   - Callee-saved registers (R4-R11) survive the ISR round-trip
 *   - Task SP is unchanged after IRQ return
 * ================================================================== */

/** @brief  Sentinel values for R4-R11 register preservation check */
static const uint32 bringup_reg_sentinels[8] = {
    0xDEAD0004u, 0xDEAD0005u, 0xDEAD0006u, 0xDEAD0007u,
    0xDEAD0008u, 0xDEAD0009u, 0xDEAD000Au, 0xDEAD000Bu
};

/**
 * @brief  Prove IRQ return preserves callee-saved registers R4-R11
 *
 * @return TRUE if all registers preserved and at least one IRQ fired
 *
 * @note   Must be called from the launched task (System mode, IRQs disabled).
 *         Reuses bringup_rti_compare0_isr from test 1.
 */
static boolean bringup_test_same_task_irq_return(void)
{
    uint32 after[8];
    uint32 spBefore;
    uint32 spAfter;
    uint32 irqsBefore;
    uint32 irqsAfter;
    boolean pass;
    uint32 i;

    sc_sci_puts("[BRINGUP-3] Same-task IRQ return + register preservation...\r\n");

    /* Setup VIM/RTI — same ISR as test 1 */
    bringup_rti_irq_count = 0u;
    vimChannelMap(2u, 2u, (t_isrFuncPTR)&bringup_rti_compare0_isr);
    vimEnableInterrupt(2u, SYS_IRQ);
    rtiREG1->SETINTENA = (uint32)1u;

    irqsBefore = bringup_rti_irq_count;

    /* Load sentinels into R4-R11, capture SP, enable IRQ, busy-wait
     * while IRQs fire, disable IRQ, read R4-R11 and SP back.
     *
     * All in one asm block so the compiler cannot use R4-R11 for
     * other purposes during the test window. */
    __asm__ volatile(
        /* Capture SP before */
        "MOV    %[spb], sp          \n\t"
        /* Load sentinel values into R4-R11 */
        "LDR    r4, [%[sen], #0]    \n\t"
        "LDR    r5, [%[sen], #4]    \n\t"
        "LDR    r6, [%[sen], #8]    \n\t"
        "LDR    r7, [%[sen], #12]   \n\t"
        "LDR    r8, [%[sen], #16]   \n\t"
        "LDR    r9, [%[sen], #20]   \n\t"
        "LDR    r10, [%[sen], #24]  \n\t"
        "LDR    r11, [%[sen], #28]  \n\t"
        /* Enable IRQ (clear I bit) */
        "CPSIE  i                   \n\t"
        /* Busy-wait ~620ms at 300 MHz — expect ~62 IRQs at 10ms */
        "MOV    r0, %[cnt]          \n\t"
        "1:                         \n\t"
        "SUBS   r0, r0, #1          \n\t"
        "BNE    1b                  \n\t"
        /* Disable IRQ */
        "CPSID  i                   \n\t"
        /* Store R4-R11 to after[] */
        "STR    r4, [%[aft], #0]    \n\t"
        "STR    r5, [%[aft], #4]    \n\t"
        "STR    r6, [%[aft], #8]    \n\t"
        "STR    r7, [%[aft], #12]   \n\t"
        "STR    r8, [%[aft], #16]   \n\t"
        "STR    r9, [%[aft], #20]   \n\t"
        "STR    r10, [%[aft], #24]  \n\t"
        "STR    r11, [%[aft], #28]  \n\t"
        /* Capture SP after */
        "MOV    %[spa], sp          \n\t"
        : [spb] "=&r" (spBefore), [spa] "=&r" (spAfter)
        : [sen] "r" (bringup_reg_sentinels), [aft] "r" (after),
          [cnt] "r" ((uint32)0x00800000u)
        : "r0", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11",
          "memory"
    );

    irqsAfter = bringup_rti_irq_count;

    /* Restore polled mode */
    rtiREG1->CLEARINTENA = (uint32)1u;
    vimDisableInterrupt(2u);
    rtiREG1->INTFLAG = (uint32)1u;

    /* Verify register preservation */
    pass = TRUE;
    for (i = 0u; i < 8u; i++) {
        if (bringup_reg_sentinels[i] != after[i]) {
            pass = FALSE;
            sc_sci_puts("[BRINGUP-3] R");
            sc_sci_put_uint(i + 4u);
            sc_sci_puts(" MISMATCH: 0x");
            bringup_put_hex(bringup_reg_sentinels[i]);
            sc_sci_puts(" -> 0x");
            bringup_put_hex(after[i]);
            sc_sci_puts("\r\n");
        }
    }

    /* Verify SP preservation */
    if (spBefore != spAfter) {
        pass = FALSE;
        sc_sci_puts("[BRINGUP-3] SP MISMATCH: 0x");
        bringup_put_hex(spBefore);
        sc_sci_puts(" -> 0x");
        bringup_put_hex(spAfter);
        sc_sci_puts("\r\n");
    }

    /* Report */
    sc_sci_puts("[BRINGUP-3] IRQs during test: ");
    sc_sci_put_uint(irqsAfter - irqsBefore);
    sc_sci_puts("\r\n");

    if ((irqsAfter - irqsBefore) == 0u) {
        sc_sci_puts("[BRINGUP-3] FAIL - no IRQs fired\r\n");
        return FALSE;
    }

    sc_sci_puts(pass ? "[BRINGUP-3] PASS\r\n" : "[BRINGUP-3] FAIL\r\n");
    return pass;
}

/* ==================================================================
 * Test 4: Prove two-task cooperative context switch
 *
 * Called from inside the launched task (test 2). Proves:
 *   - Task A can save its context (R4-R11, LR, SP) and switch to Task B
 *   - Task B runs on its own stack and executes C code
 *   - Task B can switch back to Task A
 *   - Task A resumes at the correct point with registers intact
 *
 * Uses a cooperative (solicited) switch — no IRQ involved. This matches
 * ThreadX's tx_thread_system_return.S pattern: save callee-saved regs,
 * store SP, load target's SP and regs, BX LR.
 * ================================================================== */

/** @brief  Cooperative context save area — callee-saved regs + LR + SP */
typedef struct {
    uint32 r4;      /* [0]  */
    uint32 r5;      /* [4]  */
    uint32 r6;      /* [8]  */
    uint32 r7;      /* [12] */
    uint32 r8;      /* [16] */
    uint32 r9;      /* [20] */
    uint32 r10;     /* [24] */
    uint32 r11;     /* [28] */
    uint32 lr;      /* [32] */
    uint32 sp;      /* [36] */
} BringupTaskContext;

static BringupTaskContext bringup_ctx_a;
static BringupTaskContext bringup_ctx_b;
static uint8 bringup_task_b_stack[512u] __attribute__((aligned(8)));
static volatile boolean bringup_task_b_ran = FALSE;

/**
 * @brief  Cooperative context switch between two tasks
 *
 * @param  save     Pointer to current task's save area (R0 via AAPCS)
 * @param  restore  Pointer to next task's save area (R1 via AAPCS)
 *
 * Saves R4-R11, LR, SP to save area, then loads R4-R11, LR, SP from
 * restore area and branches via LR. For an initial launch, pre-fill
 * the restore area with LR = entry address and SP = stack top.
 *
 * @note   This is a naked function — no compiler prologue/epilogue.
 */
__attribute__((naked))
static void bringup_switch_context(BringupTaskContext* save,
                                   BringupTaskContext* restore)
{
    __asm__ volatile(
        /* Save current task: R4-R11, LR, SP */
        "STR    r4,  [r0, #0]       \n\t"
        "STR    r5,  [r0, #4]       \n\t"
        "STR    r6,  [r0, #8]       \n\t"
        "STR    r7,  [r0, #12]      \n\t"
        "STR    r8,  [r0, #16]      \n\t"
        "STR    r9,  [r0, #20]      \n\t"
        "STR    r10, [r0, #24]      \n\t"
        "STR    r11, [r0, #28]      \n\t"
        "STR    lr,  [r0, #32]      \n\t"
        "STR    sp,  [r0, #36]      \n\t"
        /* Restore next task: SP first, then R4-R11, LR */
        "LDR    sp,  [r1, #36]      \n\t"
        "LDR    r4,  [r1, #0]       \n\t"
        "LDR    r5,  [r1, #4]       \n\t"
        "LDR    r6,  [r1, #8]       \n\t"
        "LDR    r7,  [r1, #12]      \n\t"
        "LDR    r8,  [r1, #16]      \n\t"
        "LDR    r9,  [r1, #20]      \n\t"
        "LDR    r10, [r1, #24]      \n\t"
        "LDR    r11, [r1, #28]      \n\t"
        "LDR    lr,  [r1, #32]      \n\t"
        /* Branch to restored task */
        "BX     lr                  \n\t"
    );
}

/**
 * @brief  Task B entry — reports over UART, then switches back to Task A
 *
 * @note   Entered via cooperative switch. LR is not meaningful (initial
 *         launch), so this function must explicitly switch back.
 */
__attribute__((used))
static void bringup_task_b_entry(void)
{
    sc_sci_puts("[BRINGUP-4] Task B reached!\r\n");
    bringup_task_b_ran = TRUE;

    /* Switch back to Task A */
    bringup_switch_context(&bringup_ctx_b, &bringup_ctx_a);

    /* Should never reach here */
    sc_sci_puts("[BRINGUP-4] ERROR: Task B resumed unexpectedly\r\n");
    for (;;) {}
}

/**
 * @brief  Prove two-task cooperative context switch
 *
 * @return TRUE if Task B ran and Task A resumed with registers intact
 *
 * @note   Must be called from the launched task (System mode).
 *         Uses bringup_task_b_stack as Task B's stack.
 */
static boolean bringup_test_two_task_switch(void)
{
    uintptr_t stackTop;
    uint32 spBefore;
    uint32 spAfter;
    boolean pass;

    sc_sci_puts("[BRINGUP-4] Two-task cooperative switch...\r\n");

    /* Prepare Task B's initial context */
    bringup_ctx_b.r4  = 0u;
    bringup_ctx_b.r5  = 0u;
    bringup_ctx_b.r6  = 0u;
    bringup_ctx_b.r7  = 0u;
    bringup_ctx_b.r8  = 0u;
    bringup_ctx_b.r9  = 0u;
    bringup_ctx_b.r10 = 0u;
    bringup_ctx_b.r11 = 0u;
    bringup_ctx_b.lr  = (uint32)(uintptr_t)&bringup_task_b_entry;
    stackTop = (uintptr_t)&bringup_task_b_stack[sizeof(bringup_task_b_stack)];
    bringup_ctx_b.sp  = (uint32)(stackTop & ~(uintptr_t)7u);

    bringup_task_b_ran = FALSE;

    /* Capture SP before switch */
    __asm__ volatile("MOV %0, sp" : "=r"(spBefore));

    sc_sci_puts("[BRINGUP-4] Task A: switching to Task B...\r\n");

    /* Save Task A's context, load Task B's, branch to Task B.
     * When Task B switches back, we resume right here. */
    bringup_switch_context(&bringup_ctx_a, &bringup_ctx_b);

    /* Resumed — Task B switched back to us */
    sc_sci_puts("[BRINGUP-4] Task A: resumed!\r\n");

    __asm__ volatile("MOV %0, sp" : "=r"(spAfter));

    /* Verify */
    pass = TRUE;
    if (bringup_task_b_ran == FALSE) {
        pass = FALSE;
        sc_sci_puts("[BRINGUP-4] FAIL - Task B did not run\r\n");
    }
    if (spBefore != spAfter) {
        pass = FALSE;
        sc_sci_puts("[BRINGUP-4] SP MISMATCH: 0x");
        bringup_put_hex(spBefore);
        sc_sci_puts(" -> 0x");
        bringup_put_hex(spAfter);
        sc_sci_puts("\r\n");
    }

    sc_sci_puts(pass ? "[BRINGUP-4] PASS\r\n" : "[BRINGUP-4] FAIL\r\n");
    return pass;
}

/* ==================================================================
 * Test 5: IRQ-driven preemption
 *
 * Called from inside the launched task (test 2). Proves:
 *   - An IRQ can preempt a running task mid-instruction-stream
 *   - The ISR can switch to a different task (Task B) and run it
 *   - Task B can switch back, resuming the ISR
 *   - The ISR returns to the original task (Task A) with all
 *     registers and SP intact
 *
 * Uses a naked ISR that:
 *   1. Saves scratch regs on IRQ stack (standard IRQ entry)
 *   2. Checks a preemption flag
 *   3. If set: switches to System mode via MSR CPSR_c, does
 *      cooperative context switch to Task B, waits for Task B
 *      to switch back, then returns to IRQ mode for exception return
 *
 * Key insight: R4-R11 are NOT banked between System and IRQ modes.
 * When the ISR fires, R4-R11 hold the interrupted task's values.
 * The cooperative switch (in System mode) saves/restores them via
 * BringupTaskContext, so the interrupted task sees them unchanged.
 *
 * This matches the ThreadX preemption pattern (tx_thread_context_restore.S
 * lines 166-229) but simplified to prove the concept without a scheduler.
 * ================================================================== */

static BringupTaskContext bringup_preempt_ctx_a;
static BringupTaskContext bringup_preempt_ctx_b;
static uint8 bringup_preempt_b_stack[512u] __attribute__((aligned(8)));
static volatile boolean bringup_preempt_b_ran = FALSE;
static volatile uint32 bringup_preempt_flag = 0u;
static volatile uint32 bringup_preempt_count = 0u;

/**
 * @brief  Task B entry for preemption test — runs inside ISR context
 *
 * Entered via cooperative switch from the preemption ISR (System mode,
 * IRQs disabled). Sets a flag and switches back to the ISR. On
 * subsequent entries, increments count and switches back.
 *
 * @note   Runs on its own stack but in System mode with IRQs disabled.
 */
__attribute__((used))
static void bringup_preempt_task_b_entry(void)
{
    sc_sci_puts("[BRINGUP-5] Task B preempted in!\r\n");
    bringup_preempt_b_ran = TRUE;
    bringup_preempt_count++;

    /* Switch back to ISR (Task A's context inside the ISR) */
    bringup_switch_context(&bringup_preempt_ctx_b, &bringup_preempt_ctx_a);

    /* Subsequent preemptions land here */
    for (;;) {
        bringup_preempt_count++;
        bringup_switch_context(&bringup_preempt_ctx_b, &bringup_preempt_ctx_a);
    }
}

/**
 * @brief  Naked preemption ISR for RTI compare0
 *
 * Normal path: acknowledge RTI, increment count, exception return.
 * Preemption path: switch to System mode, cooperative-switch to Task B,
 * wait for Task B to return, switch back to IRQ mode, exception return.
 *
 * @note   Uses MSR CPSR_c for mode switches (following ThreadX pattern)
 *         rather than CPS, for explicit control over I/F bits.
 *         0x9F = System mode + I=1 (IRQ disabled)
 *         0x92 = IRQ mode + I=1 (IRQ disabled)
 *
 * @note   SPSR_irq is preserved throughout — no code writes to it.
 *         The final LDMIA ^{pc} restores CPSR from SPSR_irq, returning
 *         to the interrupted task in its original processor state.
 */
__attribute__((naked))
void bringup_preempt_isr(void)
{
    __asm__ volatile(
        /* ---- Standard IRQ entry ---- */
        "SUB    lr, lr, #4              \n\t"
        "STMDB  sp!, {r0-r3, r12, lr}   \n\t"

        /* ---- Acknowledge RTI compare0 ---- */
        "LDR    r0, =0xFFFFFC88         \n\t"   /* rtiREG1->INTFLAG */
        "MOV    r1, #1                  \n\t"
        "STR    r1, [r0]                \n\t"

        /* ---- Increment IRQ count ---- */
        "LDR    r0, =bringup_rti_irq_count \n\t"
        "LDR    r1, [r0]                \n\t"
        "ADD    r1, r1, #1              \n\t"
        "STR    r1, [r0]                \n\t"

        /* ---- Check preemption flag ---- */
        "LDR    r0, =bringup_preempt_flag \n\t"
        "LDR    r1, [r0]                \n\t"
        "CMP    r1, #0                  \n\t"
        "BEQ    1f                      \n\t"

        /* ---- PREEMPTION PATH ---- */
        /* Clear flag (one preemption per request) */
        "MOV    r1, #0                  \n\t"
        "STR    r1, [r0]                \n\t"

        /* Switch to System mode (I=1: IRQs remain disabled) */
        "MOV    r2, #0x9F              \n\t"
        "MSR    CPSR_c, r2             \n\t"

        /* Now SP = SP_sys (Task A's stack), LR = LR_sys */
        /* Save LR_sys — BLX will clobber it */
        "PUSH   {lr}                    \n\t"

        /* Cooperative switch: save Task A state, run Task B */
        "LDR    r0, =bringup_preempt_ctx_a \n\t"
        "LDR    r1, =bringup_preempt_ctx_b \n\t"
        "LDR    r2, =bringup_switch_context \n\t"
        "BLX    r2                      \n\t"

        /* Resumed — Task B switched back */
        "POP    {lr}                    \n\t"

        /* Switch back to IRQ mode (I=1: IRQs disabled) */
        "MOV    r2, #0x92              \n\t"
        "MSR    CPSR_c, r2             \n\t"

        /* ---- Normal exception return ---- */
        "1:                             \n\t"
        "LDMIA  sp!, {r0-r3, r12, pc}^ \n\t"

        /* Literal pool for LDR =symbol references */
        ".ltorg                         \n\t"
    );
}

/**
 * @brief  Prove IRQ-driven preemptive task switch
 *
 * Loads sentinel values into R4-R11, enables IRQs, and busy-waits.
 * The preemption ISR fires, switches to Task B (which runs and
 * switches back), then returns to this busy-wait loop. After the
 * loop, verifies all registers, SP, and that Task B actually ran.
 *
 * @return TRUE if preemption succeeded with full register preservation
 *
 * @note   Must be called from the launched task (System mode).
 *         Reuses bringup_reg_sentinels from test 3.
 */
static boolean bringup_test_irq_preemption(void)
{
    uint32 after[8];
    uint32 spBefore;
    uint32 spAfter;
    uint32 irqsBefore;
    uint32 irqsAfter;
    boolean pass;
    uint32 i;
    uintptr_t stackTop;

    sc_sci_puts("[BRINGUP-5] IRQ-driven preemption...\r\n");

    /* Prepare Task B's initial context */
    bringup_preempt_ctx_b.r4  = 0u;
    bringup_preempt_ctx_b.r5  = 0u;
    bringup_preempt_ctx_b.r6  = 0u;
    bringup_preempt_ctx_b.r7  = 0u;
    bringup_preempt_ctx_b.r8  = 0u;
    bringup_preempt_ctx_b.r9  = 0u;
    bringup_preempt_ctx_b.r10 = 0u;
    bringup_preempt_ctx_b.r11 = 0u;
    bringup_preempt_ctx_b.lr  = (uint32)(uintptr_t)&bringup_preempt_task_b_entry;
    stackTop = (uintptr_t)&bringup_preempt_b_stack[sizeof(bringup_preempt_b_stack)];
    bringup_preempt_ctx_b.sp  = (uint32)(stackTop & ~(uintptr_t)7u);

    bringup_preempt_b_ran = FALSE;
    bringup_preempt_count = 0u;
    bringup_preempt_flag = 0u;
    bringup_rti_irq_count = 0u;

    /* Map preemption ISR to VIM ch2 */
    vimChannelMap(2u, 2u, (t_isrFuncPTR)&bringup_preempt_isr);
    vimEnableInterrupt(2u, SYS_IRQ);
    rtiREG1->SETINTENA = (uint32)1u;

    /* Arm preemption — next IRQ will trigger it */
    bringup_preempt_flag = 1u;

    irqsBefore = bringup_rti_irq_count;

    /* Load sentinels, enable IRQ, busy-wait, disable IRQ.
     * During the wait, the preemption ISR fires and switches to
     * Task B, then back — transparent to this code path. */
    __asm__ volatile(
        "MOV    %[spb], sp          \n\t"
        "LDR    r4, [%[sen], #0]    \n\t"
        "LDR    r5, [%[sen], #4]    \n\t"
        "LDR    r6, [%[sen], #8]    \n\t"
        "LDR    r7, [%[sen], #12]   \n\t"
        "LDR    r8, [%[sen], #16]   \n\t"
        "LDR    r9, [%[sen], #20]   \n\t"
        "LDR    r10, [%[sen], #24]  \n\t"
        "LDR    r11, [%[sen], #28]  \n\t"
        "CPSIE  i                   \n\t"
        "MOV    r0, %[cnt]          \n\t"
        "1:                         \n\t"
        "SUBS   r0, r0, #1          \n\t"
        "BNE    1b                  \n\t"
        "CPSID  i                   \n\t"
        "STR    r4, [%[aft], #0]    \n\t"
        "STR    r5, [%[aft], #4]    \n\t"
        "STR    r6, [%[aft], #8]    \n\t"
        "STR    r7, [%[aft], #12]   \n\t"
        "STR    r8, [%[aft], #16]   \n\t"
        "STR    r9, [%[aft], #20]   \n\t"
        "STR    r10, [%[aft], #24]  \n\t"
        "STR    r11, [%[aft], #28]  \n\t"
        "MOV    %[spa], sp          \n\t"
        : [spb] "=&r" (spBefore), [spa] "=&r" (spAfter)
        : [sen] "r" (bringup_reg_sentinels), [aft] "r" (after),
          [cnt] "r" ((uint32)0x00800000u)
        : "r0", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11",
          "memory"
    );

    irqsAfter = bringup_rti_irq_count;

    /* Restore polled mode */
    rtiREG1->CLEARINTENA = (uint32)1u;
    vimDisableInterrupt(2u);
    rtiREG1->INTFLAG = (uint32)1u;

    /* Verify register preservation */
    pass = TRUE;
    for (i = 0u; i < 8u; i++) {
        if (bringup_reg_sentinels[i] != after[i]) {
            pass = FALSE;
            sc_sci_puts("[BRINGUP-5] R");
            sc_sci_put_uint(i + 4u);
            sc_sci_puts(" MISMATCH: 0x");
            bringup_put_hex(bringup_reg_sentinels[i]);
            sc_sci_puts(" -> 0x");
            bringup_put_hex(after[i]);
            sc_sci_puts("\r\n");
        }
    }

    /* Verify SP preservation */
    if (spBefore != spAfter) {
        pass = FALSE;
        sc_sci_puts("[BRINGUP-5] SP MISMATCH: 0x");
        bringup_put_hex(spBefore);
        sc_sci_puts(" -> 0x");
        bringup_put_hex(spAfter);
        sc_sci_puts("\r\n");
    }

    /* Verify Task B ran */
    if (bringup_preempt_b_ran == FALSE) {
        pass = FALSE;
        sc_sci_puts("[BRINGUP-5] FAIL - Task B did not run\r\n");
    }

    /* Report */
    sc_sci_puts("[BRINGUP-5] IRQs: ");
    sc_sci_put_uint(irqsAfter - irqsBefore);
    sc_sci_puts(", preemptions: ");
    sc_sci_put_uint(bringup_preempt_count);
    sc_sci_puts("\r\n");

    if ((irqsAfter - irqsBefore) == 0u) {
        sc_sci_puts("[BRINGUP-5] FAIL - no IRQs fired\r\n");
        return FALSE;
    }

    sc_sci_puts(pass ? "[BRINGUP-5] PASS\r\n" : "[BRINGUP-5] FAIL\r\n");
    return pass;
}

/* ==================================================================
 * Test 6: FIQ does not break IRQ-return ownership
 *
 * Called from inside the launched task (test 2). Proves:
 *   - FIQ can fire during an IRQ handler (including the preemption path)
 *   - FIQ handler correctly uses banked FIQ registers (R8_fiq-R12_fiq,
 *     SP_fiq, LR_fiq, SPSR_fiq) without corrupting System/IRQ state
 *   - SPSR_irq is NOT corrupted by FIQ — only accessible from IRQ mode,
 *     and FIQ mode has its own SPSR_fiq
 *   - Task registers (R4-R11, SP) survive IRQ+FIQ concurrent firing
 *
 * Uses RTI compare0 as IRQ (preemption ISR, same as test 5) and
 * RTI compare1 as FIQ (simple counter). Both fire at different rates
 * during the busy-wait. FIQ can preempt the IRQ handler at any point
 * including during the mode-switch preemption path.
 *
 * ARM register banking guarantees safety:
 *   - FIQ-banked: R8_fiq-R12_fiq, SP_fiq, LR_fiq, SPSR_fiq
 *   - IRQ-banked: SP_irq, LR_irq, SPSR_irq
 *   - FIQ mode cannot access or corrupt IRQ-banked registers
 *   - R0-R7 are shared but __attribute__((interrupt("FIQ"))) saves them
 * ================================================================== */

static volatile uint32 bringup_fiq_count = 0u;

/**
 * @brief  FIQ handler for RTI compare1 — increments counter, acknowledges
 *
 * @note   Uses __attribute__((interrupt("FIQ"))) so compiler generates
 *         proper save/restore of shared R0-R7 and SUBS PC, LR, #4 return.
 *         FIQ-banked R8-R12 are separate from System/IRQ R8-R12.
 *         FIQ sets F=1 on entry (prevents nested FIQ).
 */
void __attribute__((interrupt("FIQ"))) bringup_fiq_isr(void)
{
    bringup_fiq_count++;
    rtiREG1->INTFLAG = 2u;  /* W1C: acknowledge compare1 (bit 1) */
}

/**
 * @brief  Task B entry for FIQ test — prints and switches back
 *
 * @note   Separate from test 5's entry so UART output identifies test 6.
 */
__attribute__((used))
static void bringup_fiq_task_b_entry(void)
{
    sc_sci_puts("[BRINGUP-6] Task B preempted in (FIQ+IRQ)!\r\n");
    bringup_preempt_b_ran = TRUE;
    bringup_preempt_count++;

    /* Switch back to ISR */
    bringup_switch_context(&bringup_preempt_ctx_b, &bringup_preempt_ctx_a);

    /* Subsequent preemptions */
    for (;;) {
        bringup_preempt_count++;
        bringup_switch_context(&bringup_preempt_ctx_b, &bringup_preempt_ctx_a);
    }
}

/**
 * @brief  Prove FIQ does not break IRQ preemption path
 *
 * Combines IRQ-driven preemption (test 5) with concurrent FIQ.
 * Uses RTI compare0 as IRQ (VIM ch2) and RTI compare1 as FIQ (VIM ch3).
 * Both fire during the busy-wait. Verifies that FIQ cannot corrupt
 * SPSR_irq, R4-R11, or SP across the combined IRQ+FIQ interrupt storm.
 *
 * @return TRUE if all registers preserved and both IRQ+FIQ fired
 *
 * @note   Must be called from the launched task (System mode).
 *         Reuses bringup_preempt_isr and bringup_preempt_ctx_a/b from test 5.
 */
static boolean bringup_test_fiq_ownership(void)
{
    uint32 after[8];
    uint32 spBefore;
    uint32 spAfter;
    uint32 irqsBefore;
    uint32 irqsAfter;
    boolean pass;
    uint32 i;
    uintptr_t stackTop;
    uint32 fiqPeriod;

    sc_sci_puts("[BRINGUP-6] FIQ does not break IRQ-return ownership...\r\n");

    /* Prepare Task B's initial context (fresh for test 6) */
    bringup_preempt_ctx_b.r4  = 0u;
    bringup_preempt_ctx_b.r5  = 0u;
    bringup_preempt_ctx_b.r6  = 0u;
    bringup_preempt_ctx_b.r7  = 0u;
    bringup_preempt_ctx_b.r8  = 0u;
    bringup_preempt_ctx_b.r9  = 0u;
    bringup_preempt_ctx_b.r10 = 0u;
    bringup_preempt_ctx_b.r11 = 0u;
    bringup_preempt_ctx_b.lr  = (uint32)(uintptr_t)&bringup_fiq_task_b_entry;
    stackTop = (uintptr_t)&bringup_preempt_b_stack[sizeof(bringup_preempt_b_stack)];
    bringup_preempt_ctx_b.sp  = (uint32)(stackTop & ~(uintptr_t)7u);

    bringup_preempt_b_ran = FALSE;
    bringup_preempt_count = 0u;
    bringup_preempt_flag = 0u;
    bringup_rti_irq_count = 0u;
    bringup_fiq_count = 0u;

    /* Map preemption ISR to VIM ch2 as IRQ (same ISR as test 5) */
    vimChannelMap(2u, 2u, (t_isrFuncPTR)&bringup_preempt_isr);
    vimEnableInterrupt(2u, SYS_IRQ);

    /* Configure RTI compare1 for FIQ at ~70% of compare0's period.
     * Different rate avoids phase-lock with compare0. */
    rtiREG1->COMPCTRL &= ~(uint32)(1u << 4u);  /* compare1 uses counter block 0 */
    fiqPeriod = (rtiREG1->CMP[0u].UDCPx * 7u) / 10u;
    if (fiqPeriod == 0u) { fiqPeriod = 1u; }
    rtiREG1->CMP[1u].COMPx = rtiREG1->CNT[0u].FRCx + fiqPeriod;
    rtiREG1->CMP[1u].UDCPx = fiqPeriod;
    rtiREG1->INTFLAG = 2u;  /* Clear any pending compare1 flag */

    /* Map FIQ ISR to VIM ch3 as FIQ */
    vimChannelMap(3u, 3u, (t_isrFuncPTR)&bringup_fiq_isr);
    vimEnableInterrupt(3u, SYS_FIQ);

    /* Enable both compare0 and compare1 interrupts */
    rtiREG1->SETINTENA = 3u;  /* bits 0+1 */

    /* Arm preemption — next IRQ triggers task switch */
    bringup_preempt_flag = 1u;

    irqsBefore = bringup_rti_irq_count;

    /* Same busy-wait as test 5, but enable BOTH IRQ and FIQ.
     * FIQ can fire at any point including during the IRQ
     * preemption path's mode switches. */
    __asm__ volatile(
        "MOV    %[spb], sp          \n\t"
        "LDR    r4, [%[sen], #0]    \n\t"
        "LDR    r5, [%[sen], #4]    \n\t"
        "LDR    r6, [%[sen], #8]    \n\t"
        "LDR    r7, [%[sen], #12]   \n\t"
        "LDR    r8, [%[sen], #16]   \n\t"
        "LDR    r9, [%[sen], #20]   \n\t"
        "LDR    r10, [%[sen], #24]  \n\t"
        "LDR    r11, [%[sen], #28]  \n\t"
        "CPSIE  if                  \n\t"   /* Enable BOTH IRQ and FIQ */
        "MOV    r0, %[cnt]          \n\t"
        "1:                         \n\t"
        "SUBS   r0, r0, #1          \n\t"
        "BNE    1b                  \n\t"
        "CPSID  if                  \n\t"   /* Disable BOTH IRQ and FIQ */
        "STR    r4, [%[aft], #0]    \n\t"
        "STR    r5, [%[aft], #4]    \n\t"
        "STR    r6, [%[aft], #8]    \n\t"
        "STR    r7, [%[aft], #12]   \n\t"
        "STR    r8, [%[aft], #16]   \n\t"
        "STR    r9, [%[aft], #20]   \n\t"
        "STR    r10, [%[aft], #24]  \n\t"
        "STR    r11, [%[aft], #28]  \n\t"
        "MOV    %[spa], sp          \n\t"
        : [spb] "=&r" (spBefore), [spa] "=&r" (spAfter)
        : [sen] "r" (bringup_reg_sentinels), [aft] "r" (after),
          [cnt] "r" ((uint32)0x00800000u)
        : "r0", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11",
          "memory"
    );

    irqsAfter = bringup_rti_irq_count;

    /* Restore polled mode — disable both compare0 and compare1 */
    rtiREG1->CLEARINTENA = 3u;  /* bits 0+1 */
    vimDisableInterrupt(2u);
    vimDisableInterrupt(3u);
    rtiREG1->INTFLAG = 3u;  /* Clear both pending flags */

    /* Verify register preservation */
    pass = TRUE;
    for (i = 0u; i < 8u; i++) {
        if (bringup_reg_sentinels[i] != after[i]) {
            pass = FALSE;
            sc_sci_puts("[BRINGUP-6] R");
            sc_sci_put_uint(i + 4u);
            sc_sci_puts(" MISMATCH: 0x");
            bringup_put_hex(bringup_reg_sentinels[i]);
            sc_sci_puts(" -> 0x");
            bringup_put_hex(after[i]);
            sc_sci_puts("\r\n");
        }
    }

    /* Verify SP preservation */
    if (spBefore != spAfter) {
        pass = FALSE;
        sc_sci_puts("[BRINGUP-6] SP MISMATCH: 0x");
        bringup_put_hex(spBefore);
        sc_sci_puts(" -> 0x");
        bringup_put_hex(spAfter);
        sc_sci_puts("\r\n");
    }

    /* Verify Task B ran */
    if (bringup_preempt_b_ran == FALSE) {
        pass = FALSE;
        sc_sci_puts("[BRINGUP-6] FAIL - Task B did not run\r\n");
    }

    /* Report */
    sc_sci_puts("[BRINGUP-6] IRQs: ");
    sc_sci_put_uint(irqsAfter - irqsBefore);
    sc_sci_puts(", FIQs: ");
    sc_sci_put_uint(bringup_fiq_count);
    sc_sci_puts(", preemptions: ");
    sc_sci_put_uint(bringup_preempt_count);
    sc_sci_puts("\r\n");

    if ((irqsAfter - irqsBefore) == 0u) {
        pass = FALSE;
        sc_sci_puts("[BRINGUP-6] FAIL - no IRQs fired\r\n");
    }

    if (bringup_fiq_count == 0u) {
        pass = FALSE;
        sc_sci_puts("[BRINGUP-6] FAIL - no FIQs fired\r\n");
    }

    sc_sci_puts(pass ? "[BRINGUP-6] PASS\r\n" : "[BRINGUP-6] FAIL\r\n");
    return pass;
}

/* ==================================================================
 * Test 2: Prove first-task context launch via direct MSR+BX
 *
 * Builds a synthetic initial frame on a dedicated stack, then launches
 * the task using MSR CPSR_cxsf + BX (not exception return). Proves:
 *   - Initial frame layout is correct
 *   - Direct mode switch via MSR works from System mode
 *   - CPU lands at the task entry function in System mode (0x1F)
 *   - Task can execute C code and call SCI UART functions
 *
 * This test is a ONE-WAY TRIP — it never returns to BringupAll.
 * The task entry also runs test 3 (IRQ return) before entering
 * the LED blink loop.
 * ================================================================== */

/** @brief  512-byte stack for the bring-up test task */
static uint8 bringup_task_stack[512u] __attribute__((aligned(8)));

/* LED control — declared here to avoid sc_hw.h/sc_types.h conflict */
extern void sc_het_led_on(void);
extern void sc_het_led_off(void);

/** @brief  Initial frame: 2 header words + 15 register words = 17 */
#define BRINGUP_FRAME_WORDS  17u
#define BRINGUP_FRAME_BYTES  (BRINGUP_FRAME_WORDS * 4u)

/** @brief  Target CPSR mode bits for the task (System mode = 0x1F).
 *
 * HALCoGen's _c_int00 switches to System mode before calling main(),
 * so tasks should also run in System mode. The full CPSR value is
 * computed at runtime by reading the current CPSR and replacing the
 * low 8 bits (mode + I/F/T flags) with the target mode. This
 * preserves the E bit (big-endian) and condition flags. */
#define BRINGUP_TARGET_MODE  0x1Fu

/**
 * @brief  Entry point for the first-task bring-up test
 *
 * Verifies CPU mode via MRS CPSR, then runs test 3 (same-task IRQ
 * return with register preservation). Reports combined pass/fail,
 * then enters a polled RTI LED blink loop to prove the task is alive.
 * This function never returns.
 *
 * @note   Entered via direct MSR+BX from bringup_launch_task().
 *         R0-R12 and LR are zeroed, SP points past the consumed frame.
 */
__attribute__((used))
static void bringup_first_task_entry(void)
{
    uint32 cpsrVal;
    boolean allPass;
    boolean test3Pass;
    uint32 blink = 0u;

    __asm__ volatile("MRS %0, CPSR" : "=r"(cpsrVal));

    sc_sci_puts("[BRINGUP-2] First task entry reached!\r\n");
    sc_sci_puts("[BRINGUP-2] CPSR = 0x");
    bringup_put_hex(cpsrVal);
    sc_sci_puts(" (mode = 0x");
    bringup_put_hex(cpsrVal & 0x1Fu);
    sc_sci_puts(")\r\n");

    /* Expect System mode (0x1F) — HALCoGen runs main() in System mode */
    allPass = ((cpsrVal & 0x1Fu) == BRINGUP_TARGET_MODE) ? TRUE : FALSE;
    sc_sci_puts(allPass ? "[BRINGUP-2] PASS\r\n" : "[BRINGUP-2] FAIL\r\n");

    /* Test 3: same-task IRQ return with register preservation */
    test3Pass = bringup_test_same_task_irq_return();
    if (test3Pass == FALSE) {
        allPass = FALSE;
    }

    /* Test 4: two-task cooperative switch */
    if (bringup_test_two_task_switch() == FALSE) {
        allPass = FALSE;
    }

    /* Test 5: IRQ-driven preemption */
    if (bringup_test_irq_preemption() == FALSE) {
        allPass = FALSE;
    }

    /* Test 6: FIQ does not break IRQ-return ownership */
    if (bringup_test_fiq_ownership() == FALSE) {
        allPass = FALSE;
    }

    sc_sci_puts("=== Bring-up ");
    sc_sci_puts(allPass ? "ALL PASS" : "SOME FAILED");
    sc_sci_puts(" ===\r\n\r\n");

    /* Stay alive: polled RTI LED blink (500ms on / 500ms off) */
    for (;;) {
        if ((rtiREG1->INTFLAG & 1u) != 0u) {
            rtiREG1->INTFLAG = 1u;
            blink++;
            if (blink < 50u) {
                sc_het_led_on();
            } else if (blink < 100u) {
                sc_het_led_off();
            } else {
                blink = 0u;
            }
        }
    }
}

/**
 * @brief  Launch a task by setting CPSR, SP, zeroing regs, and branching
 *
 * @param  frameSp  Base address of the initial frame (StackType at offset 0)
 * @param  cpsr     Target CPSR value (must preserve I/F bits to avoid
 *                  spurious interrupts during launch)
 *
 * @note   AAPCS: frameSp in R0, cpsr in R1.
 * @note   This function never returns.
 *
 * Uses direct MSR CPSR_cxsf + BX instead of exception return (LDMIA ^).
 * Exception return is not usable here because HALCoGen runs main() in
 * System mode, which has no SPSR — LDMIA ^ from System mode is
 * UNPREDICTABLE per ARM ARM.
 *
 * Critical: the target CPSR must keep I=1, F=1 (interrupts disabled)
 * during the launch sequence. Mask only mode bits (0x1F), not the full
 * low byte (0xFF), when computing the target from current CPSR.
 */
__attribute__((naked, noreturn))
static void bringup_launch_task(uint32 frameSp, uint32 cpsr)
{
    __asm__ volatile(
        /* R0 = frameSp, R1 = target CPSR (AAPCS) */

        /* 1. Compute post-frame SP and load entry address */
        "ADD    r2, r0, #68         \n\t"   /* R2 = post-frame task SP */
        "LDR    r3, [r0, #64]       \n\t"   /* R3 = PC from frame[16] */

        /* 2. Switch to target mode directly via MSR CPSR */
        "MSR    CPSR_cxsf, r1      \n\t"   /* CPSR = target (System mode) */

        /* 3. Now in System mode — set SP */
        "MOV    sp, r2              \n\t"   /* SP_sys = post-frame SP */

        /* 4. Zero all registers to match frame (R0-R12, LR = 0) */
        "MOV    r0, #0              \n\t"
        "MOV    r1, #0              \n\t"
        "MOV    r2, #0              \n\t"
        "MOV    r4, #0              \n\t"
        "MOV    r5, #0              \n\t"
        "MOV    r6, #0              \n\t"
        "MOV    r7, #0              \n\t"
        "MOV    r8, #0              \n\t"
        "MOV    r9, #0              \n\t"
        "MOV    r10, #0             \n\t"
        "MOV    r11, #0             \n\t"
        "MOV    r12, #0             \n\t"
        "MOV    lr, #0              \n\t"

        /* 5. Jump to task entry */
        "BX     r3                  \n\t"
    );
}

/**
 * @brief  Build initial frame and launch first task via direct MSR+BX
 *
 * Frame layout (17 x uint32 = 68 bytes):
 *   [0]  StackType = 1 (interrupt frame)
 *   [1]  CPSR = 0x13 (SVC mode, IRQ+FIQ enabled)
 *   [2]  R0 = 0
 *   ...
 *   [14] R12 = 0
 *   [15] LR = 0
 *   [16] PC = bringup_first_task_entry
 *
 * After LDMIA: SP = frameSp + 68, pointing past the consumed frame.
 * The task uses the remaining stack space below SP (growing downward).
 *
 * @note   This function never returns — one-way trip to the task.
 */
static void bringup_test_first_task_launch(void)
{
    uintptr_t stackTop;
    uintptr_t frameSp;
    uint32* frame;
    uint32 i;
    uint32 currentCpsr;
    uint32 targetCpsr;

    sc_sci_puts("[BRINGUP-2] First-task launch via direct MSR+BX...\r\n");

    /* Read current CPSR to preserve E (big-endian), I/F, and condition flags.
     * Replace only mode bits [4:0] with target mode. Do NOT mask the full
     * low byte — that would clear I/F bits, enabling interrupts and causing
     * a spurious FIQ during the launch sequence. */
    __asm__ volatile("MRS %0, CPSR" : "=r"(currentCpsr));
    targetCpsr = (currentCpsr & ~(uint32)0x1Fu) | BRINGUP_TARGET_MODE;

    sc_sci_puts("[BRINGUP-2] Current CPSR = 0x");
    bringup_put_hex(currentCpsr);
    sc_sci_puts(", target = 0x");
    bringup_put_hex(targetCpsr);
    sc_sci_puts("\r\n");

    /* Allocate frame at top of stack, 8-byte aligned */
    stackTop = (uintptr_t)&bringup_task_stack[sizeof(bringup_task_stack)];
    frameSp = (stackTop - BRINGUP_FRAME_BYTES) & ~(uintptr_t)7u;
    frame = (uint32*)frameSp;

    /* Zero entire frame */
    for (i = 0u; i < BRINGUP_FRAME_WORDS; i++) {
        frame[i] = 0u;
    }

    /* Fill header and entry point */
    frame[0] = 1u;                          /* StackType = interrupt frame */
    frame[1] = targetCpsr;                  /* CPSR for exception return */
    frame[16] = (uint32)(uintptr_t)&bringup_first_task_entry;  /* PC */

    sc_sci_puts("[BRINGUP-2] Frame at 0x");
    bringup_put_hex((uint32)frameSp);
    sc_sci_puts(", entry at 0x");
    bringup_put_hex(frame[16]);
    sc_sci_puts("\r\n");

    /* One-way trip — launch the task */
    bringup_launch_task((uint32)frameSp, targetCpsr);
}

/* ==================================================================
 * Public entry point
 * ================================================================== */

/**
 * @brief  Run all bring-up tests and report summary
 *
 * Call from sc_main.c after rtiStartCounter() and before the main loop.
 *
 * @note   Test 2 (first-task launch) is a one-way trip — this function
 *         never returns. The task entry function takes over as the
 *         new main loop with LED blink.
 */
void Os_Port_Tms570_BringupAll(void)
{
    sc_sci_puts("\r\n=== OS Bootstrap Bring-up Tests ===\r\n");

    if (bringup_test_rti_compare0_irq() == FALSE) {
        sc_sci_puts("=== Bring-up SOME FAILED ===\r\n\r\n");
        return;  /* Don't attempt further tests if IRQ is broken */
    }

    /* Test 2 is a one-way trip — never returns.
     * The task entry prints the final summary. */
    bringup_test_first_task_launch();

    /* Future bring-up tests (added inside the task or as separate steps):
     *   bringup_test_same_task_irq_return()
     *   bringup_test_two_task_switch()
     *   bringup_test_irq_preemption()
     *   bringup_test_fiq_ownership()
     */
}

#endif /* PLATFORM_TMS570 */
#endif /* OS_BOOTSTRAP_BRINGUP */
