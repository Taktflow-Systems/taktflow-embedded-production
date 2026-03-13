/**
 * @file    test_Os_Port_Tms570_bootstrap.c
 * @brief   Unit tests for the TMS570 Cortex-R5 bootstrap OS port
 * @date    2026-03-13
 */
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "Os.h"
#include "Os_Port_Tms570.h"
#include "Os_Port_TaskBinding.h"

#define OS_PORT_TMS570_INITIAL_FRAME_BYTES 76u
#define OS_PORT_TMS570_INITIAL_STACK_TYPE  1u
#define OS_PORT_TMS570_INITIAL_CPSR        0x13u
#define OS_PORT_TMS570_FIRST_TASK_ID       ((TaskType)0u)
#define OS_PORT_TMS570_SECOND_TASK_ID      ((TaskType)1u)
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
    scheduler_bridge_activate_status = ActivateTask(OS_PORT_TMS570_SECOND_TASK_ID);
}

static void isr_bridge_high_task(void)
{
    isr_bridge_high_runs++;
}

static void isr_bridge_isr_activate_high(void)
{
    isr_bridge_activate_status = ActivateTask(OS_PORT_TMS570_SECOND_TASK_ID);
}

static void isr_bridge_low_task(void)
{
    isr_bridge_low_runs++;
    isr_bridge_invoke_status = Os_TestInvokeIsrCat2(isr_bridge_isr_activate_high);
}

static const Os_TaskConfigType os_port_tms570_binding_tasks[] = {
    { "TaskA", dummy_task_entry, 1u, 1u, 0u, FALSE, FULL },
    { "TaskB", dummy_task_entry_alt, 2u, 1u, 0u, FALSE, FULL }
};

static const Os_TaskConfigType os_port_tms570_scheduler_bridge_tasks[] = {
    { "LowTask", scheduler_bridge_low_task, 2u, 1u, 0u, FALSE, FULL },
    { "HighTask", scheduler_bridge_high_task, 1u, 1u, 0u, FALSE, FULL }
};

static const Os_TaskConfigType os_port_tms570_isr_bridge_tasks[] = {
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

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/inc/tx_port.h
 * @requirement The bootstrap TMS570 port shall initialize its basic RTI,
 *              VIM, and interrupt bookkeeping state before first-task launch.
 * @verify Target init configures bootstrap RTI/VIM policy and clears
 *         first-task and dispatch state.
 */
void test_Os_Port_Tms570_target_init_sets_bootstrap_exception_state(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->TargetInitialized);
    TEST_ASSERT_TRUE(state->VimConfigured);
    TEST_ASSERT_TRUE(state->RtiConfigured);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->FirstTaskPrepared);
    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_INITIAL_CPSR, state->InitialCpsr);
    TEST_ASSERT_EQUAL_UINT32(0u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FiqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedIrqReturnCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->IrqCapturedTaskSp);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastSavedIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastRestoredIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastSavedIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->LastRestoredIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_NONE, state->LastSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_NONE, state->LastSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NONE, state->LastRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_IDLE_SYSTEM, Os_Port_Tms570_PeekSaveAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING,
                            Os_Port_Tms570_PeekSaveContinuationAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NONE, Os_Port_Tms570_PeekRestoreAction());
    TEST_ASSERT_EQUAL(INVALID_TASK, state->LastObservedKernelTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_stack_build.S
 * @requirement The bootstrap TMS570 port shall build metadata for the first
 *              ARM-R synthetic interrupt frame before first task start.
 * @verify PrepareFirstTask records entry address, stack top, saved stack
 *         pointer, and key ThreadX-style frame words.
 */
void test_Os_Port_Tms570_prepare_first_task_builds_initial_frame_metadata(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    uintptr_t expected_sp =
        (uintptr_t)((stack_top - (uintptr_t)OS_PORT_TMS570_INITIAL_FRAME_BYTES) & ~(uintptr_t)0x7u);
    uint32* frame = (uint32*)expected_sp;
    const Os_Port_Tms570_StateType* state;

    (void)memset(stack_storage, 0xA5, sizeof(stack_storage));
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskPrepared);
    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->FirstTaskTaskID);
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry) == state->FirstTaskEntryAddress);
    TEST_ASSERT_EQUAL_PTR((void*)stack_top, (void*)state->FirstTaskStackTop);
    TEST_ASSERT_EQUAL_PTR((void*)expected_sp, (void*)state->FirstTaskSp);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_INITIAL_STACK_TYPE, frame[0]);
    TEST_ASSERT_EQUAL_HEX32(OS_PORT_TMS570_INITIAL_CPSR, frame[1]);
    TEST_ASSERT_EQUAL_HEX32((uint32)((uintptr_t)dummy_task_entry & 0xFFFFFFFFu), frame[16]);
    TEST_ASSERT_EQUAL_HEX32(0u, frame[17]);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement The bootstrap TMS570 port shall not start the first task
 *              until initial task-frame metadata exists.
 * @verify StartFirstTask becomes effective only after PrepareFirstTask.
 */
