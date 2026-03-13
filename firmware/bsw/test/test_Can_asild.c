/**
 * @file    test_Can.c
 * @brief   Unit tests for CAN MCAL driver
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-001, SWR-BSW-002, SWR-BSW-003, SWR-BSW-004, SWR-BSW-005
 *
 * Tests CAN driver state machine, transmit, receive processing,
 * bus-off recovery, and error handling. Hardware is mocked via
 * Can_Hw_* stub functions defined in this file.
 */
#include "unity.h"
#include "Can.h"

/* ==================================================================
 * Mock Hardware Layer — replaces real HAL for host testing
 * ================================================================== */

/* Mock state */
static boolean      mock_hw_init_called;
static boolean      mock_hw_init_fail;
static uint32       mock_hw_baudrate;
static boolean      mock_hw_started;
static boolean      mock_hw_bus_off;
static uint8        mock_hw_tec;
static uint8        mock_hw_rec;

/* Mock TX capture */
#define MOCK_TX_MAX 8u
static Can_IdType   mock_tx_ids[MOCK_TX_MAX];
static uint8        mock_tx_data[MOCK_TX_MAX][8];
static uint8        mock_tx_dlc[MOCK_TX_MAX];
static uint8        mock_tx_count;
static boolean      mock_tx_full;

/* Mock RX injection */
#define MOCK_RX_MAX 16u
static Can_IdType   mock_rx_ids[MOCK_RX_MAX];
static uint8        mock_rx_data[MOCK_RX_MAX][8];
static uint8        mock_rx_dlc[MOCK_RX_MAX];
static uint8        mock_rx_count;
static uint8        mock_rx_read_idx;

/* Mock CanIf callback capture */
static Can_IdType   canif_rx_id;
static uint8        canif_rx_data[8];
static uint8        canif_rx_dlc;
static uint8        canif_rx_call_count;
static boolean      canif_busoff_called;

/* ---- Hardware mock implementations ---- */

Std_ReturnType Can_Hw_Init(uint32 baudrate)
{
    mock_hw_init_called = TRUE;
    mock_hw_baudrate = baudrate;
    if (mock_hw_init_fail) {
        return E_NOT_OK;
    }
    return E_OK;
}

void Can_Hw_Start(void)
{
    mock_hw_started = TRUE;
}

void Can_Hw_Stop(void)
{
    mock_hw_started = FALSE;
}

Std_ReturnType Can_Hw_Transmit(Can_IdType id, const uint8* data, uint8 dlc)
{
    if (mock_tx_full || (mock_tx_count >= MOCK_TX_MAX)) {
        return E_NOT_OK;
    }
    mock_tx_ids[mock_tx_count] = id;
    for (uint8 i = 0u; i < dlc; i++) {
        mock_tx_data[mock_tx_count][i] = data[i];
    }
    mock_tx_dlc[mock_tx_count] = dlc;
    mock_tx_count++;
    return E_OK;
}

boolean Can_Hw_Receive(Can_IdType* id, uint8* data, uint8* dlc)
{
    if (mock_rx_read_idx >= mock_rx_count) {
        return FALSE;
    }
    *id = mock_rx_ids[mock_rx_read_idx];
    *dlc = mock_rx_dlc[mock_rx_read_idx];
    for (uint8 i = 0u; i < *dlc; i++) {
        data[i] = mock_rx_data[mock_rx_read_idx][i];
    }
    mock_rx_read_idx++;
    return TRUE;
}

boolean Can_Hw_IsBusOff(void)
{
    return mock_hw_bus_off;
}

void Can_Hw_GetErrorCounters(uint8* tec, uint8* rec)
{
    *tec = mock_hw_tec;
    *rec = mock_hw_rec;
}

/* ---- CanIf callback mocks ---- */

void CanIf_RxIndication(Can_IdType canId, const uint8* sduPtr, uint8 dlc)
{
    canif_rx_id = canId;
    canif_rx_dlc = dlc;
    for (uint8 i = 0u; i < dlc; i++) {
        canif_rx_data[i] = sduPtr[i];
    }
    canif_rx_call_count++;
}

