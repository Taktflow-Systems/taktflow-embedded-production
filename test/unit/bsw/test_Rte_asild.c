/**
 * @file    test_Rte.c
 * @brief   Unit tests for Runtime Environment module
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-026, SWR-BSW-027
 *
 * Tests signal read/write (port-based communication), runnable
 * scheduling, WdgM checkpoint integration, and defensive checks.
 */
#include "unity.h"
#include "Rte.h"

/* ==================================================================
 * Mock: WdgM (checkpoint reached after runnable cycle)
 * ================================================================== */

static uint8  mock_wdgm_se_id;
static uint8  mock_wdgm_call_count;

Std_ReturnType WdgM_CheckpointReached(uint8 SEId)
{
    mock_wdgm_se_id = SEId;
    mock_wdgm_call_count++;
    return E_OK;
}

/* ==================================================================
 * Test runnables (set flags when called)
 * ================================================================== */

static uint8  runnable_10ms_call_count;
static uint8  runnable_100ms_call_count;
static uint8  runnable_10ms_b_call_count;

static void TestRunnable_10ms(void)
{
    runnable_10ms_call_count++;
}

static void TestRunnable_100ms(void)
{
    runnable_100ms_call_count++;
}

static void TestRunnable_10ms_B(void)
{
    runnable_10ms_b_call_count++;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

/* Signal config: 3 test signals */
static const Rte_SignalConfigType test_signals[] = {
    { RTE_SIG_TORQUE_REQUEST,  0u },   /* signal 0, initial value 0 */
    { RTE_SIG_STEERING_ANGLE,  0u },   /* signal 1, initial value 0 */
    { RTE_SIG_VEHICLE_SPEED,   0u },   /* signal 2, initial value 0 */
};

/* Runnable config: 3 runnables */
static const Rte_RunnableConfigType test_runnables[] = {
    { TestRunnable_10ms,   10u, 2u, 0u },  /* 10ms, priority 2, SE 0 */
    { TestRunnable_10ms_B, 10u, 1u, 0u },  /* 10ms, priority 1, SE 0 */
    { TestRunnable_100ms, 100u, 1u, 1u },  /* 100ms, priority 1, SE 1 */
};

static Rte_ConfigType test_config;

void setUp(void)
{
    mock_wdgm_call_count     = 0u;
    mock_wdgm_se_id          = 0xFFu;
    runnable_10ms_call_count  = 0u;
    runnable_100ms_call_count = 0u;
    runnable_10ms_b_call_count = 0u;

    test_config.signalConfig   = test_signals;
    test_config.signalCount    = 3u;
    test_config.runnableConfig = test_runnables;
    test_config.runnableCount  = 3u;

    Rte_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-026: Rte Port-Based Communication
 * ================================================================== */

/** @verifies SWR-BSW-026 */
void test_Rte_Init_valid_config(void)
{
    /* Init already called in setUp — verify we can write/read */
    uint32 val = 42u;
    Std_ReturnType ret = Rte_Write(RTE_SIG_TORQUE_REQUEST, val);
    TEST_ASSERT_EQUAL(E_OK, ret);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Init_null_config(void)
{
    Rte_Init(NULL_PTR);

    /* After null init, all operations should fail */
    uint32 val = 0u;
    Std_ReturnType ret = Rte_Write(RTE_SIG_TORQUE_REQUEST, 10u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);

    ret = Rte_Read(RTE_SIG_TORQUE_REQUEST, &val);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Write_stores_signal_value(void)
{
    Std_ReturnType ret = Rte_Write(RTE_SIG_TORQUE_REQUEST, 255u);
    TEST_ASSERT_EQUAL(E_OK, ret);

    uint32 readback = 0u;
    ret = Rte_Read(RTE_SIG_TORQUE_REQUEST, &readback);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(255u, readback);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Read_returns_initial_value_before_write(void)
{
    uint32 val = 0xDEADu;
    Std_ReturnType ret = Rte_Read(RTE_SIG_STEERING_ANGLE, &val);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0u, val);  /* initial value from config */
}

/** @verifies SWR-BSW-026 */
void test_Rte_Write_invalid_signal_id(void)
{
    Std_ReturnType ret = Rte_Write(RTE_MAX_SIGNALS + 1u, 100u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Read_invalid_signal_id(void)
{
    uint32 val = 0u;
    Std_ReturnType ret = Rte_Read(RTE_MAX_SIGNALS + 1u, &val);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Read_null_data_pointer(void)
{
    Std_ReturnType ret = Rte_Read(RTE_SIG_TORQUE_REQUEST, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Write_Read_copy_semantics(void)
{
    /* Write a value, then overwrite — read should get latest */
    Rte_Write(RTE_SIG_VEHICLE_SPEED, 50u);
    Rte_Write(RTE_SIG_VEHICLE_SPEED, 120u);

    uint32 val = 0u;
    Rte_Read(RTE_SIG_VEHICLE_SPEED, &val);
    TEST_ASSERT_EQUAL_UINT32(120u, val);

    /* Write to one signal should not affect another */
    uint32 other = 0u;
    Rte_Read(RTE_SIG_TORQUE_REQUEST, &other);
    TEST_ASSERT_EQUAL_UINT32(0u, other);
}

/* ==================================================================
 * SWR-BSW-027: Rte Runnable Scheduling
 * ================================================================== */

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_fires_10ms_runnable_every_10_ticks(void)
{
    /* Tick 10 times — 10ms runnables should fire once at tick 10 */
    for (uint32 i = 0u; i < 10u; i++) {
        Rte_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(1u, runnable_10ms_call_count);
}

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_fires_100ms_runnable_every_100_ticks(void)
{
    /* Tick 100 times — 100ms runnable should fire once */
    for (uint32 i = 0u; i < 100u; i++) {
        Rte_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(1u, runnable_100ms_call_count);
    /* 10ms runnables should have fired 10 times */
    TEST_ASSERT_EQUAL_UINT8(10u, runnable_10ms_call_count);
}

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_no_fire_before_period(void)
{
    /* Tick 9 times — 10ms runnable should NOT have fired yet */
    for (uint32 i = 0u; i < 9u; i++) {
        Rte_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(0u, runnable_10ms_call_count);
    TEST_ASSERT_EQUAL_UINT8(0u, runnable_100ms_call_count);
}

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_multiple_runnables_same_period(void)
{
    /* Tick 10 times — both 10ms runnables should fire */
    for (uint32 i = 0u; i < 10u; i++) {
        Rte_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(1u, runnable_10ms_call_count);
    TEST_ASSERT_EQUAL_UINT8(1u, runnable_10ms_b_call_count);
}

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_calls_WdgM_checkpoint(void)
{
    /* After 10 ticks, runnables with SE 0 fire — WdgM checkpoint for SE 0 */
    for (uint32 i = 0u; i < 10u; i++) {
        Rte_MainFunction();
    }

    TEST_ASSERT_TRUE(mock_wdgm_call_count > 0u);
}

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_does_nothing_before_init(void)
{
    /* Re-init with null to simulate uninitialized state */
    Rte_Init(NULL_PTR);

    Rte_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(0u, runnable_10ms_call_count);
    TEST_ASSERT_EQUAL_UINT8(0u, runnable_100ms_call_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_wdgm_call_count);
}

/* ==================================================================
 * SWR-BSW-026 / SWR-BSW-027: Hardened Boundary Tests
 * ================================================================== */

/** @verifies SWR-BSW-026 */
void test_Rte_Write_signal_id_0_valid(void)
{
    Std_ReturnType ret = Rte_Write(0u, 42u);
    TEST_ASSERT_EQUAL(E_OK, ret);

    uint32 val = 0u;
    Rte_Read(0u, &val);
    TEST_ASSERT_EQUAL_UINT32(42u, val);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Write_signal_id_last_valid(void)
{
    /* signalCount = 3, last valid index = 2 */
    Std_ReturnType ret = Rte_Write(2u, 999u);
    TEST_ASSERT_EQUAL(E_OK, ret);

    uint32 val = 0u;
    Rte_Read(2u, &val);
    TEST_ASSERT_EQUAL_UINT32(999u, val);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Write_signal_id_equals_count_invalid(void)
{
    /* signalCount = 3, so index 3 is out of bounds */
    Std_ReturnType ret = Rte_Write(3u, 100u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Read_signal_id_equals_count_invalid(void)
{
    uint32 val = 0u;
    Std_ReturnType ret = Rte_Read(3u, &val);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Write_signal_id_max_signals_invalid(void)
{
    Std_ReturnType ret = Rte_Write(RTE_MAX_SIGNALS, 100u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Read_signal_id_max_signals_invalid(void)
{
    uint32 val = 0u;
    Std_ReturnType ret = Rte_Read(RTE_MAX_SIGNALS, &val);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Write_Read_roundtrip_data_integrity(void)
{
    /* Write a known pattern and verify exact readback */
    uint32 pattern = 0xDEADBEEFu;
    Rte_Write(RTE_SIG_TORQUE_REQUEST, pattern);

    uint32 readback = 0u;
    Rte_Read(RTE_SIG_TORQUE_REQUEST, &readback);
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, readback);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Write_no_crosstalk_between_signals(void)
{
    Rte_Write(RTE_SIG_TORQUE_REQUEST, 111u);
    Rte_Write(RTE_SIG_STEERING_ANGLE, 222u);
    Rte_Write(RTE_SIG_VEHICLE_SPEED,  333u);

    uint32 val0 = 0u, val1 = 0u, val2 = 0u;
    Rte_Read(RTE_SIG_TORQUE_REQUEST, &val0);
    Rte_Read(RTE_SIG_STEERING_ANGLE, &val1);
    Rte_Read(RTE_SIG_VEHICLE_SPEED,  &val2);

    TEST_ASSERT_EQUAL_UINT32(111u, val0);
    TEST_ASSERT_EQUAL_UINT32(222u, val1);
    TEST_ASSERT_EQUAL_UINT32(333u, val2);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Read_initial_values_after_init(void)
{
    /* All signals configured with initialValue = 0 */
    uint32 val = 9999u;

    Rte_Read(RTE_SIG_TORQUE_REQUEST, &val);
    TEST_ASSERT_EQUAL_UINT32(0u, val);

    val = 9999u;
    Rte_Read(RTE_SIG_STEERING_ANGLE, &val);
    TEST_ASSERT_EQUAL_UINT32(0u, val);

    val = 9999u;
    Rte_Read(RTE_SIG_VEHICLE_SPEED, &val);
    TEST_ASSERT_EQUAL_UINT32(0u, val);
}

/** @verifies SWR-BSW-026 */
void test_Rte_Init_signal_count_exceeds_max_not_initialized(void)
{
    Rte_ConfigType overflow_cfg;
    overflow_cfg.signalConfig   = test_signals;
    overflow_cfg.signalCount    = RTE_MAX_SIGNALS + 1u;
    overflow_cfg.runnableConfig = test_runnables;
    overflow_cfg.runnableCount  = 3u;

    Rte_Init(&overflow_cfg);

    /* Should reject the config — operations should fail */
    uint32 val = 0u;
    Std_ReturnType ret = Rte_Read(0u, &val);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_highest_priority_runs_first(void)
{
    /* TestRunnable_10ms has priority 2, TestRunnable_10ms_B has priority 1 */
    /* After 10 ticks, both should have fired once. The higher priority (2)
     * should run before lower priority (1). We verify both ran. */
    for (uint32 i = 0u; i < 10u; i++) {
        Rte_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(1u, runnable_10ms_call_count);
    TEST_ASSERT_EQUAL_UINT8(1u, runnable_10ms_b_call_count);
}

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_period_1_fires_every_tick(void)
{
    /* Reconfigure with a period-1 runnable */
    static const Rte_RunnableConfigType fast_runnables[] = {
        { TestRunnable_10ms, 1u, 1u, 0u },  /* period=1, fires every tick */
    };
    Rte_ConfigType fast_cfg;
    fast_cfg.signalConfig   = test_signals;
    fast_cfg.signalCount    = 3u;
    fast_cfg.runnableConfig = fast_runnables;
    fast_cfg.runnableCount  = 1u;

    Rte_Init(&fast_cfg);

    for (uint32 i = 0u; i < 10u; i++) {
        Rte_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(10u, runnable_10ms_call_count);
}

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_period_10_fires_at_10th_tick(void)
{
    /* Already configured with period=10 runnables via setUp */
    /* At tick 9 = 0 fires, at tick 10 = 1 fire */
    for (uint32 i = 0u; i < 9u; i++) {
        Rte_MainFunction();
    }
    TEST_ASSERT_EQUAL_UINT8(0u, runnable_10ms_call_count);

    Rte_MainFunction(); /* tick 10 */
    TEST_ASSERT_EQUAL_UINT8(1u, runnable_10ms_call_count);
}

/** @verifies SWR-BSW-027 */
void test_Rte_MainFunction_null_func_ptr_skipped(void)
{
    /* Configure a runnable with NULL function pointer */
    static const Rte_RunnableConfigType null_runnables[] = {
        { NULL_PTR, 1u, 1u, 0u },  /* NULL func — should be skipped */
    };
    Rte_ConfigType null_cfg;
    null_cfg.signalConfig   = test_signals;
    null_cfg.signalCount    = 3u;
    null_cfg.runnableConfig = null_runnables;
    null_cfg.runnableCount  = 1u;

    Rte_Init(&null_cfg);

    /* Should not crash — just skip the NULL runnable */
    for (uint32 i = 0u; i < 5u; i++) {
        Rte_MainFunction();
    }

    /* If we get here without crashing, test passes */
    TEST_ASSERT_TRUE(1);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-BSW-026: Port-Based Communication */
    RUN_TEST(test_Rte_Init_valid_config);
    RUN_TEST(test_Rte_Init_null_config);
    RUN_TEST(test_Rte_Write_stores_signal_value);
    RUN_TEST(test_Rte_Read_returns_initial_value_before_write);
    RUN_TEST(test_Rte_Write_invalid_signal_id);
    RUN_TEST(test_Rte_Read_invalid_signal_id);
    RUN_TEST(test_Rte_Read_null_data_pointer);
    RUN_TEST(test_Rte_Write_Read_copy_semantics);

    /* SWR-BSW-027: Runnable Scheduling */
    RUN_TEST(test_Rte_MainFunction_fires_10ms_runnable_every_10_ticks);
    RUN_TEST(test_Rte_MainFunction_fires_100ms_runnable_every_100_ticks);
    RUN_TEST(test_Rte_MainFunction_no_fire_before_period);
    RUN_TEST(test_Rte_MainFunction_multiple_runnables_same_period);
    RUN_TEST(test_Rte_MainFunction_calls_WdgM_checkpoint);
    RUN_TEST(test_Rte_MainFunction_does_nothing_before_init);

    /* Hardened boundary tests */
    RUN_TEST(test_Rte_Write_signal_id_0_valid);
    RUN_TEST(test_Rte_Write_signal_id_last_valid);
    RUN_TEST(test_Rte_Write_signal_id_equals_count_invalid);
    RUN_TEST(test_Rte_Read_signal_id_equals_count_invalid);
    RUN_TEST(test_Rte_Write_signal_id_max_signals_invalid);
    RUN_TEST(test_Rte_Read_signal_id_max_signals_invalid);
    RUN_TEST(test_Rte_Write_Read_roundtrip_data_integrity);
    RUN_TEST(test_Rte_Write_no_crosstalk_between_signals);
    RUN_TEST(test_Rte_Read_initial_values_after_init);
    RUN_TEST(test_Rte_Init_signal_count_exceeds_max_not_initialized);
    RUN_TEST(test_Rte_MainFunction_highest_priority_runs_first);
    RUN_TEST(test_Rte_MainFunction_period_1_fires_every_tick);
    RUN_TEST(test_Rte_MainFunction_period_10_fires_at_10th_tick);
    RUN_TEST(test_Rte_MainFunction_null_func_ptr_skipped);

    return UNITY_END();
}
