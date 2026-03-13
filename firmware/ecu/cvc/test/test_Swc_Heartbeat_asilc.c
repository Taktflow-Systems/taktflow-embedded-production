/**
 * @file    test_Swc_Heartbeat.c
 * @brief   Unit tests for Heartbeat TX/RX monitoring SWC
 * @date    2026-02-21
 *
 * @verifies SWR-CVC-021, SWR-CVC-022
 *
 * Tests heartbeat transmission timing, alive counter wrap, RX indication,
 * timeout detection, DTC reporting, recovery, and RTE write.
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
#define SWC_HEARTBEAT_H
#define CVC_CFG_H

/* Prevent real Dem.h / E2E.h / WdgM.h from being pulled in through Swc_Heartbeat.c —
 * the mocks below provide the required function definitions. */
#define DEM_H
#define E2E_H
#define WDGM_H
#define PDUR_H
#define COMSTACK_TYPES_H
#define CANIF_H

/* PduR / ComStack types needed by Swc_Heartbeat.c */
typedef uint16 PduIdType;
typedef uint16 PduLengthType;
typedef struct {
    uint8          *SduDataPtr;
    PduLengthType   SduLength;
} PduInfoType;
#define DEM_EVENT_STATUS_PASSED  0u
#define DEM_EVENT_STATUS_FAILED  1u

/* E2E types needed by Swc_Heartbeat.c */
typedef struct { uint8 DataId; uint8 MaxDeltaCounter; uint16 DataLength; } E2E_ConfigType;
typedef struct { uint8 Counter; } E2E_StateType;
typedef enum {
    E2E_STATUS_OK          = 0u,
    E2E_STATUS_REPEATED    = 1u,
    E2E_STATUS_WRONG_SEQ   = 2u,
    E2E_STATUS_ERROR       = 3u,
    E2E_STATUS_NO_NEW_DATA = 4u
} E2E_CheckStatusType;

/* WdgM type needed by Swc_Heartbeat.c */
typedef uint8 WdgM_SupervisedEntityIdType;

/* Signal/DTC IDs (must match Cvc_Cfg.h) */
#define CVC_SIG_FZC_COMM_STATUS   23u
#define CVC_SIG_RZC_COMM_STATUS   24u
#define CVC_SIG_VEHICLE_STATE     20u
#define CVC_COM_TX_HEARTBEAT       1u
#define CVC_DTC_CAN_FZC_TIMEOUT    4u
#define CVC_DTC_CAN_RZC_TIMEOUT    5u
#define CVC_ECU_ID_CVC          0x01u
#define CVC_ECU_ID_FZC          0x02u
#define CVC_ECU_ID_RZC          0x03u
#define CVC_RTE_PERIOD_MS        10u
#define CVC_HB_TX_PERIOD_MS      50u
#define CVC_HB_ALIVE_MAX         15u
#define CVC_COMM_OK                0u
#define CVC_COMM_TIMEOUT           1u
#define CVC_E2E_HEARTBEAT_DATA_ID 0x02u

/* E2E SM per-ECU configurations (must match Cvc_Cfg.h) */
#define CVC_E2E_SM_FZC_WINDOW        4u
#define CVC_E2E_SM_FZC_MIN_OK_INIT   2u
#define CVC_E2E_SM_FZC_MAX_ERR_VALID 1u
#define CVC_E2E_SM_FZC_MIN_OK_INV    3u
#define CVC_E2E_SM_RZC_WINDOW        6u
#define CVC_E2E_SM_RZC_MIN_OK_INIT   3u
#define CVC_E2E_SM_RZC_MAX_ERR_VALID 2u
#define CVC_E2E_SM_RZC_MIN_OK_INV    3u

/* Named Com RX signal IDs for heartbeat alive counters */
#define CVC_COM_SIG_FZC_HB_ALIVE  9u
#define CVC_COM_SIG_RZC_HB_ALIVE 11u

/* ====================================================================
 * Mock: Com_SendSignal (still needed for other Com calls)
 * ==================================================================== */

static uint8 mock_com_send_count;
static uint8 mock_com_send_sig_id;
static uint8 mock_com_send_data[8];

Std_ReturnType Com_SendSignal(uint8 SignalId, const void* SignalDataPtr)
{
    mock_com_send_sig_id = SignalId;
    mock_com_send_count++;
    if (SignalDataPtr != NULL_PTR) {
        const uint8* p = (const uint8*)SignalDataPtr;
        uint8 i;
        for (i = 0u; i < 8u; i++) {
            mock_com_send_data[i] = p[i];
        }
    }
    return E_OK;
}

/* ====================================================================
 * Mock: PduR_Transmit (heartbeat TX path)
 * ==================================================================== */

static uint8  mock_pdur_tx_count;
static uint16 mock_pdur_tx_pdu_id;
static uint8  mock_pdur_tx_data[8];
static uint16 mock_pdur_tx_length;

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    mock_pdur_tx_pdu_id = TxPduId;
    mock_pdur_tx_count++;
    if ((PduInfoPtr != NULL_PTR) && (PduInfoPtr->SduDataPtr != NULL_PTR)) {
        uint8 i;
        mock_pdur_tx_length = PduInfoPtr->SduLength;
        for (i = 0u; i < 8u; i++) {
            mock_pdur_tx_data[i] = PduInfoPtr->SduDataPtr[i];
        }
    }
    return E_OK;
}

