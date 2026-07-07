/**
 * @file    test_Os_ConfigValidation.c
 * @brief   Unit tests for the production OS configuration path (S-OS-10)
 * @date    2026-07-07
 *
 * @details TDD suite for `Os_Configure(const Os_ConfigType*)` — the
 *          non-UNIT_TEST config injection entry — covering the three
 *          open OS-FMEA findings:
 *          - OS-T-05: null task entry rejected at init (E_OS_VALUE)
 *          - OS-R-04: resource ceiling lower (numerically greater) than
 *            the highest priority of any accessor task rejected
 *            (E_OS_RESOURCE); out-of-range ceiling rejected (E_OS_VALUE)
 *          - OS-C-03: missing idle task rejected (E_OS_NOFUNC). Idle
 *            task convention: Priority == OS_MAX_PRIORITIES-1 (lowest
 *            priority; this kernel treats numerically lower as higher),
 *            autostarted (AutostartMask != 0), Schedule == FULL.
 *          Plus a happy-path full-config boot and a refuse-to-start
 *          check after a rejected configuration.
 *
 * @verifies OS-FMEA OS-T-05, OS-R-04, OS-C-03 (docs/safety/analysis/os-fmea.md)
 */
#include "unity.h"
#include "Det.h"
#include "Os.h"
#include "Os_Cfg_Types.h"

#define CFG_TASK_SAFETY   0u
#define CFG_TASK_WORK     1u
#define CFG_TASK_IDLE     2u
#define CFG_IDLE_PRIORITY ((uint8)(OS_MAX_PRIORITIES - 1u))

static uint8 safety_runs;
static uint8 work_runs;
static uint8 idle_runs;

static void Task_Safety_Entry(void) { safety_runs++; (void)TerminateTask(); }
static void Task_Work_Entry(void)   { work_runs++;   (void)TerminateTask(); }
static void Task_Idle_Entry(void)   { idle_runs++;   (void)TerminateTask(); }

static StatusType Trusted_Echo(TrustedFunctionParameterRefType Params)
{
    (void)Params;
    return E_OK;
}

/* --- Reference (valid) per-area tables ---------------------------------- */

static const Os_TaskConfigType valid_tasks[] = {
    { "TaskSafety", Task_Safety_Entry, 1u, 1u, 0x01u, FALSE, FULL },
    { "TaskWork",   Task_Work_Entry,   2u, 1u, 0x01u, FALSE, FULL },
    { "TaskIdle",   Task_Idle_Entry,   CFG_IDLE_PRIORITY, 1u, 0x01u, FALSE, FULL }
};

/* Ceiling 1 == highest accessor priority (TaskSafety) — valid PCP config. */
static const Os_ResourceConfigType valid_resources[] = {
    { "ResComm", 1u }
};

static const Os_AlarmConfigType valid_alarms[] = {
    { "AlarmWork", CFG_TASK_WORK, 1000u, 1u, 1u }
};

static const Os_ExpiryPointConfigType valid_expiry_points[] = {
    { 5u, CFG_TASK_WORK, 0u }
};

static const Os_ScheduleTableConfigType valid_sched_tables[] = {
    { "StMain", 10u, TRUE, valid_expiry_points, 1u }
};

static const Os_ApplicationConfigType valid_applications[] = {
    { "AppSys", TRUE, 0x07u, 0x07u, 0x01u, 0x01u, 0x01u, 0x01u, 0x01u, 0x01u }
};

static const Os_TrustedFunctionConfigType valid_trusted_functions[] = {
    { "TfEcho", Trusted_Echo, 0x01u }
};

static const Os_IocConfigType valid_iocs[] = {
    { "Ioc0", 2u }
};

static const Os_StackMonitorConfigType valid_stacks[] = {
    { CFG_TASK_SAFETY, 256u },
    { CFG_TASK_WORK,   256u },
    { CFG_TASK_IDLE,   256u }
};

static const Os_MemoryRegionConfigType valid_memory_regions[] = {
    { "RegionSys", 0u, (MemoryStartAddressType)0x1000u, 64u }
};

/* Physical task-stack storage (S-OS-31 launch seam) — same shape the
 * generated Os_Cfg_<Ecu>.c emits (static, 8-byte aligned). */
static uint8 stack_safety[256] __attribute__((aligned(8)));
static uint8 stack_work[256]   __attribute__((aligned(8)));
static uint8 stack_idle[256]   __attribute__((aligned(8)));

static const Os_TaskStackConfigType valid_task_stacks[] = {
    { CFG_TASK_SAFETY, stack_safety, 256u },
    { CFG_TASK_WORK,   stack_work,   256u },
    { CFG_TASK_IDLE,   stack_idle,   256u }
};

