/**
 * @file    test_Os_ScheduleTable.c
 * @brief   Unit tests for AUTOSAR OS Schedule Table services (§10)
 * @date    2026-03-15
 */
#include "unity.h"
#include "Det.h"
#include "Os.h"

#define TASK_A      0u
#define TASK_B      1u
#define ST_SINGLE   0u
#define ST_REPEAT   1u
#define ST_CHAIN_A  0u
#define ST_CHAIN_B  1u

static uint8 taskA_runs;
static uint8 taskB_runs;

static void TaskA_Entry(void) { taskA_runs++; (void)TerminateTask(); }
static void TaskB_Entry(void) { taskB_runs++; (void)TerminateTask(); }

static const Os_TaskConfigType test_tasks[] = {
    { "TaskA", TaskA_Entry, 2u, 4u, 0u, FALSE, FULL },
    { "TaskB", TaskB_Entry, 3u, 4u, 0u, FALSE, FULL }
};

static const Os_ExpiryPointConfigType ep_single[] = {
    { 5u,  TASK_A, 0u },
    { 10u, TASK_B, 0u }
};

static const Os_ExpiryPointConfigType ep_repeat[] = {
    { 3u, TASK_A, 0u },
    { 6u, TASK_B, 0u }
};

static const Os_ScheduleTableConfigType st_single_cfg[] = {
    { "ST_Single", 10u, FALSE, ep_single, 2u }
};

static const Os_ScheduleTableConfigType st_repeat_cfg[] = {
    { "ST_Repeat", 6u, TRUE, ep_repeat, 2u }
};

static const Os_ExpiryPointConfigType ep_chainA[] = {
    { 3u, TASK_A, 0u }
};
static const Os_ExpiryPointConfigType ep_chainB[] = {
    { 2u, TASK_B, 0u }
};
static const Os_ScheduleTableConfigType st_chain_cfg[] = {
    { "ST_ChainA", 5u, FALSE, ep_chainA, 1u },
    { "ST_ChainB", 4u, FALSE, ep_chainB, 1u }
};

static const Os_AlarmConfigType test_alarm_cfg[] = {
    { "Alarm0", TASK_A, 1000u, 1u, 1u }
};

void setUp(void)
{
    Os_TestReset();
    (void)Os_TestConfigureTasks(test_tasks, 2u);
    (void)Os_TestConfigureAlarms(test_alarm_cfg, 1u);
    taskA_runs = 0u;
    taskB_runs = 0u;
}

void tearDown(void) {}

/**
 * @spec AUTOSAR OS §10.2
 * @requirement StartScheduleTableRel shall return E_OS_ID for invalid table ID.
 */
void test_StartRel_InvalidID_ReturnsE_OS_ID(void)
{
    StartOS(OSDEFAULTAPPMODE);
    StatusType st = StartScheduleTableRel(0xFFu, 1u);
    TEST_ASSERT_EQUAL(E_OS_ID, st);
}

/**
 * @spec AUTOSAR OS §10.2
 * @requirement StartScheduleTableRel with Offset=0 shall return E_OS_VALUE.
 */
void test_StartRel_ZeroOffset_ReturnsE_OS_VALUE(void)
{
    (void)Os_TestConfigureScheduleTables(st_single_cfg, 1u);
    StartOS(OSDEFAULTAPPMODE);
    StatusType st = StartScheduleTableRel(ST_SINGLE, 0u);
    TEST_ASSERT_EQUAL(E_OS_VALUE, st);
}

/**
 * @spec AUTOSAR OS §10.2
 * @requirement StartScheduleTableRel on an already-running table shall return E_OS_STATE.
 */
void test_StartRel_AlreadyRunning_ReturnsE_OS_STATE(void)
{
    (void)Os_TestConfigureScheduleTables(st_single_cfg, 1u);
    StartOS(OSDEFAULTAPPMODE);
    (void)StartScheduleTableRel(ST_SINGLE, 1u);
    StatusType st = StartScheduleTableRel(ST_SINGLE, 1u);
    TEST_ASSERT_EQUAL(E_OS_STATE, st);
}

/**
 * @spec AUTOSAR OS §10.2
 * @requirement StartScheduleTableAbs shall return E_OS_ID for invalid table ID.
 */
void test_StartAbs_InvalidID_ReturnsE_OS_ID(void)
{
    StartOS(OSDEFAULTAPPMODE);
    StatusType st = StartScheduleTableAbs(0xFFu, 0u);
    TEST_ASSERT_EQUAL(E_OS_ID, st);
}