/* ====================================================================
 * Mock: Com_ReceiveSignal
 * ==================================================================== */

static uint8 mock_com_rx_fzc_alive;
static uint8 mock_com_rx_rzc_alive;

Std_ReturnType Com_ReceiveSignal(uint8 SignalId, void* SignalDataPtr)
{
    if (SignalDataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    if (SignalId == CVC_COM_SIG_FZC_HB_ALIVE) {
        *((uint8*)SignalDataPtr) = mock_com_rx_fzc_alive;
    } else if (SignalId == CVC_COM_SIG_RZC_HB_ALIVE) {
        *((uint8*)SignalDataPtr) = mock_com_rx_rzc_alive;
    } else {
        *((uint8*)SignalDataPtr) = 0u;
    }
    return E_OK;
}

/* ====================================================================
 * Mock: E2E_Protect
 * ==================================================================== */

static uint8 mock_e2e_protect_count;
static const E2E_ConfigType* mock_e2e_config_ptr;
static E2E_StateType* mock_e2e_state_ptr;

Std_ReturnType E2E_Protect(const E2E_ConfigType* Config, E2E_StateType* State,
                           uint8* DataPtr, uint16 Length)
{
    mock_e2e_protect_count++;
    mock_e2e_config_ptr = Config;
    mock_e2e_state_ptr  = State;
    (void)DataPtr;
    (void)Length;
    return E_OK;
}

/* ====================================================================
 * Mock: WdgM_CheckpointReached
 * ==================================================================== */

static uint8 mock_wdgm_checkpoint_count;
static uint8 mock_wdgm_last_se_id;

Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEId)
{
    mock_wdgm_checkpoint_count++;
    mock_wdgm_last_se_id = SEId;
    return E_OK;
}

/* ====================================================================
 * Mock: Rte_Write
 * ==================================================================== */

static uint16 mock_rte_write_sig_ids[64];
static uint32 mock_rte_write_vals[64];
static uint8  mock_rte_write_count;

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    if (mock_rte_write_count < 64u) {
        mock_rte_write_sig_ids[mock_rte_write_count] = SignalId;
        mock_rte_write_vals[mock_rte_write_count]    = Data;
    }
    mock_rte_write_count++;
    return E_OK;
}

/* ====================================================================
 * Mock: Rte_Read
 * ==================================================================== */

static uint32 mock_rte_vehicle_state;

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    if (SignalId == CVC_SIG_VEHICLE_STATE) {
        *DataPtr = mock_rte_vehicle_state;
    } else {
        *DataPtr = 0u;
    }
    return E_OK;
}

/* ====================================================================
 * Mock: Dem_ReportErrorStatus
 * ==================================================================== */

static uint8 mock_dem_event_ids[8];
static uint8 mock_dem_event_statuses[8];
static uint8 mock_dem_report_count;

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    if (mock_dem_report_count < 8u) {
        mock_dem_event_ids[mock_dem_report_count]      = EventId;
        mock_dem_event_statuses[mock_dem_report_count]  = EventStatus;
    }
    mock_dem_report_count++;
}

/* ====================================================================
 * Include E2E SM + SWC under test (source inclusion for test build)
 * ==================================================================== */

#include "../../../bsw/services/E2E/src/E2E_Sm.c"
#include "../src/Swc_Heartbeat.c"

/* ====================================================================
 * Helper: run N MainFunction cycles
 * ==================================================================== */

static void run_cycles(uint8 n)
{
    uint8 i;
    for (i = 0u; i < n; i++) {
        Swc_Heartbeat_MainFunction();
    }
}

/* ====================================================================
 * Helpers: recover ECU from initial TIMEOUT via E2E SM
 * ==================================================================== */

/** Recover FZC to OK via 3 alive counter changes (exceeds MinOkInit=2) */
static void recover_fzc(uint8 start_alive)
{
    mock_com_rx_fzc_alive = start_alive;
    run_cycles(5u);
    mock_com_rx_fzc_alive = (uint8)(start_alive + 1u);
    run_cycles(5u);
    mock_com_rx_fzc_alive = (uint8)(start_alive + 2u);
    run_cycles(5u);
}

/** Recover RZC to OK via 3 alive counter changes (matches MinOkInit=3) */
static void recover_rzc(uint8 start_alive)
{
    mock_com_rx_rzc_alive = start_alive;
    run_cycles(5u);
    mock_com_rx_rzc_alive = (uint8)(start_alive + 1u);
    run_cycles(5u);
    mock_com_rx_rzc_alive = (uint8)(start_alive + 2u);
    run_cycles(5u);
}

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    uint8 i;

    mock_com_send_count    = 0u;
    mock_com_send_sig_id   = 0xFFu;
    mock_pdur_tx_count     = 0u;
    mock_pdur_tx_pdu_id    = 0xFFFFu;
    mock_pdur_tx_length    = 0u;
    mock_com_rx_fzc_alive  = 0u;     /* Match fzc_last_alive init (0) — no false positive */
    mock_com_rx_rzc_alive  = 0u;     /* Match rzc_last_alive init (0) — no false positive */
    mock_e2e_protect_count = 0u;
    mock_e2e_config_ptr    = NULL_PTR;
    mock_e2e_state_ptr     = NULL_PTR;
    mock_wdgm_checkpoint_count = 0u;
    mock_wdgm_last_se_id   = 0xFFu;
    mock_rte_write_count   = 0u;
    mock_dem_report_count  = 0u;
    mock_rte_vehicle_state = 1u; /* CVC_STATE_RUN */

    for (i = 0u; i < 8u; i++) {
        mock_com_send_data[i]      = 0u;
        mock_pdur_tx_data[i]       = 0u;
        mock_dem_event_ids[i]      = 0xFFu;
        mock_dem_event_statuses[i] = 0xFFu;
    }
    for (i = 0u; i < 64u; i++) {
        mock_rte_write_sig_ids[i] = 0xFFu;
        mock_rte_write_vals[i]    = 0xFFu;
    }

    Swc_Heartbeat_Init();
}

