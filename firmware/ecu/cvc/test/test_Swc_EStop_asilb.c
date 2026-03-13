/**
 * @file    test_Swc_EStop.c
 * @brief   Unit tests for Emergency Stop detection SWC
 * @date    2026-02-21
 *
 * @verifies SWR-CVC-018, SWR-CVC-019, SWR-CVC-020
 *
 * Tests E-stop detection, debounce, latch behaviour, CAN broadcast,
 * DTC reporting, and fail-safe on read failure.
 */
#include "unity.h"

/* ====================================================================
 * Local type definitions (self-contained test — no BSW headers)
 * ==================================================================== */

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef uint8          Std_ReturnType;
typedef uint8          boolean;

#define E_OK      0u
#define E_NOT_OK  1u
#define TRUE      1u
#define FALSE     0u
#define NULL_PTR  ((void*)0)
#define STD_HIGH  0x01u
#define STD_LOW   0x00u

/* Prevent BSW headers from redefining types */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_ESTOP_H
#define CVC_CFG_H

/* Prevent real Dem.h / E2E.h from being pulled in through Swc_EStop.c —
 * the mocks below provide the required function definitions.
 * DEM_EVENT_STATUS_FAILED is defined here because it is used both in
 * Swc_EStop.c and in this test file, and Dem.h is now blocked. */
#define DEM_H
#define E2E_H
#define DEM_EVENT_STATUS_FAILED  1u

/* Signal/DTC IDs (must match Cvc_Cfg.h) */
#define CVC_SIG_ESTOP_ACTIVE      22u
#define CVC_DTC_ESTOP_ACTIVATED   12u
#define CVC_COM_TX_ESTOP           0u

/* ====================================================================
 * Mock: IoHwAb_ReadEStop
 * ==================================================================== */

static uint8          mock_estop_state;
static Std_ReturnType mock_estop_read_result;

Std_ReturnType IoHwAb_ReadEStop(uint8* State)
{
    if (State != NULL_PTR) {
        *State = mock_estop_state;
    }
    return mock_estop_read_result;
}

/* ====================================================================
 * Mock: Rte_Write
 * ==================================================================== */

static uint16 mock_rte_write_sig;
static uint32 mock_rte_write_val;
static uint8  mock_rte_write_count;

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    mock_rte_write_sig = SignalId;
    mock_rte_write_val = Data;
    mock_rte_write_count++;
    return E_OK;
}

/* ====================================================================
 * Mock: Com_SendSignal
 * ==================================================================== */

static uint8 mock_com_send_count;
static uint8 mock_com_send_sig_id;

Std_ReturnType Com_SendSignal(uint8 SignalId, const void* SignalDataPtr)
{
    mock_com_send_sig_id = SignalId;
    mock_com_send_count++;
    (void)SignalDataPtr;
    return E_OK;
}

/* ====================================================================
 * Mock: E2E_Protect
 * ==================================================================== */

static uint8 mock_e2e_protect_count;

Std_ReturnType E2E_Protect(const void* Config, void* State,
                           uint8* DataPtr, uint16 Length)
{
    mock_e2e_protect_count++;
    (void)Config;
    (void)State;
    (void)DataPtr;
    (void)Length;
    return E_OK;
}

/* ====================================================================
 * Mock: Dem_ReportErrorStatus
 * ==================================================================== */

static uint8 mock_dem_event_id;
static uint8 mock_dem_event_status;
static uint8 mock_dem_report_count;

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_event_id = EventId;
    mock_dem_event_status = EventStatus;
    mock_dem_report_count++;
}

/* ====================================================================
 * Include SWC under test (source inclusion for test build)
 * ==================================================================== */

#include "../src/Swc_EStop.c"

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_estop_state       = STD_LOW;
    mock_estop_read_result = E_OK;
    mock_rte_write_sig     = 0u;
    mock_rte_write_val     = 0u;
    mock_rte_write_count   = 0u;
    mock_com_send_count    = 0u;
    mock_com_send_sig_id   = 0xFFu;
    mock_e2e_protect_count = 0u;
    mock_dem_event_id      = 0xFFu;
    mock_dem_event_status  = 0xFFu;
    mock_dem_report_count  = 0u;

    Swc_EStop_Init();
}

void tearDown(void) { }

/* ====================================================================
 * SWR-CVC-018: E-stop detection, debounce, fail-safe
 * ==================================================================== */

/** @verifies SWR-CVC-018 */
void test_EStop_Init_inactive(void)
{
    TEST_ASSERT_EQUAL(FALSE, Swc_EStop_IsActive());
}

/** @verifies SWR-CVC-018 */
void test_EStop_Detection_after_debounce(void)
{
    mock_estop_state = STD_HIGH;

    /* One cycle = debounce threshold (1 at 10ms) */
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(TRUE, Swc_EStop_IsActive());
}

