/**
 * @file    test_Os_Port_Stm32_bootstrap_isr_preempt_preemptive.c
 * @brief   Preemptive task ISR activation — Entry() skipped, PendSV staged
 * @date    2026-03-14
 *
 * @details When a preemptive LowTask is RUNNING and an ISR activates a
 *          higher-priority HighTask, the kernel does full state setup but
 *          the ISR guard in os_dispatch_task skips Entry().  PendSV is
 *          staged for the context switch after ISR return.
 *
 *          ThreadX ref: tx_timer_interrupt.S — SysTick never calls entry.
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
    { "LowTask",  low_task_entry,  2u, 1u, 0u, TRUE, FULL },
    { "HighTask", high_task_entry, 1u, 1u, 0u, TRUE, FULL }
};

void setUp(void)
{
    high_task_runs = 0u;
}

void tearDown(void) {}

/**
 * @spec ISR guard in os_dispatch_task skips Entry() during ISR context.
 * @requirement HighTask Entry() shall NOT be called from ISR.
 * @verify high_task_runs == 0 after ISR exit.
 */
void test_preemptive_isr_activation_skips_entry(void)
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
    TEST_ASSERT_EQUAL(E_OK, Os_TestSetCurrentTaskRunning(LOW_TASK_ID));

    Os_PortEnterIsr2();
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(HIGH_TASK_ID));
    Os_PortExitIsr2();

    TEST_ASSERT_EQUAL_UINT8(0u, high_task_runs);
}

/**
 * @spec Kernel state updated despite Entry() skip.
 * @requirement os_dispatch_task shall set SelectedNextTask for PendSV.
 * @verify SelectedNextTask == HIGH_TASK_ID after ISR exit.
 */
void test_preemptive_isr_activation_stages_pendsv(void)
{
    uint8 low_stack[128];
    uint8 high_stack[128];
    const Os_Port_Stm32_StateType* state;

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
    TEST_ASSERT_EQUAL(E_OK, Os_TestSetCurrentTaskRunning(LOW_TASK_ID));

    Os_PortEnterIsr2();
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(HIGH_TASK_ID));
    Os_PortExitIsr2();

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(HIGH_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_TRUE(state->PendSvPending);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_preemptive_isr_activation_skips_entry);
    RUN_TEST(test_preemptive_isr_activation_stages_pendsv);
    return UNITY_END();
}