/**
 * @spec AUTOSAR OS §10.3
 * @requirement StopScheduleTable on stopped table shall return E_OS_NOFUNC.
 */
void test_Stop_NotRunning_ReturnsE_OS_NOFUNC(void)
{
    (void)Os_TestConfigureScheduleTables(st_single_cfg, 1u);
    StartOS(OSDEFAULTAPPMODE);
    StatusType st = StopScheduleTable(ST_SINGLE);
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, st);
}

/**
 * @spec AUTOSAR OS §10.3
 * @requirement StopScheduleTable on running table shall stop it and return E_OK.
 */
void test_Stop_Running_ReturnsE_OK(void)
{
    (void)Os_TestConfigureScheduleTables(st_single_cfg, 1u);
    StartOS(OSDEFAULTAPPMODE);
    (void)StartScheduleTableRel(ST_SINGLE, 1u);
    StatusType st = StopScheduleTable(ST_SINGLE);
    TEST_ASSERT_EQUAL(E_OK, st);

    ScheduleTableStatusType status;
    (void)GetScheduleTableStatus(ST_SINGLE, &status);
    TEST_ASSERT_EQUAL(SCHEDULETABLE_STOPPED, status);
}

/**
 * @spec AUTOSAR OS §10.4
 * @requirement GetScheduleTableStatus shall return SCHEDULETABLE_RUNNING for a started table.
 */
void test_GetStatus_Running(void)
{
    (void)Os_TestConfigureScheduleTables(st_single_cfg, 1u);
    StartOS(OSDEFAULTAPPMODE);
    (void)StartScheduleTableRel(ST_SINGLE, 1u);

    ScheduleTableStatusType status;
    StatusType st = GetScheduleTableStatus(ST_SINGLE, &status);
    TEST_ASSERT_EQUAL(E_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULETABLE_RUNNING, status);
}

/**
 * @spec AUTOSAR OS §10.4
 * @requirement GetScheduleTableStatus shall return SCHEDULETABLE_STOPPED for a stopped table.
 */
void test_GetStatus_Stopped(void)
{
    (void)Os_TestConfigureScheduleTables(st_single_cfg, 1u);
    StartOS(OSDEFAULTAPPMODE);

    ScheduleTableStatusType status;
    StatusType st = GetScheduleTableStatus(ST_SINGLE, &status);
    TEST_ASSERT_EQUAL(E_OK, st);
    TEST_ASSERT_EQUAL(SCHEDULETABLE_STOPPED, status);
}

/**
 * @spec AUTOSAR OS §10.1
 * @requirement Single-shot table: expiry points fire at configured offsets, then table stops.
 */
void test_SingleShot_ExpiryPoints_FireAndStop(void)
{
    (void)Os_TestConfigureScheduleTables(st_single_cfg, 1u);
    StartOS(OSDEFAULTAPPMODE);
    (void)StartScheduleTableRel(ST_SINGLE, 1u);

    /* 1 tick delay + 5 elapsed = 6 ticks until first EP fires TaskA */
    Os_TestAdvanceCounter(6u);
    TEST_ASSERT_EQUAL(1u, taskA_runs);
    TEST_ASSERT_EQUAL(0u, taskB_runs);

    /* 5 more elapsed ticks = offset 10, second EP fires TaskB, table completes */
    Os_TestAdvanceCounter(5u);
    TEST_ASSERT_EQUAL(1u, taskA_runs);
    TEST_ASSERT_EQUAL(1u, taskB_runs);

    /* Table should now be stopped */
    ScheduleTableStatusType status;
    (void)GetScheduleTableStatus(ST_SINGLE, &status);
    TEST_ASSERT_EQUAL(SCHEDULETABLE_STOPPED, status);

    /* Further ticks should not fire anything */
    Os_TestAdvanceCounter(10u);
    TEST_ASSERT_EQUAL(1u, taskA_runs);
    TEST_ASSERT_EQUAL(1u, taskB_runs);
}

/**
 * @spec AUTOSAR OS §10.1
 * @requirement Repeating table: after duration, table restarts and fires expiry points again.
 */
void test_Repeating_ExpiryPoints_CycleAgain(void)
{
    (void)Os_TestConfigureScheduleTables(st_repeat_cfg, 1u);
    StartOS(OSDEFAULTAPPMODE);
    (void)StartScheduleTableRel(0u, 1u);

    /* 1 delay + 6 elapsed = 7 ticks for first full cycle (offsets 3,6) */
    Os_TestAdvanceCounter(7u);
    TEST_ASSERT_EQUAL(1u, taskA_runs);
    TEST_ASSERT_EQUAL(1u, taskB_runs);

    /* Second cycle: 6 more ticks (no delay on repeat) */
    Os_TestAdvanceCounter(6u);
    TEST_ASSERT_EQUAL(2u, taskA_runs);
    TEST_ASSERT_EQUAL(2u, taskB_runs);
}

