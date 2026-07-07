/**
 * @file    test_Os_Port_Tms570_bootstrap_core_dispatch.c
 * @brief   Core dispatch/config tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 port shall request deferred dispatch
 *              only after the first task has started.
 * @verify RequestContextSwitch sets the dispatch request only in running
 *         state and ignores repeated requests while already pending.
 */
void test_Os_Port_Tms570_request_context_switch_sets_dispatch_after_start(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    Os_PortRequestContextSwitch();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();
    Os_PortRequestContextSwitch();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);

    Os_PortRequestContextSwitch();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement The shared configured-dispatch binding shall reject a task
 *              that exists in OS config but does not yet have a prepared
 *              TMS570 task context.
 * @verify RequestConfiguredDispatch returns E_OS_VALUE and leaves deferred
 *         dispatch state unchanged for an unprepared configured task.
 */
void test_Os_Port_Tms570_request_configured_dispatch_rejects_unprepared_task_context(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_PrepareConfiguredFirstTask(OS_PORT_TMS570_FIRST_TASK_ID, stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The shared configured-dispatch binding shall select the
 *              prepared configured task and raise a deferred dispatch through
 *              the TMS570 port contract.
 * @verify RequestConfiguredDispatch records the selected task and marks an
 *         active dispatch request once the first task is running.
 */
void test_Os_Port_Tms570_request_configured_dispatch_selects_task_and_requests_dispatch(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_stack_build.S
 * @requirement The bootstrap TMS570 port shall be able to prepare task
 *              contexts directly from the configured OSEK task model rather
 *              than from ad-hoc test entry pointers.
 * @verify Configured tasks from Os_TestConfigureTasks are bound into both
 *         first-task and secondary prepared TMS570 task contexts.
 */
void test_Os_Port_Tms570_task_binding_prepares_contexts_from_configured_tasks(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(os_port_tms570_binding_tasks, (uint8)(sizeof(os_port_tms570_binding_tasks) /
                                                                    sizeof(os_port_tms570_binding_tasks[0]))));
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));

    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->FirstTaskTaskID);
    TEST_ASSERT_TRUE(first_ctx->Prepared);
    TEST_ASSERT_TRUE(second_ctx->Prepared);
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry) == ((uintptr_t)first_ctx->Entry));
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry_alt) == ((uintptr_t)second_ctx->Entry));
}

void test_Os_Port_Tms570_RegisterCoreDispatchTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_request_context_switch_sets_dispatch_after_start);
    RUN_TEST(test_Os_Port_Tms570_request_configured_dispatch_rejects_unprepared_task_context);
    RUN_TEST(test_Os_Port_Tms570_request_configured_dispatch_selects_task_and_requests_dispatch);
    RUN_TEST(test_Os_Port_Tms570_task_binding_prepares_contexts_from_configured_tasks);
}
