/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_idle_restore.c
 * @brief   IRQ idle-system restore tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_restore_records_idle_system_action_when_no_task_was_running(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x12131415u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentIrqSavedCpsr(0x70000013u));

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_IDLE_SYSTEM,
                            Os_Port_Tms570_PeekRestoreAction());

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_IDLE_SYSTEM, state->LastRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqIdleSystemReturnCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSchedulerReturnCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)0x12131415u, (void*)state->LastRestoredIrqReturnAddress);
}

void test_Os_Port_Tms570_RegisterIrqIdleRestoreTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_restore_records_idle_system_action_when_no_task_was_running);
}
