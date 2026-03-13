/**
 * @file    test_Os_asild.c
 * @brief   Unit tests for OSEK-first OS bootstrap kernel
 * @date    2026-03-13
 *
 * @details This file is the first TDD slice under the repo's OSEK roadmap.
 *          The tests exercise bootstrap BCC1 behavior on host, with spec
 *          citations carried in each test block so the suite can evolve
 *          toward a clause-to-test traceability matrix.
 */
#include "unity.h"

#include "Det.h"
#include "Os.h"

#define TASK_LOW            0u
#define TASK_HIGH           1u
#define TASK_CHAIN          2u
#define TASK_PEER           3u
#define TASK_PROBE          4u
#define TASK_SCHEDULE       5u
#define RES_SHARED          0u
#define RES_OUTER           0u
#define RES_INNER           1u
#define IOC_SHARED          0u
#define EVENT_ALPHA         ((EventMaskType)0x01u)
#define ALARM_ACTIVATE      0u

#define APPMODE_DIAG        1u

static char execution_log[32];
static uint8 low_runs;
static uint8 high_runs;
static uint8 chain_runs;
static uint8 peer_runs;
static uint8 probe_runs;
static uint8 schedule_runs;
static StatusType low_status;
static StatusType high_status;
static StatusType chain_status;
static StatusType peer_status;
static StatusType probe_status;
static StatusType schedule_status;
static StatusType error_hook_status;
static uint8 error_hook_count;
static StatusType shutdown_hook_status;
static uint8 shutdown_hook_count;
static uint8 pre_task_hook_count;
static uint8 post_task_hook_count;
static TaskType observed_task_id;
static TaskStateType observed_task_state;
static uint8 low_preempt_runs;
static uint8 high_preempt_runs;
static uint8 isr_low_runs;
static uint8 isr_high_runs;
static uint8 isr_handler_runs;
static StatusType low_preempt_activate_status;
static StatusType low_preempt_term_status;
static StatusType high_preempt_term_status;
static StatusType isr_invoke_status;
static StatusType isr_activate_status;
static StatusType isr_low_term_status;
static StatusType isr_high_term_status;
static uint8 observed_isr_nesting;
static uint8 resource_low_runs;
static uint8 resource_high_runs;
static uint8 resource_order_runs;
static StatusType resource_get_status;
static StatusType resource_activate_status;
static StatusType resource_release_status;
static StatusType resource_low_term_status;
static StatusType resource_high_term_status;
static StatusType resource_outer_get_status;
static StatusType resource_inner_get_status;
static StatusType resource_wrong_release_status;
static StatusType resource_inner_release_status;
static StatusType resource_outer_release_status;
static StatusType resource_order_term_status;
static uint8 resource_terminate_runs;
static uint8 resource_chain_runs;
static StatusType resource_terminate_get_status;
static StatusType resource_terminate_status;
static StatusType resource_terminate_release_status;
static StatusType resource_terminate_cleanup_status;
static StatusType resource_chain_get_status;
static StatusType resource_chain_status;
static StatusType resource_chain_release_status;
static StatusType resource_chain_cleanup_status;
static uint8 event_wait_runs;
static uint8 event_setter_runs;
static uint8 event_basic_runs;
static uint8 event_wait_phase;
static StatusType event_wait_status;
static StatusType event_set_status;
static StatusType event_get_status;
static StatusType event_clear_status;
static StatusType event_get_after_clear_status;
static StatusType event_wait_term_status;
static StatusType event_setter_term_status;
static StatusType event_basic_wait_status;
static StatusType event_basic_term_status;
static EventMaskType event_mask_before_clear;
static EventMaskType event_mask_after_clear;
static TaskStateType event_wait_state_after_block;
static uint8 event_isr_runner_runs;
static StatusType event_wait_resource_get_status;
static StatusType event_wait_resource_wait_status;
static StatusType event_wait_resource_release_status;
static StatusType event_wait_resource_term_status;
static StatusType event_isr_invoke_status;
static StatusType event_isr_set_status;
static StatusType event_isr_runner_term_status;
static uint8 alarm_target_runs;
static StatusType alarm_target_term_status;
static StatusType alarm_set_rel_status;
static StatusType alarm_set_abs_status;
static StatusType alarm_cancel_status;
static StatusType alarm_get_status;
static StatusType alarm_get_base_status;
static StatusType alarm_second_set_status;
static TickType alarm_remaining_ticks;
static TickType alarm_remaining_after_ticks;
static TickType alarm_remaining_before_wrap;
static AlarmBaseType observed_alarm_base;
static uint8 non_preempt_low_runs;
static uint8 non_preempt_high_runs;
static uint8 non_preempt_isr_runs;
static StatusType non_preempt_activate_status;
static StatusType non_preempt_low_term_status;
static StatusType non_preempt_high_term_status;
static StatusType non_preempt_isr_invoke_status;
static StatusType non_preempt_isr_activate_status;
static StatusType non_preempt_isr_low_term_status;
static uint8 non_schedule_runs;
static StatusType non_schedule_activate_status;
static StatusType non_schedule_status;
static StatusType non_schedule_term_status;
static StatusType non_schedule_resource_get_status;
static StatusType non_schedule_resource_status;
static StatusType non_schedule_resource_release_status;
static StatusType non_schedule_resource_term_status;
static uint8 multi_activation_runs;
static uint8 self_activation_runs;
static StatusType multi_first_activate_status;
static StatusType multi_second_activate_status;
static StatusType multi_term_status;
static StatusType self_activate_status;
static StatusType self_term_status;
static ApplicationType observed_application_id;
static ApplicationType observed_current_application_id;
static ApplicationType owned_task_application;
static ApplicationType owned_resource_application;
static ApplicationType owned_alarm_application;
static ObjectAccessType task_access_result;
static ObjectAccessType resource_access_result;
static ObjectAccessType alarm_access_result;
static uint8 trusted_function_call_count;
static uint8 trusted_param_value;
static StatusType trusted_function_status;
static StatusType denied_trusted_function_status;
static StatusType app_activate_denied_status;
static StatusType app_resource_denied_status;
static StatusType app_set_event_denied_status;
static StatusType app_set_alarm_denied_status;
static uint8 ioc_sender_runs;
static uint8 ioc_receiver_runs;
static uint8 ioc_denied_runs;
static StatusType ioc_send_first_status;
static StatusType ioc_send_second_status;
static StatusType ioc_send_overflow_status;
static StatusType ioc_receive_first_status;
static StatusType ioc_receive_second_status;
static StatusType ioc_receive_empty_status;
static StatusType ioc_denied_status;
static uint32 ioc_received_first_value;
static uint32 ioc_received_second_value;
static uint8 stack_safe_runs;
static uint8 stack_overflow_runs;
static StatusType stack_safe_get_id_status;
static StatusType stack_safe_term_status;
static StatusType stack_overflow_get_id_status;
static StatusType stack_overflow_term_status;
static uint16 stack_safe_peak;
static uint16 stack_overflow_peak;
static boolean stack_safe_violation;
static boolean stack_overflow_violation;
static uint8 stack_touch_sink;
static AccessType memory_access_owned_result;
static AccessType memory_access_foreign_result;
static AccessType memory_access_trusted_result;
static AccessType memory_access_invalid_result;
static uint8 memory_region_app_a[32];
static uint8 memory_region_app_b[32];

