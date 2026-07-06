/**
 * @file    Os_Demo_App.c
 * @brief   Multi-scenario OSEK bootstrap demo application
 * @date    2026-07-06
 *
 * @details The same task graph is compiled for host, STM32-port, STM32L5-port,
 *          and TMS570-port simulations. It exercises task preemption/FIFO,
 *          non-preemptive dispatch, priority ceiling resources, events,
 *          cyclic alarms, and Cat2 ISR activation.
 */
#include "Os_Demo_App.h"

#include <string.h>

#include "Det.h"
#include "Os_Port.h"

#define DEMO_TASK_LOW       ((TaskType)0u)
#define DEMO_TASK_HIGH      ((TaskType)1u)
#define DEMO_TASK_PEER_A    ((TaskType)2u)
#define DEMO_TASK_PEER_B    ((TaskType)3u)
#define DEMO_TASK_NON_LOW   ((TaskType)4u)
#define DEMO_TASK_EVENT     ((TaskType)5u)
#define DEMO_TASK_SIGNAL    ((TaskType)6u)
#define DEMO_TASK_PERIODIC  ((TaskType)7u)

#define DEMO_RES_SHARED     ((ResourceType)0u)
#define DEMO_ALARM_PERIODIC ((AlarmType)0u)
#define DEMO_EVENT_WAKE     ((EventMaskType)0x01u)

#define DEMO_TRACE_CAPACITY 160u

static char os_demo_trace[DEMO_TRACE_CAPACITY];
static const char* os_demo_failure;
static uint8 os_demo_scenario_count;
static boolean os_demo_passed;
static uint8 os_demo_periodic_runs;
static uint8 os_demo_event_phase;
static StatusType os_demo_status;
static TaskStateType os_demo_observed_state;

static void os_demo_append_char(char Marker)
{
    uint8 idx = 0u;

    while ((os_demo_trace[idx] != '\0') && (idx < (DEMO_TRACE_CAPACITY - 1u))) {
        idx++;
    }

    if (idx < (DEMO_TRACE_CAPACITY - 1u)) {
        os_demo_trace[idx] = Marker;
        os_demo_trace[idx + 1u] = '\0';
    }
}

static void os_demo_append_text(const char* Text)
{
    while ((Text != (const char*)0) && (*Text != '\0')) {
        os_demo_append_char(*Text);
        Text++;
    }
}

static boolean os_demo_expect_status(StatusType Actual, StatusType Expected, const char* Failure)
{
    if (Actual != Expected) {
        os_demo_failure = Failure;
        os_demo_passed = FALSE;
        return FALSE;
    }

    return TRUE;
}

static boolean os_demo_expect_trace_suffix(const char* Expected, const char* Failure)
{
    const uint8 actual_len = (uint8)strlen(os_demo_trace);
    const uint8 expected_len = (uint8)strlen(Expected);
    const char* suffix;

    if (expected_len > actual_len) {
        os_demo_failure = Failure;
        os_demo_passed = FALSE;
        return FALSE;
    }

    suffix = &os_demo_trace[actual_len - expected_len];
    if (strcmp(suffix, Expected) != 0) {
        os_demo_failure = Failure;
        os_demo_passed = FALSE;
        return FALSE;
    }

    return TRUE;
}

static void task_high(void)
{
    os_demo_append_char('H');
    os_demo_status = TerminateTask();
}

static void task_peer_a(void)
{
    os_demo_append_char('A');
    os_demo_status = TerminateTask();
}

static void task_peer_b(void)
{
    os_demo_append_char('B');
    os_demo_status = TerminateTask();
}

static void task_low(void)
{
    os_demo_append_char('L');
    (void)ActivateTask(DEMO_TASK_PEER_A);
    (void)ActivateTask(DEMO_TASK_PEER_B);
    (void)ActivateTask(DEMO_TASK_HIGH);
    os_demo_append_char('l');
    os_demo_status = TerminateTask();
}

static void task_non_low(void)
{
    os_demo_append_char('N');
    (void)ActivateTask(DEMO_TASK_HIGH);
    (void)GetTaskState(DEMO_TASK_HIGH, &os_demo_observed_state);
    os_demo_append_char('n');
    os_demo_status = TerminateTask();
}

