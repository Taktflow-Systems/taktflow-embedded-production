/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restore_link.c
 * @brief   IRQ restore link-register helper tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_restore_link_helper_follows_selected_runtime_frame(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t selected_lr = (uintptr_t)0x55556666u;

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
                          OS_PORT_TMS570_SECOND_TASK_ID, selected_lr));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL_PTR((void*)selected_lr, (void*)Os_Port_Tms570_PeekRestoreTaskLinkRegister());
}

void test_Os_Port_Tms570_RegisterIrqRestoreLinkTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_restore_link_helper_follows_selected_runtime_frame);
}
