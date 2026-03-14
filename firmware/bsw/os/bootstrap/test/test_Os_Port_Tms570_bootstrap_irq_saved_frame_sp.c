/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_saved_frame_sp.c
 * @brief   IRQ saved-frame stack-pointer tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include <string.h>

#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_irq_switch_commits_saved_frame_sp_from_live_running_sp(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t running_sp = (uintptr_t)(&first_stack[120]);
    uintptr_t expected_saved_sp;
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    Os_Port_Tms570_VfpStateType vfp_state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PrepareFirstTask(OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_PrepareTaskContext(OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();

    memset(&vfp_state, 0, sizeof(vfp_state));
    vfp_state.Enabled = TRUE;
    vfp_state.Fpscr = 0x12345678u;
    vfp_state.D[0].Low = 1u;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(running_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskVfp(&vfp_state));
    expected_saved_sp = running_sp -
                        (uintptr_t)(OS_PORT_TMS570_INTERRUPT_FRAME_CORE_BYTES +
                                    OS_PORT_TMS570_INTERRUPT_FRAME_VFP_BYTES);

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)running_sp, (void*)first_ctx->RuntimeSp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)first_ctx->SavedSp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)first_ctx->RuntimeFrame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)state->LastSavedTaskSp);
}

void test_Os_Port_Tms570_RegisterIrqSavedFrameSpTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_irq_switch_commits_saved_frame_sp_from_live_running_sp);
}