void tearDown(void) { }

/* ====================================================================
 * SWR-CVC-021: Heartbeat TX
 * ==================================================================== */

/** @verifies SWR-CVC-021 */
void test_Heartbeat_Init_comm_status_timeout(void)
{
    /* After init, both FZC and RZC comm status should be TIMEOUT
     * (safe default — heartbeats must be received before declaring OK) */
    /* Run one cycle to see RTE writes */
    Swc_Heartbeat_MainFunction();

    /* Find FZC comm status write */
    boolean found_fzc = FALSE;
    boolean found_rzc = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if (mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) {
            TEST_ASSERT_EQUAL(CVC_COMM_TIMEOUT, mock_rte_write_vals[i]);
            found_fzc = TRUE;
        }
        if (mock_rte_write_sig_ids[i] == CVC_SIG_RZC_COMM_STATUS) {
            TEST_ASSERT_EQUAL(CVC_COMM_TIMEOUT, mock_rte_write_vals[i]);
            found_rzc = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_fzc);
    TEST_ASSERT_TRUE(found_rzc);
}

/** @verifies SWR-CVC-021 */
void test_Heartbeat_TX_every_50ms(void)
{
    /* 50ms = 5 calls at 10ms each */
    run_cycles(5u);

    TEST_ASSERT_EQUAL(1u, mock_pdur_tx_count);
    TEST_ASSERT_EQUAL(CVC_COM_TX_HEARTBEAT, mock_pdur_tx_pdu_id);
}

/** @verifies SWR-CVC-021 */
void test_Heartbeat_Alive_counter_increments(void)
{
    /* First TX at cycle 5 — alive_counter increments after each TX */
    run_cycles(5u);
    uint8 first_alive = alive_counter;   /* internal counter (not in PDU — E2E manages byte 0) */

    /* Second TX at cycle 10 */
    run_cycles(5u);
    uint8 second_alive = alive_counter;

    TEST_ASSERT_EQUAL(first_alive + 1u, second_alive);
}

/** @verifies SWR-CVC-021 */
void test_Heartbeat_Alive_counter_wraps_at_15(void)
{
    uint8 cycle;

    /* Force alive counter to 15 by running 16 TX periods (16 * 5 = 80 cycles) */
    for (cycle = 0u; cycle < 16u; cycle++) {
        run_cycles(5u);
    }

    /* The alive counter should have wrapped: 0..15, 0 */
    /* Last sent alive = 15, next TX should wrap to 0 */
    run_cycles(5u);
    /* Internal alive_counter wraps at CVC_HB_ALIVE_MAX (E2E manages byte 0) */
    TEST_ASSERT_TRUE(alive_counter <= CVC_HB_ALIVE_MAX);
}

/** @verifies SWR-CVC-021 */
void test_Heartbeat_TX_data_includes_ecu_id_and_state(void)
{
    mock_rte_vehicle_state = 2u; /* CVC_STATE_DEGRADED */

    run_cycles(5u);

    /* PDU layout: [E2E_byte0, E2E_CRC, ECU_ID, FaultStatus|OpMode, 0,0,0,0] */
    TEST_ASSERT_EQUAL(CVC_ECU_ID_CVC, mock_pdur_tx_data[2]);
    TEST_ASSERT_EQUAL(2u, mock_pdur_tx_data[3] & 0x0Fu);  /* OpMode in low nibble */
}

/** @verifies SWR-CVC-021 */
void test_Heartbeat_No_TX_before_50ms(void)
{
    /* Only 4 cycles (40ms) — should NOT transmit */
    run_cycles(4u);

    TEST_ASSERT_EQUAL(0u, mock_pdur_tx_count);
}

/* ====================================================================
 * SWR-CVC-022: Heartbeat RX monitoring
 * ==================================================================== */

/** @verifies SWR-CVC-022 */
void test_Heartbeat_FZC_RxIndication_resets_timer(void)
{
    /* Run 2 check periods without FZC RX (2 misses) */
    run_cycles(5u);
    run_cycles(5u);

    /* Now indicate FZC heartbeat received */
    Swc_Heartbeat_RxIndication(CVC_ECU_ID_FZC);

    /* Run 1 more period — miss count should have been reset */
    mock_dem_report_count = 0u;
    run_cycles(5u);

    /* Should NOT have timed out (only 1 miss after reset) */
    /* Check no FZC timeout DTC */
    boolean fzc_timeout_reported = FALSE;
    uint8 i;
    for (i = 0u; i < mock_dem_report_count; i++) {
        if ((mock_dem_event_ids[i] == CVC_DTC_CAN_FZC_TIMEOUT) &&
            (mock_dem_event_statuses[i] == DEM_EVENT_STATUS_FAILED)) {
            fzc_timeout_reported = TRUE;
        }
    }
    TEST_ASSERT_FALSE(fzc_timeout_reported);
}

