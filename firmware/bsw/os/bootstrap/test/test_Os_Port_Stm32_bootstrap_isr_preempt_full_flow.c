/**
 * @file    test_Os_Port_Stm32_bootstrap_isr_preempt_full_flow.c
 * @brief   Full ISR preemption flow — LowTask runs, ISR activates HighTask
 * @date    2026-03-14
 *
 * @details Non-preemptive LowTask calls Os_TestInvokeIsrCat2 which activates
 *          HighTask.  After LowTask completes, os_run_ready_tasks dispatches
 *          HighTask synchronously (unit test, no real PendSV).
 *
 *          ThreadX ref: _tx_thread_system_resume — after current thread
 *          completes, scheduler picks next from priority list.
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

static uint8 low_task_runs;
static uint8 high_task_runs;
static StatusType isr_activate_status;

static void high_task_entry(void)
{
    high_task_runs++;
}

static void isr_handler_activate_high(void)
{
    isr_activate_status = ActivateTask(HIGH_TASK_ID);
}

static void low_task_entry(void)
{
    low_task_runs++;
    (void)Os_TestInvokeIsrCat2(isr_handler_activate_high);
}

static const Os_TaskConfigType tasks[] = {
    { "LowTask",  low_task_entry,  2u, 1u, 0u, FALSE, NON },
    { "HighTask", high_task_entry, 1u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    low_task_runs = 0u;
    high_task_runs = 0u;
    isr_activate_status = E_OK;
}

void tearDown(void) {}

/**
 * @spec After non-preemptive LowTask completes, HighTask dispatches.
 * @requirement os_run_ready_tasks loop shall dispatch HighTask after
 *              LowTask Entry() returns.
 * @verify low_task_runs == 1, high_task_runs == 1.
 */
void test_low_task_runs_first(void)
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
    TEST_ASSERT_EQUAL(E_OK, isr_activate_status);
}

/**
 * @spec HighTask dispatched after LowTask completes.
 * @requirement The os_run_ready_tasks loop picks up HighTask.
 * @verify high_task_runs == 1 after Os_TestRunReadyTasks.
 */
void test_high_task_dispatched_after_low_completes(void)
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

    TEST_ASSERT_EQUAL_UINT8(1u, high_task_runs);
}

/**
 * @spec Port state after full flow.
 * @requirement PendSvPending set during ISR, ISR nesting back to 0.
 * @verify Isr2Nesting == 0, DeferredPendSv == FALSE.
 */
void test_port_state_after_full_flow(void)
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
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->Isr2Nesting);
    TEST_ASSERT_FALSE(state->DeferredPendSv);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_low_task_runs_first);
    RUN_TEST(test_high_task_dispatched_after_low_completes);
    RUN_TEST(test_port_state_after_full_flow);
    return UNITY_END();
}
