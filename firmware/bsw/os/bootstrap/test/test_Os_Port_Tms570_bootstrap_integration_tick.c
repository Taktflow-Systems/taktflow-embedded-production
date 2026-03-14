/**
 * @file    test_Os_Port_Tms570_bootstrap_integration_tick.c
 * @brief   Tick/alarm integration tests for the TMS570 bootstrap port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec Local ThreadX reference: ports/arm11/gnu/src/tx_timer_interrupt.S
 * @requirement The bootstrap TMS570 tick ISR shall route into the OSEK
 *              counter/alarm path without asserting dispatch when no alarm
 *              expiry or ready-task transition occurs.
 * @verify TickIsr increments the bootstrap counter and leaves the
 *         dispatch-request state idle when no alarm expires on that tick.
 */
void test_Os_Port_Tms570_tick_isr_counts_ticks_without_spurious_dispatch_when_no_alarm_expires(void)
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
    Os_Port_Tms570_TickIsr();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/example_build/tx_initialize_low_level.S
 * @requirement The bootstrap TMS570 RTI interrupt entry shall route through
 *              the same timer path used by the low-level Cortex-R5 port
 *              structure and wake prepared configured tasks when an alarm
 *              expires.
 * @verify RtiTickHandler advances the counter, raises a dispatch request
 *         for the ready low task, and the shared run-to-idle helper then
 *         completes the prepared low-to-high handoff sequence.
 */
void test_Os_Port_Tms570_rti_tick_handler_routes_alarm_expiry_into_prepared_dispatch(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_TMS570_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));
    Os_Port_Tms570_RtiTickHandler();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_TestRunToIdle());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/example_build/tx_initialize_low_level.S
 * @requirement The bootstrap TMS570 RTI tick path shall defer dispatch
 *              while still inside nested IRQ processing and assert it on the
 *              final IRQ exit when an alarm wakes a prepared task.
 * @verify RtiTickHandler inside nested IRQ marks deferred dispatch for the
 *         alarm-ready low task, converts it to an active request on outer
 *         IRQ exit, and the shared run-to-idle helper then completes the
 *         prepared low-to-high handoff.
 */
void test_Os_Port_Tms570_rti_tick_inside_irq_defers_dispatch_until_exit(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_TMS570_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));

    Os_PortEnterIsr2();
    Os_PortEnterIsr2();
    Os_Port_Tms570_RtiTickHandler();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_TRUE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_high_runs);

    Os_PortExitIsr2();
    Os_PortExitIsr2();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);

    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec Local ThreadX reference: ports/arm11/gnu/src/tx_timer_interrupt.S
 * @requirement The shared bootstrap counter helper shall drain any deferred
 *              TMS570 configured-task handoff that alarm processing creates.
 * @verify An alarm-activated low task that readies a prepared high task
 *         leaves the port in the completed high-task state after a single
 *         Os_TestAdvanceCounter call.
 */
void test_Os_Port_Tms570_alarm_counter_helper_drains_deferred_dispatch_to_idle(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_TMS570_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));
    Os_TestAdvanceCounter(1u);
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

void test_Os_Port_Tms570_RegisterIntegrationTickTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_tick_isr_counts_ticks_without_spurious_dispatch_when_no_alarm_expires);
    RUN_TEST(test_Os_Port_Tms570_rti_tick_handler_routes_alarm_expiry_into_prepared_dispatch);
    RUN_TEST(test_Os_Port_Tms570_rti_tick_inside_irq_defers_dispatch_until_exit);
    RUN_TEST(test_Os_Port_Tms570_alarm_counter_helper_drains_deferred_dispatch_to_idle);
}