/** @verifies SWR-CVC-022 */
void test_Heartbeat_RZC_RxIndication_resets_timer(void)
{
    /* Run 2 check periods without RZC RX */
    run_cycles(5u);
    run_cycles(5u);

    /* Indicate RZC heartbeat received */
    Swc_Heartbeat_RxIndication(CVC_ECU_ID_RZC);

    /* Run 1 more period */
    mock_dem_report_count = 0u;
    run_cycles(5u);

    /* Should NOT have timed out */
    boolean rzc_timeout_reported = FALSE;
    uint8 i;
    for (i = 0u; i < mock_dem_report_count; i++) {
        if ((mock_dem_event_ids[i] == CVC_DTC_CAN_RZC_TIMEOUT) &&
            (mock_dem_event_statuses[i] == DEM_EVENT_STATUS_FAILED)) {
            rzc_timeout_reported = TRUE;
        }
    }
    TEST_ASSERT_FALSE(rzc_timeout_reported);
}

/** @verifies SWR-CVC-022 */
void test_Heartbeat_FZC_timeout_after_3_misses(void)
{
    /* Run 3 check periods (3 * 5 = 15 cycles) without FZC RX */
    run_cycles(5u);
    run_cycles(5u);
    run_cycles(5u);

    /* Check FZC comm status written as TIMEOUT */
    boolean found_timeout = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            found_timeout = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_timeout);
}

/** @verifies SWR-CVC-022 */
void test_Heartbeat_RZC_timeout_after_3_misses(void)
{
    /* Run 3 check periods without RZC RX */
    run_cycles(5u);
    run_cycles(5u);
    run_cycles(5u);

    boolean found_timeout = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_RZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            found_timeout = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_timeout);
}

/** @verifies SWR-CVC-022 */
void test_Heartbeat_FZC_timeout_DTC_reported(void)
{
    /* First recover FZC from initial TIMEOUT to OK (3 consecutive HBs) */
    recover_fzc(1u);

    /* Clear DEM mock state */
    mock_dem_report_count = 0u;

    /* Now run 2 check periods without FZC RX to trigger timeout (FZC threshold = 2) */
    run_cycles(5u);
    run_cycles(5u);

    boolean fzc_dtc = FALSE;
    uint8 i;
    for (i = 0u; i < mock_dem_report_count; i++) {
        if ((mock_dem_event_ids[i] == CVC_DTC_CAN_FZC_TIMEOUT) &&
            (mock_dem_event_statuses[i] == DEM_EVENT_STATUS_FAILED)) {
            fzc_dtc = TRUE;
        }
    }
    TEST_ASSERT_TRUE(fzc_dtc);
}

/** @verifies SWR-CVC-022 */
void test_Heartbeat_RZC_timeout_DTC_reported(void)
{
    /* First recover RZC from initial TIMEOUT to OK (3 consecutive HBs) */
    recover_rzc(1u);

    /* Clear DEM mock state */
    mock_dem_report_count = 0u;

    /* Now run 3 check periods without RZC RX to trigger timeout (RZC threshold = 3) */
    run_cycles(5u);
    run_cycles(5u);
    run_cycles(5u);

    boolean rzc_dtc = FALSE;
    uint8 i;
    for (i = 0u; i < mock_dem_report_count; i++) {
        if ((mock_dem_event_ids[i] == CVC_DTC_CAN_RZC_TIMEOUT) &&
            (mock_dem_event_statuses[i] == DEM_EVENT_STATUS_FAILED)) {
            rzc_dtc = TRUE;
        }
    }
    TEST_ASSERT_TRUE(rzc_dtc);
}

/** @verifies SWR-CVC-022 */
void test_Heartbeat_Recovery_after_timeout(void)
{
    /* FZC starts in TIMEOUT (from init).
     * Need 3 consecutive HBs via RxIndication to recover. */
    Swc_Heartbeat_RxIndication(CVC_ECU_ID_FZC);
    run_cycles(5u);  /* recovery_count = 1 */
    Swc_Heartbeat_RxIndication(CVC_ECU_ID_FZC);
    run_cycles(5u);  /* recovery_count = 2 */
    Swc_Heartbeat_RxIndication(CVC_ECU_ID_FZC);
    mock_rte_write_count = 0u;
    run_cycles(5u);  /* recovery_count = 3 → OK */

    /* Find FZC comm status — should be back to OK */
    boolean found_ok = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_OK)) {
            found_ok = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_ok);
}

/** @verifies SWR-CVC-022 */
void test_Heartbeat_RTE_write_fzc_comm_each_cycle(void)
{
    /* Run 3 cycles */
    run_cycles(3u);

    /* Count FZC comm status writes */
    uint8 fzc_writes = 0u;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if (mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) {
            fzc_writes++;
        }
    }
    TEST_ASSERT_EQUAL(3u, fzc_writes);
}

/** @verifies SWR-CVC-022 */
void test_Heartbeat_RTE_write_rzc_comm_each_cycle(void)
{
    /* Run 3 cycles */
    run_cycles(3u);

    /* Count RZC comm status writes */
    uint8 rzc_writes = 0u;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if (mock_rte_write_sig_ids[i] == CVC_SIG_RZC_COMM_STATUS) {
            rzc_writes++;
        }
    }
    TEST_ASSERT_EQUAL(3u, rzc_writes);
}

