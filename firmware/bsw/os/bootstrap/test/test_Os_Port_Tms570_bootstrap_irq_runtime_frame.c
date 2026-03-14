/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_runtime_frame.c
 * @brief   IRQ runtime-frame tests for the TMS570 Cortex-R5 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_runtime_frame_sp_moves_without_changing_initial_frame(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;
    uintptr_t initial_sp;
    uintptr_t runtime_sp;
    uintptr_t expected_saved_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    initial_sp = first_ctx->InitialFrame.Sp;
    runtime_sp = initial_sp - (uintptr_t)32u;
    expected_saved_sp = runtime_sp - (uintptr_t)OS_PORT_TMS570_IRQ_MINIMAL_FRAME_BYTES;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp(runtime_sp));

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(first_ctx->InitialFrame.Valid);
    TEST_ASSERT_TRUE(first_ctx->RuntimeFrame.Valid);
    TEST_ASSERT_EQUAL_PTR((void*)initial_sp, (void*)first_ctx->InitialFrame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)first_ctx->SavedSp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)first_ctx->RuntimeFrame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)runtime_sp, (void*)first_ctx->RuntimeSp);
    TEST_ASSERT_TRUE(first_ctx->InitialFrame.Sp != first_ctx->RuntimeFrame.Sp);
}

void test_Os_Port_Tms570_irq_switch_uses_runtime_frame_timeslices_without_touching_initial_frame(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(3u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetTaskSavedTimeSlice(OS_PORT_TMS570_SECOND_TASK_ID, 7u));

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    TEST_ASSERT_EQUAL_UINT32(0u, first_ctx->InitialFrame.TimeSlice);
    TEST_ASSERT_EQUAL_UINT32(3u, first_ctx->RuntimeFrame.TimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0u, second_ctx->InitialFrame.TimeSlice);
    TEST_ASSERT_EQUAL_UINT32(7u, second_ctx->RuntimeFrame.TimeSlice);
    TEST_ASSERT_EQUAL_UINT32(7u, state->CurrentTimeSlice);
}

void test_Os_Port_Tms570_RegisterIrqRuntimeFrameTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_runtime_frame_sp_moves_without_changing_initial_frame);
    RUN_TEST(test_Os_Port_Tms570_irq_switch_uses_runtime_frame_timeslices_without_touching_initial_frame);
}