void test_Os_Port_Tms570_start_first_task_requires_prepared_frame(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    Os_PortStartFirstTask();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->FirstTaskStarted);

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FirstTaskLaunchCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)state->FirstTaskSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)state->FirstTaskSp, (void*)state->LastRestoredTaskSp);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement The bootstrap TMS570 port shall enter the first-task launch
 *              path only once for a prepared task context.
 * @verify A repeated StartFirstTask call does not relaunch or increment the
 *         launch count after the first launch.
 */
void test_Os_Port_Tms570_start_first_task_is_not_relaunched_after_first_start(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));

    Os_PortStartFirstTask();
    Os_PortStartFirstTask();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FirstTaskLaunchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 port shall request deferred dispatch
 *              only after the first task has started.
 * @verify RequestContextSwitch sets the dispatch request only in running
 *         state and ignores repeated requests while already pending.
 */
void test_Os_Port_Tms570_request_context_switch_sets_dispatch_after_start(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    Os_PortRequestContextSwitch();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();
    Os_PortRequestContextSwitch();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);

    Os_PortRequestContextSwitch();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement The shared configured-dispatch binding shall reject a task
 *              that exists in OS config but does not yet have a prepared
 *              TMS570 task context.
 * @verify RequestConfiguredDispatch returns E_OS_VALUE and leaves deferred
 *         dispatch state unchanged for an unprepared configured task.
 */
void test_Os_Port_Tms570_request_configured_dispatch_rejects_unprepared_task_context(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_PrepareConfiguredFirstTask(OS_PORT_TMS570_FIRST_TASK_ID, stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The shared configured-dispatch binding shall select the
 *              prepared configured task and raise a deferred dispatch through
 *              the TMS570 port contract.
 * @verify RequestConfiguredDispatch records the selected task and marks an
 *         active dispatch request once the first task is running.
 */
void test_Os_Port_Tms570_request_configured_dispatch_selects_task_and_requests_dispatch(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 port shall expose a save-current-SP hook
 *              that records the outermost interrupted task runtime SP for the
 *              future assembly path.
 * @verify SaveCurrentTaskSp updates the current task runtime SP and capture
 *         on outermost use, while nested use leaves the outer capture intact.
 */
void test_Os_Port_Tms570_save_current_task_sp_updates_outermost_capture_only(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    uintptr_t outer_runtime_sp;
    uintptr_t nested_runtime_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    outer_runtime_sp = first_ctx->SavedSp - (uintptr_t)32u;
    nested_runtime_sp = first_ctx->SavedSp - (uintptr_t)48u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp(outer_runtime_sp));
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)state->IrqCapturedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)first_ctx->RuntimeSp);

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_SaveCurrentTaskSp(nested_runtime_sp));
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)state->IrqCapturedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)first_ctx->RuntimeSp);
    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 save helper shall distinguish the first
 *              IRQ entry that captures the running task from later nested IRQ
 *              entries that only extend nesting depth.
 * @verify The first save records capture-current, and a nested save records
 *         nested-IRQ without overwriting the outer captured task/SP.
 */
void test_Os_Port_Tms570_irq_context_save_records_capture_then_nested_actions(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CAPTURE_CURRENT,
                            Os_Port_Tms570_PeekSaveAction());
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CAPTURE_CURRENT, state->LastSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING,
                            state->LastSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedIrqReturnCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)state->CurrentTaskSp, (void*)state->IrqCapturedTaskSp);

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_NESTED_IRQ, Os_Port_Tms570_PeekSaveAction());
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_NESTED_RETURN,
                            Os_Port_Tms570_PeekSaveContinuationAction());
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_NESTED_IRQ, state->LastSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_NESTED_RETURN,
                            state->LastSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->NestedIrqReturnCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)state->CurrentTaskSp, (void*)state->IrqCapturedTaskSp);

    Os_Port_Tms570_IrqContextRestore();
    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 save helper shall model the idle-system
 *              save branch when no task has started yet.
 * @verify Before first-task launch, save peeks and records idle-system, still
 *         enters IRQ bookkeeping, and does not capture a task stack pointer.
 */
