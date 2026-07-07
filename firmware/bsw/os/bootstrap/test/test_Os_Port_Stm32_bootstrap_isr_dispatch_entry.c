/**
 * @file    test_Os_Port_Stm32_bootstrap_isr_dispatch_entry.c
 * @brief   ISR dispatch Entry() guard tests for the STM32 bootstrap OS port
 * @date    2026-03-14
 *
 * @details Verifies that when the OSEK scheduler dispatches a higher-priority
 *          task from within ISR context (e.g. Os_BootstrapExitIsr2 calling
 *          os_maybe_dispatch_preemption), the task Entry() function is NOT
 *          called directly.  Instead, all state setup occurs (push preempted
 *          task, set CurrentTask, stage SelectedNextTask) and PendSV handles
 *          the actual context switch after ISR return.
 *
 *          ThreadX reference: tx_timer_interrupt.S — SysTick never calls task
 *          entry.  Only sets _tx_timer_expired + PENDSVSET.  PendSV reads
 *          _tx_thread_execute_ptr set by the C scheduler.
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

#define FIRST_TASK_ID   ((TaskType)0u)
#define SECOND_TASK_ID  ((TaskType)1u)

static uint8 low_task_runs;
static uint8 high_task_runs;

static void low_task_entry(void)
{
    low_task_runs++;
}

static void high_task_entry(void)
{
    high_task_runs++;
}

static const Os_TaskConfigType isr_guard_tasks[] = {
    { "LowTask",  low_task_entry,  2u, 1u, 0u, TRUE, FULL },
    { "HighTask",  high_task_entry, 1u, 1u, 0u, TRUE, FULL }
};

void setUp(void)
{
    low_task_runs = 0u;
    high_task_runs = 0u;
}

void tearDown(void) {}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 *       SysTick ISR sets _tx_timer_expired and PENDSVSET — never calls a
 *       task entry function from exception context.
 * @requirement When Os_BootstrapExitIsr2 triggers preemption dispatch while
 *              the port-level ISR context is still active (Isr2Nesting > 0),
 *              the kernel shall set up the task state but shall NOT call the
 *              task Entry() function.
 * @verify After an ISR activates a high-priority task and ExitIsr2 runs the
 *         preemption check, high_task_runs remains 0 (Entry not called) but
 *         SelectedNextTask is staged for PendSV.
 */
void test_Os_Port_Stm32_dispatch_from_isr_context_skips_entry_call(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(E_OK,
        Os_TestConfigureTasks(isr_guard_tasks,
            (uint8)(sizeof(isr_guard_tasks) / sizeof(isr_guard_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredFirstTask(FIRST_TASK_ID,
            (uintptr_t)(&first_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredTask(SECOND_TASK_ID,
            (uintptr_t)(&second_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    /* Set LowTask as RUNNING without calling its Entry().
     * This simulates a task that was launched by PendSV and is
     * executing on its own PSP — the normal state when SysTick fires. */
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestSetCurrentTaskRunning(FIRST_TASK_ID));

    /* Now simulate: enter ISR, activate high-priority task, exit ISR.
     * On real hardware, ExitIsr2 must NOT call high_task_entry(). */
    Os_PortEnterIsr2();
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(SECOND_TASK_ID));
    Os_PortExitIsr2();

    /* Key assertion: Entry() was NOT called from ISR context */
    TEST_ASSERT_EQUAL_UINT8(0u, high_task_runs);

    /* But the dispatch was staged for PendSV */
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL(SECOND_TASK_ID, state->SelectedNextTask);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Os_Port_Stm32_dispatch_from_isr_context_skips_entry_call);

    return UNITY_END();
}
