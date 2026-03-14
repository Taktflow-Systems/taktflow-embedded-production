/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_runtime_metadata.c
 * @brief   FIQ runtime metadata tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_scheduler_return_refreshes_runtime_metadata_from_live_fiq_state(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_IrqScratchSnapshotType scratch = {31u, 32u, 33u, 34u, 310u, 312u};

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt,
                          (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0x78787870u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x70000013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&scratch));

    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextRestore();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)0x78787870u, (void*)first_ctx->RuntimeFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_UINT32(0x70000013u, first_ctx->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(scratch.R0, first_ctx->RuntimeFrame.IrqScratch.R0);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->RuntimeFrame.IrqScratch.R12);
}

void test_Os_Port_Tms570_RegisterFiqRuntimeMetadataTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_scheduler_return_refreshes_runtime_metadata_from_live_fiq_state);
}
