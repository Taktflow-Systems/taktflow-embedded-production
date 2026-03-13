/**
 * @file    test_Swc_Dashboard.c
 * @brief   Unit tests for Swc_Dashboard — OLED dashboard display SWC
 * @date    2026-02-21
 *
 * @verifies SWR-CVC-027, SWR-CVC-028
 *
 * Tests dashboard initialization, vehicle state rendering, speed rendering,
 * refresh rate gating, I2C fault handling, and fault resilience.
 *
 * Mocks: Ssd1306_Init, Ssd1306_Clear, Ssd1306_SetCursor,
 *        Ssd1306_WriteString, Rte_Read, Dem_ReportErrorStatus
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef uint8           Std_ReturnType;
typedef unsigned char   boolean;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * Signal IDs (from Cvc_Cfg.h)
 * ================================================================== */

#define CVC_SIG_PEDAL_POSITION    18u
#define CVC_SIG_VEHICLE_STATE     20u
#define CVC_SIG_MOTOR_SPEED       25u
#define CVC_SIG_FAULT_MASK        26u

/* Vehicle states */
#define CVC_STATE_INIT             0u
#define CVC_STATE_RUN              1u
#define CVC_STATE_DEGRADED         2u
#define CVC_STATE_LIMP             3u
#define CVC_STATE_SAFE_STOP        4u
#define CVC_STATE_SHUTDOWN         5u
#define CVC_STATE_COUNT            6u

/* DTC event IDs */
#define CVC_DTC_DISPLAY_COMM      17u

/* DEM event status */
#define DEM_EVENT_STATUS_PASSED    0u
#define DEM_EVENT_STATUS_FAILED    1u

/* ==================================================================
 * Swc_Dashboard API declarations
 * ================================================================== */

extern void Swc_Dashboard_Init(void);
extern void Swc_Dashboard_MainFunction(void);

/* ==================================================================
 * Mock: Ssd1306_Init
 * ================================================================== */

static uint8           mock_ssd1306_init_called;
static Std_ReturnType  mock_ssd1306_init_result;

Std_ReturnType Ssd1306_Init(void)
{
    mock_ssd1306_init_called++;
    return mock_ssd1306_init_result;
}

/* ==================================================================
 * Mock: Ssd1306_Clear
 * ================================================================== */

static uint8  mock_ssd1306_clear_called;

void Ssd1306_Clear(void)
{
    mock_ssd1306_clear_called++;
}

/* ==================================================================
 * Mock: Ssd1306_SetCursor
 * ================================================================== */

static uint8  mock_ssd1306_cursor_page;
static uint8  mock_ssd1306_cursor_col;
static uint8  mock_ssd1306_cursor_call_count;

void Ssd1306_SetCursor(uint8 page, uint8 col)
{
    mock_ssd1306_cursor_page = page;
    mock_ssd1306_cursor_col  = col;
    mock_ssd1306_cursor_call_count++;
}

/* ==================================================================
 * Mock: Ssd1306_WriteString
 * ================================================================== */

#define MOCK_MAX_DISPLAY_STRINGS  8u
#define MOCK_MAX_STRING_LEN      32u

static char    mock_display_strings[MOCK_MAX_DISPLAY_STRINGS][MOCK_MAX_STRING_LEN];
static uint8   mock_display_write_count;
static Std_ReturnType mock_ssd1306_write_result;

Std_ReturnType Ssd1306_WriteString(const char* str)
{
    uint8 i;

    if (mock_display_write_count < MOCK_MAX_DISPLAY_STRINGS) {
        /* Copy string into tracking array */
        for (i = 0u; (i < (MOCK_MAX_STRING_LEN - 1u)) && (str[i] != '\0'); i++) {
            mock_display_strings[mock_display_write_count][i] = str[i];
        }
        mock_display_strings[mock_display_write_count][i] = '\0';
    }
    mock_display_write_count++;

    return mock_ssd1306_write_result;
}

/* ==================================================================
 * Mock: Rte_Read
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  32u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: Dem_ReportErrorStatus
 * ================================================================== */

#define MOCK_DEM_MAX_EVENTS  32u

static uint8  mock_dem_last_event_id;
static uint8  mock_dem_last_status;
static uint8  mock_dem_call_count;

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_call_count++;
    mock_dem_last_event_id = EventId;
    mock_dem_last_status   = EventStatus;
}

/* ==================================================================
 * Helper: check if any display string contains a substring
 * ================================================================== */

