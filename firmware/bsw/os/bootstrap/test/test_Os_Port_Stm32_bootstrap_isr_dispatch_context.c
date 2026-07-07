/**
 * @file    test_Os_Port_Stm32_bootstrap_isr_dispatch_context.c
 * @brief   ISR context query tests for the STM32 bootstrap OS port
 * @date    2026-03-14
 *
 * @details Verifies that Os_PortIsInIsrContext correctly reports whether the
 *          CPU is currently executing inside an ISR (Cat2 nesting > 0).
 *          On real Cortex-M4 hardware, IPSR would be non-zero in ISR context.
 *          The unit-test mock uses the port-level Isr2Nesting counter.
 *
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "Os.h"
#include "Os_Port_Stm32.h"

void setUp(void) {}
void tearDown(void) {}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_schedule.S
 *       ThreadX checks IPSR to determine ISR context. Bootstrap uses
 *       port-level Isr2Nesting counter as the equivalent query.
 * @requirement The port shall report ISR context as inactive when no
 *              Cat2 ISR is in progress.
 * @verify Os_PortIsInIsrContext returns FALSE after target init with
 *         zero nesting depth.
 */
void test_Os_Port_Stm32_isr_context_reports_false_when_not_in_isr(void)
{
    Os_PortTargetInit();

    TEST_ASSERT_FALSE(Os_PortIsInIsrContext());
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * @requirement The port shall report ISR context as active after
 *              Os_PortEnterIsr2 increments the nesting counter.
 * @verify Os_PortIsInIsrContext returns TRUE between EnterIsr2 and ExitIsr2.
 */
void test_Os_Port_Stm32_isr_context_reports_true_inside_isr2(void)
{
    Os_PortTargetInit();

    Os_PortEnterIsr2();
    TEST_ASSERT_TRUE(Os_PortIsInIsrContext());

    Os_PortExitIsr2();
    TEST_ASSERT_FALSE(Os_PortIsInIsrContext());
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * @requirement The port shall report ISR context as active at every
 *              nesting level until the outermost ISR exits.
 * @verify Os_PortIsInIsrContext stays TRUE through nested EnterIsr2 calls
 *         and returns FALSE only after the final ExitIsr2.
 */
void test_Os_Port_Stm32_isr_context_stays_true_through_nested_isrs(void)
{
    Os_PortTargetInit();

    Os_PortEnterIsr2();
    Os_PortEnterIsr2();
    TEST_ASSERT_TRUE(Os_PortIsInIsrContext());

    Os_PortExitIsr2();
    TEST_ASSERT_TRUE(Os_PortIsInIsrContext());

    Os_PortExitIsr2();
    TEST_ASSERT_FALSE(Os_PortIsInIsrContext());
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Os_Port_Stm32_isr_context_reports_false_when_not_in_isr);
    RUN_TEST(test_Os_Port_Stm32_isr_context_reports_true_inside_isr2);
    RUN_TEST(test_Os_Port_Stm32_isr_context_stays_true_through_nested_isrs);

    return UNITY_END();
}
