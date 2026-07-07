/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restore_blocks.c
 * @brief   IRQ restore block-pointer tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_restore_block_helpers_follow_selected_runtime_frame(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_IrqScratchSnapshotType scratch = {5u, 6u, 7u, 8u, 15u, 17u};
    const Os_Port_Tms570_PreservedSnapshotType preserved = {24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u};
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareTaskContext(
            OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&preserved));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)&second_ctx->RuntimeFrame.IrqScratch,
                          (void*)Os_Port_Tms570_PeekRestoreTaskIrqScratch());
    TEST_ASSERT_EQUAL_PTR((void*)&second_ctx->RuntimeFrame.Preserved,
                          (void*)Os_Port_Tms570_PeekRestoreTaskPreserved());
}

void test_Os_Port_Tms570_RegisterIrqRestoreBlockTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_restore_block_helpers_follow_selected_runtime_frame);
}
