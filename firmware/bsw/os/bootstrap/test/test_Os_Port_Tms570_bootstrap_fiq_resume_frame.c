/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_resume_frame.c
 * @brief   FIQ resume-current frame tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_resume_restore_reapplies_fiq_frame_without_switch_payload(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_IrqScratchSnapshotType saved_scratch = {61u, 62u, 63u, 64u, 71u, 73u};
    const Os_Port_Tms570_IrqScratchSnapshotType dirty_scratch = {81u, 82u, 83u, 84u, 91u, 93u};
    const Os_Port_Tms570_PreservedSnapshotType dirty_preserved = {104u, 105u, 106u, 107u, 108u, 109u, 110u, 111u, 112u};
    const Os_Port_Tms570_TaskLowerSnapshotType dirty_lower = {121u, 122u, 123u, 124u};
    Os_Port_Tms570_VfpStateType dirty_vfp;
    uintptr_t dirty_lr = (uintptr_t)0x71727374u;
    uintptr_t saved_sp = (uintptr_t)(&first_stack[120]);
    uintptr_t expected_restored_sp = saved_sp - (uintptr_t)OS_PORT_TMS570_FIQ_MINIMAL_FRAME_BYTES;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    memset(&dirty_vfp, 0, sizeof(dirty_vfp));
    dirty_vfp.Enabled = TRUE;
    dirty_vfp.Fpscr = 0xCAFEB003u;
    dirty_vfp.D[2].Low = 95u;
    dirty_vfp.D[2].High = 96u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(saved_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(7u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x31323334u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x71000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&saved_scratch));
    Os_Port_Tms570_FiqContextSave();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[92])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(77u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x11000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&dirty_scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&dirty_lower));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&dirty_preserved));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&dirty_vfp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister(dirty_lr));
    Os_Port_Tms570_FiqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_RESUME_PREVIOUS_MODE,
                            state->LastFiqRestoreAction);
    TEST_ASSERT_EQUAL_PTR((void*)expected_restored_sp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_UINT32(0x71000013u, state->CurrentFiqSavedCpsr);
    TEST_ASSERT_EQUAL_UINT32(saved_scratch.R0, state->CurrentFiqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->CurrentFiqScratch.R12);
    TEST_ASSERT_EQUAL_UINT32(77u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(dirty_lower.R0, state->CurrentTaskLower.R0);
    TEST_ASSERT_EQUAL_UINT32(dirty_lower.R3, state->CurrentTaskLower.R3);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R4, state->CurrentTaskPreserved.R4);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R10, state->CurrentTaskPreserved.R10);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R11, state->CurrentTaskPreserved.R11);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R12, state->CurrentTaskPreserved.R12);
    TEST_ASSERT_TRUE(state->CurrentTaskVfp.Enabled);
    TEST_ASSERT_EQUAL_UINT32(dirty_vfp.Fpscr, state->CurrentTaskVfp.Fpscr);
    TEST_ASSERT_EQUAL_UINT32(dirty_vfp.D[2].High, state->CurrentTaskVfp.D[2].High);
    TEST_ASSERT_EQUAL_PTR((void*)dirty_lr, (void*)state->CurrentTaskLinkRegister);
}

void test_Os_Port_Tms570_RegisterFiqResumeFrameTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_resume_restore_reapplies_fiq_frame_without_switch_payload);
}
