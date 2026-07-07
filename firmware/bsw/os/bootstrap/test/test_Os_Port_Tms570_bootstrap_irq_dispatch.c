/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_dispatch.c
 * @brief   IRQ dispatch-completion tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The shared configured-dispatch completion helper shall be
 *              safe to call when no TMS570 IRQ-return handoff is pending.
 * @verify CompleteConfiguredDispatch returns E_OS_NOFUNC and leaves the
 *         bootstrap dispatch state unchanged when no switch is pending.
 */
void test_Os_Port_Tms570_complete_configured_dispatch_returns_e_os_nofunc_when_idle(void)
{
    uint8 stack_storage[160];
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&stack_storage[160])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Port_CompleteConfiguredDispatch());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The shared TMS570 configured-dispatch completion helper shall
 *              synthesize the IRQ save/restore seam when a pending handoff
 *              exists outside an already-active IRQ context.
 * @verify CompleteConfiguredDispatch consumes the pending handoff through one
 *         matched IRQ save/restore pair and completes the task switch.
 */
void test_Os_Port_Tms570_complete_configured_dispatch_uses_irq_context_when_no_active_context(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
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
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqSchedulerReturnCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 IRQ restore helper shall ignore restore
 *              attempts that do not match an active saved IRQ context.
 * @verify IrqContextRestore with zero active context depth leaves dispatch,
 *         current task, and restore counters unchanged.
 */
void test_Os_Port_Tms570_irq_context_restore_without_save_is_ignored(void)
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
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NONE, state->LastRestoreAction);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 IRQ return path shall complete deferred
 *              dispatch only when the final matched IRQ context restore runs.
 * @verify Nested IrqContextSave calls keep dispatch deferred until the last
 *         IrqContextRestore, which then completes the selected task handoff.
 */
void test_Os_Port_Tms570_nested_irq_context_restore_completes_dispatch_only_on_final_restore(void)
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
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqNesting);
    TEST_ASSERT_TRUE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqNesting);
    TEST_ASSERT_TRUE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NESTED_RETURN, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(2u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(2u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqSchedulerReturnCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 IRQ save path shall capture the
 *              interrupted task and runtime SP only on the outermost IRQ
 *              context save, matching the local ThreadX nested-save split.
 * @verify A nested IrqContextSave leaves the outer captured task/SP intact,
 *         and the capture is cleared on final restore.
 */
void test_Os_Port_Tms570_nested_irq_save_captures_outermost_task_sp_once(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    uintptr_t outer_runtime_sp;
    uintptr_t inner_runtime_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    outer_runtime_sp = first_ctx->SavedSp - (uintptr_t)12u;
    inner_runtime_sp = first_ctx->SavedSp - (uintptr_t)28u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(outer_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(inner_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);

    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeFrame.Sp, (void*)state->IrqCapturedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)first_ctx->RuntimeSp);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeFrame.Sp, (void*)state->IrqCapturedTaskSp);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->IrqCapturedTaskSp);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement A TMS570 nested IRQ-driven dispatch shall preserve the
 *              outermost captured runtime SP as the saved context for the
 *              interrupted task.
 * @verify If outer IRQ entry captures one SP and nested IRQ activity changes
 *         CurrentTaskSp before dispatch is requested, final restore still
 *         derives LastSavedTaskSp from the outer captured SP instead of the
 *         nested live SP.
 */
void test_Os_Port_Tms570_nested_irq_dispatch_uses_outermost_captured_runtime_sp(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;
    uintptr_t outer_runtime_sp;
    uintptr_t nested_runtime_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    outer_runtime_sp = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID)->SavedSp - (uintptr_t)20u;
    nested_runtime_sp = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID)->SavedSp - (uintptr_t)40u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(outer_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(nested_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    Os_Port_Tms570_IrqContextRestore();
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->IrqCapturedTaskSp);
}

void test_Os_Port_Tms570_RegisterIrqDispatchTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_complete_configured_dispatch_returns_e_os_nofunc_when_idle);
    RUN_TEST(test_Os_Port_Tms570_complete_configured_dispatch_uses_irq_context_when_no_active_context);
    RUN_TEST(test_Os_Port_Tms570_irq_context_restore_without_save_is_ignored);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_context_restore_completes_dispatch_only_on_final_restore);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_save_captures_outermost_task_sp_once);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_dispatch_uses_outermost_captured_runtime_sp);
}
