/**
 * @file    test_sc_relay.c
 * @brief   Unit tests for sc_relay — kill relay GPIO control
 * @date    2026-02-23
 *
 * @verifies SSR-SC-010, SSR-SC-011, SSR-SC-012, SWR-SC-010, SWR-SC-011, SWR-SC-012
 *
 * Tests relay initialization (LOW), energize/de-energize, kill latch,
 * readback verification, and all de-energize trigger conditions.
 *
 * Mocks: GIO registers, heartbeat/plausibility/selftest state getters.
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions
 * ================================================================== */

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int       uint32;
typedef signed short        sint16;
typedef signed int         sint32;
typedef uint8               boolean;
typedef uint8               Std_ReturnType;

#define TRUE                1u
#define FALSE               0u
#define E_OK                0u
#define E_NOT_OK            1u
#define NULL_PTR            ((void*)0)

/* ==================================================================
 * SC Configuration Constants
 * ================================================================== */

#define SC_GIO_PORT_A               0u
#define SC_PIN_RELAY                0u
#define SC_RELAY_READBACK_THRESHOLD 2u

/* Kill reason enum (matching sc_cfg.h) */
#define SC_KILL_REASON_NONE         0u
#define SC_KILL_REASON_HB_TIMEOUT   1u
#define SC_KILL_REASON_PLAUSIBILITY 2u
#define SC_KILL_REASON_SELFTEST     3u
#define SC_KILL_REASON_ESM          4u
#define SC_KILL_REASON_BUSOFF       5u
#define SC_KILL_REASON_READBACK     6u
#define SC_KILL_REASON_ESTOP        7u
#define SC_KILL_REASON_BUS_SILENCE  8u
#define SC_KILL_REASON_E2E_FAIL     9u
#define SC_KILL_REASON_CREEP_GUARD 10u

/* Fault source enum */
#define SC_FAULT_SOURCE_NONE        0u
#define SC_FAULT_SOURCE_CVC         1u
#define SC_FAULT_SOURCE_FZC         2u
#define SC_FAULT_SOURCE_RZC         3u


/* ==================================================================
 * Mock: GIO Register Access
 * ================================================================== */

static uint8 mock_gio_a_dout[8];
static uint8 mock_gio_a_din[8];

void gioSetBit(uint8 port, uint8 pin, uint8 value)
{
    (void)port;
    if (pin < 8u) {
        mock_gio_a_dout[pin] = value;
        /* By default, readback matches output */
        mock_gio_a_din[pin] = value;
    }
}

uint8 gioGetBit(uint8 port, uint8 pin)
{
    (void)port;
    if (pin < 8u) {
        return mock_gio_a_din[pin];
    }
    return 0u;
}

/* ==================================================================
 * Mock: Module state getters
 * ================================================================== */

static boolean mock_hb_any_confirmed;
static boolean mock_plaus_faulted;
static boolean mock_selftest_healthy;
static boolean mock_esm_error;
static boolean mock_can_bus_off;
static boolean mock_can_bus_silent;
static boolean mock_estop_active;
static boolean mock_e2e_any_critical_failed;
static boolean mock_creep_faulted;

boolean SC_Heartbeat_IsAnyConfirmed(void)
{
    return mock_hb_any_confirmed;
}

boolean SC_Plausibility_IsFaulted(void)
{
    return mock_plaus_faulted;
}

boolean SC_SelfTest_IsHealthy(void)
{
    return mock_selftest_healthy;
}

boolean SC_ESM_IsErrorActive(void)
{
    return mock_esm_error;
}

boolean SC_CAN_IsBusOff(void)
{
    return mock_can_bus_off;
}

boolean SC_CAN_IsBusSilent(void)
{
    return mock_can_bus_silent;
}

boolean SC_CAN_IsEStopActive(void)
{
    return mock_estop_active;
}

boolean SC_E2E_IsAnyCriticalFailed(void)
{
    return mock_e2e_any_critical_failed;
}

boolean SC_Plausibility_IsCreepFaulted(void)
{
    return mock_creep_faulted;
}


/* ==================================================================
 * Include source under test
 * ================================================================== */

#include "../src/sc_relay.c"

