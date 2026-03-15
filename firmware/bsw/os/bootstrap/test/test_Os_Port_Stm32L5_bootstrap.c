/**
 * @file    test_Os_Port_Stm32L5_bootstrap.c
 * @brief   Unit tests for the STM32L5 Cortex-M33 bootstrap OS port
 * @date    2026-03-15
 *
 * @details Tests cover the same bootstrap state model as the M4 port, plus
 *          M33-specific features:
 *          - PSPLIM stack limit tracking per task
 *          - FPU lazy stacking state (FpuActive flag via EXC_RETURN bit[4])
 *          - Variable software frame size (9 vs 25 words)
 */
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "Os.h"
#include "Os_Port_Stm32L5.h"
#include "Os_Port_TaskBinding.h"

#define OS_PORT_M33_INITIAL_FRAME_WORDS    17u
#define OS_PORT_M33_INITIAL_FRAME_BYTES    (OS_PORT_M33_INITIAL_FRAME_WORDS * sizeof(uint32))
#define OS_PORT_M33_SW_FRAME_BYTES         (9u * sizeof(uint32))
#define OS_PORT_M33_INITIAL_EXC_RETURN     0xFFFFFFFDu
#define OS_PORT_M33_EXC_RETURN_FPU         0xFFFFFFEDu  /* bit[4]=0 → FPU frame */
#define OS_PORT_M33_INITIAL_TASK_LR        0xFFFFFFFFu
#define OS_PORT_M33_FIRST_TASK_ID          ((TaskType)0u)
#define OS_PORT_M33_SECOND_TASK_ID         ((TaskType)1u)
#define ALARM_ACTIVATE                     ((AlarmType)0u)

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
    scheduler_bridge_activate_status = ActivateTask(OS_PORT_M33_SECOND_TASK_ID);
}

static const Os_TaskConfigType os_port_m33_binding_tasks[] = {
    { "TaskA", dummy_task_entry, 1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

static const Os_TaskConfigType os_port_m33_scheduler_bridge_tasks[] = {
    { "LowTask", scheduler_bridge_low_task, 2u, 1u, 0u, FALSE, FULL },
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
        Os_TestConfigureTasks(os_port_m33_binding_tasks, (uint8)(sizeof(os_port_m33_binding_tasks) /
                                                                  sizeof(os_port_m33_binding_tasks[0]))));
}

void tearDown(void)
{
}

/* =================================================================
 * Bootstrap initialization
 * ================================================================= */

/**
 * @spec ThreadX reference: ports/cortex_m33/ac6/src/tx_thread_schedule.S
 * @verify Target init configures bootstrap SysTick/PendSV policy and clears
 *         first-task and dispatch state.
 */
void test_Os_Port_M33_target_init_sets_bootstrap_exception_state(void)
{
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();
    state = Os_Port_Stm32L5_GetBootstrapState();

    TEST_ASSERT_TRUE(state->TargetInitialized);
    TEST_ASSERT_TRUE(state->SysTickConfigured);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_FALSE(state->FirstTaskPrepared);
    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_FALSE(state->DeferredPendSv);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, state->PendSvPriority);
    TEST_ASSERT_EQUAL_HEX8(0x40u, state->SysTickPriority);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->LastObservedKernelTask);
}

/* =================================================================
 * First task frame building
 * ================================================================= */

/**
 * @verify PrepareFirstTask records metadata and computes PSP with StackLimit.
 */
void test_Os_Port_M33_prepare_first_task_builds_initial_frame_metadata(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    uintptr_t stack_limit = (uintptr_t)(&stack_storage[0]);
    uintptr_t expected_psp =
        (uintptr_t)((stack_top - (uintptr_t)OS_PORT_M33_INITIAL_FRAME_BYTES) & ~(uintptr_t)0x7u);
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, stack_limit));
    state = Os_Port_Stm32L5_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskPrepared);
    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL(OS_PORT_M33_FIRST_TASK_ID, state->FirstTaskTaskID);
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry) == state->FirstTaskEntryAddress);
    TEST_ASSERT_EQUAL_PTR((void*)stack_top, (void*)state->FirstTaskStackTop);
    TEST_ASSERT_EQUAL_PTR((void*)expected_psp, (void*)state->FirstTaskPsp);
    TEST_ASSERT_EQUAL_HEX32(0x01000000u, state->InitialXpsr);
}

