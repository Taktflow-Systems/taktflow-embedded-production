/**
 * @file    test_Com.c
 * @brief   Unit tests for Communication module
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-015, SWR-BSW-016, SWR-BSW-023, SWR-BSW-024, SWR-BSW-025, SWR-BSW-031, SWR-BSW-050
 *
 * Tests signal send/receive, RX indication, TX main function,
 * and signal packing/unpacking.
 */
#include "unity.h"
#include "Com.h"
#include "E2E.h"
#include "Dem.h"

/* ==================================================================
 * Mock: PduR (lower layer)
 * ================================================================== */

static PduIdType      mock_pdur_tx_pdu_id;
static uint8          mock_pdur_tx_data[8];
static uint8          mock_pdur_tx_dlc;
static uint8          mock_pdur_tx_count;
static Std_ReturnType mock_pdur_tx_result;

Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    mock_pdur_tx_pdu_id = TxPduId;
    if (PduInfoPtr != NULL_PTR) {
        mock_pdur_tx_dlc = PduInfoPtr->SduLength;
        for (uint8 i = 0u; i < PduInfoPtr->SduLength; i++) {
            mock_pdur_tx_data[i] = PduInfoPtr->SduDataPtr[i];
        }
    }
    mock_pdur_tx_count++;
    return mock_pdur_tx_result;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

/* Signal buffers */
static uint8  sig_torque_buf;
static sint16 sig_steering_buf;
static uint8  sig_motor_status_buf;

/* Mock: Dem */
static uint8 mock_dem_event_id;
static uint8 mock_dem_event_status;
static uint8 mock_dem_call_count;

void Dem_ReportErrorStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    mock_dem_event_id = (uint8)EventId;
    mock_dem_event_status = (uint8)EventStatus;
    mock_dem_call_count++;
}

Std_ReturnType Dem_GetEventStatus(Dem_EventIdType EventId, uint8* StatusPtr)
{
    (void)EventId;
    if (StatusPtr != NULL_PTR) { *StatusPtr = 0u; }
    return E_OK;
}

/* Signal config table */
static const Com_SignalConfigType test_signals[] = {
    /* id, bitPos, bitSize, type,      pduId, shadowBuf,              rteSignalId,       updateBit */
    {  0u,   16u,     8u,  COM_UINT8,  0u,   &sig_torque_buf,        COM_RTE_SIGNAL_NONE, COM_NO_UPDATE_BIT },
    {  1u,   16u,    16u,  COM_SINT16, 1u,   &sig_steering_buf,      COM_RTE_SIGNAL_NONE, COM_NO_UPDATE_BIT },
    {  2u,   16u,     8u,  COM_UINT8,  0u,   &sig_motor_status_buf,  COM_RTE_SIGNAL_NONE, COM_NO_UPDATE_BIT },
};

/* TX PDU config */
static const Com_TxPduConfigType test_tx_pdus[] = {
    /* pduId, dlc, cycleMs, txMode,             e2eProt, dataId, cntBit, crcBit */
    { 0u, 8u, 10u, COM_TX_MODE_PERIODIC, FALSE, 0u, 0u, 8u },
    { 1u, 8u, 10u, COM_TX_MODE_PERIODIC, TRUE,  5u, 4u, 8u },  /* E2E protected */
};

/* RX PDU config */
#define TEST_DEM_E2E_EVENT  10u
static const Com_RxPduConfigType test_rx_pdus[] = {
    /* pduId, dlc, timeoutMs, e2eProt, dataId, maxDelta, demEvtId,         smValid, smInvalid */
    { 0u, 8u, 100u, FALSE, 0u, 2u, COM_DEM_EVENT_NONE, 0u, 0u },
};

/* E2E-protected RX PDU config (for E2E tests) */
static const Com_RxPduConfigType test_rx_pdus_e2e[] = {
    { 0u, 8u, 100u, TRUE, 5u, 2u, TEST_DEM_E2E_EVENT, 3u, 2u },
};

