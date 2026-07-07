/**
 * @file    test_Os_Port_Stm32_bootstrap_termination_switchback.c
 * @brief   Task-termination switchback tests for the STM32 port
 * @date    2026-07-07
 *
 * @details S-OS-31 task-termination switchback: on PLATFORM_STM32, when the
 *          production PendSV dispatch is live (Os_PortStartFirstTask engaged),
 *          TerminateTask must wire the BRINGUP-6-validated sequence into the
 *          kernel: complete the task per OSEK, then either
 *            (a) resume the preempted task from the restore stack —
 *                port select WITHOUT frame rebuild + context-switch request
 *                (the preempted task's saved context is live; a rebuild
 *                would destroy it), or
 *            (b) freshly dispatch a higher-priority ready task via the
 *                rebuild+select+request sequence (BRINGUP-5/6 preemption
 *                path), or
 *            (c) fail closed (report, stage nothing) when there is neither
 *                a resume target nor a dispatchable ready task.
 *          A terminated task that re-enters READY (BCC2 pending activation)
 *          is NEVER redispatched from its own termination (rebuilding the
 *          frame of the stack still executing corrupts live call frames —
 *          docs/lessons-learned/stm32-bringup-p3.md); the next tick's
 *          preemption dispatch picks it up with a fresh frame.
 *
 *          Host build: the UNIT_TEST port mock stages PendSV in the mocked
 *          register model; staging and frame state are asserted directly.
 *
 * @verifies S-OS-31 task-termination switchback
 *           (docs/plans/os-migration-baseline.md)
 * @standard OSEK/VDX, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "Os.h"
#include "Os_Cfg_Types.h"
#include "Os_Port_Stm32.h"
#include "Os_Port_TaskBinding.h"

#define FRAME_WORDS            17u
#define FRAME_BYTES            (FRAME_WORDS * sizeof(uint32))
#define FRAME_PC_INDEX         15u
#define FRAME_LR_INDEX         14u
#define INITIAL_TASK_LR        0xFFFFFFFFu
#define FRAME_SENTINEL         0xC0FFEE00u

#define TASK_1MS               ((TaskType)0u)
#define TASK_10MS              ((TaskType)1u)
#define TASK_IDLE              ((TaskType)2u)
#define TASK_COUNT             3u
#define IDLE_PRIORITY          ((uint8)(OS_MAX_PRIORITIES - 1u))

#define TABLE_1MS              ((ScheduleTableType)0u)

#define STACK_SIZE             256u

static uint8 runs_1ms;
static uint8 runs_10ms;
static uint8 runs_idle;
static boolean reactivate_1ms_once;

static uint8 pre_hook_count;
static uint8 post_hook_count;
static TaskType post_hook_task;
static uint8 error_hook_count;
static StatusType error_hook_status;

static void Pre_Hook(void)  { pre_hook_count++; }
static void Post_Hook(void)
{
    post_hook_count++;
    post_hook_task = Os_TestGetCurrentTask();
}
static void Error_Hook(StatusType Error)
{
    /* Host-test artifact: OS_STACK_SAMPLE measures the HOST call stack
     * against the configured target stack arrays and reports E_OS_LIMIT.
     * Ignore it — these tests assert the switchback error reports only. */
    if (Error == E_OS_LIMIT) {
        return;
    }

    error_hook_count++;
    error_hook_status = Error;
}

/* Production-shaped basic-task entries: every period task terminates via
 * TerminateTask() (mirrors generated Rte_TaskBodies_<Ecu>.c). */
static void Task_1ms_Entry(void)
{
    runs_1ms++;
    if (reactivate_1ms_once == TRUE) {
        reactivate_1ms_once = FALSE;
        (void)ActivateTask(TASK_1MS);
    }
    (void)TerminateTask();
}

static void Task_10ms_Entry(void)
{
    runs_10ms++;
    (void)TerminateTask();
}

static void Task_Idle_Entry(void) { runs_idle++; }

static const Os_TaskConfigType term_tasks[TASK_COUNT] = {
    { "Term_1ms",  Task_1ms_Entry,  0u, 1u, 0u, FALSE, FULL },
    { "Term_10ms", Task_10ms_Entry, 1u, 1u, 0u, FALSE, FULL },
    { "Term_Idle", Task_Idle_Entry, IDLE_PRIORITY, 1u, 1u, FALSE, FULL },
};

