/**
 * @file    test_Os_Port_Stm32_bootstrap_init.c
 * @brief   Target init tests for the STM32 Cortex-M4 bootstrap OS port
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

static const Os_TaskConfigType init_tasks[] = {
    { "TaskA", dummy_task_entry,     1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    dummy_task_runs = 0u;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(init_tasks, (uint8)(sizeof(init_tasks) /
                                                  sizeof(init_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/inc/tx_port.h
 * @requirement The bootstrap STM32 port shall initialize its basic
 *              scheduling and exception state before first-task launch.
 * @verify Target init configures bootstrap SysTick/PendSV policy and clears
 *         first-task and dispatch state.
 */
void test_Os_Port_Stm32_target_init_sets_bootstrap_exception_state(void)
{
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_TRUE(state->TargetInitialized);
    TEST_ASSERT_TRUE(state->SysTickConfigured);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_FALSE(state->FirstTaskPrepared);
    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_FALSE(state->DeferredPendSv);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, state->PendSvPriority);
    TEST_ASSERT_EQUAL_HEX8(0x40u, state->SysTickPriority);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->LastObservedKernelTask);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_target_init_sets_bootstrap_exception_state);
    return UNITY_END();
}
