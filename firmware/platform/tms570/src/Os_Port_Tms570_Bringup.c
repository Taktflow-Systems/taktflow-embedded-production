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
static void bringup_rti_compare0_isr(void)
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
 * Test 2: Prove VIM channel 2 routes to IRQ (not FIQ)
 *
 * This is implicitly proven by test 1 — if the ISR fires via IRQ
 * vector and increments the counter, VIM channel 2 is routed as IRQ.
 * A separate FIQ test would require routing to FIQ and checking
 * the FIQ vector, which is Phase 3 step 7 territory.
 * ================================================================== */

/* ==================================================================
 * Public entry point
 * ================================================================== */

/**
 * @brief  Run all bring-up tests and report summary
 *
 * Call from sc_main.c after rtiStartCounter() and before the main loop.
 * All tests restore the hardware to polled-RTI state on completion.
 */
void Os_Port_Tms570_BringupAll(void)
{
    boolean allPass = TRUE;

    sc_sci_puts("\r\n=== OS Bootstrap Bring-up Tests ===\r\n");

    if (bringup_test_rti_compare0_irq() == FALSE) {
        allPass = FALSE;
    }

    /* Future bring-up tests will be added here:
     *   bringup_test_first_task_launch()
     *   bringup_test_same_task_irq_return()
     *   bringup_test_two_task_switch()
     *   bringup_test_irq_preemption()
     *   bringup_test_fiq_ownership()
     */

    sc_sci_puts("=== Bring-up ");
    sc_sci_puts(allPass ? "ALL PASS" : "SOME FAILED");
    sc_sci_puts(" ===\r\n\r\n");
}

#endif /* PLATFORM_TMS570 */
#endif /* OS_BOOTSTRAP_BRINGUP */
