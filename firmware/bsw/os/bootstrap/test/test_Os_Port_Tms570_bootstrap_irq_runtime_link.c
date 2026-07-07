/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_runtime_link.c
 * @brief   IRQ runtime link-register tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_switch_captures_outgoing_link_register_and_applies_incoming_link_register(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    uintptr_t outgoing_lr = (uintptr_t)0x11112222u;
    uintptr_t incoming_lr = (uintptr_t)0x33334444u;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareTaskContext(
            OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetTaskSavedLinkRegister(
                          OS_PORT_TMS570_SECOND_TASK_ID, incoming_lr));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister(outgoing_lr));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)outgoing_lr, (void*)first_ctx->RuntimeFrame.LinkRegister);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)incoming_lr, (void*)state->CurrentTaskLinkRegister);
    TEST_ASSERT_EQUAL_PTR((void*)incoming_lr, (void*)state->LastRestoredTaskLinkRegister);
}

void test_Os_Port_Tms570_RegisterIrqRuntimeLinkTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_switch_captures_outgoing_link_register_and_applies_incoming_link_register);
}
