/**
 * @file    test_Os_Port_Stm32_bootstrap_pendsv.c
 * @brief   PendSV handler tests for the STM32 Cortex-M4 bootstrap OS port
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

static const Os_TaskConfigType pendsv_tasks[] = {
    { "TaskA", dummy_task_entry,     1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    dummy_task_runs = 0u;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(pendsv_tasks, (uint8)(sizeof(pendsv_tasks) /
                                                    sizeof(pendsv_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap STM32 PendSV handler shall clear the pending
 *              request and track completion when a context switch runs.
 * @verify PendSvHandler clears pending state, records completion, and keeps
 *         the prepared PSP as the active thread stack pointer.
 */
void test_Os_Port_Stm32_pendsv_handler_clears_pending_and_tracks_completion(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    uintptr_t expected_active_psp;
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_PrepareFirstTask(OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, stack_top));
    expected_active_psp = Os_Port_Stm32_GetTaskContext(OS_PORT_STM32_FIRST_TASK_ID)->RestorePsp;
    Os_PortStartFirstTask();
    Os_PortRequestContextSwitch();

    Os_Port_Stm32_PendSvHandler();
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)expected_active_psp, (void*)state->ActivePsp);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * @requirement The bootstrap STM32 PendSV path shall be able to restore a
 *              different prepared task context after the first task is
 *              running.
 * @verify Selecting a second prepared PSP causes PendSV to save the
 *         current task PSP, activate the selected task PSP, and count a
 *         task-to-task switch.
 */
void test_Os_Port_Stm32_pendsv_handler_switches_to_selected_next_task_context(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[128]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[128]);
    uintptr_t expected_second_active_psp;
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Stm32_PrepareFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Stm32_PrepareTaskContext(
                          OS_PORT_STM32_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    expected_second_active_psp = Os_Port_Stm32_GetTaskContext(OS_PORT_STM32_SECOND_TASK_ID)->RestorePsp;

    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_STM32_SECOND_TASK_ID));
    Os_Port_Stm32_PendSvHandler();
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)Os_Port_Stm32_GetTaskContext(OS_PORT_STM32_FIRST_TASK_ID)->SavedPsp,
                          (void*)state->LastSavedPsp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_second_active_psp, (void*)state->ActivePsp);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_PTR((void*)0u, (void*)state->SelectedNextTaskPsp);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap STM32 port shall preserve the saved PSP of a
 *              switched-out task so it can be selected again on a later
 *              PendSV restore.
 * @verify After switching from task A to task B, the saved PSP of task A
 *         can be selected to switch back on the next PendSV.
 */
void test_Os_Port_Stm32_saved_psp_can_be_selected_to_switch_back(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[128]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[128]);
    uintptr_t expected_first_active_psp;
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Stm32_PrepareFirstTask(
                          OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Stm32_PrepareTaskContext(
                          OS_PORT_STM32_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));

    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_STM32_SECOND_TASK_ID));
    Os_Port_Stm32_PendSvHandler();

    expected_first_active_psp = Os_Port_Stm32_GetTaskContext(OS_PORT_STM32_FIRST_TASK_ID)->RestorePsp;
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_STM32_FIRST_TASK_ID));
    Os_Port_Stm32_PendSvHandler();
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(2u, state->PendSvCompleteCount);
    TEST_ASSERT_EQUAL_UINT32(2u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)Os_Port_Stm32_GetTaskContext(OS_PORT_STM32_SECOND_TASK_ID)->SavedPsp,
                          (void*)state->LastSavedPsp);
    TEST_ASSERT_EQUAL_PTR((void*)expected_first_active_psp, (void*)state->ActivePsp);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_pendsv_handler_clears_pending_and_tracks_completion);
    RUN_TEST(test_Os_Port_Stm32_pendsv_handler_switches_to_selected_next_task_context);
    RUN_TEST(test_Os_Port_Stm32_saved_psp_can_be_selected_to_switch_back);
    return UNITY_END();
}
