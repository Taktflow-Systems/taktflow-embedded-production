/**
 * @file    test_Os_Port_Stm32_bootstrap_production_launch.c
 * @brief   Production StartOS first-task launch tests for the STM32 port
 * @date    2026-07-07
 *
 * @details S-OS-31 first-task launch seam: StartOS on PLATFORM_STM32 must
 *          prepare every configured task's initial PSP frame from the
 *          configured task-stack table (Os_ConfigType.TaskStacks) via the
 *          task-binding seam, then launch the highest-priority autostart
 *          task through Os_PortStartFirstTask — the mechanism validated
 *          on hardware by the 6/6 bringup line (Os_Port_Stm32_Bringup.c
 *          BRINGUP-2/5/6). Host build: the UNIT_TEST launch mock marks
 *          FirstTaskStarted without switching context.
 *
 *          The configuration mirrors the generated Os_Cfg_<Ecu>.c shape:
 *          period-group basic tasks + autostarted idle, one repeating
 *          schedule table per period group, stack budgets, task stacks.
 *
 * @verifies S-OS-31 launch seam (docs/plans/os-migration-baseline.md)
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
#define INITIAL_EXC_RETURN     0xFFFFFFFDu
#define INITIAL_TASK_LR        0xFFFFFFFFu
#define XPSR_THUMB             0x01000000u

#define TASK_1MS               ((TaskType)0u)
#define TASK_10MS              ((TaskType)1u)
#define TASK_100MS             ((TaskType)2u)
#define TASK_IDLE              ((TaskType)3u)
#define TASK_COUNT             4u
#define IDLE_PRIORITY          ((uint8)(OS_MAX_PRIORITIES - 1u))

#define TABLE_1MS              ((ScheduleTableType)0u)

#define STACK_SIZE             256u

static uint8 runs_1ms;
static uint8 runs_10ms;
static uint8 runs_100ms;
static uint8 runs_idle;

static void Task_1ms_Entry(void)   { runs_1ms++; }
static void Task_10ms_Entry(void)  { runs_10ms++; }
static void Task_100ms_Entry(void) { runs_100ms++; }
static void Task_Idle_Entry(void)  { runs_idle++; }

/* Generated-shape task table: period groups + autostarted idle
 * (Os_Cfg.c.j2 emits exactly this pattern). */
static const Os_TaskConfigType prod_tasks[TASK_COUNT] = {
    { "Prod_1ms",   Task_1ms_Entry,   0u, 1u, 0u, FALSE, FULL },
    { "Prod_10ms",  Task_10ms_Entry,  1u, 1u, 0u, FALSE, FULL },
    { "Prod_100ms", Task_100ms_Entry, 2u, 1u, 0u, FALSE, FULL },
    { "Prod_Idle",  Task_Idle_Entry,  IDLE_PRIORITY, 1u, 1u, FALSE, FULL },
};

static const Os_ExpiryPointConfigType prod_ep_1ms[1] = {
    { 1u, TASK_1MS, 0u },
};
static const Os_ExpiryPointConfigType prod_ep_10ms[1] = {
    { 10u, TASK_10MS, 0u },
};
static const Os_ExpiryPointConfigType prod_ep_100ms[1] = {
    { 100u, TASK_100MS, 0u },
};

static const Os_ScheduleTableConfigType prod_tables[3] = {
    { "Prod_1ms",   1u,   TRUE, prod_ep_1ms,   1u },
    { "Prod_10ms",  10u,  TRUE, prod_ep_10ms,  1u },
    { "Prod_100ms", 100u, TRUE, prod_ep_100ms, 1u },
};

static const Os_StackMonitorConfigType prod_budgets[TASK_COUNT] = {
    { TASK_1MS,   STACK_SIZE },
    { TASK_10MS,  STACK_SIZE },
    { TASK_100MS, STACK_SIZE },
    { TASK_IDLE,  STACK_SIZE },
};

/* Physical stack storage — generated Os_Cfg_<Ecu>.c emits the same shape
 * (static, 8-byte aligned, one array per task). */
static uint8 prod_stack_1ms[STACK_SIZE]   __attribute__((aligned(8)));
static uint8 prod_stack_10ms[STACK_SIZE]  __attribute__((aligned(8)));
static uint8 prod_stack_100ms[STACK_SIZE] __attribute__((aligned(8)));
static uint8 prod_stack_idle[STACK_SIZE]  __attribute__((aligned(8)));

static const Os_TaskStackConfigType prod_task_stacks[TASK_COUNT] = {
    { TASK_1MS,   prod_stack_1ms,   STACK_SIZE },
    { TASK_10MS,  prod_stack_10ms,  STACK_SIZE },
    { TASK_100MS, prod_stack_100ms, STACK_SIZE },
    { TASK_IDLE,  prod_stack_idle,  STACK_SIZE },
};

static Os_ConfigType make_prod_config(void)
{
    Os_ConfigType cfg;

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.Tasks = prod_tasks;
    cfg.TaskCount = TASK_COUNT;
    cfg.ScheduleTables = prod_tables;
    cfg.ScheduleTableCount = 3u;
    cfg.Stacks = prod_budgets;
    cfg.StackCount = TASK_COUNT;
    cfg.TaskStacks = prod_task_stacks;
    cfg.TaskStackCount = TASK_COUNT;
    return cfg;
}