void test_Os_Port_Tms570_irq_context_save_before_first_task_records_idle_system_action(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_IDLE_SYSTEM, Os_Port_Tms570_PeekSaveAction());
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_IDLE_SYSTEM, state->LastSaveAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_SAVE_CONTINUE_IRQ_PROCESSING,
                            state->LastSaveContinuationAction);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqProcessingEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->NestedIrqReturnCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->IrqCapturedTaskSp);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT, state->LastRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S
 * @requirement The bootstrap TMS570 port shall model the explicit transition
 *              from IRQ save into system-mode IRQ processing and back again
 *              before restore.
 * @verify A first-save path enters IRQ processing on nesting start, leaves it
 *         on nesting end, and keeps the IRQ context active until restore.
 */
void test_Os_Port_Tms570_irq_nesting_start_end_wrap_first_save_processing_phase(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0x11111110u));

    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->LastSavedIrqProcessingReturnAddress);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0x22222220u));

    Os_Port_Tms570_IrqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->CurrentIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->LastRestoredIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S
 * @requirement The bootstrap TMS570 nesting-start/end model shall restore
 *              saved processing return addresses in LIFO order across nested
 *              IRQ processing phases.
 * @verify Two nested processing phases save two different return addresses,
 *         and the inner end restores the inner one while the outer end
 *         restores the outer one.
 */
void test_Os_Port_Tms570_nested_irq_nesting_start_end_restores_processing_return_addresses_lifo(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));
    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xBBBBBBB0u));
    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(16u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(16u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0xBBBBBBB0u, (void*)state->LastSavedIrqProcessingReturnAddress);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xCCCCCCC0u));
    Os_Port_Tms570_IrqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0xBBBBBBB0u, (void*)state->CurrentIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xBBBBBBB0u, (void*)state->LastRestoredIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xDDDDDDD0u));
    Os_Port_Tms570_IrqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(16u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentIrqProcessingReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->LastRestoredIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_irq_nesting_start.S
 * @requirement The bootstrap TMS570 port shall keep the IRQ-mode banked
 *              return address separate from the system-mode processing return
 *              address while `irq_nesting_start/end` switches execution mode.
 * @verify The outer IRQ save records the IRQ banked return, nesting start
 *         switches into system mode without losing it, nesting end returns to
 *         IRQ mode, and final restore consumes that IRQ return and returns the
 *         mode model to thread execution.
 */
void test_Os_Port_Tms570_irq_nesting_start_keeps_irq_banked_return_separate_from_system_mode_return(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x12345670u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));

    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_IRQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->LastSavedIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqNestingStart();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentIrqProcessingReturnAddress);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xBBBBBBB0u));
    Os_Port_Tms570_IrqNestingEnd();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_IRQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0xAAAAAAA0u, (void*)state->CurrentIrqProcessingReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x12345670u, (void*)state->LastRestoredIrqReturnAddress);
}

/**
 * @spec ThreadX reference: ports/arm11/gnu/src/tx_thread_context_restore.S
 * @requirement A nested TMS570 IRQ restore shall return from IRQ mode back
 *              into system-mode IRQ processing using the nested IRQ banked
 *              return address, while preserving the outer IRQ return until the
 *              later outer restore.
 * @verify Nested restore consumes the nested IRQ return and restores system
 *         mode, then the outer restore later consumes the original outer IRQ
 *         return address.
 */
void test_Os_Port_Tms570_nested_irq_restore_returns_to_system_mode_with_nested_irq_return_address(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x11111110u));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqProcessingReturnAddress((uintptr_t)0xAAAAAAA0u));
    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqNestingStart();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetIrqReturnAddress((uintptr_t)0x22222220u));
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_IRQ, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x22222220u, (void*)state->CurrentIrqReturnAddress);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NESTED_RETURN, state->LastRestoreAction);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_SYSTEM, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->CurrentIrqReturnAddress);
    TEST_ASSERT_EQUAL_PTR((void*)0x22222220u, (void*)state->LastRestoredIrqReturnAddress);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);

    Os_Port_Tms570_IrqNestingEnd();
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_MODE_THREAD, state->CurrentExecutionMode);
    TEST_ASSERT_EQUAL_PTR((void*)0x11111110u, (void*)state->LastRestoredIrqReturnAddress);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 port shall expose the runtime SP that a
 *              future assembly restore path should use for the current or
 *              selected task.
 * @verify PeekRestoreTaskSp returns the current task runtime SP when no
 *         switch is pending and the selected task runtime SP when dispatch
 *         has armed a handoff.
 */
void test_Os_Port_Tms570_peek_restore_task_sp_prefers_selected_task_runtime_sp(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->RuntimeSp, (void*)Os_Port_Tms570_PeekRestoreTaskSp());

    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->RuntimeSp, (void*)Os_Port_Tms570_PeekRestoreTaskSp());
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 restore helper shall distinguish the
 *              nested-return path from the final IRQ-return path.
 * @verify PeekRestoreAction reports nested return while more than one IRQ
 *         context is active, and the first matched restore records that
 *         nested-return action.
 */
