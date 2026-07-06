/**
 * @file    test_Os_Conformance_Sched.c
 * @brief   OSEK conformance slice 1: scheduling & task management scenarios
 * @date    2026-07-06
 *
 * @details Scenario-level conformance tests for the OSEK bootstrap kernel
 *          (plan: docs/plans/plan-os-test-suite.md, step S-OSTS-02).
 *          Each test cites the OSEK OS 2.2.3 clause it verifies. Unlike the
 *          per-service assertions in test_Os_asild.c, these tests exercise
 *          cross-service interactions: nested preemption chains, FIFO
 *          ordering under queued activations, dispatch boundaries of
 *          non-preemptive tasks, alarm-driven periodic activation, and
 *          ISR2 dispatch deferral.
 */
#include "unity.h"

#include "Det.h"
#include "Os.h"

#define TASK_0              0u
#define TASK_1              1u
#define TASK_2              2u
#define TASK_INVALID        ((TaskType)0xEEu)
#define EVENT_ALPHA         ((EventMaskType)0x01u)
#define ALARM_PERIODIC      0u

static char execution_log[32];
static uint8 task_a_runs;
static uint8 task_b_runs;
static uint8 task_c_runs;
static uint8 chain_low_runs;
static uint8 chain_mid_runs;
static uint8 chain_high_runs;
static uint8 alarm_target_runs;
static uint8 event_life_runs;
static uint8 event_life_phase;
static uint8 isr_high_runs_inside_isr;
static StatusType status_a;
static StatusType status_b;
static StatusType status_c;
static StatusType probe_invalid_state_status;
static StatusType probe_null_state_status;
static StatusType probe_get_id_status;
static StatusType isr_activate_status;
static StatusType schedule_status;
static StatusType event_life_wait_status;
static StatusType event_life_get_status;
static TaskType probe_own_id;
static TaskStateType observed_preempted_state;
static TaskStateType observed_running_state;
static TaskStateType observed_suspended_state;
static TaskStateType observed_deferred_high_state;
static EventMaskType event_mask_second_life;

void setUp(void)
{
    Det_Init();
    Os_TestReset();
    execution_log[0] = '\0';
    task_a_runs = 0u;
    task_b_runs = 0u;
    task_c_runs = 0u;
    chain_low_runs = 0u;
    chain_mid_runs = 0u;
    chain_high_runs = 0u;
    alarm_target_runs = 0u;
    event_life_runs = 0u;
    event_life_phase = 0u;
    isr_high_runs_inside_isr = 0xFFu;
    status_a = E_OK;
    status_b = E_OK;
    status_c = E_OK;
    probe_invalid_state_status = E_OK;
    probe_null_state_status = E_OK;
    probe_get_id_status = E_OK;
    isr_activate_status = E_OK;
    schedule_status = E_OK;
    event_life_wait_status = E_OK;
    event_life_get_status = E_OK;
    probe_own_id = INVALID_TASK;
    observed_preempted_state = SUSPENDED;
    observed_running_state = SUSPENDED;
    observed_suspended_state = RUNNING;
    observed_deferred_high_state = SUSPENDED;
    event_mask_second_life = 0xFFu;
}

void tearDown(void)
{
}

static void append_log(char Marker)
{
    uint8 idx = 0u;

    while ((execution_log[idx] != '\0') && (idx < (sizeof(execution_log) - 1u))) {
        idx++;
    }

    if (idx < (sizeof(execution_log) - 1u)) {
        execution_log[idx] = Marker;
        execution_log[idx + 1u] = '\0';
    }
}

/* ==================================================================
 * Task entries
 * ================================================================== */

static void task_marker_a(void)
{
    task_a_runs++;
    append_log('A');
    status_a = TerminateTask();
}

static void task_marker_b(void)
{
    task_b_runs++;
    append_log('B');
    status_b = TerminateTask();
}

static void task_marker_c(void)
{
    task_c_runs++;
    append_log('C');
    status_c = TerminateTask();
}

/* 3-level nested preemption: TASK_0 (low) -> TASK_1 (mid) -> TASK_2 (high) */
static void chain_low(void)
{
    chain_low_runs++;
    append_log('L');
    (void)ActivateTask(TASK_1);
    append_log('l');
    status_a = TerminateTask();
}

static void chain_mid(void)
{
    chain_mid_runs++;
    append_log('M');
    (void)ActivateTask(TASK_2);
    append_log('m');
    status_b = TerminateTask();
}