/**
 * @verify PrepareFirstTask writes ThreadX-compatible frame words (same as M4).
 */
void test_Os_Port_M33_prepare_first_task_builds_compatible_frame_words(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    uintptr_t expected_psp =
        (uintptr_t)((stack_top - (uintptr_t)OS_PORT_M33_INITIAL_FRAME_BYTES) & ~(uintptr_t)0x7u);
    uint32* frame = (uint32*)expected_psp;
    uint32 index;

    (void)memset(stack_storage, 0xA5, sizeof(stack_storage));
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_M33_INITIAL_EXC_RETURN, frame[0]);

    for (index = 1u; index <= 13u; index++) {
        TEST_ASSERT_EQUAL_HEX32(0u, frame[index]);
    }

    TEST_ASSERT_EQUAL_HEX32(OS_PORT_M33_INITIAL_TASK_LR, frame[14]);
    TEST_ASSERT_EQUAL_HEX32((uint32)((uintptr_t)dummy_task_entry & 0xFFFFFFFFu), frame[15]);
    TEST_ASSERT_EQUAL_HEX32(0x01000000u, frame[16]);
}

/* =================================================================
 * First task launch
 * ================================================================= */

/**
 * @verify StartFirstTask only becomes effective after PrepareFirstTask.
 */
void test_Os_Port_M33_start_first_task_requires_prepared_launch_frame(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    uintptr_t expected_active_psp;
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();
    Os_PortStartFirstTask();
    state = Os_Port_Stm32L5_GetBootstrapState();
    TEST_ASSERT_FALSE(state->FirstTaskStarted);

    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));
    expected_active_psp = Os_Port_Stm32L5_GetTaskContext(OS_PORT_M33_FIRST_TASK_ID)->RestorePsp;
    Os_PortStartFirstTask();
    state = Os_Port_Stm32L5_GetBootstrapState();
    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FirstTaskLaunchCount);
    TEST_ASSERT_EQUAL(OS_PORT_M33_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)expected_active_psp, (void*)state->ActivePsp);
}

/**
 * @verify Repeated StartFirstTask does not relaunch.
 */
void test_Os_Port_M33_start_first_task_is_not_relaunched_after_first_start(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));

    Os_PortStartFirstTask();
    Os_PortStartFirstTask();
    state = Os_Port_Stm32L5_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FirstTaskLaunchCount);
}

/* =================================================================
 * Context switch requests
 * ================================================================= */

/**
 * @verify RequestContextSwitch pends PendSV only after first task started.
 */
void test_Os_Port_M33_request_context_switch_pends_pendsv_after_start(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();
    Os_PortRequestContextSwitch();
    state = Os_Port_Stm32L5_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
    TEST_ASSERT_FALSE(state->PendSvPending);

    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));
    Os_PortStartFirstTask();
    Os_PortRequestContextSwitch();
    state = Os_Port_Stm32L5_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);

    /* Duplicate request is absorbed */
    Os_PortRequestContextSwitch();
    state = Os_Port_Stm32L5_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
}

/* =================================================================
 * PendSV handler
 * ================================================================= */

/**
 * @verify PendSvHandler clears pending state and tracks completion.
 */
void test_Os_Port_M33_pendsv_handler_clears_pending_and_tracks_completion(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    uintptr_t expected_active_psp;
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));
    expected_active_psp = Os_Port_Stm32L5_GetTaskContext(OS_PORT_M33_FIRST_TASK_ID)->RestorePsp;
    Os_PortStartFirstTask();
    Os_PortRequestContextSwitch();

    Os_Port_Stm32L5_PendSvHandler();
    state = Os_Port_Stm32L5_GetBootstrapState();

    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_EQUAL(OS_PORT_M33_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)expected_active_psp, (void*)state->ActivePsp);
}

