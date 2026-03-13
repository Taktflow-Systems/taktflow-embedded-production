/**
 * @file    test_int_bswm_mode.c
 * @brief   Integration test INT-006: BswM mode transitions propagate to
 *          dependent modules via action callbacks
 * @date    2026-02-24
 *
 * @verifies  SWR-BSW-022
 * @aspice    SWE.5 — Software Component Verification & Integration
 * @iso       ISO 26262 Part 6, Section 10 — Software Integration & Testing
 * @traces_to TSR-046, TSR-047, TSR-048
 *
 * Linked modules (REAL): BswM.c
 * Mocked:                none (BswM has no HW dependencies)
 *
 * Tests verify the BswM forward-only mode state machine, action callback
 * dispatch per mode, and rejection of invalid backward transitions.
 *
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "BswM.h"

/* ===================================================================
 * Action callback tracking
 * =================================================================== */

static uint8 cb_startup_count;
static uint8 cb_run_count;
static uint8 cb_degraded_count;
static uint8 cb_safe_stop_count;
static uint8 cb_shutdown_count;

/** Invocation order tracking for sequence verification */
static uint8 cb_order_log[16];
static uint8 cb_order_idx;

/** Additional RUN callbacks for multi-action test */
static uint8 cb_run_extra1_count;
static uint8 cb_run_extra2_count;

static void Cb_Startup(void)
{
    cb_startup_count++;
    if (cb_order_idx < 16u) {
        cb_order_log[cb_order_idx] = (uint8)BSWM_STARTUP;
        cb_order_idx++;
    }
}

static void Cb_Run(void)
{
    cb_run_count++;
    if (cb_order_idx < 16u) {
        cb_order_log[cb_order_idx] = (uint8)BSWM_RUN;
        cb_order_idx++;
    }
}

static void Cb_RunExtra1(void)
{
    cb_run_extra1_count++;
}

static void Cb_RunExtra2(void)
{
    cb_run_extra2_count++;
}

static void Cb_Degraded(void)
{
    cb_degraded_count++;
    if (cb_order_idx < 16u) {
        cb_order_log[cb_order_idx] = (uint8)BSWM_DEGRADED;
        cb_order_idx++;
    }
}

static void Cb_SafeStop(void)
{
    cb_safe_stop_count++;
    if (cb_order_idx < 16u) {
        cb_order_log[cb_order_idx] = (uint8)BSWM_SAFE_STOP;
        cb_order_idx++;
    }
}

static void Cb_Shutdown(void)
{
    cb_shutdown_count++;
    if (cb_order_idx < 16u) {
        cb_order_log[cb_order_idx] = (uint8)BSWM_SHUTDOWN;
        cb_order_idx++;
    }
}

/* ===================================================================
 * Test configurations
 * =================================================================== */

/** Standard config: one action per mode */
static const BswM_ModeActionType actions_standard[] = {
    { BSWM_STARTUP,   Cb_Startup  },
    { BSWM_RUN,       Cb_Run      },
    { BSWM_DEGRADED,  Cb_Degraded },
    { BSWM_SAFE_STOP, Cb_SafeStop },
    { BSWM_SHUTDOWN,  Cb_Shutdown }
};

static BswM_ConfigType cfg_standard;

/** Multi-action config: 3 actions for RUN mode */
static const BswM_ModeActionType actions_multi_run[] = {
    { BSWM_STARTUP,   Cb_Startup   },
    { BSWM_RUN,       Cb_Run       },
    { BSWM_RUN,       Cb_RunExtra1 },
    { BSWM_RUN,       Cb_RunExtra2 },
    { BSWM_DEGRADED,  Cb_Degraded  },
    { BSWM_SAFE_STOP, Cb_SafeStop  },
    { BSWM_SHUTDOWN,  Cb_Shutdown  }
};

static BswM_ConfigType cfg_multi_run;

/* ===================================================================
 * setUp / tearDown
 * =================================================================== */

void setUp(void)
{
    /* Reset all callback counters */
    cb_startup_count    = 0u;
    cb_run_count        = 0u;
    cb_degraded_count   = 0u;
    cb_safe_stop_count  = 0u;
    cb_shutdown_count   = 0u;
    cb_run_extra1_count = 0u;
    cb_run_extra2_count = 0u;

    /* Reset order log */
    cb_order_idx = 0u;
    for (uint8 i = 0u; i < 16u; i++) {
        cb_order_log[i] = 0xFFu;
    }

    /* Standard config */
    cfg_standard.ModeActions = actions_standard;
    cfg_standard.ActionCount = 5u;

    /* Multi-action config */
    cfg_multi_run.ModeActions = actions_multi_run;
    cfg_multi_run.ActionCount = 7u;

    /* Default: init with standard config */
    BswM_Init(&cfg_standard);
}