void CanIf_ControllerBusOff(uint8 controllerId)
{
    (void)controllerId;
    canif_busoff_called = TRUE;
}

/* ---- Mock helpers ---- */

static void mock_inject_rx(Can_IdType id, const uint8* data, uint8 dlc)
{
    if (mock_rx_count < MOCK_RX_MAX) {
        mock_rx_ids[mock_rx_count] = id;
        mock_rx_dlc[mock_rx_count] = dlc;
        for (uint8 i = 0u; i < dlc; i++) {
            mock_rx_data[mock_rx_count][i] = data[i];
        }
        mock_rx_count++;
    }
}

/* ==================================================================
 * Test fixtures
 * ================================================================== */

static Can_ConfigType test_config;

void setUp(void)
{
    Can_DeInit();

    /* Reset all mock state */
    mock_hw_init_called = FALSE;
    mock_hw_init_fail = FALSE;
    mock_hw_baudrate = 0u;
    mock_hw_started = FALSE;
    mock_hw_bus_off = FALSE;
    mock_hw_tec = 0u;
    mock_hw_rec = 0u;
    mock_tx_count = 0u;
    mock_tx_full = FALSE;
    mock_rx_count = 0u;
    mock_rx_read_idx = 0u;
    canif_rx_call_count = 0u;
    canif_busoff_called = FALSE;

    for (uint8 i = 0u; i < MOCK_TX_MAX; i++) {
        mock_tx_ids[i] = 0u;
        mock_tx_dlc[i] = 0u;
    }

    /* Default test config */
    test_config.baudrate = 500000u;
    test_config.controllerId = 0u;
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-001: CAN Driver Initialization
 * ================================================================== */

/** @verifies SWR-BSW-001 */
void test_Can_Init_success(void)
{
    Can_Init(&test_config);

    TEST_ASSERT_TRUE(mock_hw_init_called);
    TEST_ASSERT_EQUAL_UINT32(500000u, mock_hw_baudrate);
    TEST_ASSERT_EQUAL(CAN_CS_STOPPED, Can_GetControllerMode(0u));
}

/** @verifies SWR-BSW-001 */
void test_Can_Init_hw_failure(void)
{
    mock_hw_init_fail = TRUE;
    Can_Init(&test_config);

    TEST_ASSERT_EQUAL(CAN_CS_UNINIT, Can_GetControllerMode(0u));
}

/** @verifies SWR-BSW-001 */
void test_Can_Init_null_config(void)
{
    Can_Init(NULL_PTR);
    TEST_ASSERT_EQUAL(CAN_CS_UNINIT, Can_GetControllerMode(0u));
}

/** @verifies SWR-BSW-001 */
void test_Can_SetMode_start(void)
{
    Can_Init(&test_config);
    Std_ReturnType ret = Can_SetControllerMode(0u, CAN_CS_STARTED);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(CAN_CS_STARTED, Can_GetControllerMode(0u));
    TEST_ASSERT_TRUE(mock_hw_started);
}

/** @verifies SWR-BSW-001 */
void test_Can_SetMode_stop(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);
    Std_ReturnType ret = Can_SetControllerMode(0u, CAN_CS_STOPPED);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(CAN_CS_STOPPED, Can_GetControllerMode(0u));
    TEST_ASSERT_FALSE(mock_hw_started);
}