static void reset_observations(void)
{
    execution_log[0] = '\0';
    low_runs = 0u;
    high_runs = 0u;
    chain_runs = 0u;
    peer_runs = 0u;
    probe_runs = 0u;
    schedule_runs = 0u;
    low_status = E_OK;
    high_status = E_OK;
    chain_status = E_OK;
    peer_status = E_OK;
    probe_status = E_OK;
    schedule_status = E_OK;
    error_hook_status = E_OK;
    error_hook_count = 0u;
    shutdown_hook_status = E_OK;
    shutdown_hook_count = 0u;
    pre_task_hook_count = 0u;
    post_task_hook_count = 0u;
    observed_task_id = INVALID_TASK;
    observed_task_state = SUSPENDED;
    low_preempt_runs = 0u;
    high_preempt_runs = 0u;
    isr_low_runs = 0u;
    isr_high_runs = 0u;
    isr_handler_runs = 0u;
    low_preempt_activate_status = E_OK;
    low_preempt_term_status = E_OK;
    high_preempt_term_status = E_OK;
    isr_invoke_status = E_OK;
    isr_activate_status = E_OK;
    isr_low_term_status = E_OK;
    isr_high_term_status = E_OK;
    observed_isr_nesting = 0u;
    resource_low_runs = 0u;
    resource_high_runs = 0u;
    resource_order_runs = 0u;
    resource_get_status = E_OK;
    resource_activate_status = E_OK;
    resource_release_status = E_OK;
    resource_low_term_status = E_OK;
    resource_high_term_status = E_OK;
    resource_outer_get_status = E_OK;
    resource_inner_get_status = E_OK;
    resource_wrong_release_status = E_OK;
    resource_inner_release_status = E_OK;
    resource_outer_release_status = E_OK;
    resource_order_term_status = E_OK;
    resource_terminate_runs = 0u;
    resource_chain_runs = 0u;
    resource_terminate_get_status = E_OK;
    resource_terminate_status = E_OK;
    resource_terminate_release_status = E_OK;
    resource_terminate_cleanup_status = E_OK;
    resource_chain_get_status = E_OK;
    resource_chain_status = E_OK;
    resource_chain_release_status = E_OK;
    resource_chain_cleanup_status = E_OK;
    event_wait_runs = 0u;
    event_setter_runs = 0u;
    event_basic_runs = 0u;
    event_wait_phase = 0u;
    event_wait_status = E_OK;
    event_set_status = E_OK;
    event_get_status = E_OK;
    event_clear_status = E_OK;
    event_get_after_clear_status = E_OK;
    event_wait_term_status = E_OK;
    event_setter_term_status = E_OK;
    event_basic_wait_status = E_OK;
    event_basic_term_status = E_OK;
    event_mask_before_clear = 0u;
    event_mask_after_clear = 0u;
    event_wait_state_after_block = SUSPENDED;
    event_isr_runner_runs = 0u;
    event_wait_resource_get_status = E_OK;
    event_wait_resource_wait_status = E_OK;
    event_wait_resource_release_status = E_OK;
    event_wait_resource_term_status = E_OK;
    event_isr_invoke_status = E_OK;
    event_isr_set_status = E_OK;
    event_isr_runner_term_status = E_OK;
    alarm_target_runs = 0u;
    alarm_target_term_status = E_OK;
    alarm_set_rel_status = E_OK;
    alarm_set_abs_status = E_OK;
    alarm_cancel_status = E_OK;
    alarm_get_status = E_OK;
    alarm_get_base_status = E_OK;
    alarm_second_set_status = E_OK;
    alarm_remaining_ticks = 0u;
    alarm_remaining_after_ticks = 0u;
    alarm_remaining_before_wrap = 0u;
    observed_alarm_base.maxallowedvalue = 0u;
    observed_alarm_base.ticksperbase = 0u;
    observed_alarm_base.mincycle = 0u;
    non_preempt_low_runs = 0u;
    non_preempt_high_runs = 0u;
    non_preempt_isr_runs = 0u;
    non_preempt_activate_status = E_OK;
    non_preempt_low_term_status = E_OK;
    non_preempt_high_term_status = E_OK;
    non_preempt_isr_invoke_status = E_OK;
    non_preempt_isr_activate_status = E_OK;
    non_preempt_isr_low_term_status = E_OK;
    non_schedule_runs = 0u;
    non_schedule_activate_status = E_OK;
    non_schedule_status = E_OK;
    non_schedule_term_status = E_OK;
    non_schedule_resource_get_status = E_OK;
    non_schedule_resource_status = E_OK;
    non_schedule_resource_release_status = E_OK;
    non_schedule_resource_term_status = E_OK;
    multi_activation_runs = 0u;
    self_activation_runs = 0u;
    multi_first_activate_status = E_OK;
    multi_second_activate_status = E_OK;
    multi_term_status = E_OK;
    self_activate_status = E_OK;
    self_term_status = E_OK;
    observed_application_id = INVALID_OSAPPLICATION;
    observed_current_application_id = INVALID_OSAPPLICATION;
    owned_task_application = INVALID_OSAPPLICATION;
    owned_resource_application = INVALID_OSAPPLICATION;
    owned_alarm_application = INVALID_OSAPPLICATION;
    task_access_result = NO_ACCESS;
    resource_access_result = NO_ACCESS;
    alarm_access_result = NO_ACCESS;
    trusted_function_call_count = 0u;
    trusted_param_value = 0u;
    trusted_function_status = E_OK;
    denied_trusted_function_status = E_OK;
    app_activate_denied_status = E_OK;
    app_resource_denied_status = E_OK;
    app_set_event_denied_status = E_OK;
    app_set_alarm_denied_status = E_OK;
    ioc_sender_runs = 0u;
    ioc_receiver_runs = 0u;
    ioc_denied_runs = 0u;
    ioc_send_first_status = E_OK;
    ioc_send_second_status = E_OK;
    ioc_send_overflow_status = E_OK;
    ioc_receive_first_status = E_OK;
    ioc_receive_second_status = E_OK;
    ioc_receive_empty_status = E_OK;
    ioc_denied_status = E_OK;
    ioc_received_first_value = 0u;
    ioc_received_second_value = 0u;
    stack_safe_runs = 0u;
    stack_overflow_runs = 0u;
    stack_safe_get_id_status = E_OK;
    stack_safe_term_status = E_OK;
    stack_overflow_get_id_status = E_OK;
    stack_overflow_term_status = E_OK;
    stack_safe_peak = 0u;
    stack_overflow_peak = 0u;
    stack_safe_violation = FALSE;
    stack_overflow_violation = FALSE;
    stack_touch_sink = 0u;
    memory_access_owned_result = NO_ACCESS;
    memory_access_foreign_result = NO_ACCESS;
    memory_access_trusted_result = NO_ACCESS;
    memory_access_invalid_result = NO_ACCESS;
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

static void error_hook(StatusType Error)
{
    error_hook_status = Error;
    error_hook_count++;
}

static void startup_hook(void)
{
    append_log('S');
}

static void pre_task_hook(void)
{
    pre_task_hook_count++;
    append_log('>');
}

static void post_task_hook(void)
{
    post_task_hook_count++;
    append_log('<');
}

static void shutdown_hook(StatusType Error)
{
    shutdown_hook_status = Error;
    shutdown_hook_count++;
    append_log('D');
}

static void task_low(void)
{
    TaskType current_task = INVALID_TASK;

    low_runs++;
    append_log('L');
    (void)GetTaskID(&current_task);
    observed_task_id = current_task;
    low_status = TerminateTask();
}

static void task_high(void)
{
    high_runs++;
    append_log('H');
    high_status = TerminateTask();
}

static void task_chain(void)
{
    chain_runs++;
    append_log('C');
    chain_status = ChainTask(TASK_HIGH);
}

static void task_peer(void)
{
    peer_runs++;
    append_log('Q');
    peer_status = TerminateTask();
}

static void task_probe(void)
{
    TaskType current_task = INVALID_TASK;

    probe_runs++;
    append_log('P');
    (void)GetTaskID(&current_task);
    (void)GetTaskState(current_task, &observed_task_state);
    probe_status = TerminateTask();
}

static void task_schedule(void)
{
    schedule_runs++;
    append_log('Y');
    schedule_status = Schedule();
    (void)TerminateTask();
}

static void task_low_preempt(void)
{
    low_preempt_runs++;
    append_log('A');
    low_preempt_activate_status = ActivateTask(TASK_HIGH);
    append_log('a');
    low_preempt_term_status = TerminateTask();
}

static void task_high_preempt(void)
{
    high_preempt_runs++;
    append_log('B');
    high_preempt_term_status = TerminateTask();
}

static void isr_activate_high(void)
{
    isr_handler_runs++;
    append_log('I');
    observed_isr_nesting = Os_TestGetIsrCat2Nesting();
    isr_activate_status = ActivateTask(TASK_HIGH);
}

static void task_low_isr(void)
{
    isr_low_runs++;
    append_log('L');
    isr_invoke_status = Os_TestInvokeIsrCat2(isr_activate_high);
    append_log('l');
    isr_low_term_status = TerminateTask();
}

static void task_high_isr(void)
{
    isr_high_runs++;
    append_log('H');
    isr_high_term_status = TerminateTask();
}

static void task_low_resource(void)
{
    resource_low_runs++;
    append_log('R');
    resource_get_status = GetResource(RES_SHARED);
    resource_activate_status = ActivateTask(TASK_HIGH);
    append_log('r');
    resource_release_status = ReleaseResource(RES_SHARED);
    append_log('x');
    resource_low_term_status = TerminateTask();
}

static void task_high_resource(void)
{
    resource_high_runs++;
    append_log('H');
    resource_high_term_status = TerminateTask();
}

static void task_resource_release_order(void)
{
    resource_order_runs++;
    append_log('N');
    resource_outer_get_status = GetResource(RES_OUTER);
    resource_inner_get_status = GetResource(RES_INNER);
    resource_wrong_release_status = ReleaseResource(RES_OUTER);
    append_log('n');
    resource_inner_release_status = ReleaseResource(RES_INNER);
    resource_outer_release_status = ReleaseResource(RES_OUTER);
    resource_order_term_status = TerminateTask();
}

static void task_terminate_with_resource(void)
{
    resource_terminate_runs++;
    append_log('T');
    resource_terminate_get_status = GetResource(RES_SHARED);
    resource_terminate_status = TerminateTask();
    append_log('t');
    resource_terminate_release_status = ReleaseResource(RES_SHARED);
    resource_terminate_cleanup_status = TerminateTask();
}

static void task_chain_with_resource(void)
{
    resource_chain_runs++;
    append_log('C');
    resource_chain_get_status = GetResource(RES_SHARED);
    resource_chain_status = ChainTask(TASK_HIGH);
    append_log('c');
    resource_chain_release_status = ReleaseResource(RES_SHARED);
    resource_chain_cleanup_status = TerminateTask();
}

static void task_wait_event_extended(void)
{
    event_wait_runs++;

    if (event_wait_phase == 0u) {
        append_log('W');
        event_wait_status = WaitEvent(EVENT_ALPHA);
        (void)GetTaskState(TASK_LOW, &event_wait_state_after_block);

        if (event_wait_state_after_block == WAITING) {
            event_wait_phase = 1u;
            return;
        }
    }

    append_log('E');
    event_get_status = GetEvent(TASK_LOW, &event_mask_before_clear);
    event_clear_status = ClearEvent(EVENT_ALPHA);
    event_get_after_clear_status = GetEvent(TASK_LOW, &event_mask_after_clear);
    event_wait_phase = 2u;
    event_wait_term_status = TerminateTask();
}

static void task_set_event_notifier(void)
{
    event_setter_runs++;
    append_log('N');
    event_set_status = SetEvent(TASK_LOW, EVENT_ALPHA);
    append_log('n');
    event_setter_term_status = TerminateTask();
}

static void task_wait_event_basic(void)
{
    event_basic_runs++;
    append_log('B');
    event_basic_wait_status = WaitEvent(EVENT_ALPHA);
    event_basic_term_status = TerminateTask();
}

static void task_wait_event_with_resource(void)
{
    event_wait_runs++;
    append_log('R');
    event_wait_resource_get_status = GetResource(RES_SHARED);
    event_wait_resource_wait_status = WaitEvent(EVENT_ALPHA);
    append_log('r');
    event_wait_resource_release_status = ReleaseResource(RES_SHARED);
    event_wait_resource_term_status = TerminateTask();
}

static void isr_set_event_waiter(void)
{
    append_log('I');
    event_isr_set_status = SetEvent(TASK_LOW, EVENT_ALPHA);
}

static void task_isr_event_runner(void)
{
    event_isr_runner_runs++;
    append_log('L');
    event_isr_invoke_status = Os_TestInvokeIsrCat2(isr_set_event_waiter);
    append_log('l');
    event_isr_runner_term_status = TerminateTask();
}

static void task_alarm_target(void)
{
    alarm_target_runs++;
    append_log('A');
    alarm_target_term_status = TerminateTask();
}

static void task_low_non_preemptive(void)
{
    non_preempt_low_runs++;
    append_log('N');
    non_preempt_activate_status = ActivateTask(TASK_HIGH);
    append_log('n');
    non_preempt_low_term_status = TerminateTask();
}

static void task_high_non_preemptive(void)
{
    non_preempt_high_runs++;
    append_log('H');
    non_preempt_high_term_status = TerminateTask();
}

static void isr_activate_high_non_preemptive(void)
{
    append_log('I');
    non_preempt_isr_activate_status = ActivateTask(TASK_HIGH);
}

static void task_low_non_preemptive_isr(void)
{
    non_preempt_isr_runs++;
    append_log('L');
    non_preempt_isr_invoke_status = Os_TestInvokeIsrCat2(isr_activate_high_non_preemptive);
    append_log('l');
    non_preempt_isr_low_term_status = TerminateTask();
}

static void task_non_preemptive_schedule(void)
{
    non_schedule_runs++;
    append_log('S');
    non_schedule_activate_status = ActivateTask(TASK_HIGH);
    non_schedule_status = Schedule();
    append_log('s');
    non_schedule_term_status = TerminateTask();
}

static void task_non_preemptive_schedule_with_resource(void)
{
    non_schedule_runs++;
    append_log('R');
    non_schedule_resource_get_status = GetResource(RES_SHARED);
    non_schedule_resource_status = Schedule();
    append_log('r');
    non_schedule_resource_release_status = ReleaseResource(RES_SHARED);
    non_schedule_resource_term_status = TerminateTask();
}

static void task_multiple_activation_target(void)
{
    multi_activation_runs++;
    append_log('M');
    multi_term_status = TerminateTask();
}

static void task_self_activate_target(void)
{
    self_activation_runs++;

    if (self_activation_runs == 1u) {
        append_log('X');
        self_activate_status = ActivateTask(TASK_LOW);
    } else {
        append_log('Y');
    }

    self_term_status = TerminateTask();
}

static void task_application_probe(void)
{
    observed_application_id = GetApplicationID();
    observed_current_application_id = GetCurrentApplicationID();
    append_log('P');
    probe_status = TerminateTask();
}

static StatusType trusted_function_probe(TrustedFunctionParameterRefType Params)
{
    uint8* value_ptr = (uint8*)Params;

    trusted_function_call_count++;
    append_log('T');

    if (value_ptr != NULL_PTR) {
        trusted_param_value = *value_ptr;
    }

    return E_OK;
}

static void task_trusted_function_allowed(void)
{
    uint8 param = 7u;

    trusted_function_status = CallTrustedFunction((TrustedFunctionIndexType)0u, &param);
    append_log('A');
    probe_status = TerminateTask();
}

static void task_trusted_function_denied(void)
{
    uint8 param = 9u;

    denied_trusted_function_status = CallTrustedFunction((TrustedFunctionIndexType)0u, &param);
    append_log('D');
    probe_status = TerminateTask();
}

static void task_app_denied_activate(void)
{
    app_activate_denied_status = ActivateTask(TASK_HIGH);
    append_log('a');
    probe_status = TerminateTask();
}

static void task_app_denied_resource(void)
{
    app_resource_denied_status = GetResource(RES_SHARED);
    append_log('r');
    probe_status = TerminateTask();
}

static void task_app_denied_set_event(void)
{
    app_set_event_denied_status = SetEvent(TASK_LOW, EVENT_ALPHA);
    append_log('e');
    probe_status = TerminateTask();
}

static void task_app_denied_set_alarm(void)
{
    app_set_alarm_denied_status = SetRelAlarm(ALARM_ACTIVATE, 1u, 0u);
    append_log('l');
    probe_status = TerminateTask();
}

static void task_ioc_sender(void)
{
    ioc_sender_runs++;
    ioc_send_first_status = IocSend(IOC_SHARED, 0x11u);
    ioc_send_second_status = IocSend(IOC_SHARED, 0x22u);
    append_log('S');
    probe_status = TerminateTask();
}

static void task_ioc_receiver(void)
{
    ioc_receiver_runs++;
    ioc_receive_first_status = IocReceive(IOC_SHARED, &ioc_received_first_value);
    ioc_receive_second_status = IocReceive(IOC_SHARED, &ioc_received_second_value);
    append_log('R');
    probe_status = TerminateTask();
}

static void task_ioc_receiver_empty(void)
{
    ioc_receiver_runs++;
    ioc_receive_empty_status = IocReceive(IOC_SHARED, &ioc_received_first_value);
    append_log('N');
    probe_status = TerminateTask();
}

static void task_ioc_sender_overflow(void)
{
    ioc_sender_runs++;
    ioc_send_first_status = IocSend(IOC_SHARED, 0x33u);
    ioc_send_second_status = IocSend(IOC_SHARED, 0x44u);
    ioc_send_overflow_status = IocSend(IOC_SHARED, 0x55u);
    append_log('O');
    probe_status = TerminateTask();
}

static void task_ioc_denied_sender(void)
{
    ioc_denied_runs++;
    ioc_denied_status = IocSend(IOC_SHARED, 0x66u);
    append_log('X');
    probe_status = TerminateTask();
}

static void task_stack_safe(void)
{
    volatile uint8 stack_buffer[64];
    TaskType current_task = INVALID_TASK;

    stack_safe_runs++;
    stack_buffer[0] = 0x5Au;
    stack_buffer[63] = 0xA5u;
    stack_touch_sink = (uint8)(stack_buffer[0] ^ stack_buffer[63]);
    stack_safe_get_id_status = GetTaskID(&current_task);
    (void)current_task;
    append_log('K');
    stack_safe_term_status = TerminateTask();
}

static void task_stack_overflow(void)
{
    volatile uint8 stack_buffer[256];
    TaskType current_task = INVALID_TASK;

    stack_overflow_runs++;
    stack_buffer[0] = 0x11u;
    stack_buffer[255] = 0x22u;
    stack_touch_sink = (uint8)(stack_buffer[0] ^ stack_buffer[255]);
    append_log('V');
    stack_overflow_get_id_status = GetTaskID(&current_task);
    (void)current_task;
    stack_overflow_term_status = TerminateTask();
}

void setUp(void)
{
    Det_Init();
    Os_TestReset();
    reset_observations();
}

void tearDown(void)
{
}

void test_Os_test_run_to_idle_drains_ready_queue_in_host_bootstrap(void)
{
    const Os_TaskConfigType cfg[] = {
        { "LowPreempt", task_low_preempt, 3u, 1u, 0u, FALSE, FULL },
        { "HighPreempt", task_high_preempt, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("ABa", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, low_preempt_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, high_preempt_runs);
    TEST_ASSERT_EQUAL(E_OK, low_preempt_activate_status);
    TEST_ASSERT_EQUAL(E_OK, low_preempt_term_status);
    TEST_ASSERT_EQUAL(E_OK, high_preempt_term_status);
    TEST_ASSERT_EQUAL(INVALID_TASK, Os_TestGetCurrentTask());
}

void test_Os_test_complete_port_dispatches_is_nofunc_in_plain_host_build(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 3u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL_UINT8(1u, Os_TestGetPendingActivations(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_TestCompletePortDispatches());
    TEST_ASSERT_EQUAL_STRING("", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, Os_TestGetPendingActivations(TASK_LOW));

    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("L", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, low_runs);
}

void test_Os_test_run_to_idle_executes_chain_task_successor_sequence(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Chain", task_chain, 2u, 1u, 0u, FALSE, FULL },
        { "High", task_high, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("CH", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, chain_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, high_runs);
    TEST_ASSERT_EQUAL(E_OK, chain_status);
    TEST_ASSERT_EQUAL(E_OK, high_status);
    TEST_ASSERT_EQUAL(INVALID_TASK, Os_TestGetCurrentTask());
}

void test_Os_test_run_to_idle_processes_queued_multiple_activations_until_idle(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Multi", task_multiple_activation_target, 2u, 2u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL_UINT8(2u, Os_TestGetPendingActivations(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("MM", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, multi_activation_runs);
    TEST_ASSERT_EQUAL(E_OK, multi_term_status);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetPendingActivations(TASK_LOW));
    TEST_ASSERT_EQUAL(INVALID_TASK, Os_TestGetCurrentTask());
}

/**
 * @spec OSEK OS 2.2.3 §13.7
 * @requirement OS services shall not be used before the OS is started.
 * @verify ActivateTask returns E_OS_STATE and the error hook observes it.
 */
/**
 * @spec OSEK OS 2.2.3 §13.1
 * @requirement A helper that runs the bootstrap until quiescent shall report
 *              when no ready work or deferred dispatch remains.
 * @verify Os_TestRunToIdle returns E_OS_NOFUNC from an already idle state.
 */
void test_Os_test_run_to_idle_returns_nofunc_when_system_is_already_idle(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 3u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("", execution_log);
    TEST_ASSERT_EQUAL_UINT8(0u, low_runs);
    TEST_ASSERT_EQUAL(INVALID_TASK, Os_TestGetCurrentTask());
}

/**
 * @spec OSEK OS 2.2.3 §13.1, §13.3
 * @requirement The shared bootstrap run-to-idle helper shall cover the same
 *              Cat2 ISR exit dispatch behavior as the manual schedule path.
 * @verify A Cat2 ISR-triggered higher-priority activation completes through
 *         Os_TestRunToIdle and returns the system to idle.
 */
void test_Os_test_run_to_idle_processes_cat2_isr_preemption_sequence(void)
{
    const Os_TaskConfigType cfg[] = {
        { "LowIsr", task_low_isr, 3u, 1u, 0u, FALSE, FULL },
        { "HighIsr", task_high_isr, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("LIHl", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_high_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_handler_runs);
    TEST_ASSERT_EQUAL(E_OK, isr_invoke_status);
    TEST_ASSERT_EQUAL(E_OK, isr_activate_status);
    TEST_ASSERT_EQUAL(E_OK, isr_low_term_status);
    TEST_ASSERT_EQUAL(E_OK, isr_high_term_status);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetIsrCat2Nesting());
    TEST_ASSERT_EQUAL(INVALID_TASK, Os_TestGetCurrentTask());
}

/**
 * @spec OSEK OS 2.2.3 §13.5
 * @requirement The shared bootstrap run-to-idle helper shall handle an
 *              extended task blocking and then resuming after SetEvent.
 * @verify The waiter reaches WAITING after the first run-to-idle call and
 *         completes after notifier activation and the second call.
 */
void test_Os_test_run_to_idle_resumes_waiting_task_after_notifier_activation(void)
{
    const Os_TaskConfigType cfg[] = {
        { "WaitExt", task_wait_event_extended, 2u, 1u, 0u, TRUE, FULL },
        { "Notifier", task_set_event_notifier, 3u, 1u, 0u, FALSE, FULL }
    };
    TaskStateType wait_state = RUNNING;

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("W", execution_log);
    TEST_ASSERT_EQUAL(WAITING, event_wait_state_after_block);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_LOW, &wait_state));
    TEST_ASSERT_EQUAL(WAITING, wait_state);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_HIGH));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("WNEn", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, event_wait_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, event_setter_runs);
    TEST_ASSERT_EQUAL(E_OK, event_set_status);
    TEST_ASSERT_EQUAL(E_OK, event_get_status);
    TEST_ASSERT_EQUAL(EVENT_ALPHA, event_mask_before_clear);
    TEST_ASSERT_EQUAL(E_OK, event_clear_status);
    TEST_ASSERT_EQUAL(E_OK, event_get_after_clear_status);
    TEST_ASSERT_EQUAL(0u, event_mask_after_clear);
    TEST_ASSERT_EQUAL(E_OK, event_wait_term_status);
    TEST_ASSERT_EQUAL(E_OK, event_setter_term_status);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_LOW, &wait_state));
    TEST_ASSERT_EQUAL(SUSPENDED, wait_state);
    TEST_ASSERT_EQUAL(INVALID_TASK, Os_TestGetCurrentTask());
}