static void task_resource_low(void)
{
    os_demo_append_char('R');
    (void)GetResource(DEMO_RES_SHARED);
    (void)ActivateTask(DEMO_TASK_HIGH);
    (void)ReleaseResource(DEMO_RES_SHARED);
    os_demo_append_char('r');
    os_demo_status = TerminateTask();
}

static void task_event_waiter(void)
{
    if (os_demo_event_phase == 0u) {
        os_demo_append_char('W');
        os_demo_status = WaitEvent(DEMO_EVENT_WAKE);
        (void)GetTaskState(DEMO_TASK_EVENT, &os_demo_observed_state);
        if ((os_demo_status == E_OK) && (os_demo_observed_state == WAITING)) {
            os_demo_event_phase = 1u;
            return;
        }
    }

    if (os_demo_status == E_OK) {
        (void)ClearEvent(DEMO_EVENT_WAKE);
        os_demo_append_char('w');
        os_demo_status = TerminateTask();
    }
}

static void task_event_signal(void)
{
    os_demo_append_char('E');
    os_demo_status = SetEvent(DEMO_TASK_EVENT, DEMO_EVENT_WAKE);
    if (os_demo_status == E_OK) {
        os_demo_status = TerminateTask();
    }
}

static void task_periodic(void)
{
    os_demo_periodic_runs++;
    os_demo_append_char('P');
    os_demo_status = TerminateTask();
}

static void isr_activate_high(void)
{
    os_demo_append_char('I');
    os_demo_status = ActivateTask(DEMO_TASK_HIGH);
}

static const Os_TaskConfigType os_demo_task_cfg[] = {
    { "Low", task_low, 3u, 1u, 0u, FALSE, FULL },
    { "High", task_high, 1u, 4u, 0u, FALSE, FULL },
    { "PeerA", task_peer_a, 3u, 1u, 0u, FALSE, FULL },
    { "PeerB", task_peer_b, 3u, 1u, 0u, FALSE, FULL },
    { "NonLow", task_non_low, 4u, 1u, 0u, FALSE, NON },
    { "EventWaiter", task_event_waiter, 2u, 1u, 0u, TRUE, FULL },
    { "EventSignal", task_event_signal, 3u, 1u, 0u, FALSE, FULL },
    { "Periodic", task_periodic, 2u, 3u, 0u, FALSE, FULL }
};

static const Os_TaskConfigType os_demo_resource_task_cfg[] = {
    { "ResLow", task_resource_low, 3u, 1u, 0u, FALSE, FULL },
    { "High", task_high, 1u, 1u, 0u, FALSE, FULL }
};

static const Os_ResourceConfigType os_demo_resource_cfg[] = {
    { "Shared", 1u }
};

static const Os_AlarmConfigType os_demo_alarm_cfg[] = {
    { "PeriodicAlarm", DEMO_TASK_PERIODIC, 20u, 1u, 1u }
};

static void os_demo_reset_runtime(void)
{
    Det_Init();
#if defined(UNIT_TEST)
    Os_TestReset();
#if defined(PLATFORM_STM32) || defined(PLATFORM_STM32L5) || defined(PLATFORM_TMS570)
    Os_PortTargetInit();
#endif
#else
    Os_Init();
#endif
    os_demo_status = E_OK;
    os_demo_observed_state = SUSPENDED;
    os_demo_event_phase = 0u;
}

void Os_Demo_Reset(void)
{
    os_demo_trace[0] = '\0';
    os_demo_failure = "not run";
    os_demo_scenario_count = 0u;
    os_demo_passed = TRUE;
    os_demo_periodic_runs = 0u;
    os_demo_status = E_OK;
    os_demo_observed_state = SUSPENDED;
    os_demo_event_phase = 0u;
}

const Os_TaskConfigType* Os_Demo_GetTaskConfig(uint8* Count)
{
    if (Count != (uint8*)0) {
        *Count = (uint8)(sizeof(os_demo_task_cfg) / sizeof(os_demo_task_cfg[0]));
    }

    return os_demo_task_cfg;
}