/** @verifies SWR-CVC-018 */
void test_EStop_Debounce_transient_no_activation(void)
{
    /* HIGH for 0 full debounce cycles — released before threshold */
    mock_estop_state = STD_HIGH;
    /* Do NOT call MainFunction — button pressed but not yet sampled */
    /* Now release before first sample */
    mock_estop_state = STD_LOW;
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(FALSE, Swc_EStop_IsActive());
}

/** @verifies SWR-CVC-018 */
void test_EStop_RTE_write_on_activation(void)
{
    mock_estop_state = STD_HIGH;
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(CVC_SIG_ESTOP_ACTIVE, mock_rte_write_sig);
    TEST_ASSERT_EQUAL(TRUE, mock_rte_write_val);
}

/** @verifies SWR-CVC-018 */
void test_EStop_No_false_activation_when_low(void)
{
    mock_estop_state = STD_LOW;

    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(FALSE, Swc_EStop_IsActive());
    TEST_ASSERT_EQUAL(0u, mock_com_send_count);
}

/** @verifies SWR-CVC-018 */
void test_EStop_ReadFailure_failsafe_active(void)
{
    mock_estop_read_result = E_NOT_OK;

    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(TRUE, Swc_EStop_IsActive());
}

/* ====================================================================
 * SWR-CVC-019: E-stop CAN broadcast
 * ==================================================================== */

/** @verifies SWR-CVC-019 */
void test_EStop_Broadcast_on_activation(void)
{
    mock_estop_state = STD_HIGH;
    Swc_EStop_MainFunction();

    TEST_ASSERT_TRUE(mock_com_send_count >= 1u);
    TEST_ASSERT_EQUAL(CVC_COM_TX_ESTOP, mock_com_send_sig_id);
}

/** @verifies SWR-CVC-019 */
void test_EStop_Repeat_broadcasts_total_4(void)
{
    mock_estop_state = STD_HIGH;

    /* Cycle 1: activation + first broadcast */
    Swc_EStop_MainFunction();
    /* Cycles 2-4: repeat broadcasts */
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(4u, mock_com_send_count);

    /* Cycle 5: no more broadcasts */
    mock_com_send_count = 0u;
    Swc_EStop_MainFunction();
    TEST_ASSERT_EQUAL(0u, mock_com_send_count);
}

/* ====================================================================
 * SWR-CVC-020: E-stop latch and DTC
 * ==================================================================== */

/** @verifies SWR-CVC-020 */
void test_EStop_Latch_stays_active_after_release(void)
{
    mock_estop_state = STD_HIGH;
    Swc_EStop_MainFunction();

    /* Release button */
    mock_estop_state = STD_LOW;
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(TRUE, Swc_EStop_IsActive());
}

/** @verifies SWR-CVC-020 */
void test_EStop_DTC_reported_on_activation(void)
{
    mock_estop_state = STD_HIGH;
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(CVC_DTC_ESTOP_ACTIVATED, mock_dem_event_id);
    TEST_ASSERT_EQUAL(DEM_EVENT_STATUS_FAILED, mock_dem_event_status);
}

/* ====================================================================
 * HARDENED TESTS — Boundary Value, NULL Pointer, Fault Injection
 * ==================================================================== */

/* ------------------------------------------------------------------
 * SWR-CVC-018: Debounce boundary — exact threshold cycle count
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-018
 *  Equivalence class: VALID — activation on exactly the 1st cycle (threshold=1)
 *  Boundary: debounce_counter == ESTOP_DEBOUNCE_THRESHOLD (=1) */
void test_EStop_Debounce_exact_threshold_boundary(void)
{
    /* With threshold=1, first MainFunction cycle with HIGH should activate */
    mock_estop_state = STD_HIGH;
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(TRUE, Swc_EStop_IsActive());
}

/* ------------------------------------------------------------------
 * SWR-CVC-018: Consecutive read failures — fail-safe stays active
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-018
 *  Equivalence class: FAULT — multiple consecutive read failures
 *  Fault injection: IoHwAb_ReadEStop returns E_NOT_OK repeatedly */
void test_EStop_Consecutive_read_failures_stay_active(void)
{
    mock_estop_read_result = E_NOT_OK;

    Swc_EStop_MainFunction();
    TEST_ASSERT_EQUAL(TRUE, Swc_EStop_IsActive());

    /* Subsequent calls with failure — remains active (latched) */
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(TRUE, Swc_EStop_IsActive());
}

/* ------------------------------------------------------------------
 * SWR-CVC-018: Read failure after normal LOW — fail-safe activation
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-018
 *  Equivalence class: FAULT — read failure after stable LOW period
 *  Fault injection: sudden I2C/GPIO failure triggers fail-safe */
