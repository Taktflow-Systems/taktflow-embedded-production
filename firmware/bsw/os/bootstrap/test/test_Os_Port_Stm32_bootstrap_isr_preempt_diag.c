/**
 * @file    test_Os_Port_Stm32_bootstrap_isr_preempt_diag.c
 * @brief   Diagnostic — trace state after each step of ISR preemption flow
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

#define LOW_TASK_ID   ((TaskType)0u)
#define HIGH_TASK_ID  ((TaskType)1u)

static uint8 low_task_runs;
static uint8 high_task_runs;
static TaskType current_task_after_isr;
static TaskStateType high_state_after_isr;
static TaskStateType low_state_after_low_entry;

static void high_task_entry(void)
{
    high_task_runs++;
}

static void isr_handler(void)
{
    (void)ActivateTask(HIGH_TASK_ID);
}

static void low_task_entry(void)
{
    low_task_runs++;
    (void)Os_TestInvokeIsrCat2(isr_handler);
    /* Capture state right after ISR returns */
    GetTaskID(&current_task_after_isr);
    {
        TaskStateType s;
        GetTaskState(HIGH_TASK_ID, &s);
        high_state_after_isr = s;
    }
}

static const Os_TaskConfigType tasks[] = {
    { "LowTask",  low_task_entry,  2u, 1u, 0u, FALSE, NON },
    { "HighTask", high_task_entry, 1u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    low_task_runs = 0u;
    high_task_runs = 0u;
    current_task_after_isr = INVALID_TASK;
    high_state_after_isr = SUSPENDED;
    low_state_after_low_entry = SUSPENDED;
}

void tearDown(void) {}

/**
 * @spec After ISR exit inside non-preemptive LowTask, current task is still LowTask.
 * @verify current_task_after_isr == LOW_TASK_ID.
 */
void test_current_task_still_low_after_isr(void)
{
    uint8 low_stack[128];
    uint8 high_stack[128];

    Os_TestReset();
    TEST_ASSERT_EQUAL(E_OK,
        Os_TestConfigureTasks(tasks,
            (uint8)(sizeof(tasks) / sizeof(tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredFirstTask(LOW_TASK_ID,
            (uintptr_t)(&low_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredTask(HIGH_TASK_ID,
            (uintptr_t)(&high_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(LOW_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());

    TEST_ASSERT_EQUAL_UINT8(1u, low_task_runs);
    TEST_ASSERT_EQUAL(LOW_TASK_ID, current_task_after_isr);
}

/**
 * @spec HighTask is READY after ISR activation.
 * @verify high_state_after_isr == READY.
 */
void test_high_task_ready_after_isr(void)
{
    uint8 low_stack[128];
    uint8 high_stack[128];

    Os_TestReset();
    TEST_ASSERT_EQUAL(E_OK,
        Os_TestConfigureTasks(tasks,
            (uint8)(sizeof(tasks) / sizeof(tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredFirstTask(LOW_TASK_ID,
            (uintptr_t)(&low_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredTask(HIGH_TASK_ID,
            (uintptr_t)(&high_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(LOW_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());

    TEST_ASSERT_EQUAL_UINT8(1u, low_task_runs);
    TEST_ASSERT_EQUAL_UINT8(READY, high_state_after_isr);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_current_task_still_low_after_isr);
    RUN_TEST(test_high_task_ready_after_isr);
    return UNITY_END();
}
