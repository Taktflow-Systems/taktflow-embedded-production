/**
 * @file    test_Os_Port_Stm32_bootstrap_isr_nesting.c
 * @brief   ISR nesting and deferred PendSV tests for the STM32 Cortex-M4 bootstrap OS port
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

static const Os_TaskConfigType isr_nesting_tasks[] = {
    { "TaskA", dummy_task_entry,     1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    dummy_task_runs = 0u;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(isr_nesting_tasks, (uint8)(sizeof(isr_nesting_tasks) /
                                                         sizeof(isr_nesting_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 * @requirement The bootstrap STM32 port shall defer PendSV assertion until
 *              the final ISR2 exit when dispatch was requested in nested ISR.
 * @verify A nested ISR request is remembered and converted to PendSV only
 *         on the outermost ISR exit.
 */
void test_Os_Port_Stm32_nested_isr_exit_releases_deferred_pendsv_request(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_PrepareFirstTask(OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();

    Os_PortEnterIsr2();
    Os_PortEnterIsr2();
    Os_PortRequestContextSwitch();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_TRUE(state->DeferredPendSv);
    TEST_ASSERT_EQUAL_UINT8(2u, state->Isr2Nesting);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);

    Os_PortExitIsr2();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT8(1u, state->Isr2Nesting);

    Os_PortExitIsr2();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_FALSE(state->DeferredPendSv);
    TEST_ASSERT_EQUAL_UINT8(0u, state->Isr2Nesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_nested_isr_exit_releases_deferred_pendsv_request);
    return UNITY_END();
}