static const Os_ExpiryPointConfigType term_ep_1ms[1] = {
    { 1u, TASK_1MS, 0u },
};

static const Os_ScheduleTableConfigType term_tables[1] = {
    { "Term_1ms", 1u, TRUE, term_ep_1ms, 1u },
};

static const Os_StackMonitorConfigType term_budgets[TASK_COUNT] = {
    { TASK_1MS,  STACK_SIZE },
    { TASK_10MS, STACK_SIZE },
    { TASK_IDLE, STACK_SIZE },
};

static uint8 term_stack_1ms[STACK_SIZE]  __attribute__((aligned(8)));
static uint8 term_stack_10ms[STACK_SIZE] __attribute__((aligned(8)));
static uint8 term_stack_idle[STACK_SIZE] __attribute__((aligned(8)));

static const Os_TaskStackConfigType term_task_stacks[TASK_COUNT] = {
    { TASK_1MS,  term_stack_1ms,  STACK_SIZE },
    { TASK_10MS, term_stack_10ms, STACK_SIZE },
    { TASK_IDLE, term_stack_idle, STACK_SIZE },
};

static Os_ConfigType make_term_config(void)
{
    Os_ConfigType cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.Tasks = term_tasks;
    cfg.TaskCount = TASK_COUNT;
    cfg.ScheduleTables = term_tables;
    cfg.ScheduleTableCount = 1u;
    cfg.Stacks = term_budgets;
    cfg.StackCount = TASK_COUNT;
    cfg.TaskStacks = term_task_stacks;
    cfg.TaskStackCount = TASK_COUNT;
    return cfg;
}

static uint32* task_frame(TaskType TaskID)
{
    const Os_Port_Stm32_TaskContextType* ctx = Os_Port_Stm32_GetTaskContext(TaskID);

    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_TRUE(ctx->Prepared);
    return (uint32*)ctx->SavedPsp;
}

/** @brief Overwrite a prepared frame's PC slot so a later rebuild is visible. */
static void scribble_frame_pc(TaskType TaskID)
{
    task_frame(TaskID)[FRAME_PC_INDEX] = FRAME_SENTINEL;
}

static void start_production_os(void)
{
    Os_ConfigType cfg = make_term_config();

    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));
    Os_TestSetPreTaskHook(Pre_Hook);
    Os_TestSetPostTaskHook(Post_Hook);
    Os_TestSetErrorHook(Error_Hook);
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_TRUE(Os_Port_Stm32_GetBootstrapState()->FirstTaskStarted);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
}

static void isr_activate_1ms(void)  { (void)ActivateTask(TASK_1MS); }
static void isr_activate_both(void)
{
    (void)ActivateTask(TASK_1MS);
    (void)ActivateTask(TASK_10MS);
}

void setUp(void)
{
    runs_1ms = 0u;
    runs_10ms = 0u;
    runs_idle = 0u;
    reactivate_1ms_once = FALSE;
    pre_hook_count = 0u;
    post_hook_count = 0u;
    post_hook_task = INVALID_TASK;
    error_hook_count = 0u;
    error_hook_status = E_OK;
    Os_TestReset();
    Os_PortTargetInit();
}

void tearDown(void)
{
}

/**
 * @requirement When the PendSV dispatch is live, TerminateTask shall mark
 *              the task SUSPENDED per OSEK, select the preempted task from
 *              the restore stack WITHOUT rebuilding its frame (its saved
 *              context is live — BRINGUP-6 sequence), and request a context
 *              switch so PendSV lands in the resumed task.
 * @verify After the 1ms task (dispatched by tick preemption over idle)
 *         terminates: a second PendSV is staged selecting the idle task,
 *         the idle frame is NOT rewritten, completing the switch resumes
 *         idle in kernel and port without re-invoking its entry.
 */