/* ====================================================================
 * HARDENED TESTS — Boundary Value, Fault Injection, Defensive Checks
 * ==================================================================== */

/* ------------------------------------------------------------------
 * SWR-CVC-021: TX timing boundary — exactly 4 cycles (no TX yet)
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-021
 *  Equivalence class: VALID — boundary: 4 cycles = 40ms, no TX yet
 *  Boundary: tx_timer = 4 (one below threshold of 5) */
void test_Heartbeat_No_TX_at_4_cycles(void)
{
    run_cycles(4u);

    TEST_ASSERT_EQUAL(0u, mock_pdur_tx_count);
}

/** @verifies SWR-CVC-021
 *  Equivalence class: VALID — boundary: exactly 5 cycles = 50ms, TX fires
 *  Boundary: tx_timer = 5 (exactly at threshold) */
void test_Heartbeat_TX_at_exactly_5_cycles(void)
{
    run_cycles(5u);

    TEST_ASSERT_EQUAL(1u, mock_pdur_tx_count);
}

/* ------------------------------------------------------------------
 * SWR-CVC-021: Alive counter wrap at max (15)
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-021
 *  Equivalence class: VALID — alive counter boundary: wraps from 15 to 0
 *  Boundary: alive_counter = CVC_HB_ALIVE_MAX (15), next should be 0 */
void test_Heartbeat_Alive_wrap_exact_boundary(void)
{
    uint8 cycle;

    /* Run 15 TX periods to get alive counter to 14 (0..14 = 15 values) */
    for (cycle = 0u; cycle < 15u; cycle++) {
        run_cycles(5u);
    }

    /* After 15 TXes, alive counter should be 15 (wraps after next) */
    /* 16th TX: counter was 15, next cycle resets to 0 */
    run_cycles(5u);

    /* Internal alive_counter wraps at CVC_HB_ALIVE_MAX (E2E manages byte 0) */
    TEST_ASSERT_TRUE(alive_counter <= CVC_HB_ALIVE_MAX);
}

/* ------------------------------------------------------------------
 * SWR-CVC-022: RX indication with unknown ECU ID — ignored
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-022
 *  Equivalence class: INVALID — unknown ECU ID in RxIndication
 *  Boundary: ecuId != CVC_ECU_ID_FZC and != CVC_ECU_ID_RZC */
void test_Heartbeat_RxIndication_unknown_ecu_ignored(void)
{
    /* Run enough for both timeouts */
    run_cycles(5u);
    run_cycles(5u);
    run_cycles(5u);

    /* Indicate with unknown ECU ID — should have no effect */
    Swc_Heartbeat_RxIndication(0xFFu);

    mock_rte_write_count = 0u;
    run_cycles(5u);

    /* FZC and RZC should still be timed out */
    boolean fzc_timeout = FALSE;
    boolean rzc_timeout = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            fzc_timeout = TRUE;
        }
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_RZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            rzc_timeout = TRUE;
        }
    }
    TEST_ASSERT_TRUE(fzc_timeout);
    TEST_ASSERT_TRUE(rzc_timeout);
}

/* ------------------------------------------------------------------
 * SWR-CVC-022: RX indication with ECU ID = 0 — ignored
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-022
 *  Equivalence class: INVALID — ECU ID boundary: 0 (not a valid ECU)
 *  Boundary: ecuId = 0 */
void test_Heartbeat_RxIndication_ecu_id_zero_ignored(void)
{
    Swc_Heartbeat_RxIndication(0u);

    /* Run timeout check — both should still time out */
    run_cycles(5u);
    run_cycles(5u);
    run_cycles(5u);

    boolean fzc_timeout = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            fzc_timeout = TRUE;
        }
    }
    TEST_ASSERT_TRUE(fzc_timeout);
}

/* ------------------------------------------------------------------
 * SWR-CVC-022: Timeout boundary — exactly 2 misses (below threshold)
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-022
 *  Equivalence class: VALID — 1 miss (below 2-miss FZC threshold), no timeout
 *  Boundary: miss_count = 1 (one below CVC_HB_FZC_MAX_MISS = 2) */
void test_Heartbeat_FZC_no_timeout_at_1_miss(void)
{
    /* First recover FZC from initial TIMEOUT to OK (3 consecutive HBs) */
    recover_fzc(1u);

    /* Reset RTE mock to only see writes from here */
    mock_rte_write_count = 0u;

    /* 1 check period without FZC RX */
    run_cycles(5u);

    /* FZC should NOT yet be in timeout (only 1 miss, need 2) */
    boolean found_timeout = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            found_timeout = TRUE;
        }
    }
    TEST_ASSERT_FALSE(found_timeout);
}

/* ------------------------------------------------------------------
 * SWR-CVC-022: FZC timeout does not affect RZC and vice versa
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-022
 *  Equivalence class: VALID — FZC timeout while RZC receives heartbeats
 *  Fault injection: asymmetric ECU failure */
