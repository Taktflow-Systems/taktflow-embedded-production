/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restored_task_frame_view.c
 * @brief   IRQ/task restored frame-view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_resume_exposes_packed_restored_task_frame_view(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_TaskFrameViewType* restored;
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {191u, 192u, 193u, 194u, 1910u, 1912u};
    const Os_Port_Tms570_IrqScratchSnapshotType corrupted = {201u, 202u, 203u, 204u, 2010u, 2012u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x11121314u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x7D100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedReturnAddress(OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)0x21222324u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedCpsr(OS_PORT_TMS570_FIRST_TASK_ID, 0x7D200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedIrqScratch(OS_PORT_TMS570_FIRST_TASK_ID, &corrupted));
    Os_Port_Tms570_IrqContextRestore();

    restored = Os_Port_Tms570_PeekRestoredTaskFrameView();
    TEST_ASSERT_NOT_NULL(restored);
    TEST_ASSERT_TRUE(restored->Frame.Valid);
    TEST_ASSERT_EQUAL_UINT32(0x11121314u, (uint32)restored->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x7D100013u, restored->Frame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(saved.R10, restored->Frame.IrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(saved.R12, restored->Frame.IrqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES, restored->FrameBytes);
}

void test_Os_Port_Tms570_RegisterIrqRestoredTaskFrameViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_resume_exposes_packed_restored_task_frame_view);
}
