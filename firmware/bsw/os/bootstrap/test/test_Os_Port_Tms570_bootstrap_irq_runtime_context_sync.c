/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_runtime_context_sync.c
 * @brief   IRQ runtime-frame sync tests for packed saved context
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_finish_save_refreshes_runtime_frame_from_packed_context(void)
{
    const Os_Port_Tms570_TaskContextType* task;
    const Os_Port_Tms570_InterruptContextType* saved;
    const Os_Port_Tms570_IrqScratchSnapshotType begin = {71u, 72u, 73u, 74u, 710u, 712u};
    const Os_Port_Tms570_IrqScratchSnapshotType finish = {81u, 82u, 83u, 84u, 810u, 812u};
    uint8 save_action;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x56789ABCu));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x77100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&begin));
    save_action = Os_Port_Tms570_BeginIrqContextSave((uintptr_t)0x20002000u);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CAPTURE_CURRENT, save_action);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x77200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&finish));
    Os_Port_Tms570_FinishIrqContextSave(save_action);

    task = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    saved = Os_Port_Tms570_PeekSavedIrqContext();
    TEST_ASSERT_NOT_NULL(task);
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT_EQUAL_HEX32(saved->Cpsr, task->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(saved->Scratch.R10, task->RuntimeFrame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(saved->Scratch.R12, task->RuntimeFrame.IrqScratch.R12);
    TEST_ASSERT_EQUAL_HEX32(0x77100013u, task->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(finish.R12, task->RuntimeFrame.IrqScratch.R12);
}

void test_Os_Port_Tms570_RegisterIrqRuntimeContextSyncTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_finish_save_refreshes_runtime_frame_from_packed_context);
}
