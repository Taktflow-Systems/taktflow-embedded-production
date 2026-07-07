/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_runtime_context_sync.c
 * @brief   FIQ runtime-frame sync tests for packed saved context
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_finish_save_refreshes_runtime_frame_from_packed_context(void)
{
    const Os_Port_Tms570_TaskContextType* task;
    const Os_Port_Tms570_InterruptContextType* saved;
    const Os_Port_Tms570_IrqScratchSnapshotType begin = {91u, 92u, 93u, 94u, 910u, 912u};
    const Os_Port_Tms570_IrqScratchSnapshotType finish = {101u, 102u, 103u, 104u, 1010u, 1012u};
    uint8 save_action;

    Os_PortTargetInit();
    prepare_running_first_task_for_fiq_tests();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x6789ABCDu));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x78100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&begin));
    save_action = Os_Port_Tms570_BeginFiqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_SAVE_FIRST_ENTRY, save_action);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x789ABCDEu));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x78200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&finish));
    Os_Port_Tms570_FinishFiqContextSave(save_action);

    task = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    saved = Os_Port_Tms570_PeekSavedFiqContext();
    TEST_ASSERT_NOT_NULL(task);
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT_EQUAL_UINT32((uint32)saved->ReturnAddress, (uint32)task->RuntimeFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(saved->Cpsr, task->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(saved->Scratch.R10, task->RuntimeFrame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(saved->Scratch.R12, task->RuntimeFrame.IrqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(0x6789ABCDu, (uint32)task->RuntimeFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x78200013u, task->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(0u, task->RuntimeFrame.IrqScratch.R12);
}

void test_Os_Port_Tms570_RegisterFiqRuntimeContextSyncTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_finish_save_refreshes_runtime_frame_from_packed_context);
}
