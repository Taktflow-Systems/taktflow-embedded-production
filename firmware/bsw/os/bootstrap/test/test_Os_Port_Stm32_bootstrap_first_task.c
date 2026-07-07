/**
 * @file    test_Os_Port_Stm32_bootstrap_first_task.c
 * @brief   First-task prepare/start tests for the STM32 Cortex-M4 bootstrap OS port
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

#define OS_PORT_STM32_INITIAL_FRAME_WORDS  17u
#define OS_PORT_STM32_INITIAL_FRAME_BYTES  (OS_PORT_STM32_INITIAL_FRAME_WORDS * sizeof(uint32))
#define OS_PORT_STM32_INITIAL_EXC_RETURN   0xFFFFFFFDu
#define OS_PORT_STM32_INITIAL_TASK_LR      0xFFFFFFFFu
#define OS_PORT_STM32_FIRST_TASK_ID        ((TaskType)0u)
#define OS_PORT_STM32_SECOND_TASK_ID       ((TaskType)1u)

static uint8 dummy_task_runs;

static void dummy_task_entry(void)
{
    dummy_task_runs++;
}

static void dummy_task_entry_alt(void)
{
    dummy_task_runs++;
}

static const Os_TaskConfigType first_task_tasks[] = {
    { "TaskA", dummy_task_entry,     1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

void setUp(void)
{
    dummy_task_runs = 0u;
    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(first_task_tasks, (uint8)(sizeof(first_task_tasks) /
                                                        sizeof(first_task_tasks[0]))));
}

void tearDown(void)
{
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_stack_build.S
 * @requirement The bootstrap STM32 port shall build metadata for the first
 *              synthetic exception frame before first task start.
 * @verify Preparing the first task records entry address, xPSR, stack top,
 *         and derived PSP value.
 */
void test_Os_Port_Stm32_prepare_first_task_builds_initial_frame_metadata(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    uintptr_t expected_psp =
        (uintptr_t)((stack_top - (uintptr_t)OS_PORT_STM32_INITIAL_FRAME_BYTES) & ~(uintptr_t)0x7u);
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_PrepareFirstTask(OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, stack_top));
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskPrepared);
    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->FirstTaskTaskID);
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry) == state->FirstTaskEntryAddress);
    TEST_ASSERT_EQUAL_PTR((void*)stack_top, (void*)state->FirstTaskStackTop);
    TEST_ASSERT_EQUAL_PTR((void*)expected_psp, (void*)state->FirstTaskPsp);
    TEST_ASSERT_EQUAL_HEX32(0x01000000u, state->InitialXpsr);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_stack_build.S
 * @requirement The bootstrap STM32 port shall lay out the first synthetic
 *              task frame in the same word order used by the ThreadX
 *              Cortex-M4 stack builder.
 * @verify PrepareFirstTask writes EXC_RETURN, software-saved registers,
 *         hardware-saved registers, poisoned task LR, PC, and xPSR at the
 *         expected offsets in the prepared frame.
 */
void test_Os_Port_Stm32_prepare_first_task_builds_threadx_compatible_frame_words(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    uintptr_t expected_psp =
        (uintptr_t)((stack_top - (uintptr_t)OS_PORT_STM32_INITIAL_FRAME_BYTES) & ~(uintptr_t)0x7u);
    uint32* frame = (uint32*)expected_psp;
    uint32 index;

    (void)memset(stack_storage, 0xA5, sizeof(stack_storage));
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_PrepareFirstTask(OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, stack_top));
    /* STMDB-compatible layout: [R4..R11, EXC_RETURN, R0..R3, R12, LR, PC, xPSR] */
    for (index = 0u; index <= 7u; index++) {
        TEST_ASSERT_EQUAL_HEX32(0u, frame[index]);
    }
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_STM32_INITIAL_EXC_RETURN, frame[8]);
    for (index = 9u; index <= 13u; index++) {
        TEST_ASSERT_EQUAL_HEX32(0u, frame[index]);
    }

    TEST_ASSERT_EQUAL_HEX32(OS_PORT_STM32_INITIAL_TASK_LR, frame[14]);
    TEST_ASSERT_EQUAL_HEX32((uint32)((uintptr_t)dummy_task_entry & 0xFFFFFFFFu), frame[15]);
    TEST_ASSERT_EQUAL_HEX32(0x01000000u, frame[16]);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * @requirement The bootstrap STM32 port shall not start the first task until
 *              initial launch metadata exists.
 * @verify StartFirstTask only becomes effective after PrepareFirstTask.
 */
void test_Os_Port_Stm32_start_first_task_requires_prepared_launch_frame(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    uintptr_t expected_active_psp;
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    Os_PortStartFirstTask();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_FALSE(state->FirstTaskStarted);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_PrepareFirstTask(OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, stack_top));
    expected_active_psp = Os_Port_Stm32_GetTaskContext(OS_PORT_STM32_FIRST_TASK_ID)->RestorePsp;
    Os_PortStartFirstTask();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FirstTaskLaunchCount);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)expected_active_psp, (void*)state->ActivePsp);
}

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_thread_schedule.S
 * @requirement The bootstrap STM32 port shall enter the first-task launch
 *              path only once for a prepared task context.
 * @verify A repeated StartFirstTask call does not relaunch or increment the
 *         launch count after the first launch.
 */
void test_Os_Port_Stm32_start_first_task_is_not_relaunched_after_first_start(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_PrepareFirstTask(OS_PORT_STM32_FIRST_TASK_ID, dummy_task_entry, stack_top));

    Os_PortStartFirstTask();
    Os_PortStartFirstTask();
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FirstTaskLaunchCount);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_Port_Stm32_prepare_first_task_builds_initial_frame_metadata);
    RUN_TEST(test_Os_Port_Stm32_prepare_first_task_builds_threadx_compatible_frame_words);
    RUN_TEST(test_Os_Port_Stm32_start_first_task_requires_prepared_launch_frame);
    RUN_TEST(test_Os_Port_Stm32_start_first_task_is_not_relaunched_after_first_start);
    return UNITY_END();
}