void test_TerminateTask_switchback_resumes_preempted_without_rebuild(void)
{
    const Os_Port_Stm32_StateType* state;
    const Os_Port_Stm32_TaskContextType* idle_ctx;
    uintptr_t idle_saved_psp;
    TaskStateType task_state;

    start_production_os();
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_1MS, 0u));

    /* Tick: schedule table activates the 1ms task, preemption dispatch is
     * staged over idle (BRINGUP-5 path, already covered elsewhere). */
    Os_Port_Stm32_SysTickHandler();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL(TASK_1MS, state->SelectedNextTask);

    /* Sentinel the idle frame AFTER launch: any rebuild would rewrite it. */
    scribble_frame_pc(TASK_IDLE);

    /* Complete the preemption PendSV: the 1ms entry runs and terminates.
     * The kernel must stage the switchback before the entry call returns. */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    TEST_ASSERT_EQUAL_UINT8(1u, runs_1ms);

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL(TASK_IDLE, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT32(2u, state->PendSvRequestCount);

    /* Select WITHOUT rebuild: the staged PSP is idle's saved context and
     * the idle frame sentinel survives. */
    idle_ctx = Os_Port_Stm32_GetTaskContext(TASK_IDLE);
    idle_saved_psp = idle_ctx->SavedPsp;
    TEST_ASSERT_EQUAL_PTR((void*)idle_saved_psp, (void*)state->SelectedNextTaskPsp);
    TEST_ASSERT_EQUAL_HEX32(FRAME_SENTINEL, task_frame(TASK_IDLE)[FRAME_PC_INDEX]);

    /* Kernel state: terminated task SUSPENDED, idle restored as current. */
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_1MS, &task_state));
    TEST_ASSERT_EQUAL(SUSPENDED, task_state);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());

    /* Complete the switchback PendSV: port lands in idle, no entry re-run. */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL(TASK_IDLE, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT8(0u, runs_idle);   /* resume, not a fresh entry */
    TEST_ASSERT_EQUAL_UINT32(2u, state->TaskSwitchCount);
}

/**
 * @requirement PostTaskHook shall run in the termination path while the
 *              terminating task is still the current task (OSEK: on leaving
 *              RUNNING), before the switchback restores the preempted task.
 * @verify The post hook observes the 1ms task as current exactly once for
 *         its termination.
 */
void test_TerminateTask_switchback_calls_posttaskhook_before_restore(void)
{
    start_production_os();
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_1MS, 0u));

    Os_Port_Stm32_SysTickHandler();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());

    TEST_ASSERT_EQUAL_UINT8(1u, post_hook_count);
    TEST_ASSERT_EQUAL(TASK_1MS, post_hook_task);
}

/**
 * @requirement When a READY task with higher priority than the restored
 *              preempted task exists at termination (and it is not the
 *              terminating task itself), the switchback shall dispatch it
 *              freshly: frame REBUILT (stale saved context replaced —
 *              stm32-bringup-p3 lesson), select + context-switch request,
 *              preempted task pushed back on the restore stack,
 *              PreTaskHook per dispatch.
 * @verify With 1ms and 10ms activated together: 1ms runs and terminates,
 *         the switchback stages 10ms with a rebuilt frame instead of
 *         resuming idle; 10ms then runs, terminates, and idle resumes.
 */
void test_TerminateTask_switchback_dispatches_next_ready_with_fresh_frame(void)
{
    const Os_Port_Stm32_StateType* state;
    uint32 dispatches_before;

    start_production_os();
    dispatches_before = Os_TestGetDispatchCount();

    /* Activate both period tasks from ISR context: exit-ISR preemption
     * dispatches 1ms (prio 0); 10ms (prio 1) stays READY behind it. */
    TEST_ASSERT_EQUAL(E_OK, Os_TestInvokeIsrCat2(isr_activate_both));
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL(TASK_1MS, state->SelectedNextTask);

    /* Sentinel the 10ms frame: the switchback MUST rebuild it. */
    scribble_frame_pc(TASK_10MS);

    /* 1ms runs + terminates: switchback stages fresh dispatch of 10ms. */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    TEST_ASSERT_EQUAL_UINT8(1u, runs_1ms);
    TEST_ASSERT_EQUAL_UINT8(0u, runs_10ms);

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL(TASK_10MS, state->SelectedNextTask);
    TEST_ASSERT_EQUAL(TASK_10MS, Os_TestGetCurrentTask());
    TEST_ASSERT_EQUAL_HEX32(
        (uint32)((uintptr_t)Task_10ms_Entry & 0xFFFFFFFFu),
        task_frame(TASK_10MS)[FRAME_PC_INDEX]);          /* rebuilt */
    TEST_ASSERT_EQUAL_HEX32(INITIAL_TASK_LR,
        task_frame(TASK_10MS)[FRAME_LR_INDEX]);          /* poisoned LR */

    /* 10ms runs + terminates; idle resumes. */
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, runs_10ms);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
    TEST_ASSERT_EQUAL(TASK_IDLE, state->CurrentTask);
    TEST_ASSERT_FALSE(state->PendSvPending);
    /* launch + 1ms + 10ms dispatches (resume is not a fresh dispatch) */
    TEST_ASSERT_EQUAL_UINT32(dispatches_before + 2u, Os_TestGetDispatchCount());
    /* PreTaskHook per fresh dispatch: 1ms + 10ms */
    TEST_ASSERT_EQUAL_UINT8(2u, pre_hook_count);
}