/**
 * @spec AUTOSAR OS §8
 * @requirement The shared bootstrap run-to-idle helper shall drain multiple
 *              ready IOC participants through one quiescence pass.
 * @verify Sender and receiver tasks both complete and preserve FIFO data
 *         ordering when activated before Os_TestRunToIdle.
 */
void test_Os_test_run_to_idle_drains_sender_and_receiver_ioc_sequence(void)
{
    const Os_TaskConfigType cfg[] = {
        { "IocSender", task_ioc_sender, 2u, 1u, 0u, FALSE, FULL },
        { "IocReceiver", task_ioc_receiver, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, (1u << IOC_SHARED), (1u << IOC_SHARED) },
        { "AppB", FALSE, (1u << TASK_HIGH), (1u << TASK_HIGH), 0u, 0u, 0u, 0u, 0u, (1u << IOC_SHARED) }
    };
    const Os_IocConfigType ioc_cfg[] = {
        { "SharedQueue", 2u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureIoc(ioc_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_HIGH));
    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    TEST_ASSERT_EQUAL_STRING("SR", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, ioc_sender_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, ioc_receiver_runs);
    TEST_ASSERT_EQUAL(E_OK, ioc_send_first_status);
    TEST_ASSERT_EQUAL(E_OK, ioc_send_second_status);
    TEST_ASSERT_EQUAL(E_OK, ioc_receive_first_status);
    TEST_ASSERT_EQUAL(E_OK, ioc_receive_second_status);
    TEST_ASSERT_EQUAL_HEX32(0x11u, ioc_received_first_value);
    TEST_ASSERT_EQUAL_HEX32(0x22u, ioc_received_second_value);
    TEST_ASSERT_EQUAL(INVALID_TASK, Os_TestGetCurrentTask());
}

void test_Os_activate_task_before_start_reports_state_error(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 3u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    Os_TestSetErrorHook(error_hook);

    TEST_ASSERT_EQUAL(E_OS_STATE, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL_UINT16(1u, Det_GetErrorCount());
    TEST_ASSERT_EQUAL(E_OS_STATE, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.1, §13.7
 * @requirement Autostarted tasks shall become ready at StartOS and the
 *              highest-priority ready task shall run first.
 * @verify Startup hook runs once, then autostart tasks execute in priority
 *         order and return to SUSPENDED after completion.
 */
void test_Os_startos_runs_autostart_tasks_by_priority(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 3u, 1u, (1u << OSDEFAULTAPPMODE), FALSE, FULL },
        { "High", task_high, 1u, 1u, (1u << OSDEFAULTAPPMODE), FALSE, FULL }
    };
    TaskStateType low_state = RUNNING;
    TaskStateType high_state = RUNNING;

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    Os_TestSetStartupHook(startup_hook);
    Os_TestSetErrorHook(error_hook);

    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL_STRING("SHL", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, high_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, low_runs);
    TEST_ASSERT_EQUAL(E_OK, high_status);
    TEST_ASSERT_EQUAL(E_OK, low_status);
    TEST_ASSERT_EQUAL(OSDEFAULTAPPMODE, GetActiveApplicationMode());
    TEST_ASSERT_EQUAL_UINT32(0u, Os_TestGetReadyBitmap());
    TEST_ASSERT_EQUAL_UINT32(2u, Os_TestGetDispatchCount());
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_LOW, &low_state));
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_HIGH, &high_state));
    TEST_ASSERT_EQUAL(SUSPENDED, low_state);
    TEST_ASSERT_EQUAL(SUSPENDED, high_state);
}

