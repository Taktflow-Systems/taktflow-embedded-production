/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_restored_task_frame_view.c
 * @brief   FIQ/task restored frame-view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_resume_exposes_packed_restored_task_frame_view(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_TaskFrameViewType* restored;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {211u, 212u, 213u, 214u, 2110u, 2112u};
    const Os_Port_Tms570_IrqScratchSnapshotType corrupted = {221u, 222u, 223u, 224u, 2210u, 2212u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x31323334u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x7E100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&saved));
    Os_Port_Tms570_FiqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedReturnAddress(OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)0x41424344u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedCpsr(OS_PORT_TMS570_FIRST_TASK_ID, 0x7E200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedIrqScratch(OS_PORT_TMS570_FIRST_TASK_ID, &corrupted));
    Os_Port_Tms570_FiqContextRestore();

    restored = Os_Port_Tms570_PeekRestoredTaskFrameView();
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_TRUE(restored->Frame.Valid);
    TEST_ASSERT_EQUAL_UINT32(0x31323334u, (uint32)restored->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x7E100013u, restored->Frame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(saved.R0, restored->Frame.IrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, restored->Frame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(0u, restored->Frame.IrqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES, restored->FrameBytes);
}

void test_Os_Port_Tms570_RegisterFiqRestoredTaskFrameViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_resume_exposes_packed_restored_task_frame_view);
}
