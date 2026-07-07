/**
 * @file    test_Os_Port_Stm32_bootstrap_kernel_bridge.c
 * @brief   Kernel scheduler bridge tests for the STM32 Cortex-M4 bootstrap OS port
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

static uint8 dummy_task_runs;
static uint8 scheduler_bridge_low_runs;
static uint8 scheduler_bridge_high_runs;
static StatusType scheduler_bridge_activate_status;

static void dummy_task_entry(void)
{
    dummy_task_runs++;
}

static void dummy_task_entry_alt(void)
{
    dummy_task_runs++;
}

static void scheduler_bridge_high_task(void)
{
    scheduler_bridge_high_runs++;
}

static void scheduler_bridge_low_task(void)
{
    scheduler_bridge_low_runs++;
    scheduler_bridge_activate_status = ActivateTask(OS_PORT_STM32_SECOND_TASK_ID);
}

static const Os_TaskConfigType kernel_bridge_binding_tasks[] = {
    { "TaskA", dummy_task_entry,     1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

static const Os_TaskConfigType kernel_bridge_scheduler_tasks[] = {
    { "LowTask",  scheduler_bridge_low_task,  2u, 1u, 0u, FALSE, FULL },
    { "HighTask", scheduler_bridge_high_task, 1u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    dummy_task_runs = 0u;
    scheduler_bridge_low_runs = 0u;
    scheduler_bridge_high_runs = 0u;
    scheduler_bridge_activate_status = E_OK;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(kernel_bridge_binding_tasks,
                              (uint8)(sizeof(kernel_bridge_binding_tasks) /
                                      sizeof(kernel_bridge_binding_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * @requirement The portable bootstrap scheduler shall publish the configured
 *              task it dispatches into the STM32 port observation seam.
 * @verify A host-side StartOS plus ActivateTask dispatch increments the
 *         observed kernel dispatch count and records the dispatched task ID.
 */
void test_Os_Port_Stm32_kernel_scheduler_publishes_dispatch_to_port_state(void)
{
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_STM32_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(1u, dummy_task_runs);
    TEST_ASSERT_EQUAL_UINT32(1u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->LastObservedKernelTask);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_FALSE(state->PendSvPending);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * @requirement When the STM32 bootstrap port already owns a prepared and
 *              started first task, the portable scheduler shall synchronize
 *              that current task without leaving a stale selected-next-task.
 * @verify A first portable dispatch with an already-started prepared task
 *         keeps CurrentTask aligned and leaves no pending next-task latch.
 */
void test_Os_Port_Stm32_kernel_first_dispatch_synchronizes_started_task_without_stale_selection(void)
{
    uint8 first_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, (uintptr_t)(&first_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_STM32_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->LastObservedKernelTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_FALSE(state->PendSvPending);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_context_save.S
 * @requirement A portable-kernel preemption from one configured task to a
 *              higher-priority configured task shall arm the STM32 port
 *              handoff seam when both target contexts are prepared.
 * @verify Dispatching a low task that activates a higher-priority task
 *         causes the scheduler to request PendSV for the prepared high task.
 */
void test_Os_Port_Stm32_kernel_preemption_arms_target_handoff_for_prepared_task(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            kernel_bridge_scheduler_tasks,
            (uint8)(sizeof(kernel_bridge_scheduler_tasks) /
                    sizeof(kernel_bridge_scheduler_tasks[0]))));
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

    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(2u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->LastObservedKernelTask);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_context_restore.S
 * @requirement A scheduler-driven configured-task preemption shall complete
 *              through the STM32 PendSV restore path once the deferred port
 *              handler runs.
 * @verify After portable-kernel preemption arms the handoff, the shared
 *         completion helper drives PendSV restore, switches the bootstrap
 *         current task to the prepared high task, and clears the
 *         selected-next-task state.
 */
void test_Os_Port_Stm32_kernel_preemption_completes_through_pendsv_handler(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            kernel_bridge_scheduler_tasks,
            (uint8)(sizeof(kernel_bridge_scheduler_tasks) /
                    sizeof(kernel_bridge_scheduler_tasks[0]))));
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
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_kernel_scheduler_publishes_dispatch_to_port_state);
    RUN_TEST(test_Os_Port_Stm32_kernel_first_dispatch_synchronizes_started_task_without_stale_selection);
    RUN_TEST(test_Os_Port_Stm32_kernel_preemption_arms_target_handoff_for_prepared_task);
    RUN_TEST(test_Os_Port_Stm32_kernel_preemption_completes_through_pendsv_handler);
    return UNITY_END();
}
