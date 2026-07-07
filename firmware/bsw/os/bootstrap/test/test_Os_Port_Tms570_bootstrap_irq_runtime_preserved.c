/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_runtime_preserved.c
 * @brief   IRQ preserved-register runtime-frame tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_switch_captures_outgoing_preserved_registers_into_runtime_frame(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_PreservedSnapshotType preserved = {4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&preserved));

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(4u, first_ctx->RuntimeFrame.Preserved.R4);
    TEST_ASSERT_EQUAL_UINT32(5u, first_ctx->RuntimeFrame.Preserved.R5);
    TEST_ASSERT_EQUAL_UINT32(6u, first_ctx->RuntimeFrame.Preserved.R6);
    TEST_ASSERT_EQUAL_UINT32(7u, first_ctx->RuntimeFrame.Preserved.R7);
    TEST_ASSERT_EQUAL_UINT32(8u, first_ctx->RuntimeFrame.Preserved.R8);
    TEST_ASSERT_EQUAL_UINT32(9u, first_ctx->RuntimeFrame.Preserved.R9);
    TEST_ASSERT_EQUAL_UINT32(10u, first_ctx->RuntimeFrame.Preserved.R10);
    TEST_ASSERT_EQUAL_UINT32(11u, first_ctx->RuntimeFrame.Preserved.R11);
    TEST_ASSERT_EQUAL_UINT32(12u, first_ctx->RuntimeFrame.Preserved.R12);
}

void test_Os_Port_Tms570_RegisterIrqRuntimePreservedTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_switch_captures_outgoing_preserved_registers_into_runtime_frame);
}
