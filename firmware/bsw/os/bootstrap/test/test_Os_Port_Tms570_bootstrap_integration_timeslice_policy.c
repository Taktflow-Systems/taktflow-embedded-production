/**
 * @file    test_Os_Port_Tms570_bootstrap_integration_timeslice_policy.c
 * @brief   Time-slice policy boundary tests for the TMS570 bootstrap port
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

static const Os_TaskConfigType same_priority_tasks[] = {
    { "PeerA", dummy_task_entry, 2u, 1u, 0u, FALSE, FULL },
    { "PeerB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

/**
 * @spec Local ThreadX reference: common/src/tx_thread_time_slice.c
 * @requirement The bootstrap TMS570 time-slice service seam shall stop at
 *              reloading the running task's next slice budget and shall not
 *              claim a ThreadX-style same-priority peer rotation inside the
 *              OSEK-oriented bootstrap scheduler model.
 * @verify When another same-priority task is ready, TickIsr still reloads
 *         the current task's saved slice, leaves the ready peer untouched,
 *         and does not request or complete a dispatch.
 */
void test_Os_Port_Tms570_tick_isr_does_not_rotate_to_same_priority_peer(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    TaskStateType peer_state;
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            same_priority_tasks,
            (uint8)(sizeof(same_priority_tasks) / sizeof(same_priority_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_TestSetTaskSavedTimeSlice(
                          OS_PORT_TMS570_FIRST_TASK_ID, 4u));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, Os_TestSetCurrentTaskRunning(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(OS_PORT_TMS570_SECOND_TASK_ID, &peer_state));
    TEST_ASSERT_EQUAL(READY, peer_state);
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTimeSlice(1u));
    TEST_ASSERT_FALSE(Os_Port_Tms570_GetBootstrapState()->DispatchRequested);

    Os_Port_Tms570_TickIsr();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(OS_PORT_TMS570_SECOND_TASK_ID, &peer_state));

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(READY, peer_state);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TimeSliceExpirationCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TimeSliceServiceCount);
    TEST_ASSERT_EQUAL_UINT32(4u, state->CurrentTimeSlice);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
}

void test_Os_Port_Tms570_RegisterIntegrationTimeSlicePolicyTests(void)
{
    RUN_TEST(test_Os_Port_Tms570_tick_isr_does_not_rotate_to_same_priority_peer);
}
