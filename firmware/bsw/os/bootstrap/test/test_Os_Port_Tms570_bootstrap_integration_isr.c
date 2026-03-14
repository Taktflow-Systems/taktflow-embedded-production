/**
 * @file    test_Os_Port_Tms570_bootstrap_integration_isr.c
 * @brief   ISR bridge integration tests for the TMS570 bootstrap port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S
 * @requirement A kernel Cat2 ISR preemption shall exercise the TMS570 IRQ
 *              context save/restore seam instead of only raw nesting
 *              bookkeeping.
 * @verify A low task that activates a high task from Os_TestInvokeIsrCat2
 *         completes the prepared handoff on IRQ restore and leaves no
 *         pending dispatch for the generic completion helper.
 */
void test_Os_Port_Tms570_isr2_preemption_flows_through_target_exit_and_irq_restore(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_isr_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_isr_bridge_tasks) /
                    sizeof(os_port_tms570_isr_bridge_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK, isr_bridge_invoke_status);
    TEST_ASSERT_EQUAL(E_OK, isr_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_bridge_high_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_TestCompletePortDispatches());
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S
 * @requirement The bootstrap TMS570 port shall defer dispatch assertion
 *              until the final IRQ exit when dispatch was requested inside
 *              nested IRQ processing.
 * @verify A nested IRQ request is remembered and converted to a dispatch
 *         request only on the outermost IRQ exit.
 */
void test_Os_Port_Tms570_nested_irq_exit_releases_deferred_dispatch(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();

    Os_PortEnterIsr2();
    Os_PortEnterIsr2();
    Os_PortRequestContextSwitch();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_TRUE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);

    Os_PortExitIsr2();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqNesting);

    Os_PortExitIsr2();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
}

void test_Os_Port_Tms570_RegisterIntegrationIsrTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_isr2_preemption_flows_through_target_exit_and_irq_restore);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_exit_releases_deferred_dispatch);
}
