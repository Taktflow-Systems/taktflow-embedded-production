/**
 * @file    test_int_dem_to_dcm.c
 * @brief   Integration test: DEM error report -> DCM DTC readout via UDS
 * @date    2026-02-24
 *
 * @verifies SWR-BSW-017, SWR-BSW-018
 * @aspice   SWE.5 — Software Component Verification & Integration
 * @iso      ISO 26262 Part 6, Section 10
 *
 * Tests the diagnostic chain: DEM event storage with debouncing, DCM DID
 * readout of DTC status, UDS session management, NRC generation, and
 * end-to-end PDU routing through PduR -> CanIf -> Can_Write (mocked).
 *
 * Linked modules (REAL): Dem.c, Dcm.c, PduR.c, CanIf.c
 * Mocked: Can_Write (captures TX response)
 *
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "Dem.h"
#include "Dcm.h"
#include "PduR.h"
#include "CanIf.h"
#include "Can.h"

/* ====================================================================
 * Mock: Can_Write — captures transmitted CAN frame
 * ==================================================================== */

static Can_ReturnType mock_can_write_result;
static uint8          mock_can_tx_data[8];
static uint8          mock_can_tx_dlc;
static Can_IdType     mock_can_tx_id;
static uint8          mock_can_tx_count;

Can_ReturnType Can_Write(uint8 Hth, const Can_PduType* PduInfo)
{
    (void)Hth;
    if (PduInfo != NULL_PTR) {
        mock_can_tx_id  = PduInfo->id;
        mock_can_tx_dlc = PduInfo->length;
        if (PduInfo->sdu != NULL_PTR) {
            uint8 i;
            for (i = 0u; (i < PduInfo->length) && (i < 8u); i++) {
                mock_can_tx_data[i] = PduInfo->sdu[i];
            }
        }
        mock_can_tx_count++;
    }
    return mock_can_write_result;
}

/* ====================================================================
 * Mock: Com_RxIndication — PduR routes COM PDUs here (unused in these tests)
 * ==================================================================== */

void Com_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    (void)RxPduId;
    (void)PduInfoPtr;
}

/* ====================================================================
 * DID Read Callback: reads DEM event 5 status via Dem_GetEventStatus
 * ==================================================================== */

#define DID_DTC_STATUS        0xD001u
#define DID_DTC_STATUS_LEN    1u
#define DEM_EVENT_UNDER_TEST  5u

static Std_ReturnType DID_ReadDtcStatus(uint8* Data, uint8 Length)
{
    if ((Data == NULL_PTR) || (Length < DID_DTC_STATUS_LEN)) {
        return E_NOT_OK;
    }
    return Dem_GetEventStatus(DEM_EVENT_UNDER_TEST, &Data[0]);
}

/* ====================================================================
 * Test Configuration
 * ==================================================================== */

/* DCM DID table */
static const Dcm_DidTableType test_did_table[] = {
    { DID_DTC_STATUS, DID_ReadDtcStatus, DID_DTC_STATUS_LEN }
};

static Dcm_ConfigType dcm_cfg;

/* PduR routing table: PDU 0 -> DCM */
#define PDUR_RX_DIAG_PDU  0u
#define PDUR_DCM_UPPER_ID 0u

static const PduR_RoutingTableType pdur_routes[] = {
    { PDUR_RX_DIAG_PDU, PDUR_DEST_DCM, PDUR_DCM_UPPER_ID }
};

static PduR_ConfigType pdur_cfg;

/* CanIf RX config: CAN ID 0x7DF -> PDU 0 (diag request) */
#define CANIF_DIAG_RX_CAN_ID  0x7DFu
/* CanIf TX config: PDU 0 -> CAN ID 0x7E8, HTH 0 */
#define CANIF_DIAG_TX_CAN_ID  0x7E8u

static const CanIf_RxPduConfigType canif_rx[] = {
    { CANIF_DIAG_RX_CAN_ID, PDUR_RX_DIAG_PDU, 8u, FALSE }
};