static void chain_high(void)
{
    chain_high_runs++;
    append_log('H');
    status_c = TerminateTask();
}

/* Resume-before-peer: TASK_0 activates equal-prio TASK_1, then high TASK_2 */
static void resume_low(void)
{
    append_log('R');
    (void)ActivateTask(TASK_1);   /* equal priority peer: no preemption   */
    (void)ActivateTask(TASK_2);   /* higher priority: preempts            */
    append_log('r');
    status_a = TerminateTask();
}

/* Non-preemptive dispatch boundary: NON TASK_0 activates high TASK_1 */
static void non_preemptive_body(void)
{
    append_log('N');
    (void)ActivateTask(TASK_1);
    (void)GetTaskState(TASK_1, &observed_deferred_high_state);
    append_log('n');
    status_a = TerminateTask();
}

/* State matrix: TASK_0 (low) activates TASK_1 (high); high observes states */
static void matrix_low(void)
{
    append_log('L');
    (void)ActivateTask(TASK_1);
    append_log('l');
    status_a = TerminateTask();
}

static void matrix_high_observer(void)
{
    append_log('H');
    (void)GetTaskState(TASK_0, &observed_preempted_state);
    (void)GetTaskState(TASK_1, &observed_running_state);
    (void)GetTaskState(TASK_2, &observed_suspended_state);
    status_b = TerminateTask();
}

/* Schedule() no-op path: FULL TASK_0 activates lower-prio TASK_1 */
static void schedule_noop_body(void)
{
    append_log('S');
    (void)ActivateTask(TASK_1);
    schedule_status = Schedule();
    append_log('s');
    status_a = TerminateTask();
}

/* Error/identity probe running as a task */
static void probe_task(void)
{
    TaskStateType dummy_state;

    probe_invalid_state_status = GetTaskState(TASK_INVALID, &dummy_state);
    probe_null_state_status = GetTaskState(TASK_0, (TaskStateRefType)0);
    probe_get_id_status = GetTaskID(&probe_own_id);
    status_a = TerminateTask();
}

/* Alarm target */
static void alarm_target(void)
{
    alarm_target_runs++;
    append_log('T');
    status_a = TerminateTask();
}

/* Extended task used for event-clear-on-activation scenario.
 * Life 1: waits on EVENT_ALPHA (blocks), resumes, terminates WITHOUT
 * clearing the event. Life 2: reads its own pending mask. */
static void event_lifecycle_task(void)
{
    event_life_runs++;

    if (event_life_phase == 0u) {
        append_log('W');
        event_life_wait_status = WaitEvent(EVENT_ALPHA);

        if (event_life_wait_status == E_OK) {
            TaskStateType own_state = RUNNING;

            (void)GetTaskState(TASK_0, &own_state);
            if (own_state == WAITING) {
                event_life_phase = 1u;
                return;
            }
        }
    }

    if (event_life_phase <= 1u) {
        /* Life 1 unblocked: terminate with EVENT_ALPHA still set */
        append_log('w');
        event_life_phase = 2u;
        status_a = TerminateTask();
        return;
    }

    /* Life 2: events must have been cleared by the new activation */
    append_log('V');
    event_life_get_status = GetEvent(TASK_0, &event_mask_second_life);
    status_b = TerminateTask();
}

static void event_notifier(void)
{
    append_log('X');
    (void)SetEvent(TASK_0, EVENT_ALPHA);
    status_c = TerminateTask();
}

/* ISR2 handler: activates TASK_0 (high prio) and snapshots its run count */
static void isr_activate_high(void)
{
    isr_activate_status = ActivateTask(TASK_0);
    isr_high_runs_inside_isr = task_a_runs;
}

/* ==================================================================
 * Scheduling scenarios
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 §4.6 (full-preemptive scheduling)
 * @requirement Activation of a higher-priority task shall preempt the
 *              running task immediately; preempted tasks shall resume in
 *              reverse preemption order.
 * @verify A 3-level activation chain executes nested-preemption order
 *         L M H m l and each task runs exactly once.
 */
