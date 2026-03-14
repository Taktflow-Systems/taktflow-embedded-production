/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_save.c
 * @brief   FIQ save/frame tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_save.S
 * @requirement The bootstrap TMS570 FIQ save seam shall distinguish the
 *              first FIQ entry from a nested FIQ entry before the future
 *              assembly path commits the save.
 * @verify Begin/Finish FIQ save records first-entry then nested-FIQ actions
 *         and updates the processing-versus-nested counters accordingly.
 */
void test_Os_Port_Tms570_fiq_begin_finish_save_records_first_then_nested_actions(void)
{
    const Os_Port_Tms570_StateType* state;
    uint8 save_action;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY, Os_Port_Tms570_PeekFiqSaveAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_CONTINUE_PROCESSING,
                            Os_Port_Tms570_PeekFiqSaveContinuationAction());
    save_action = Os_Port_Tms570_BeginFiqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY, save_action);
    Os_Port_Tms570_FinishFiqContextSave(save_action);
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY, state->LastFiqSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_CONTINUE_PROCESSING,
                            state->LastFiqSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedFiqReturnCount);

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_NESTED_FIQ, Os_Port_Tms570_PeekFiqSaveAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NESTED_RETURN,
                            Os_Port_Tms570_PeekFiqSaveContinuationAction());
    save_action = Os_Port_Tms570_BeginFiqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_NESTED_FIQ, save_action);
    Os_Port_Tms570_FinishFiqContextSave(save_action);
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_NESTED_FIQ, state->LastFiqSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_CONTINUE_NESTED_RETURN,
                            state->LastFiqSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(2u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(2u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->NestedFiqReturnCount);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_save.S
 * @requirement The bootstrap TMS570 FIQ save seam shall distinguish a
 *              running-thread first entry from an idle-system first entry.
 * @verify Before the first task starts, FIQ save reports idle-system action,
 *         keeps processing continuation semantics, and uses the minimal save
 *         frame size from the local ThreadX idle-system path.
 */
void test_Os_Port_Tms570_fiq_context_save_before_first_task_records_idle_system_action(void)
{
    const Os_Port_Tms570_StateType* state;
    uint8 save_action;

    Os_PortTargetInit();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_IDLE_SYSTEM, Os_Port_Tms570_PeekFiqSaveAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_CONTINUE_PROCESSING,
                            Os_Port_Tms570_PeekFiqSaveContinuationAction());

    save_action = Os_Port_Tms570_BeginFiqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_IDLE_SYSTEM, save_action);
    Os_Port_Tms570_FinishFiqContextSave(save_action);
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_IDLE_SYSTEM, state->LastFiqSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_CONTINUE_PROCESSING,
                            state->LastFiqSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES,
                             state->LastSavedFiqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES,
                             state->FiqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedFiqReturnCount);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_save.S
 * @requirement The bootstrap TMS570 FIQ save/restore model shall distinguish
 *              the minimal first-entry frame from the larger nested-FIQ scratch
 *              frame used by the local ThreadX reference.
 * @verify First-entry save tracks an 8-byte FIQ interrupt frame, nested save
 *         adds a 32-byte FIQ interrupt frame, and restore removes them in LIFO
 *         order while preserving the peak byte count.
 */
void test_Os_Port_Tms570_fiq_save_restore_tracks_minimal_then_nested_interrupt_stack_bytes(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    Os_Port_Tms570_FiqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES,
                             state->LastSavedFiqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES,
                             state->FiqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES,
                             state->FiqInterruptStackPeakBytes);

    Os_Port_Tms570_FiqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_NESTED_FRAME_BYTES,
                             state->LastSavedFiqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES +
                                 OS_PORT_TMS570_FIQ_NESTED_FRAME_BYTES,
                             state->FiqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES +
                                 OS_PORT_TMS570_FIQ_NESTED_FRAME_BYTES,
                             state->FiqInterruptStackPeakBytes);

    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_NESTED_FRAME_BYTES,
                             state->LastRestoredFiqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES,
                             state->FiqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES +
                                 OS_PORT_TMS570_FIQ_NESTED_FRAME_BYTES,
                             state->FiqInterruptStackPeakBytes);

    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES,
                             state->LastRestoredFiqContextBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqInterruptStackBytes);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES +
                                 OS_PORT_TMS570_FIQ_NESTED_FRAME_BYTES,
                             state->FiqInterruptStackPeakBytes);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_save.S
 * @requirement The bootstrap TMS570 first-entry FIQ save path shall capture
 *              the live running task stack pointer into the task context
 *              before switching into handler-side processing.
 * @verify A changed running-task SP is stored into the prepared task context
 *         when first-entry FIQ save begins on a running task.
 */
void test_Os_Port_Tms570_fiq_first_entry_save_captures_running_task_stack_pointer(void)
{
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    uintptr_t live_sp = (uintptr_t)0x20001230u;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(live_sp));

    Os_Port_Tms570_FiqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY, state->LastFiqSaveAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)live_sp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)live_sp, (void*)first_ctx->RuntimeSp);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextSaveCount);
}

void test_Os_Port_Tms570_RegisterFiqSaveTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_begin_finish_save_records_first_then_nested_actions);
    RUN_TEST(test_Os_Port_Tms570_fiq_context_save_before_first_task_records_idle_system_action);
    RUN_TEST(test_Os_Port_Tms570_fiq_save_restore_tracks_minimal_then_nested_interrupt_stack_bytes);
    RUN_TEST(test_Os_Port_Tms570_fiq_first_entry_save_captures_running_task_stack_pointer);
}
