/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_lifecycle.c
 * @brief   FIQ lifecycle tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

static uint8 fiq_handler_runs;
static uint8 fiq_handler_observed_mode;

static void fiq_handler_observe_mode(void)
{
    fiq_handler_runs++;
    fiq_handler_observed_mode = Os_Port_Tms570_GetBootstrapState()->CurrentExecutionMode;
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_start.S
 * @requirement The bootstrap TMS570 port shall track balanced FIQ nesting
 *              depth independently of IRQ bookkeeping while restoring the
 *              pre-FIQ execution mode on final FIQ exit.
 * @verify EnterFiq and ExitFiq update only FIQ nesting depth, leave IRQ
 *         nesting unchanged, and return to thread mode on the final exit.
 */
void test_Os_Port_Tms570_fiq_nesting_tracks_balanced_entry_exit(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x33333330u));
    Os_Port_Tms570_EnterFiq();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x44444440u));
    Os_Port_Tms570_EnterFiq();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(2u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_FIQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x44444440u, (void*)state->LastSavedFiqReturnAddress);

    Os_Port_Tms570_ExitFiq();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_FIQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x44444440u, (void*)state->LastRestoredFiqReturnAddress);

    Os_Port_Tms570_ExitFiq();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x33333330u, (void*)state->LastRestoredFiqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentFiqReturnAddress);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_nesting_start.S
 * @requirement The bootstrap TMS570 FIQ path shall restore the pre-FIQ
 *              execution mode when the final FIQ nesting level exits.
 * @verify If FIQ interrupts while the bootstrap is in system-mode IRQ
 *         processing, final FIQ exit returns to system mode rather than
 *         thread mode or IRQ mode.
 */
void test_Os_Port_Tms570_fiq_exit_restores_previous_system_mode(void)
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
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x55555550u));
    Os_Port_Tms570_FiqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_FIQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->FiqResumeMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x55555550u, (void*)state->LastSavedFiqReturnAddress);

    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->FiqResumeMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x55555550u, (void*)state->LastRestoredFiqReturnAddress);

    Os_Port_Tms570_IrqNestingEnd();
    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_save.S
 * @requirement The bootstrap TMS570 FIQ handler-entry seam shall run the
 *              same save-then-processing-start lifecycle as the explicit
 *              ThreadX FIQ context-save plus nesting-start split.
 * @verify FiqProcessingStart enters FIQ context, switches into system-mode
 *         processing, and records both the FIQ save action and the nesting
 *         start bookkeeping.
 */
void test_Os_Port_Tms570_fiq_processing_start_enters_system_mode_through_split_seam(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x12345670u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));

    Os_Port_Tms570_FiqProcessingStart();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY, state->LastFiqSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_CONTINUE_PROCESSING,
                            state->LastFiqSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->LastSavedFiqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->LastSavedFiqProcessingReturnAddress);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_save.S
 * @requirement The bootstrap TMS570 test FIQ invoke helper shall execute a
 *              handler inside the same FIQ save -> processing -> restore
 *              lifecycle that the split FIQ bootstrap path models.
 * @verify TestInvokeFiq runs the handler in system mode, then returns the
 *         bootstrap state to thread mode with balanced FIQ bookkeeping.
 */
void test_Os_Port_Tms570_test_invoke_fiq_runs_handler_inside_system_mode_and_balances_state(void)
{
    const Os_Port_Tms570_StateType* state;

    fiq_handler_runs = 0u;
    fiq_handler_observed_mode = 0xFFu;
    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x12345670u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestInvokeFiq(fiq_handler_observe_mode));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(1u, fiq_handler_runs);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, fiq_handler_observed_mode);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->LastRestoredFiqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->LastRestoredFiqProcessingReturnAddress);
}

void test_Os_Port_Tms570_RegisterFiqLifecycleTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_nesting_tracks_balanced_entry_exit);
    RUN_TEST(test_Os_Port_Tms570_fiq_exit_restores_previous_system_mode);
    RUN_TEST(test_Os_Port_Tms570_fiq_processing_start_enters_system_mode_through_split_seam);
    RUN_TEST(test_Os_Port_Tms570_test_invoke_fiq_runs_handler_inside_system_mode_and_balances_state);
}
