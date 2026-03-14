/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_dispatch_context_sync.c
 * @brief   IRQ dispatch sync tests for packed restored context
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_switch_away_uses_packed_restored_context_not_mutated_live_state(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_IrqScratchSnapshotType saved = {111u, 112u, 113u, 114u, 1110u, 1112u};
    const Os_Port_Tms570_IrqScratchSnapshotType live = {121u, 122u, 123u, 124u, 1210u, 1212u};
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
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x89ABCDE0u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x79100013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&saved));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    restore_action = Os_Port_Tms570_BeginIrqContextRestore();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK, restore_action);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x9ABCDE01u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x79200013u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqScratch(&live));
    Os_Port_Tms570_FinishIrqContextRestore(restore_action);

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_NOT_NULL(first_ctx);
    TEST_ASSERT_EQUAL_UINT32(0x89ABCDE0u, (uint32)first_ctx->RuntimeFrame.ReturnAddress);
    TEST_ASSERT_EQUAL_HEX32(0x79100013u, first_ctx->RuntimeFrame.Cpsr);
    TEST_ASSERT_EQUAL_UINT32(saved.R12, first_ctx->RuntimeFrame.IrqScratch.R12);
}

void test_Os_Port_Tms570_RegisterIrqDispatchContextSyncTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_switch_away_uses_packed_restored_context_not_mutated_live_state);
}