void test_EStop_ReadFailure_after_low_activates(void)
{
    /* Several cycles with LOW — no activation */
    mock_estop_state       = STD_LOW;
    mock_estop_read_result = E_OK;
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    TEST_ASSERT_EQUAL(FALSE, Swc_EStop_IsActive());

    /* Now read fails — fail-safe must activate */
    mock_estop_read_result = E_NOT_OK;
    Swc_EStop_MainFunction();
    TEST_ASSERT_EQUAL(TRUE, Swc_EStop_IsActive());
}

/* ------------------------------------------------------------------
 * SWR-CVC-019: E2E protection applied on every broadcast
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-019
 *  Equivalence class: VALID — E2E_Protect called for every CAN broadcast
 *  Boundary: exactly ESTOP_BROADCAST_COUNT (4) E2E protect calls */
void test_EStop_E2E_protect_called_on_all_broadcasts(void)
{
    mock_estop_state = STD_HIGH;

    /* 4 cycles = 4 broadcasts (activation + 3 repeats) */
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(4u, mock_e2e_protect_count);
}

/* ------------------------------------------------------------------
 * SWR-CVC-019: No broadcast after count exhausted
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-019
 *  Equivalence class: VALID — broadcast stops after 4 transmissions
 *  Boundary: repeat_counter == ESTOP_BROADCAST_COUNT */
void test_EStop_No_broadcast_after_count_exhausted(void)
{
    mock_estop_state = STD_HIGH;

    /* Exhaust all 4 broadcasts */
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(4u, mock_com_send_count);

    /* 5th, 6th cycles — no more broadcasts */
    mock_com_send_count = 0u;
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(0u, mock_com_send_count);
    /* But latch is still active */
    TEST_ASSERT_EQUAL(TRUE, Swc_EStop_IsActive());
}

/* ------------------------------------------------------------------
 * SWR-CVC-020: DTC only reported once (on first activation)
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-020
 *  Equivalence class: VALID — DTC reported exactly once
 *  Boundary: Dem_ReportErrorStatus called only on activation transition */
void test_EStop_DTC_reported_only_once(void)
{
    mock_estop_state = STD_HIGH;

    Swc_EStop_MainFunction();
    TEST_ASSERT_EQUAL(1u, mock_dem_report_count);

    /* Subsequent cycles with latch active — DTC should not repeat */
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();
    Swc_EStop_MainFunction();

    TEST_ASSERT_EQUAL(1u, mock_dem_report_count);
}

/* ------------------------------------------------------------------
 * SWR-CVC-018: MainFunction before Init — no action
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-018
 *  Equivalence class: INVALID — uninitialized module
 *  Boundary: initialized == FALSE */
void test_EStop_MainFunction_before_init_no_action(void)
{
    /* Reset the initialized flag by re-including source state
     * Since we include Swc_EStop.c, we can manipulate 'initialized' */
    initialized = FALSE;

    mock_estop_state = STD_HIGH;
    mock_com_send_count = 0u;
    Swc_EStop_MainFunction();

    /* Should do nothing — not initialized */
    TEST_ASSERT_EQUAL(FALSE, Swc_EStop_IsActive());
    TEST_ASSERT_EQUAL(0u, mock_com_send_count);

    /* Restore for tearDown */
    Swc_EStop_Init();
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_EStop_Init_inactive);
    RUN_TEST(test_EStop_Detection_after_debounce);
    RUN_TEST(test_EStop_Debounce_transient_no_activation);
    RUN_TEST(test_EStop_Broadcast_on_activation);
    RUN_TEST(test_EStop_Repeat_broadcasts_total_4);
    RUN_TEST(test_EStop_Latch_stays_active_after_release);
    RUN_TEST(test_EStop_DTC_reported_on_activation);
    RUN_TEST(test_EStop_RTE_write_on_activation);
    RUN_TEST(test_EStop_No_false_activation_when_low);
    RUN_TEST(test_EStop_ReadFailure_failsafe_active);

    /* --- HARDENED TESTS --- */
    RUN_TEST(test_EStop_Debounce_exact_threshold_boundary);
    RUN_TEST(test_EStop_Consecutive_read_failures_stay_active);
    RUN_TEST(test_EStop_ReadFailure_after_low_activates);
    RUN_TEST(test_EStop_E2E_protect_called_on_all_broadcasts);
    RUN_TEST(test_EStop_No_broadcast_after_count_exhausted);
    RUN_TEST(test_EStop_DTC_reported_only_once);
    RUN_TEST(test_EStop_MainFunction_before_init_no_action);

    return UNITY_END();
}