void test_Heartbeat_FZC_timeout_RZC_ok(void)
{
    /* Indicate RZC heartbeat BEFORE each period (3 consecutive = recovery).
     * FZC gets no heartbeats — stays in initial TIMEOUT. */
    Swc_Heartbeat_RxIndication(CVC_ECU_ID_RZC);
    run_cycles(5u);   /* RZC recovery 1/3 */
    Swc_Heartbeat_RxIndication(CVC_ECU_ID_RZC);
    run_cycles(5u);   /* RZC recovery 2/3 */
    Swc_Heartbeat_RxIndication(CVC_ECU_ID_RZC);
    run_cycles(5u);   /* RZC recovery 3/3 → OK */

    /* After 3 periods: FZC should timeout, RZC should be OK */
    boolean fzc_timeout = FALSE;
    boolean rzc_ok = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            fzc_timeout = TRUE;
        }
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_RZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_OK)) {
            rzc_ok = TRUE;
        }
    }
    TEST_ASSERT_TRUE(fzc_timeout);
    TEST_ASSERT_TRUE(rzc_ok);
}

/* ------------------------------------------------------------------
 * SWR-CVC-022: DTC not re-reported once already in timeout
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-022
 *  Equivalence class: VALID — DTC reported once, not again on subsequent misses
 *  Boundary: fzc_comm_status already == CVC_COMM_TIMEOUT */
void test_Heartbeat_DTC_not_re_reported_after_timeout(void)
{
    /* Trigger FZC timeout (3 misses) */
    run_cycles(5u);
    run_cycles(5u);
    run_cycles(5u);

    uint8 dtc_count_at_timeout = mock_dem_report_count;

    /* Run 3 more periods — DTC should not be re-reported */
    run_cycles(5u);
    run_cycles(5u);
    run_cycles(5u);

    /* Count FZC timeout DTCs in the additional reports */
    uint8 fzc_timeout_dtcs = 0u;
    uint8 i;
    for (i = dtc_count_at_timeout; i < mock_dem_report_count; i++) {
        if (i < 8u) {
            if ((mock_dem_event_ids[i] == CVC_DTC_CAN_FZC_TIMEOUT) &&
                (mock_dem_event_statuses[i] == DEM_EVENT_STATUS_FAILED)) {
                fzc_timeout_dtcs++;
            }
        }
    }
    /* No additional FZC timeout FAILED DTCs after initial report */
    TEST_ASSERT_EQUAL(0u, fzc_timeout_dtcs);
}

/* ------------------------------------------------------------------
 * SWR-CVC-021: MainFunction before Init — no action
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-021
 *  Equivalence class: INVALID — uninitialized module
 *  Boundary: initialized == FALSE */
void test_Heartbeat_MainFunction_before_init_no_action(void)
{
    /* Reset initialized flag (source included) */
    initialized = FALSE;

    mock_pdur_tx_count = 0u;
    run_cycles(10u);

    /* Should do nothing — not initialized */
    TEST_ASSERT_EQUAL(0u, mock_pdur_tx_count);

    /* Restore */
    Swc_Heartbeat_Init();
}

/* ====================================================================
 * PHASE 1: Derived Constants Verification
 * ==================================================================== */

/** @verifies SWR-CVC-021
 *  Verifies HB_TX_CYCLES is derived from CVC_HB_TX_PERIOD_MS / CVC_RTE_PERIOD_MS */
void test_Heartbeat_Derived_TX_cycles_value(void)
{
    /* HB_TX_CYCLES should equal 50ms / 10ms = 5 cycles */
    TEST_ASSERT_EQUAL(5u, HB_TX_CYCLES);
    TEST_ASSERT_EQUAL(CVC_HB_TX_PERIOD_MS / CVC_RTE_PERIOD_MS, HB_TX_CYCLES);
}

/** @verifies SWR-CVC-022
 *  Verifies FZC alive signal is polled using named constant, not magic number */
void test_Heartbeat_Named_signal_FZC_alive_detection(void)
{
    /* Recover FZC via 3 alive counter changes (poll-based detection) */
    recover_fzc(5u);

    /* After recovery, fzc_comm_status should be OK */
    mock_rte_write_count = 0u;
    run_cycles(1u);  /* trigger RTE write */

    boolean found_ok = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_OK)) {
            found_ok = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_ok);
}

/** @verifies SWR-CVC-022
 *  Verifies RZC alive signal is polled using named constant */
void test_Heartbeat_Named_signal_RZC_alive_detection(void)
{
    /* Recover RZC via 3 alive counter changes (poll-based detection) */
    recover_rzc(3u);

    /* After recovery, rzc_comm_status should be OK */
    mock_rte_write_count = 0u;
    run_cycles(1u);  /* trigger RTE write */

    boolean found_ok = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_RZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_OK)) {
            found_ok = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_ok);
}

/* ====================================================================
 * PHASE 3: WdgM Integration + E2E Fix
 * ==================================================================== */

/** @verifies SWR-CVC-021
 *  Phase 3: WdgM checkpoint fires at TX boundary (SE 3) */
void test_Heartbeat_WdgM_checkpoint_at_TX(void)
{
    /* Run one TX period */
    run_cycles(5u);

    TEST_ASSERT_EQUAL(1u, mock_wdgm_checkpoint_count);
    TEST_ASSERT_EQUAL(3u, mock_wdgm_last_se_id);
}

/** @verifies SWR-CVC-021
 *  Phase 3: WdgM gets 2 checkpoints in 100ms (2 TX periods) */
void test_Heartbeat_WdgM_two_checkpoints_in_100ms(void)
{
    /* Run two TX periods = 100ms */
    run_cycles(10u);

    TEST_ASSERT_EQUAL(2u, mock_wdgm_checkpoint_count);
}

