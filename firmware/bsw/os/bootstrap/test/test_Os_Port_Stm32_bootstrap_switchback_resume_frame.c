/**
 * @file    test_Os_Port_Stm32_bootstrap_switchback_resume_frame.c
 * @brief   S-OS-31-FIX-01/02: switchback resume-frame validity (fail-closed)
 * @date    2026-07-08
 *
 * @details Reproduces the on-target defect found during S-OS-31 bringup on
 *          the 3x G474RE bench (docs/plans/memo-s-os-31-switchback-resume-defect.md):
 *          the task-termination switchback resumes a preempted task WITHOUT
 *          rebuilding its frame (Os_Port_StageConfiguredResume), trusting its
 *          SavedPsp holds a live saved context.  Under a kernel/port
 *          bookkeeping desync the resumed task's SavedPsp frame is stale/
 *          zeroed (stacked PC=0, xPSR T-bit clear); on hardware PendSV then
 *          exception-returns to PC=0 with the Thumb bit clear -> INVSTATE
 *          UsageFault -> forced HardFault (CFSR=0x00020000, HFSR=0x40000000).
 *
 *          The existing host switchback suite asserts a staging boolean, not
 *          a real restored frame, so it never modelled a stale resume frame.
 *          These tests model it directly: the resume target's saved frame is
 *          corrupted (PC/xPSR zeroed) before the switchback resumes it, and
 *          the port+kernel must FAIL CLOSED (F-A guard) rather than stage a
 *          context switch into an invalid frame (which would HardFault on
 *          silicon).  Fail-closed converts an uncontrolled HardFault into the
 *          documented watchdog-starve safe-state reaction (ASIL-D).
 *
 * @verifies S-OS-31-FIX-02 fail-closed resume-frame guard
 *           (docs/plans/memo-s-os-31-switchback-resume-defect.md)
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

#define FRAME_PC_INDEX         15u
#define FRAME_XPSR_INDEX       16u
#define XPSR_THUMB_BIT         0x01000000u

#define TASK_1MS               ((TaskType)0u)
#define TASK_10MS              ((TaskType)1u)
#define TASK_IDLE              ((TaskType)2u)
#define TASK_COUNT             3u
#define IDLE_PRIORITY          ((uint8)(OS_MAX_PRIORITIES - 1u))

#define TABLE_1MS              ((ScheduleTableType)0u)
#define STACK_SIZE             256u

static uint8 runs_1ms;
static uint8 runs_idle;
static uint8 error_hook_count;
static StatusType error_hook_status;

static void Error_Hook(StatusType Error)
{
    /* Host artifact: OS_STACK_SAMPLE measures the HOST call stack against the
     * target stack arrays and reports E_OS_LIMIT. Ignore it. */
    if (Error == E_OS_LIMIT) {
        return;
    }
    error_hook_count++;
    error_hook_status = Error;
}

static void Task_1ms_Entry(void)  { runs_1ms++;  (void)TerminateTask(); }
static void Task_10ms_Entry(void) { (void)TerminateTask(); }
static void Task_Idle_Entry(void) { runs_idle++; }

static const Os_TaskConfigType res_tasks[TASK_COUNT] = {
    { "Res_1ms",  Task_1ms_Entry,  0u, 1u, 0u, FALSE, FULL },
    { "Res_10ms", Task_10ms_Entry, 1u, 1u, 0u, FALSE, FULL },
    { "Res_Idle", Task_Idle_Entry, IDLE_PRIORITY, 1u, 1u, FALSE, FULL },
};

static const Os_ExpiryPointConfigType res_ep_1ms[1] = { { 1u, TASK_1MS, 0u } };
static const Os_ScheduleTableConfigType res_tables[1] = {
    { "Res_1ms", 1u, TRUE, res_ep_1ms, 1u },
};