/**
 * @requirement When termination leaves neither a preempted task to resume
 *              nor a dispatchable ready task, the kernel shall fail closed:
 *              report via ErrorHook (E_OS_STATE) and stage NO context
 *              switch (hardware parks; watchdog forces the safe state).
 * @verify Terminating the launch task with an empty restore stack reports
 *         E_OS_STATE and leaves the PendSV model untouched.
 */
void test_TerminateTask_switchback_fails_closed_when_nothing_to_run(void)
{
    const Os_Port_Stm32_StateType* state;

    start_production_os();

    /* Idle is the launch task; nothing preempted, nothing ready. */
    (void)TerminateTask();

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
    TEST_ASSERT_EQUAL(E_OS_STATE, error_hook_status);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, Os_TestGetCurrentTask());
}

/**
 * @requirement The schedule-table activation -> dispatch -> terminate ->
 *              switchback cycle shall be repeatable indefinitely (bench
 *              steady state): every period re-activation dispatches with a
 *              FRESH initial frame (rebuild replaces the stale context of
 *              the previous run) and returns to the idle task.
 * @verify Three full activate/terminate rounds: per round the staged 1ms
 *         frame is rebuilt (sentinel gone, PC = entry, poisoned LR), the
 *         run counter increments, and idle is resumed in kernel and port.
 */
void test_periodic_terminate_cycle_repeats_with_fresh_frames(void)
{
    const Os_Port_Stm32_StateType* state;
    uint8 round;

    start_production_os();
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_1MS, 0u));

    for (round = 1u; round <= 3u; round++) {
        /* Stale-frame injection: previous-run context must not survive. */
        scribble_frame_pc(TASK_1MS);

        Os_Port_Stm32_SysTickHandler();
        state = Os_Port_Stm32_GetBootstrapState();
        TEST_ASSERT_TRUE(state->PendSvPending);
        TEST_ASSERT_EQUAL(TASK_1MS, state->SelectedNextTask);
        TEST_ASSERT_EQUAL_HEX32(
            (uint32)((uintptr_t)Task_1ms_Entry & 0xFFFFFFFFu),
            task_frame(TASK_1MS)[FRAME_PC_INDEX]);       /* rebuilt */
        TEST_ASSERT_EQUAL_HEX32(INITIAL_TASK_LR,
            task_frame(TASK_1MS)[FRAME_LR_INDEX]);

        TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
        state = Os_Port_Stm32_GetBootstrapState();
        TEST_ASSERT_EQUAL_UINT8(round, runs_1ms);
        TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
        TEST_ASSERT_EQUAL(TASK_IDLE, state->CurrentTask);
        TEST_ASSERT_FALSE(state->PendSvPending);
        TEST_ASSERT_EQUAL_UINT32((uint32)round * 2u, state->TaskSwitchCount);
    }
}

/**
 * @requirement A task that terminates with a pending activation (BCC2,
 *              ActivationLimit > 1) shall re-enter READY, but shall NOT be
 *              redispatched from its own termination (rebuilding the frame
 *              of the still-executing stack corrupts live call frames —
 *              stm32-bringup-p3 lesson): the switchback resumes the
 *              preempted task and the next tick's preemption dispatch runs
 *              the pending activation with a fresh frame.
 * @verify 1ms task re-activates itself, terminates: state READY with one
 *         pending activation while idle is resumed; the next tick
 *         dispatches it freshly and it completes to SUSPENDED.
 */
