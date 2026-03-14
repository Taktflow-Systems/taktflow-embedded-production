/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_frame_bytes.c
 * @brief   IRQ frame-byte profile tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_restore_frame_bytes_follow_interrupt_and_solicited_profiles(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    Os_Port_Tms570_VfpStateType saved_vfp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PrepareFirstTask(OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PrepareTaskContext(OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    memset(&saved_vfp, 0, sizeof(saved_vfp));
    saved_vfp.Enabled = TRUE;
    saved_vfp.Fpscr = 0x1234u;
    saved_vfp.D[0].Low = 1u;
    saved_vfp.D[8].High = 2u;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedVfp(OS_PORT_TMS570_SECOND_TASK_ID, &saved_vfp));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES + OS_PORT_TMS570_INTERRUPT_FRAME_VFP_BYTES,
                             Os_Port_Tms570_PeekRestoreTaskFrameBytes());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedStackType(OS_PORT_TMS570_SECOND_TASK_ID, OS_PORT_TMS570_SOLICITED_STACK_TYPE));
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES + OS_PORT_TMS570_SOLICITED_FRAME_VFP_BYTES,
                             Os_Port_Tms570_PeekRestoreTaskFrameBytes());
}

void test_Os_Port_Tms570_solicited_and_interrupt_paths_record_task_frame_bytes(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    Os_Port_Tms570_VfpStateType saved_vfp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PrepareFirstTask(OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PrepareTaskContext(OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    memset(&saved_vfp, 0, sizeof(saved_vfp));
    saved_vfp.Enabled = TRUE;
    saved_vfp.Fpscr = 0x5678u;
    saved_vfp.D[8].Low = 3u;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&saved_vfp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SolicitedSystemReturn());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES + OS_PORT_TMS570_SOLICITED_FRAME_VFP_BYTES,
                             state->LastSavedTaskFrameBytes);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES, state->LastRestoredTaskFrameBytes);
}

void test_Os_Port_Tms570_RegisterIrqFrameByteTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_restore_frame_bytes_follow_interrupt_and_solicited_profiles);
    RUN_TEST(test_Os_Port_Tms570_solicited_and_interrupt_paths_record_task_frame_bytes);
}
