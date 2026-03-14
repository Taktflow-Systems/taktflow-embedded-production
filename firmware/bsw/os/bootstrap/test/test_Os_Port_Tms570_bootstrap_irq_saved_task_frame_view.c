/**
 * @file    test_Os_Port_Tms570_bootstrap_irq_saved_task_frame_view.c
 * @brief   IRQ/task save frame-view tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

void test_Os_Port_Tms570_solicited_return_exposes_packed_saved_task_frame_view(void)
{
    uint8 first_stack[160];
    uintptr_t running_sp = (uintptr_t)(&first_stack[112]);
    uintptr_t expected_saved_sp = running_sp - (uintptr_t)OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES;
    const Os_Port_Tms570_TaskFrameViewType* saved;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(running_sp));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(7u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskLinkRegister((uintptr_t)0xABCDEF01u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SolicitedSystemReturn());

    saved = Os_Port_Tms570_PeekSavedTaskFrameView();
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT_TRUE(saved->Frame.Valid);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SOLICITED_FRAME_CORE_BYTES, saved->FrameBytes);
    TEST_ASSERT_EQUAL_PTR((void*)expected_saved_sp, (void*)saved->Frame.Sp);
    TEST_ASSERT_EQUAL_PTR((void*)0xABCDEF01u, (void*)saved->Frame.ReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xABCDEF01u, (void*)saved->Frame.LinkRegister);
    TEST_ASSERT_EQUAL_UINT32(7u, saved->Frame.TimeSlice);
    TEST_ASSERT_EQUAL_UINT32(OS_PORT_TMS570_SOLICITED_STACK_TYPE, saved->Frame.StackType);
    TEST_ASSERT_EQUAL_PTR((void*)saved->Frame.Sp, (void*)Os_Port_Tms570_PeekSavedTaskSp());
    TEST_ASSERT_EQUAL_PTR((void*)saved->Frame.ReturnAddress, (void*)Os_Port_Tms570_PeekSavedTaskReturnAddress());
    TEST_ASSERT_EQUAL_PTR((void*)saved->Frame.LinkRegister, (void*)Os_Port_Tms570_PeekSavedTaskLinkRegister());
    TEST_ASSERT_EQUAL_HEX32(saved->Frame.Cpsr, Os_Port_Tms570_PeekSavedTaskCpsr());
    TEST_ASSERT_EQUAL_UINT32(saved->Frame.StackType, Os_Port_Tms570_PeekSavedTaskStackType());
    TEST_ASSERT_EQUAL_UINT32(saved->FrameBytes, Os_Port_Tms570_PeekSavedTaskFrameBytes());
    TEST_ASSERT_EQUAL_UINT32(saved->Frame.Preserved.R4, Os_Port_Tms570_PeekSavedTaskPreserved()->R4);
}

void test_Os_Port_Tms570_RegisterIrqSavedTaskFrameViewTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_solicited_return_exposes_packed_saved_task_frame_view);
}
