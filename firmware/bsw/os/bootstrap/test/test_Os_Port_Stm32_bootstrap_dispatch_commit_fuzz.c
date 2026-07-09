/**
 * @file    test_Os_Port_Stm32_bootstrap_dispatch_commit_fuzz.c
 * @brief   S-OS-31-FIX-10: kernel/port single-advance commit invariant
 * @date    2026-07-09
 *
 * @details The S-OS-31 root defect (memo sections 7.2/8.6/8.8) is a kernel
 *          push/advance that is SPECULATIVE with respect to the single
 *          physical PendSV save: the kernel could push a task onto
 *          os_preempted_task_stack whose live context had not been (and
 *          under coalescing never would be) saved by the port.  FIX-10
 *          (memo 8.8, option A + C) moves the kernel push/pop/advance into
 *          the PendSV commit (Os_BootstrapCommitDispatch, called by
 *          Os_Port_Stm32_ResolvePendSvTarget on adoption), so the invariant
 *
 *            every task on os_preempted_task_stack has SavedContextValid TRUE
 *
 *          holds at EVERY point where thread-level or ISR-level kernel code
 *          can observe it.  The host mock exposes exactly the hardware
 *          observation windows: Os_Port_Stm32_SysTickHandler is the SysTick
 *          ISR (stage), Os_Port_CompleteConfiguredDispatch is the PendSV
 *          (commit) — the harness interleaves them freely, which models
 *          SysTick landing in the stage->PendSV gap (the 7.2 race).
 *
 *          HOST-MODEL LIMITATION (memo section 7.4 caveat, unchanged): the
 *          mock moves pointers, it does not stack registers — the literal
 *          zeroed-frame INVSTATE is silicon-only.  These tests machine-check
 *          the bookkeeping invariant; the 3x G474RE free-run soak (FIX-10c)
 *          is the sole end-to-end acceptance.
 *
 * @verifies S-OS-31-FIX-10 kernel/port single-advance reconciliation
 *           (docs/plans/memo-s-os-31-switchback-resume-defect.md section 8.8)
 * @standard OSEK/VDX, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "Os.h"
#include "Os_Cfg_Types.h"
#include "Os_Internal.h"
#include "Os_Port_Stm32.h"
#include "Os_Port_TaskBinding.h"

#define TASK_FAST              ((TaskType)0u)   /* prio 0, every 2 ticks  */
#define TASK_MID               ((TaskType)1u)   /* prio 1, every 5 ticks  */
#define TASK_SLOW              ((TaskType)2u)   /* prio 2, every 11 ticks */
#define TASK_IDLE              ((TaskType)3u)
#define TASK_COUNT             4u
#define IDLE_PRIORITY          ((uint8)(OS_MAX_PRIORITIES - 1u))

#define TABLE_FAST             ((ScheduleTableType)0u)
#define TABLE_MID              ((ScheduleTableType)1u)
#define TABLE_SLOW             ((ScheduleTableType)2u)
#define STACK_SIZE             256u

/* Fuzz shape: seeds and length are FIXED so every run is reproducible.
 * 8 seeds x 20000 steps; action mix biased toward ticks so staged-but-
 * uncommitted windows (the 7.2 race surface) are hit constantly. */
static const unsigned int fuzz_seeds[] = {
    0x5EED0001u, 0x5EED0002u, 0x5EED0003u, 0x5EED0005u,
    0x5EED0008u, 0x5EED000Du, 0x5EED0015u, 0x5EED0022u,
};
#define FUZZ_STEPS_PER_SEED    20000u

static uint32 runs_fast;
static uint32 runs_mid;
static uint32 runs_slow;
static uint8 fatal_hook_count;
static StatusType fatal_hook_status;

static void Error_Hook(StatusType Error)
{
    /* E_OS_LIMIT: host stack-sample artifact + activation-limit hits when
     * the fuzz delays PendSV completion across table expiries — expected,
     * not a desync.  Everything else (E_OS_STATE fail-closed above all) is
     * fatal for the invariant run. */
    if (Error == E_OS_LIMIT) {
        return;
    }
    fatal_hook_count++;
    fatal_hook_status = Error;
}

