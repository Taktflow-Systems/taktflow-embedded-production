/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_restore_vfp.c
 * @brief   IRQ restore VFP-helper tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_restore_vfp_helper_follows_selected_runtime_frame(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    Os_Port_Tms570_VfpStateType selected;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareFirstTask(
            OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_Port_Tms570_PrepareTaskContext(
            OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));

    memset(&selected, 0, sizeof(selected));
    selected.Enabled = TRUE;
    selected.Fpscr = 0xAABBCCDDu;
    selected.D[3].Low = 101u;
    selected.D[3].High = 102u;
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetTaskSavedVfp(
                          OS_PORT_TMS570_SECOND_TASK_ID, &selected));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)&second_ctx->RuntimeFrame.Vfp,
                          (void*)Os_Port_Tms570_PeekRestoreTaskVfp());
}

void test_Os_Port_Tms570_RegisterIrqRestoreVfpTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_restore_vfp_helper_follows_selected_runtime_frame);
}