/**
 * @spec AUTOSAR OS §10.5
 * @requirement NextScheduleTable: when From table completes, To table starts.
 */
void test_NextScheduleTable_ChainsToNextTable(void)
{
    (void)Os_TestConfigureScheduleTables(st_chain_cfg, 2u);
    StartOS(OSDEFAULTAPPMODE);
    (void)StartScheduleTableRel(ST_CHAIN_A, 1u);
    StatusType st = NextScheduleTable(ST_CHAIN_A, ST_CHAIN_B);
    TEST_ASSERT_EQUAL(E_OK, st);

    /* Verify "To" table is marked NEXT */
    ScheduleTableStatusType status;
    (void)GetScheduleTableStatus(ST_CHAIN_B, &status);
    TEST_ASSERT_EQUAL(SCHEDULETABLE_NEXT, status);

    /* 1 delay + 3 elapsed = 4 ticks for ChainA EP (offset 3) */
    Os_TestAdvanceCounter(4u);
    TEST_ASSERT_EQUAL(1u, taskA_runs);

    /* 2 more elapsed = offset 5, ChainA ends, ChainB starts */
    Os_TestAdvanceCounter(2u);
    (void)GetScheduleTableStatus(ST_CHAIN_A, &status);
    TEST_ASSERT_EQUAL(SCHEDULETABLE_STOPPED, status);
    (void)GetScheduleTableStatus(ST_CHAIN_B, &status);
    TEST_ASSERT_EQUAL(SCHEDULETABLE_RUNNING, status);

    /* ChainB EP fires at offset 2 (no initial delay on chained start) */
    Os_TestAdvanceCounter(2u);
    TEST_ASSERT_EQUAL(1u, taskB_runs);
}

/**
 * @spec AUTOSAR OS §10.5
 * @requirement NextScheduleTable on non-running From returns E_OS_NOFUNC.
 */
void test_NextScheduleTable_FromNotRunning_ReturnsE_OS_NOFUNC(void)
{
    (void)Os_TestConfigureScheduleTables(st_chain_cfg, 2u);
    StartOS(OSDEFAULTAPPMODE);
    StatusType st = NextScheduleTable(ST_CHAIN_A, ST_CHAIN_B);
    TEST_ASSERT_EQUAL(E_OS_NOFUNC, st);
}

/**
 * @spec AUTOSAR OS §10.5
 * @requirement NextScheduleTable with To already running returns E_OS_STATE.
 */
void test_NextScheduleTable_ToAlreadyRunning_ReturnsE_OS_STATE(void)
{
    (void)Os_TestConfigureScheduleTables(st_chain_cfg, 2u);
    StartOS(OSDEFAULTAPPMODE);
    (void)StartScheduleTableRel(ST_CHAIN_A, 1u);
    (void)StartScheduleTableRel(ST_CHAIN_B, 1u);
    StatusType st = NextScheduleTable(ST_CHAIN_A, ST_CHAIN_B);
    TEST_ASSERT_EQUAL(E_OS_STATE, st);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_StartRel_InvalidID_ReturnsE_OS_ID);
    RUN_TEST(test_StartRel_ZeroOffset_ReturnsE_OS_VALUE);
    RUN_TEST(test_StartRel_AlreadyRunning_ReturnsE_OS_STATE);
    RUN_TEST(test_StartAbs_InvalidID_ReturnsE_OS_ID);
    RUN_TEST(test_Stop_NotRunning_ReturnsE_OS_NOFUNC);
    RUN_TEST(test_Stop_Running_ReturnsE_OK);
    RUN_TEST(test_GetStatus_Running);
    RUN_TEST(test_GetStatus_Stopped);
    RUN_TEST(test_SingleShot_ExpiryPoints_FireAndStop);
    RUN_TEST(test_Repeating_ExpiryPoints_CycleAgain);
    RUN_TEST(test_NextScheduleTable_ChainsToNextTable);
    RUN_TEST(test_NextScheduleTable_FromNotRunning_ReturnsE_OS_NOFUNC);
    RUN_TEST(test_NextScheduleTable_ToAlreadyRunning_ReturnsE_OS_STATE);
    return UNITY_END();
}