static void Task_Fast_Entry(void) { runs_fast++; (void)TerminateTask(); }
static void Task_Mid_Entry(void)  { runs_mid++;  (void)TerminateTask(); }
static void Task_Slow_Entry(void) { runs_slow++; (void)TerminateTask(); }
static void Task_Idle_Entry(void) { }

static const Os_TaskConfigType fz_tasks[TASK_COUNT] = {
    { "Fz_Fast", Task_Fast_Entry, 0u, 3u, 0u, FALSE, FULL },
    { "Fz_Mid",  Task_Mid_Entry,  1u, 3u, 0u, FALSE, FULL },
    { "Fz_Slow", Task_Slow_Entry, 2u, 3u, 0u, FALSE, FULL },
    { "Fz_Idle", Task_Idle_Entry, IDLE_PRIORITY, 1u, 1u, FALSE, FULL },
};

static const Os_ExpiryPointConfigType fz_ep_fast[1] = { { 1u, TASK_FAST, 0u } };
static const Os_ExpiryPointConfigType fz_ep_mid[1]  = { { 2u, TASK_MID,  0u } };
static const Os_ExpiryPointConfigType fz_ep_slow[1] = { { 4u, TASK_SLOW, 0u } };
static const Os_ScheduleTableConfigType fz_tables[3] = {
    { "Fz_Fast", 2u,  TRUE, fz_ep_fast, 1u },
    { "Fz_Mid",  5u,  TRUE, fz_ep_mid,  1u },
    { "Fz_Slow", 11u, TRUE, fz_ep_slow, 1u },
};

static const Os_StackMonitorConfigType fz_budgets[TASK_COUNT] = {
    { TASK_FAST, STACK_SIZE }, { TASK_MID, STACK_SIZE },
    { TASK_SLOW, STACK_SIZE }, { TASK_IDLE, STACK_SIZE },
};
static uint8 fz_stack_fast[STACK_SIZE] __attribute__((aligned(8)));
static uint8 fz_stack_mid[STACK_SIZE]  __attribute__((aligned(8)));
static uint8 fz_stack_slow[STACK_SIZE] __attribute__((aligned(8)));
static uint8 fz_stack_idle[STACK_SIZE] __attribute__((aligned(8)));
static const Os_TaskStackConfigType fz_task_stacks[TASK_COUNT] = {
    { TASK_FAST, fz_stack_fast, STACK_SIZE },
    { TASK_MID,  fz_stack_mid,  STACK_SIZE },
    { TASK_SLOW, fz_stack_slow, STACK_SIZE },
    { TASK_IDLE, fz_stack_idle, STACK_SIZE },
};

static Os_ConfigType make_config(void)
{
    Os_ConfigType cfg;
    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.Tasks = fz_tasks;
    cfg.TaskCount = TASK_COUNT;
    cfg.ScheduleTables = fz_tables;
    cfg.ScheduleTableCount = 3u;
    cfg.Stacks = fz_budgets;
    cfg.StackCount = TASK_COUNT;
    cfg.TaskStacks = fz_task_stacks;
    cfg.TaskStackCount = TASK_COUNT;
    return cfg;
}

static void start_production_os(void)
{
    Os_ConfigType cfg = make_config();

    runs_fast = 0u;
    runs_mid = 0u;
    runs_slow = 0u;
    fatal_hook_count = 0u;
    fatal_hook_status = E_OK;

    Os_TestReset();
    Os_PortTargetInit();
    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));
    Os_TestSetErrorHook(Error_Hook);
    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_TRUE(Os_Port_Stm32_GetBootstrapState()->FirstTaskStarted);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_FAST, 0u));
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_MID, 0u));
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_SLOW, 0u));
}

void setUp(void) {}
void tearDown(void) {}

/**
 * @brief The FIX-10 invariant, checked at a harness observation point.
 *
 * Observation points = the instruction boundaries where kernel code can run
 * outside PendSV: after every SysTick ISR return and after every PendSV
 * completion.  These are exactly the windows the 7.2 race consumes.
 */