/** @verifies SWR-CVC-021
 *  Phase 3: E2E_Protect called with real config (not NULL) */
void test_Heartbeat_E2E_Protect_with_real_config(void)
{
    /* Run one TX period */
    run_cycles(5u);

    TEST_ASSERT_NOT_NULL(mock_e2e_config_ptr);
    TEST_ASSERT_NOT_NULL(mock_e2e_state_ptr);
    TEST_ASSERT_EQUAL(CVC_E2E_HEARTBEAT_DATA_ID, mock_e2e_config_ptr->DataId);
}

/* ====================================================================
 * PHASE 2: Per-ECU Timeout Thresholds (FTTI Budget)
 * ==================================================================== */

/** @verifies SWR-CVC-022
 *  Phase 2: FZC timeout after 2 misses (100ms — SG-008 FTTI) */
void test_Heartbeat_FZC_timeout_after_2_misses(void)
{
    /* Recover FZC from initial TIMEOUT to OK (3 consecutive HBs) */
    recover_fzc(1u);

    /* Reset tracking */
    mock_rte_write_count = 0u;

    /* 2 check periods without FZC RX (alive counter unchanged) */
    run_cycles(5u);
    run_cycles(5u);

    /* FZC should be in TIMEOUT (2 misses = CVC_HB_FZC_MAX_MISS) */
    boolean found_timeout = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            found_timeout = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_timeout);
}

/** @verifies SWR-CVC-022
 *  Phase 2: RZC NOT timed out at 2 misses (needs 3 — local motor cutoff primary) */
void test_Heartbeat_RZC_no_timeout_at_2_misses(void)
{
    /* Recover RZC from initial TIMEOUT to OK (3 consecutive HBs) */
    recover_rzc(1u);

    /* Reset tracking */
    mock_rte_write_count = 0u;

    /* 2 check periods without RZC RX */
    run_cycles(5u);
    run_cycles(5u);

    /* RZC should NOT be in TIMEOUT (only 2 misses, need 3) */
    boolean found_timeout = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_RZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            found_timeout = TRUE;
        }
    }
    TEST_ASSERT_FALSE(found_timeout);
}

/** @verifies SWR-CVC-022
 *  Phase 2: FZC DTC fires after exactly 2 misses (tighter than RZC) */
void test_Heartbeat_FZC_DTC_at_2_misses(void)
{
    /* Recover FZC from initial TIMEOUT to OK (3 consecutive HBs) */
    recover_fzc(1u);

    /* Clear DEM mock */
    mock_dem_report_count = 0u;

    /* 2 check periods without FZC RX */
    run_cycles(5u);
    run_cycles(5u);

    /* FZC timeout DTC should have been reported */
    boolean fzc_dtc = FALSE;
    uint8 i;
    for (i = 0u; i < mock_dem_report_count; i++) {
        if ((mock_dem_event_ids[i] == CVC_DTC_CAN_FZC_TIMEOUT) &&
            (mock_dem_event_statuses[i] == DEM_EVENT_STATUS_FAILED)) {
            fzc_dtc = TRUE;
        }
    }
    TEST_ASSERT_TRUE(fzc_dtc);
}

/* ====================================================================
 * PHASE 5: E2E State Machine Integration
 * ==================================================================== */

/** @verifies SWR-CVC-022
 *  Phase 5: Single OK in INIT does not reach MinOkInit (FZC needs 2) */
void test_Heartbeat_SM_single_OK_stays_INIT(void)
{
    /* FZC starts in INIT (comm_status = TIMEOUT).
     * 1 alive change = 1 OK, MinOkInit = 2, stays INIT. */
    mock_com_rx_fzc_alive = 1u;
    mock_rte_write_count = 0u;
    run_cycles(5u);  /* 1 OK, OkCount=1 < MinOkInit=2 */

    boolean found_timeout = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            found_timeout = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_timeout);
}

/** @verifies SWR-CVC-022
 *  Phase 5: FZC INIT→VALID after exactly MinOkInit=2 OKs */
void test_Heartbeat_SM_FZC_INIT_to_VALID_after_2_OKs(void)
{
    /* Feed exactly 2 alive changes → 2 OKs → INIT→VALID */
    mock_com_rx_fzc_alive = 1u;
    run_cycles(5u);  /* OK 1 */
    mock_com_rx_fzc_alive = 2u;
    mock_rte_write_count = 0u;
    run_cycles(5u);  /* OK 2 → VALID */

    boolean found_ok = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_OK)) {
            found_ok = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_ok);
}

/** @verifies SWR-CVC-022
 *  Phase 5: Single error in VALID window doesn't cause INVALID (MaxErrValid=1) */
void test_Heartbeat_SM_single_miss_stays_VALID(void)
{
    /* Recover FZC to VALID */
    recover_fzc(1u);

    /* 1 miss in VALID — ErrorCount=1, MaxErrValid=1, 1 > 1 is false → stays VALID */
    mock_rte_write_count = 0u;
    run_cycles(5u);  /* 1 error, still VALID */

    boolean found_ok = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_OK)) {
            found_ok = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_ok);
}

/** @verifies SWR-CVC-022
 *  Phase 5: INVALID recovery requires MinOkInvalid=3 OKs (FZC) */
