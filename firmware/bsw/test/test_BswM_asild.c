/**
 * @file    test_BswM.c
 * @brief   Unit tests for BSW Mode Manager
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-022
 *
 * Tests ECU mode transitions (STARTUP -> RUN -> DEGRADED -> SAFE_STOP ->
 * SHUTDOWN), rule-based mode actions via config callbacks, and
 * defensive programming checks.
 */
#include "unity.h"
#include "BswM.h"

/* ==================================================================
 * Mock: Mode action tracking
 * ================================================================== */

static uint8 mock_action_run_count;
static uint8 mock_action_degraded_count;
static uint8 mock_action_safe_stop_count;
static uint8 mock_action_shutdown_count;
static uint8 mock_action_startup_count;

static void Action_Startup(void)
{
    mock_action_startup_count++;
}

static void Action_Run(void)
{
    mock_action_run_count++;
}

static void Action_Degraded(void)
{
    mock_action_degraded_count++;
}

static void Action_SafeStop(void)
{
    mock_action_safe_stop_count++;
}

static void Action_Shutdown(void)
{
    mock_action_shutdown_count++;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static const BswM_ModeActionType test_mode_actions[] = {
    { BSWM_STARTUP,   Action_Startup   },
    { BSWM_RUN,       Action_Run       },
    { BSWM_DEGRADED,  Action_Degraded  },
    { BSWM_SAFE_STOP, Action_SafeStop  },
    { BSWM_SHUTDOWN,  Action_Shutdown  },
};

static BswM_ConfigType test_config;

void setUp(void)
{
    mock_action_startup_count   = 0u;
    mock_action_run_count       = 0u;
    mock_action_degraded_count  = 0u;
    mock_action_safe_stop_count = 0u;
    mock_action_shutdown_count  = 0u;

    test_config.ModeActions     = test_mode_actions;
    test_config.ActionCount     = 5u;

    BswM_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-022: Initialization and Default State
 * ================================================================== */

/** @verifies SWR-BSW-022 — initial mode is STARTUP */
void test_BswM_Init_default_startup(void)
{
    TEST_ASSERT_EQUAL(BSWM_STARTUP, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022 — init null config */
void test_BswM_Init_null_config(void)
{
    BswM_Init(NULL_PTR);

    /* Should stay in STARTUP (safe default) */
    TEST_ASSERT_EQUAL(BSWM_STARTUP, BswM_GetCurrentMode());

    /* RequestMode should fail */
    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_RUN);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-022: Mode Transitions
 * ================================================================== */

/** @verifies SWR-BSW-022 — transition STARTUP -> RUN */
void test_BswM_RequestMode_startup_to_run(void)
{
    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_RUN);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022 — transition RUN -> DEGRADED */
void test_BswM_RequestMode_run_to_degraded(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_DEGRADED);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_DEGRADED, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022 — transition RUN -> SAFE_STOP */
void test_BswM_RequestMode_run_to_safe_stop(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_SAFE_STOP);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022 — transition SAFE_STOP -> SHUTDOWN */
void test_BswM_RequestMode_safe_stop_to_shutdown(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_SAFE_STOP);
    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_SHUTDOWN);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022 — DEGRADED -> SAFE_STOP allowed */
void test_BswM_RequestMode_degraded_to_safe_stop(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_DEGRADED);
    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_SAFE_STOP);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());
}

/* ==================================================================
 * SWR-BSW-022: Mode Actions via MainFunction
 * ================================================================== */

/** @verifies SWR-BSW-022 — MainFunction executes RUN action */
void test_BswM_MainFunction_executes_run_action(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_action_run_count);
}

/** @verifies SWR-BSW-022 — MainFunction executes DEGRADED action */
void test_BswM_MainFunction_executes_degraded_action(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_DEGRADED);
    BswM_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_action_degraded_count);
}

/** @verifies SWR-BSW-022 — MainFunction executes SAFE_STOP action */
void test_BswM_MainFunction_executes_safe_stop_action(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_SAFE_STOP);
    BswM_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_action_safe_stop_count);
}

/** @verifies SWR-BSW-022 — MainFunction executes SHUTDOWN action */
void test_BswM_MainFunction_executes_shutdown_action(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_SAFE_STOP);
    BswM_RequestMode(0u, BSWM_SHUTDOWN);
    BswM_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_action_shutdown_count);
}

/* ==================================================================
 * SWR-BSW-022: Invalid Transitions
 * ================================================================== */

/** @verifies SWR-BSW-022 — invalid mode value rejected */
void test_BswM_RequestMode_invalid_mode(void)
{
    Std_ReturnType ret = BswM_RequestMode(0u, 99u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-022 — backward transition SHUTDOWN -> RUN rejected */
void test_BswM_RequestMode_backward_rejected(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_SAFE_STOP);
    BswM_RequestMode(0u, BSWM_SHUTDOWN);

    /* Cannot go back from SHUTDOWN */
    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_RUN);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022 — MainFunction with null config does not crash */
void test_BswM_MainFunction_not_initialized(void)
{
    BswM_Init(NULL_PTR);
    BswM_MainFunction();

    /* No action should execute, no crash */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_action_run_count);
}

/* ==================================================================
 * SWR-BSW-022: Hardened Boundary / Fault Injection Tests
 * ================================================================== */

/** @verifies SWR-BSW-022
 *  Mode transition: STARTUP -> RUN -> SHUTDOWN full path (valid forward) */
