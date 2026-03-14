/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_nested.c
 * @brief   Nested FIQ tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

static uint8 fiq_handler_runs;
static uint8 nested_fiq_handler_runs;
static uint8 fiq_handler_observed_mode;
static uint8 nested_fiq_handler_observed_mode;
static StatusType nested_fiq_invoke_status;

static void nested_fiq_handler(void)
{
    nested_fiq_handler_runs++;
    nested_fiq_handler_observed_mode = Os_Port_Tms570_GetBootstrapState()->CurrentExecutionMode;
}

static void fiq_handler_that_nests(void)
{
    fiq_handler_runs++;
    fiq_handler_observed_mode = Os_Port_Tms570_GetBootstrapState()->CurrentExecutionMode;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x22222220u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xBBBBBBB0u));
    nested_fiq_invoke_status = Os_Port_Tms570_TestInvokeFiq(nested_fiq_handler);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_restore.S
 * @requirement The bootstrap TMS570 FIQ return-address model shall restore
 *              nested FIQ banked return addresses in LIFO order without
 *              disturbing the outer pre-FIQ resume mode.
 * @verify Two nested FIQ saves restore the inner FIQ return first, keep FIQ
 *         mode active for the remaining outer save, and restore the outer FIQ
 *         return on the final exit back to thread mode.
 */
void test_Os_Port_Tms570_nested_fiq_restore_uses_banked_return_addresses_lifo(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x11111110u));
    Os_Port_Tms570_FiqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x22222220u));
    Os_Port_Tms570_FiqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(2u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(2u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_FIQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x22222220u, (void*)state->LastSavedFiqReturnAddress);

    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_FIQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x22222220u, (void*)state->LastRestoredFiqReturnAddress);

    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->LastRestoredFiqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentFiqReturnAddress);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_fiq_context_save.S
 * @requirement The bootstrap TMS570 test FIQ invoke helper shall preserve
 *              nested FIQ bookkeeping when a handler invokes another FIQ
 *              handler through the same lifecycle seam.
 * @verify A nested TestInvokeFiq call runs both handlers in system mode and
 *         leaves balanced nested FIQ save/restore and nesting-start/end
 *         counters with the outer return addresses restored last.
 */
void test_Os_Port_Tms570_test_invoke_fiq_supports_nested_handler_invocation(void)
{
    const Os_Port_Tms570_StateType* state;

    fiq_handler_runs = 0u;
    nested_fiq_handler_runs = 0u;
    fiq_handler_observed_mode = 0xFFu;
    nested_fiq_handler_observed_mode = 0xFFu;
    nested_fiq_invoke_status = E_OS_STATE;
    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x11111110u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestInvokeFiq(fiq_handler_that_nests));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK, nested_fiq_invoke_status);
    TEST_ASSERT_EQUAL_UINT8(1u, fiq_handler_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, nested_fiq_handler_runs);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, fiq_handler_observed_mode);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, nested_fiq_handler_observed_mode);
    TEST_ASSERT_EQUAL_UINT32(2u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(2u, state->FiqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(2u, state->FiqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(2u, state->FiqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->NestedFiqReturnCount);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->LastRestoredFiqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->LastRestoredFiqProcessingReturnAddress);
}

void test_Os_Port_Tms570_RegisterFiqNestedTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_nested_fiq_restore_uses_banked_return_addresses_lifo);
    RUN_TEST(test_Os_Port_Tms570_test_invoke_fiq_supports_nested_handler_invocation);
}
