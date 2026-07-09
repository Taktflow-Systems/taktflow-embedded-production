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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_select_next_task_rejects_invalid_frame);
    RUN_TEST(test_switchback_resume_into_stale_frame_fails_closed);
    RUN_TEST(test_switchback_resume_with_valid_frame_unaffected);
    return UNITY_END();
}