/**
 * @verify PendSV switches to a selected next task.
 */
void test_Os_Port_M33_pendsv_handler_switches_to_selected_next_task_context(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[128]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[128]);
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(
            OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, first_stack_top, (uintptr_t)0u));
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareTaskContext(
            OS_PORT_M33_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top, (uintptr_t)0u));

    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32L5_SelectNextTask(OS_PORT_M33_SECOND_TASK_ID));
    Os_PortRequestContextSwitch();
    Os_Port_Stm32L5_PendSvHandler();
    state = Os_Port_Stm32L5_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL(OS_PORT_M33_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_M33_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/* =================================================================
 * ISR nesting and deferred dispatch
 * ================================================================= */

/**
 * @verify Nested ISR defers PendSV until outermost exit.
 */
void test_Os_Port_M33_nested_isr_exit_releases_deferred_pendsv_request(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));
    Os_PortStartFirstTask();

    Os_PortEnterIsr2();
    Os_PortEnterIsr2();
    Os_PortRequestContextSwitch();
    state = Os_Port_Stm32L5_GetBootstrapState();
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_TRUE(state->DeferredPendSv);
    TEST_ASSERT_EQUAL_UINT8(2u, state->Isr2Nesting);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);

    Os_PortExitIsr2();
    state = Os_Port_Stm32L5_GetBootstrapState();
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT8(1u, state->Isr2Nesting);

    Os_PortExitIsr2();
    state = Os_Port_Stm32L5_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_FALSE(state->DeferredPendSv);
    TEST_ASSERT_EQUAL_UINT8(0u, state->Isr2Nesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
}

/* =================================================================
 * Tick ISR
 * ================================================================= */

/**
 * @verify TickIsr counts ticks without spurious dispatch when no alarm expires.
 */
void test_Os_Port_M33_tick_isr_counts_ticks_without_spurious_dispatch(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    Os_Port_Stm32L5_TickIsr();
    state = Os_Port_Stm32L5_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
}

/* =================================================================
 * M33-specific: PSPLIM stack limit tracking
 * ================================================================= */

/**
 * @verify Task context stores StackLimit for PSPLIM register.
 */
void test_Os_Port_M33_task_context_stores_stack_limit(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    uintptr_t stack_limit = (uintptr_t)(&stack_storage[0]);
    const Os_Port_Stm32L5_TaskContextType* ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareTaskContext(
            OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, stack_limit));

    ctx = Os_Port_Stm32L5_GetTaskContext(OS_PORT_M33_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(ctx->Prepared);
    TEST_ASSERT_EQUAL_PTR((void*)stack_limit, (void*)ctx->StackLimit);
    TEST_ASSERT_FALSE(ctx->FpuActive);
}

/**
 * @verify GetCurrentTaskStackLimit returns the active task's limit.
 */
void test_Os_Port_M33_get_current_task_stack_limit_returns_active_limit(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    uintptr_t stack_limit = (uintptr_t)(&stack_storage[0]);

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(
            OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, stack_limit));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL_PTR((void*)stack_limit,
                          (void*)Os_Port_Stm32L5_GetCurrentTaskStackLimit());
}

/**
 * @verify GetCurrentTaskStackLimit returns 0 when StackLimit was set to 0 (no guard).
 */
void test_Os_Port_M33_get_current_task_stack_limit_returns_zero_when_unset(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(
            OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL_PTR((void*)0u,
                          (void*)Os_Port_Stm32L5_GetCurrentTaskStackLimit());
}

/**
 * @verify PrepareTaskContext rejects StackLimit >= StackTop.
 */
void test_Os_Port_M33_prepare_rejects_invalid_stack_limit(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);

    Os_PortTargetInit();

    /* StackLimit == StackTop → invalid */
    TEST_ASSERT_EQUAL(E_OS_VALUE,
        Os_Port_Stm32L5_PrepareTaskContext(
            OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, stack_top));

    /* StackLimit > StackTop → invalid */
    TEST_ASSERT_EQUAL(E_OS_VALUE,
        Os_Port_Stm32L5_PrepareTaskContext(
            OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, stack_top + 1u));
}