void test_Os_conformance_three_level_preemption_chain_unwinds_in_order(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", chain_low, 3u, 1u, 0u, FALSE, FULL },
        { "Mid", chain_mid, 2u, 1u, 0u, FALSE, FULL },
        { "High", chain_high, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 3u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL_STRING("LMHml", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, chain_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, chain_mid_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, chain_high_runs);
    TEST_ASSERT_EQUAL(INVALID_TASK, Os_TestGetCurrentTask());
}

/**
 * @spec OSEK OS 2.2.3 §4.6.1 (order of activation within one priority)
 * @requirement Tasks of equal priority shall be dispatched in the order of
 *              their activation (FIFO per priority).
 * @verify Three equal-priority tasks activated C, A, B run as C A B.
 */
void test_Os_conformance_equal_priority_tasks_dispatch_in_activation_order(void)
{
    const Os_TaskConfigType cfg[] = {
        { "TaskA", task_marker_a, 2u, 1u, 0u, FALSE, FULL },
        { "TaskB", task_marker_b, 2u, 1u, 0u, FALSE, FULL },
        { "TaskC", task_marker_c, 2u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 3u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_2));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_1));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL_STRING("CAB", execution_log);
}

/**
 * @spec OSEK OS 2.2.3 §4.6.1, §13.2.3.1 (multiple activation ordering)
 * @requirement Queued activations shall be processed in activation order,
 *              interleaved FIFO with other tasks of the same priority.
 * @verify Activations A, B, A of equal-priority tasks run as A B A.
 */
void test_Os_conformance_queued_activations_interleave_fifo(void)
{
    const Os_TaskConfigType cfg[] = {
        { "TaskA", task_marker_a, 2u, 2u, 0u, FALSE, FULL },
        { "TaskB", task_marker_b, 2u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_1));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL_STRING("ABA", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, task_a_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, task_b_runs);
}

/**
 * @spec OSEK OS 2.2.3 §4.6.2 (preempted task is oldest in its priority)
 * @requirement A preempted task shall resume before ready tasks of the same
 *              priority that were activated while it was running.
 * @verify Low activates an equal-priority peer, is then preempted by high;
 *         after high terminates, low resumes before the peer (R H r P).
 */
