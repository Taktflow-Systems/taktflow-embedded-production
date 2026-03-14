/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_runtime_vfp.c
 * @brief   IRQ runtime VFP-state tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_switch_captures_outgoing_vfp_and_applies_incoming_vfp(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    Os_Port_Tms570_VfpStateType outgoing;
    Os_Port_Tms570_VfpStateType incoming;

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
    memset(&incoming, 0, sizeof(incoming));
    outgoing.Enabled = TRUE;
    outgoing.Fpscr = 0x12340001u;
    outgoing.D[0].Low = 10u;
    outgoing.D[0].High = 11u;
    outgoing.D[15].Low = 40u;
    outgoing.D[15].High = 41u;
    incoming.Enabled = TRUE;
    incoming.Fpscr = 0x56780002u;
    incoming.D[1].Low = 50u;
    incoming.D[1].High = 51u;
    incoming.D[14].Low = 80u;
    incoming.D[14].High = 81u;

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetTaskSavedVfp(
                          OS_PORT_TMS570_SECOND_TASK_ID, &incoming));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&outgoing));

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(first_ctx->RuntimeFrame.Vfp.Enabled);
    TEST_ASSERT_EQUAL_UINT32(outgoing.Fpscr, first_ctx->RuntimeFrame.Vfp.Fpscr);
    TEST_ASSERT_EQUAL_UINT32(outgoing.D[0].Low, first_ctx->RuntimeFrame.Vfp.D[0].Low);
    TEST_ASSERT_EQUAL_UINT32(outgoing.D[15].High, first_ctx->RuntimeFrame.Vfp.D[15].High);
    TEST_ASSERT_TRUE(state->CurrentTaskVfp.Enabled);
    TEST_ASSERT_EQUAL_UINT32(incoming.Fpscr, state->CurrentTaskVfp.Fpscr);
    TEST_ASSERT_EQUAL_UINT32(incoming.D[1].High, state->LastRestoredTaskVfp.D[1].High);
    TEST_ASSERT_EQUAL_UINT32(incoming.D[14].Low, state->LastRestoredTaskVfp.D[14].Low);
}

void test_Os_Port_Tms570_RegisterIrqRuntimeVfpTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_switch_captures_outgoing_vfp_and_applies_incoming_vfp);
}