static uint8 display_contains(const char* substr)
{
    uint8 s;
    uint8 i;
    uint8 j;
    uint8 match;
    uint8 limit;

    limit = mock_display_write_count;
    if (limit > MOCK_MAX_DISPLAY_STRINGS) {
        limit = MOCK_MAX_DISPLAY_STRINGS;
    }

    for (s = 0u; s < limit; s++) {
        /* Check if substr exists in mock_display_strings[s] */
        for (i = 0u; mock_display_strings[s][i] != '\0'; i++) {
            match = 1u;
            for (j = 0u; substr[j] != '\0'; j++) {
                if (mock_display_strings[s][i + j] != substr[j]) {
                    match = 0u;
                    break;
                }
                if (mock_display_strings[s][i + j] == '\0') {
                    match = 0u;
                    break;
                }
            }
            if ((match == 1u) && (substr[0] != '\0')) {
                return 1u;
            }
        }
    }
    return 0u;
}

/* ==================================================================
 * Test setup / teardown
 * ================================================================== */

void setUp(void)
{
    uint8 i;
    uint8 j;

    /* Reset Ssd1306 mocks */
    mock_ssd1306_init_called      = 0u;
    mock_ssd1306_init_result      = E_OK;
    mock_ssd1306_clear_called     = 0u;
    mock_ssd1306_cursor_page      = 0u;
    mock_ssd1306_cursor_col       = 0u;
    mock_ssd1306_cursor_call_count = 0u;
    mock_display_write_count      = 0u;
    mock_ssd1306_write_result     = E_OK;

    for (i = 0u; i < MOCK_MAX_DISPLAY_STRINGS; i++) {
        for (j = 0u; j < MOCK_MAX_STRING_LEN; j++) {
            mock_display_strings[i][j] = '\0';
        }
    }

    /* Reset RTE signals */
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }

    /* Default: vehicle in RUN state */
    mock_rte_signals[CVC_SIG_VEHICLE_STATE]  = CVC_STATE_RUN;
    mock_rte_signals[CVC_SIG_MOTOR_SPEED]    = 0u;
    mock_rte_signals[CVC_SIG_FAULT_MASK]     = 0u;
    mock_rte_signals[CVC_SIG_PEDAL_POSITION] = 0u;

    /* Reset DEM mock */
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;
    mock_dem_call_count    = 0u;

    /* Re-initialize dashboard for each test */
    Swc_Dashboard_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-CVC-027: Init calls Ssd1306_Init and Clear
 * ================================================================== */

/** @verifies SWR-CVC-027 — Init calls Ssd1306_Init and Ssd1306_Clear */
void test_Init_calls_ssd1306_init_and_clear(void)
{
    /* Init already called in setUp. Verify mock tracking. */
    TEST_ASSERT_TRUE(mock_ssd1306_init_called >= 1u);
    TEST_ASSERT_TRUE(mock_ssd1306_clear_called >= 1u);
}

/* ==================================================================
 * SWR-CVC-027: State display — RUN
 * ================================================================== */

/** @verifies SWR-CVC-027 — Vehicle state RUN renders "ST:RUN" */
void test_State_display_run(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = CVC_STATE_RUN;

    /* Run 20 cycles (10ms each) to trigger one 200ms refresh */
    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("ST:RUN"));
}

/* ==================================================================
 * SWR-CVC-027: State display — DEGRADED
 * ================================================================== */

/** @verifies SWR-CVC-027 — Vehicle state DEGRADED renders "ST:DEGD" */
void test_State_display_degraded(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = CVC_STATE_DEGRADED;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("ST:DEGD"));
}

/* ==================================================================
 * SWR-CVC-027: State display — SAFE_STOP
 * ================================================================== */

/** @verifies SWR-CVC-027 — Vehicle state SAFE_STOP renders "ST:SAFE" */
void test_State_display_safe_stop(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = CVC_STATE_SAFE_STOP;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("ST:SAFE"));
}

/* ==================================================================
 * SWR-CVC-028: Speed display
 * ================================================================== */

/** @verifies SWR-CVC-028 — Motor speed 1234 renders "SPD:1234" */
void test_Speed_display(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_MOTOR_SPEED] = 1234u;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("SPD:1234"));
}

/* ==================================================================
 * SWR-CVC-027: Refresh rate — only updated every 200ms
 * ================================================================== */

/** @verifies SWR-CVC-027 — Display only updated every 200ms (20 calls at 10ms) */
void test_Refresh_rate_200ms(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = CVC_STATE_RUN;

    /* Reset write count after init */
    mock_display_write_count = 0u;

    /* Run 20 cycles at 10ms each = 200ms — should trigger exactly 1 update */
    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    /* At least one line rendered (at least 1 WriteString call per update) */
    uint8 first_update_writes = mock_display_write_count;
    TEST_ASSERT_TRUE(first_update_writes > 0u);

    /* Reset and run only 19 cycles — should NOT trigger another update */
    mock_display_write_count = 0u;
    for (i = 0u; i < 19u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(0u, mock_display_write_count);
}

/* ==================================================================
 * SWR-CVC-028: I2C fault — persistent write fail sets display_fault
 * ================================================================== */

/** @verifies SWR-CVC-028 — Persistent I2C write failure reports DTC */
void test_I2c_fault_reports_dtc(void)
{
    uint8 i;

    /* Make write fail persistently */
    mock_ssd1306_write_result = E_NOT_OK;

    /* First refresh (20 calls) — retry_count increments to 1 */
    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    /* Second refresh (another 20 calls) — retry_count reaches 2, fault set */
    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    /* DTC should be reported for display comm failure */
    TEST_ASSERT_TRUE(mock_dem_call_count > 0u);
    TEST_ASSERT_EQUAL_UINT8(CVC_DTC_DISPLAY_COMM, mock_dem_last_event_id);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED, mock_dem_last_status);
}

