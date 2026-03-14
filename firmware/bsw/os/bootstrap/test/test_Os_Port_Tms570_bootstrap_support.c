/**
 * @file    test_Os_Port_Tms570_bootstrap_support.c
 * @brief   Shared support for TMS570 Cortex-R5 bootstrap OS port tests
 * @date    2026-03-13
 */
#include "test_Os_Port_Tms570_bootstrap_support.h"

uint8 dummy_task_runs;
uint8 scheduler_bridge_low_runs;
uint8 scheduler_bridge_high_runs;
StatusType scheduler_bridge_activate_status;
uint8 isr_bridge_low_runs;
uint8 isr_bridge_high_runs;
StatusType isr_bridge_invoke_status;
StatusType isr_bridge_activate_status;

void dummy_task_entry(void)
{
    dummy_task_runs++;
}

void dummy_task_entry_alt(void)
{
    dummy_task_runs++;
}

void scheduler_bridge_high_task(void)
{
    scheduler_bridge_high_runs++;
}

void scheduler_bridge_low_task(void)
{
    scheduler_bridge_low_runs++;
    scheduler_bridge_activate_status = ActivateTask(OS_PORT_TMS570_SECOND_TASK_ID);
}

void isr_bridge_high_task(void)
{
    isr_bridge_high_runs++;
}

void isr_bridge_isr_activate_high(void)
{
    isr_bridge_activate_status = ActivateTask(OS_PORT_TMS570_SECOND_TASK_ID);
}

void isr_bridge_low_task(void)
{
    isr_bridge_low_runs++;
    isr_bridge_invoke_status = Os_TestInvokeIsrCat2(isr_bridge_isr_activate_high);
}

void prepare_running_first_task_for_fiq_tests(void)
{
    static uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
}

const Os_TaskConfigType os_port_tms570_binding_tasks[] = {
    { "TaskA", dummy_task_entry, 1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

const Os_TaskConfigType os_port_tms570_scheduler_bridge_tasks[] = {
    { "LowTask", scheduler_bridge_low_task, 2u, 1u, 0u, FALSE, FULL },
    { "HighTask", scheduler_bridge_high_task, 1u, 1u, 0u, FALSE, FULL }
};

const Os_TaskConfigType os_port_tms570_isr_bridge_tasks[] = {
    { "LowTask", isr_bridge_low_task, 2u, 1u, 0u, FALSE, FULL },
    { "HighTask", isr_bridge_high_task, 1u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    dummy_task_runs = 0u;
    scheduler_bridge_low_runs = 0u;
    scheduler_bridge_high_runs = 0u;
    scheduler_bridge_activate_status = E_OK;
    isr_bridge_low_runs = 0u;
    isr_bridge_high_runs = 0u;
    isr_bridge_invoke_status = E_OK;
    isr_bridge_activate_status = E_OK;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(os_port_tms570_binding_tasks, (uint8)(sizeof(os_port_tms570_binding_tasks) /
                                                                    sizeof(os_port_tms570_binding_tasks[0]))));
}

void tearDown(void)
{
}