void test_Os_Port_Tms570_peek_restore_action_reports_nested_return_path(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqContextSave();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NESTED_RETURN,
                            Os_Port_Tms570_PeekRestoreAction());

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NESTED_RETURN, state->LastRestoreAction);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 restore helper shall distinguish a final
 *              IRQ return that resumes the interrupted task from one that
 *              switches to a different prepared task.
 * @verify Final restore with no pending handoff reports and records the
 *         resume-current action.
 */
void test_Os_Port_Tms570_irq_restore_records_resume_current_action(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT,
                            Os_Port_Tms570_PeekRestoreAction());

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 final-restore path shall treat a pending
 *              handoff to the already-running task as resume-current work,
 *              not as a real task switch.
 * @verify Final restore clears the same-task dispatch request, records the
 *         resume-current action, and leaves task-switch count unchanged.
 */
void test_Os_Port_Tms570_resume_current_restore_clears_same_task_dispatch_request(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT,
                            Os_Port_Tms570_PeekRestoreAction());

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_RESUME_CURRENT, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 restore helper shall report a final IRQ
 *              return that will switch to a selected prepared task.
 * @verify Final restore sees a deferred higher-priority handoff as a
 *         switch-task action before the IRQ exit converts that handoff into
 *         an active dispatch request.
 */
void test_Os_Port_Tms570_peek_restore_action_reports_switch_task_path(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK,
                            Os_Port_Tms570_PeekRestoreAction());

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The shared configured-dispatch completion helper shall be
 *              safe to call when no TMS570 IRQ-return handoff is pending.
 * @verify CompleteConfiguredDispatch returns E_OS_NOFUNC and leaves the
 *         bootstrap dispatch state unchanged when no switch is pending.
 */
void test_Os_Port_Tms570_complete_configured_dispatch_returns_e_os_nofunc_when_idle(void)
{
    uint8 stack_storage[160];
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&stack_storage[160])));
    Os_PortStartFirstTask();

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Port_CompleteConfiguredDispatch());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The shared TMS570 configured-dispatch completion helper shall
 *              synthesize the IRQ save/restore seam when a pending handoff
 *              exists outside an already-active IRQ context.
 * @verify CompleteConfiguredDispatch consumes the pending handoff through one
 *         matched IRQ save/restore pair and completes the task switch.
 */
void test_Os_Port_Tms570_complete_configured_dispatch_uses_irq_context_when_no_active_context(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);

    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement The portable bootstrap scheduler shall publish the configured
 *              task it dispatches into the TMS570 port observation seam.
 * @verify A host-side StartOS plus ActivateTask dispatch increments the
 *         observed kernel dispatch count and records the dispatched task ID.
 */
void test_Os_Port_Tms570_kernel_scheduler_publishes_dispatch_to_port_state(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(1u, dummy_task_runs);
    TEST_ASSERT_EQUAL_UINT32(1u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastObservedKernelTask);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_schedule.S
 * @requirement When the TMS570 bootstrap port already owns a prepared and
 *              started first task, the portable scheduler shall synchronize
 *              that current task without leaving a stale selected-next-task.
 * @verify A first portable dispatch with an already-started prepared task
 *         keeps CurrentTask aligned and leaves no pending next-task latch.
 */
void test_Os_Port_Tms570_kernel_first_dispatch_synchronizes_started_task_without_stale_selection(void)
{
    uint8 first_stack[160];
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastObservedKernelTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_FALSE(state->DispatchRequested);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement A portable-kernel preemption from one configured task to a
 *              higher-priority configured task shall arm the TMS570 port
 *              handoff seam when both target contexts are prepared.
 * @verify Dispatching a low task that activates a higher-priority task
 *         causes the scheduler to request IRQ-return dispatch for the
 *         prepared high task.
 */
void test_Os_Port_Tms570_kernel_preemption_arms_target_handoff_for_prepared_task(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(2u, state->KernelDispatchObserveCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->LastObservedKernelTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement A scheduler-driven configured-task preemption shall complete
 *              through the TMS570 IRQ-return restore path once the deferred
 *              port handler runs.
 * @verify After portable-kernel preemption arms the handoff, the shared
 *         completion helper drives IRQ-return restore, switches the
 *         bootstrap current task to the prepared high task, and clears the
 *         selected-next-task state.
 */
void test_Os_Port_Tms570_kernel_preemption_completes_through_irq_restore(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S
 * @requirement A kernel Cat2 ISR preemption shall exercise the TMS570 IRQ
 *              context save/restore seam instead of only raw nesting
 *              bookkeeping.
 * @verify A low task that activates a high task from Os_TestInvokeIsrCat2
 *         completes the prepared handoff on IRQ restore and leaves no
 *         pending dispatch for the generic completion helper.
 */
void test_Os_Port_Tms570_isr2_preemption_flows_through_target_exit_and_irq_restore(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_isr_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_isr_bridge_tasks) /
                    sizeof(os_port_tms570_isr_bridge_tasks[0]))));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(OS_PORT_TMS570_FIRST_TASK_ID));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunReadyTasks());
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK, isr_bridge_invoke_status);
    TEST_ASSERT_EQUAL(E_OK, isr_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_bridge_high_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_TestCompletePortDispatches());
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S
 * @requirement The bootstrap TMS570 port shall defer dispatch assertion
 *              until the final IRQ exit when dispatch was requested inside
 *              nested IRQ processing.
 * @verify A nested IRQ request is remembered and converted to a dispatch
 *         request only on the outermost IRQ exit.
 */