/* ==================================================================
 * SWR-CVC-028: Display fault does not crash — MainFunction continues
 * ================================================================== */

/** @verifies SWR-CVC-028 — Display fault does not crash, MainFunction continues */
void test_Display_fault_does_not_crash(void)
{
    uint8 i;

    /* Trigger display fault (persistent write failure) */
    mock_ssd1306_write_result = E_NOT_OK;

    /* Two refresh cycles to trigger fault */
    for (i = 0u; i < 40u; i++) {
        Swc_Dashboard_MainFunction();
    }

    /* Now continue calling MainFunction — should not crash */
    mock_display_write_count = 0u;

    for (i = 0u; i < 40u; i++) {
        Swc_Dashboard_MainFunction();
    }

    /* Display fault should suppress rendering — no new writes */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_display_write_count);

    /* Reaching here without crash = test passes */
    TEST_ASSERT_TRUE(TRUE);
}

/* ==================================================================
 * HARDENED TESTS — Boundary Value, Fault Injection, Defensive Checks
 * ================================================================== */

/* ------------------------------------------------------------------
 * SWR-CVC-027: State display — all valid states rendered
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-027
 *  Equivalence class: VALID — INIT state renders "ST:INIT"
 *  Boundary: vehicle_state == CVC_STATE_INIT (0, minimum valid) */
void test_State_display_init(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = CVC_STATE_INIT;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("ST:INIT"));
}

/** @verifies SWR-CVC-027
 *  Equivalence class: VALID — LIMP state renders "ST:LIMP"
 *  Boundary: vehicle_state == CVC_STATE_LIMP (3) */
void test_State_display_limp(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = CVC_STATE_LIMP;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("ST:LIMP"));
}

/** @verifies SWR-CVC-027
 *  Equivalence class: VALID — SHUTDOWN state renders "ST:SHUT"
 *  Boundary: vehicle_state == CVC_STATE_SHUTDOWN (5, maximum valid) */
void test_State_display_shutdown(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = CVC_STATE_SHUTDOWN;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("ST:SHUT"));
}

/* ------------------------------------------------------------------
 * SWR-CVC-027: Invalid state — renders "????" fallback
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-027
 *  Equivalence class: INVALID — vehicle_state out of range
 *  Boundary: vehicle_state == CVC_STATE_COUNT (6, first invalid) */
void test_State_display_out_of_range_shows_fallback(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = 6u;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("ST:????"));
}

/** @verifies SWR-CVC-027
 *  Equivalence class: INVALID — vehicle_state at max uint32
 *  Boundary: vehicle_state == 0xFFFFFFFF */
void test_State_display_max_uint32_shows_fallback(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = 0xFFFFFFFFuL;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("ST:????"));
}

/* ------------------------------------------------------------------
 * SWR-CVC-027: Refresh boundary — exactly 19 cycles (no refresh)
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-027
 *  Equivalence class: VALID — 19 cycles = 190ms, no refresh yet
 *  Boundary: refresh_counter = 19 (one below threshold of 20) */
void test_Refresh_no_update_at_19_cycles(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = CVC_STATE_RUN;
    mock_display_write_count = 0u;

    /* Run 20 to trigger first refresh */
    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    /* Reset and run only 19 — no second refresh */
    mock_display_write_count = 0u;
    for (i = 0u; i < 19u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(0u, mock_display_write_count);

    /* One more cycle = exactly 20 — triggers refresh */
    Swc_Dashboard_MainFunction();
    TEST_ASSERT_TRUE(mock_display_write_count > 0u);
}

/* ------------------------------------------------------------------
 * SWR-CVC-028: Speed display — boundary values
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-028
 *  Equivalence class: VALID — speed = 0 (minimum)
 *  Boundary: motor_speed == 0 */
void test_Speed_display_zero(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_MOTOR_SPEED] = 0u;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("SPD:0"));
}

/** @verifies SWR-CVC-028
 *  Equivalence class: VALID — speed = 65535 (max uint16)
 *  Boundary: motor_speed == 0xFFFF */
void test_Speed_display_max_uint16(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_MOTOR_SPEED] = 65535u;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("SPD:65535"));
}