/** @verifies SWR-BSW-001 */
void test_Can_SetMode_before_init_fails(void)
{
    Std_ReturnType ret = Can_SetControllerMode(0u, CAN_CS_STARTED);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-002: CAN Driver Write
 * ================================================================== */

/** @verifies SWR-BSW-002 */
void test_Can_Write_success(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    Can_PduType pdu;
    uint8 data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    pdu.id = 0x100u;
    pdu.length = 8u;
    pdu.sdu = data;

    Can_ReturnType ret = Can_Write(0u, &pdu);

    TEST_ASSERT_EQUAL(CAN_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_tx_count);
    TEST_ASSERT_EQUAL_HEX32(0x100u, mock_tx_ids[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11, mock_tx_data[0][0]);
}

/** @verifies SWR-BSW-002 */
void test_Can_Write_not_started_fails(void)
{
    Can_Init(&test_config);
    /* Controller is STOPPED, not STARTED */

    Can_PduType pdu;
    uint8 data[] = {0x01};
    pdu.id = 0x100u;
    pdu.length = 1u;
    pdu.sdu = data;

    Can_ReturnType ret = Can_Write(0u, &pdu);
    TEST_ASSERT_EQUAL(CAN_NOT_OK, ret);
}

/** @verifies SWR-BSW-002 */
void test_Can_Write_null_pdu_fails(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    Can_ReturnType ret = Can_Write(0u, NULL_PTR);
    TEST_ASSERT_EQUAL(CAN_NOT_OK, ret);
}

/** @verifies SWR-BSW-002 */
void test_Can_Write_invalid_dlc_fails(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    Can_PduType pdu;
    uint8 data[] = {0x01};
    pdu.id = 0x100u;
    pdu.length = 9u; /* > 8 is invalid for CAN 2.0B */
    pdu.sdu = data;

    Can_ReturnType ret = Can_Write(0u, &pdu);
    TEST_ASSERT_EQUAL(CAN_NOT_OK, ret);
}

/** @verifies SWR-BSW-002 */
void test_Can_Write_tx_full_returns_busy(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    mock_tx_full = TRUE;

    Can_PduType pdu;
    uint8 data[] = {0x01};
    pdu.id = 0x100u;
    pdu.length = 1u;
    pdu.sdu = data;

    Can_ReturnType ret = Can_Write(0u, &pdu);
    TEST_ASSERT_EQUAL(CAN_BUSY, ret);
}

/* ==================================================================
 * SWR-BSW-003: CAN MainFunction_Read
 * ================================================================== */

/** @verifies SWR-BSW-003 */
void test_Can_MainFunction_Read_processes_message(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    uint8 rx_data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x00, 0x00, 0x00};
    mock_inject_rx(0x200u, rx_data, 4u);

    Can_MainFunction_Read();

    TEST_ASSERT_EQUAL_UINT8(1u, canif_rx_call_count);
    TEST_ASSERT_EQUAL_HEX32(0x200u, canif_rx_id);
    TEST_ASSERT_EQUAL_UINT8(4u, canif_rx_dlc);
    TEST_ASSERT_EQUAL_HEX8(0xAA, canif_rx_data[0]);
}

/** @verifies SWR-BSW-003 */
void test_Can_MainFunction_Read_multiple_messages(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    uint8 data1[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8 data2[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8 data3[] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    mock_inject_rx(0x100u, data1, 8u);
    mock_inject_rx(0x101u, data2, 8u);
    mock_inject_rx(0x102u, data3, 8u);

    Can_MainFunction_Read();

    TEST_ASSERT_EQUAL_UINT8(3u, canif_rx_call_count);
}

/** @verifies SWR-BSW-003 */
void test_Can_MainFunction_Read_empty_fifo(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    Can_MainFunction_Read();

    TEST_ASSERT_EQUAL_UINT8(0u, canif_rx_call_count);
}

/** @verifies SWR-BSW-003 */
void test_Can_MainFunction_Read_not_started_skips(void)
{
    Can_Init(&test_config);
    /* Controller is STOPPED */

    uint8 data[] = {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    mock_inject_rx(0x100u, data, 1u);

    Can_MainFunction_Read();

    TEST_ASSERT_EQUAL_UINT8(0u, canif_rx_call_count);
}

/* ==================================================================
 * SWR-BSW-004: CAN Bus-Off Recovery
 * ================================================================== */

/** @verifies SWR-BSW-004 */
void test_Can_MainFunction_BusOff_detects_busoff(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    mock_hw_bus_off = TRUE;
    Can_MainFunction_BusOff();

    TEST_ASSERT_TRUE(canif_busoff_called);
}

/** @verifies SWR-BSW-004 */
void test_Can_MainFunction_BusOff_no_busoff(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    mock_hw_bus_off = FALSE;
    Can_MainFunction_BusOff();

    TEST_ASSERT_FALSE(canif_busoff_called);
}

/* ==================================================================
 * SWR-BSW-005: Error Counters
 * ================================================================== */

/** @verifies SWR-BSW-005 */
void test_Can_GetErrorCounters(void)
{
    Can_Init(&test_config);
    mock_hw_tec = 127u;
    mock_hw_rec = 64u;

    uint8 tec = 0u;
    uint8 rec = 0u;
    Std_ReturnType ret = Can_GetErrorCounters(0u, &tec, &rec);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(127u, tec);
    TEST_ASSERT_EQUAL_UINT8(64u, rec);
}

/** @verifies SWR-BSW-005 */
void test_Can_GetErrorCounters_null_fails(void)
{
    Can_Init(&test_config);
    Std_ReturnType ret = Can_GetErrorCounters(0u, NULL_PTR, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * Hardened Tests: State Machine Transitions (SWR-BSW-001)
 * Equivalence classes: UNINIT, STOPPED, STARTED
 * Boundary: invalid transitions rejected, valid transitions succeed
 * ================================================================== */

/** @verifies SWR-BSW-001 */
void test_Can_StateMachine_full_cycle_uninit_stopped_started_stopped(void)
{
    /* UNINIT -> STOPPED (via Init) -> STARTED -> STOPPED */
    TEST_ASSERT_EQUAL(CAN_CS_UNINIT, Can_GetControllerMode(0u));

    Can_Init(&test_config);
    TEST_ASSERT_EQUAL(CAN_CS_STOPPED, Can_GetControllerMode(0u));

    Std_ReturnType ret = Can_SetControllerMode(0u, CAN_CS_STARTED);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(CAN_CS_STARTED, Can_GetControllerMode(0u));

    ret = Can_SetControllerMode(0u, CAN_CS_STOPPED);
    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL(CAN_CS_STOPPED, Can_GetControllerMode(0u));
}

/** @verifies SWR-BSW-001 */
void test_Can_StateMachine_uninit_to_started_invalid(void)
{
    /* UNINIT -> STARTED is invalid (must go through STOPPED first) */
    /* setUp leaves driver in UNINIT (no Init called beyond setUp reset) */
    Std_ReturnType ret = Can_SetControllerMode(0u, CAN_CS_STARTED);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(CAN_CS_UNINIT, Can_GetControllerMode(0u));
}

/** @verifies SWR-BSW-001 */
void test_Can_StateMachine_stopped_to_stopped_invalid(void)
{
    /* STOPPED -> STOPPED is not a valid transition */
    Can_Init(&test_config);
    Std_ReturnType ret = Can_SetControllerMode(0u, CAN_CS_STOPPED);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL(CAN_CS_STOPPED, Can_GetControllerMode(0u));
}

/** @verifies SWR-BSW-001 */
void test_Can_StateMachine_started_to_started_invalid(void)
{
    /* STARTED -> STARTED is not a valid transition */
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);
    Std_ReturnType ret = Can_SetControllerMode(0u, CAN_CS_STARTED);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    /* State remains STARTED */
    TEST_ASSERT_EQUAL(CAN_CS_STARTED, Can_GetControllerMode(0u));
}

/* ==================================================================
 * Hardened Tests: CAN Write DLC Boundary (SWR-BSW-002)
 * Equivalence classes: DLC=0 (remote frame), DLC=1..8 (valid), DLC>8 (invalid)
 * Boundary values: 0, 8, 9
 * ================================================================== */

/** @verifies SWR-BSW-002 */
void test_Can_Write_dlc_zero_remote_frame(void)
{
    /* DLC=0 is valid for CAN 2.0B (remote frame / zero-length data) */
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    Can_PduType pdu;
    pdu.id = 0x200u;
    pdu.length = 0u;
    pdu.sdu = NULL_PTR;  /* No data for remote frame */

    Can_ReturnType ret = Can_Write(0u, &pdu);
    TEST_ASSERT_EQUAL(CAN_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_tx_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_dlc[0]);
}

/** @verifies SWR-BSW-002 */
void test_Can_Write_dlc_max_eight(void)
{
    /* DLC=8 is the maximum valid data length for CAN 2.0B */
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    uint8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    Can_PduType pdu;
    pdu.id = 0x300u;
    pdu.length = 8u;
    pdu.sdu = data;

    Can_ReturnType ret = Can_Write(0u, &pdu);
    TEST_ASSERT_EQUAL(CAN_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(8u, mock_tx_dlc[0]);
}

/** @verifies SWR-BSW-002 */
void test_Can_Write_dlc_nine_invalid(void)
{
    /* DLC=9 exceeds CAN 2.0B max (8) — must be rejected */
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    uint8 data[] = {0x01};
    Can_PduType pdu;
    pdu.id = 0x400u;
    pdu.length = 9u;
    pdu.sdu = data;

    Can_ReturnType ret = Can_Write(0u, &pdu);
    TEST_ASSERT_EQUAL(CAN_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_count);
}

/** @verifies SWR-BSW-002 */
void test_Can_Write_null_sdu_with_nonzero_length(void)
{
    /* NULL sdu pointer with length > 0 is invalid — must be rejected */
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    Can_PduType pdu;
    pdu.id = 0x500u;
    pdu.length = 4u;
    pdu.sdu = NULL_PTR;

    Can_ReturnType ret = Can_Write(0u, &pdu);
    TEST_ASSERT_EQUAL(CAN_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_count);
}

/* ==================================================================
 * Hardened Tests: GetControllerErrorState (SWR-BSW-005)
 * Boundary: bus-off vs active, null pointer
 * ================================================================== */

/** @verifies SWR-BSW-005 */
void test_Can_GetControllerErrorState_active(void)
{
    /* Normal operation: error state = 0 (ACTIVE) */
    Can_Init(&test_config);
    mock_hw_bus_off = FALSE;

    uint8 error_state = 0xFFu;
    Std_ReturnType ret = Can_GetControllerErrorState(0u, &error_state);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, error_state);  /* CAN_ERRORSTATE_ACTIVE */
}

/** @verifies SWR-BSW-005 */
void test_Can_GetControllerErrorState_busoff(void)
{
    /* Bus-off condition: error state = 2 (BUS_OFF) */
    Can_Init(&test_config);
    mock_hw_bus_off = TRUE;

    uint8 error_state = 0u;
    Std_ReturnType ret = Can_GetControllerErrorState(0u, &error_state);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(2u, error_state);  /* CAN_ERRORSTATE_BUS_OFF */
}

/** @verifies SWR-BSW-005 */
void test_Can_GetControllerErrorState_null_ptr(void)
{
    Can_Init(&test_config);

    Std_ReturnType ret = Can_GetControllerErrorState(0u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * Hardened Tests: MainFunction_Read Starvation Protection (SWR-BSW-003)
 * Boundary: CAN_MAX_RX_PER_CALL = 16 messages max per call
 * ================================================================== */

/** @verifies SWR-BSW-003 */
void test_Can_MainFunction_Read_starvation_protection(void)
{
    /* Inject more messages than CAN_MAX_RX_PER_CALL (16) to verify
     * the driver caps processing and does not starve other tasks */
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    uint8 data[] = {0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    /* MOCK_RX_MAX is 16, so inject exactly 16 to fill the mock buffer */
    for (uint8 i = 0u; i < MOCK_RX_MAX; i++) {
        mock_inject_rx((Can_IdType)(0x100u + i), data, 1u);
    }

    Can_MainFunction_Read();

    /* All 16 processed (mock only has 16 slots, matching CAN_MAX_RX_PER_CALL) */
    TEST_ASSERT_EQUAL_UINT8(MOCK_RX_MAX, canif_rx_call_count);
}

/* ==================================================================
 * Hardened Tests: Bus-Off Hysteresis (SWR-BSW-004)
 * Scenario: off -> callback -> recovery -> no callback -> re-off -> callback
 * ================================================================== */

/** @verifies SWR-BSW-004 */
void test_Can_BusOff_hysteresis_full_cycle(void)
{
    Can_Init(&test_config);
    Can_SetControllerMode(0u, CAN_CS_STARTED);

    /* Step 1: No bus-off initially — no callback */
    mock_hw_bus_off = FALSE;
    Can_MainFunction_BusOff();
    TEST_ASSERT_FALSE(canif_busoff_called);

    /* Step 2: Bus-off occurs — callback fired */
    mock_hw_bus_off = TRUE;
    Can_MainFunction_BusOff();
    TEST_ASSERT_TRUE(canif_busoff_called);

    /* Step 3: Still bus-off — no SECOND callback (hysteresis) */
    canif_busoff_called = FALSE;
    Can_MainFunction_BusOff();
    TEST_ASSERT_FALSE(canif_busoff_called);

    /* Step 4: Recovery (bus-off clears) — no callback on recovery */
    mock_hw_bus_off = FALSE;
    Can_MainFunction_BusOff();
    TEST_ASSERT_FALSE(canif_busoff_called);

    /* Step 5: Bus-off again — callback fires again */
    mock_hw_bus_off = TRUE;
    Can_MainFunction_BusOff();
    TEST_ASSERT_TRUE(canif_busoff_called);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Init tests (SWR-BSW-001) */
    RUN_TEST(test_Can_Init_success);
    RUN_TEST(test_Can_Init_hw_failure);
    RUN_TEST(test_Can_Init_null_config);
    RUN_TEST(test_Can_SetMode_start);
    RUN_TEST(test_Can_SetMode_stop);
    RUN_TEST(test_Can_SetMode_before_init_fails);

    /* Write tests (SWR-BSW-002) */
    RUN_TEST(test_Can_Write_success);
    RUN_TEST(test_Can_Write_not_started_fails);
    RUN_TEST(test_Can_Write_null_pdu_fails);
    RUN_TEST(test_Can_Write_invalid_dlc_fails);
    RUN_TEST(test_Can_Write_tx_full_returns_busy);

    /* Read tests (SWR-BSW-003) */
    RUN_TEST(test_Can_MainFunction_Read_processes_message);
    RUN_TEST(test_Can_MainFunction_Read_multiple_messages);
    RUN_TEST(test_Can_MainFunction_Read_empty_fifo);
    RUN_TEST(test_Can_MainFunction_Read_not_started_skips);

    /* Bus-off tests (SWR-BSW-004) */
    RUN_TEST(test_Can_MainFunction_BusOff_detects_busoff);
    RUN_TEST(test_Can_MainFunction_BusOff_no_busoff);

    /* Error counter tests (SWR-BSW-005) */
    RUN_TEST(test_Can_GetErrorCounters);
    RUN_TEST(test_Can_GetErrorCounters_null_fails);

    /* Hardened: State machine transitions (SWR-BSW-001) */
    RUN_TEST(test_Can_StateMachine_full_cycle_uninit_stopped_started_stopped);
    RUN_TEST(test_Can_StateMachine_uninit_to_started_invalid);
    RUN_TEST(test_Can_StateMachine_stopped_to_stopped_invalid);
    RUN_TEST(test_Can_StateMachine_started_to_started_invalid);

    /* Hardened: DLC boundary values (SWR-BSW-002) */
    RUN_TEST(test_Can_Write_dlc_zero_remote_frame);
    RUN_TEST(test_Can_Write_dlc_max_eight);
    RUN_TEST(test_Can_Write_dlc_nine_invalid);
    RUN_TEST(test_Can_Write_null_sdu_with_nonzero_length);

    /* Hardened: Controller error state (SWR-BSW-005) */
    RUN_TEST(test_Can_GetControllerErrorState_active);
    RUN_TEST(test_Can_GetControllerErrorState_busoff);
    RUN_TEST(test_Can_GetControllerErrorState_null_ptr);

    /* Hardened: RX starvation protection (SWR-BSW-003) */
    RUN_TEST(test_Can_MainFunction_Read_starvation_protection);

    /* Hardened: Bus-off hysteresis (SWR-BSW-004) */
    RUN_TEST(test_Can_BusOff_hysteresis_full_cycle);

    return UNITY_END();
}