static void assert_dispatch_invariants(const char* Phase)
{
    const Os_Port_Stm32_StateType* state = Os_Port_Stm32_GetBootstrapState();
    uint8 idx;

    for (idx = 0u; idx < os_preempted_task_depth; idx++) {
        const Os_Port_Stm32_TaskContextType* ctx =
            Os_Port_Stm32_GetTaskContext(os_preempted_task_stack[idx]);

        TEST_ASSERT_NOT_NULL(ctx);
        if (ctx->SavedContextValid == FALSE) {
            TEST_FAIL_MESSAGE(Phase);
        }
    }

    /* With no PendSV in flight the kernel and port must agree on who is
     * physically running (the double-advance skew is exactly a disagreement
     * here). */
    if ((state->PendSvPending == FALSE) &&
        (Os_TestGetCurrentTask() != state->CurrentTask)) {
        TEST_FAIL_MESSAGE(Phase);
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, state->DesyncFailClosedCount, Phase);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, fatal_hook_count, Phase);
}

/**
 * @requirement A kernel push to os_preempted_task_stack shall be paired
 *              one-to-one with the port save of that task's live context:
 *              no observation point outside PendSV may see a stacked task
 *              with SavedContextValid FALSE (memo 8.8, FIX-10 invariant).
 * @verify The very first tick dispatch: the SysTick ISR stages the fast
 *         task over the just-launched idle task (whose initial frame the
 *         launch consumed, SavedContextValid FALSE).  Pre-FIX-10 the kernel
 *         pushes idle at stage time — the returned-from-ISR state shows a
 *         stacked task without a saved context, the exact state the 7.2
 *         race consumes.  Post-FIX-10 the push happens inside the PendSV
 *         commit, after the save made the flag TRUE.
 */
void test_tick_dispatch_does_not_push_before_port_save(void)
{
    start_production_os();

    /* Tick 1: fast task due (offset 1).  This is the SysTick ISR returning
     * with a dispatch staged but its PendSV not yet entered. */
    Os_Port_Stm32_SysTickHandler();
    assert_dispatch_invariants("after tick-1 stage, before PendSV");

    /* PendSV lands: save idle -> commit -> fast task runs and terminates. */
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    assert_dispatch_invariants("after dispatch commit");
    TEST_ASSERT_EQUAL_UINT32(1u, runs_fast);

    /* Drain the switchback PendSV staged by the termination. */
    while (Os_Port_CompleteConfiguredDispatch() == E_OK) {
        assert_dispatch_invariants("after switchback drain");
    }
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
}

/**
 * @requirement A SysTick landing between a termination switchback's staging
 *              and its PendSV (the memo 7.2 park-gap race) shall not produce
 *              a second kernel advance: one PendSV, one commit, and the
 *              invariant holds through the entire interleaving.
 * @verify Deterministic replay of the 7.2 shape: fast task terminates
 *         (switchback staged, PendSV pending), two further ticks land
 *         before the PendSV completes, then the PendSVs drain.  At every
 *         observation point the invariant holds and the kernel keeps
 *         running (liveness: all three periodic tasks accumulate runs).
 */
