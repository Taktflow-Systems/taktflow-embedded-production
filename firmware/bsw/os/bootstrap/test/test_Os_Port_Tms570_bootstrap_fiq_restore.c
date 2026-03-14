/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_restore.c
 * @brief   FIQ final-restore tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_restore.S
 * @requirement The bootstrap TMS570 FIQ restore seam shall distinguish the
 *              idle-system first-entry return path from the normal running-task
 *              resume path, matching the local ThreadX scheduler return branch.
 * @verify Before the first task starts, FIQ restore reports idle-system action
 *         and returns the bootstrap execution mode to the scheduler-side system
 *         mode marker instead of the running-thread resume path.
 */
void test_Os_Port_Tms570_fiq_context_restore_before_first_task_returns_idle_system_action(void)
{
    const Os_Port_Tms570_StateType* state;
    uint8 restore_action;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x66666660u));
    Os_Port_Tms570_FiqContextSave();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM,
                            Os_Port_Tms570_PeekFiqRestoreAction());
    restore_action = Os_Port_Tms570_BeginFiqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM, restore_action);
    Os_Port_Tms570_FinishFiqContextRestore(restore_action);
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_IDLE_SYSTEM,
                            state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqSchedulerReturnCount);
    TEST_ASSERT_EQUAL_PTR((void*)0x66666660u, (void*)state->LastRestoredFiqReturnAddress);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_nesting_end.S
 * @requirement The bootstrap TMS570 FIQ handler-exit seam shall run the
 *              same processing-end-then-context-restore lifecycle as the
 *              explicit ThreadX nesting-end plus context-restore split.
 * @verify FiqProcessingEnd leaves system-mode processing, restores the
 *         saved processing return address, and returns to the pre-FIQ mode
 *         on the final FIQ restore.
 */
void test_Os_Port_Tms570_fiq_processing_end_restores_previous_mode_through_split_seam(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x12345670u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));
    Os_Port_Tms570_FiqProcessingStart();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xBBBBBBB0u));
    Os_Port_Tms570_FiqProcessingEnd();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE,
                            state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentFiqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->LastRestoredFiqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->LastRestoredFiqReturnAddress);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_restore.S
 * @requirement The bootstrap TMS570 FIQ restore seam shall distinguish a
 *              nested FIQ return from the final return to the pre-FIQ mode.
 * @verify Begin/Finish FIQ restore records nested-return on the inner exit
 *         and resume-previous-mode on the final exit.
 */
void test_Os_Port_Tms570_fiq_begin_finish_restore_records_nested_then_resume_actions(void)
{
    const Os_Port_Tms570_StateType* state;
    uint8 restore_action;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextSave();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_NESTED_RETURN,
                            Os_Port_Tms570_PeekFiqRestoreAction());
    restore_action = Os_Port_Tms570_BeginFiqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_NESTED_RETURN, restore_action);
    Os_Port_Tms570_FinishFiqContextRestore(restore_action);
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_NESTED_RETURN, state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_FIQ, state->CurrentExecutionMode);

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE,
                            Os_Port_Tms570_PeekFiqRestoreAction());
    restore_action = Os_Port_Tms570_BeginFiqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE, restore_action);
    Os_Port_Tms570_FinishFiqContextRestore(restore_action);
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE,
                            state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(2u, state->FiqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
}

void test_Os_Port_Tms570_RegisterFiqRestoreTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_context_restore_before_first_task_returns_idle_system_action);
    RUN_TEST(test_Os_Port_Tms570_fiq_processing_end_restores_previous_mode_through_split_seam);
    RUN_TEST(test_Os_Port_Tms570_fiq_begin_finish_restore_records_nested_then_resume_actions);
}
