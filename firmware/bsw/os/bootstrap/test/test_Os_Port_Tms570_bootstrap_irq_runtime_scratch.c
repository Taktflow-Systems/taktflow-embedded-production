/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_runtime_scratch.c
 * @brief   IRQ runtime scratch-register tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_runtime_frame_captures_outermost_irq_scratch_snapshot(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_IrqScratchSnapshotType scratch = {1u, 2u, 3u, 4u, 10u, 12u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp((uintptr_t)(&first_stack[120])));

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->InitialFrame.IrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(1u, first_ctx->RuntimeFrame.IrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(2u, first_ctx->RuntimeFrame.IrqScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(3u, first_ctx->RuntimeFrame.IrqScratch.R2);
    TEST_ASSERT_EQUAL_UINT32(4u, first_ctx->RuntimeFrame.IrqScratch.R3);
    TEST_ASSERT_EQUAL_UINT32(10u, first_ctx->RuntimeFrame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(12u, first_ctx->RuntimeFrame.IrqScratch.R12);
}

void test_Os_Port_Tms570_nested_irq_save_does_not_overwrite_outer_runtime_scratch_snapshot(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_IrqScratchSnapshotType outer = {11u, 12u, 13u, 14u, 110u, 112u};
    const Os_Port_Tms570_IrqScratchSnapshotType inner = {21u, 22u, 23u, 24u, 210u, 212u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&outer));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&inner));
    Os_Port_Tms570_IrqContextSave();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(11u, first_ctx->RuntimeFrame.IrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(112u, first_ctx->RuntimeFrame.IrqScratch.R12);

    Os_Port_Tms570_IrqContextRestore();
    Os_Port_Tms570_IrqContextRestore();
}

void test_Os_Port_Tms570_runtime_frame_allows_zero_irq_scratch_snapshot_to_overwrite_old_values(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_IrqScratchSnapshotType non_zero = {31u, 32u, 33u, 34u, 310u, 312u};
    const Os_Port_Tms570_IrqScratchSnapshotType zero = {0u, 0u, 0u, 0u, 0u, 0u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&non_zero));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&zero));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp((uintptr_t)(&first_stack[116])));

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->RuntimeFrame.IrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->RuntimeFrame.IrqScratch.R1);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->RuntimeFrame.IrqScratch.R2);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->RuntimeFrame.IrqScratch.R3);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->RuntimeFrame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->RuntimeFrame.IrqScratch.R12);
}

void test_Os_Port_Tms570_RegisterIrqRuntimeScratchTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_runtime_frame_captures_outermost_irq_scratch_snapshot);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_save_does_not_overwrite_outer_runtime_scratch_snapshot);
    RUN_TEST(test_Os_Port_Tms570_runtime_frame_allows_zero_irq_scratch_snapshot_to_overwrite_old_values);
}