static const Os_StackMonitorConfigType res_budgets[TASK_COUNT] = {
    { TASK_1MS, STACK_SIZE }, { TASK_10MS, STACK_SIZE }, { TASK_IDLE, STACK_SIZE },
};
static uint8 res_stack_1ms[STACK_SIZE]  __attribute__((aligned(8)));
static uint8 res_stack_10ms[STACK_SIZE] __attribute__((aligned(8)));
static uint8 res_stack_idle[STACK_SIZE] __attribute__((aligned(8)));
static const Os_TaskStackConfigType res_task_stacks[TASK_COUNT] = {
    { TASK_1MS,  res_stack_1ms,  STACK_SIZE },
    { TASK_10MS, res_stack_10ms, STACK_SIZE },
    { TASK_IDLE, res_stack_idle, STACK_SIZE },
};

static Os_ConfigType make_config(void)
{
    Os_ConfigType cfg;
    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.Tasks = res_tasks;
    cfg.TaskCount = TASK_COUNT;
    cfg.ScheduleTables = res_tables;
    cfg.ScheduleTableCount = 1u;
    cfg.Stacks = res_budgets;
    cfg.StackCount = TASK_COUNT;
    cfg.TaskStacks = res_task_stacks;
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

/** @brief Model the on-target stale/zeroed saved frame (PC=0, xPSR T-bit clear). */
static void corrupt_resume_frame(TaskType TaskID)
{
    uint32* frame = task_frame(TaskID);
    frame[FRAME_PC_INDEX] = 0u;
    frame[FRAME_XPSR_INDEX] = 0u;
}

static void start_production_os(void)
{
    Os_ConfigType cfg = make_config();
    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));
    Os_TestSetErrorHook(Error_Hook);
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_TRUE(Os_Port_Stm32_GetBootstrapState()->FirstTaskStarted);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
}

void setUp(void)
{
    runs_1ms = 0u;
    runs_idle = 0u;
    error_hook_count = 0u;
    error_hook_status = E_OK;
    Os_TestReset();
    Os_PortTargetInit();
}

void tearDown(void) {}

/**
 * @requirement The port shall NOT stage a context switch into a task frame
 *              that lacks a valid resume context (stacked PC==0 or xPSR
 *              Thumb bit clear).  Os_Port_Stm32_SelectNextTask shall reject
 *              such a target (E_OS_STATE) so no INVSTATE HardFault can occur.
 * @verify Selecting a task whose saved frame is zeroed returns E_OS_STATE
 *         and leaves no switch staged.
 */
void test_select_next_task_rejects_invalid_frame(void)
{
    const Os_Port_Stm32_StateType* state;

    start_production_os();   /* idle launched: its saved frame is prepared+valid */

    /* Nothing staged yet. */
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);

    corrupt_resume_frame(TASK_IDLE);

    /* Directly exercise the select-for-resume seam: an invalid frame is
     * rejected and NO selection is staged. */
    TEST_ASSERT_EQUAL(E_OS_STATE, Os_Port_Stm32_SelectNextTask(TASK_IDLE));
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
}

/**
 * @requirement When the termination switchback would resume a preempted task
 *              whose saved frame is not a live resume context, the kernel
 *              shall FAIL CLOSED: report E_OS_STATE via ErrorHook and stage
 *              NO context switch (hardware parks; watchdog forces the safe
 *              state) instead of resuming into it (on-target: INVSTATE
 *              HardFault).
 * @verify After the 1ms task terminates with the idle resume frame zeroed,
 *         the switchback reports E_OS_STATE and stages no resume.
 */
void test_switchback_resume_into_stale_frame_fails_closed(void)
{
    const Os_Port_Stm32_StateType* state;

    start_production_os();
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_1MS, 0u));

    /* Tick stages the 1ms dispatch over idle. */
    Os_Port_Stm32_SysTickHandler();
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(TASK_1MS, state->SelectedNextTask);

    /* Model the on-target stale/zeroed resume context for idle. */
    corrupt_resume_frame(TASK_IDLE);

    /* Complete the 1ms dispatch: 1ms runs and terminates; the switchback
     * attempts to resume idle WITHOUT rebuild from its (now zeroed) frame. */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    TEST_ASSERT_EQUAL_UINT8(1u, runs_1ms);

    /* FAIL CLOSED: E_OS_STATE reported, no switch staged into the bad frame. */
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
    TEST_ASSERT_EQUAL(E_OS_STATE, error_hook_status);
    TEST_ASSERT_FALSE(state->PendSvPending);
}