static uintptr_t stack_top(const uint8* base)
{
    return (uintptr_t)base + (uintptr_t)STACK_SIZE;
}

void setUp(void)
{
    runs_1ms = 0u;
    runs_10ms = 0u;
    runs_100ms = 0u;
    runs_idle = 0u;
    Os_TestReset();
    Os_PortTargetInit();
}

void tearDown(void)
{
}

/**
 * @requirement On PLATFORM_STM32, StartOS with a configured task-stack
 *              table shall launch the highest-priority autostart task
 *              through Os_PortStartFirstTask (BRINGUP-2 mechanism) —
 *              never by calling its entry inline on the boot (MSP) stack.
 * @verify After StartOS the port reports the first task started with the
 *         idle task (only autostart) as launch target; no entry ran
 *         inline; kernel and port agree on the current task.
 */
void test_StartOS_production_config_launches_first_task_via_port(void)
{
    Os_ConfigType cfg = make_prod_config();
    const Os_Port_Stm32_StateType* state;

    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));

    StartOS(OSDEFAULTAPPMODE);
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_TRUE(state->FirstTaskPrepared);
    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL_UINT32(1u, state->FirstTaskLaunchCount);
    TEST_ASSERT_EQUAL(TASK_IDLE, state->FirstTaskTaskID);
    TEST_ASSERT_EQUAL(TASK_IDLE, state->CurrentTask);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetDispatchCount());
    /* Launch is via exception return, not an inline call. */
    TEST_ASSERT_EQUAL_UINT8(0u, runs_idle);
    TEST_ASSERT_EQUAL_UINT8(0u, runs_1ms);
}

/**
 * @requirement StartOS shall prepare an initial PSP frame for EVERY
 *              configured task from its configured stack storage
 *              (task-binding seam Os_Port_PrepareConfiguredTask), so a
 *              later PendSV dispatch to any task restores a valid frame.
 * @verify Every task context is Prepared with StackTop at the top of its
 *         configured stack array.
 */
void test_StartOS_prepares_every_configured_task_context(void)
{
    Os_ConfigType cfg = make_prod_config();
    uint8 idx;

    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));
    StartOS(OSDEFAULTAPPMODE);

    for (idx = 0u; idx < TASK_COUNT; idx++) {
        const Os_Port_Stm32_TaskContextType* ctx =
            Os_Port_Stm32_GetTaskContext((TaskType)idx);

        TEST_ASSERT_NOT_NULL(ctx);
        TEST_ASSERT_TRUE(ctx->Prepared);
        TEST_ASSERT_EQUAL_PTR(
            (void*)stack_top(prod_task_stacks[idx].StackBase),
            (void*)ctx->StackTop);
    }
}

/**
 * @requirement The first-task frame built by StartOS shall follow the
 *              hardware-validated bringup convention: xPSR Thumb bit set,
 *              PC = task entry, poisoned task LR, EXC_RETURN 0xFFFFFFFD,
 *              PSP 8-byte aligned below the stack top.
 * @verify Frame words at FirstTaskPsp match the BRINGUP-2 layout.
 */
void test_StartOS_first_task_frame_matches_bringup_convention(void)
{
    Os_ConfigType cfg = make_prod_config();
    const Os_Port_Stm32_StateType* state;
    uintptr_t expected_psp;
    const uint32* frame;
    uint32 idx;

    (void)memset(prod_stack_idle, 0xA5, sizeof(prod_stack_idle));
    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));
    StartOS(OSDEFAULTAPPMODE);

    state = Os_Port_Stm32_GetBootstrapState();
    expected_psp = (uintptr_t)((stack_top(prod_stack_idle) -
                                (uintptr_t)FRAME_BYTES) & ~(uintptr_t)0x7u);
    TEST_ASSERT_EQUAL_PTR((void*)expected_psp, (void*)state->FirstTaskPsp);
    TEST_ASSERT_EQUAL_HEX32(XPSR_THUMB, state->InitialXpsr);

    frame = (const uint32*)state->FirstTaskPsp;
    for (idx = 0u; idx <= 7u; idx++) {
        TEST_ASSERT_EQUAL_HEX32(0u, frame[idx]);      /* R4-R11 */
    }
    TEST_ASSERT_EQUAL_HEX32(INITIAL_EXC_RETURN, frame[8]);
    for (idx = 9u; idx <= 13u; idx++) {
        TEST_ASSERT_EQUAL_HEX32(0u, frame[idx]);      /* R0-R3, R12 */
    }
    TEST_ASSERT_EQUAL_HEX32(INITIAL_TASK_LR, frame[14]);
    TEST_ASSERT_EQUAL_HEX32(
        (uint32)((uintptr_t)Task_Idle_Entry & 0xFFFFFFFFu), frame[15]);
    TEST_ASSERT_EQUAL_HEX32(XPSR_THUMB, frame[16]);
}

