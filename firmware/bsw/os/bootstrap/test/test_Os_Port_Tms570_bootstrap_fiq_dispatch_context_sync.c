/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_dispatch_context_sync.c
 * @brief   FIQ dispatch sync tests for packed restored context
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_fiq_scheduler_return_uses_packed_restored_context_not_mutated_live_state(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {131u, 132u, 133u, 134u, 1310u, 1312u};
    const Os_Port_Tms570_IrqScratchSnapshotType live = {141u, 142u, 143u, 144u, 1410u, 1412u};
    const Os_Port_Tms570_TaskContextType* first_ctx;
    uint8 restore_action;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xABCDEF12u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x7A100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&saved));
    Os_Port_Tms570_FiqContextSave();

    restore_action = Os_Port_Tms570_BeginFiqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_FIQ_RESTORE_PREEMPT_SCHEDULER, restore_action);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xBCDEF123u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqSavedCpsr(0x7A200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentFiqScratch(&live));
    Os_Port_Tms570_FinishFiqContextRestore(restore_action);

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_NOT_NULL(first_ctx);
    TEST_ASSERT_EQUAL_UINT32(0xABCDEF12u, (uint32)first_ctx->RuntimeFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x7A100013u, first_ctx->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->RuntimeFrame.IrqScratch.R12);
}

void test_Os_Port_Tms570_RegisterFiqDispatchContextSyncTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_fiq_scheduler_return_uses_packed_restored_context_not_mutated_live_state);
}