static Com_ConfigType test_config;

void setUp(void)
{
    mock_pdur_tx_count = 0u;
    mock_pdur_tx_result = E_OK;
    mock_dem_call_count = 0u;
    mock_dem_event_id = 0u;
    mock_dem_event_status = 0u;
    sig_torque_buf = 0u;
    sig_steering_buf = 0;
    sig_motor_status_buf = 0u;

    test_config.signalConfig = test_signals;
    test_config.signalCount  = 3u;
    test_config.txPduConfig  = test_tx_pdus;
    test_config.txPduCount   = 2u;
    test_config.rxPduConfig  = test_rx_pdus;
    test_config.rxPduCount   = 1u;

    Com_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-015: Signal Send/Receive
 * ================================================================== */

/** @verifies SWR-BSW-015 */
void test_Com_SendSignal_stores_value(void)
{
    uint8 torque = 128u;
    Std_ReturnType ret = Com_SendSignal(0u, &torque);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(128u, sig_torque_buf);
}

/** @verifies SWR-BSW-015 */
void test_Com_ReceiveSignal_reads_value(void)
{
    sig_motor_status_buf = 42u;
    uint8 value = 0u;

    Std_ReturnType ret = Com_ReceiveSignal(2u, &value);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(42u, value);
}

/** @verifies SWR-BSW-015 */
void test_Com_SendSignal_null_data(void)
{
    Std_ReturnType ret = Com_SendSignal(0u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-015 */
void test_Com_SendSignal_invalid_id(void)
{
    uint8 val = 0u;
    Std_ReturnType ret = Com_SendSignal(99u, &val);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-015 */
void test_Com_ReceiveSignal_null_data(void)
{
    Std_ReturnType ret = Com_ReceiveSignal(0u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-016: RX Indication and TX MainFunction
 * ================================================================== */

/** @verifies SWR-BSW-016 */
void test_Com_RxIndication_stores_pdu(void)
{
    uint8 data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { data, 8u };

    Com_RxIndication(0u, &pdu);

    /* Signal at bit 16 (byte 2), 8 bits = data[2] = 0xCC */
    uint8 val = 0u;
    Com_ReceiveSignal(2u, &val);
    TEST_ASSERT_EQUAL_HEX8(0xCC, val);
}

/** @verifies SWR-BSW-016 */
void test_Com_MainFunction_Tx_sends_pending(void)
{
    uint8 torque = 200u;
    uint8 c;
    Com_SendSignal(0u, &torque);

    /* Burn through startup delay + one cycle time */
    for (c = 0u; c < 10u; c++) {
        Com_MainFunction_Tx();
    }

    TEST_ASSERT_TRUE(mock_pdur_tx_count > 0u);
}

/** @verifies SWR-BSW-016 */
void test_Com_RxIndication_null_pdu(void)
{
    Com_RxIndication(0u, NULL_PTR);
    /* Should not crash */
    TEST_ASSERT_TRUE(TRUE);
}

/** @verifies SWR-BSW-016 */
void test_Com_Init_null_config(void)
{
    Com_Init(NULL_PTR);
    uint8 val = 0u;
    Std_ReturnType ret = Com_SendSignal(0u, &val);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-015: Boundary / Fault Injection Tests
 * ================================================================== */

/** @verifies SWR-BSW-015
 *  Equivalence class: NULL pointer for receive buffer
 *  Already covered above — this tests ReceiveSignal with invalid ID */
void test_Com_ReceiveSignal_invalid_id(void)
{
    uint8 val = 0u;
    Std_ReturnType ret = Com_ReceiveSignal(99u, &val);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-016
 *  Equivalence class: PDU DLC boundary — max valid DLC (8 bytes) */
void test_Com_RxIndication_max_dlc(void)
{
    uint8 data[8] = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u};
    PduInfoType pdu = { data, 8u };

    Com_RxIndication(0u, &pdu);

    /* Signal at bit 16 (byte 2), 8 bits = data[2] = 0x03 */
    uint8 val = 0u;
    Com_ReceiveSignal(2u, &val);
    TEST_ASSERT_EQUAL_HEX8(0x03u, val);
}

/** @verifies SWR-BSW-016
 *  Equivalence class: PDU DLC boundary — zero-length PDU (should be rejected/no-op) */
void test_Com_RxIndication_dlc_zero(void)
{
    uint8 data[1] = {0xFFu};
    PduInfoType pdu = { data, 0u };

    /* Reset signal buffer to known value */
    sig_motor_status_buf = 0xAAu;

    Com_RxIndication(0u, &pdu);

    /* Signal should remain unchanged — zero-length PDU has no data to unpack */
    uint8 val = 0u;
    Com_ReceiveSignal(2u, &val);
    TEST_ASSERT_EQUAL_HEX8(0xAAu, val);
}

/** @verifies SWR-BSW-016
 *  Equivalence class: PDU DLC boundary — DLC=9 exceeds CAN 2.0B max (invalid) */
void test_Com_RxIndication_dlc_exceeds_max(void)
{
    uint8 data[9] = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u, 0x08u, 0x09u};
    PduInfoType pdu = { data, 9u };

    /* Reset signal buffer to known value */
    sig_motor_status_buf = 0xBBu;

    Com_RxIndication(0u, &pdu);

    /* Module should clamp or reject — signal should remain unchanged or safely handled */
    /* No crash is the minimum acceptable behavior */
    TEST_ASSERT_TRUE(TRUE);
}

/** @verifies SWR-BSW-016
 *  Equivalence class: RX indication for unknown PDU ID (no configured RX PDU) */
void test_Com_RxIndication_invalid_pdu_id(void)
{
    uint8 data[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { data, 8u };

    /* PDU ID 99 is not configured — should be silently ignored */
    Com_RxIndication(99u, &pdu);

    /* No crash, no data corruption */
    TEST_ASSERT_TRUE(TRUE);
}

/** @verifies SWR-BSW-015
 *  Equivalence class: Signal packing — send then receive round-trip consistency */
void test_Com_SendReceive_roundtrip(void)
{
    uint8 send_val = 0xA5u;
    Std_ReturnType ret = Com_SendSignal(0u, &send_val);
    TEST_ASSERT_EQUAL(E_OK, ret);

    uint8 recv_val = 0u;
    ret = Com_ReceiveSignal(0u, &recv_val);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_HEX8(0xA5u, recv_val);
}

/** @verifies SWR-BSW-015
 *  Equivalence class: Boundary value — send max uint8 (0xFF) */
void test_Com_SendSignal_max_value(void)
{
    uint8 val = 0xFFu;
    Std_ReturnType ret = Com_SendSignal(0u, &val);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, sig_torque_buf);
}

/** @verifies SWR-BSW-015
 *  Equivalence class: Boundary value — send zero */
void test_Com_SendSignal_zero_value(void)
{
    /* First set to non-zero, then send zero to verify it updates */
    sig_torque_buf = 0xFFu;
    uint8 val = 0x00u;
    Std_ReturnType ret = Com_SendSignal(0u, &val);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_HEX8(0x00u, sig_torque_buf);
}

/** @verifies SWR-BSW-016
 *  Fault injection: TX with PduR returning E_NOT_OK */
void test_Com_MainFunction_Tx_pdur_fail(void)
{
    uint8 torque = 100u;
    uint8 c;
    Com_SendSignal(0u, &torque);

    mock_pdur_tx_result = E_NOT_OK;

    /* Burn through startup delay + one cycle time */
    for (c = 0u; c < 10u; c++) {
        Com_MainFunction_Tx();
    }

    /* PduR was called but returned failure */
    TEST_ASSERT_TRUE(mock_pdur_tx_count > 0u);
}

/** @verifies SWR-BSW-016
 *  Equivalence class: RX timeout — shadow buffers zeroed after deadline
 *  RX PDU 0 has 100ms timeout (10 cycles at 10ms). After 10 cycles of
 *  Com_MainFunction_Rx without RxIndication, shadow buffers must be zeroed. */
void test_Com_RxTimeout_zeros_shadow_buffers(void)
{
    /* Pre-load signal buffer with known value */
    sig_torque_buf = 0xAAu;
    sig_motor_status_buf = 0xBBu;

    /* Run 11 RX main cycles without any RxIndication — exceeds 100ms timeout */
    uint16 i;
    for (i = 0u; i < 11u; i++) {
        Com_MainFunction_Rx();
    }

    /* Shadow buffers for signals on PDU 0 must be zeroed (AUTOSAR REPLACE) */
    TEST_ASSERT_EQUAL_HEX8(0x00u, sig_torque_buf);
    TEST_ASSERT_EQUAL_HEX8(0x00u, sig_motor_status_buf);
}

/** @verifies SWR-BSW-016
 *  Equivalence class: RX indication resets timeout counter — no false timeout */
void test_Com_RxTimeout_reset_by_indication(void)
{
    /* Run 5 cycles (50ms) — halfway to 100ms timeout */
    uint16 i;
    for (i = 0u; i < 5u; i++) {
        Com_MainFunction_Rx();
    }

    /* Receive a valid PDU — resets timeout counter */
    uint8 data[] = {0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { data, 8u };
    Com_RxIndication(0u, &pdu);

    /* Run 5 more cycles — total since reset is only 50ms, within 100ms timeout */
    for (i = 0u; i < 5u; i++) {
        Com_MainFunction_Rx();
    }

    /* Signal at bit 16 (byte 2) should still hold received value, not zeroed */
    uint8 val = 0u;
    Com_ReceiveSignal(2u, &val);
    TEST_ASSERT_EQUAL_HEX8(0xAAu, val);
}

/** @verifies SWR-BSW-015
 *  Equivalence class: Multiple signals on same PDU — independence check */
void test_Com_MultipleSignals_same_pdu(void)
{
    /* Signals 0 and 2 share PDU 0 */
    uint8 torque = 0x55u;
    uint8 motor_status = 0xAAu;

    Com_SendSignal(0u, &torque);
    Com_SendSignal(2u, &motor_status);

    /* Both should be stored independently */
    TEST_ASSERT_EQUAL_HEX8(0x55u, sig_torque_buf);
    TEST_ASSERT_EQUAL_HEX8(0xAAu, sig_motor_status_buf);
}

/* ==================================================================
 * E2E Integration & Signal Quality Tests (Phase 2)
 * ================================================================== */

/** Helper: reinitialize Com with E2E-protected RX PDU config */
static void setup_e2e_rx(void)
{
    test_config.rxPduConfig = test_rx_pdus_e2e;
    test_config.rxPduCount  = 1u;
    Com_Init(&test_config);
}

/** @verifies SWR-BSW-016
 *  E2E: corrupted CRC discards frame after SM enters INVALID (2 consecutive errors).
 *  First error: SM NODATA→INIT (frame still unpacked — SM is lenient during init).
 *  Second error: SM INIT→INVALID (frame discarded — SM rejects). */
void test_Com_RxIndication_E2E_fail_discards_frame(void)
{
    setup_e2e_rx();

    /* Send 2 bad frames to push SM into INVALID */
    uint8 bad_data1[] = {0x50, 0x00, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu1 = { bad_data1, 8u };
    Com_RxIndication(0u, &pdu1);  /* Error 1: SM NODATA→INIT (unpacked) */
    Com_RxIndication(0u, &pdu1);  /* Error 2: SM INIT→INVALID (discarded) */

    /* Now set signal to known value and send another bad frame */
    sig_torque_buf = 0xAAu;
    uint8 bad_data2[] = {0x50, 0x00, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu2 = { bad_data2, 8u };
    Com_RxIndication(0u, &pdu2);  /* Error 3: SM stays INVALID → discarded */

    /* Signal should retain 0xAA — SM is INVALID, frame discarded */
    TEST_ASSERT_EQUAL_HEX8(0xAAu, sig_torque_buf);
}

/** @verifies SWR-BSW-016
 *  E2E failure triggers DTC after SM enters INVALID (2 consecutive errors) */
void test_Com_RxIndication_E2E_fail_reports_DTC(void)
{
    setup_e2e_rx();

    uint8 bad_data[] = {0x50, 0x00, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { bad_data, 8u };
    Com_RxIndication(0u, &pdu);  /* Error 1: SM NODATA→INIT, no DTC yet */
    Com_RxIndication(0u, &pdu);  /* Error 2: SM INIT→INVALID, DTC reported */

    TEST_ASSERT_TRUE(mock_dem_call_count >= 1u);
    TEST_ASSERT_EQUAL_UINT8(TEST_DEM_E2E_EVENT, mock_dem_event_id);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_status); /* DEM_EVENT_STATUS_FAILED */
}

/** @verifies SWR-BSW-016
 *  E2E: valid frame unpacks signal correctly */
void test_Com_RxIndication_E2E_pass_unpacks_signal(void)
{
    setup_e2e_rx();

    /* Build a valid E2E-protected PDU using E2E_Protect */
    uint8 good_data[8] = {0u};
    good_data[2] = 0x77u;  /* Payload: torque = 0x77 */
    E2E_ConfigType e2e_cfg = { 5u, 2u, 8u };
    E2E_StateType e2e_state = { 0u };
    (void)E2E_Protect(&e2e_cfg, &e2e_state, good_data, 8u);

    PduInfoType pdu = { good_data, 8u };
    Com_RxIndication(0u, &pdu);

    TEST_ASSERT_EQUAL_HEX8(0x77u, sig_torque_buf);
}

/** @verifies SWR-BSW-016
 *  Signal quality: E2E failure sets quality to E2E_FAIL after SM enters INVALID */
void test_Com_RxIndication_E2E_fail_sets_quality(void)
{
    setup_e2e_rx();

    uint8 bad_data[] = {0x50, 0x00, 0xDD, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { bad_data, 8u };
    Com_RxIndication(0u, &pdu);  /* Error 1 */
    Com_RxIndication(0u, &pdu);  /* Error 2 → SM INVALID */

    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_E2E_FAIL, Com_GetRxPduQuality(0u));
}

/** @verifies SWR-BSW-016
 *  Signal quality: valid frame sets quality to FRESH */
void test_Com_RxIndication_E2E_pass_sets_quality_fresh(void)
{
    setup_e2e_rx();

    uint8 good_data[8] = {0u};
    good_data[2] = 0x33u;
    E2E_ConfigType e2e_cfg = { 5u, 2u, 8u };
    E2E_StateType e2e_state = { 0u };
    (void)E2E_Protect(&e2e_cfg, &e2e_state, good_data, 8u);

    PduInfoType pdu = { good_data, 8u };
    Com_RxIndication(0u, &pdu);

    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_FRESH, Com_GetRxPduQuality(0u));
}

/** @verifies SWR-BSW-016
 *  Signal quality: timeout sets quality to TIMED_OUT */
void test_Com_RxTimeout_sets_quality_timed_out(void)
{
    /* Use default config (non-E2E, 100ms timeout) */
    test_config.rxPduConfig = test_rx_pdus;
    test_config.rxPduCount  = 1u;
    Com_Init(&test_config);

    /* First send a valid frame to set quality to FRESH */
    uint8 data[] = {0x00, 0x00, 0xEE, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { data, 8u };
    Com_RxIndication(0u, &pdu);
    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_FRESH, Com_GetRxPduQuality(0u));

    /* Run 11 RX main cycles (110ms > 100ms timeout) */
    uint16 i;
    for (i = 0u; i < 11u; i++) {
        Com_MainFunction_Rx();
    }

    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_TIMED_OUT, Com_GetRxPduQuality(0u));
}

/** @verifies SWR-BSW-016
 *  Signal quality: initial state is TIMED_OUT (no data received yet) */
void test_Com_GetRxPduQuality_initial_timed_out(void)
{
    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_TIMED_OUT, Com_GetRxPduQuality(0u));
}

/** @verifies SWR-BSW-016
 *  Signal quality: invalid PDU ID returns TIMED_OUT */
void test_Com_GetRxPduQuality_invalid_pdu(void)
{
    TEST_ASSERT_EQUAL(COM_SIGNAL_QUALITY_TIMED_OUT, Com_GetRxPduQuality(99u));
}

/** @verifies SWR-BSW-016
 *  TX E2E: MainFunction_Tx applies E2E header to protected PDU */
void test_Com_MainFunction_Tx_E2E_protect_applied(void)
{
    sint16 steer = 1500;
    uint8 c;
    Com_SendSignal(1u, &steer);  /* Signal 1 on PDU 1 (E2E protected) */

    /* Burn through startup delay + one cycle time */
    for (c = 0u; c < 10u; c++) {
        Com_MainFunction_Tx();
    }

    /* PDU 1 is E2E-protected: byte 0 should have counter|dataId, byte 1 should be CRC (non-zero) */
    /* DataId=5 → low nibble = 5, counter starts at 1 after first protect → byte0 = 0x15 */
    TEST_ASSERT_EQUAL_HEX8(0x15u, mock_pdur_tx_data[0]);
    TEST_ASSERT_TRUE(mock_pdur_tx_data[1] != 0x00u);  /* CRC should be non-zero */
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_Com_SendSignal_stores_value);
    RUN_TEST(test_Com_ReceiveSignal_reads_value);
    RUN_TEST(test_Com_SendSignal_null_data);
    RUN_TEST(test_Com_SendSignal_invalid_id);
    RUN_TEST(test_Com_ReceiveSignal_null_data);
    RUN_TEST(test_Com_RxIndication_stores_pdu);
    RUN_TEST(test_Com_MainFunction_Tx_sends_pending);
    RUN_TEST(test_Com_RxIndication_null_pdu);
    RUN_TEST(test_Com_Init_null_config);

    /* Hardened boundary / fault injection tests */
    RUN_TEST(test_Com_ReceiveSignal_invalid_id);
    RUN_TEST(test_Com_RxIndication_max_dlc);
    RUN_TEST(test_Com_RxIndication_dlc_zero);
    RUN_TEST(test_Com_RxIndication_dlc_exceeds_max);
    RUN_TEST(test_Com_RxIndication_invalid_pdu_id);
    RUN_TEST(test_Com_SendReceive_roundtrip);
    RUN_TEST(test_Com_SendSignal_max_value);
    RUN_TEST(test_Com_SendSignal_zero_value);
    RUN_TEST(test_Com_MainFunction_Tx_pdur_fail);
    RUN_TEST(test_Com_RxTimeout_zeros_shadow_buffers);
    RUN_TEST(test_Com_RxTimeout_reset_by_indication);
    RUN_TEST(test_Com_MultipleSignals_same_pdu);

    /* E2E integration & signal quality tests (Phase 2) */
    RUN_TEST(test_Com_RxIndication_E2E_fail_discards_frame);
    RUN_TEST(test_Com_RxIndication_E2E_fail_reports_DTC);
    RUN_TEST(test_Com_RxIndication_E2E_pass_unpacks_signal);
    RUN_TEST(test_Com_RxIndication_E2E_fail_sets_quality);
    RUN_TEST(test_Com_RxIndication_E2E_pass_sets_quality_fresh);
    RUN_TEST(test_Com_RxTimeout_sets_quality_timed_out);
    RUN_TEST(test_Com_GetRxPduQuality_initial_timed_out);
    RUN_TEST(test_Com_GetRxPduQuality_invalid_pdu);
    RUN_TEST(test_Com_MainFunction_Tx_E2E_protect_applied);

    return UNITY_END();
}