void test_Os_Port_Tms570_nested_irq_exit_releases_deferred_dispatch(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();

    Os_PortEnterIsr2();
    Os_PortEnterIsr2();
    Os_PortRequestContextSwitch();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_TRUE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);

    Os_PortExitIsr2();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqNesting);

    Os_PortExitIsr2();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 IRQ restore helper shall ignore restore
 *              attempts that do not match an active saved IRQ context.
 * @verify IrqContextRestore with zero active context depth leaves dispatch,
 *         current task, and restore counters unchanged.
 */
void test_Os_Port_Tms570_irq_context_restore_without_save_is_ignored(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NONE, state->LastRestoreAction);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 IRQ return path shall complete deferred
 *              dispatch only when the final matched IRQ context restore runs.
 * @verify Nested IrqContextSave calls keep dispatch deferred until the last
 *         IrqContextRestore, which then completes the selected task handoff.
 */
void test_Os_Port_Tms570_nested_irq_context_restore_completes_dispatch_only_on_final_restore(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqNesting);
    TEST_ASSERT_TRUE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqNesting);
    TEST_ASSERT_TRUE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_NESTED_RETURN, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(2u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(2u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT8(OS_PORT_TMS570_RESTORE_SWITCH_TASK, state->LastRestoreAction);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 IRQ save path shall capture the
 *              interrupted task and runtime SP only on the outermost IRQ
 *              context save, matching the local ThreadX nested-save split.
 * @verify A nested IrqContextSave leaves the outer captured task/SP intact,
 *         and the capture is cleared on final restore.
 */
void test_Os_Port_Tms570_nested_irq_save_captures_outermost_task_sp_once(void)
{
    uint8 first_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    uintptr_t outer_runtime_sp;
    uintptr_t inner_runtime_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    outer_runtime_sp = first_ctx->SavedSp - (uintptr_t)12u;
    inner_runtime_sp = first_ctx->SavedSp - (uintptr_t)28u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(outer_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(inner_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);

    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)state->IrqCapturedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)first_ctx->RuntimeSp);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)state->IrqCapturedTaskSp);

    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->IrqCapturedTaskSp);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement A TMS570 nested IRQ-driven dispatch shall preserve the
 *              outermost captured runtime SP as the saved context for the
 *              interrupted task.
 * @verify If outer IRQ entry captures one SP and nested IRQ activity changes
 *         CurrentTaskSp before dispatch is requested, final restore still
 *         reports the outer captured SP as LastSavedTaskSp.
 */
void test_Os_Port_Tms570_nested_irq_dispatch_uses_outermost_captured_runtime_sp(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* second_ctx;
    uintptr_t outer_runtime_sp;
    uintptr_t nested_runtime_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    outer_runtime_sp = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID)->SavedSp - (uintptr_t)20u;
    nested_runtime_sp = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID)->SavedSp - (uintptr_t)40u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(outer_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(nested_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    Os_Port_Tms570_IrqContextRestore();
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)outer_runtime_sp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->RuntimeSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->RuntimeSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->IrqCapturedTask);
    TEST_ASSERT_EQUAL_PTR(0, (void*)state->IrqCapturedTaskSp);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_start.S
 * @requirement The bootstrap TMS570 port shall track balanced FIQ nesting
 *              depth independently of IRQ bookkeeping.
 * @verify EnterFiq and ExitFiq update only FIQ nesting depth and leave IRQ
 *         nesting unchanged.
 */
