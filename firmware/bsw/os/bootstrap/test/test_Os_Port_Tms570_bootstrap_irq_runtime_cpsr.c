/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_runtime_cpsr.c
 * @brief   IRQ runtime CPSR tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_runtime_frame_captures_outermost_irq_saved_cpsr(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x60000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp((uintptr_t)(&first_stack[120])));

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INITIAL_CPSR, first_ctx->InitialFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(0x60000013u, first_ctx->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(0x60000013u, Os_Port_Tms570_PeekRestoreTaskCpsr());
}

void test_Os_Port_Tms570_nested_irq_save_does_not_overwrite_outer_runtime_cpsr(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x60000013u));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x20000013u));
    Os_Port_Tms570_IrqContextSave();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(0x60000013u, first_ctx->RuntimeFrame.Cpsr);

    Os_Port_Tms570_IrqContextRestore();
    Os_Port_Tms570_IrqContextRestore();
}

void test_Os_Port_Tms570_RegisterIrqRuntimeCpsrTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_runtime_frame_captures_outermost_irq_saved_cpsr);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_save_does_not_overwrite_outer_runtime_cpsr);
}