/**
 * @requirement The guard shall NOT interfere with a valid resume: a live
 *              saved frame (PC != 0, xPSR Thumb bit set) resumes normally.
 * @verify With idle's frame intact, the 1ms termination resumes idle and
 *         reports no error.
 */
void test_switchback_resume_with_valid_frame_unaffected(void)
{
    const Os_Port_Stm32_StateType* state;

    start_production_os();
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_1MS, 0u));

    Os_Port_Stm32_SysTickHandler();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, runs_1ms);
    TEST_ASSERT_EQUAL_UINT8(0u, error_hook_count);
    TEST_ASSERT_EQUAL(TASK_IDLE, state->SelectedNextTask);   /* idle resume staged */
}

/* ==================================================================
 * S-OS-31-FIX-03 (F-B) + FIX-04 (F-C): per-task saved-context-valid
 * invariant and terminated-task save suppression.
 *
 * Root mechanism (memo section 7): the port advances its own CurrentTask
 * only inside ResolvePendSvTarget (one PendSV = one save), while the kernel
 * advances os_current_task and pushes to os_preempted_task_stack
 * speculatively.  Under the terminate->PendSV park-gap coalescing race a
 * single PendSV can save an outgoing (terminated/dead) task's frame OVER a
 * frame just rebuilt for its re-dispatch, or leave a pushed task without a
 * live saved frame.  F-B adds a per-task SavedContextValid flag (the resume
 * gate becomes authoritative, not a byte heuristic); F-C suppresses the save
 * of a terminated task's dead frame so the coalesced save cannot clobber a
 * rebuilt frame.
 *
 * HOST-MODEL LIMITATION (documented, memo section 3/5a/7 caveat): the mock's
 * ResolvePendSvTarget moves pointers and never runs STMDB, and every task
 * uses one fixed prepared_psp, so the literal clobbered-frame / zeroed-frame
 * HardFault is NOT host-reproducible — only the 3x G474RE on-target soak
 * (FIX-05) proves the silicon fix.  These tests are unit-level guards on the
 * new invariant and the suppress API, not an end-to-end fault reproduction.
 * ================================================================== */

static const Os_Port_Stm32_TaskContextType* ctx_of(TaskType TaskID)
{
    const Os_Port_Stm32_TaskContextType* ctx = Os_Port_Stm32_GetTaskContext(TaskID);
    TEST_ASSERT_NOT_NULL(ctx);
    return ctx;
}

/**
 * @requirement The port shall track, per task, whether its SavedPsp holds a
 *              live saved context (SavedContextValid): TRUE once a fresh
 *              initial frame is built or the task's live frame is saved by
 *              PendSV; FALSE once the task is restored (running on the CPU,
 *              its saved frame consumed).
 * @verify After the 1ms task is dispatched over idle and the PendSV
 *         completes, the preempted (saved) idle task reports
 *         SavedContextValid TRUE while the running 1ms task reports FALSE.
 */
void test_saved_context_valid_tracks_save_and_restore(void)
{
    start_production_os();
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_1MS, 0u));

    /* Freshly prepared, not yet run: initial frame is a valid resume point. */
    TEST_ASSERT_TRUE(ctx_of(TASK_10MS)->SavedContextValid);

    /* Tick stages the 1ms dispatch over idle; PendSV saves idle, restores 1ms. */
    Os_Port_Stm32_SysTickHandler();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    TEST_ASSERT_EQUAL_UINT8(1u, runs_1ms);

    /* idle was preempted -> its saved frame is live; 1ms ran -> no live save. */
    TEST_ASSERT_TRUE(ctx_of(TASK_IDLE)->SavedContextValid);
    TEST_ASSERT_FALSE(ctx_of(TASK_1MS)->SavedContextValid);
}

