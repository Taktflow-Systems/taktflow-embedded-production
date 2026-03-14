/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_runtime_vfp.c
 * @brief   FIQ runtime VFP-state tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_scheduler_return_captures_live_vfp_state(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_TaskContextType* first_ctx;
    Os_Port_Tms570_VfpStateType outgoing;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareTaskContext(
            OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));

    memset(&outgoing, 0, sizeof(outgoing));
    outgoing.Enabled = TRUE;
    outgoing.Fpscr = 0x13572468u;
    outgoing.D[2].Low = 201u;
    outgoing.D[2].High = 202u;
    outgoing.D[12].Low = 301u;
    outgoing.D[12].High = 302u;

    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&outgoing));
    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextRestore();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(first_ctx->RuntimeFrame.Vfp.Enabled);
    TEST_ASSERT_EQUAL_UINT32(outgoing.Fpscr, first_ctx->RuntimeFrame.Vfp.Fpscr);
    TEST_ASSERT_EQUAL_UINT32(outgoing.D[2].Low, first_ctx->RuntimeFrame.Vfp.D[2].Low);
    TEST_ASSERT_EQUAL_UINT32(outgoing.D[12].High, first_ctx->RuntimeFrame.Vfp.D[12].High);
}

void test_Os_Port_Tms570_RegisterFiqRuntimeVfpTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_scheduler_return_captures_live_vfp_state);
}