void test_Os_Port_Tms570_fiq_nesting_tracks_balanced_entry_exit(void)
{
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    Os_Port_Tms570_EnterFiq();
    Os_Port_Tms570_EnterFiq();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(2u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);

    Os_Port_Tms570_ExitFiq();
    Os_Port_Tms570_ExitFiq();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_fiq_context_restore.S
 * @requirement The bootstrap TMS570 FIQ return path shall remain separate
 *              from the IRQ-return dispatch completion path.
 * @verify FiqContextSave and FiqContextRestore update only FIQ bookkeeping
 *         and leave a pending configured-task dispatch untouched.
 */
void test_Os_Port_Tms570_fiq_context_restore_does_not_complete_irq_style_dispatch(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));

    Os_Port_Tms570_FiqContextSave();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextSaveCount);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);

    Os_Port_Tms570_FiqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->FiqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FiqContextRestoreCount);
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->TaskSwitchCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqContextRestoreCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 IRQ return path shall be able to
 *              complete a deferred dispatch into a selected prepared task
 *              context.
 * @verify After selecting a second prepared task, IRQ restore clears the
 *         pending dispatch, updates the current task, and counts a switch.
 */
void test_Os_Port_Tms570_irq_restore_switches_to_selected_next_task(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_stack_build.S
 * @requirement The bootstrap TMS570 port shall be able to prepare task
 *              contexts directly from the configured OSEK task model rather
 *              than from ad-hoc test entry pointers.
 * @verify Configured tasks from Os_TestConfigureTasks are bound into both
 *         first-task and secondary prepared TMS570 task contexts.
 */
void test_Os_Port_Tms570_task_binding_prepares_contexts_from_configured_tasks(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(os_port_tms570_binding_tasks, (uint8)(sizeof(os_port_tms570_binding_tasks) /
                                                                    sizeof(os_port_tms570_binding_tasks[0]))));
    Os_PortTargetInit();

    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));

    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->FirstTaskTaskID);
    TEST_ASSERT_TRUE(first_ctx->Prepared);
    TEST_ASSERT_TRUE(second_ctx->Prepared);
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry) == ((uintptr_t)first_ctx->Entry));
    TEST_ASSERT_TRUE(((uintptr_t)dummy_task_entry_alt) == ((uintptr_t)second_ctx->Entry));
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_restore.S
 * @requirement The bootstrap TMS570 port shall allow a saved task to be
 *              selected again on a later IRQ-driven dispatch.
 * @verify After switching from task A to task B, selecting task A and
 *         completing IRQ restore switches back and increments the switch
 *         count again.
 */
void test_Os_Port_Tms570_irq_restore_can_switch_back_to_previous_task(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)second_ctx->SavedSp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_ctx->SavedSp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_UINT32(2u, state->TaskSwitchCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_thread_context_save.S
 * @requirement The bootstrap TMS570 port shall preserve a task's last saved
 *              runtime stack pointer across a later switch back to that task.
 * @verify After changing the running SP for task A and then task B, switching
 *         A->B->A restores task A's previously saved runtime SP instead of its
 *         original prepared SP.
 */
void test_Os_Port_Tms570_irq_restore_switches_back_to_saved_runtime_sp(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    uintptr_t first_stack_top = (uintptr_t)(&first_stack[160]);
    uintptr_t second_stack_top = (uintptr_t)(&second_stack[160]);
    const Os_Port_Tms570_StateType* state;
    const Os_Port_Tms570_TaskContextType* first_ctx;
    const Os_Port_Tms570_TaskContextType* second_ctx;
    uintptr_t first_runtime_sp;
    uintptr_t second_runtime_sp;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, first_stack_top));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareTaskContext(
                          OS_PORT_TMS570_SECOND_TASK_ID, dummy_task_entry_alt, second_stack_top));
    Os_PortStartFirstTask();

    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);
    first_runtime_sp = first_ctx->SavedSp - (uintptr_t)16u;
    second_runtime_sp = second_ctx->SavedSp - (uintptr_t)24u;

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(first_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_SECOND_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Tms570_TestSetCurrentTaskSp(second_runtime_sp));
    Os_Port_Tms570_IrqContextSave();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_RequestConfiguredDispatch(OS_PORT_TMS570_FIRST_TASK_ID));
    Os_Port_Tms570_IrqContextRestore();
    state = Os_Port_Tms570_GetBootstrapState();
    first_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_FIRST_TASK_ID);
    second_ctx = Os_Port_Tms570_GetTaskContext(OS_PORT_TMS570_SECOND_TASK_ID);

    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_PTR((void*)second_runtime_sp, (void*)state->LastSavedTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_runtime_sp, (void*)state->CurrentTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_runtime_sp, (void*)state->LastRestoredTaskSp);
    TEST_ASSERT_EQUAL_PTR((void*)first_runtime_sp, (void*)first_ctx->RuntimeSp);
    TEST_ASSERT_EQUAL_PTR((void*)second_runtime_sp, (void*)second_ctx->RuntimeSp);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_timer_interrupt.S
 * @requirement The bootstrap TMS570 tick ISR shall route into the OSEK
 *              counter/alarm path without asserting dispatch when no alarm
 *              expiry or ready-task transition occurs.
 * @verify TickIsr increments the bootstrap counter and leaves the
 *         dispatch-request state idle when no alarm expires on that tick.
 */
