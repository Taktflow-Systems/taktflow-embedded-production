/**
 * @file    test_Os_Port_Stm32_bootstrap_alarm.c
 * @brief   Alarm counter helper tests for the STM32 Cortex-M4 bootstrap OS port
 * @date    2026-03-14
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "Os.h"
#include "Os_Port_Stm32.h"
#include "Os_Port_TaskBinding.h"

#define OS_PORT_STM32_FIRST_TASK_ID  ((TaskType)0u)
#define OS_PORT_STM32_SECOND_TASK_ID ((TaskType)1u)
#define ALARM_ACTIVATE               ((AlarmType)0u)

static uint8 scheduler_bridge_low_runs;
static uint8 scheduler_bridge_high_runs;
static StatusType scheduler_bridge_activate_status;

static void scheduler_bridge_high_task(void)
{
    scheduler_bridge_high_runs++;
}

static void scheduler_bridge_low_task(void)
{
    scheduler_bridge_low_runs++;
    scheduler_bridge_activate_status = ActivateTask(OS_PORT_STM32_SECOND_TASK_ID);
}

static const Os_TaskConfigType alarm_scheduler_tasks[] = {
    { "LowTask",  scheduler_bridge_low_task,  2u, 1u, 0u, FALSE, FULL },
    { "HighTask", scheduler_bridge_high_task, 1u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    scheduler_bridge_low_runs = 0u;
    scheduler_bridge_high_runs = 0u;
    scheduler_bridge_activate_status = E_OK;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(alarm_scheduler_tasks, (uint8)(sizeof(alarm_scheduler_tasks) /
                                                             sizeof(alarm_scheduler_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 * @requirement The shared bootstrap counter helper shall drain any deferred
 *              STM32 configured-task handoff that alarm processing creates.
 * @verify An alarm-activated low task that readies a prepared high task
 *         leaves the port in the completed high-task state after a single
 *         Os_TestAdvanceCounter call.
 */
void test_Os_Port_Stm32_alarm_counter_helper_drains_deferred_dispatch_to_idle(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_STM32_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            alarm_scheduler_tasks,
            (uint8)(sizeof(alarm_scheduler_tasks) /
                    sizeof(alarm_scheduler_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, (uintptr_t)(&first_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_STM32_SECOND_TASK_ID, (uintptr_t)(&second_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));
    Os_TestAdvanceCounter(1u);
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_alarm_counter_helper_drains_deferred_dispatch_to_idle);
    return UNITY_END();
}