void test_BswM_RequestMode_full_forward_path(void)
{
    Std_ReturnType ret;

    ret = BswM_RequestMode(0u, BSWM_RUN);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());

    ret = BswM_RequestMode(0u, BSWM_SAFE_STOP);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    ret = BswM_RequestMode(0u, BSWM_SHUTDOWN);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022
 *  Invalid transition: RUN -> STARTUP (backward, should be rejected) */
void test_BswM_RequestMode_run_to_startup_rejected(void)
{
    BswM_RequestMode(0u, BSWM_RUN);

    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_STARTUP);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022
 *  Invalid transition: DEGRADED -> RUN (backward, should be rejected) */
void test_BswM_RequestMode_degraded_to_run_rejected(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_DEGRADED);

    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_RUN);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_DEGRADED, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022
 *  Invalid transition: SAFE_STOP -> RUN (backward, should be rejected) */
void test_BswM_RequestMode_safe_stop_to_run_rejected(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_SAFE_STOP);

    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_RUN);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022
 *  Invalid mode request: mode value beyond defined enum (e.g. 255) */
void test_BswM_RequestMode_invalid_mode_255(void)
{
    Std_ReturnType ret = BswM_RequestMode(0u, 255u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_STARTUP, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022
 *  Invalid mode request: mode value just beyond BSWM_SHUTDOWN (5) */
void test_BswM_RequestMode_invalid_mode_5(void)
{
    Std_ReturnType ret = BswM_RequestMode(0u, 5u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_STARTUP, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022
 *  Each mode individually: verify mode after setting to each valid mode */
void test_BswM_RequestMode_each_mode_individually(void)
{
    /* STARTUP is the init mode */
    TEST_ASSERT_EQUAL(BSWM_STARTUP, BswM_GetCurrentMode());

    /* RUN */
    BswM_RequestMode(0u, BSWM_RUN);
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());

    /* Re-init to start fresh */
    BswM_Init(&test_config);
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_DEGRADED);
    TEST_ASSERT_EQUAL(BSWM_DEGRADED, BswM_GetCurrentMode());

    BswM_Init(&test_config);
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_SAFE_STOP);
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    BswM_Init(&test_config);
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_SAFE_STOP);
    BswM_RequestMode(0u, BSWM_SHUTDOWN);
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022
 *  Rule execution: verify action callback called for STARTUP mode via MainFunction */
void test_BswM_MainFunction_executes_startup_action(void)
{
    /* After init, mode is STARTUP */
    BswM_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_action_startup_count);
}

/** @verifies SWR-BSW-022
 *  Rule execution: verify only the current mode's action is called, not others */
void test_BswM_MainFunction_only_current_mode_action(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_MainFunction();

    /* Only RUN action should be called */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_action_run_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_action_degraded_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_action_safe_stop_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_action_shutdown_count);
}

/** @verifies SWR-BSW-022
 *  Re-initialization: verify mode resets to STARTUP after re-init */
void test_BswM_ReInit_resets_mode(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_DEGRADED);
    TEST_ASSERT_EQUAL(BSWM_DEGRADED, BswM_GetCurrentMode());

    /* Re-init should reset back to STARTUP */
    BswM_Init(&test_config);
    TEST_ASSERT_EQUAL(BSWM_STARTUP, BswM_GetCurrentMode());
}

/** @verifies SWR-BSW-022
 *  Self-transition: requesting the same mode should be OK or rejected (not crash) */
void test_BswM_RequestMode_same_mode(void)
{
    BswM_RequestMode(0u, BSWM_RUN);

    /* Request RUN again while already in RUN */
    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_RUN);

    /* Implementation may accept or reject — both are valid. No crash. */
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());
    (void)ret;
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Init / default */
    RUN_TEST(test_BswM_Init_default_startup);
    RUN_TEST(test_BswM_Init_null_config);

    /* Mode transitions */
    RUN_TEST(test_BswM_RequestMode_startup_to_run);
    RUN_TEST(test_BswM_RequestMode_run_to_degraded);
    RUN_TEST(test_BswM_RequestMode_run_to_safe_stop);
    RUN_TEST(test_BswM_RequestMode_safe_stop_to_shutdown);
    RUN_TEST(test_BswM_RequestMode_degraded_to_safe_stop);

    /* Mode actions */
    RUN_TEST(test_BswM_MainFunction_executes_run_action);
    RUN_TEST(test_BswM_MainFunction_executes_degraded_action);
    RUN_TEST(test_BswM_MainFunction_executes_safe_stop_action);
    RUN_TEST(test_BswM_MainFunction_executes_shutdown_action);

    /* Invalid transitions */
    RUN_TEST(test_BswM_RequestMode_invalid_mode);
    RUN_TEST(test_BswM_RequestMode_backward_rejected);
    RUN_TEST(test_BswM_MainFunction_not_initialized);

    /* Hardened boundary / fault injection tests */
    RUN_TEST(test_BswM_RequestMode_full_forward_path);
    RUN_TEST(test_BswM_RequestMode_run_to_startup_rejected);
    RUN_TEST(test_BswM_RequestMode_degraded_to_run_rejected);
    RUN_TEST(test_BswM_RequestMode_safe_stop_to_run_rejected);
    RUN_TEST(test_BswM_RequestMode_invalid_mode_255);
    RUN_TEST(test_BswM_RequestMode_invalid_mode_5);
    RUN_TEST(test_BswM_RequestMode_each_mode_individually);
    RUN_TEST(test_BswM_MainFunction_executes_startup_action);
    RUN_TEST(test_BswM_MainFunction_only_current_mode_action);
    RUN_TEST(test_BswM_ReInit_resets_mode);
    RUN_TEST(test_BswM_RequestMode_same_mode);

    return UNITY_END();
}