void test_Os_Port_Tms570_tick_isr_counts_ticks_without_spurious_dispatch_when_no_alarm_expires(void)
{
    uint8 stack_storage[160];
    uintptr_t stack_top = (uintptr_t)(&stack_storage[160]);
    const Os_Port_Tms570_StateType* state;

    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_Tms570_PrepareFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, dummy_task_entry, stack_top));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    Os_Port_Tms570_TickIsr();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/example_build/tx_initialize_low_level.S
 * @requirement The bootstrap TMS570 RTI interrupt entry shall route through
 *              the same timer path used by the low-level Cortex-R5 port
 *              structure and wake prepared configured tasks when an alarm
 *              expires.
 * @verify RtiTickHandler advances the counter, raises a dispatch request
 *         for the ready low task, and the shared run-to-idle helper then
 *         completes the prepared low-to-high handoff sequence.
 */
void test_Os_Port_Tms570_rti_tick_handler_routes_alarm_expiry_into_prepared_dispatch(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_TMS570_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));
    Os_Port_Tms570_RtiTickHandler();
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_TestRunToIdle());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/example_build/tx_initialize_low_level.S
 * @requirement The bootstrap TMS570 RTI tick path shall defer dispatch
 *              while still inside nested IRQ processing and assert it on the
 *              final IRQ exit when an alarm wakes a prepared task.
 * @verify RtiTickHandler inside nested IRQ marks deferred dispatch for the
 *         alarm-ready low task, converts it to an active request on outer
 *         IRQ exit, and the shared run-to-idle helper then completes the
 *         prepared low-to-high handoff.
 */
void test_Os_Port_Tms570_rti_tick_inside_irq_defers_dispatch_until_exit(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_TMS570_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));

    Os_PortEnterIsr2();
    Os_PortEnterIsr2();
    Os_Port_Tms570_RtiTickHandler();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT32(1u, state->TickInterruptCount);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_EQUAL_UINT8(2u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT8(2u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_TRUE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, scheduler_bridge_high_runs);

    Os_PortExitIsr2();
    Os_PortExitIsr2();
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqNesting);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqContextDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqProcessingDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, state->IrqSystemStackFrameDepth);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_TRUE(state->DispatchRequested);
    TEST_ASSERT_FALSE(state->DeferredDispatch);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextSaveCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqContextRestoreCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingStartCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->IrqNestingEndCount);
    TEST_ASSERT_EQUAL_UINT32(0u, state->IrqSystemStackBytes);
    TEST_ASSERT_EQUAL_UINT32(8u, state->IrqSystemStackPeakBytes);
    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);

    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Tms570_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @spec ThreadX reference: ports/cortex_r5/gnu/src/tx_timer_interrupt.S
 * @requirement The shared bootstrap counter helper shall drain any deferred
 *              TMS570 configured-task handoff that alarm processing creates.
 * @verify An alarm-activated low task that readies a prepared high task
 *         leaves the port in the completed high-task state after a single
 *         Os_TestAdvanceCounter call.
 */