static const CanIf_TxPduConfigType canif_tx[] = {
    { CANIF_DIAG_TX_CAN_ID, 0u, 8u, 0u }
};

static CanIf_ConfigType canif_cfg;

/* ====================================================================
 * Helper: reset mock state
 * ==================================================================== */

static void reset_mocks(void)
{
    uint8 i;
    mock_can_write_result = CAN_OK;
    mock_can_tx_dlc       = 0u;
    mock_can_tx_id        = 0u;
    mock_can_tx_count     = 0u;
    for (i = 0u; i < 8u; i++) {
        mock_can_tx_data[i] = 0u;
    }
}

/* ====================================================================
 * Helper: inject UDS request via CanIf RX indication and process
 * ==================================================================== */

static void inject_uds_request(const uint8* data, uint8 length)
{
    /* Simulate CAN reception: CanIf routes to PduR, PduR routes to DCM */
    CanIf_RxIndication(CANIF_DIAG_RX_CAN_ID, data, length);
    /* DCM processes in MainFunction */
    Dcm_MainFunction();
}

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    reset_mocks();

    /* Init all linked modules in stack order */
    Dem_Init(NULL_PTR);

    canif_cfg.txPduConfig = canif_tx;
    canif_cfg.txPduCount  = 1u;
    canif_cfg.rxPduConfig = canif_rx;
    canif_cfg.rxPduCount  = 1u;
    CanIf_Init(&canif_cfg);

    pdur_cfg.routingTable = pdur_routes;
    pdur_cfg.routingCount = 1u;
    PduR_Init(&pdur_cfg);

    dcm_cfg.DidTable    = test_did_table;
    dcm_cfg.DidCount    = 1u;
    dcm_cfg.TxPduId     = 0u;
    dcm_cfg.S3TimeoutMs = 5000u;
    Dcm_Init(&dcm_cfg);
}

void tearDown(void) { }

/* ====================================================================
 * INT-004-01: Report DEM event as FAILED 3 times (debounce), then
 *             read DTC status via UDS ReadDID -> verify CONFIRMED
 * ==================================================================== */

/** @verifies SWR-BSW-017, SWR-BSW-018 */
void test_int_dem_report_then_dcm_read_status(void)
{
    uint8 i;

    /* Report event FAILED 3 times to pass debounce threshold */
    for (i = 0u; i < (uint8)DEM_DEBOUNCE_FAIL_THRESHOLD; i++) {
        Dem_ReportErrorStatus(DEM_EVENT_UNDER_TEST, DEM_EVENT_STATUS_FAILED);
    }

    /* Verify DEM internally stored the confirmed DTC */
    {
        uint8 status = 0u;
        TEST_ASSERT_EQUAL(E_OK, Dem_GetEventStatus(DEM_EVENT_UNDER_TEST, &status));
        TEST_ASSERT_BITS(DEM_STATUS_CONFIRMED_DTC, DEM_STATUS_CONFIRMED_DTC, status);
    }

    /* Send UDS ReadDataByIdentifier for DID 0xD001 */
    {
        uint8 req[] = { 0x22u, (uint8)(DID_DTC_STATUS >> 8u),
                         (uint8)(DID_DTC_STATUS & 0xFFu) };
        inject_uds_request(req, 3u);
    }

    /* Verify positive response with confirmed DTC status */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x62u, mock_can_tx_data[0]);                  /* SID + 0x40 */
    TEST_ASSERT_EQUAL_HEX8((uint8)(DID_DTC_STATUS >> 8u), mock_can_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8((uint8)(DID_DTC_STATUS & 0xFFu), mock_can_tx_data[2]);
    TEST_ASSERT_BITS(DEM_STATUS_CONFIRMED_DTC, DEM_STATUS_CONFIRMED_DTC,
                     mock_can_tx_data[3]);
}

/* ====================================================================
 * INT-004-02: Clear all DTCs, then UDS read -> verify status is 0
 * ==================================================================== */