static Os_ConfigType make_valid_config(void)
{
    Os_ConfigType cfg;

    cfg.Tasks = valid_tasks;
    cfg.TaskCount = 3u;
    cfg.Resources = valid_resources;
    cfg.ResourceCount = 1u;
    cfg.Alarms = valid_alarms;
    cfg.AlarmCount = 1u;
    cfg.ScheduleTables = valid_sched_tables;
    cfg.ScheduleTableCount = 1u;
    cfg.Applications = valid_applications;
    cfg.ApplicationCount = 1u;
    cfg.TrustedFunctions = valid_trusted_functions;
    cfg.TrustedFunctionCount = 1u;
    cfg.Iocs = valid_iocs;
    cfg.IocCount = 1u;
    cfg.Stacks = valid_stacks;
    cfg.StackCount = 3u;
    cfg.MemoryRegions = valid_memory_regions;
    cfg.MemoryRegionCount = 1u;
    cfg.MemProtTasks = NULL_PTR;
    cfg.MemProtTaskCount = 0u;
    cfg.TaskStacks = valid_task_stacks;
    cfg.TaskStackCount = 3u;
    return cfg;
}

void setUp(void)
{
    Os_TestReset();
    safety_runs = 0u;
    work_runs = 0u;
    idle_runs = 0u;
}

void tearDown(void) {}

/**
 * @requirement Os_Configure shall reject a null config pointer.
 */
void test_Configure_NullConfig_ReturnsE_OS_VALUE(void)
{
    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(NULL_PTR));
}

/**
 * @verifies OS-T-05
 * @requirement Os_Configure shall reject a configuration containing a
 *              task whose entry function pointer is NULL (E_OS_VALUE).
 */
void test_Configure_NullTaskEntry_Rejected(void)
{
    Os_TaskConfigType bad_tasks[3];
    Os_ConfigType cfg = make_valid_config();

    bad_tasks[0] = valid_tasks[0];
    bad_tasks[1] = valid_tasks[1];
    bad_tasks[2] = valid_tasks[2];
    bad_tasks[1].Entry = (Os_TaskEntryType)0;
    cfg.Tasks = bad_tasks;

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));
}

/**
 * @verifies OS-R-04
 * @requirement With no OS-Applications configured every task is a
 *              potential accessor: a ceiling numerically greater than
 *              the highest-priority task's priority shall be rejected
 *              with E_OS_RESOURCE.
 */
void test_Configure_CeilingBelowAccessor_NoApps_Rejected(void)
{
    /* Ceiling 5 vs accessor TaskSafety priority 1 (1 = higher priority
     * in this kernel): PCP could not block TaskSafety — misconfigured. */
    static const Os_ResourceConfigType bad_resources[] = {
        { "ResComm", 5u }
    };
    Os_ConfigType cfg = make_valid_config();

    cfg.Resources = bad_resources;
    cfg.Applications = NULL_PTR;
    cfg.ApplicationCount = 0u;
    /* Memory regions and trusted functions require applications. */
    cfg.MemoryRegions = NULL_PTR;
    cfg.MemoryRegionCount = 0u;

    TEST_ASSERT_EQUAL(E_OS_RESOURCE, Os_Configure(&cfg));
}

/**
 * @verifies OS-R-04
 * @requirement With OS-Applications configured, only tasks owned by an
 *              application that owns or may access the resource count
 *              as accessors; a ceiling above (numerically) any accessor
 *              priority shall be rejected with E_OS_RESOURCE.
 */
void test_Configure_CeilingBelowAccessor_AppScoped_Rejected(void)
{
    /* AppSys owns all three tasks and the resource; ceiling 3 is
     * numerically greater than accessor TaskSafety's priority 1. */
    static const Os_ResourceConfigType bad_resources[] = {
        { "ResComm", 3u }
    };
    Os_ConfigType cfg = make_valid_config();

    cfg.Resources = bad_resources;

    TEST_ASSERT_EQUAL(E_OS_RESOURCE, Os_Configure(&cfg));
}

/**
 * @verifies OS-R-04
 * @requirement A ceiling outside the priority range (>= OS_MAX_PRIORITIES)
 *              shall be rejected with E_OS_VALUE (per-area range check).
 */
void test_Configure_CeilingOutOfRange_ReturnsE_OS_VALUE(void)
{
    static const Os_ResourceConfigType bad_resources[] = {
        { "ResComm", OS_MAX_PRIORITIES }
    };
    Os_ConfigType cfg = make_valid_config();

    cfg.Resources = bad_resources;

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));
}

/**
 * @verifies OS-C-03
 * @requirement Os_Configure shall reject a configuration with no idle
 *              task (no task at priority OS_MAX_PRIORITIES-1) with
 *              E_OS_NOFUNC.
 */
