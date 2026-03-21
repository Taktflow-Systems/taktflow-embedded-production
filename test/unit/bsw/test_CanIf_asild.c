/**
 * @file    test_CanIf.c
 * @brief   Unit tests for CAN Interface (CanIf) module
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-011, SWR-BSW-012, TSR-022
 *
 * Tests PDU routing (CAN ID <-> PduId mapping), transmit path,
 * receive indication, and controller mode management.
 */
#include "unity.h"
#include "CanIf.h"

/* ==================================================================
 * Test Configuration — routing tables for test
 * ================================================================== */

/* TX config: PduId -> CAN ID */
static const CanIf_TxPduConfigType test_tx_config[] = {
    { 0x100u, 0u, 8u, 0u },  /* PDU 0 -> CAN 0x100, DLC 8, Hth 0 */
    { 0x010u, 1u, 8u, 0u },  /* PDU 1 -> CAN 0x010, DLC 8, Hth 0 */
    { 0x001u, 2u, 8u, 0u },  /* PDU 2 -> CAN 0x001, DLC 8, Hth 0 */
};

/* RX config: CAN ID -> PduId */
static const CanIf_RxPduConfigType test_rx_config[] = {
    { 0x200u, 0u, 8u, FALSE },  /* CAN 0x200 -> PDU 0 */
    { 0x011u, 1u, 8u, FALSE },  /* CAN 0x011 -> PDU 1 */
    { 0x012u, 2u, 8u, FALSE },  /* CAN 0x012 -> PDU 2 */
};

/* ==================================================================
 * Mock: Can driver (lower layer)
 * ================================================================== */

static Can_ReturnType mock_can_write_result;
static Can_IdType     mock_can_last_id;
static uint8          mock_can_last_dlc;
static uint8          mock_can_last_data[8];
static uint8          mock_can_write_count;

Can_ReturnType Can_Write(uint8 Hth, const Can_PduType* PduInfo)
{
    (void)Hth;
    if (PduInfo == NULL_PTR) {
        return CAN_NOT_OK;
    }
    mock_can_last_id = PduInfo->id;
    mock_can_last_dlc = PduInfo->length;
    for (uint8 i = 0u; i < PduInfo->length; i++) {
        mock_can_last_data[i] = PduInfo->sdu[i];
    }
    mock_can_write_count++;
    return mock_can_write_result;
}

/* ==================================================================
 * Mock: PduR (upper layer)
 * ================================================================== */

static PduIdType  mock_pdur_rx_pdu_id;
static uint8      mock_pdur_rx_data[8];
static uint8      mock_pdur_rx_dlc;
static uint8      mock_pdur_rx_count;

void PduR_CanIfRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    mock_pdur_rx_pdu_id = RxPduId;
    mock_pdur_rx_dlc = PduInfoPtr->SduLength;
    for (uint8 i = 0u; i < PduInfoPtr->SduLength; i++) {
        mock_pdur_rx_data[i] = PduInfoPtr->SduDataPtr[i];
    }
    mock_pdur_rx_count++;
}

/* ==================================================================
 * Mock: E2E RX callback
 * ================================================================== */

static Std_ReturnType    mock_e2e_rx_result;
static uint8             mock_e2e_rx_call_count;
static uint8             mock_e2e_rx_last_pdu_id;

static Std_ReturnType mock_e2e_rx_check(uint8 PduId, const uint8* Data, uint8 Length)
{
    (void)Data;
    (void)Length;
    mock_e2e_rx_call_count++;
    mock_e2e_rx_last_pdu_id = PduId;
    return mock_e2e_rx_result;
}

/* ==================================================================
 * Test fixtures
 * ================================================================== */

static CanIf_ConfigType canif_cfg;