/** @verifies SWR-BSW-017, SWR-BSW-018 */
void test_int_dem_clear_then_dcm_reads_clean(void)
{
    uint8 i;

    /* First create a confirmed DTC */
    for (i = 0u; i < (uint8)DEM_DEBOUNCE_FAIL_THRESHOLD; i++) {
        Dem_ReportErrorStatus(DEM_EVENT_UNDER_TEST, DEM_EVENT_STATUS_FAILED);
    }

    /* Clear all DTCs */
    TEST_ASSERT_EQUAL(E_OK, Dem_ClearAllDTCs());

    /* Read via UDS */
    {
        uint8 req[] = { 0x22u, (uint8)(DID_DTC_STATUS >> 8u),
                         (uint8)(DID_DTC_STATUS & 0xFFu) };
        inject_uds_request(req, 3u);
    }

    /* Verify DTC status byte is 0 (all clear) */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x62u, mock_can_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_can_tx_data[3]);
}

/* ====================================================================
 * INT-004-03: UDS session switch to Extended, verify positive response,
 *             then send ReadDID in extended session
 * ==================================================================== */

/** @verifies SWR-BSW-017 */
void test_int_dcm_uds_session_switch(void)
{
    /* Send DiagnosticSessionControl to Extended (0x10, 0x03) */
    {
        uint8 req[] = { 0x10u, 0x03u };
        inject_uds_request(req, 2u);
    }

    /* Verify positive response */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x50u, mock_can_tx_data[0]);  /* 0x10 + 0x40 */
    TEST_ASSERT_EQUAL_HEX8(0x03u, mock_can_tx_data[1]);
    TEST_ASSERT_EQUAL(DCM_EXTENDED_SESSION, Dcm_GetCurrentSession());

    /* Reset mock for next response */
    reset_mocks();

    /* Send ReadDID in extended session */
    {
        uint8 req[] = { 0x22u, (uint8)(DID_DTC_STATUS >> 8u),
                         (uint8)(DID_DTC_STATUS & 0xFFu) };
        inject_uds_request(req, 3u);
    }

    /* Verify positive response for ReadDID */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x62u, mock_can_tx_data[0]);
}

/* ====================================================================
 * INT-004-04: Send unknown SID 0xFF -> verify NRC (0x7F, 0xFF, 0x11)
 * ==================================================================== */

/** @verifies SWR-BSW-017 */
void test_int_dcm_unknown_sid_nrc(void)
{
    uint8 req[] = { 0xFFu, 0x00u };
    inject_uds_request(req, 2u);

    /* NRC: 0x7F (negative response SID), 0xFF (rejected SID), 0x11 (not supported) */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_tx_count);
    TEST_ASSERT_EQUAL_HEX8(DCM_NEGATIVE_RESPONSE_SID, mock_can_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, mock_can_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(DCM_NRC_SERVICE_NOT_SUPPORTED, mock_can_tx_data[2]);
}

/* ====================================================================
 * INT-004-05: Verify DCM response routes through PduR -> CanIf -> Can_Write
 *             by checking the CAN ID of the transmitted frame
 * ==================================================================== */

/** @verifies SWR-BSW-013, SWR-BSW-011, SWR-BSW-017 */
void test_int_dcm_response_routes_through_pdur(void)
{
    /* Send TesterPresent (simple request with guaranteed response) */
    uint8 req[] = { 0x3Eu, 0x00u };
    inject_uds_request(req, 2u);

    /* Verify the mock Can_Write was called (proves full routing chain) */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_tx_count);

    /* Verify CAN TX ID matches CanIf TX config */
    TEST_ASSERT_EQUAL_HEX32(CANIF_DIAG_TX_CAN_ID, mock_can_tx_id);

    /* Verify TesterPresent positive response content */
    TEST_ASSERT_EQUAL_HEX8(0x7Eu, mock_can_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_can_tx_data[1]);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_int_dem_report_then_dcm_read_status);
    RUN_TEST(test_int_dem_clear_then_dcm_reads_clean);
    RUN_TEST(test_int_dcm_uds_session_switch);
    RUN_TEST(test_int_dcm_unknown_sid_nrc);
    RUN_TEST(test_int_dcm_response_routes_through_pdur);

    return UNITY_END();
}