/* ==================================================================
 * Test setup / teardown
 * ================================================================== */

void setUp(void)
{
    uint8 i;
    for (i = 0u; i < 8u; i++) {
        mock_gio_a_dout[i] = 0u;
        mock_gio_a_din[i]  = 0u;
    }

    mock_hb_any_confirmed = FALSE;
    mock_plaus_faulted    = FALSE;
    mock_selftest_healthy = TRUE;
    mock_esm_error        = FALSE;
    mock_can_bus_off      = FALSE;
    mock_can_bus_silent   = FALSE;
    mock_estop_active             = FALSE;
    mock_e2e_any_critical_failed  = FALSE;
    mock_creep_faulted            = FALSE;

    /* Direct reset of kill latch for test isolation.
     * SC_Relay_Init() does NOT clear the latch (SWR-SC-011). */
    relay_killed = FALSE;
    SC_Relay_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-SC-010: Initialization
 * ================================================================== */

/** @verifies SWR-SC-010 -- Init sets relay LOW (safe state) */
void test_Relay_Init_low(void)
{
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a_dout[SC_PIN_RELAY]);
    TEST_ASSERT_FALSE(SC_Relay_IsKilled());
}

/* ==================================================================
 * SWR-SC-010: Energize / De-energize
 * ================================================================== */

/** @verifies SWR-SC-010 -- Energize sets relay HIGH */
void test_Relay_Energize(void)
{
    SC_Relay_Energize();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a_dout[SC_PIN_RELAY]);
}

/** @verifies SWR-SC-010 -- De-energize sets relay LOW and latches */
void test_Relay_DeEnergize(void)
{
    SC_Relay_Energize();
    SC_Relay_DeEnergize();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a_dout[SC_PIN_RELAY]);
    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
}

/* ==================================================================
 * SWR-SC-011: Kill Latch
 * ================================================================== */

/** @verifies SWR-SC-011 -- Once killed, energize has no effect */
void test_Relay_kill_latch(void)
{
    SC_Relay_Energize();
    SC_Relay_DeEnergize();
    TEST_ASSERT_TRUE(SC_Relay_IsKilled());

    /* Try to re-energize — should be blocked by latch */
    SC_Relay_Energize();
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a_dout[SC_PIN_RELAY]);
}

/* ==================================================================
 * SWR-SC-012: De-energize Triggers
 * ================================================================== */

/** @verifies SWR-SC-035 -- E-Stop command triggers immediate relay kill (GAP-SC-001) */
void test_Relay_trigger_estop(void)
{
    SC_Relay_Energize();
    mock_estop_active = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a_dout[SC_PIN_RELAY]);
    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
    TEST_ASSERT_EQUAL_UINT8(SC_KILL_REASON_ESTOP, SC_Relay_GetKillReason());
}

/** @verifies SWR-SC-035 -- E-Stop has higher priority than heartbeat timeout */
void test_Relay_trigger_estop_priority_over_heartbeat(void)
{
    SC_Relay_Energize();
    mock_estop_active     = TRUE;
    mock_hb_any_confirmed = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
    TEST_ASSERT_EQUAL_UINT8(SC_KILL_REASON_ESTOP, SC_Relay_GetKillReason());
}

/** @verifies SWR-SC-012 -- Heartbeat confirmed timeout triggers kill */
void test_Relay_trigger_heartbeat(void)
{
    SC_Relay_Energize();
    mock_hb_any_confirmed = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a_dout[SC_PIN_RELAY]);
    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012 -- Plausibility fault triggers kill */