void test_pending_activation_requeues_ready_then_redispatches_fresh(void)
{
    Os_TaskConfigType tasks[TASK_COUNT];
    Os_ConfigType cfg = make_term_config();
    const Os_Port_Stm32_StateType* state;
    TaskStateType task_state;
    uint8 idx;

    for (idx = 0u; idx < TASK_COUNT; idx++) {
        tasks[idx] = term_tasks[idx];
    }
    tasks[TASK_1MS].ActivationLimit = 2u;   /* BCC2 queueing */
    cfg.Tasks = tasks;

    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));
    Os_TestSetErrorHook(Error_Hook);
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());

    reactivate_1ms_once = TRUE;

    /* First activation via ISR: dispatch, self-reactivate, terminate. */
    TEST_ASSERT_EQUAL(E_OK, Os_TestInvokeIsrCat2(isr_activate_1ms));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    TEST_ASSERT_EQUAL_UINT8(1u, runs_1ms);

    /* Re-queued READY with the pending activation; idle staged for resume
     * (never a self-redispatch from the terminating context). */
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_1MS, &task_state));
    TEST_ASSERT_EQUAL(READY, task_state);
    TEST_ASSERT_EQUAL_UINT8(1u, Os_TestGetPendingActivations(TASK_1MS));
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(TASK_IDLE, state->SelectedNextTask);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());

    /* Next tick: exit-ISR preemption dispatches the pending activation
     * with a fresh frame; it runs once more and completes to SUSPENDED. */
    scribble_frame_pc(TASK_1MS);
    Os_Port_Stm32_SysTickHandler();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL(TASK_1MS, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_HEX32(
        (uint32)((uintptr_t)Task_1ms_Entry & 0xFFFFFFFFu),
        task_frame(TASK_1MS)[FRAME_PC_INDEX]);           /* fresh frame */

    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_UINT8(2u, runs_1ms);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_1MS, &task_state));
    TEST_ASSERT_EQUAL(SUSPENDED, task_state);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetPendingActivations(TASK_1MS));
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
}

/**
 * @requirement ChainTask on a live PendSV dispatch is NOT covered by the
 *              BRINGUP-6-validated switchback sequence; it shall fail
 *              closed: reject with E_OS_CALLEVEL (ErrorHook reported)
 *              WITHOUT terminating the caller or staging a switch
 *              (documented limitation — no untested switch machinery).
 * @verify ChainTask from the live launch task returns E_OS_CALLEVEL, the
 *         caller stays RUNNING and current, nothing is activated or staged.
 */
void test_ChainTask_rejected_fail_closed_when_dispatch_live(void)
{
    const Os_Port_Stm32_StateType* state;
    TaskStateType task_state;

    start_production_os();

    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, ChainTask(TASK_1MS));

    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, error_hook_status);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_IDLE, &task_state));
    TEST_ASSERT_EQUAL(RUNNING, task_state);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_1MS, &task_state));
    TEST_ASSERT_EQUAL(SUSPENDED, task_state);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetPendingActivations(TASK_1MS));
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
}

/**
 * @requirement The termination switchback engages ONLY when the port
 *              dispatch is live; host-cooperative dispatch (no task-stack
 *              table, FirstTaskStarted FALSE) keeps the pre-existing
 *              TerminateTask behavior unchanged.
 * @verify With Os_TestConfigure*-driven config (no stacks), TerminateTask
 *         completes the task without staging any PendSV.
 */
void test_TerminateTask_without_live_dispatch_keeps_host_behavior(void)
{
    const Os_Port_Stm32_StateType* state;
    TaskStateType task_state;

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(term_tasks, TASK_COUNT));
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_FALSE(Os_Port_Stm32_GetBootstrapState()->FirstTaskStarted);

    /* Legacy inline dispatch already ran the autostart idle entry. */
    TEST_ASSERT_EQUAL_UINT8(1u, runs_idle);

    TEST_ASSERT_EQUAL(E_OK, Os_TestSetCurrentTaskRunning(TASK_1MS));
    TEST_ASSERT_EQUAL(E_OK, TerminateTask());

    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_1MS, &task_state));
    TEST_ASSERT_EQUAL(SUSPENDED, task_state);
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(0u, state->PendSvRequestCount);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_TerminateTask_switchback_resumes_preempted_without_rebuild);
    RUN_TEST(test_TerminateTask_switchback_calls_posttaskhook_before_restore);
    RUN_TEST(test_TerminateTask_switchback_dispatches_next_ready_with_fresh_frame);
    RUN_TEST(test_TerminateTask_switchback_fails_closed_when_nothing_to_run);
    RUN_TEST(test_periodic_terminate_cycle_repeats_with_fresh_frames);
    RUN_TEST(test_pending_activation_requeues_ready_then_redispatches_fresh);
    RUN_TEST(test_ChainTask_rejected_fail_closed_when_dispatch_live);
    RUN_TEST(test_TerminateTask_without_live_dispatch_keeps_host_behavior);
    return UNITY_END();
}