void test_Os_conformance_preempted_task_resumes_before_equal_priority_peer(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", resume_low, 2u, 1u, 0u, FALSE, FULL },
        { "Peer", task_marker_b, 2u, 1u, 0u, FALSE, FULL },
        { "High", chain_high, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 3u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL_STRING("RHrB", execution_log);
}

/**
 * @spec OSEK OS 2.2.3 §4.6.3 (non-preemptive scheduling)
 * @requirement A non-preemptive task shall not be preempted by a
 *              higher-priority activation before reaching a point of
 *              rescheduling (here: TerminateTask).
 * @verify The NON task runs to completion first (N n H) and the activated
 *         higher-priority task is READY while dispatch is deferred.
 */
void test_Os_conformance_non_preemptive_task_defers_dispatch_to_termination(void)
{
    const Os_TaskConfigType cfg[] = {
        { "NonPre", non_preemptive_body, 3u, 1u, 0u, FALSE, NON },
        { "High", chain_high, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL_STRING("NnH", execution_log);
    TEST_ASSERT_EQUAL(READY, observed_deferred_high_state);
    TEST_ASSERT_EQUAL_UINT8(1u, chain_high_runs);
}

/**
 * @spec OSEK OS 2.2.3 §4.2 (task state model), §13.2.3.5 (GetTaskState)
 * @requirement A preempted task is READY, the running task is RUNNING and
 *              a never-activated task is SUSPENDED.
 * @verify The preempting high task observes exactly those three states.
 */
void test_Os_conformance_task_state_matrix_during_preemption(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", matrix_low, 3u, 1u, 0u, FALSE, FULL },
        { "High", matrix_high_observer, 1u, 1u, 0u, FALSE, FULL },
        { "Idle", task_marker_c, 4u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 3u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL_STRING("LHl", execution_log);
    TEST_ASSERT_EQUAL(READY, observed_preempted_state);
    TEST_ASSERT_EQUAL(RUNNING, observed_running_state);
    TEST_ASSERT_EQUAL(SUSPENDED, observed_suspended_state);
}

/**
 * @spec OSEK OS 2.2.3 §13.2.3.4 (Schedule)
 * @requirement Schedule shall have no effect when no higher-priority task
 *              is ready; the caller continues and lower-priority ready
 *              tasks run only after the caller terminates.
 * @verify Schedule returns E_OK, the log stays S s B.
 */
void test_Os_conformance_schedule_is_noop_without_higher_priority_ready(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Caller", schedule_noop_body, 2u, 1u, 0u, FALSE, FULL },
        { "Lower", task_marker_b, 3u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL(E_OK, schedule_status);
    TEST_ASSERT_EQUAL_STRING("SsB", execution_log);
}

/* ==================================================================
 * Task management conformance
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 §13.2.3.1 (ActivateTask), §13.2.3.5 (GetTaskState),
 *       §13.2.3.6 (GetTaskID)
 * @requirement GetTaskState shall reject invalid task IDs with E_OS_ID and
 *              null out-parameters; GetTaskID shall return the caller's ID.
 * @verify Probe task observes E_OS_ID, E_OS_VALUE and its own task ID.
 */
void test_Os_conformance_task_query_error_matrix(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Probe", probe_task, 2u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL(E_OS_ID, probe_invalid_state_status);
    TEST_ASSERT_EQUAL(E_OS_VALUE, probe_null_state_status);
    TEST_ASSERT_EQUAL(E_OK, probe_get_id_status);
    TEST_ASSERT_EQUAL(TASK_0, probe_own_id);
}

/**
 * @spec OSEK OS 2.2.3 §13.2.3.1 (ActivateTask, E_OS_LIMIT)
 * @requirement Activations beyond the configured limit shall be rejected
 *              with E_OS_LIMIT and the task shall run exactly limit times.
 * @verify Third activation of a limit-2 task fails; the task runs twice.
 */
void test_Os_conformance_activation_limit_bounds_run_count(void)
{
    const Os_TaskConfigType cfg[] = {
        { "TaskA", task_marker_a, 2u, 2u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OS_LIMIT, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL_UINT8(2u, task_a_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetPendingActivations(TASK_0));
}

/**
 * @spec OSEK OS 2.2.3 §13.7.2.2 (StartOS), autostart tasks
 * @requirement Tasks with an autostart entry for the active application
 *              mode shall be activated during StartOS.
 * @verify The autostart task runs without any explicit ActivateTask (the
 *         host bootstrap dispatches ready tasks inside StartOS); the
 *         non-autostart task never runs and stays SUSPENDED.
 */
void test_Os_conformance_autostart_task_is_activated_by_startos(void)
{
    TaskStateType manual_state = RUNNING;
    const Os_TaskConfigType cfg[] = {
        { "Auto", task_marker_a, 2u, 1u, ((uint32)1u << OSDEFAULTAPPMODE), FALSE, FULL },
        { "Manual", task_marker_b, 3u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL_UINT8(1u, task_a_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, task_b_runs);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_1, &manual_state));
    TEST_ASSERT_EQUAL(SUSPENDED, manual_state);
    TEST_ASSERT_EQUAL_STRING("A", execution_log);
}

/**
 * @spec OSEK OS 2.2.3 §7.3 (events at activation)
 * @requirement All events of an extended task shall be cleared when the
 *              task enters the READY state from SUSPENDED.
 * @verify EVENT_ALPHA set in the task's first life (and never cleared) is
 *         not pending in its second life.
 */
void test_Os_conformance_activation_clears_events_of_extended_task(void)
{
    const Os_TaskConfigType cfg[] = {
        { "EventTask", event_lifecycle_task, 2u, 1u, 0u, TRUE, FULL },
        { "Unused", task_marker_b, 4u, 1u, 0u, FALSE, FULL },
        { "Notifier", event_notifier, 3u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 3u));
    StartOS(OSDEFAULTAPPMODE);

    /* Life 1: wait, get unblocked by the notifier, terminate w/o clearing */
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_2));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("WXw", execution_log);

    /* Life 2: pending events must be gone */
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_0));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());

    TEST_ASSERT_EQUAL(E_OK, event_life_get_status);
    TEST_ASSERT_EQUAL_UINT32(0u, event_mask_second_life);
    TEST_ASSERT_EQUAL_STRING("WXwV", execution_log);
}

/* ==================================================================
 * Timer- and ISR-driven scenarios
 * ================================================================== */

/**
 * @spec OSEK OS 2.2.3 §9.2 (SetRelAlarm cyclic), §13.6.3.3
 * @requirement A cyclic relative alarm shall activate its task at the
 *              initial offset and then once per cycle; CancelAlarm shall
 *              stop further activations.
 * @verify Offset 5 / cycle 10 activates at ticks 5, 15, 25; no activation
 *         after cancel.
 */
void test_Os_conformance_cyclic_alarm_drives_periodic_activation(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Target", alarm_target, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "Periodic", TASK_0, 1000u, 1u, 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_PERIODIC, 5u, 10u));

    Os_TestAdvanceCounter(4u);
    TEST_ASSERT_EQUAL_UINT8(0u, alarm_target_runs);

    Os_TestAdvanceCounter(1u);
    TEST_ASSERT_EQUAL_UINT8(1u, alarm_target_runs);

    Os_TestAdvanceCounter(10u);
    TEST_ASSERT_EQUAL_UINT8(2u, alarm_target_runs);

    Os_TestAdvanceCounter(10u);
    TEST_ASSERT_EQUAL_UINT8(3u, alarm_target_runs);

    TEST_ASSERT_EQUAL(E_OK, CancelAlarm(ALARM_PERIODIC));
    Os_TestAdvanceCounter(20u);
    TEST_ASSERT_EQUAL_UINT8(3u, alarm_target_runs);
}

/**
 * @spec OSEK OS 2.2.3 §9.2 (SetAbsAlarm one-shot), §13.6.3.4
 * @requirement A one-shot absolute alarm shall expire exactly when the
 *              counter reaches the start value and never again.
 * @verify Start value 3 fires at tick 3 only.
 */
void test_Os_conformance_one_shot_abs_alarm_fires_at_absolute_tick(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Target", alarm_target, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "OneShot", TASK_0, 1000u, 1u, 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL_UINT32(0u, Os_TestGetCounterValue());
    TEST_ASSERT_EQUAL(E_OK, SetAbsAlarm(ALARM_PERIODIC, 3u, 0u));

    Os_TestAdvanceCounter(2u);
    TEST_ASSERT_EQUAL_UINT8(0u, alarm_target_runs);

    Os_TestAdvanceCounter(1u);
    TEST_ASSERT_EQUAL_UINT8(1u, alarm_target_runs);

    Os_TestAdvanceCounter(10u);
    TEST_ASSERT_EQUAL_UINT8(1u, alarm_target_runs);
}

/**
 * @spec OSEK OS 2.2.3 §6.2 (ISR2 rescheduling at interrupt exit)
 * @requirement Task activation from a Cat2 ISR shall not dispatch inside
 *              the ISR; rescheduling happens when the ISR completes.
 * @verify Run count is still zero inside the handler and one immediately
 *         after the simulated ISR exit, without any explicit dispatch.
 */
void test_Os_conformance_isr2_activation_dispatches_only_after_isr_exit(void)
{
    const Os_TaskConfigType cfg[] = {
        { "TaskA", task_marker_a, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, Os_TestInvokeIsrCat2(isr_activate_high));
    TEST_ASSERT_EQUAL(E_OK, isr_activate_status);
    TEST_ASSERT_EQUAL_UINT8(0u, isr_high_runs_inside_isr);
    TEST_ASSERT_EQUAL_UINT8(1u, task_a_runs);
}

/* ==================================================================
 * Runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Os_conformance_three_level_preemption_chain_unwinds_in_order);
    RUN_TEST(test_Os_conformance_equal_priority_tasks_dispatch_in_activation_order);
    RUN_TEST(test_Os_conformance_queued_activations_interleave_fifo);
    RUN_TEST(test_Os_conformance_preempted_task_resumes_before_equal_priority_peer);
    RUN_TEST(test_Os_conformance_non_preemptive_task_defers_dispatch_to_termination);
    RUN_TEST(test_Os_conformance_task_state_matrix_during_preemption);
    RUN_TEST(test_Os_conformance_schedule_is_noop_without_higher_priority_ready);
    RUN_TEST(test_Os_conformance_task_query_error_matrix);
    RUN_TEST(test_Os_conformance_activation_limit_bounds_run_count);
    RUN_TEST(test_Os_conformance_autostart_task_is_activated_by_startos);
    RUN_TEST(test_Os_conformance_activation_clears_events_of_extended_task);
    RUN_TEST(test_Os_conformance_cyclic_alarm_drives_periodic_activation);
    RUN_TEST(test_Os_conformance_one_shot_abs_alarm_fires_at_absolute_tick);
    RUN_TEST(test_Os_conformance_isr2_activation_dispatches_only_after_isr_exit);
    return UNITY_END();
}
