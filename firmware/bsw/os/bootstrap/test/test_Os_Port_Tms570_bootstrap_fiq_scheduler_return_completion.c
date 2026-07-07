/**
 * @file    test_Os_Port_Tms570_bootstrap_fiq_scheduler_return_completion.c
 * @brief   FIQ scheduler-return completion tests for the TMS570 OS port
 * @date    2026-03-14
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

static void prepare_pending_fiq_scheduler_return(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetFiqReturnAddress((uintptr_t)0xC6C7C8C9u));
    Os_Port_Tms570_FiqContextSave();
    Os_Port_Tms570_FiqContextRestore();
}

void test_Os_Port_Tms570_complete_configured_dispatch_finishes_pending_fiq_scheduler_return(void)
{
    const Os_Port_Tms570_StateType* state;

    prepare_pending_fiq_scheduler_return();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->FiqSchedulerReturnInProgress);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

void test_Os_Port_Tms570_observe_kernel_dispatch_finishes_pending_fiq_scheduler_return(void)
{
    const Os_Port_Tms570_StateType* state;

    prepare_pending_fiq_scheduler_return();
    Os_Port_Tms570_ObserveKernelDispatch(OS_PORT_TMS570_SECOND_TASK_ID);

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->FiqSchedulerReturnInProgress);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->KernelDispatchObserveCount);
}

void test_Os_Port_Tms570_RegisterFiqSchedulerReturnCompletionTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_complete_configured_dispatch_finishes_pending_fiq_scheduler_return);
    RUN_TEST(test_Os_Port_Tms570_observe_kernel_dispatch_finishes_pending_fiq_scheduler_return);
}