/* =================================================================
 * M33-specific: FPU lazy stacking state tracking
 * ================================================================= */

/**
 * @verify PendSvSaveContext tracks FpuActive from EXC_RETURN bit[4].
 */
void test_Os_Port_M33_pendsv_save_context_tracks_fpu_active(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32L5_TaskContextType* ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(
            OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));
    Os_PortStartFirstTask();

    /* Simulate save with FPU active (bit[4]=0) */
    Os_Port_Stm32L5_PendSvSaveContext(stack_top - 100u, OS_PORT_M33_EXC_RETURN_FPU);
    ctx = Os_Port_Stm32L5_GetTaskContext(OS_PORT_M33_FIRST_TASK_ID);
    TEST_ASSERT_TRUE(ctx->FpuActive);

    /* Simulate save with no FPU (bit[4]=1) */
    Os_Port_Stm32L5_PendSvSaveContext(stack_top - 80u, OS_PORT_M33_INITIAL_EXC_RETURN);
    ctx = Os_Port_Stm32L5_GetTaskContext(OS_PORT_M33_FIRST_TASK_ID);
    TEST_ASSERT_FALSE(ctx->FpuActive);
}

/**
 * @verify PendSvGetNextExcReturn reflects FPU state of current task.
 */
void test_Os_Port_M33_pendsv_get_next_exc_return_reflects_fpu_state(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareFirstTask(
            OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));
    Os_PortStartFirstTask();

    /* No FPU → bit[4] should be 1 */
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_M33_INITIAL_EXC_RETURN,
                             Os_Port_Stm32L5_PendSvGetNextExcReturn());

    /* Simulate FPU active save */
    Os_Port_Stm32L5_PendSvSaveContext(stack_top - 100u, OS_PORT_M33_EXC_RETURN_FPU);
    /* Now bit[4] should be 0 */
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_M33_EXC_RETURN_FPU,
                             Os_Port_Stm32L5_PendSvGetNextExcReturn());
}

/**
 * @verify Initial task context has FpuActive = FALSE.
 */
void test_Os_Port_M33_initial_task_context_has_no_fpu(void)
{
    uint8 stack_storage[128];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[128]);
    const Os_Port_Stm32L5_TaskContextType* ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_Stm32L5_PrepareTaskContext(
            OS_PORT_M33_FIRST_TASK_ID, dummy_task_entry, stack_top, (uintptr_t)0u));

    ctx = Os_Port_Stm32L5_GetTaskContext(OS_PORT_M33_FIRST_TASK_ID);
    TEST_ASSERT_FALSE(ctx->FpuActive);
}

/* =================================================================
 * Kernel integration (via TaskBinding)
 * ================================================================= */

/**
 * @verify Kernel scheduler publishes dispatch to M33 port state.
 */
