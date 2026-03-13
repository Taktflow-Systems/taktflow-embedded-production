/**
 * @file    test_Os_Port_Stm32_bootstrap.c
 * @brief   Unit tests for the STM32 Cortex-M4 bootstrap OS port
 * @date    2026-03-13
 */
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "Os.h"
#include "Os_Port_Stm32.h"
#include "Os_Port_TaskBinding.h"

#define OS_PORT_STM32_INITIAL_FRAME_WORDS  17u
#define OS_PORT_STM32_INITIAL_FRAME_BYTES  (OS_PORT_STM32_INITIAL_FRAME_WORDS * sizeof(uint32))
#define OS_PORT_STM32_SOFTWARE_RESTORE_BYTES  (9u * sizeof(uint32))
#define OS_PORT_STM32_INITIAL_EXC_RETURN   0xFFFFFFFDu
#define OS_PORT_STM32_INITIAL_TASK_LR      0xFFFFFFFFu
#define OS_PORT_STM32_FIRST_TASK_ID        ((TaskType)0u)
#define OS_PORT_STM32_SECOND_TASK_ID       ((TaskType)1u)
#define ALARM_ACTIVATE                     ((AlarmType)0u)

static uint8 dummy_task_runs;
static uint8 scheduler_bridge_low_runs;
static uint8 scheduler_bridge_high_runs;
static StatusType scheduler_bridge_activate_status;
static uint8 isr_bridge_low_runs;
static uint8 isr_bridge_high_runs;
static StatusType isr_bridge_invoke_status;
static StatusType isr_bridge_activate_status;

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

static void isr_bridge_high_task(void)
{
    isr_bridge_high_runs++;
}

static void isr_bridge_isr_activate_high(void)
{
    isr_bridge_activate_status = ActivateTask(OS_PORT_STM32_SECOND_TASK_ID);
}

static void isr_bridge_low_task(void)
{
    isr_bridge_low_runs++;
    isr_bridge_invoke_status = Os_TestInvokeIsrCat2(isr_bridge_isr_activate_high);
}

static const Os_TaskConfigType os_port_stm32_binding_tasks[] = {
    { "TaskA", dummy_task_entry, 1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

static const Os_TaskConfigType os_port_stm32_scheduler_bridge_tasks[] = {
    { "LowTask", scheduler_bridge_low_task, 2u, 1u, 0u, FALSE, FULL },
    { "HighTask", scheduler_bridge_high_task, 1u, 1u, 0u, FALSE, FULL }
};

static const Os_TaskConfigType os_port_stm32_isr_bridge_tasks[] = {
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
        Os_TestConfigureTasks(os_port_stm32_binding_tasks, (uint8)(sizeof(os_port_stm32_binding_tasks) /
                                                                   sizeof(os_port_stm32_binding_tasks[0]))));
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
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_STM32_INITIAL_EXC_RETURN, frame[0]);

    for (index = 1u; index <= 13u; index++) {
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
            os_port_stm32_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_stm32_scheduler_bridge_tasks) /
                    sizeof(os_port_stm32_scheduler_bridge_tasks[0]))));
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
            os_port_stm32_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_stm32_scheduler_bridge_tasks) /
                    sizeof(os_port_stm32_scheduler_bridge_tasks[0]))));
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

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 * @requirement A kernel Cat2 ISR preemption shall exercise the STM32 target
 *              ISR nesting and deferred-dispatch release seam before the
 *              PendSV completion path runs.
 * @verify A low task that activates a high task from Os_TestInvokeIsrCat2
 *         leaves PendSV requested for the prepared high task after ISR exit,
 *         and the shared completion helper then completes the switch.
 */
void test_Os_Port_Stm32_isr2_preemption_flows_through_target_exit_and_pendsv(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_Port_Stm32_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_stm32_isr_bridge_tasks,
            (uint8)(sizeof(os_port_stm32_isr_bridge_tasks) /
                    sizeof(os_port_stm32_isr_bridge_tasks[0]))));
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

    TEST_ASSERT_EQUAL(E_OK, isr_bridge_invoke_status);
    TEST_ASSERT_EQUAL(E_OK, isr_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_bridge_high_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, state->Isr2Nesting);
    TEST_ASSERT_FALSE(state->DeferredPendSv);
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->SelectedNextTask);

    TEST_ASSERT_EQUAL(E_OK, Os_TestCompletePortDispatches());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_FALSE(state->PendSvPending);
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
        Os_TestConfigureTasks(os_port_stm32_binding_tasks, (uint8)(sizeof(os_port_stm32_binding_tasks) /
                                                                   sizeof(os_port_stm32_binding_tasks[0]))));
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

