/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_processing.c
 * @brief   FIQ processing-phase tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_nesting_start.S
 * @requirement The bootstrap TMS570 port shall model the explicit transition
 *              from FIQ save into system-mode FIQ processing and back again
 *              before FIQ restore.
 * @verify A first FIQ save enters system-mode processing on nesting start,
 *         leaves it on nesting end, and keeps the FIQ context active until
 *         the later FIQ restore.
 */
void test_Os_Port_Tms570_fiq_nesting_start_end_wrap_first_save_processing_phase(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x11111110u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));

    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqNestingStart();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(8u, state->FiqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->FiqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedFiqReturnCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->LastSavedFiqProcessingReturnAddress);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xBBBBBBB0u));
    Os_Port_Tms570_FiqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->FiqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_FIQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentFiqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->LastRestoredFiqProcessingReturnAddress);

    Os_Port_Tms570_FiqContextRestore();
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_nesting_start.S
 * @requirement The bootstrap TMS570 FIQ processing seam shall model the local
 *              ThreadX rule that `fiq_nesting_start` enables nested FIQ during
 *              system-mode handler execution and `fiq_nesting_end` disables it
 *              again before restore.
 * @verify FiqProcessingStart enables FIQ processing interrupts, and
 *         FiqProcessingEnd disables them again while counting both transitions.
 */
void test_Os_Port_Tms570_fiq_processing_start_end_toggle_nested_fiq_enable_state(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x11111110u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));

    Os_Port_Tms570_FiqProcessingStart();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->FiqProcessingInterruptsEnabled);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqInterruptEnableCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqInterruptDisableCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);

    Os_Port_Tms570_FiqProcessingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->FiqProcessingInterruptsEnabled);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqInterruptEnableCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqInterruptDisableCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_nesting_start.S
 * @requirement The bootstrap TMS570 FIQ nesting-start/end model shall
 *              restore saved processing return addresses in LIFO order across
 *              nested FIQ processing phases.
 * @verify Two nested FIQ processing phases save two different return
 *         addresses, and the inner end restores the inner one while the
 *         outer end restores the outer one.
 */
void test_Os_Port_Tms570_nested_fiq_nesting_start_end_restores_processing_return_addresses_lifo(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x11111110u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));
    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqNestingStart();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x22222220u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xBBBBBBB0u));
    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqNestingStart();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(2u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, state->FiqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(16u, state->FiqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(16u, state->FiqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->NestedFiqReturnCount);
    TEST_ASSERT_EQUAL_PTR((void*)0xBBBBBBB0u, (void*)state->LastSavedFiqProcessingReturnAddress);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xCCCCCCC0u));
    Os_Port_Tms570_FiqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(8u, state->FiqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_FIQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0xBBBBBBB0u, (void*)state->CurrentFiqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xBBBBBBB0u, (void*)state->LastRestoredFiqProcessingReturnAddress);

    Os_Port_Tms570_FiqContextRestore();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xDDDDDDD0u));
    Os_Port_Tms570_FiqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(16u, state->FiqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentFiqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->LastRestoredFiqProcessingReturnAddress);

    Os_Port_Tms570_FiqContextRestore();
}

void test_Os_Port_Tms570_RegisterFiqProcessingTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_nesting_start_end_wrap_first_save_processing_phase);
    RUN_TEST(test_Os_Port_Tms570_fiq_processing_start_end_toggle_nested_fiq_enable_state);
    RUN_TEST(test_Os_Port_Tms570_nested_fiq_nesting_start_end_restores_processing_return_addresses_lifo);
}