void test_Configure_MissingIdleTask_Rejected(void)
{
    static const Os_TaskConfigType no_idle_tasks[] = {
        { "TaskSafety", Task_Safety_Entry, 1u, 1u, 0x01u, FALSE, FULL },
        { "TaskWork",   Task_Work_Entry,   2u, 1u, 0x01u, FALSE, FULL }
    };
    Os_ConfigType cfg = make_valid_config();

    cfg.Tasks = no_idle_tasks;
    cfg.TaskCount = 2u;
    cfg.Stacks = NULL_PTR;
    cfg.StackCount = 0u;
    /* Stacks reference task 2 (removed here) — drop them like the
     * budget table so the idle check is what rejects this config. */
    cfg.TaskStacks = NULL_PTR;
    cfg.TaskStackCount = 0u;

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Configure(&cfg));
}

/**
 * @verifies OS-C-03
 * @requirement A lowest-priority task that is not autostarted in any
 *              application mode does not satisfy the idle-task
 *              requirement (it would never run without activation).
 */
void test_Configure_IdleTaskNotAutostarted_Rejected(void)
{
    Os_TaskConfigType bad_tasks[3];
    Os_ConfigType cfg = make_valid_config();

    bad_tasks[0] = valid_tasks[0];
    bad_tasks[1] = valid_tasks[1];
    bad_tasks[2] = valid_tasks[2];
    bad_tasks[2].AutostartMask = 0u;
    cfg.Tasks = bad_tasks;

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Configure(&cfg));
}

/**
 * @verifies OS-C-03
 * @requirement A non-preemptable (NON) lowest-priority task does not
 *              satisfy the idle-task requirement: a never-terminating
 *              NON idle task would starve every other task.
 */
void test_Configure_IdleTaskNonPreemptive_Rejected(void)
{
    Os_TaskConfigType bad_tasks[3];
    Os_ConfigType cfg = make_valid_config();

    bad_tasks[0] = valid_tasks[0];
    bad_tasks[1] = valid_tasks[1];
    bad_tasks[2] = valid_tasks[2];
    bad_tasks[2].Schedule = NON;
    cfg.Tasks = bad_tasks;

    TEST_ASSERT_EQUAL(E_OS_NOFUNC, Os_Configure(&cfg));
}

/**
 * @requirement Task-stack entries (S-OS-31 launch seam) shall be rejected
 *              with E_OS_VALUE when the base address is not 8-byte
 *              aligned (AAPCS port frame alignment).
 */
void test_Configure_TaskStack_MisalignedBase_Rejected(void)
{
    static const Os_TaskStackConfigType bad_stacks[] = {
        { CFG_TASK_SAFETY, &stack_safety[4], 248u },
        { CFG_TASK_WORK,   stack_work,       256u },
        { CFG_TASK_IDLE,   stack_idle,       256u }
    };
    Os_ConfigType cfg = make_valid_config();

    cfg.TaskStacks = bad_stacks;

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));
}

/**
 * @requirement Task-stack entries with zero size, or a size that is not a
 *              multiple of 8, shall be rejected with E_OS_VALUE.
 */
void test_Configure_TaskStack_BadSize_Rejected(void)
{
    static const Os_TaskStackConfigType zero_stacks[] = {
        { CFG_TASK_SAFETY, stack_safety, 0u },
        { CFG_TASK_WORK,   stack_work,   256u },
        { CFG_TASK_IDLE,   stack_idle,   256u }
    };
    static const Os_TaskStackConfigType odd_stacks[] = {
        { CFG_TASK_SAFETY, stack_safety, 252u },
        { CFG_TASK_WORK,   stack_work,   256u },
        { CFG_TASK_IDLE,   stack_idle,   256u }
    };
    Os_ConfigType cfg = make_valid_config();

    cfg.TaskStacks = zero_stacks;
    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));

    cfg = make_valid_config();
    cfg.TaskStacks = odd_stacks;
    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));
}

/**
 * @requirement A task stack smaller than the task's configured monitor
 *              budget shall be rejected with E_OS_VALUE (the monitor
 *              budget must fit inside the physical storage).
 */
void test_Configure_TaskStack_SizeBelowBudget_Rejected(void)
{
    static const Os_TaskStackConfigType small_stacks[] = {
        { CFG_TASK_SAFETY, stack_safety, 128u },   /* budget is 256 */
        { CFG_TASK_WORK,   stack_work,   256u },
        { CFG_TASK_IDLE,   stack_idle,   256u }
    };
    Os_ConfigType cfg = make_valid_config();

    cfg.TaskStacks = small_stacks;

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));
}

/**
 * @requirement Task-stack entries referencing an invalid task, a NULL
 *              base, or a NULL table with non-zero count shall be
 *              rejected with E_OS_VALUE.
 */
