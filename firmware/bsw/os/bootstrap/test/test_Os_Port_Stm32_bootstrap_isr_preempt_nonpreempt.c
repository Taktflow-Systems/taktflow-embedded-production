/**
 * @file    test_Os_Port_Stm32_bootstrap_isr_preempt_nonpreempt.c
 * @brief   Non-preemptive task ISR activation — smallest slice
 * @date    2026-03-14
 *
 * @details Tests the simplest case: a non-preemptive LowTask is RUNNING,
 *          an ISR activates HighTask.  After ISR exit, HighTask should NOT
 *          have run (non-preemptive = no preemption).
 *
 *          ThreadX reference: non-preemption threshold — if a thread's
 *          preemption-threshold equals its priority, it cannot be preempted.
 *
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

static uint8 high_task_runs;

static void low_task_entry(void)  { /* does nothing */ }
static void high_task_entry(void) { high_task_runs++; }

static const Os_TaskConfigType tasks[] = {
    { "LowTask",  low_task_entry,  2u, 1u, 0u, FALSE, NON },
    { "HighTask", high_task_entry, 1u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    high_task_runs = 0u;
}

void tearDown(void) {}

/**
 * @spec Non-preemptive task cannot be preempted even by higher priority.
 * @requirement After ISR activates HighTask while non-preemptive LowTask
 *              is RUNNING, HighTask Entry() shall NOT be called.
 * @verify high_task_runs == 0 after ISR exit.
 */
void test_nonpreemptive_task_not_preempted_by_isr_activation(void)
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

    /* LowTask is RUNNING (non-preemptive) */
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(LOW_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestSetCurrentTaskRunning(LOW_TASK_ID));

    /* ISR activates HighTask */
    Os_PortEnterIsr2();
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(HIGH_TASK_ID));
    Os_PortExitIsr2();

    /* Non-preemptive: HighTask must NOT run */
    TEST_ASSERT_EQUAL_UINT8(0u, high_task_runs);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nonpreemptive_task_not_preempted_by_isr_activation);
    return UNITY_END();
}
