/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_solicited_return.c
 * @brief   Solicited system-return tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_solicited_system_return_captures_minimal_runtime_frame(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* context;
    uintptr_t running_sp = (uintptr_t)(&first_stack[112]);
    uintptr_t expected_saved_sp;
    const Os_Port_Tms570_TaskLowerSnapshotType dirty_lower = {1u, 2u, 3u, 4u};
    const Os_Port_Tms570_IrqScratchSnapshotType dirty_scratch = {5u, 6u, 7u, 8u, 9u, 10u};
    const Os_Port_Tms570_PreservedSnapshotType dirty_preserved = {11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u};
    Os_Port_Tms570_VfpStateType dirty_vfp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PrepareFirstTask(OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    memset(&dirty_vfp, 0, sizeof(dirty_vfp));
    dirty_vfp.Enabled = TRUE;
    dirty_vfp.Fpscr = 0x13570001u;
    dirty_vfp.D[0].Low = 21u;
    dirty_vfp.D[8].High = 31u;
    expected_saved_sp = running_sp -
                        (uintptr_t)(OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES +
                                    OS_PORT_TMS570_SOLICITED_FRAME_VFP_BYTES);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(running_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(23u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x60000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLower(&dirty_lower));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&dirty_scratch));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskPreserved(&dirty_preserved));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&dirty_vfp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0xABCDEF01u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SolicitedSystemReturn());

    state = Os_Port_Tms570_GetBootstrapState();
    context = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SOLICITED_STACK_TYPE, context->RuntimeFrame.StackType);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)context->RuntimeFrame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)context->SavedSp);
    TEST_ASSERT_EQUAL_PTR((void*)running_sp, (void*)context->RuntimeSp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_UINT32(23u, context->RuntimeFrame.TimeSlice);
    TEST_ASSERT_EQUAL_PTR((void*)0xABCDEF01u, (void*)context->RuntimeFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xABCDEF01u, (void*)context->RuntimeFrame.LinkRegister);
    TEST_ASSERT_EQUAL_UINT32(0x60000013u, context->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(0u, context->RuntimeFrame.TaskLower.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, context->RuntimeFrame.IrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R4, context->RuntimeFrame.Preserved.R4);
    TEST_ASSERT_EQUAL_UINT32(dirty_preserved.R11, context->RuntimeFrame.Preserved.R11);
    TEST_ASSERT_EQUAL_UINT32(0u, context->RuntimeFrame.Preserved.R12);
    TEST_ASSERT_TRUE(context->RuntimeFrame.Vfp.Enabled);
    TEST_ASSERT_EQUAL_UINT32(dirty_vfp.Fpscr, context->RuntimeFrame.Vfp.Fpscr);
    TEST_ASSERT_EQUAL_UINT32(0u, context->RuntimeFrame.Vfp.D[0].Low);
    TEST_ASSERT_EQUAL_UINT32(dirty_vfp.D[8].High, context->RuntimeFrame.Vfp.D[8].High);
}

void test_Os_Port_Tms570_RegisterIrqSolicitedReturnTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_solicited_system_return_captures_minimal_runtime_frame);
}