void test_Configure_TaskStack_InvalidReference_Rejected(void)
{
    static const Os_TaskStackConfigType bad_task_stacks[] = {
        { 7u, stack_safety, 256u },
        { CFG_TASK_WORK, stack_work, 256u },
        { CFG_TASK_IDLE, stack_idle, 256u }
    };
    static const Os_TaskStackConfigType null_base_stacks[] = {
        { CFG_TASK_SAFETY, NULL_PTR, 256u },
        { CFG_TASK_WORK,   stack_work, 256u },
        { CFG_TASK_IDLE,   stack_idle, 256u }
    };
    Os_ConfigType cfg = make_valid_config();

    cfg.TaskStacks = bad_task_stacks;
    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));

    cfg = make_valid_config();
    cfg.TaskStacks = null_base_stacks;
    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));

    cfg = make_valid_config();
    cfg.TaskStacks = NULL_PTR;
    cfg.TaskStackCount = 3u;
    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));
}

/**
 * @requirement On host (no hardware platform define) a configuration
 *              WITHOUT a task-stack table remains valid: host-cooperative
 *              dispatch needs no physical task stacks. (The per-platform
 *              completeness check is verified in the STM32 port suite
 *              test_Os_Port_Stm32_bootstrap_production_launch.c.)
 */
void test_Configure_NoTaskStacks_AcceptedOnHost(void)
{
    Os_ConfigType cfg = make_valid_config();

    cfg.TaskStacks = NULL_PTR;
    cfg.TaskStackCount = 0u;

    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));
}

/**
 * @requirement After a rejected configuration the kernel shall hold no
 *              runnable configuration: StartOS must dispatch nothing
 *              (refuse-to-start, fail-closed).
 */
void test_Configure_FailedConfig_RefusesToStart(void)
{
    Os_TaskConfigType bad_tasks[3];
    Os_ConfigType cfg = make_valid_config();

    bad_tasks[0] = valid_tasks[0];
    bad_tasks[1] = valid_tasks[1];
    bad_tasks[2] = valid_tasks[2];
    bad_tasks[0].Entry = (Os_TaskEntryType)0;
    cfg.Tasks = bad_tasks;

    TEST_ASSERT_EQUAL(E_OS_VALUE, Os_Configure(&cfg));

    StartOS(OSDEFAULTAPPMODE);
    (void)Os_TestRunToIdle();

    TEST_ASSERT_EQUAL_UINT32(0u, Os_TestGetDispatchCount());
    TEST_ASSERT_EQUAL_UINT8(0u, safety_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, work_runs);
    TEST_ASSERT_EQUAL_UINT8(0u, idle_runs);
}

/**
 * @requirement A full valid configuration (tasks incl. idle, resource,
 *              alarm, schedule table, application, trusted function,
 *              IOC, stacks, memory regions) shall be accepted and the
 *              kernel shall boot: StartOS runs every autostart task.
 */
void test_Configure_FullConfig_BootsAndRunsAutostartTasks(void)
{
    Os_ConfigType cfg = make_valid_config();

    TEST_ASSERT_EQUAL(E_OK, Os_Configure(&cfg));

    StartOS(OSDEFAULTAPPMODE);
    (void)Os_TestRunToIdle();

    TEST_ASSERT_EQUAL_UINT8(1u, safety_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, work_runs);
    TEST_ASSERT_EQUAL_UINT8(1u, idle_runs);
    TEST_ASSERT_EQUAL_UINT32(3u, Os_TestGetDispatchCount());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_Configure_NullConfig_ReturnsE_OS_VALUE);
    RUN_TEST(test_Configure_NullTaskEntry_Rejected);
    RUN_TEST(test_Configure_CeilingBelowAccessor_NoApps_Rejected);
    RUN_TEST(test_Configure_CeilingBelowAccessor_AppScoped_Rejected);
    RUN_TEST(test_Configure_CeilingOutOfRange_ReturnsE_OS_VALUE);
    RUN_TEST(test_Configure_MissingIdleTask_Rejected);
    RUN_TEST(test_Configure_IdleTaskNotAutostarted_Rejected);
    RUN_TEST(test_Configure_IdleTaskNonPreemptive_Rejected);
    RUN_TEST(test_Configure_TaskStack_MisalignedBase_Rejected);
    RUN_TEST(test_Configure_TaskStack_BadSize_Rejected);
    RUN_TEST(test_Configure_TaskStack_SizeBelowBudget_Rejected);
    RUN_TEST(test_Configure_TaskStack_InvalidReference_Rejected);
    RUN_TEST(test_Configure_NoTaskStacks_AcceptedOnHost);
    RUN_TEST(test_Configure_FailedConfig_RefusesToStart);
    RUN_TEST(test_Configure_FullConfig_BootsAndRunsAutostartTasks);
    return UNITY_END();
}
