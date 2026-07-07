/**
 * @file    test_Os_Port_Tms570_bootstrap_integration_scheduler.c
 * @brief   Scheduler bridge integration tests for the TMS570 bootstrap port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement The portable bootstrap scheduler shall publish the configured
 *              task it dispatches into the TMS570 port observation seam.
 * @verify A host-side StartOS plus ActivateTask dispatch increments the
 *         observed kernel dispatch count and records the dispatched task ID.
 */
void test_Os_Port_Tms570_kernel_scheduler_publishes_dispatch_to_port_state(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(1u, dummy_task_runs);
    TEST_ASSERT_EQUAL_UINT32(1u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastObservedKernelTask);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement When the TMS570 bootstrap port already owns a prepared and
 *              started first task, the portable scheduler shall synchronize
 *              that current task without leaving a stale selected-next-task.
 * @verify A first portable dispatch with an already-started prepared task
 *         keeps CurrentTask aligned and leaves no pending next-task latch.
 */
void test_Os_Port_Tms570_kernel_first_dispatch_synchronizes_started_task_without_stale_selection(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastObservedKernelTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement A portable-kernel preemption from one configured task to a
 *              higher-priority configured task shall arm the TMS570 port
 *              handoff seam when both target contexts are prepared.
 * @verify Dispatching a low task that activates a higher-priority task
 *         causes the scheduler to request IRQ-return dispatch for the
 *         prepared high task.
 */
void test_Os_Port_Tms570_kernel_preemption_arms_target_handoff_for_prepared_task(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
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

    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(2u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->LastObservedKernelTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement A scheduler-driven configured-task preemption shall complete
 *              through the TMS570 IRQ-return restore path once the deferred
 *              port handler runs.
 * @verify After portable-kernel preemption arms the handoff, the shared
 *         completion helper drives IRQ-return restore, switches the
 *         bootstrap current task to the prepared high task, and clears the
 *         selected-next-task state.
 */
void test_Os_Port_Tms570_kernel_preemption_completes_through_irq_restore(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
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
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_schedule.S
 * @requirement The bootstrap TMS570 dispatch-completion path shall load the
 *              selected task's saved time slice into the live current
 *              time-slice slot when that task becomes current.
 * @verify A prepared high task with a configured saved time slice becomes the
 *         current task through IRQ-return dispatch and its saved slice becomes
 *         the live current time slice.
 */
void test_Os_Port_Tms570_kernel_preemption_restores_selected_task_time_slice(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetTaskSavedTimeSlice(
                          OS_PORT_TMS570_SECOND_TASK_ID, 7u));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Tms570_GetBootstrapState();
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(7u, second_ctx->SavedTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(7u, state->CurrentTimeSlice);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 IRQ-return dispatch path shall save the
 *              outgoing task's live current time slice into that task context
 *              before switching to the selected next task.
 * @verify A running low task with a live time slice saves that slice into its
 *         task context during IRQ-return dispatch, while the incoming high
 *         task's saved slice becomes the new live current time slice.
 */
void test_Os_Port_Tms570_kernel_preemption_saves_outgoing_task_time_slice(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetTaskSavedTimeSlice(
                          OS_PORT_TMS570_SECOND_TASK_ID, 7u));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(4u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_UINT32(4u, state->LastSavedTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(4u, first_ctx->SavedTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(7u, second_ctx->SavedTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(7u, state->CurrentTimeSlice);
}

void test_Os_Port_Tms570_RegisterIntegrationSchedulerTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_kernel_scheduler_publishes_dispatch_to_port_state);
    RUN_TEST(test_Os_Port_Tms570_kernel_first_dispatch_synchronizes_started_task_without_stale_selection);
    RUN_TEST(test_Os_Port_Tms570_kernel_preemption_arms_target_handoff_for_prepared_task);
    RUN_TEST(test_Os_Port_Tms570_kernel_preemption_completes_through_irq_restore);
    RUN_TEST(test_Os_Port_Tms570_kernel_preemption_restores_selected_task_time_slice);
    RUN_TEST(test_Os_Port_Tms570_kernel_preemption_saves_outgoing_task_time_slice);
}