void tearDown(void)
{
    /* No dynamic resources */
}

/* ===================================================================
 * INT-006 Test 1: STARTUP -> RUN callback fires
 * =================================================================== */

/**
 * @verifies SWR-BSW-022
 * @brief    Transition STARTUP -> RUN, call MainFunction, verify RUN callback
 *
 * Scenario: After init (STARTUP mode), transition to RUN, execute
 * BswM_MainFunction, verify that the RUN action callback was invoked.
 */
void test_int_bswm_startup_to_run_callback(void)
{
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_RUN));
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());

    BswM_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, cb_run_count);
    /* STARTUP callback should NOT have fired (mode already changed) */
    TEST_ASSERT_EQUAL_UINT8(0u, cb_startup_count);
}

/* ===================================================================
 * INT-006 Test 2: DEGRADED callback fires
 * =================================================================== */

/**
 * @verifies SWR-BSW-022
 * @brief    Transition to DEGRADED, verify DEGRADED callbacks fire
 *
 * Scenario: STARTUP -> RUN -> DEGRADED, call MainFunction, verify only
 * the DEGRADED action callback fires.
 */
void test_int_bswm_degraded_callback(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_DEGRADED));

    BswM_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, cb_degraded_count);
    TEST_ASSERT_EQUAL_UINT8(0u, cb_run_count);
    TEST_ASSERT_EQUAL_UINT8(0u, cb_safe_stop_count);
}

/* ===================================================================
 * INT-006 Test 3: SAFE_STOP callback fires
 * =================================================================== */

/**
 * @verifies SWR-BSW-022
 * @brief    Transition to SAFE_STOP, verify SAFE_STOP callbacks fire
 *
 * Scenario: STARTUP -> RUN -> SAFE_STOP, call MainFunction, verify
 * the SAFE_STOP action callback fires.
 */
void test_int_bswm_safe_stop_callback(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_SAFE_STOP));

    BswM_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, cb_safe_stop_count);
    TEST_ASSERT_EQUAL_UINT8(0u, cb_run_count);
    TEST_ASSERT_EQUAL_UINT8(0u, cb_degraded_count);
}

/* ===================================================================
 * INT-006 Test 4: Multiple actions per mode all fire
 * =================================================================== */

/**
 * @verifies SWR-BSW-022
 * @brief    3 actions registered for RUN mode, all 3 fire during MainFunction
 *
 * Scenario: Use multi-action config with 3 RUN mode actions. Transition
 * to RUN, call MainFunction, verify all 3 action callbacks were invoked.
 */
void test_int_bswm_multiple_actions_per_mode(void)
{
    /* Re-init with multi-action config */
    BswM_Init(&cfg_multi_run);

    BswM_RequestMode(0u, BSWM_RUN);
    BswM_MainFunction();

    /* All 3 RUN callbacks should fire */
    TEST_ASSERT_EQUAL_UINT8(1u, cb_run_count);
    TEST_ASSERT_EQUAL_UINT8(1u, cb_run_extra1_count);
    TEST_ASSERT_EQUAL_UINT8(1u, cb_run_extra2_count);

    /* Other mode callbacks should NOT fire */
    TEST_ASSERT_EQUAL_UINT8(0u, cb_degraded_count);
    TEST_ASSERT_EQUAL_UINT8(0u, cb_safe_stop_count);
}

/* ===================================================================
 * INT-006 Test 5: Invalid backward transition rejected
 * =================================================================== */

/**
 * @verifies SWR-BSW-022
 * @brief    RUN -> STARTUP rejected (backward transition is invalid)
 *
 * Scenario: Transition to RUN, then try to go back to STARTUP.
 * Verify the transition is rejected and mode remains RUN.
 */
void test_int_bswm_invalid_backward_transition(void)
{
    BswM_RequestMode(0u, BSWM_RUN);
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());

    /* Attempt backward: RUN -> STARTUP */
    Std_ReturnType ret = BswM_RequestMode(0u, BSWM_STARTUP);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());
}