/* ------------------------------------------------------------------
 * SWR-CVC-028: Fault mask display — zero shows "OK"
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-028
 *  Equivalence class: VALID — no faults renders "FLT:OK"
 *  Boundary: fault_mask == 0 */
void test_Fault_display_no_faults_shows_ok(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_FAULT_MASK] = 0u;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("FLT:OK"));
}

/** @verifies SWR-CVC-028
 *  Equivalence class: VALID — single fault bit renders hex
 *  Boundary: fault_mask == 0x01 (minimum non-zero) */
void test_Fault_display_single_bit_shows_hex(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_FAULT_MASK] = 1u;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("FLT:1"));
}

/* ------------------------------------------------------------------
 * SWR-CVC-028: I2C fault — single failure is retried, not latched
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-028
 *  Equivalence class: VALID — 1 failed refresh recovers on next success
 *  Boundary: retry_count = 1 (below threshold of 2) */
void test_I2c_single_failure_recovers(void)
{
    uint8 i;

    /* First refresh fails */
    mock_ssd1306_write_result = E_NOT_OK;
    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    /* Second refresh succeeds — retry_count should reset */
    mock_ssd1306_write_result = E_OK;
    mock_display_write_count = 0u;
    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    /* Third refresh — should still render (not faulted) */
    mock_display_write_count = 0u;
    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(mock_display_write_count > 0u);
}

/* ------------------------------------------------------------------
 * SWR-CVC-028: DTC not re-reported after fault latch
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-028
 *  Equivalence class: VALID — DTC reported once on fault latch
 *  Boundary: display_fault transitions TRUE -> remains TRUE */
void test_DTC_reported_once_on_fault_latch(void)
{
    uint8 i;

    mock_ssd1306_write_result = E_NOT_OK;

    /* Two refresh cycles to trigger fault latch */
    for (i = 0u; i < 40u; i++) {
        Swc_Dashboard_MainFunction();
    }

    uint8 dtc_count_at_latch = mock_dem_call_count;
    TEST_ASSERT_TRUE(dtc_count_at_latch > 0u);

    /* Further cycles should not re-report DTC (display_fault latched) */
    for (i = 0u; i < 40u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_EQUAL_UINT8(dtc_count_at_latch, mock_dem_call_count);
}

/* ------------------------------------------------------------------
 * SWR-CVC-027: Pedal display — percentage rendering
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-027
 *  Equivalence class: VALID — pedal at 0 renders "PED:0%"
 *  Boundary: pedal_position == 0 (minimum) */
void test_Pedal_display_zero(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_PEDAL_POSITION] = 0u;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("PED:0%"));
}

/** @verifies SWR-CVC-027
 *  Equivalence class: VALID — pedal at 1000 renders "PED:100%"
 *  Boundary: pedal_position == 1000 (full scale) */
void test_Pedal_display_full_scale(void)
{
    uint8 i;

    mock_rte_signals[CVC_SIG_PEDAL_POSITION] = 1000u;

    for (i = 0u; i < 20u; i++) {
        Swc_Dashboard_MainFunction();
    }

    TEST_ASSERT_TRUE(display_contains("PED:100%"));
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-CVC-027: Dashboard initialization and state display */
    RUN_TEST(test_Init_calls_ssd1306_init_and_clear);
    RUN_TEST(test_State_display_run);
    RUN_TEST(test_State_display_degraded);
    RUN_TEST(test_State_display_safe_stop);

    /* SWR-CVC-028: Speed display and fault handling */
    RUN_TEST(test_Speed_display);
    RUN_TEST(test_Refresh_rate_200ms);
    RUN_TEST(test_I2c_fault_reports_dtc);
    RUN_TEST(test_Display_fault_does_not_crash);

    /* --- HARDENED TESTS --- */
    RUN_TEST(test_State_display_init);
    RUN_TEST(test_State_display_limp);
    RUN_TEST(test_State_display_shutdown);
    RUN_TEST(test_State_display_out_of_range_shows_fallback);
    RUN_TEST(test_State_display_max_uint32_shows_fallback);
    RUN_TEST(test_Refresh_no_update_at_19_cycles);
    RUN_TEST(test_Speed_display_zero);
    RUN_TEST(test_Speed_display_max_uint16);
    RUN_TEST(test_Fault_display_no_faults_shows_ok);
    RUN_TEST(test_Fault_display_single_bit_shows_hex);
    RUN_TEST(test_I2c_single_failure_recovers);
    RUN_TEST(test_DTC_reported_once_on_fault_latch);
    RUN_TEST(test_Pedal_display_zero);
    RUN_TEST(test_Pedal_display_full_scale);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_DASHBOARD_H
#define CVC_CFG_H
#define SSD1306_H
#define RTE_H
#define DEM_H

#include "../src/Swc_Dashboard.c"
