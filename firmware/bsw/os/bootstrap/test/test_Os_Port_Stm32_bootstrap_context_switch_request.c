/**
 * @file    test_Os_Port_Stm32_bootstrap_context_switch_request.c
 * @brief   Context switch request and configured dispatch tests for the STM32 Cortex-M4 bootstrap OS port
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

static void dummy_task_entry(void)
{
    dummy_task_runs++;
}

static void dummy_task_entry_alt(void)
{
    dummy_task_runs++;
}

static const Os_TaskConfigType ctx_switch_tasks[] = {
    { "TaskA", dummy_task_entry,     1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    dummy_task_runs = 0u;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(ctx_switch_tasks, (uint8)(sizeof(ctx_switch_tasks) /
                                                        sizeof(ctx_switch_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap STM32 port shall pend a context switch request
 *              only after the first task has started.
 * @verify RequestContextSwitch sets PendSV pending and increments the
 *         request counter only in running state.
 */
void test_Os_Port_Stm32_request_context_switch_pends_pendsv_after_start(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    Os_PortRequestContextSwitch();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
    TEST_ASSERT_FALSE(state->PendSvPending);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_PrepareFirstTask(OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();
    Os_PortRequestContextSwitch();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);

    Os_PortRequestContextSwitch();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * @requirement The shared configured-dispatch binding shall reject a task
 *              that exists in OS config but does not yet have a prepared
 *              STM32 task context.
 * @verify RequestConfiguredDispatch returns E_OS_VALUE and leaves PendSV
 *         request state unchanged for an unprepared configured task.
 */
void test_Os_Port_Stm32_request_configured_dispatch_rejects_unprepared_task_context(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_PrepareConfiguredFirstTask(OS_PORT_STM32_FIRST_TASK_ID, stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Port_RequestConfiguredDispatch(OS_PORT_STM32_SECOND_TASK_ID));
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_context_save.S
 * @requirement The shared configured-dispatch binding shall select the
 *              prepared configured task and pend PendSV through the STM32
 *              port contract.
 * @verify RequestConfiguredDispatch records the selected task and marks a
 *         pending PendSV request once the first task is running.
 */
void test_Os_Port_Stm32_request_configured_dispatch_selects_task_and_pends_switch(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, (uintptr_t)(&first_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_STM32_SECOND_TASK_ID, (uintptr_t)(&second_stack[128])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_STM32_SECOND_TASK_ID));
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_context_restore.S
 * @requirement The shared configured-dispatch completion helper shall be
 *              safe to call when no STM32 PendSV handoff is pending.
 * @verify CompleteConfiguredDispatch returns E_OS_NOFUNC and leaves the
 *         bootstrap PendSV state unchanged when no switch is pending.
 */
void test_Os_Port_Stm32_complete_configured_dispatch_returns_e_os_nofunc_when_idle(void)
{
    uint8 stack_storage[128];
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, (uintptr_t)(&stack_storage[128])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Port_CompleteConfiguredDispatch());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvCompleteCount);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_request_context_switch_pends_pendsv_after_start);
    RUN_TEST(test_Os_Port_Stm32_request_configured_dispatch_rejects_unprepared_task_context);
    RUN_TEST(test_Os_Port_Stm32_request_configured_dispatch_selects_task_and_pends_switch);
    RUN_TEST(test_Os_Port_Stm32_complete_configured_dispatch_returns_e_os_nofunc_when_idle);
    return UNITY_END();
}