/* ===================================================================
 * INT-006 Test 6: Full lifecycle — each mode's callbacks fire in sequence
 * =================================================================== */

/**
 * @verifies SWR-BSW-022
 * @brief    Full forward lifecycle: STARTUP -> RUN -> DEGRADED ->
 *           SAFE_STOP -> SHUTDOWN, each mode's callbacks fire in sequence
 *
 * Scenario: Walk through all modes in forward order. At each mode, call
 * MainFunction and verify the correct callback fires. Track callback
 * invocation order to ensure correct sequence.
 */
void test_int_bswm_full_lifecycle(void)
{
    /* Phase 1: STARTUP — execute STARTUP action */
    BswM_MainFunction();
    TEST_ASSERT_EQUAL_UINT8(1u, cb_startup_count);

    /* Phase 2: RUN */
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_RUN));
    BswM_MainFunction();
    TEST_ASSERT_EQUAL_UINT8(1u, cb_run_count);

    /* Phase 3: DEGRADED */
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_DEGRADED));
    BswM_MainFunction();
    TEST_ASSERT_EQUAL_UINT8(1u, cb_degraded_count);

    /* Phase 4: SAFE_STOP */
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_SAFE_STOP));
    BswM_MainFunction();
    TEST_ASSERT_EQUAL_UINT8(1u, cb_safe_stop_count);

    /* Phase 5: SHUTDOWN */
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_SHUTDOWN));
    BswM_MainFunction();
    TEST_ASSERT_EQUAL_UINT8(1u, cb_shutdown_count);

    /* Verify order log: STARTUP, RUN, DEGRADED, SAFE_STOP, SHUTDOWN */
    TEST_ASSERT_EQUAL_UINT8((uint8)BSWM_STARTUP,   cb_order_log[0]);
    TEST_ASSERT_EQUAL_UINT8((uint8)BSWM_RUN,       cb_order_log[1]);
    TEST_ASSERT_EQUAL_UINT8((uint8)BSWM_DEGRADED,  cb_order_log[2]);
    TEST_ASSERT_EQUAL_UINT8((uint8)BSWM_SAFE_STOP, cb_order_log[3]);
    TEST_ASSERT_EQUAL_UINT8((uint8)BSWM_SHUTDOWN,  cb_order_log[4]);
}

/* ===================================================================
 * INT-006 Test 7: SHUTDOWN is terminal — no further transitions
 * =================================================================== */

/**
 * @verifies SWR-BSW-022
 * @brief    After SHUTDOWN, all transition requests are rejected
 *
 * Scenario: Reach SHUTDOWN state, then attempt transitions to every
 * other mode. Verify all are rejected and mode stays SHUTDOWN.
 */
void test_int_bswm_shutdown_terminal(void)
{
    /* Walk to SHUTDOWN */
    BswM_RequestMode(0u, BSWM_RUN);
    BswM_RequestMode(0u, BSWM_SAFE_STOP);
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_SHUTDOWN));
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());

    /* Try every possible transition from SHUTDOWN — all must fail */
    TEST_ASSERT_EQUAL(E_NOT_OK, BswM_RequestMode(0u, BSWM_STARTUP));
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());

    TEST_ASSERT_EQUAL(E_NOT_OK, BswM_RequestMode(0u, BSWM_RUN));
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());

    TEST_ASSERT_EQUAL(E_NOT_OK, BswM_RequestMode(0u, BSWM_DEGRADED));
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());

    TEST_ASSERT_EQUAL(E_NOT_OK, BswM_RequestMode(0u, BSWM_SAFE_STOP));
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());

    /* Even SHUTDOWN -> SHUTDOWN (self) might be accepted per implementation.
     * Verify mode is still SHUTDOWN regardless. */
    BswM_RequestMode(0u, BSWM_SHUTDOWN);
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());
}

/* ===================================================================
 * Test runner
 * =================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_int_bswm_startup_to_run_callback);
    RUN_TEST(test_int_bswm_degraded_callback);
    RUN_TEST(test_int_bswm_safe_stop_callback);
    RUN_TEST(test_int_bswm_multiple_actions_per_mode);
    RUN_TEST(test_int_bswm_invalid_backward_transition);
    RUN_TEST(test_int_bswm_full_lifecycle);
    RUN_TEST(test_int_bswm_shutdown_terminal);

    return UNITY_END();
}
