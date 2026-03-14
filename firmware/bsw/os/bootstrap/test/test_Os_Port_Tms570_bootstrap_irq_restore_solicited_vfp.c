/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restore_solicited_vfp.c
 * @brief   Solicited VFP restore-split tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_solicited_restore_only_reapplies_high_vfp_registers(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    Os_Port_Tms570_VfpStateType selected;
    Os_Port_Tms570_VfpStateType dirty;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt,
                          (uintptr_t)(&second_stack[160])));

    memset(&selected, 0, sizeof(selected));
    memset(&dirty, 0, sizeof(dirty));
    selected.Enabled = TRUE;
    selected.Fpscr = 0xAABBCCDDu;
    selected.D[2].Low = 21u;
    selected.D[10].High = 1010u;
    dirty.Enabled = TRUE;
    dirty.Fpscr = 0x11223344u;
    dirty.D[2].Low = 202u;
    dirty.D[10].High = 909u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedStackType(
                                OS_PORT_TMS570_SECOND_TASK_ID, OS_PORT_TMS570_SOLICITED_STACK_TYPE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedVfp(OS_PORT_TMS570_SECOND_TASK_ID, &selected));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&dirty));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SOLICITED_STACK_TYPE, state->LastRestoredTaskStackType);
    TEST_ASSERT_TRUE(state->CurrentTaskVfp.Enabled);
    TEST_ASSERT_EQUAL_HEX32(selected.Fpscr, state->CurrentTaskVfp.Fpscr);
    TEST_ASSERT_EQUAL_UINT32(dirty.D[2].Low, state->CurrentTaskVfp.D[2].Low);
    TEST_ASSERT_EQUAL_UINT32(selected.D[10].High, state->CurrentTaskVfp.D[10].High);
}

void test_Os_Port_Tms570_RegisterIrqRestoreSolicitedVfpTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_solicited_restore_only_reapplies_high_vfp_registers);
}
