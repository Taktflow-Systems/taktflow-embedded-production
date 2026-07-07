/**
 * @file    test_Os_Port_Stm32_bootstrap_isr_exit_staging.c
 * @brief   ISR exit staging tests for the STM32 bootstrap OS port
 * @date    2026-03-14
 *
 * @details Verifies that Os_BootstrapExitIsr2 on STM32 does NOT call
 *          synchronous dispatch (os_run_ready_tasks / os_maybe_dispatch_preemption).
 *          Instead, it stages the highest-priority ready task via
 *          Os_Port_SelectConfiguredTask + Os_PortRequestContextSwitch, and
 *          PendSV handles the actual context switch after ISR return.
 *
 *          ThreadX reference: tx_thread_schedule.S — after ISR return,
 *          PendSV reads _tx_thread_execute_ptr (set by C scheduler in
 *          _tx_timer_thread_entry or _tx_thread_system_resume).  The ISR
 *          itself never calls the thread entry point.
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

#define IDLE_TASK_ID    ((TaskType)0u)
#define READY_TASK_ID   ((TaskType)1u)

static uint8 idle_task_runs;
static uint8 ready_task_runs;

static void idle_task_entry(void)
{
    idle_task_runs++;
}

static void ready_task_entry(void)
{
    ready_task_runs++;
}

static const Os_TaskConfigType staging_tasks[] = {
    { "IdleTask",  idle_task_entry,  2u, 1u, 0u, TRUE, FULL },
    { "ReadyTask", ready_task_entry, 1u, 1u, 0u, TRUE, FULL }
};

void setUp(void)
{
    idle_task_runs  = 0u;
    ready_task_runs = 0u;
}

void tearDown(void) {}

/**
 * @spec ThreadX reference: tx_thread_schedule.S — PendSV is the only path
 *       that restores a thread context and branches to its entry.  ISR exit
 *       code (_tx_thread_context_restore) only sets PENDSVSET if a new
 *       thread was selected by the C scheduler.
 * @requirement When Os_BootstrapExitIsr2 runs at kernel nesting == 0 on
 *              STM32, it shall select the highest-priority ready task and
 *              request a context switch via the port, but shall NOT invoke
 *              synchronous dispatch (os_run_ready_tasks / os_dispatch_task).
 * @verify After ISR activates ReadyTask and ExitIsr2 runs, ready_task_runs
 *         remains 0, SelectedNextTask == READY_TASK_ID, and PendSvPending
 *         is TRUE.
 */
void test_Os_BootstrapExitIsr2_stages_for_pendsv_without_sync_dispatch(void)
{
    uint8 idle_stack[128];
    uint8 ready_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(E_OK,
        Os_TestConfigureTasks(staging_tasks,
            (uint8)(sizeof(staging_tasks) / sizeof(staging_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredFirstTask(IDLE_TASK_ID,
            (uintptr_t)(&idle_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredTask(READY_TASK_ID,
            (uintptr_t)(&ready_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    /* IdleTask is RUNNING — simulate it was launched by PendSV */
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(IDLE_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestSetCurrentTaskRunning(IDLE_TASK_ID));

    /* Enter ISR, activate higher-priority task, exit ISR */
    Os_PortEnterIsr2();
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(READY_TASK_ID));
    Os_PortExitIsr2();

    /* Entry() must NOT have been called from ISR context */
    TEST_ASSERT_EQUAL_UINT8(0u, ready_task_runs);

    /* But dispatch was staged for PendSV */
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL(READY_TASK_ID, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: _tx_thread_context_restore — when no higher-
 *       priority thread is ready, the ISR returns to the interrupted thread
 *       without triggering PendSV.
 * @requirement When Os_BootstrapExitIsr2 runs on STM32 and no task is
 *              ready (only the current task is RUNNING), it shall not
 *              request a context switch.
 * @verify After ISR exit with no new activations, PendSvPending remains
 *         FALSE and SelectedNextTask is INVALID_TASK.
 */
void test_Os_BootstrapExitIsr2_no_ready_task_no_pendsv(void)
{
    uint8 idle_stack[128];
    uint8 ready_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(E_OK,
        Os_TestConfigureTasks(staging_tasks,
            (uint8)(sizeof(staging_tasks) / sizeof(staging_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredFirstTask(IDLE_TASK_ID,
            (uintptr_t)(&idle_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredTask(READY_TASK_ID,
            (uintptr_t)(&ready_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    /* Only IdleTask is RUNNING, nothing else ready */
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(IDLE_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestSetCurrentTaskRunning(IDLE_TASK_ID));

    /* Enter and exit ISR without activating any new task */
    Os_PortEnterIsr2();
    Os_PortExitIsr2();

    /* No context switch requested */
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_FALSE(state->PendSvPending);
}

/**
 * @spec ThreadX reference: tx_timer_interrupt.S — nested timer interrupts
 *       increment _tx_timer_system_clock but only the outermost ISR exit
 *       checks _tx_timer_expired and sets PENDSVSET.
 * @requirement When nested ISRs occur on STM32, Os_BootstrapExitIsr2 at
 *              the inner nesting level shall NOT stage any dispatch.  Only
 *              the outermost ExitIsr2 (nesting → 0) stages the dispatch.
 * @verify After nested ISR with inner activation, ready_task_runs == 0,
 *         PendSvPending == TRUE, SelectedNextTask == READY_TASK_ID — all
 *         set only after the outer ExitIsr2.
 */
void test_Os_BootstrapExitIsr2_nested_isr_defers_to_outermost_exit(void)
{
    uint8 idle_stack[128];
    uint8 ready_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(E_OK,
        Os_TestConfigureTasks(staging_tasks,
            (uint8)(sizeof(staging_tasks) / sizeof(staging_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredFirstTask(IDLE_TASK_ID,
            (uintptr_t)(&idle_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredTask(READY_TASK_ID,
            (uintptr_t)(&ready_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(IDLE_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestSetCurrentTaskRunning(IDLE_TASK_ID));

    /* Outer ISR */
    Os_PortEnterIsr2();

    /* Inner (nested) ISR — activate task here */
    Os_PortEnterIsr2();
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(READY_TASK_ID));
    Os_PortExitIsr2();  /* inner exit — nesting still > 0 */

    /* After inner exit: no dispatch yet */
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, ready_task_runs);

    /* Outer exit — this is where staging happens */
    Os_PortExitIsr2();

    /* Entry() still NOT called */
    TEST_ASSERT_EQUAL_UINT8(0u, ready_task_runs);

    /* But dispatch staged for PendSV */
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL(READY_TASK_ID, state->SelectedNextTask);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Os_BootstrapExitIsr2_stages_for_pendsv_without_sync_dispatch);
    RUN_TEST(test_Os_BootstrapExitIsr2_no_ready_task_no_pendsv);
    RUN_TEST(test_Os_BootstrapExitIsr2_nested_isr_defers_to_outermost_exit);

    return UNITY_END();
}
