/**
 * @file    test_Os_Port_Stm32_bootstrap_task_binding.c
 * @brief   Task binding tests for the STM32 Cortex-M4 bootstrap OS port
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

static const Os_TaskConfigType task_binding_tasks[] = {
    { "TaskA", dummy_task_entry,     1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    dummy_task_runs = 0u;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(task_binding_tasks, (uint8)(sizeof(task_binding_tasks) /
                                                          sizeof(task_binding_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_stack_build.S
 * @requirement The bootstrap STM32 port shall be able to prepare task
 *              contexts directly from the configured OSEK task model rather
 *              than from ad-hoc test entry pointers.
 * @verify Configured tasks from Os_TestConfigureTasks are bound into both
 *         first-task and secondary prepared STM32 task contexts.
 */
void test_Os_Port_Stm32_task_binding_prepares_contexts_from_configured_tasks(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_Port_Stm32_StateType* state;
    const Os_Port_Stm32_TaskContextType* first_ctx;
    const Os_Port_Stm32_TaskContextType* second_ctx;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(task_binding_tasks, (uint8)(sizeof(task_binding_tasks) /
                                                          sizeof(task_binding_tasks[0]))));
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, (uintptr_t)(&first_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_STM32_SECOND_TASK_ID, (uintptr_t)(&second_stack[128])));

    state = Os_Port_Stm32_GetBootstrapState();
    first_ctx = Os_Port_Stm32_GetTaskContext(OS_PORT_STM32_FIRST_TASK_ID);
    second_ctx = Os_Port_Stm32_GetTaskContext(OS_PORT_STM32_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->FirstTaskTaskID);
    TEST_ASSERT_TRUE(first_ctx->Prepared);
    TEST_ASSERT_TRUE(second_ctx->Prepared);
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry) == ((uintptr_t)first_ctx->Entry));
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry_alt) == ((uintptr_t)second_ctx->Entry));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_task_binding_prepares_contexts_from_configured_tasks);
    return UNITY_END();
}
