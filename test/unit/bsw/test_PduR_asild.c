/**
 * @file    test_PduR.c
 * @brief   Unit tests for PDU Router module
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-013
 *
 * Tests PDU routing between CanIf and upper layers (Com, Dcm).
 * Both lower and upper layer calls are mocked.
 */
#include "unity.h"
#include "PduR.h"

/* ==================================================================
 * Test Configuration — routing table
 * ================================================================== */

static const PduR_RoutingTableType test_routing[] = {
    { 0u, PDUR_DEST_COM, 0u },   /* RxPdu 0 -> Com PDU 0 */
    { 1u, PDUR_DEST_COM, 1u },   /* RxPdu 1 -> Com PDU 1 */
    { 2u, PDUR_DEST_COM, 2u },   /* RxPdu 2 -> Com PDU 2 */
    { 3u, PDUR_DEST_DCM, 0u },   /* RxPdu 3 -> Dcm PDU 0 */
};

/* ==================================================================
 * Mock: CanIf (lower layer, for TX path)
 * ================================================================== */

static PduIdType      mock_canif_tx_pdu_id;
static uint8          mock_canif_tx_data[8];
static PduLengthType  mock_canif_tx_dlc;
static uint8          mock_canif_tx_count;
static Std_ReturnType mock_canif_tx_result;

Std_ReturnType CanIf_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    mock_canif_tx_pdu_id = TxPduId;
    if (PduInfoPtr != NULL_PTR) {
        mock_canif_tx_dlc = PduInfoPtr->SduLength;
        for (uint8 i = 0u; i < PduInfoPtr->SduLength; i++) {
            mock_canif_tx_data[i] = PduInfoPtr->SduDataPtr[i];
        }
    }
    mock_canif_tx_count++;
    return mock_canif_tx_result;
}

/* ==================================================================
 * Mock: Com (upper layer, for RX path)
 * ================================================================== */

static PduIdType      mock_com_rx_pdu_id;
static uint8          mock_com_rx_data[8];
static PduLengthType  mock_com_rx_dlc;
static uint8          mock_com_rx_count;

void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    mock_com_rx_pdu_id = RxPduId;
    if (PduInfoPtr != NULL_PTR) {
        mock_com_rx_dlc = PduInfoPtr->SduLength;
        for (uint8 i = 0u; i < PduInfoPtr->SduLength; i++) {
            mock_com_rx_data[i] = PduInfoPtr->SduDataPtr[i];
        }
    }
    mock_com_rx_count++;
}

/* ==================================================================
 * Mock: Dcm (upper layer, for diagnostic RX path)
 * ================================================================== */

static PduIdType  mock_dcm_rx_pdu_id;
static uint8      mock_dcm_rx_count;

void Dcm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    (void)PduInfoPtr;
    mock_dcm_rx_pdu_id = RxPduId;
    mock_dcm_rx_count++;
}

/* ==================================================================
 * Mock: CanTp (transport layer, for diagnostic TP RX path)
 * ================================================================== */

static PduIdType  mock_cantp_rx_pdu_id;
static uint8      mock_cantp_rx_count;

void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    (void)PduInfoPtr;
    mock_cantp_rx_pdu_id = RxPduId;
    mock_cantp_rx_count++;
}

/* ==================================================================
 * Test fixtures
 * ================================================================== */

static PduR_ConfigType pdur_cfg;

void setUp(void)
{
    mock_canif_tx_count = 0u;
    mock_canif_tx_result = E_OK;
    mock_com_rx_count = 0u;
    mock_com_rx_pdu_id = 0xFFu;
    mock_dcm_rx_count = 0u;
    mock_dcm_rx_pdu_id = 0xFFu;
    mock_cantp_rx_count = 0u;
    mock_cantp_rx_pdu_id = 0xFFu;

    pdur_cfg.routingTable = test_routing;
    pdur_cfg.routingCount = 4u;

    PduR_Init(&pdur_cfg);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-013: PduR Routing
 * ================================================================== */

/** @verifies SWR-BSW-013 */
void test_PduR_RxIndication_routes_to_com(void)
{
    uint8 data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    PduInfoType pdu = { data, 8u };

    PduR_CanIfRxIndication(0u, &pdu);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_com_rx_count);
    TEST_ASSERT_EQUAL(0u, mock_com_rx_pdu_id);
    TEST_ASSERT_EQUAL_HEX8(0x11, mock_com_rx_data[0]);
}

/** @verifies SWR-BSW-013 */
void test_PduR_RxIndication_routes_to_dcm(void)
{
    uint8 data[] = {0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { data, 8u };

    PduR_CanIfRxIndication(3u, &pdu);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dcm_rx_count);
    TEST_ASSERT_EQUAL(0u, mock_dcm_rx_pdu_id);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_rx_count);
}

/** @verifies SWR-BSW-013 */
void test_PduR_RxIndication_unknown_pdu_ignored(void)
{
    uint8 data[] = {0xFF};
    PduInfoType pdu = { data, 1u };

    PduR_CanIfRxIndication(99u, &pdu);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_rx_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dcm_rx_count);
}

