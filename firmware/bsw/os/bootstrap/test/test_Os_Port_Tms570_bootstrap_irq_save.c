/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_save.c
 * @brief   IRQ save/nesting tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 port shall expose a save-current-SP hook
 *              that preserves the live running SP while also recording the
 *              outermost interrupted task's minimal saved-frame SP for the
 *              future assembly path.
 * @verify SaveCurrentTaskSp keeps the current task runtime SP live, records a
 *         minimal saved-frame SP on outermost use, and nested use leaves the
 *         outer capture intact.
 */
void test_Os_Port_Tms570_save_current_task_sp_updates_outermost_capture_only(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    uintptr_t outer_runtime_sp;
    uintptr_t nested_runtime_sp;
    uintptr_t expected_outer_saved_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    outer_runtime_sp = first_ctx->SavedSp - (uintptr_t)32u;
    nested_runtime_sp = first_ctx->SavedSp - (uintptr_t)48u;
    expected_outer_saved_sp = outer_runtime_sp - (uintptr_t)OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp(outer_runtime_sp));
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)expected_outer_saved_sp, (void*)state->IrqCapturedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)first_ctx->RuntimeSp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_outer_saved_sp, (void*)first_ctx->RuntimeFrame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_outer_saved_sp, (void*)first_ctx->SavedSp);

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp(nested_runtime_sp));
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)expected_outer_saved_sp, (void*)state->IrqCapturedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)first_ctx->RuntimeSp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_outer_saved_sp, (void*)first_ctx->RuntimeFrame.Sp);
    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 save helper shall distinguish the first
 *              IRQ entry that captures the running task from later nested IRQ
 *              entries that only extend nesting depth.
 * @verify The first save records capture-current, and a nested save records
 *         nested-IRQ without overwriting the outer captured task/SP.
 */
void test_Os_Port_Tms570_irq_context_save_records_capture_then_nested_actions(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CAPTURE_CURRENT,
                            Os_Port_Tms570_PeekSaveAction());
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CAPTURE_CURRENT, state->LastSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING,
                            state->LastSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedIrqReturnCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeFrame.Sp, (void*)state->IrqCapturedTaskSp);

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_NESTED_IRQ, Os_Port_Tms570_PeekSaveAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_NESTED_RETURN,
                            Os_Port_Tms570_PeekSaveContinuationAction());
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_NESTED_IRQ, state->LastSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_NESTED_RETURN,
                            state->LastSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->NestedIrqReturnCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeFrame.Sp, (void*)state->IrqCapturedTaskSp);

    Os_Port_Tms570_IrqContextRestore();
    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 save helper shall model the idle-system
 *              save branch when no task has started yet.
 * @verify Before first-task launch, save peeks and records idle-system, still
 *         enters IRQ bookkeeping, and does not capture a task stack pointer.
 */
void test_Os_Port_Tms570_irq_context_save_before_first_task_records_idle_system_action(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_IDLE_SYSTEM, Os_Port_Tms570_PeekSaveAction());
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_IDLE_SYSTEM, state->LastSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING,
                            state->LastSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedIrqReturnCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->IrqCapturedTaskSp);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_IDLE_SYSTEM, state->LastRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S
 * @requirement The bootstrap TMS570 port shall model the explicit transition
 *              from IRQ save into system-mode IRQ processing and back again
 *              before restore.
 * @verify A first-save path enters IRQ processing on nesting start, leaves it
 *         on nesting end, and keeps the IRQ context active until restore.
 */
void test_Os_Port_Tms570_irq_nesting_start_end_wrap_first_save_processing_phase(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0x11111110u));

    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->LastSavedIrqProcessingReturnAddress);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0x22222220u));

    Os_Port_Tms570_IrqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->CurrentIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->LastRestoredIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S
 * @requirement The bootstrap TMS570 nesting-start/end model shall restore
 *              saved processing return addresses in LIFO order across nested
 *              IRQ processing phases.
 * @verify Two nested processing phases save two different return addresses,
 *         and the inner end restores the inner one while the outer end
 *         restores the outer one.
 */
void test_Os_Port_Tms570_nested_irq_nesting_start_end_restores_processing_return_addresses_lifo(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));
    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xBBBBBBB0u));
    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(16u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(16u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0xBBBBBBB0u, (void*)state->LastSavedIrqProcessingReturnAddress);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xCCCCCCC0u));
    Os_Port_Tms570_IrqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0xBBBBBBB0u, (void*)state->CurrentIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xBBBBBBB0u, (void*)state->LastRestoredIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xDDDDDDD0u));
    Os_Port_Tms570_IrqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(16u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->LastRestoredIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
}

void test_Os_Port_Tms570_RegisterIrqSaveTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_save_current_task_sp_updates_outermost_capture_only);
    RUN_TEST(test_Os_Port_Tms570_irq_context_save_records_capture_then_nested_actions);
    RUN_TEST(test_Os_Port_Tms570_irq_context_save_before_first_task_records_idle_system_action);
    RUN_TEST(test_Os_Port_Tms570_irq_nesting_start_end_wrap_first_save_processing_phase);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_nesting_start_end_restores_processing_return_addresses_lifo);
}