/**
 * @requirement Os_Port_SuppressTaskSave shall mark a task's NEXT PendSV save
 *              as suppressed (one-shot): the following ResolvePendSvTarget
 *              shall NOT overwrite that task's SavedPsp and shall NOT mark its
 *              context valid (its frame is dead / a re-dispatch may have
 *              rebuilt over it), then shall clear the suppression.
 * @verify After suppressing the running 1ms task and completing a PendSV that
 *         switches to idle, 1ms reports SaveSuppressed cleared and
 *         SavedContextValid FALSE (the skipped save did not mark it valid).
 */
void test_suppress_task_save_skips_next_save_one_shot(void)
{
    start_production_os();
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_1MS, 0u));

    /* 1ms running over the (saved) idle task. */
    Os_Port_Stm32_SysTickHandler();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    TEST_ASSERT_EQUAL(TASK_1MS, Os_Port_Stm32_GetBootstrapState()->CurrentTask);
    TEST_ASSERT_FALSE(ctx_of(TASK_1MS)->SavedContextValid);

    /* Suppress the terminated task's dead-frame save. */
    Os_Port_SuppressTaskSave(TASK_1MS);
    TEST_ASSERT_TRUE(ctx_of(TASK_1MS)->SaveSuppressed);

    /* Stage a resume back to the (still valid) idle frame and complete PendSV. */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_StageConfiguredResume(TASK_IDLE));
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());

    /* Suppression consumed; the skipped save did NOT mark 1ms context valid. */
    TEST_ASSERT_FALSE(ctx_of(TASK_1MS)->SaveSuppressed);
    TEST_ASSERT_FALSE(ctx_of(TASK_1MS)->SavedContextValid);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_Port_Stm32_GetBootstrapState()->CurrentTask);
}

/**
 * @requirement The terminate switchback shall suppress the terminated task's
 *              next save (F-C) so that, even when a re-dispatch of that task
 *              interposes before the switchback PendSV runs (coalescing race),
 *              no spurious fail-closed occurs and the kernel stays runnable.
 * @verify With 1ms running over idle, 1ms terminates (switchback stages a
 *         resume of idle and suppresses the 1ms save); completing the PendSV
 *         resumes idle with no E_OS_STATE report and the 1ms save suppressed.
 */
void test_terminate_switchback_suppresses_terminated_save(void)
{
    start_production_os();
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_1MS, 0u));

    Os_Port_Stm32_SysTickHandler();

    /* Completing this dispatch runs the 1ms entry -> TerminateTask ->
     * os_terminate_switchback, which must suppress the terminated 1ms save
     * and stage the idle resume without reporting a fail-closed error. */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    TEST_ASSERT_EQUAL_UINT8(1u, runs_1ms);
    TEST_ASSERT_EQUAL_UINT8(0u, error_hook_count);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_Port_Stm32_GetBootstrapState()->SelectedNextTask);

    /* Complete the switchback PendSV: idle resumes, no fail-closed, and the
     * terminated task's save stayed suppressed through the resolve (consumed). */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    TEST_ASSERT_EQUAL_UINT8(0u, error_hook_count);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_Port_Stm32_GetBootstrapState()->CurrentTask);
    TEST_ASSERT_FALSE(ctx_of(TASK_1MS)->SaveSuppressed);
}

/* ==================================================================
 * S-OS-31-FIX-06 (GAP-A, memo section 8.3/8.5): consume-time resume gate.
 *
 * Every FIX-02/03 resume gate runs at STAGE time (Os_Port_Stm32_SelectNextTask).
 * ResolvePendSvTarget re-reads the target's SavedPsp at CONSUME time and,
 * before FIX-06, exception-returned into it with no re-validation — anything
 * that invalidates the staged frame between staging and PendSV entry defeated
 * every guard (the free-running INVSTATE class).  FIX-06 re-validates the
 * staged target inside ResolvePendSvTarget; on failure the port stays on the
 * interrupted context (fail closed, no invalid exception return possible) and
 * counts the event in DesyncFailClosedCount.
 * ================================================================== */

/**
 * @requirement ResolvePendSvTarget shall re-validate the staged target at
 *              consume time: a target whose SavedContextValid was revoked
 *              AFTER a (then-valid) staging shall NOT be adopted; the port
 *              stays on the interrupted context and increments
 *              DesyncFailClosedCount.
 * @verify Stage a valid select of the 10ms task, revoke its context validity
 *         (SuppressTaskSave), run PendSV: the port stays on idle, counts one
 *         fail-closed event, and consumes the selection.
 */