/**
 * @spec OSEK OS 2.2.3 §13.7.2
 * @requirement StartupHook shall run before task dispatch, and PreTaskHook /
 *              PostTaskHook shall bracket each dispatched task.
 * @verify Hook markers appear around each autostart task in dispatch order.
 */
void test_Os_startup_and_task_hooks_wrap_each_dispatched_task(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 3u, 1u, (1u << OSDEFAULTAPPMODE), FALSE, FULL },
        { "High", task_high, 1u, 1u, (1u << OSDEFAULTAPPMODE), FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    Os_TestSetStartupHook(startup_hook);
    Os_TestSetPreTaskHook(pre_task_hook);
    Os_TestSetPostTaskHook(post_task_hook);

    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL_STRING("S>H<>L<", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, pre_task_hook_count);
    TEST_ASSERT_EQUAL_UINT8(2u, post_task_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.2
 * @requirement ActivateTask shall put a suspended task into the ready set.
 * @verify Schedule dispatches the activated task from idle context.
 */
void test_Os_schedule_dispatches_ready_tasks_from_idle_context(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 3u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL_UINT8(1u, Os_TestGetPendingActivations(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("L", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, low_runs);
    TEST_ASSERT_EQUAL(E_OK, low_status);
    TEST_ASSERT_EQUAL(TASK_LOW, observed_task_id);
}

/**
 * @spec OSEK OS 2.2.3 §13.1
 * @requirement Ready tasks at the same priority shall follow a deterministic
 *              ready-queue order.
 * @verify Equal-priority tasks execute FIFO in the bootstrap scheduler.
 */
void test_Os_same_priority_tasks_run_fifo_order(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 2u, 1u, 0u, FALSE, FULL },
        { "Peer", task_peer, 2u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_HIGH));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("LQ", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, peer_runs);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement The OS shall identify the current OS-Application of the
 *              running execution context.
 * @verify A running task observes the configured owning application through
 *         GetApplicationID and GetCurrentApplicationID.
 */
void test_Os_get_application_id_reports_owner_of_running_task(void)
{
    const Os_TaskConfigType cfg[] = {
        { "AppProbe", task_application_probe, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppZero", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("P", execution_log);
    TEST_ASSERT_EQUAL_UINT8(0u, observed_application_id);
    TEST_ASSERT_EQUAL_UINT8(0u, observed_current_application_id);
}

/**
 * @spec AUTOSAR OS §7.10.5
 * @requirement Memory access checks shall grant a task access to memory
 *              regions owned by its OS-Application.
 * @verify CheckTaskMemoryAccess returns ACCESS for an address range fully
 *         contained in the owning application's configured region.
 */
void test_Os_check_task_memory_access_allows_owned_application_region(void)
{
    const Os_TaskConfigType cfg[] = {
        { "MemTaskA", task_low, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u }
    };
    const Os_MemoryRegionConfigType mem_cfg[] = {
        { "RegionA", (ApplicationType)0u, (MemoryStartAddressType)&memory_region_app_a[0], (MemorySizeType)sizeof(memory_region_app_a) }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureMemoryRegions(mem_cfg, 1u));

    memory_access_owned_result =
        CheckTaskMemoryAccess(TASK_LOW, (MemoryStartAddressType)&memory_region_app_a[4], 8u);

    TEST_ASSERT_EQUAL(ACCESS, memory_access_owned_result);
}

/**
 * @spec AUTOSAR OS §7.10.5
 * @requirement Memory access checks shall deny a task access to memory
 *              regions owned by a different OS-Application.
 * @verify CheckTaskMemoryAccess returns NO_ACCESS for a foreign application's
 *         configured memory region and for an invalid task ID.
 */
void test_Os_check_task_memory_access_denies_foreign_region_and_invalid_task(void)
{
    const Os_TaskConfigType cfg[] = {
        { "MemTaskA", task_low, 2u, 1u, 0u, FALSE, FULL },
        { "MemTaskB", task_high, 1u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u },
        { "AppB", FALSE, (1u << TASK_HIGH), (1u << TASK_HIGH), 0u, 0u, 0u, 0u, 0u, 0u }
    };
    const Os_MemoryRegionConfigType mem_cfg[] = {
        { "RegionA", (ApplicationType)0u, (MemoryStartAddressType)&memory_region_app_a[0], (MemorySizeType)sizeof(memory_region_app_a) },
        { "RegionB", (ApplicationType)1u, (MemoryStartAddressType)&memory_region_app_b[0], (MemorySizeType)sizeof(memory_region_app_b) }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureMemoryRegions(mem_cfg, 2u));

    memory_access_foreign_result =
        CheckTaskMemoryAccess(TASK_LOW, (MemoryStartAddressType)&memory_region_app_b[8], 8u);
    memory_access_invalid_result =
        CheckTaskMemoryAccess((TaskType)7u, (MemoryStartAddressType)&memory_region_app_a[0], 4u);

    TEST_ASSERT_EQUAL(NO_ACCESS, memory_access_foreign_result);
    TEST_ASSERT_EQUAL(NO_ACCESS, memory_access_invalid_result);
}

/**
 * @spec AUTOSAR OS §7.10.3, §7.10.5
 * @requirement A trusted OS-Application may bypass bootstrap software memory
 *              region restrictions.
 * @verify CheckTaskMemoryAccess returns ACCESS for a trusted task even when
 *         the address lies outside its configured region set.
 */
void test_Os_check_task_memory_access_allows_trusted_application_any_region(void)
{
    const Os_TaskConfigType cfg[] = {
        { "TrustedMemTask", task_low, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "TrustedApp", TRUE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 1u));

    memory_access_trusted_result =
        CheckTaskMemoryAccess(TASK_LOW, (MemoryStartAddressType)&memory_region_app_b[0], (MemorySizeType)sizeof(memory_region_app_b));

    TEST_ASSERT_EQUAL(ACCESS, memory_access_trusted_result);
}

/**
 * @spec OSEK OS 2.2.3 §13.2.3.2
 * @requirement If ActivateTask is called for a BCC1 task that is not in
 *              SUSPENDED state, the service shall return E_OS_LIMIT.
 * @verify Pending activation count does not exceed one and the error hook
 *         receives E_OS_LIMIT.
 */
void test_Os_activation_limit_is_enforced(void)
{
    const Os_TaskConfigType cfg[] = {
        { "High", task_high, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OS_LIMIT, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL_UINT8(1u, Os_TestGetPendingActivations(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OS_LIMIT, error_hook_status);
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("H", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, high_runs);
}

/**
 * @spec OSEK OS 2.2.3 §13.2
 * @requirement ChainTask shall terminate the current task and activate the
 *              successor task atomically.
 * @verify The chained task runs first, then its successor runs next.
 */
void test_Os_chain_task_queues_successor_after_current_completion(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Chain", task_chain, 2u, 1u, 0u, FALSE, FULL },
        { "High", task_high, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("CH", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, chain_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, high_runs);
    TEST_ASSERT_EQUAL(E_OK, chain_status);
    TEST_ASSERT_EQUAL(E_OK, high_status);
}

/**
 * @spec OSEK OS 2.2.3 §13.2
 * @requirement GetTaskState shall reflect the current state of a valid task.
 * @verify A running task observes itself in RUNNING state.
 */
void test_Os_get_task_state_reports_running_while_task_executes(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Probe", task_probe, 2u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("P", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, probe_runs);
    TEST_ASSERT_EQUAL(RUNNING, observed_task_state);
    TEST_ASSERT_EQUAL(E_OK, probe_status);
}

/**
 * @spec OSEK OS 2.2.3 §13.2
 * @requirement Schedule is meaningful only at the allowed call level.
 * @verify Calling Schedule from inside a running task is rejected in the
 *         current bootstrap with E_OS_CALLEVEL.
 */
void test_Os_schedule_from_running_task_is_rejected_in_current_milestone(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Schedule", task_schedule, 2u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("Y", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, schedule_runs);
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, schedule_status);
    TEST_ASSERT_EQUAL(E_OS_CALLEVEL, error_hook_status);
}

/**
 * @spec OSEK OS 2.2.3 §13.7
 * @requirement StartOS shall activate only the tasks configured for the
 *              selected application mode.
 * @verify A task autostarts only in the matching application mode.
 */
void test_Os_app_mode_mask_controls_autostart(void)
{
    const Os_TaskConfigType cfg[] = {
        { "High", task_high, 1u, 1u, (1u << APPMODE_DIAG), FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));

    StartOS(OSDEFAULTAPPMODE);
    TEST_ASSERT_EQUAL_STRING("", execution_log);

    Os_Init();
    reset_observations();
    StartOS(APPMODE_DIAG);
    TEST_ASSERT_EQUAL_STRING("H", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, high_runs);
}

/**
 * @spec OSEK OS 2.2.3 §13.7.2
 * @requirement ShutdownHook shall receive the shutdown status code when the
 *              OS is shut down.
 * @verify ShutdownOS calls the shutdown hook once with the requested error.
 */
void test_Os_shutdown_hook_observes_shutdown_error(void)
{
    Os_TestSetShutdownHook(shutdown_hook);
    StartOS(OSDEFAULTAPPMODE);

    ShutdownOS(E_OS_LIMIT);

    TEST_ASSERT_EQUAL_STRING("D", execution_log);
    TEST_ASSERT_EQUAL(E_OS_LIMIT, shutdown_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, shutdown_hook_count);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement The OS shall provide ownership and access checks for objects
 *              across OS-Applications.
 * @verify Ownership is reported per configured application, and access is
 *         granted or denied according to the configured access masks.
 */
void test_Os_application_object_ownership_and_access_follow_configuration(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 3u, 1u, 0u, FALSE, FULL }
    };
    const Os_ResourceConfigType res_cfg[] = {
        { "ResShared", 1u }
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmShared", TASK_LOW, 9u, 1u, 1u }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), (1u << RES_SHARED), (1u << RES_SHARED), 0u, 0u, 0u, 0u },
        { "AppB", FALSE, 0u, 0u, 0u, (1u << RES_SHARED), (1u << ALARM_ACTIVATE), (1u << ALARM_ACTIVATE), 0u, 0u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureResources(res_cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));

    owned_task_application = CheckObjectOwnership(OBJECT_TASK, TASK_LOW);
    owned_resource_application = CheckObjectOwnership(OBJECT_RESOURCE, RES_SHARED);
    owned_alarm_application = CheckObjectOwnership(OBJECT_ALARM, ALARM_ACTIVATE);
    task_access_result = CheckObjectAccess((ApplicationType)0u, OBJECT_TASK, TASK_LOW);
    resource_access_result = CheckObjectAccess((ApplicationType)1u, OBJECT_RESOURCE, RES_SHARED);
    alarm_access_result = CheckObjectAccess((ApplicationType)0u, OBJECT_ALARM, ALARM_ACTIVATE);

    TEST_ASSERT_EQUAL_UINT8(0u, owned_task_application);
    TEST_ASSERT_EQUAL_UINT8(0u, owned_resource_application);
    TEST_ASSERT_EQUAL_UINT8(1u, owned_alarm_application);
    TEST_ASSERT_EQUAL(ACCESS, task_access_result);
    TEST_ASSERT_EQUAL(ACCESS, resource_access_result);
    TEST_ASSERT_EQUAL(NO_ACCESS, alarm_access_result);
}

/**
 * @spec AUTOSAR OS §7.10.3
 * @requirement A configured OS-Application shall be able to call an allowed
 *              trusted function.
 * @verify The trusted function handler runs once, receives the parameter,
 *         and CallTrustedFunction returns E_OK.
 */
void test_Os_trusted_function_allows_authorized_application(void)
{
    const Os_TaskConfigType cfg[] = {
        { "TrustedCaller", task_trusted_function_allowed, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u }
    };
    const Os_TrustedFunctionConfigType trusted_cfg[] = {
        { "TrustedProbe", trusted_function_probe, (1u << 0u) }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTrustedFunctions(trusted_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("TA", execution_log);
    TEST_ASSERT_EQUAL(E_OK, trusted_function_status);
    TEST_ASSERT_EQUAL_UINT8(1u, trusted_function_call_count);
    TEST_ASSERT_EQUAL_UINT8(7u, trusted_param_value);
}

/**
 * @spec AUTOSAR OS §7.10.3
 * @requirement A trusted function call shall be rejected when the current
 *              OS-Application is not authorized for that function.
 * @verify CallTrustedFunction returns E_OS_ACCESS and the handler is not run.
 */
void test_Os_trusted_function_rejects_unauthorized_application(void)
{
    const Os_TaskConfigType cfg[] = {
        { "TrustedDenied", task_trusted_function_denied, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u }
    };
    const Os_TrustedFunctionConfigType trusted_cfg[] = {
        { "TrustedProbe", trusted_function_probe, 0u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTrustedFunctions(trusted_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("D", execution_log);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, denied_trusted_function_status);
    TEST_ASSERT_EQUAL_UINT8(0u, trusted_function_call_count);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement A task shall not activate a task object outside its
 *              configured OS-Application access rights.
 * @verify ActivateTask returns E_OS_ACCESS and the foreign task does not run.
 */
void test_Os_application_access_denies_activate_task_on_foreign_task(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Caller", task_app_denied_activate, 2u, 1u, 0u, FALSE, FULL },
        { "Foreign", task_high, 1u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u },
        { "AppB", FALSE, (1u << TASK_HIGH), (1u << TASK_HIGH), 0u, 0u, 0u, 0u, 0u, 0u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("a", execution_log);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, app_activate_denied_status);
    TEST_ASSERT_EQUAL_UINT8(0u, high_runs);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement A task shall not acquire a resource object outside its
 *              configured OS-Application access rights.
 * @verify GetResource returns E_OS_ACCESS for a foreign resource.
 */
void test_Os_application_access_denies_get_resource_on_foreign_resource(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Caller", task_app_denied_resource, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ResourceConfigType res_cfg[] = {
        { "ForeignRes", 1u }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u },
        { "AppB", FALSE, 0u, 0u, (1u << RES_SHARED), (1u << RES_SHARED), 0u, 0u, 0u, 0u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureResources(res_cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("r", execution_log);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, app_resource_denied_status);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement A task shall not signal an extended task outside its
 *              configured OS-Application access rights.
 * @verify SetEvent returns E_OS_ACCESS and the foreign waiting task remains
 *         blocked.
 */
void test_Os_application_access_denies_set_event_on_foreign_task(void)
{
    const Os_TaskConfigType cfg[] = {
        { "WaitExt", task_wait_event_extended, 2u, 1u, 0u, TRUE, FULL },
        { "Caller", task_app_denied_set_event, 3u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_HIGH), (1u << TASK_HIGH), 0u, 0u, 0u, 0u, 0u, 0u },
        { "AppB", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u }
    };
    TaskStateType wait_state = RUNNING;

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("W", execution_log);
    TEST_ASSERT_EQUAL(WAITING, event_wait_state_after_block);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_HIGH));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("We", execution_log);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, app_set_event_denied_status);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_LOW, &wait_state));
    TEST_ASSERT_EQUAL(WAITING, wait_state);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec AUTOSAR OS §7.10
 * @requirement A task shall not control an alarm object outside its
 *              configured OS-Application access rights.
 * @verify SetRelAlarm returns E_OS_ACCESS for a foreign alarm.
 */
void test_Os_application_access_denies_set_rel_alarm_on_foreign_alarm(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Caller", task_app_denied_set_alarm, 2u, 1u, 0u, FALSE, FULL },
        { "AlarmTarget", task_high, 1u, 1u, 0u, FALSE, FULL }
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "ForeignAlarm", TASK_HIGH, 9u, 1u, 1u }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u },
        { "AppB", FALSE, (1u << TASK_HIGH), (1u << TASK_HIGH), 0u, 0u, (1u << ALARM_ACTIVATE), (1u << ALARM_ACTIVATE), 0u, 0u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("l", execution_log);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, app_set_alarm_denied_status);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec AUTOSAR OS §8
 * @requirement IOC shall transfer queued data between authorized
 *              OS-Applications in FIFO order.
 * @verify Two sends by the sender task are received in order by the
 *         receiver task on the configured IOC channel.
 */
void test_Os_ioc_queue_transfers_fifo_data_between_applications(void)
{
    const Os_TaskConfigType cfg[] = {
        { "IocSender", task_ioc_sender, 2u, 1u, 0u, FALSE, FULL },
        { "IocReceiver", task_ioc_receiver, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, (1u << IOC_SHARED), (1u << IOC_SHARED) },
        { "AppB", FALSE, (1u << TASK_HIGH), (1u << TASK_HIGH), 0u, 0u, 0u, 0u, 0u, (1u << IOC_SHARED) }
    };
    const Os_IocConfigType ioc_cfg[] = {
        { "SharedQueue", 2u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureIoc(ioc_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_HIGH));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("SR", execution_log);
    TEST_ASSERT_EQUAL(E_OK, ioc_send_first_status);
    TEST_ASSERT_EQUAL(E_OK, ioc_send_second_status);
    TEST_ASSERT_EQUAL(E_OK, ioc_receive_first_status);
    TEST_ASSERT_EQUAL(E_OK, ioc_receive_second_status);
    TEST_ASSERT_EQUAL_HEX32(0x11u, ioc_received_first_value);
    TEST_ASSERT_EQUAL_HEX32(0x22u, ioc_received_second_value);
}

/**
 * @spec AUTOSAR OS §8
 * @requirement IOC receive shall report when no queued data is available.
 * @verify IocReceive returns IOC_E_NO_DATA on an empty queue.
 */
void test_Os_ioc_receive_on_empty_queue_returns_no_data(void)
{
    const Os_TaskConfigType cfg[] = {
        { "IocEmpty", task_ioc_receiver_empty, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppB", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, (1u << IOC_SHARED) }
    };
    const Os_IocConfigType ioc_cfg[] = {
        { "SharedQueue", 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureIoc(ioc_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("N", execution_log);
    TEST_ASSERT_EQUAL(IOC_E_NO_DATA, ioc_receive_empty_status);
    TEST_ASSERT_EQUAL(IOC_E_NO_DATA, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec AUTOSAR OS §8
 * @requirement IOC send shall reject data when the configured queue is full.
 * @verify The third send beyond queue capacity returns IOC_E_LIMIT and the
 *         first two queued elements remain readable.
 */
void test_Os_ioc_send_rejects_queue_overflow_with_limit_error(void)
{
    const Os_TaskConfigType cfg[] = {
        { "IocOverflow", task_ioc_sender_overflow, 2u, 1u, 0u, FALSE, FULL },
        { "IocReceiver", task_ioc_receiver, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, (1u << IOC_SHARED), (1u << IOC_SHARED) },
        { "AppB", FALSE, (1u << TASK_HIGH), (1u << TASK_HIGH), 0u, 0u, 0u, 0u, 0u, (1u << IOC_SHARED) }
    };
    const Os_IocConfigType ioc_cfg[] = {
        { "SharedQueue", 2u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureIoc(ioc_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_HIGH));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("OR", execution_log);
    TEST_ASSERT_EQUAL(E_OK, ioc_send_first_status);
    TEST_ASSERT_EQUAL(E_OK, ioc_send_second_status);
    TEST_ASSERT_EQUAL(IOC_E_LIMIT, ioc_send_overflow_status);
    TEST_ASSERT_EQUAL_HEX32(0x33u, ioc_received_first_value);
    TEST_ASSERT_EQUAL_HEX32(0x44u, ioc_received_second_value);
    TEST_ASSERT_EQUAL(IOC_E_LIMIT, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec AUTOSAR OS §8
 * @requirement IOC access shall follow configured OS-Application rights.
 * @verify IocSend returns E_OS_ACCESS when the caller is not allowed to use
 *         the foreign IOC channel.
 */
void test_Os_application_access_denies_ioc_send_on_foreign_channel(void)
{
    const Os_TaskConfigType cfg[] = {
        { "IocDenied", task_ioc_denied_sender, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ApplicationConfigType app_cfg[] = {
        { "AppA", FALSE, (1u << TASK_LOW), (1u << TASK_LOW), 0u, 0u, 0u, 0u, 0u, 0u },
        { "AppB", FALSE, 0u, 0u, 0u, 0u, 0u, 0u, (1u << IOC_SHARED), (1u << IOC_SHARED) }
    };
    const Os_IocConfigType ioc_cfg[] = {
        { "SharedQueue", 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureApplications(app_cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureIoc(ioc_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("X", execution_log);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, ioc_denied_status);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec AUTOSAR OS §7.6.5
 * @requirement Stack monitoring shall record task stack usage against the
 *              configured budget.
 * @verify A task running within budget updates its peak usage without
 *         raising a stack-monitoring violation.
 */
void test_Os_stack_monitor_records_peak_usage_within_budget(void)
{
    const Os_TaskConfigType cfg[] = {
        { "StackSafe", task_stack_safe, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_StackMonitorConfigType stack_cfg[] = {
        { TASK_LOW, 4096u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureStacks(stack_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    stack_safe_peak = Os_TestGetTaskStackPeak(TASK_LOW);
    stack_safe_violation = Os_TestTaskHasStackViolation(TASK_LOW);

    TEST_ASSERT_EQUAL_STRING("K", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, stack_safe_runs);
    TEST_ASSERT_EQUAL(E_OK, stack_safe_get_id_status);
    TEST_ASSERT_EQUAL(E_OK, stack_safe_term_status);
    TEST_ASSERT_TRUE(stack_safe_peak > 0u);
    TEST_ASSERT_FALSE(stack_safe_violation);
    TEST_ASSERT_EQUAL_UINT8(0u, error_hook_count);
}

/**
 * @spec AUTOSAR OS §7.6.5
 * @requirement Stack monitoring shall detect configured budget violations
 *              and force a safe shutdown reaction.
 * @verify A task exceeding its budget is marked as violated, reports
 *         E_OS_LIMIT, and triggers ShutdownOS(E_OS_LIMIT).
 */
void test_Os_stack_monitor_detects_budget_violation_and_requests_shutdown(void)
{
    const Os_TaskConfigType cfg[] = {
        { "StackOverflow", task_stack_overflow, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_StackMonitorConfigType stack_cfg[] = {
        { TASK_LOW, 64u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureStacks(stack_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    Os_TestSetShutdownHook(shutdown_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    stack_overflow_peak = Os_TestGetTaskStackPeak(TASK_LOW);
    stack_overflow_violation = Os_TestTaskHasStackViolation(TASK_LOW);

    TEST_ASSERT_EQUAL_STRING("VD", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, stack_overflow_runs);
    TEST_ASSERT_EQUAL(E_OK, stack_overflow_get_id_status);
    TEST_ASSERT_EQUAL(E_OK, stack_overflow_term_status);
    TEST_ASSERT_TRUE(stack_overflow_peak > 64u);
    TEST_ASSERT_TRUE(stack_overflow_violation);
    TEST_ASSERT_EQUAL(E_OS_LIMIT, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
    TEST_ASSERT_EQUAL(E_OS_LIMIT, shutdown_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, shutdown_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.2
 * @requirement Services taking a TaskID shall reject invalid task IDs.
 * @verify ActivateTask returns E_OS_ID for an out-of-range task.
 */
void test_Os_activate_task_with_invalid_id_returns_e_os_id(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 3u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OS_ID, ActivateTask((TaskType)7u));
    TEST_ASSERT_EQUAL(E_OS_ID, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.2
 * @requirement Services with output pointers shall reject NULL pointers.
 * @verify GetTaskID returns E_OS_VALUE when called with NULL.
 */
void test_Os_get_task_id_with_null_pointer_returns_e_os_value(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Low", task_low, 3u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    Os_TestSetErrorHook(error_hook);

    TEST_ASSERT_EQUAL(E_OS_VALUE, GetTaskID(NULL_PTR));
    TEST_ASSERT_EQUAL(E_OS_VALUE, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.1, §13.2
 * @requirement If a higher-priority task becomes ready while a lower-
 *              priority task is running, the higher-priority task shall run
 *              before the lower-priority task continues.
 * @verify ActivateTask from a running low-priority task causes immediate
 *         nested dispatch of the higher-priority task in the bootstrap.
 */
void test_Os_higher_priority_activation_preempts_running_task_immediately(void)
{
    const Os_TaskConfigType cfg[] = {
        { "LowPreempt", task_low_preempt, 3u, 1u, 0u, FALSE, FULL },
        { "HighPreempt", task_high_preempt, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("ABa", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, low_preempt_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, high_preempt_runs);
    TEST_ASSERT_EQUAL(E_OK, low_preempt_activate_status);
    TEST_ASSERT_EQUAL(E_OK, low_preempt_term_status);
    TEST_ASSERT_EQUAL(E_OK, high_preempt_term_status);
}

/**
 * @spec OSEK OS 2.2.3 §13.1, §13.3
 * @requirement A higher-priority task readied from Category 2 ISR context
 *              shall be dispatched when the ISR completes.
 * @verify ISR activation does not preempt while nesting is non-zero, then
 *         dispatches the higher-priority task on ISR exit before low task
 *         execution resumes.
 */
void test_Os_cat2_isr_exit_dispatches_higher_priority_task_before_return(void)
{
    const Os_TaskConfigType cfg[] = {
        { "LowIsr", task_low_isr, 3u, 1u, 0u, FALSE, FULL },
        { "HighIsr", task_high_isr, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("LIHl", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_high_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, isr_handler_runs);
    TEST_ASSERT_EQUAL(E_OK, isr_invoke_status);
    TEST_ASSERT_EQUAL(E_OK, isr_activate_status);
    TEST_ASSERT_EQUAL(E_OK, isr_low_term_status);
    TEST_ASSERT_EQUAL(E_OK, isr_high_term_status);
    TEST_ASSERT_EQUAL_UINT8(1u, observed_isr_nesting);
    TEST_ASSERT_EQUAL_UINT8(0u, Os_TestGetIsrCat2Nesting());
}

/**
 * @spec OSEK OS 2.2.3 §13.4
 * @requirement A task holding a resource shall execute at the resource
 *              ceiling priority so lower-ceiling preemption is deferred
 *              until ReleaseResource.
 * @verify A higher-priority ready task does not run while the resource is
 *         held, then preempts immediately when the resource is released.
 */
void test_Os_resource_ceiling_defers_preemption_until_release(void)
{
    const Os_TaskConfigType cfg[] = {
        { "ResLow", task_low_resource, 3u, 1u, 0u, FALSE, FULL },
        { "ResHigh", task_high_resource, 1u, 1u, 0u, FALSE, FULL }
    };
    const Os_ResourceConfigType res_cfg[] = {
        { "ResShared", 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureResources(res_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("RrHx", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, resource_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, resource_high_runs);
    TEST_ASSERT_EQUAL(E_OK, resource_get_status);
    TEST_ASSERT_EQUAL(E_OK, resource_activate_status);
    TEST_ASSERT_EQUAL(E_OK, resource_release_status);
    TEST_ASSERT_EQUAL(E_OK, resource_low_term_status);
    TEST_ASSERT_EQUAL(E_OK, resource_high_term_status);
}

/**
 * @spec OSEK OS 2.2.3 §13.4
 * @requirement Nested resources shall be released in reverse acquisition
 *              order.
 * @verify Releasing the outer resource first returns E_OS_NOFUNC, then the
 *         inner and outer resources can be released in proper LIFO order.
 */
void test_Os_nested_resources_must_release_in_lifo_order(void)
{
    const Os_TaskConfigType cfg[] = {
        { "ResOrder", task_resource_release_order, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ResourceConfigType res_cfg[] = {
        { "ResOuter", 2u },
        { "ResInner", 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureResources(res_cfg, 2u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("Nn", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, resource_order_runs);
    TEST_ASSERT_EQUAL(E_OK, resource_outer_get_status);
    TEST_ASSERT_EQUAL(E_OK, resource_inner_get_status);
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, resource_wrong_release_status);
    TEST_ASSERT_EQUAL(E_OK, resource_inner_release_status);
    TEST_ASSERT_EQUAL(E_OK, resource_outer_release_status);
    TEST_ASSERT_EQUAL(E_OK, resource_order_term_status);
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.2, §13.4
 * @requirement A task shall not terminate while it still occupies a
 *              resource.
 * @verify TerminateTask returns E_OS_RESOURCE until the held resource is
 *         released, then succeeds on cleanup termination.
 */
void test_Os_terminate_task_rejects_held_resource_with_e_os_resource(void)
{
    const Os_TaskConfigType cfg[] = {
        { "ResTerminate", task_terminate_with_resource, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_ResourceConfigType res_cfg[] = {
        { "ResShared", 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureResources(res_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("Tt", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, resource_terminate_runs);
    TEST_ASSERT_EQUAL(E_OK, resource_terminate_get_status);
    TEST_ASSERT_EQUAL(E_OS_RESOURCE, resource_terminate_status);
    TEST_ASSERT_EQUAL(E_OK, resource_terminate_release_status);
    TEST_ASSERT_EQUAL(E_OK, resource_terminate_cleanup_status);
    TEST_ASSERT_EQUAL(E_OS_RESOURCE, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.2, §13.4
 * @requirement ChainTask shall not terminate a task that still occupies a
 *              resource.
 * @verify ChainTask returns E_OS_RESOURCE while the resource is held and
 *         does not activate the successor task.
 */
void test_Os_chain_task_rejects_held_resource_with_e_os_resource(void)
{
    const Os_TaskConfigType cfg[] = {
        { "ResChain", task_chain_with_resource, 2u, 1u, 0u, FALSE, FULL },
        { "High", task_high, 1u, 1u, 0u, FALSE, FULL }
    };
    const Os_ResourceConfigType res_cfg[] = {
        { "ResShared", 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureResources(res_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("Cc", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, resource_chain_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, high_runs);
    TEST_ASSERT_EQUAL(E_OK, resource_chain_get_status);
    TEST_ASSERT_EQUAL(E_OS_RESOURCE, resource_chain_status);
    TEST_ASSERT_EQUAL(E_OK, resource_chain_release_status);
    TEST_ASSERT_EQUAL(E_OK, resource_chain_cleanup_status);
    TEST_ASSERT_EQUAL(E_OS_RESOURCE, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.5
 * @requirement An extended task calling WaitEvent shall enter WAITING until
 *              a matching SetEvent readies it again.
 * @verify The waiter blocks in WAITING, resumes after SetEvent, observes the
 *         event mask, clears it, and then terminates cleanly.
 */
void test_Os_wait_event_blocks_extended_task_until_set_event(void)
{
    const Os_TaskConfigType cfg[] = {
        { "WaitExt", task_wait_event_extended, 2u, 1u, 0u, TRUE, FULL },
        { "Notifier", task_set_event_notifier, 3u, 1u, 0u, FALSE, FULL }
    };
    TaskStateType wait_state = RUNNING;

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("W", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, event_wait_runs);
    TEST_ASSERT_EQUAL(E_OK, event_wait_status);
    TEST_ASSERT_EQUAL(WAITING, event_wait_state_after_block);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_LOW, &wait_state));
    TEST_ASSERT_EQUAL(WAITING, wait_state);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_HIGH));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("WNEn", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, event_wait_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, event_setter_runs);
    TEST_ASSERT_EQUAL(E_OK, event_set_status);
    TEST_ASSERT_EQUAL(E_OK, event_get_status);
    TEST_ASSERT_EQUAL(EVENT_ALPHA, event_mask_before_clear);
    TEST_ASSERT_EQUAL(E_OK, event_clear_status);
    TEST_ASSERT_EQUAL(E_OK, event_get_after_clear_status);
    TEST_ASSERT_EQUAL(0u, event_mask_after_clear);
    TEST_ASSERT_EQUAL(E_OK, event_wait_term_status);
    TEST_ASSERT_EQUAL(E_OK, event_setter_term_status);
    TEST_ASSERT_EQUAL_UINT8(2u, event_wait_phase);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_LOW, &wait_state));
    TEST_ASSERT_EQUAL(SUSPENDED, wait_state);
}

/**
 * @spec OSEK OS 2.2.3 §13.5
 * @requirement Event services shall be available only to extended tasks.
 * @verify WaitEvent from a basic task returns E_OS_ACCESS and the task can
 *         still terminate normally afterward.
 */
void test_Os_wait_event_rejects_basic_task_with_e_os_access(void)
{
    const Os_TaskConfigType cfg[] = {
        { "BasicWait", task_wait_event_basic, 2u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("B", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, event_basic_runs);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, event_basic_wait_status);
    TEST_ASSERT_EQUAL(E_OK, event_basic_term_status);
    TEST_ASSERT_EQUAL(E_OS_ACCESS, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.4, §13.5
 * @requirement WaitEvent shall not be accepted while the current extended
 *              task still occupies a resource.
 * @verify WaitEvent returns E_OS_RESOURCE, the task does not enter WAITING,
 *         and cleanup succeeds only after ReleaseResource.
 */
void test_Os_wait_event_rejects_held_resource_with_e_os_resource(void)
{
    const Os_TaskConfigType cfg[] = {
        { "WaitRes", task_wait_event_with_resource, 2u, 1u, 0u, TRUE, FULL }
    };
    const Os_ResourceConfigType res_cfg[] = {
        { "ResShared", 1u }
    };
    TaskStateType wait_state = RUNNING;

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureResources(res_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("Rr", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, event_wait_runs);
    TEST_ASSERT_EQUAL(E_OK, event_wait_resource_get_status);
    TEST_ASSERT_EQUAL(E_OS_RESOURCE, event_wait_resource_wait_status);
    TEST_ASSERT_EQUAL(E_OK, event_wait_resource_release_status);
    TEST_ASSERT_EQUAL(E_OK, event_wait_resource_term_status);
    TEST_ASSERT_EQUAL(E_OS_RESOURCE, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_LOW, &wait_state));
    TEST_ASSERT_EQUAL(SUSPENDED, wait_state);
}

/**
 * @spec OSEK OS 2.2.3 §13.3, §13.5
 * @requirement A matching SetEvent from Category 2 ISR context shall ready
 *              a waiting extended task and dispatch it on ISR exit.
 * @verify The waiting task resumes after ISR completion before the low-
 *         priority runner continues.
 */
void test_Os_cat2_isr_set_event_dispatches_waiting_task_on_exit(void)
{
    const Os_TaskConfigType cfg[] = {
        { "WaitExt", task_wait_event_extended, 2u, 1u, 0u, TRUE, FULL },
        { "IsrRunner", task_isr_event_runner, 3u, 1u, 0u, FALSE, FULL }
    };
    TaskStateType wait_state = RUNNING;

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("W", execution_log);
    TEST_ASSERT_EQUAL(WAITING, event_wait_state_after_block);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_HIGH));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("WLIEl", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, event_wait_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, event_isr_runner_runs);
    TEST_ASSERT_EQUAL(E_OK, event_isr_invoke_status);
    TEST_ASSERT_EQUAL(E_OK, event_isr_set_status);
    TEST_ASSERT_EQUAL(E_OK, event_wait_term_status);
    TEST_ASSERT_EQUAL(E_OK, event_isr_runner_term_status);
    TEST_ASSERT_EQUAL(E_OK, GetTaskState(TASK_LOW, &wait_state));
    TEST_ASSERT_EQUAL(SUSPENDED, wait_state);
}

/**
 * @spec OSEK OS 2.2.3 §13.6
 * @requirement A relative alarm shall expire after the configured increment
 *              and activate its configured task.
 * @verify GetAlarmBase reports the configured counter base, GetAlarm reports
 *         remaining ticks, and the task runs on expiry.
 */
void test_Os_relative_alarm_reports_remaining_ticks_and_activates_task(void)
{
    const Os_TaskConfigType cfg[] = {
        { "AlarmTask", task_alarm_target, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmActivate", TASK_LOW, 9u, 1u, 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    alarm_get_base_status = GetAlarmBase(ALARM_ACTIVATE, &observed_alarm_base);
    alarm_set_rel_status = SetRelAlarm(ALARM_ACTIVATE, 3u, 0u);
    alarm_get_status = GetAlarm(ALARM_ACTIVATE, &alarm_remaining_ticks);

    TEST_ASSERT_EQUAL(E_OK, alarm_get_base_status);
    TEST_ASSERT_EQUAL_UINT32(9u, observed_alarm_base.maxallowedvalue);
    TEST_ASSERT_EQUAL_UINT32(1u, observed_alarm_base.ticksperbase);
    TEST_ASSERT_EQUAL_UINT32(1u, observed_alarm_base.mincycle);
    TEST_ASSERT_EQUAL(E_OK, alarm_set_rel_status);
    TEST_ASSERT_EQUAL(E_OK, alarm_get_status);
    TEST_ASSERT_EQUAL_UINT32(3u, alarm_remaining_ticks);

    Os_TestAdvanceCounter(2u);
    TEST_ASSERT_EQUAL_STRING("", execution_log);
    TEST_ASSERT_EQUAL(E_OK, GetAlarm(ALARM_ACTIVATE, &alarm_remaining_after_ticks));
    TEST_ASSERT_EQUAL_UINT32(1u, alarm_remaining_after_ticks);

    Os_TestAdvanceCounter(1u);
    TEST_ASSERT_EQUAL_STRING("A", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, alarm_target_runs);
    TEST_ASSERT_EQUAL(E_OK, alarm_target_term_status);
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, GetAlarm(ALARM_ACTIVATE, &alarm_remaining_after_ticks));
}

/**
 * @spec OSEK OS 2.2.3 §13.6
 * @requirement A cyclic alarm shall re-arm itself after each expiry until
 *              CancelAlarm is called.
 * @verify The target task runs repeatedly with the configured cycle and no
 *         further activations occur after cancellation.
 */
void test_Os_cyclic_alarm_rearms_and_cancel_alarm_stops_future_activations(void)
{
    const Os_TaskConfigType cfg[] = {
        { "AlarmTask", task_alarm_target, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmCycle", TASK_LOW, 9u, 1u, 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, SetRelAlarm(ALARM_ACTIVATE, 1u, 2u));
    Os_TestAdvanceCounter(1u);
    TEST_ASSERT_EQUAL_STRING("A", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, alarm_target_runs);
    TEST_ASSERT_EQUAL(E_OK, GetAlarm(ALARM_ACTIVATE, &alarm_remaining_ticks));
    TEST_ASSERT_EQUAL_UINT32(2u, alarm_remaining_ticks);

    Os_TestAdvanceCounter(2u);
    TEST_ASSERT_EQUAL_STRING("AA", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, alarm_target_runs);

    alarm_cancel_status = CancelAlarm(ALARM_ACTIVATE);
    TEST_ASSERT_EQUAL(E_OK, alarm_cancel_status);
    Os_TestAdvanceCounter(3u);
    TEST_ASSERT_EQUAL_STRING("AA", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, alarm_target_runs);
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, GetAlarm(ALARM_ACTIVATE, &alarm_remaining_ticks));
}

/**
 * @spec OSEK OS 2.2.3 §13.6
 * @requirement An absolute alarm shall expire when the counter reaches the
 *              configured absolute tick, including after wrap-around.
 * @verify The remaining time spans the wrap boundary and the task activates
 *         at the wrapped absolute tick.
 */
void test_Os_absolute_alarm_wraps_over_counter_boundary(void)
{
    const Os_TaskConfigType cfg[] = {
        { "AlarmTask", task_alarm_target, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmAbs", TASK_LOW, 5u, 1u, 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    Os_TestAdvanceCounter(4u);
    alarm_set_abs_status = SetAbsAlarm(ALARM_ACTIVATE, 1u, 0u);
    alarm_get_status = GetAlarm(ALARM_ACTIVATE, &alarm_remaining_before_wrap);

    TEST_ASSERT_EQUAL(E_OK, alarm_set_abs_status);
    TEST_ASSERT_EQUAL(E_OK, alarm_get_status);
    TEST_ASSERT_EQUAL_UINT32(3u, alarm_remaining_before_wrap);

    Os_TestAdvanceCounter(2u);
    TEST_ASSERT_EQUAL_STRING("", execution_log);
    TEST_ASSERT_EQUAL(E_OK, GetAlarm(ALARM_ACTIVATE, &alarm_remaining_after_ticks));
    TEST_ASSERT_EQUAL_UINT32(1u, alarm_remaining_after_ticks);

    Os_TestAdvanceCounter(1u);
    TEST_ASSERT_EQUAL_STRING("A", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, alarm_target_runs);
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
}

/**
 * @spec OSEK OS 2.2.3 §13.6
 * @requirement Alarm services shall reject invalid parameters and attempts
 *              to re-arm an already active alarm.
 * @verify Invalid increments return E_OS_VALUE and a second SetRelAlarm on
 *         an active alarm returns E_OS_STATE.
 */
void test_Os_alarm_api_rejects_invalid_increment_and_active_alarm(void)
{
    const Os_TaskConfigType cfg[] = {
        { "AlarmTask", task_alarm_target, 2u, 1u, 0u, FALSE, FULL }
    };
    const Os_AlarmConfigType alarm_cfg[] = {
        { "AlarmGuard", TASK_LOW, 9u, 1u, 2u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureAlarms(alarm_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    alarm_set_rel_status = SetRelAlarm(ALARM_ACTIVATE, 0u, 0u);
    alarm_second_set_status = SetRelAlarm(ALARM_ACTIVATE, 1u, 0u);
    alarm_cancel_status = SetRelAlarm(ALARM_ACTIVATE, 1u, 0u);

    TEST_ASSERT_EQUAL(E_OS_VALUE, alarm_set_rel_status);
    TEST_ASSERT_EQUAL(E_OK, alarm_second_set_status);
    TEST_ASSERT_EQUAL(E_OS_STATE, alarm_cancel_status);
    TEST_ASSERT_EQUAL(E_OS_STATE, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(2u, error_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.2
 * @requirement A non-preemptive task shall continue to run until it yields
 *              or terminates, even if a higher-priority task becomes ready.
 * @verify Higher-priority activation is deferred until the NON task
 *         terminates.
 */
void test_Os_non_preemptive_task_defers_higher_priority_activation_until_termination(void)
{
    const Os_TaskConfigType cfg[] = {
        { "NonLow", task_low_non_preemptive, 3u, 1u, 0u, FALSE, NON },
        { "NonHigh", task_high_non_preemptive, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("NnH", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, non_preempt_low_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, non_preempt_high_runs);
    TEST_ASSERT_EQUAL(E_OK, non_preempt_activate_status);
    TEST_ASSERT_EQUAL(E_OK, non_preempt_low_term_status);
    TEST_ASSERT_EQUAL(E_OK, non_preempt_high_term_status);
}

/**
 * @spec OSEK OS 2.2.3 §13.2, §13.3
 * @requirement Category 2 ISR activation shall not cause task preemption
 *              while a NON task is still running.
 * @verify The ISR readies the higher-priority task, but it runs only after
 *         the NON task completes.
 */
void test_Os_cat2_isr_activation_does_not_preempt_non_preemptive_task(void)
{
    const Os_TaskConfigType cfg[] = {
        { "NonIsrLow", task_low_non_preemptive_isr, 3u, 1u, 0u, FALSE, NON },
        { "NonIsrHigh", task_high_non_preemptive, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("LIlH", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, non_preempt_isr_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, non_preempt_high_runs);
    TEST_ASSERT_EQUAL(E_OK, non_preempt_isr_invoke_status);
    TEST_ASSERT_EQUAL(E_OK, non_preempt_isr_activate_status);
    TEST_ASSERT_EQUAL(E_OK, non_preempt_isr_low_term_status);
    TEST_ASSERT_EQUAL(E_OK, non_preempt_high_term_status);
}

/**
 * @spec OSEK OS 2.2.3 §13.2
 * @requirement Schedule shall allow a non-preemptive task to yield to a
 *              higher-priority ready task.
 * @verify The higher-priority task runs during Schedule and control then
 *         returns to the NON task.
 */
void test_Os_schedule_from_non_preemptive_task_dispatches_higher_priority_task(void)
{
    const Os_TaskConfigType cfg[] = {
        { "NonSchedLow", task_non_preemptive_schedule, 3u, 1u, 0u, FALSE, NON },
        { "NonSchedHigh", task_high_non_preemptive, 1u, 1u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 2u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("SHs", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, non_schedule_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, non_preempt_high_runs);
    TEST_ASSERT_EQUAL(E_OK, non_schedule_activate_status);
    TEST_ASSERT_EQUAL(E_OK, non_schedule_status);
    TEST_ASSERT_EQUAL(E_OK, non_schedule_term_status);
    TEST_ASSERT_EQUAL(E_OK, non_preempt_high_term_status);
}

/**
 * @spec OSEK OS 2.2.3 §13.2, §13.4
 * @requirement Schedule shall not be accepted while the current task still
 *              occupies a resource.
 * @verify Schedule returns E_OS_RESOURCE until the resource is released.
 */
void test_Os_schedule_from_non_preemptive_task_rejects_held_resource(void)
{
    const Os_TaskConfigType cfg[] = {
        { "NonSchedRes", task_non_preemptive_schedule_with_resource, 2u, 1u, 0u, FALSE, NON }
    };
    const Os_ResourceConfigType res_cfg[] = {
        { "ResShared", 1u }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureResources(res_cfg, 1u));
    Os_TestSetErrorHook(error_hook);
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("Rr", execution_log);
    TEST_ASSERT_EQUAL_UINT8(1u, non_schedule_runs);
    TEST_ASSERT_EQUAL(E_OK, non_schedule_resource_get_status);
    TEST_ASSERT_EQUAL(E_OS_RESOURCE, non_schedule_resource_status);
    TEST_ASSERT_EQUAL(E_OK, non_schedule_resource_release_status);
    TEST_ASSERT_EQUAL(E_OK, non_schedule_resource_term_status);
    TEST_ASSERT_EQUAL(E_OS_RESOURCE, error_hook_status);
    TEST_ASSERT_EQUAL_UINT8(1u, error_hook_count);
}

/**
 * @spec OSEK OS 2.2.3 §13.2.3
 * @requirement A task with activation count greater than one shall be able
 *              to queue multiple activations.
 * @verify Two activations before dispatch cause the task to run twice.
 */
void test_Os_multiple_activations_run_task_multiple_times_when_limit_allows(void)
{
    const Os_TaskConfigType cfg[] = {
        { "Multi", task_multiple_activation_target, 2u, 2u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    multi_first_activate_status = ActivateTask(TASK_LOW);
    multi_second_activate_status = ActivateTask(TASK_LOW);

    TEST_ASSERT_EQUAL(E_OK, multi_first_activate_status);
    TEST_ASSERT_EQUAL(E_OK, multi_second_activate_status);
    TEST_ASSERT_EQUAL_UINT8(2u, Os_TestGetPendingActivations(TASK_LOW));

    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("MM", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, multi_activation_runs);
    TEST_ASSERT_EQUAL(E_OK, multi_term_status);
}

/**
 * @spec OSEK OS 2.2.3 §13.2.3
 * @requirement A running task may queue one more activation of itself when
 *              its activation limit allows it.
 * @verify Self-activation during the first run causes a second run after the
 *         first termination.
 */
void test_Os_self_activation_queues_another_run_when_limit_allows(void)
{
    const Os_TaskConfigType cfg[] = {
        { "SelfMulti", task_self_activate_target, 2u, 2u, 0u, FALSE, FULL }
    };

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(cfg, 1u));
    StartOS(OSDEFAULTAPPMODE);

    TEST_ASSERT_EQUAL(E_OK, ActivateTask(TASK_LOW));
    TEST_ASSERT_EQUAL(E_OK, Schedule());
    TEST_ASSERT_EQUAL_STRING("XY", execution_log);
    TEST_ASSERT_EQUAL_UINT8(2u, self_activation_runs);
    TEST_ASSERT_EQUAL(E_OK, self_activate_status);
    TEST_ASSERT_EQUAL(E_OK, self_term_status);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Os_activate_task_before_start_reports_state_error);
    RUN_TEST(test_Os_startos_runs_autostart_tasks_by_priority);
    RUN_TEST(test_Os_startup_and_task_hooks_wrap_each_dispatched_task);
    RUN_TEST(test_Os_schedule_dispatches_ready_tasks_from_idle_context);
    RUN_TEST(test_Os_test_run_to_idle_drains_ready_queue_in_host_bootstrap);
    RUN_TEST(test_Os_test_complete_port_dispatches_is_nofunc_in_plain_host_build);
    RUN_TEST(test_Os_test_run_to_idle_executes_chain_task_successor_sequence);
    RUN_TEST(test_Os_test_run_to_idle_processes_queued_multiple_activations_until_idle);
    RUN_TEST(test_Os_test_run_to_idle_returns_nofunc_when_system_is_already_idle);
    RUN_TEST(test_Os_test_run_to_idle_processes_cat2_isr_preemption_sequence);
    RUN_TEST(test_Os_test_run_to_idle_resumes_waiting_task_after_notifier_activation);
    RUN_TEST(test_Os_test_run_to_idle_drains_sender_and_receiver_ioc_sequence);
    RUN_TEST(test_Os_same_priority_tasks_run_fifo_order);
    RUN_TEST(test_Os_get_application_id_reports_owner_of_running_task);
    RUN_TEST(test_Os_check_task_memory_access_allows_owned_application_region);
    RUN_TEST(test_Os_check_task_memory_access_denies_foreign_region_and_invalid_task);
    RUN_TEST(test_Os_check_task_memory_access_allows_trusted_application_any_region);
    RUN_TEST(test_Os_activation_limit_is_enforced);
    RUN_TEST(test_Os_chain_task_queues_successor_after_current_completion);
    RUN_TEST(test_Os_get_task_state_reports_running_while_task_executes);
    RUN_TEST(test_Os_schedule_from_running_task_is_rejected_in_current_milestone);
    RUN_TEST(test_Os_app_mode_mask_controls_autostart);
    RUN_TEST(test_Os_shutdown_hook_observes_shutdown_error);
    RUN_TEST(test_Os_application_object_ownership_and_access_follow_configuration);
    RUN_TEST(test_Os_trusted_function_allows_authorized_application);
    RUN_TEST(test_Os_trusted_function_rejects_unauthorized_application);
    RUN_TEST(test_Os_application_access_denies_activate_task_on_foreign_task);
    RUN_TEST(test_Os_application_access_denies_get_resource_on_foreign_resource);
    RUN_TEST(test_Os_application_access_denies_set_event_on_foreign_task);
    RUN_TEST(test_Os_application_access_denies_set_rel_alarm_on_foreign_alarm);
    RUN_TEST(test_Os_ioc_queue_transfers_fifo_data_between_applications);
    RUN_TEST(test_Os_ioc_receive_on_empty_queue_returns_no_data);
    RUN_TEST(test_Os_ioc_send_rejects_queue_overflow_with_limit_error);
    RUN_TEST(test_Os_application_access_denies_ioc_send_on_foreign_channel);
    RUN_TEST(test_Os_stack_monitor_records_peak_usage_within_budget);
    RUN_TEST(test_Os_stack_monitor_detects_budget_violation_and_requests_shutdown);
    RUN_TEST(test_Os_activate_task_with_invalid_id_returns_e_os_id);
    RUN_TEST(test_Os_get_task_id_with_null_pointer_returns_e_os_value);
    RUN_TEST(test_Os_higher_priority_activation_preempts_running_task_immediately);
    RUN_TEST(test_Os_cat2_isr_exit_dispatches_higher_priority_task_before_return);
    RUN_TEST(test_Os_resource_ceiling_defers_preemption_until_release);
    RUN_TEST(test_Os_nested_resources_must_release_in_lifo_order);
    RUN_TEST(test_Os_terminate_task_rejects_held_resource_with_e_os_resource);
    RUN_TEST(test_Os_chain_task_rejects_held_resource_with_e_os_resource);
    RUN_TEST(test_Os_wait_event_blocks_extended_task_until_set_event);
    RUN_TEST(test_Os_wait_event_rejects_basic_task_with_e_os_access);
    RUN_TEST(test_Os_wait_event_rejects_held_resource_with_e_os_resource);
    RUN_TEST(test_Os_cat2_isr_set_event_dispatches_waiting_task_on_exit);
    RUN_TEST(test_Os_relative_alarm_reports_remaining_ticks_and_activates_task);
    RUN_TEST(test_Os_cyclic_alarm_rearms_and_cancel_alarm_stops_future_activations);
    RUN_TEST(test_Os_absolute_alarm_wraps_over_counter_boundary);
    RUN_TEST(test_Os_alarm_api_rejects_invalid_increment_and_active_alarm);
    RUN_TEST(test_Os_non_preemptive_task_defers_higher_priority_activation_until_termination);
    RUN_TEST(test_Os_cat2_isr_activation_does_not_preempt_non_preemptive_task);
    RUN_TEST(test_Os_schedule_from_non_preemptive_task_dispatches_higher_priority_task);
    RUN_TEST(test_Os_schedule_from_non_preemptive_task_rejects_held_resource);
    RUN_TEST(test_Os_multiple_activations_run_task_multiple_times_when_limit_allows);
    RUN_TEST(test_Os_self_activation_queues_another_run_when_limit_allows);

    return UNITY_END();
}
