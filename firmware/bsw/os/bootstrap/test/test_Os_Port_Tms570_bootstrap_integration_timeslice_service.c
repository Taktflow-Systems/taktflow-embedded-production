/**
 * @file    test_Os_Port_Tms570_bootstrap_integration_timeslice_service.c
 * @brief   Tick time-slice service tests for the TMS570 bootstrap port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec Local ThreadX reference: ports/arm11/gnu/src/tx_timer_interrupt.S
 * @requirement The bootstrap TMS570 tick ISR shall keep the separate
 *              time-slice service hook idle until the active running-task
 *              time slice actually expires.
 * @verify TickIsr decrements a non-final active slice without setting a
 *         pending service flag or incrementing the service count.
 */
void test_Os_Port_Tms570_tick_isr_keeps_time_slice_service_idle_before_expiry(void)
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

    TEST_ASSERT_EQUAL_UINT32(1u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TimeSliceServiceCount);
    TEST_ASSERT_FALSE(state->TimeSliceServicePending);
}

/**
 * @spec Local ThreadX reference: ports/arm11/gnu/src/tx_timer_interrupt.S
 * @requirement The bootstrap TMS570 tick ISR shall model the separate
 *              post-expiry time-slice service hook once per slice expiry
 *              without re-firing it on later ticks after the pending work
 *              has already been serviced.
 * @verify TickIsr services a just-expired slice once, clears the pending
 *         service flag, and leaves later zero-slice ticks without another
 *         service call.
 */
void test_Os_Port_Tms570_tick_isr_services_time_slice_expiry_once(void)
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
    TEST_ASSERT_EQUAL_UINT32(0u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TimeSliceExpirationCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TimeSliceServiceCount);
    TEST_ASSERT_FALSE(state->TimeSliceServicePending);

    Os_Port_Tms570_TickIsr();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(1u, state->TimeSliceExpirationCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TimeSliceServiceCount);
    TEST_ASSERT_FALSE(state->TimeSliceServicePending);
}

/**
 * @spec Local ThreadX reference: common/src/tx_thread_time_slice.c
 * @requirement The bootstrap TMS570 time-slice service seam shall reload the
 *              running task's configured next slice budget before any future
 *              scheduler-rotation modeling is considered.
 * @verify TickIsr expires the active slice, services it once, and reloads
 *         the running task's configured saved time slice into the live
 *         current time-slice slot.
 */
void test_Os_Port_Tms570_tick_isr_service_reloads_running_task_saved_time_slice(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetTaskSavedTimeSlice(
                          OS_PORT_TMS570_FIRST_TASK_ID, 4u));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(1u));

    Os_Port_Tms570_TickIsr();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TimeSliceExpirationCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TimeSliceServiceCount);
    TEST_ASSERT_EQUAL_UINT32(4u, state->CurrentTimeSlice);
    TEST_ASSERT_FALSE(state->TimeSliceServicePending);
    TEST_ASSERT_FALSE(state->DispatchRequested);
}

void test_Os_Port_Tms570_RegisterIntegrationTimeSliceServiceTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_tick_isr_keeps_time_slice_service_idle_before_expiry);
    RUN_TEST(test_Os_Port_Tms570_tick_isr_services_time_slice_expiry_once);
    RUN_TEST(test_Os_Port_Tms570_tick_isr_service_reloads_running_task_saved_time_slice);
}