/**
 * @spec ThreadX reference: ports/cortex_m4/gnu/src/tx_timer_interrupt.S
 * @requirement The shared bootstrap counter helper shall drain any deferred
 *              STM32 configured-task handoff that alarm processing creates.
 * @verify An alarm-activated low task that readies a prepared high task
 *         leaves the port in the completed high-task state after a single
 *         Os_TestAdvanceCounter call.
 */
void test_Os_Port_Stm32_alarm_counter_helper_drains_deferred_dispatch_to_idle(void)
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
            os_port_stm32_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_stm32_scheduler_bridge_tasks) /
                    sizeof(os_port_stm32_scheduler_bridge_tasks[0]))));
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
    Os_TestAdvanceCounter(1u);
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
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
            os_port_stm32_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_stm32_scheduler_bridge_tasks) /
                    sizeof(os_port_stm32_scheduler_bridge_tasks[0]))));
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
    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);

    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
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
            os_port_stm32_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_stm32_scheduler_bridge_tasks) /
                    sizeof(os_port_stm32_scheduler_bridge_tasks[0]))));
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
    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);

    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_STM32_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Os_Port_Stm32_target_init_sets_bootstrap_exception_state);
    RUN_TEST(test_Os_Port_Stm32_prepare_first_task_builds_initial_frame_metadata);
    RUN_TEST(test_Os_Port_Stm32_prepare_first_task_builds_threadx_compatible_frame_words);
    RUN_TEST(test_Os_Port_Stm32_start_first_task_requires_prepared_launch_frame);
    RUN_TEST(test_Os_Port_Stm32_start_first_task_is_not_relaunched_after_first_start);
    RUN_TEST(test_Os_Port_Stm32_request_context_switch_pends_pendsv_after_start);
    RUN_TEST(test_Os_Port_Stm32_request_configured_dispatch_rejects_unprepared_task_context);
    RUN_TEST(test_Os_Port_Stm32_request_configured_dispatch_selects_task_and_pends_switch);
    RUN_TEST(test_Os_Port_Stm32_complete_configured_dispatch_returns_e_os_nofunc_when_idle);
    RUN_TEST(test_Os_Port_Stm32_kernel_scheduler_publishes_dispatch_to_port_state);
    RUN_TEST(test_Os_Port_Stm32_kernel_first_dispatch_synchronizes_started_task_without_stale_selection);
    RUN_TEST(test_Os_Port_Stm32_kernel_preemption_arms_target_handoff_for_prepared_task);
    RUN_TEST(test_Os_Port_Stm32_kernel_preemption_completes_through_pendsv_handler);
    RUN_TEST(test_Os_Port_Stm32_isr2_preemption_flows_through_target_exit_and_pendsv);
    RUN_TEST(test_Os_Port_Stm32_pendsv_handler_clears_pending_and_tracks_completion);
    RUN_TEST(test_Os_Port_Stm32_pendsv_handler_switches_to_selected_next_task_context);
    RUN_TEST(test_Os_Port_Stm32_task_binding_prepares_contexts_from_configured_tasks);
    RUN_TEST(test_Os_Port_Stm32_saved_psp_can_be_selected_to_switch_back);
    RUN_TEST(test_Os_Port_Stm32_nested_isr_exit_releases_deferred_pendsv_request);
    RUN_TEST(test_Os_Port_Stm32_alarm_counter_helper_drains_deferred_dispatch_to_idle);
    RUN_TEST(test_Os_Port_Stm32_tick_isr_counts_ticks_without_spurious_dispatch_when_no_alarm_expires);
    RUN_TEST(test_Os_Port_Stm32_systick_handler_routes_alarm_expiry_into_prepared_dispatch);
    RUN_TEST(test_Os_Port_Stm32_systick_inside_isr_defers_dispatch_until_outer_exit);

    return UNITY_END();
}
