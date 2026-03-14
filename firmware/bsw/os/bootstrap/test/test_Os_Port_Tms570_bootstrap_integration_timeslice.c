/**
 * @file    test_Os_Port_Tms570_bootstrap_integration_timeslice.c
 * @brief   Tick time-slice integration tests for the TMS570 bootstrap port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec Local ThreadX reference: ports/arm11/gnu/src/tx_timer_interrupt.S
 * @requirement The bootstrap TMS570 tick ISR shall count down an active
 *              running-task time slice on each tick before any future
 *              round-robin scheduling work is modeled.
 * @verify TickIsr decrements the live time slice by one tick, leaves the
 *         expiry count unchanged, and does not request dispatch when no
 *         alarm expires on that tick.
 */
void test_Os_Port_Tms570_tick_isr_decrements_active_time_slice_without_expiry(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(2u));

    Os_Port_Tms570_TickIsr();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_EQUAL_UINT32(1u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TimeSliceExpirationCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
}

/**
 * @spec Local ThreadX reference: ports/arm11/gnu/src/tx_timer_interrupt.S
 * @requirement The bootstrap TMS570 tick ISR shall record that a running
 *              time slice reached zero without overclaiming a full ThreadX
 *              scheduler handoff that is not modeled yet in this bootstrap.
 * @verify TickIsr decrements the final slice tick to zero, increments the
 *         time-slice expiration counter, and still leaves dispatch idle
 *         when no alarm expiry or ready-task transition occurs.
 */
void test_Os_Port_Tms570_tick_isr_records_time_slice_expiry_without_forcing_dispatch(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(1u));

    Os_Port_Tms570_TickIsr();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_EQUAL_UINT32(0u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TimeSliceExpirationCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
}

void test_Os_Port_Tms570_RegisterIntegrationTimeSliceTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_tick_isr_decrements_active_time_slice_without_expiry);
    RUN_TEST(test_Os_Port_Tms570_tick_isr_records_time_slice_expiry_without_forcing_dispatch);
}