void test_Os_Port_Tms570_alarm_counter_helper_drains_deferred_dispatch_to_idle(void)
{
    uint8 first_stack[160];
    uint8 second_stack[160];
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmLowTask", OS_PORT_TMS570_FIRST_TASK_ID, 9u, 1u, 1u }
    };
    const Os_Port_Tms570_StateType* state;

    Os_TestReset();
    TEST_ASSERT_EQUAL(
        E_OK,
        Os_TestConfigureTasks(
            os_port_tms570_scheduler_bridge_tasks,
            (uint8)(sizeof(os_port_tms570_scheduler_bridge_tasks) /
                    sizeof(os_port_tms570_scheduler_bridge_tasks[0]))));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredFirstTask(
                          OS_PORT_TMS570_FIRST_TASK_ID, (uintptr_t)(&first_stack[160])));
    TEST_ASSERT_EQUAL(E_OK,
                      Os_Port_PrepareConfiguredTask(
                          OS_PORT_TMS570_SECOND_TASK_ID, (uintptr_t)(&second_stack[160])));
    Os_PortStartFirstTask();
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 0u));
    Os_TestAdvanceCounter(1u);
    state = Os_Port_Tms570_GetBootstrapState();

    TEST_ASSERT_EQUAL(E_OK, scheduler_bridge_activate_status);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, scheduler_bridge_high_runs);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_SECOND_TASK_ID, state->CurrentTask);
    TEST_ASSERT_EQUAL(OS_PORT_TMS570_FIRST_TASK_ID, state->LastSavedTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DispatchRequestCount);
    TEST_ASSERT_EQUAL_UINT32(1u, state->TaskSwitchCount);
    TEST_ASSERT_FALSE(state->DispatchRequested);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Os_Port_Tms570_target_init_sets_bootstrap_exception_state);
    RUN_TEST(test_Os_Port_Tms570_prepare_first_task_builds_initial_frame_metadata);
    RUN_TEST(test_Os_Port_Tms570_start_first_task_requires_prepared_frame);
    RUN_TEST(test_Os_Port_Tms570_start_first_task_is_not_relaunched_after_first_start);
    RUN_TEST(test_Os_Port_Tms570_request_context_switch_sets_dispatch_after_start);
    RUN_TEST(test_Os_Port_Tms570_request_configured_dispatch_rejects_unprepared_task_context);
    RUN_TEST(test_Os_Port_Tms570_request_configured_dispatch_selects_task_and_requests_dispatch);
    RUN_TEST(test_Os_Port_Tms570_save_current_task_sp_updates_outermost_capture_only);
    RUN_TEST(test_Os_Port_Tms570_irq_context_save_records_capture_then_nested_actions);
    RUN_TEST(test_Os_Port_Tms570_irq_context_save_before_first_task_records_idle_system_action);
    RUN_TEST(test_Os_Port_Tms570_irq_nesting_start_end_wrap_first_save_processing_phase);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_nesting_start_end_restores_processing_return_addresses_lifo);
    RUN_TEST(test_Os_Port_Tms570_irq_nesting_start_keeps_irq_banked_return_separate_from_system_mode_return);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_restore_returns_to_system_mode_with_nested_irq_return_address);
    RUN_TEST(test_Os_Port_Tms570_peek_restore_task_sp_prefers_selected_task_runtime_sp);
    RUN_TEST(test_Os_Port_Tms570_peek_restore_action_reports_nested_return_path);
    RUN_TEST(test_Os_Port_Tms570_irq_restore_records_resume_current_action);
    RUN_TEST(test_Os_Port_Tms570_resume_current_restore_clears_same_task_dispatch_request);
    RUN_TEST(test_Os_Port_Tms570_peek_restore_action_reports_switch_task_path);
    RUN_TEST(test_Os_Port_Tms570_complete_configured_dispatch_returns_e_os_nofunc_when_idle);
    RUN_TEST(test_Os_Port_Tms570_complete_configured_dispatch_uses_irq_context_when_no_active_context);
    RUN_TEST(test_Os_Port_Tms570_kernel_scheduler_publishes_dispatch_to_port_state);
    RUN_TEST(test_Os_Port_Tms570_kernel_first_dispatch_synchronizes_started_task_without_stale_selection);
    RUN_TEST(test_Os_Port_Tms570_kernel_preemption_arms_target_handoff_for_prepared_task);
    RUN_TEST(test_Os_Port_Tms570_kernel_preemption_completes_through_irq_restore);
    RUN_TEST(test_Os_Port_Tms570_isr2_preemption_flows_through_target_exit_and_irq_restore);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_exit_releases_deferred_dispatch);
    RUN_TEST(test_Os_Port_Tms570_irq_context_restore_without_save_is_ignored);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_context_restore_completes_dispatch_only_on_final_restore);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_save_captures_outermost_task_sp_once);
    RUN_TEST(test_Os_Port_Tms570_nested_irq_dispatch_uses_outermost_captured_runtime_sp);
    RUN_TEST(test_Os_Port_Tms570_fiq_nesting_tracks_balanced_entry_exit);
    RUN_TEST(test_Os_Port_Tms570_fiq_context_restore_does_not_complete_irq_style_dispatch);
    RUN_TEST(test_Os_Port_Tms570_irq_restore_switches_to_selected_next_task);
    RUN_TEST(test_Os_Port_Tms570_task_binding_prepares_contexts_from_configured_tasks);
    RUN_TEST(test_Os_Port_Tms570_irq_restore_can_switch_back_to_previous_task);
    RUN_TEST(test_Os_Port_Tms570_irq_restore_switches_back_to_saved_runtime_sp);
    RUN_TEST(test_Os_Port_Tms570_tick_isr_counts_ticks_without_spurious_dispatch_when_no_alarm_expires);
    RUN_TEST(test_Os_Port_Tms570_rti_tick_handler_routes_alarm_expiry_into_prepared_dispatch);
    RUN_TEST(test_Os_Port_Tms570_rti_tick_inside_irq_defers_dispatch_until_exit);
    RUN_TEST(test_Os_Port_Tms570_alarm_counter_helper_drains_deferred_dispatch_to_idle);

    return UNITY_END();
}