/**
 * @requirement StartOS shall launch the HIGHEST-PRIORITY autostart task
 *              (kernel convention: numerically lowest priority value),
 *              not simply the idle task.
 * @verify With the 10ms task additionally autostarted, it is selected as
 *         the first task; idle stays READY.
 */
void test_StartOS_selects_highest_priority_autostart_task(void)
{
    Os_TaskConfigType tasks[TASK_COUNT];
    Os_ConfigType cfg = make_prod_config();
    const Os_Port_Stm32_StateType* state;
    uint8 idx;

    for (idx = 0u; idx < TASK_COUNT; idx++) {
        tasks[idx] = prod_tasks[idx];
    }
    tasks[TASK_10MS].AutostartMask = 1u;   /* prio 1 beats idle prio 7 */
    cfg.Tasks = tasks;

    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));
    StartOS(OSDEFAULTAPPMODE);

    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_TRUE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL(TASK_10MS, state->FirstTaskTaskID);
    TEST_ASSERT_EQUAL(TASK_10MS, Os_TestGetCurrentTask());
}

/**
 * @requirement After the production launch, a counter tick that expires a
 *              schedule-table point for a higher-priority task shall stage
 *              a PendSV dispatch (BRINGUP-5 preemption path) — the entry
 *              must not run from ISR context.
 * @verify SysTick tick activates the 1ms task, PendSV is requested with
 *         the 1ms task selected; completing the dispatch runs the task
 *         once and returns to the idle task.
 */
void test_StartOS_tick_preemption_requests_pendsv_dispatch(void)
{
    Os_ConfigType cfg = make_prod_config();
    const Os_Port_Stm32_StateType* state;

    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));
    StartOS(OSDEFAULTAPPMODE);

    /* Production idle body starts the period tables from task context. */
    TEST_ASSERT_EQUAL(E_OK, StartScheduleTableAbs(TABLE_1MS, 0u));

    Os_Port_Stm32_SysTickHandler();
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_EQUAL_UINT32(1u, Os_TestGetCounterValue());
    TEST_ASSERT_TRUE(state->PendSvPending);
    TEST_ASSERT_EQUAL_UINT32(1u, state->PendSvRequestCount);
    TEST_ASSERT_EQUAL(TASK_1MS, state->SelectedNextTask);
    TEST_ASSERT_EQUAL_UINT8(0u, runs_1ms);   /* not called from ISR */

    TEST_ASSERT_EQUAL(E_OK, Os_TestRunToIdle());
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_EQUAL_UINT8(1u, runs_1ms);
    TEST_ASSERT_FALSE(state->PendSvPending);
    TEST_ASSERT_EQUAL(TASK_IDLE, Os_TestGetCurrentTask());
}

/**
 * @requirement On PLATFORM_STM32, Os_Configure shall reject (fail-closed,
 *              E_OS_VALUE) a configuration in which any task lacks a
 *              task-stack entry: without physical stack storage the port
 *              cannot build that task's dispatch frame.
 * @verify A task-stack table missing one task is rejected and the kernel
 *         refuses to start (no launch, nothing dispatched).
 */
void test_Os_Configure_rejects_task_without_stack_on_stm32(void)
{
    Os_ConfigType cfg = make_prod_config();
    const Os_Port_Stm32_StateType* state;

    cfg.TaskStackCount = 3u;   /* drops the idle-task stack entry */

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));

    StartOS(OSDEFAULTAPPMODE);
    state = Os_Port_Stm32_GetBootstrapState();
    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL_UINT32(0u, Os_TestGetDispatchCount());
    TEST_ASSERT_EQUAL_UINT8(0u, runs_idle);
}

/**
 * @requirement Configurations without a task-stack table (bringup and
 *              host-driven suites using Os_TestConfigure*) shall keep the
 *              pre-existing host-cooperative StartOS behavior unchanged.
 * @verify StartOS without task stacks does not launch through the port;
 *         autostart entries run inline as before.
 */
void test_StartOS_without_task_stacks_keeps_host_cooperative_dispatch(void)
{
    const Os_Port_Stm32_StateType* state;

    TEST_ASSERT_EQUAL(E_OK, Os_TestConfigureTasks(prod_tasks, TASK_COUNT));

    StartOS(OSDEFAULTAPPMODE);
    state = Os_Port_Stm32_GetBootstrapState();

    TEST_ASSERT_FALSE(state->FirstTaskStarted);
    TEST_ASSERT_EQUAL_UINT32(0u, state->FirstTaskLaunchCount);
    TEST_ASSERT_EQUAL_UINT8(1u, runs_idle);   /* legacy inline dispatch */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_StartOS_production_config_launches_first_task_via_port);
    RUN_TEST(test_StartOS_prepares_every_configured_task_context);
    RUN_TEST(test_StartOS_first_task_frame_matches_bringup_convention);
    RUN_TEST(test_StartOS_selects_highest_priority_autostart_task);
    RUN_TEST(test_StartOS_tick_preemption_requests_pendsv_dispatch);
    RUN_TEST(test_Os_Configure_rejects_task_without_stack_on_stm32);
    RUN_TEST(test_StartOS_without_task_stacks_keeps_host_cooperative_dispatch);
    return UNITY_END();
}
