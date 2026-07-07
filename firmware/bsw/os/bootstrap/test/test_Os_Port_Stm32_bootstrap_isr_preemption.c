/**
 * @file    test_Os_Port_Stm32_bootstrap_isr_preemption.c
 * @brief   ISR2 preemption flow tests for the STM32 Cortex-M4 bootstrap OS port
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

static uint8 isr_bridge_low_runs;
static uint8 isr_bridge_high_runs;
static StatusType isr_bridge_invoke_status;
static StatusType isr_bridge_activate_status;

static void isr_bridge_high_task(void)
{
    isr_bridge_high_runs++;
}

static void isr_bridge_isr_activate_high(void)
{
    isr_bridge_activate_status = ActivateTask(OS_PORT_STM32_SECOND_TASK_ID);
}

static void isr_bridge_low_task(void)
{
    isr_bridge_low_runs++;
    isr_bridge_invoke_status = Os_TestInvokeIsrCat2(isr_bridge_isr_activate_high);
}

static const Os_TaskConfigType isr_preemption_tasks[] = {
    { "LowTask",  isr_bridge_low_task,  2u, 1u, 0u, FALSE, FULL },
    { "HighTask", isr_bridge_high_task, 1u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    isr_bridge_low_runs = 0u;
    isr_bridge_high_runs = 0u;
    isr_bridge_invoke_status = E_OK;
    isr_bridge_activate_status = E_OK;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(isr_preemption_tasks, (uint8)(sizeof(isr_preemption_tasks) /
                                                            sizeof(isr_preemption_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 * @requirement A kernel Cat2 ISR preemption shall exercise the STM32 target
 *              ISR nesting and deferred-dispatch release seam before the
 *              PendSV completion path runs.
 * @verify A low task that activates a high task from Os_TestInvokeIsrCat2
 *         leaves PendSV requested for the prepared high task after ISR exit,
 *         and the shared completion helper then completes the switch.
 */
void test_Os_Port_Stm32_isr2_preemption_flows_through_target_exit_and_pendsv(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            isr_preemption_tasks,
            (uint8)(sizeof(isr_preemption_tasks) /
                    sizeof(isr_preemption_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, (uintptr_t)(&first_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_STM32_SECOND_TASK_ID, (uintptr_t)(&second_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_STM32_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK, isr_bridge_invoke_status);
    TEST_ASSERT_EQUAL(E_OK, isr_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_bridge_low_runs);
    /* Both tasks FULL (preemptive).  os_maybe_dispatch_preemption during
     * ISR exit set os_current_task=HighTask, pushed LowTask.  ISR guard
     * skipped Entry().  HighTask needs PendSV (CompletePortDispatches). */
    TEST_ASSERT_EQUAL_UINT8(0u, isr_bridge_high_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, state->Isr2Nesting);
    TEST_ASSERT_FALSE(state->DeferredPendSv);
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->SelectedNextTask);

    /* Simulate PendSV completing the context switch */
    TEST_ASSERT_EQUAL(E_OK, Os_TestCompletePortDispatches());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_FALSE(state->PendSvPending);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_isr2_preemption_flows_through_target_exit_and_pendsv);
    return UNITY_END();
}