const Os_ResourceConfigType* Os_Demo_GetResourceConfig(uint8* Count)
{
    if (Count != (uint8*)0) {
        *Count = (uint8)(sizeof(os_demo_resource_cfg) / sizeof(os_demo_resource_cfg[0]));
    }

    return os_demo_resource_cfg;
}

const Os_AlarmConfigType* Os_Demo_GetAlarmConfig(uint8* Count)
{
    if (Count != (uint8*)0) {
        *Count = (uint8)(sizeof(os_demo_alarm_cfg) / sizeof(os_demo_alarm_cfg[0]));
    }

    return os_demo_alarm_cfg;
}

static boolean os_demo_run_preemption_fifo(void)
{
#if defined(UNIT_TEST)
    os_demo_reset_runtime();
    os_demo_append_text("PRE:");
    if (os_demo_expect_status(Os_TestConfigureTasks(os_demo_task_cfg, 8u), E_OK, "configure preemption") == FALSE) {
        return FALSE;
    }
    StartOS(OSDEFAULTAPPMODE);
    if (os_demo_expect_status(ActivateTask(DEMO_TASK_LOW), E_OK, "activate low") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_status(Os_TestRunToIdle(), E_OK, "run preemption") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_trace_suffix("LHlAB", "trace preemption") == FALSE) {
        return FALSE;
    }
    os_demo_append_char(';');
    os_demo_scenario_count++;
    return TRUE;
#else
    os_demo_failure = "UNIT_TEST host driver required";
    os_demo_passed = FALSE;
    return FALSE;
#endif
}

static boolean os_demo_run_non_preemptive(void)
{
#if defined(UNIT_TEST)
    os_demo_reset_runtime();
    os_demo_append_text("NON:");
    if (os_demo_expect_status(Os_TestConfigureTasks(os_demo_task_cfg, 8u), E_OK, "configure non") == FALSE) {
        return FALSE;
    }
    StartOS(OSDEFAULTAPPMODE);
    if (os_demo_expect_status(ActivateTask(DEMO_TASK_NON_LOW), E_OK, "activate non") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_status(Os_TestRunToIdle(), E_OK, "run non") == FALSE) {
        return FALSE;
    }
    if ((os_demo_expect_trace_suffix("NnH", "trace non") == FALSE) ||
        (os_demo_observed_state != READY)) {
        os_demo_failure = "state non";
        os_demo_passed = FALSE;
        return FALSE;
    }
    os_demo_append_char(';');
    os_demo_scenario_count++;
    return TRUE;
#else
    os_demo_failure = "UNIT_TEST host driver required";
    os_demo_passed = FALSE;
    return FALSE;
#endif
}

static boolean os_demo_run_resource_ceiling(void)
{
#if defined(UNIT_TEST)
    os_demo_reset_runtime();
    os_demo_append_text("RES:");
    if (os_demo_expect_status(Os_TestConfigureTasks(os_demo_resource_task_cfg, 2u), E_OK, "configure resource tasks") ==
        FALSE) {
        return FALSE;
    }
    if (os_demo_expect_status(Os_TestConfigureResources(os_demo_resource_cfg, 1u), E_OK, "configure resource") ==
        FALSE) {
        return FALSE;
    }
    StartOS(OSDEFAULTAPPMODE);
    if (os_demo_expect_status(ActivateTask(DEMO_TASK_LOW), E_OK, "activate resource") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_status(Os_TestRunToIdle(), E_OK, "run resource") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_trace_suffix("RHr", "trace resource") == FALSE) {
        return FALSE;
    }
    os_demo_append_char(';');
    os_demo_scenario_count++;
    return TRUE;
#else
    os_demo_failure = "UNIT_TEST host driver required";
    os_demo_passed = FALSE;
    return FALSE;
#endif
}

