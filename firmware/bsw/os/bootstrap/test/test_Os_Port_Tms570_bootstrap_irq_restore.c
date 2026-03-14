/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restore.c
 * @brief   IRQ restore-path tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_irq_nesting_start.S
 * @requirement The bootstrap TMS570 port shall keep the IRQ-mode banked
 *              return address separate from the system-mode processing return
 *              address while `irq_nesting_start/end` switches execution mode.
 * @verify The outer IRQ save records the IRQ banked return, nesting start
 *         switches into system mode without losing it, nesting end returns to
 *         IRQ mode, and final restore consumes that IRQ return and returns the
 *         mode model to thread execution.
 */
void test_Os_Port_Tms570_irq_nesting_start_keeps_irq_banked_return_separate_from_system_mode_return(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x12345670u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));

    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_IRQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->LastSavedIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqNestingStart();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentIrqProcessingReturnAddress);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xBBBBBBB0u));
    Os_Port_Tms570_IrqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_IRQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->LastRestoredIrqReturnAddress);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_context_restore.S
 * @requirement A nested TMS570 IRQ restore shall return from IRQ mode back
 *              into system-mode IRQ processing using the nested IRQ banked
 *              return address, while preserving the outer IRQ return until the
 *              later outer restore.
 * @verify Nested restore consumes the nested IRQ return and restores system
 *         mode, then the outer restore later consumes the original outer IRQ
 *         return address.
 */
void test_Os_Port_Tms570_nested_irq_restore_returns_to_system_mode_with_nested_irq_return_address(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x11111110u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));
    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x22222220u));
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_IRQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x22222220u, (void*)state->CurrentIrqReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NESTED_RETURN, state->LastRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x22222220u, (void*)state->LastRestoredIrqReturnAddress);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);

    Os_Port_Tms570_IrqNestingEnd();
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->LastRestoredIrqReturnAddress);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 port shall expose the runtime SP that a
 *              future assembly restore path should use for the current or
 *              selected task.
 * @verify PeekRestoreTaskSp returns the current task runtime SP when no
 *         switch is pending and the selected task runtime SP when dispatch
 *         has armed a handoff.
 */
void test_Os_Port_Tms570_peek_restore_task_sp_prefers_selected_task_runtime_sp(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeSp, (void*)Os_Port_Tms570_PeekRestoreTaskSp());

    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->RuntimeSp, (void*)Os_Port_Tms570_PeekRestoreTaskSp());
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 restore helper shall distinguish the
 *              nested-return path from the final IRQ-return path.
 * @verify PeekRestoreAction reports nested return while more than one IRQ
 *         context is active, and the first matched restore records that
 *         nested-return action.
 */
void test_Os_Port_Tms570_peek_restore_action_reports_nested_return_path(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqContextSave();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NESTED_RETURN,
                            Os_Port_Tms570_PeekRestoreAction());

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NESTED_RETURN, state->LastRestoreAction);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 restore helper shall distinguish a final
 *              IRQ return that resumes the interrupted task from one that
 *              switches to a different prepared task.
 * @verify Final restore with no pending handoff reports and records the
 *         resume-current action.
 */
void test_Os_Port_Tms570_irq_restore_records_resume_current_action(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT,
                            Os_Port_Tms570_PeekRestoreAction());

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 final-restore path shall treat a pending
 *              handoff to the already-running task as resume-current work,
 *              not as a real task switch.
 * @verify Final restore clears the same-task dispatch request, records the
 *         resume-current action, and leaves task-switch count unchanged.
 */
void test_Os_Port_Tms570_resume_current_restore_clears_same_task_dispatch_request(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT,
                            Os_Port_Tms570_PeekRestoreAction());

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 restore helper shall report a final IRQ
 *              return that will switch to a selected prepared task.
 * @verify Final restore sees a deferred higher-priority handoff as a
 *         switch-task action before the IRQ exit converts that handoff into
 *         an active dispatch request.
 */
void test_Os_Port_Tms570_peek_restore_action_reports_switch_task_path(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK,
                            Os_Port_Tms570_PeekRestoreAction());

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
}

void test_Os_Port_Tms570_RegisterIrqRestoreTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_nesting_start_keeps_irq_banked_return_separate_from_system_mode_return);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_restore_returns_to_system_mode_with_nested_irq_return_address);
    RUN_TEST(test_Os_Port_Tms570_peek_restore_task_sp_prefers_selected_task_runtime_sp);
    RUN_TEST(test_Os_Port_Tms570_peek_restore_action_reports_nested_return_path);
    RUN_TEST(test_Os_Port_Tms570_irq_restore_records_resume_current_action);
    RUN_TEST(test_Os_Port_Tms570_resume_current_restore_clears_same_task_dispatch_request);
    RUN_TEST(test_Os_Port_Tms570_peek_restore_action_reports_switch_task_path);
}
