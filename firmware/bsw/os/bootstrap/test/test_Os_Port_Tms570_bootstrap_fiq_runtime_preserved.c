/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_runtime_preserved.c
 * @brief   FIQ preserved-register runtime-frame tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_scheduler_return_captures_outgoing_preserved_registers(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_PreservedSnapshotType preserved = {14u, 15u, 16u, 17u, 18u, 19u, 110u, 111u, 112u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&preserved));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x78787870u));

    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextRestore();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(14u, first_ctx->RuntimeFrame.Preserved.R4);
    TEST_ASSERT_EQUAL_UINT32(15u, first_ctx->RuntimeFrame.Preserved.R5);
    TEST_ASSERT_EQUAL_UINT32(16u, first_ctx->RuntimeFrame.Preserved.R6);
    TEST_ASSERT_EQUAL_UINT32(17u, first_ctx->RuntimeFrame.Preserved.R7);
    TEST_ASSERT_EQUAL_UINT32(18u, first_ctx->RuntimeFrame.Preserved.R8);
    TEST_ASSERT_EQUAL_UINT32(19u, first_ctx->RuntimeFrame.Preserved.R9);
    TEST_ASSERT_EQUAL_UINT32(110u, first_ctx->RuntimeFrame.Preserved.R10);
    TEST_ASSERT_EQUAL_UINT32(111u, first_ctx->RuntimeFrame.Preserved.R11);
    TEST_ASSERT_EQUAL_UINT32(112u, first_ctx->RuntimeFrame.Preserved.R12);
}

void test_Os_Port_Tms570_RegisterFiqRuntimePreservedTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_scheduler_return_captures_outgoing_preserved_registers);
}