/** @verifies SWR-BSW-013 */
void test_PduR_Transmit_routes_to_canif(void)
{
    uint8 data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { data, 8u };

    Std_ReturnType ret = PduR_Transmit(0u, &pdu);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_canif_tx_count);
    TEST_ASSERT_EQUAL(0u, mock_canif_tx_pdu_id);
}

/** @verifies SWR-BSW-013 */
void test_PduR_Transmit_null_pdu(void)
{
    Std_ReturnType ret = PduR_Transmit(0u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-013 */
void test_PduR_Transmit_canif_failure(void)
{
    mock_canif_tx_result = E_NOT_OK;
    uint8 data[] = {0x01};
    PduInfoType pdu = { data, 1u };

    Std_ReturnType ret = PduR_Transmit(0u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-013 */
void test_PduR_Init_null_config(void)
{
    PduR_Init(NULL_PTR);
    uint8 data[] = {0x01};
    PduInfoType pdu = { data, 1u };
    Std_ReturnType ret = PduR_Transmit(0u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-013 */
void test_PduR_RxIndication_null_pdu(void)
{
    PduR_CanIfRxIndication(0u, NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_rx_count);
}

/* ==================================================================
 * SWR-BSW-013: Hardened Boundary Tests
 * ================================================================== */

/** @verifies SWR-BSW-013 */
void test_PduR_RxIndication_routes_second_com_pdu(void)
{
    uint8 data[] = {0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    PduInfoType pdu = { data, 8u };

    PduR_CanIfRxIndication(1u, &pdu);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_com_rx_count);
    TEST_ASSERT_EQUAL(1u, mock_com_rx_pdu_id);
    TEST_ASSERT_EQUAL_HEX8(0x22, mock_com_rx_data[0]);
}

/** @verifies SWR-BSW-013 */
void test_PduR_RxIndication_routes_third_com_pdu(void)
{
    uint8 data[] = {0xAB, 0xCD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { data, 8u };

    PduR_CanIfRxIndication(2u, &pdu);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_com_rx_count);
    TEST_ASSERT_EQUAL(2u, mock_com_rx_pdu_id);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dcm_rx_count);
}

/** @verifies SWR-BSW-013 */
void test_PduR_DcmTransmit_delegates_to_pdur_transmit(void)
{
    uint8 data[] = {0x50, 0x01, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00};
    PduInfoType pdu = { data, 8u };

    Std_ReturnType ret = PduR_DcmTransmit(0u, &pdu);

    TEST_ASSERT_EQUAL(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_canif_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x50, mock_canif_tx_data[0]);
}

/** @verifies SWR-BSW-013 */
void test_PduR_DcmTransmit_null_pdu_returns_not_ok(void)
{
    Std_ReturnType ret = PduR_DcmTransmit(0u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-013 */
void test_PduR_Transmit_null_pdu_no_canif_call(void)
{
    Std_ReturnType ret = PduR_Transmit(0u, NULL_PTR);

    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_canif_tx_count);
}

/** @verifies SWR-BSW-013 */
void test_PduR_RxIndication_before_init_discarded(void)
{
    /* Enter uninitialized state */
    PduR_Init(NULL_PTR);

    uint8 data[] = {0x01};
    PduInfoType pdu = { data, 1u };

    PduR_CanIfRxIndication(0u, &pdu);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_rx_count);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dcm_rx_count);
}

/** @verifies SWR-BSW-013 */
void test_PduR_Transmit_before_init_returns_not_ok(void)
{
    PduR_Init(NULL_PTR);

    uint8 data[] = {0x01};
    PduInfoType pdu = { data, 1u };

    Std_ReturnType ret = PduR_Transmit(0u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_canif_tx_count);
}

/** @verifies SWR-BSW-013 */
void test_PduR_DcmTransmit_before_init_returns_not_ok(void)
{
    PduR_Init(NULL_PTR);

    uint8 data[] = {0x01};
    PduInfoType pdu = { data, 1u };

    Std_ReturnType ret = PduR_DcmTransmit(0u, &pdu);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_PduR_RxIndication_routes_to_com);
    RUN_TEST(test_PduR_RxIndication_routes_to_dcm);
    RUN_TEST(test_PduR_RxIndication_unknown_pdu_ignored);
    RUN_TEST(test_PduR_Transmit_routes_to_canif);
    RUN_TEST(test_PduR_Transmit_null_pdu);
    RUN_TEST(test_PduR_Transmit_canif_failure);
    RUN_TEST(test_PduR_Init_null_config);
    RUN_TEST(test_PduR_RxIndication_null_pdu);

    /* Hardened boundary tests */
    RUN_TEST(test_PduR_RxIndication_routes_second_com_pdu);
    RUN_TEST(test_PduR_RxIndication_routes_third_com_pdu);
    RUN_TEST(test_PduR_DcmTransmit_delegates_to_pdur_transmit);
    RUN_TEST(test_PduR_DcmTransmit_null_pdu_returns_not_ok);
    RUN_TEST(test_PduR_Transmit_null_pdu_no_canif_call);
    RUN_TEST(test_PduR_RxIndication_before_init_discarded);
    RUN_TEST(test_PduR_Transmit_before_init_returns_not_ok);
    RUN_TEST(test_PduR_DcmTransmit_before_init_returns_not_ok);

    return UNITY_END();
}