void test_Relay_trigger_plausibility(void)
{
    SC_Relay_Energize();
    mock_plaus_faulted = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012 -- Self-test failure triggers kill */
void test_Relay_trigger_selftest(void)
{
    SC_Relay_Energize();
    mock_selftest_healthy = FALSE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012 -- ESM lockstep error triggers kill */
void test_Relay_trigger_esm(void)
{
    SC_Relay_Energize();
    mock_esm_error = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012 -- GPIO readback mismatch (2 consecutive) triggers kill */
void test_Relay_trigger_readback_mismatch(void)
{
    SC_Relay_Energize();

    /* Force readback to differ from output */
    mock_gio_a_din[SC_PIN_RELAY] = 0u;  /* Output is 1, readback is 0 */

    SC_Relay_CheckTriggers();  /* Mismatch 1 */
    TEST_ASSERT_FALSE(SC_Relay_IsKilled());  /* Not yet — need 2 */

    SC_Relay_CheckTriggers();  /* Mismatch 2 */
    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012 -- Readback mismatch counter resets on match */
void test_Relay_readback_resets(void)
{
    SC_Relay_Energize();

    /* Force 1 mismatch */
    mock_gio_a_din[SC_PIN_RELAY] = 0u;
    SC_Relay_CheckTriggers();

    /* Readback matches again */
    mock_gio_a_din[SC_PIN_RELAY] = 1u;
    SC_Relay_CheckTriggers();

    /* Another single mismatch — counter was reset, so no kill */
    mock_gio_a_din[SC_PIN_RELAY] = 0u;
    SC_Relay_CheckTriggers();

    TEST_ASSERT_FALSE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012 -- No triggers = relay stays energized */
void test_Relay_no_trigger(void)
{
    SC_Relay_Energize();

    SC_Relay_CheckTriggers();
    SC_Relay_CheckTriggers();
    SC_Relay_CheckTriggers();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a_dout[SC_PIN_RELAY]);
    TEST_ASSERT_FALSE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-036 -- Bus silence triggers relay kill (GAP-SC-003) */
void test_Relay_trigger_bus_silence(void)
{
    SC_Relay_Energize();
    mock_can_bus_silent = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
    TEST_ASSERT_EQUAL_UINT8(SC_KILL_REASON_BUS_SILENCE, SC_Relay_GetKillReason());
}

/* ==================================================================
 * SWR-SC-003: E2E Persistent Failure Trigger (GAP-SC-002)
 * ================================================================== */

/** @verifies SWR-SC-003 -- E2E persistent failure on critical mailbox triggers kill */
void test_Relay_trigger_e2e_fail(void)
{
    SC_Relay_Energize();
    mock_e2e_any_critical_failed = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
    TEST_ASSERT_EQUAL_UINT8(SC_KILL_REASON_E2E_FAIL, SC_Relay_GetKillReason());
}

/** @verifies SWR-SC-003 -- E2E failure has lower priority than plausibility */
void test_Relay_trigger_e2e_priority_below_plausibility(void)
{
    SC_Relay_Energize();
    mock_plaus_faulted           = TRUE;
    mock_e2e_any_critical_failed = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
    TEST_ASSERT_EQUAL_UINT8(SC_KILL_REASON_PLAUSIBILITY, SC_Relay_GetKillReason());
}

/** @verifies SWR-SC-003 -- E2E failure has higher priority than self-test */
void test_Relay_trigger_e2e_priority_above_selftest(void)
{
    SC_Relay_Energize();
    mock_e2e_any_critical_failed = TRUE;
    mock_selftest_healthy        = FALSE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
    TEST_ASSERT_EQUAL_UINT8(SC_KILL_REASON_E2E_FAIL, SC_Relay_GetKillReason());
}

/* ==================================================================
 * HARDENED TESTS — Boundary Values, Fault Injection
 * ================================================================== */

/** @verifies SWR-SC-012
 *  Equivalence class: Fault injection — CAN bus-off triggers kill */
void test_relay_trigger_can_busoff(void)
{
    SC_Relay_Energize();
    mock_can_bus_off = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012
 *  Equivalence class: Fault injection — multiple triggers simultaneously */
void test_relay_trigger_multiple_simultaneous(void)
{
    SC_Relay_Energize();
    mock_hb_any_confirmed = TRUE;
    mock_plaus_faulted    = TRUE;
    mock_esm_error        = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a_dout[SC_PIN_RELAY]);
}

/** @verifies SWR-SC-011
 *  Equivalence class: Boundary — kill latch survives Init re-call */
void test_relay_kill_survives_double_init(void)
{
    SC_Relay_Energize();
    SC_Relay_DeEnergize();
    TEST_ASSERT_TRUE(SC_Relay_IsKilled());

    /* Re-init should NOT clear the kill latch */
    SC_Relay_Init();
    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-010
 *  Equivalence class: Boundary — de-energize without prior energize */
void test_relay_deenergize_without_energize(void)
{
    /* Relay starts LOW after Init; de-energize should be safe */
    SC_Relay_DeEnergize();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a_dout[SC_PIN_RELAY]);
    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012
 *  Equivalence class: Boundary — readback mismatch counter at exactly 1 (under threshold) */
void test_relay_readback_exactly_1_no_kill(void)
{
    SC_Relay_Energize();

    mock_gio_a_din[SC_PIN_RELAY] = 0u;
    SC_Relay_CheckTriggers();

    TEST_ASSERT_FALSE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012
 *  Equivalence class: Fault injection — check triggers with relay not energized */
void test_relay_check_triggers_not_energized(void)
{
    /* Relay is LOW (not energized), check triggers should be safe */
    SC_Relay_CheckTriggers();

    TEST_ASSERT_FALSE(SC_Relay_IsKilled());
}

/** @verifies SWR-SC-012 -- Creep guard fault triggers kill (SSR-SC-018) */
void test_Relay_trigger_creep_guard(void)
{
    SC_Relay_Energize();
    mock_creep_faulted = TRUE;

    SC_Relay_CheckTriggers();

    TEST_ASSERT_TRUE(SC_Relay_IsKilled());
    TEST_ASSERT_EQUAL_UINT8(SC_KILL_REASON_CREEP_GUARD, SC_Relay_GetKillReason());
}

/** @verifies SWR-SC-012 -- GetKillReason returns correct reason code */
void test_relay_get_kill_reason(void)
{
    TEST_ASSERT_EQUAL_UINT8(SC_KILL_REASON_NONE, SC_Relay_GetKillReason());

    SC_Relay_Energize();
    mock_hb_any_confirmed = TRUE;
    SC_Relay_CheckTriggers();

    TEST_ASSERT_EQUAL_UINT8(SC_KILL_REASON_HB_TIMEOUT, SC_Relay_GetKillReason());
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-SC-010: Init */
    RUN_TEST(test_Relay_Init_low);

    /* SWR-SC-010: Energize / De-energize */
    RUN_TEST(test_Relay_Energize);
    RUN_TEST(test_Relay_DeEnergize);

    /* SWR-SC-011: Kill Latch */
    RUN_TEST(test_Relay_kill_latch);

    /* SWR-SC-035: E-Stop trigger (GAP-SC-001) */
    RUN_TEST(test_Relay_trigger_estop);
    RUN_TEST(test_Relay_trigger_estop_priority_over_heartbeat);

    /* SWR-SC-012: Triggers */
    RUN_TEST(test_Relay_trigger_heartbeat);
    RUN_TEST(test_Relay_trigger_plausibility);
    RUN_TEST(test_Relay_trigger_selftest);
    RUN_TEST(test_Relay_trigger_esm);
    RUN_TEST(test_Relay_trigger_readback_mismatch);
    RUN_TEST(test_Relay_readback_resets);
    RUN_TEST(test_Relay_no_trigger);

    /* SWR-SC-036: Bus silence trigger (GAP-SC-003) */
    RUN_TEST(test_Relay_trigger_bus_silence);

    /* SWR-SC-003: E2E persistent failure trigger (GAP-SC-002) */
    RUN_TEST(test_Relay_trigger_e2e_fail);
    RUN_TEST(test_Relay_trigger_e2e_priority_below_plausibility);
    RUN_TEST(test_Relay_trigger_e2e_priority_above_selftest);

    /* SSR-SC-018: Creep guard trigger */
    RUN_TEST(test_Relay_trigger_creep_guard);

    /* Hardened tests — boundary values, fault injection */
    RUN_TEST(test_relay_trigger_can_busoff);
    RUN_TEST(test_relay_trigger_multiple_simultaneous);
    RUN_TEST(test_relay_kill_survives_double_init);
    RUN_TEST(test_relay_deenergize_without_energize);
    RUN_TEST(test_relay_readback_exactly_1_no_kill);
    RUN_TEST(test_relay_check_triggers_not_energized);

    /* Kill reason getter */
    RUN_TEST(test_relay_get_kill_reason);

    return UNITY_END();
}