void test_tick_in_switchback_gap_causes_no_double_advance(void)
{
    uint32 step;

    start_production_os();

    /* Tick 1 stages fast-over-idle; complete it: fast runs, terminates,
     * and its switchback stages a resume with PendSV pending — the park
     * gap, held open by the harness. */
    Os_Port_Stm32_SysTickHandler();
    TEST_ASSERT_EQUAL(E_OK, Os_Port_CompleteConfiguredDispatch());
    TEST_ASSERT_EQUAL_UINT32(1u, runs_fast);
    TEST_ASSERT_TRUE(Os_Port_Stm32_GetBootstrapState()->PendSvPending);

    /* SysTicks land inside the gap (tick 2 = mid task due, tick 3 = fast
     * due again): pre-FIX-10 this is the second speculative advance. */
    Os_Port_Stm32_SysTickHandler();
    assert_dispatch_invariants("tick-2 inside switchback gap");
    Os_Port_Stm32_SysTickHandler();
    assert_dispatch_invariants("tick-3 inside switchback gap");

    /* Drain everything; the kernel must come back consistent and alive. */
    for (step = 0u; step < 32u; step++) {
        if (Os_Port_CompleteConfiguredDispatch() != E_OK) {
            break;
        }
        assert_dispatch_invariants("drain after gap ticks");
    }

    for (step = 0u; step < 64u; step++) {
        Os_Port_Stm32_SysTickHandler();
        assert_dispatch_invariants("post-gap tick");
        while (Os_Port_CompleteConfiguredDispatch() == E_OK) {
            assert_dispatch_invariants("post-gap drain");
        }
    }

    TEST_ASSERT_TRUE(runs_fast > 1u);
    TEST_ASSERT_TRUE(runs_mid > 0u);
    TEST_ASSERT_TRUE(runs_slow > 0u);
}

/**
 * @requirement The FIX-10 invariant shall hold under arbitrary interleavings
 *              of SysTick and PendSV completion, including bursts of ticks
 *              against a delayed PendSV (coalescing) — machine-checked, not
 *              hand-traced (memo 8.4 showed hand-tracing is insufficient).
 * @verify 8 fixed seeds x 20000 randomized steps of {tick, complete-one,
 *         tick-burst-then-complete-all}; the invariant, kernel/port
 *         agreement, fail-closed silence and ErrorHook silence are asserted
 *         at every step; liveness is asserted per seed.
 */
void test_randomized_tick_terminate_interleaving_fuzz(void)
{
    uint32 seed_idx;
    uint32 step;
    uint32 burst;

    for (seed_idx = 0u; seed_idx < (sizeof(fuzz_seeds) / sizeof(fuzz_seeds[0])); seed_idx++) {
        srand(fuzz_seeds[seed_idx]);
        start_production_os();

        for (step = 0u; step < FUZZ_STEPS_PER_SEED; step++) {
            int action = rand() % 100;

            if (action < 60) {
                /* Hardware-faithful cadence: PendSV (and any switchback
                 * chain it triggers) drains before the next tick. */
                Os_Port_Stm32_SysTickHandler();
                assert_dispatch_invariants("fuzz: tick");
                while (Os_Port_CompleteConfiguredDispatch() == E_OK) {
                    assert_dispatch_invariants("fuzz: drain");
                }
            } else if (action < 85) {
                /* Coalescing burst: several ticks land while the pending
                 * PendSV is held off (the 7.2 gap), then everything drains. */
                for (burst = 0u; burst < (uint32)(1 + (rand() % 3)); burst++) {
                    Os_Port_Stm32_SysTickHandler();
                    assert_dispatch_invariants("fuzz: burst tick");
                }
                while (Os_Port_CompleteConfiguredDispatch() == E_OK) {
                    assert_dispatch_invariants("fuzz: burst drain");
                }
            } else {
                /* Partial drain: one PendSV lands, the rest of the chain is
                 * held into the next step (interrupted switchback chain). */
                Os_Port_Stm32_SysTickHandler();
                assert_dispatch_invariants("fuzz: tick");
                (void)Os_Port_CompleteConfiguredDispatch();
                assert_dispatch_invariants("fuzz: complete-one");
            }
        }

        /* Drain and prove liveness for this seed. */
        while (Os_Port_CompleteConfiguredDispatch() == E_OK) {
            assert_dispatch_invariants("fuzz: final drain");
        }
        TEST_ASSERT_TRUE(runs_fast > 0u);
        TEST_ASSERT_TRUE(runs_mid > 0u);
        TEST_ASSERT_TRUE(runs_slow > 0u);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tick_dispatch_does_not_push_before_port_save);
    RUN_TEST(test_tick_in_switchback_gap_causes_no_double_advance);
    RUN_TEST(test_randomized_tick_terminate_interleaving_fuzz);
    return UNITY_END();
}