void test_pendsv_rejects_target_invalidated_after_staging(void)
{
    const Os_Port_Stm32_StateType* state;

    start_production_os();

    /* Valid staging (initial frame, SavedContextValid TRUE from prepare). */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_SelectNextTask(TASK_10MS));
    Os_PortRequestContextSwitch();
    TEST_ASSERT_TRUE(Os_Port_Stm32_GetBootstrapState()->PendSvPending);

    /* TOCTOU: validity revoked AFTER staging, BEFORE the PendSV consumes it. */
    Os_Port_Stm32_SuppressTaskSave(TASK_10MS);
    TEST_ASSERT_FALSE(ctx_of(TASK_10MS)->SavedContextValid);

    Os_Port_Stm32_PendSvHandler();

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(TASK_IDLE, state->CurrentTask);          /* no adoption */
    TEST_ASSERT_EQUAL_UINT32(1u, state->DesyncFailClosedCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);  /* consumed    */
    TEST_ASSERT_FALSE(state->PendSvPending);
}

/**
 * @requirement ResolvePendSvTarget shall re-validate the staged target's frame
 *              BYTES at consume time: a frame corrupted (stacked PC=0 / xPSR
 *              T-bit clear) after a valid staging shall NOT be exception-
 *              returned into (on-target: INVSTATE HardFault); the port stays
 *              on the interrupted context and increments DesyncFailClosedCount.
 * @verify Stage a valid select of the 10ms task, then zero its frame's PC and
 *         xPSR, run PendSV: the port stays on idle and counts one fail-closed
 *         event.
 */
void test_pendsv_rejects_target_frame_corrupted_after_staging(void)
{
    const Os_Port_Stm32_StateType* state;

    start_production_os();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_SelectNextTask(TASK_10MS));
    Os_PortRequestContextSwitch();

    /* TOCTOU: frame bytes die AFTER staging (SavedContextValid still TRUE). */
    corrupt_resume_frame(TASK_10MS);
    TEST_ASSERT_TRUE(ctx_of(TASK_10MS)->SavedContextValid);

    Os_Port_Stm32_PendSvHandler();

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(TASK_IDLE, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(1u, state->DesyncFailClosedCount);
    TEST_ASSERT_EQUAL(INVALID_TASK, state->SelectedNextTask);
    TEST_ASSERT_FALSE(state->PendSvPending);
}

/**
 * @requirement The consume-time gate shall NOT interfere with a valid staged
 *              target: a select whose frame stays live is adopted normally and
 *              no fail-closed event is counted.
 * @verify Stage a valid select of the 10ms task and run PendSV untouched: the
 *         port adopts the 10ms task and DesyncFailClosedCount stays zero.
 */
void test_pendsv_adopts_valid_target_without_fail_closed(void)
{
    const Os_Port_Stm32_StateType* state;

    start_production_os();

    TEST_ASSERT_EQUAL(E_OK, Os_Port_Stm32_SelectNextTask(TASK_10MS));
    Os_PortRequestContextSwitch();
    Os_Port_Stm32_PendSvHandler();

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL(TASK_10MS, state->CurrentTask);
    TEST_ASSERT_EQUAL_UINT32(0u, state->DesyncFailClosedCount);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_select_next_task_rejects_invalid_frame);
    RUN_TEST(test_switchback_resume_into_stale_frame_fails_closed);
    RUN_TEST(test_switchback_resume_with_valid_frame_unaffected);
    RUN_TEST(test_saved_context_valid_tracks_save_and_restore);
    RUN_TEST(test_suppress_task_save_skips_next_save_one_shot);
    RUN_TEST(test_terminate_switchback_suppresses_terminated_save);
    RUN_TEST(test_pendsv_rejects_target_invalidated_after_staging);
    RUN_TEST(test_pendsv_rejects_target_frame_corrupted_after_staging);
    RUN_TEST(test_pendsv_adopts_valid_target_without_fail_closed);
    return UNITY_END();
}
