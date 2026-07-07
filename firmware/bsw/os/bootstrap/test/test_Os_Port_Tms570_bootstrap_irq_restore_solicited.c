/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restore_solicited.c
 * @brief   IRQ solicited-restore tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_switch_with_solicited_stack_restores_only_solicited_payload(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* second_ctx;
    const Os_Port_Tms570_IrqScratchSnapshotType saved_scratch = {11u, 12u, 13u, 14u, 15u, 16u};
    const Os_Port_Tms570_IrqScratchSnapshotType dirty_scratch = {31u, 32u, 33u, 34u, 35u, 36u};
    const Os_Port_Tms570_TaskLowerSnapshotType saved_lower = {21u, 22u, 23u, 24u};
    const Os_Port_Tms570_TaskLowerSnapshotType dirty_lower = {41u, 42u, 43u, 44u};
    const Os_Port_Tms570_PreservedSnapshotType saved_preserved = {51u, 52u, 53u, 54u, 55u, 56u, 57u, 58u, 59u};
    const Os_Port_Tms570_PreservedSnapshotType dirty_preserved = {61u, 62u, 63u, 64u, 65u, 66u, 67u, 68u, 69u};
    Os_Port_Tms570_VfpStateType saved_vfp;
    Os_Port_Tms570_VfpStateType dirty_vfp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PrepareFirstTask(OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PrepareTaskContext(OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[120])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x11111111u));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    memset(&saved_vfp, 0, sizeof(saved_vfp));
    saved_vfp.Enabled = TRUE;
    saved_vfp.Fpscr = 0x12340001u;
    saved_vfp.D[0].Low = 101u;
    saved_vfp.D[8].High = 202u;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&second_stack[112])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(17u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x22222222u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x40000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved_scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&saved_lower));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&saved_preserved));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&saved_vfp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0x33333333u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SolicitedSystemReturn());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());

    memset(&dirty_vfp, 0, sizeof(dirty_vfp));
    dirty_vfp.Enabled = TRUE;
    dirty_vfp.Fpscr = 0xABCD0002u;
    dirty_vfp.D[0].Low = 111u;
    dirty_vfp.D[8].High = 222u;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp((uintptr_t)(&first_stack[96])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(77u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x50000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&dirty_scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&dirty_lower));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&dirty_preserved));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&dirty_vfp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0x44444444u));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_UINT32(17u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_PTR((void*)0x33333333u, (void*)state->CurrentTaskLinkRegister);
    TEST_ASSERT_EQUAL_UINT32(0x40000013u, state->CurrentIrqSavedCpsr);
    TEST_ASSERT_EQUAL_UINT32(dirty_lower.R0, state->CurrentTaskLower.R0);
    TEST_ASSERT_EQUAL_UINT32(dirty_scratch.R0, state->CurrentIrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(saved_preserved.R4, state->CurrentTaskPreserved.R4);
    TEST_ASSERT_EQUAL_UINT32(saved_preserved.R11, state->CurrentTaskPreserved.R11);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R12, state->CurrentTaskPreserved.R12);
    TEST_ASSERT_EQUAL_UINT32(saved_vfp.Fpscr, state->CurrentTaskVfp.Fpscr);
    TEST_ASSERT_EQUAL_UINT32(dirty_vfp.D[0].Low, state->CurrentTaskVfp.D[0].Low);
    TEST_ASSERT_EQUAL_UINT32(saved_vfp.D[8].High, state->CurrentTaskVfp.D[8].High);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SOLICITED_STACK_TYPE, state->LastRestoredTaskStackType);
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastRestoredTaskLower.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, state->LastRestoredTaskIrqScratch.R0);
}

void test_Os_Port_Tms570_RegisterIrqRestoreSolicitedTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_switch_with_solicited_stack_restores_only_solicited_payload);
}