static boolean os_demo_run_events(void)
{
#if defined(UNIT_TEST)
    os_demo_reset_runtime();
    os_demo_append_text("EVT:");
    if (os_demo_expect_status(Os_TestConfigureTasks(os_demo_task_cfg, 8u), E_OK, "configure events") == FALSE) {
        return FALSE;
    }
    StartOS(OSDEFAULTAPPMODE);
    if (os_demo_expect_status(ActivateTask(DEMO_TASK_EVENT), E_OK, "activate waiter") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_status(Os_TestRunToIdle(), E_OK, "run waiter to wait") == FALSE) {
        return FALSE;
    }
    if ((os_demo_expect_trace_suffix("W", "trace event wait") == FALSE) ||
        (os_demo_observed_state != WAITING)) {
        os_demo_failure = "state event wait";
        os_demo_passed = FALSE;
        return FALSE;
    }
    if (os_demo_expect_status(ActivateTask(DEMO_TASK_SIGNAL), E_OK, "activate signal") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_status(Os_TestRunToIdle(), E_OK, "run events") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_trace_suffix("WEw", "trace events") == FALSE) {
        return FALSE;
    }
    os_demo_append_char(';');
    os_demo_scenario_count++;
    return TRUE;
#else
    os_demo_failure = "UNIT_TEST host driver required";
    os_demo_passed = FALSE;
    return FALSE;
#endif
}

static boolean os_demo_run_alarm(void)
{
#if defined(UNIT_TEST)
    os_demo_reset_runtime();
    os_demo_append_text("ALM:");
    if (os_demo_expect_status(Os_TestConfigureTasks(os_demo_task_cfg, 8u), E_OK, "configure alarm tasks") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_status(Os_TestConfigureAlarms(os_demo_alarm_cfg, 1u), E_OK, "configure alarm") == FALSE) {
        return FALSE;
    }
    StartOS(OSDEFAULTAPPMODE);
    if (os_demo_expect_status(SetRelAlarm(DEMO_ALARM_PERIODIC, 1u, 2u), E_OK, "set alarm") == FALSE) {
        return FALSE;
    }
    Os_TestAdvanceCounter(1u);
    Os_TestAdvanceCounter(2u);
    if (os_demo_expect_status(CancelAlarm(DEMO_ALARM_PERIODIC), E_OK, "cancel alarm") == FALSE) {
        return FALSE;
    }
    Os_TestAdvanceCounter(4u);
    if ((os_demo_periodic_runs != 2u) || (os_demo_expect_trace_suffix("PP", "trace alarm") == FALSE)) {
        os_demo_failure = "count alarm";
        os_demo_passed = FALSE;
        return FALSE;
    }
    os_demo_append_char(';');
    os_demo_scenario_count++;
    return TRUE;
#else
    os_demo_failure = "UNIT_TEST host driver required";
    os_demo_passed = FALSE;
    return FALSE;
#endif
}

static boolean os_demo_run_isr2(void)
{
#if defined(UNIT_TEST)
    os_demo_reset_runtime();
    os_demo_append_text("ISR:");
    if (os_demo_expect_status(Os_TestConfigureTasks(os_demo_task_cfg, 8u), E_OK, "configure isr") == FALSE) {
        return FALSE;
    }
    StartOS(OSDEFAULTAPPMODE);
    if (os_demo_expect_status(Os_TestInvokeIsrCat2(isr_activate_high), E_OK, "invoke isr") == FALSE) {
        return FALSE;
    }
    if (os_demo_expect_trace_suffix("IH", "trace isr") == FALSE) {
        return FALSE;
    }
    os_demo_append_char(';');
    os_demo_scenario_count++;
    return TRUE;
#else
    os_demo_failure = "UNIT_TEST host driver required";
    os_demo_passed = FALSE;
    return FALSE;
#endif
}

StatusType Os_Demo_RunAll(void)
{
    Os_Demo_Reset();

    if ((os_demo_run_preemption_fifo() == FALSE) ||
        (os_demo_run_non_preemptive() == FALSE) ||
        (os_demo_run_resource_ceiling() == FALSE) ||
        (os_demo_run_events() == FALSE) ||
        (os_demo_run_alarm() == FALSE) ||
        (os_demo_run_isr2() == FALSE)) {
        return E_OS_STATE;
    }

    os_demo_failure = "none";
    return E_OK;
}

const Os_Demo_ResultType* Os_Demo_GetResult(void)
{
    static Os_Demo_ResultType result;

    result.Passed = os_demo_passed;
    result.Trace = os_demo_trace;
    result.Failure = os_demo_failure;
    result.ScenarioCount = os_demo_scenario_count;
    return &result;
}