void test_Os_Port_M33_kernel_scheduler_publishes_dispatch_to_port_state(void)
{
    const Os_Port_Stm32L5_StateType* state;

    Os_PortTargetInit();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_M33_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Stm32L5_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(1u, dummy_task_runs);
    TEST_ASSERT_EQUAL_UINT32(1u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(OS_PORT_M33_FIRST_TASK_ID, state->LastObservedKernelTask);
    TEST_ASSERT_FALSE(state->PendSvPending);
}

/**
 * @verify Kernel preemption completes through PendSV handler.
 */
void test_Os_Port_M33_kernel_preemption_completes_through_pendsv_handler(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_Port_Stm32L5_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_m33_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_m33_scheduler_bridge_tasks) /
                    sizeof(os_port_m33_scheduler_bridge_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredFirstTask(
            OS_PORT_M33_FIRST_TASK_ID, (uintptr_t)(&first_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredTask(
            OS_PORT_M33_SECOND_TASK_ID, (uintptr_t)(&second_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_M33_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Stm32L5_GetBootstrapState();

    TEST_ASSERT_EQUAL(OS_PORT_M33_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_M33_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @verify SysTickHandler routes alarm expiry into prepared dispatch.
 */
void test_Os_Port_M33_systick_handler_routes_alarm_expiry_into_prepared_dispatch(void)
{
    uint8 first_stack[128];
    uint8 second_stack[128];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_M33_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Stm32L5_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_m33_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_m33_scheduler_bridge_tasks) /
                    sizeof(os_port_m33_scheduler_bridge_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredFirstTask(
            OS_PORT_M33_FIRST_TASK_ID, (uintptr_t)(&first_stack[128])));
    TEST_ASSERT_EQUAL(E_OK,
        Os_Port_PrepareConfiguredTask(
            OS_PORT_M33_SECOND_TASK_ID, (uintptr_t)(&second_stack[128])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));
    Os_Port_Stm32L5_SysTickHandler();
    state = Os_Port_Stm32L5_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT8(0u, state->Isr2Nesting);
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);

    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Stm32L5_GetBootstrapState();
    TEST_ASSERT_EQUAL(OS_PORT_M33_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvCompleteCount);
    TEST_ASSERT_FALSE(state->PendSvPending);
}

int main(void)
{
    UNITY_BEGIN();

    /* Bootstrap initialization */
    RUN_TEST(test_Os_Port_M33_target_init_sets_bootstrap_exception_state);

    /* First task frame building */
    RUN_TEST(test_Os_Port_M33_prepare_first_task_builds_initial_frame_metadata);
    RUN_TEST(test_Os_Port_M33_prepare_first_task_builds_compatible_frame_words);

    /* First task launch */
    RUN_TEST(test_Os_Port_M33_start_first_task_requires_prepared_launch_frame);
    RUN_TEST(test_Os_Port_M33_start_first_task_is_not_relaunched_after_first_start);

    /* Context switch requests */
    RUN_TEST(test_Os_Port_M33_request_context_switch_pends_pendsv_after_start);

    /* PendSV handler */
    RUN_TEST(test_Os_Port_M33_pendsv_handler_clears_pending_and_tracks_completion);
    RUN_TEST(test_Os_Port_M33_pendsv_handler_switches_to_selected_next_task_context);

    /* ISR nesting */
    RUN_TEST(test_Os_Port_M33_nested_isr_exit_releases_deferred_pendsv_request);

    /* Tick ISR */
    RUN_TEST(test_Os_Port_M33_tick_isr_counts_ticks_without_spurious_dispatch);

    /* M33-specific: PSPLIM */
    RUN_TEST(test_Os_Port_M33_task_context_stores_stack_limit);
    RUN_TEST(test_Os_Port_M33_get_current_task_stack_limit_returns_active_limit);
    RUN_TEST(test_Os_Port_M33_get_current_task_stack_limit_returns_zero_when_unset);
    RUN_TEST(test_Os_Port_M33_prepare_rejects_invalid_stack_limit);

    /* M33-specific: FPU lazy stacking */
    RUN_TEST(test_Os_Port_M33_pendsv_save_context_tracks_fpu_active);
    RUN_TEST(test_Os_Port_M33_pendsv_get_next_exc_return_reflects_fpu_state);
    RUN_TEST(test_Os_Port_M33_initial_task_context_has_no_fpu);

    /* Kernel integration */
    RUN_TEST(test_Os_Port_M33_kernel_scheduler_publishes_dispatch_to_port_state);
    RUN_TEST(test_Os_Port_M33_kernel_preemption_completes_through_pendsv_handler);
    RUN_TEST(test_Os_Port_M33_systick_handler_routes_alarm_expiry_into_prepared_dispatch);

    return UNITY_END();
}
