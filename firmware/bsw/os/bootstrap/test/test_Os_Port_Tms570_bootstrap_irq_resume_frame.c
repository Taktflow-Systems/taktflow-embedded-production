/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_resume_frame.c
 * @brief   IRQ resume-current frame tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_resume_current_restore_reapplies_irq_frame_without_switch_payload(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_IrqScratchSnapshotType saved_scratch = {7u, 8u, 9u, 10u, 17u, 19u};
    const Os_Port_Tms570_IrqScratchSnapshotType dirty_scratch = {27u, 28u, 29u, 30u, 37u, 39u};
    const Os_Port_Tms570_PreservedSnapshotType dirty_preserved = {44u, 45u, 46u, 47u, 48u, 49u, 50u, 51u, 52u};
    const Os_Port_Tms570_TaskLowerSnapshotType dirty_lower = {71u, 72u, 73u, 74u};
    Os_Port_Tms570_VfpStateType dirty_vfp;
    uintptr_t dirty_lr = (uintptr_t)0x45464748u;
    uintptr_t saved_sp = (uintptr_t)(&first_stack[120]);
    uintptr_t expected_restored_sp = saved_sp - (uintptr_t)OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    memset(&dirty_vfp, 0, sizeof(dirty_vfp));
    dirty_vfp.Enabled = TRUE;
    dirty_vfp.Fpscr = 0xCAFEB001u;
    dirty_vfp.D[0].Low = 91u;
    dirty_vfp.D[0].High = 92u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(saved_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(5u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x10203040u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x60000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved_scratch));
    Os_Port_Tms570_IrqContextSave();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[96])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(99u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x20000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&dirty_scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&dirty_lower));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&dirty_preserved));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&dirty_vfp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister(dirty_lr));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT, state->LastRestoreAction);
    TEST_ASSERT_EQUAL_PTR((void*)expected_restored_sp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_UINT32(0x60000013u, state->CurrentIrqSavedCpsr);
    TEST_ASSERT_EQUAL_UINT32(saved_scratch.R0, state->CurrentIrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(saved_scratch.R12, state->CurrentIrqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(99u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(dirty_lower.R0, state->CurrentTaskLower.R0);
    TEST_ASSERT_EQUAL_UINT32(dirty_lower.R3, state->CurrentTaskLower.R3);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R4, state->CurrentTaskPreserved.R4);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R10, state->CurrentTaskPreserved.R10);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R11, state->CurrentTaskPreserved.R11);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R12, state->CurrentTaskPreserved.R12);
    TEST_ASSERT_TRUE(state->CurrentTaskVfp.Enabled);
    TEST_ASSERT_EQUAL_UINT32(dirty_vfp.Fpscr, state->CurrentTaskVfp.Fpscr);
    TEST_ASSERT_EQUAL_UINT32(dirty_vfp.D[0].High, state->CurrentTaskVfp.D[0].High);
    TEST_ASSERT_EQUAL_PTR((void*)dirty_lr, (void*)state->CurrentTaskLinkRegister);
}

void test_Os_Port_Tms570_same_task_dispatch_restore_keeps_resume_current_payload_rules(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_IrqScratchSnapshotType saved_scratch = {11u, 12u, 13u, 14u, 21u, 23u};
    const Os_Port_Tms570_IrqScratchSnapshotType dirty_scratch = {41u, 42u, 43u, 44u, 51u, 53u};
    const Os_Port_Tms570_PreservedSnapshotType dirty_preserved = {54u, 55u, 56u, 57u, 58u, 59u, 60u, 61u, 62u};
    const Os_Port_Tms570_TaskLowerSnapshotType dirty_lower = {81u, 82u, 83u, 84u};
    Os_Port_Tms570_VfpStateType dirty_vfp;
    uintptr_t dirty_lr = (uintptr_t)0x61626364u;
    uintptr_t saved_sp = (uintptr_t)(&first_stack[124]);
    uintptr_t expected_restored_sp = saved_sp - (uintptr_t)OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    memset(&dirty_vfp, 0, sizeof(dirty_vfp));
    dirty_vfp.Enabled = TRUE;
    dirty_vfp.Fpscr = 0xCAFEB002u;
    dirty_vfp.D[1].Low = 93u;
    dirty_vfp.D[1].High = 94u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(saved_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(6u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x55667788u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x50000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved_scratch));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[88])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(88u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x10000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&dirty_scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&dirty_lower));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&dirty_preserved));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&dirty_vfp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister(dirty_lr));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT, state->LastRestoreAction);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL_PTR((void*)expected_restored_sp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_UINT32(0x50000013u, state->CurrentIrqSavedCpsr);
    TEST_ASSERT_EQUAL_UINT32(saved_scratch.R2, state->CurrentIrqScratch.R2);
    TEST_ASSERT_EQUAL_UINT32(saved_scratch.R10, state->CurrentIrqScratch.R10);
    TEST_ASSERT_EQUAL_UINT32(88u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(dirty_lower.R1, state->CurrentTaskLower.R1);
    TEST_ASSERT_EQUAL_UINT32(dirty_lower.R2, state->CurrentTaskLower.R2);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R6, state->CurrentTaskPreserved.R6);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R10, state->CurrentTaskPreserved.R10);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R11, state->CurrentTaskPreserved.R11);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R12, state->CurrentTaskPreserved.R12);
    TEST_ASSERT_TRUE(state->CurrentTaskVfp.Enabled);
    TEST_ASSERT_EQUAL_UINT32(dirty_vfp.Fpscr, state->CurrentTaskVfp.Fpscr);
    TEST_ASSERT_EQUAL_UINT32(dirty_vfp.D[1].Low, state->CurrentTaskVfp.D[1].Low);
    TEST_ASSERT_EQUAL_PTR((void*)dirty_lr, (void*)state->CurrentTaskLinkRegister);
}

void test_Os_Port_Tms570_RegisterIrqResumeFrameTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_resume_current_restore_reapplies_irq_frame_without_switch_payload);
    RUN_TEST(test_Os_Port_Tms570_same_task_dispatch_restore_keeps_resume_current_payload_rules);
}