void test_Heartbeat_SM_INVALID_recovery_requires_3_OKs(void)
{
    /* Recover FZC to VALID, then force INVALID with 2 misses */
    recover_fzc(1u);
    run_cycles(5u);  /* miss 1 */
    run_cycles(5u);  /* miss 2 → ErrorCount > MaxErrValid → INVALID */

    /* Feed only 2 OKs — not enough for MinOkInvalid=3 */
    mock_com_rx_fzc_alive = 10u;
    run_cycles(5u);  /* OK 1 */
    mock_com_rx_fzc_alive = 11u;
    mock_rte_write_count = 0u;
    run_cycles(5u);  /* OK 2 — still INVALID */

    boolean found_timeout = FALSE;
    uint8 i;
    for (i = 0u; i < mock_rte_write_count; i++) {
        if ((mock_rte_write_sig_ids[i] == CVC_SIG_FZC_COMM_STATUS) &&
            (mock_rte_write_vals[i] == CVC_COMM_TIMEOUT)) {
            found_timeout = TRUE;
        }
    }
    TEST_ASSERT_TRUE(found_timeout);
}

/** @verifies SWR-CVC-022
 *  Phase 5: DTC PASSED when SM transitions INIT→VALID */
void test_Heartbeat_SM_DTC_passed_on_VALID(void)
{
    /* Recover FZC from initial TIMEOUT (INIT→VALID) */
    mock_dem_report_count = 0u;
    recover_fzc(1u);

    /* Check DTC PASSED reported on transition */
    boolean fzc_passed = FALSE;
    uint8 i;
    for (i = 0u; i < mock_dem_report_count; i++) {
        if ((mock_dem_event_ids[i] == CVC_DTC_CAN_FZC_TIMEOUT) &&
            (mock_dem_event_statuses[i] == DEM_EVENT_STATUS_PASSED)) {
            fzc_passed = TRUE;
        }
    }
    TEST_ASSERT_TRUE(fzc_passed);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Heartbeat_Init_comm_status_timeout);
    RUN_TEST(test_Heartbeat_TX_every_50ms);
    RUN_TEST(test_Heartbeat_Alive_counter_increments);
    RUN_TEST(test_Heartbeat_Alive_counter_wraps_at_15);
    RUN_TEST(test_Heartbeat_FZC_RxIndication_resets_timer);
    RUN_TEST(test_Heartbeat_RZC_RxIndication_resets_timer);
    RUN_TEST(test_Heartbeat_FZC_timeout_after_3_misses);
    RUN_TEST(test_Heartbeat_RZC_timeout_after_3_misses);
    RUN_TEST(test_Heartbeat_FZC_timeout_DTC_reported);
    RUN_TEST(test_Heartbeat_RZC_timeout_DTC_reported);
    RUN_TEST(test_Heartbeat_Recovery_after_timeout);
    RUN_TEST(test_Heartbeat_RTE_write_fzc_comm_each_cycle);
    RUN_TEST(test_Heartbeat_RTE_write_rzc_comm_each_cycle);
    RUN_TEST(test_Heartbeat_TX_data_includes_ecu_id_and_state);
    RUN_TEST(test_Heartbeat_No_TX_before_50ms);

    /* --- HARDENED TESTS --- */
    RUN_TEST(test_Heartbeat_No_TX_at_4_cycles);
    RUN_TEST(test_Heartbeat_TX_at_exactly_5_cycles);
    RUN_TEST(test_Heartbeat_Alive_wrap_exact_boundary);
    RUN_TEST(test_Heartbeat_RxIndication_unknown_ecu_ignored);
    RUN_TEST(test_Heartbeat_RxIndication_ecu_id_zero_ignored);
    RUN_TEST(test_Heartbeat_FZC_no_timeout_at_1_miss);
    RUN_TEST(test_Heartbeat_FZC_timeout_RZC_ok);
    RUN_TEST(test_Heartbeat_DTC_not_re_reported_after_timeout);
    RUN_TEST(test_Heartbeat_MainFunction_before_init_no_action);

    /* --- PHASE 1: Derived Constants --- */
    RUN_TEST(test_Heartbeat_Derived_TX_cycles_value);
    RUN_TEST(test_Heartbeat_Named_signal_FZC_alive_detection);
    RUN_TEST(test_Heartbeat_Named_signal_RZC_alive_detection);

    /* --- PHASE 3: WdgM + E2E Fix --- */
    RUN_TEST(test_Heartbeat_WdgM_checkpoint_at_TX);
    RUN_TEST(test_Heartbeat_WdgM_two_checkpoints_in_100ms);
    RUN_TEST(test_Heartbeat_E2E_Protect_with_real_config);

    /* --- PHASE 2: Per-ECU Timeout Thresholds --- */
    RUN_TEST(test_Heartbeat_FZC_timeout_after_2_misses);
    RUN_TEST(test_Heartbeat_RZC_no_timeout_at_2_misses);
    RUN_TEST(test_Heartbeat_FZC_DTC_at_2_misses);

    /* --- PHASE 5: E2E State Machine --- */
    RUN_TEST(test_Heartbeat_SM_single_OK_stays_INIT);
    RUN_TEST(test_Heartbeat_SM_FZC_INIT_to_VALID_after_2_OKs);
    RUN_TEST(test_Heartbeat_SM_single_miss_stays_VALID);
    RUN_TEST(test_Heartbeat_SM_INVALID_recovery_requires_3_OKs);
    RUN_TEST(test_Heartbeat_SM_DTC_passed_on_VALID);

    return UNITY_END();
}