void setUp(void)
{
    mock_can_write_result = CAN_OK;
    mock_can_last_id = 0u;
    mock_can_last_dlc = 0u;
    mock_can_write_count = 0u;
    mock_pdur_rx_pdu_id = 0xFFu;
    mock_pdur_rx_dlc = 0u;
    mock_pdur_rx_count = 0u;
    mock_e2e_rx_result = E_OK;
    mock_e2e_rx_call_count = 0u;
    mock_e2e_rx_last_pdu_id = 0xFFu;

    canif_cfg.txPduConfig    = test_tx_config;
    canif_cfg.txPduCount     = 3u;
    canif_cfg.rxPduConfig    = test_rx_config;
    canif_cfg.rxPduCount     = 3u;
    canif_cfg.e2eRxCheck     = NULL_PTR;

    CanIf_Init(&canif_cfg);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-011: CanIf PDU Routing
 * ================================================================== */

/** @verifies SWR-BSW-011 */
void test_CanIf_Transmit_maps_pdu_to_can_id(void)
{
    uint8 data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    PduInfoType pdu = { data, 8u };

    Std_ReturnType ret = CanIf_Transmit(0u, &pdu);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_HEX32(0x100u, mock_can_last_id);
    TEST_ASSERT_EQUAL_UINT8(8u, mock_can_last_dlc);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_Transmit_second_pdu(void)
{
    uint8 data[] = {0xAA, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { data, 8u };

    CanIf_Transmit(1u, &pdu);

    TEST_ASSERT_EQUAL_HEX32(0x010u, mock_can_last_id);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_Transmit_invalid_pdu_id(void)
{
    uint8 data[] = {0x01};
    PduInfoType pdu = { data, 1u };

    Std_ReturnType ret = CanIf_Transmit(99u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_Transmit_null_pdu(void)
{
    Std_ReturnType ret = CanIf_Transmit(0u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_Transmit_can_busy_returns_not_ok(void)
{
    mock_can_write_result = CAN_BUSY;
    uint8 data[] = {0x01};
    PduInfoType pdu = { data, 1u };

    Std_ReturnType ret = CanIf_Transmit(0u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_RxIndication_routes_to_pdur(void)
{
    uint8 data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00};

    CanIf_RxIndication(0x200u, data, 8u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_pdur_rx_count);
    TEST_ASSERT_EQUAL(0u, mock_pdur_rx_pdu_id);
    TEST_ASSERT_EQUAL_HEX8(0xDE, mock_pdur_rx_data[0]);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_RxIndication_second_mapping(void)
{
    uint8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    CanIf_RxIndication(0x011u, data, 8u);

    TEST_ASSERT_EQUAL(1u, mock_pdur_rx_pdu_id);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_RxIndication_unknown_id_discarded(void)
{
    uint8 data[] = {0xFF};

    CanIf_RxIndication(0x999u, data, 1u);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_pdur_rx_count);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_Init_null_config(void)
{
    CanIf_Init(NULL_PTR);
    /* Should handle gracefully — no crash */
    uint8 data[] = {0x01};
    PduInfoType pdu = { data, 1u };
    Std_ReturnType ret = CanIf_Transmit(0u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-011 / SWR-BSW-012: Hardened Boundary Tests
 * ================================================================== */

/** @verifies SWR-BSW-011 */
void test_CanIf_Transmit_first_pdu_id_valid(void)
{
    uint8 data[] = {0xAA};
    PduInfoType pdu = { data, 1u };

    Std_ReturnType ret = CanIf_Transmit(0u, &pdu);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_HEX32(0x100u, mock_can_last_id);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_Transmit_last_pdu_id_valid(void)
{
    uint8 data[] = {0xBB};
    PduInfoType pdu = { data, 1u };

    /* txPduCount = 3, last valid index = 2 */
    Std_ReturnType ret = CanIf_Transmit(2u, &pdu);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_HEX32(0x001u, mock_can_last_id);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_Transmit_pdu_id_equals_count_invalid(void)
{
    uint8 data[] = {0xCC};
    PduInfoType pdu = { data, 1u };

    /* txPduCount = 3, so index 3 is out of bounds */
    Std_ReturnType ret = CanIf_Transmit(3u, &pdu);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_can_write_count);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_RxIndication_null_sdu_ptr_no_crash(void)
{
    /* NULL SduPtr should be treated as a no-op */
    CanIf_RxIndication(0x200u, NULL_PTR, 8u);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_pdur_rx_count);
}

/** @verifies SWR-BSW-012 */
void test_CanIf_Transmit_before_init_fails(void)
{
    /* Re-init with NULL to enter uninitialized state */
    CanIf_Init(NULL_PTR);

    uint8 data[] = {0x01};
    PduInfoType pdu = { data, 1u };

    Std_ReturnType ret = CanIf_Transmit(0u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_can_write_count);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_RxIndication_third_mapping(void)
{
    uint8 data[] = {0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x00, 0x00, 0x00};

    CanIf_RxIndication(0x012u, data, 8u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_pdur_rx_count);
    TEST_ASSERT_EQUAL(2u, mock_pdur_rx_pdu_id);
    TEST_ASSERT_EQUAL_HEX8(0xCA, mock_pdur_rx_data[0]);
}

/** @verifies SWR-BSW-011 */
void test_CanIf_RxIndication_multiple_messages_routed_correctly(void)
{
    uint8 data1[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8 data2[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11};
    uint8 data3[] = {0xDE, 0xAD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    /* Send three RX messages for all three configured CAN IDs */
    CanIf_RxIndication(0x200u, data1, 8u);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_pdur_rx_count);
    TEST_ASSERT_EQUAL(0u, mock_pdur_rx_pdu_id);
    TEST_ASSERT_EQUAL_HEX8(0x01, mock_pdur_rx_data[0]);

    CanIf_RxIndication(0x011u, data2, 8u);
    TEST_ASSERT_EQUAL_UINT8(2u, mock_pdur_rx_count);
    TEST_ASSERT_EQUAL(1u, mock_pdur_rx_pdu_id);
    TEST_ASSERT_EQUAL_HEX8(0xAA, mock_pdur_rx_data[0]);

    CanIf_RxIndication(0x012u, data3, 8u);
    TEST_ASSERT_EQUAL_UINT8(3u, mock_pdur_rx_count);
    TEST_ASSERT_EQUAL(2u, mock_pdur_rx_pdu_id);
    TEST_ASSERT_EQUAL_HEX8(0xDE, mock_pdur_rx_data[0]);
}

/** @verifies SWR-BSW-012 */
void test_CanIf_RxIndication_before_init_discarded(void)
{
    /* Enter uninitialized state */
    CanIf_Init(NULL_PTR);

    uint8 data[] = {0xFF};
    CanIf_RxIndication(0x200u, data, 1u);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_pdur_rx_count);
}

/* ==================================================================
 * SWR-BSW-011: E2E RX Callback Tests
 * ================================================================== */

/** @verifies SWR-BSW-011 -- E2E callback returning E_OK allows frame through */
void test_CanIf_RxIndication_e2e_callback_ok_routes_to_pdur(void)
{
    canif_cfg.e2eRxCheck = mock_e2e_rx_check;
    mock_e2e_rx_result = E_OK;
    CanIf_Init(&canif_cfg);

    uint8 data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00};
    CanIf_RxIndication(0x200u, data, 8u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_e2e_rx_call_count);
    TEST_ASSERT_EQUAL(0u, mock_e2e_rx_last_pdu_id);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_pdur_rx_count);
}

/** @verifies SWR-BSW-011 -- E2E callback returning E_NOT_OK drops frame */
void test_CanIf_RxIndication_e2e_callback_not_ok_drops_frame(void)
{
    canif_cfg.e2eRxCheck = mock_e2e_rx_check;
    mock_e2e_rx_result = E_NOT_OK;
    CanIf_Init(&canif_cfg);

    uint8 data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00};
    CanIf_RxIndication(0x200u, data, 8u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_e2e_rx_call_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_pdur_rx_count);  /* Frame dropped */
}

/** @verifies SWR-BSW-011 -- NULL E2E callback skips check, routes normally */
void test_CanIf_RxIndication_null_e2e_callback_routes_normally(void)
{
    canif_cfg.e2eRxCheck = NULL_PTR;
    CanIf_Init(&canif_cfg);

    uint8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    CanIf_RxIndication(0x200u, data, 8u);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_e2e_rx_call_count);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_pdur_rx_count);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_CanIf_Transmit_maps_pdu_to_can_id);
    RUN_TEST(test_CanIf_Transmit_second_pdu);
    RUN_TEST(test_CanIf_Transmit_invalid_pdu_id);
    RUN_TEST(test_CanIf_Transmit_null_pdu);
    RUN_TEST(test_CanIf_Transmit_can_busy_returns_not_ok);
    RUN_TEST(test_CanIf_RxIndication_routes_to_pdur);
    RUN_TEST(test_CanIf_RxIndication_second_mapping);
    RUN_TEST(test_CanIf_RxIndication_unknown_id_discarded);
    RUN_TEST(test_CanIf_Init_null_config);

    /* Hardened boundary tests */
    RUN_TEST(test_CanIf_Transmit_first_pdu_id_valid);
    RUN_TEST(test_CanIf_Transmit_last_pdu_id_valid);
    RUN_TEST(test_CanIf_Transmit_pdu_id_equals_count_invalid);
    RUN_TEST(test_CanIf_RxIndication_null_sdu_ptr_no_crash);
    RUN_TEST(test_CanIf_Transmit_before_init_fails);
    RUN_TEST(test_CanIf_RxIndication_third_mapping);
    RUN_TEST(test_CanIf_RxIndication_multiple_messages_routed_correctly);
    RUN_TEST(test_CanIf_RxIndication_before_init_discarded);

    /* E2E RX callback tests */
    RUN_TEST(test_CanIf_RxIndication_e2e_callback_ok_routes_to_pdur);
    RUN_TEST(test_CanIf_RxIndication_e2e_callback_not_ok_drops_frame);
    RUN_TEST(test_CanIf_RxIndication_null_e2e_callback_routes_normally);

    return UNITY_END();
}
