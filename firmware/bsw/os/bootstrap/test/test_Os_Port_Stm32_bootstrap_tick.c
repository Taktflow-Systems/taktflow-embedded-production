/**
 * @file    test_Os_Port_Stm32_bootstrap_tick.c
 * @brief   Tick ISR and SysTick handler tests for the STM32 Cortex-M4 bootstrap OS port
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
#define ALARM_ACTIVATE               ((AlarmType)0u)

static uint8 dummy_task_runs;
static uint8 scheduler_bridge_low_runs;
static uint8 scheduler_bridge_high_runs;
static StatusType scheduler_bridge_activate_status;

static void dummy_task_entry(void)
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

static const Os_TaskConfigType tick_binding_tasks[] = {
    { "TaskA", dummy_task_entry, 1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry, 2u, 1u, 0u, FALSE, FULL }
};

static const Os_TaskConfigType tick_scheduler_tasks[] = {
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
        Os_TestConfigureTasks(tick_binding_tasks, (uint8)(sizeof(tick_binding_tasks) /
                                                          sizeof(tick_binding_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 * @requirement The bootstrap STM32 tick ISR shall route into the OSEK
 *              counter/alarm path without inventing a switch request when
 *              no alarm expiry or ready-task transition occurs.
 * @verify TickIsr increments the bootstrap counter and leaves PendSV idle
 *         when no alarm expires on that tick.
 */
void test_Os_Port_Stm32_tick_isr_counts_ticks_without_spurious_dispatch_when_no_alarm_expires(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_PrepareFirstTask(OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    Os_Port_Stm32_TickIsr();
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/example_build/tx_initialize_low_level.S
 * @requirement The bootstrap STM32 SysTick entry shall route through the
 *              same timer-interrupt path used by the low-level Cortex-M4
 *              port structure and wake prepared configured tasks when an
 *              alarm expires.
 * @verify SysTickHandler advances the counter, arms PendSV for the ready
 *         low task, and the shared run-to-idle helper then completes the
 *         prepared low-to-high handoff sequence.
 */
void test_Os_Port_Stm32_systick_handler_routes_alarm_expiry_into_prepared_dispatch(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_STM32_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            tick_scheduler_tasks,
            (uint8)(sizeof(tick_scheduler_tasks) /
                    sizeof(tick_scheduler_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, (uintptr_t)(&first_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_STM32_SECOND_TASK_ID, (uintptr_t)(&second_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));
    Os_Port_Stm32_SysTickHandler();
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_EQUAL_UINT8(0u, state->Isr2Nesting);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_FALSE(state->DeferredPendSv);
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
    /* SysTick stages dispatch for PendSV — Entry() NOT called from ISR.
     * ThreadX ref: SysTick never calls thread entry. */
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_high_runs);

    /* RunToIdle drives PendSV simulation + synchronous dispatch loop */
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_FALSE(state->PendSvPending);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/example_build/tx_initialize_low_level.S
 * @requirement The bootstrap STM32 SysTick entry shall follow the same ISR
 *              nesting deferral rule as other Cat2 ISR paths when an alarm
 *              wakes a prepared task inside nested interrupt processing.
 * @verify SysTickHandler inside nested ISR leaves PendSV deferred until the
 *         outermost exit, after which the shared run-to-idle helper
 *         completes the prepared low-to-high handoff.
 */
void test_Os_Port_Stm32_systick_inside_isr_defers_dispatch_until_outer_exit(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_STM32_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            tick_scheduler_tasks,
            (uint8)(sizeof(tick_scheduler_tasks) /
                    sizeof(tick_scheduler_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, (uintptr_t)(&first_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_STM32_SECOND_TASK_ID, (uintptr_t)(&second_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));

    Os_PortEnterIsr2();
    Os_PortEnterIsr2();
    Os_Port_Stm32_SysTickHandler();
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_EQUAL_UINT8(2u, state->Isr2Nesting);
    TEST_ASSERT_EQUAL_UINT8(2u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_TRUE(state->DeferredPendSv);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_high_runs);

    Os_PortExitIsr2();
    Os_PortExitIsr2();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->Isr2Nesting);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_FALSE(state->DeferredPendSv);
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
    /* Nested ISR exit stages dispatch for PendSV — Entry() NOT called.
     * ThreadX ref: nested ISR exit only fires PENDSVSET at outermost. */
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_high_runs);

    /* RunToIdle drives PendSV simulation + synchronous dispatch loop */
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_tick_isr_counts_ticks_without_spurious_dispatch_when_no_alarm_expires);
    RUN_TEST(test_Os_Port_Stm32_systick_handler_routes_alarm_expiry_into_prepared_dispatch);
    RUN_TEST(test_Os_Port_Stm32_systick_inside_isr_defers_dispatch_until_outer_exit);
    return UNITY_END();
}
