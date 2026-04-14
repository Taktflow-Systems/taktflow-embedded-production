/**
 * @file    test_Dcm.c
 * @brief   Unit tests for Diagnostic Communication Manager
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-017
 *
 * Tests UDS service dispatch (0x10 DiagnosticSessionControl,
 * 0x22 ReadDataByIdentifier, 0x3E TesterPresent), session management,
 * S3 timer timeout, and NRC generation.
 */
#include "unity.h"
#include "Dcm.h"
#include "Dem.h"

#include <string.h>

/* ==================================================================
 * Mock: PduR_DcmTransmit + CanTp_Transmit (lower layer transmit)
 * ================================================================== */

static PduIdType       mock_tx_pdu_id;
static uint8           mock_tx_data[DCM_TX_BUF_SIZE];
static PduLengthType   mock_tx_dlc;
static uint8           mock_tx_count;
static Std_ReturnType  mock_tx_result;

static void mock_capture_tx(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    mock_tx_pdu_id = TxPduId;
    if (PduInfoPtr != NULL_PTR) {
        mock_tx_dlc = PduInfoPtr->SduLength;
        if ((PduInfoPtr->SduDataPtr != NULL_PTR) &&
            (PduInfoPtr->SduLength <= DCM_TX_BUF_SIZE)) {
            (void)memcpy(mock_tx_data, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
        }
    }
    mock_tx_count++;
}

Std_ReturnType PduR_DcmTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    mock_capture_tx(TxPduId, PduInfoPtr);
    return mock_tx_result;
}

Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    mock_capture_tx(TxPduId, PduInfoPtr);
    return mock_tx_result;
}

/* ==================================================================
 * Mock: BswM_RequestMode (for ECUReset)
 * ================================================================== */

static boolean mock_bswm_called;
static uint8   mock_bswm_mode;

Std_ReturnType BswM_RequestMode(BswM_RequesterIdType RequesterId,
                                 BswM_ModeType RequestedMode)
{
    (void)RequesterId;
    mock_bswm_called = TRUE;
    mock_bswm_mode = (uint8)RequestedMode;
    return E_OK;
}

/* ==================================================================
 * Mock: Dem 0x19 filter API
 * ================================================================== */

#define MOCK_DEM_MAX_FILTERED_DTCS  40u

static uint8          mock_dem_filter_mask;
static boolean        mock_dem_report_supported_only;
static Std_ReturnType mock_dem_set_filter_result;
static Std_ReturnType mock_dem_get_count_result;
static uint16         mock_dem_filtered_count;
static uint8          mock_dem_filtered_entry_count;
static uint8          mock_dem_filtered_next_index;
static uint32         mock_dem_filtered_dtcs[MOCK_DEM_MAX_FILTERED_DTCS];
static uint8          mock_dem_filtered_status[MOCK_DEM_MAX_FILTERED_DTCS];
static uint32         mock_dem_clear_selector;
static Dem_ClearDtcResultType mock_dem_clear_result;
static boolean        mock_routine_precondition_ok;
static uint8          mock_routine_precondition_nrc;
static uint8          mock_routine_start_call_count;
static uint8          mock_routine_stop_call_count;
static uint8          mock_routine_results_call_count;
static Dcm_RoutineStateType mock_routine_start_next_state;
static Dcm_RoutineStateType mock_routine_stop_next_state;
static Dcm_RoutineStateType mock_routine_results_next_state;
static uint8          mock_routine_start_payload[8];
static PduLengthType  mock_routine_start_payload_len;
static uint8          mock_routine_stop_payload[8];
static PduLengthType  mock_routine_stop_payload_len;
static uint8          mock_routine_results_payload[8];
static PduLengthType  mock_routine_results_payload_len;
static uint8          mock_routine_start_error;
static uint8          mock_routine_stop_error;
static uint8          mock_routine_results_error;

Std_ReturnType Dem_SetDTCFilter(uint8 StatusMask, boolean ReportSupportedOnly)
{
    mock_dem_filter_mask = StatusMask;
    mock_dem_report_supported_only = ReportSupportedOnly;
    mock_dem_filtered_next_index = 0u;
    return mock_dem_set_filter_result;
}

Std_ReturnType Dem_GetNumberOfFilteredDTC(uint16* CountPtr)
{
    if (CountPtr == NULL_PTR) {
        return E_NOT_OK;
    }

    if (mock_dem_get_count_result != E_OK) {
        return mock_dem_get_count_result;
    }

    *CountPtr = mock_dem_filtered_count;
    return E_OK;
}

Std_ReturnType Dem_GetNextFilteredDTC(uint32* DtcPtr, uint8* StatusPtr)
{
    if ((DtcPtr == NULL_PTR) || (StatusPtr == NULL_PTR)) {
        return E_NOT_OK;
    }

    if (mock_dem_filtered_next_index >= mock_dem_filtered_entry_count) {
        return E_NOT_OK;
    }

    *DtcPtr = mock_dem_filtered_dtcs[mock_dem_filtered_next_index];
    *StatusPtr = mock_dem_filtered_status[mock_dem_filtered_next_index];
    mock_dem_filtered_next_index++;
    return E_OK;
}

Dem_ClearDtcResultType Dem_ClearDTC(uint32 Selector)
{
    mock_dem_clear_selector = Selector;
    return mock_dem_clear_result;
}

static Std_ReturnType Mock_Routine_CheckStart(uint8* ErrorCode)
{
    if (mock_routine_precondition_ok == TRUE) {
        return E_OK;
    }

    if (ErrorCode != NULL_PTR) {
        *ErrorCode = mock_routine_precondition_nrc;
    }
    return E_NOT_OK;
}

static Std_ReturnType Mock_Routine_Start(const uint8* RequestData,
                                         PduLengthType RequestLength,
                                         Dcm_RoutineStateType* NextState,
                                         uint8* ResponseData,
                                         PduLengthType ResponseBufSize,
                                         PduLengthType* ResponseLength,
                                         uint8* ErrorCode)
{
    mock_routine_start_call_count++;
    (void)RequestData;
    (void)RequestLength;

    if (mock_routine_start_error != 0u) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = mock_routine_start_error;
        }
        return E_NOT_OK;
    }

    if ((NextState == NULL_PTR) || (ResponseData == NULL_PTR) || (ResponseLength == NULL_PTR) ||
        (mock_routine_start_payload_len > ResponseBufSize)) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        }
        return E_NOT_OK;
    }

    *NextState = mock_routine_start_next_state;
    *ResponseLength = mock_routine_start_payload_len;
    (void)memcpy(ResponseData, mock_routine_start_payload, mock_routine_start_payload_len);
    return E_OK;
}

static Std_ReturnType Mock_Routine_Stop(Dcm_RoutineStateType CurrentState,
                                        Dcm_RoutineStateType* NextState,
                                        uint8* ResponseData,
                                        PduLengthType ResponseBufSize,
                                        PduLengthType* ResponseLength,
                                        uint8* ErrorCode)
{
    mock_routine_stop_call_count++;
    (void)CurrentState;

    if (mock_routine_stop_error != 0u) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = mock_routine_stop_error;
        }
        return E_NOT_OK;
    }

    if ((NextState == NULL_PTR) || (ResponseData == NULL_PTR) || (ResponseLength == NULL_PTR) ||
        (mock_routine_stop_payload_len > ResponseBufSize)) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        }
        return E_NOT_OK;
    }

    *NextState = mock_routine_stop_next_state;
    *ResponseLength = mock_routine_stop_payload_len;
    (void)memcpy(ResponseData, mock_routine_stop_payload, mock_routine_stop_payload_len);
    return E_OK;
}

static Std_ReturnType Mock_Routine_Results(Dcm_RoutineStateType CurrentState,
                                           Dcm_RoutineStateType* NextState,
                                           uint8* ResponseData,
                                           PduLengthType ResponseBufSize,
                                           PduLengthType* ResponseLength,
                                           uint8* ErrorCode)
{
    mock_routine_results_call_count++;
    (void)CurrentState;

    if (mock_routine_results_error != 0u) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = mock_routine_results_error;
        }
        return E_NOT_OK;
    }

    if ((NextState == NULL_PTR) || (ResponseData == NULL_PTR) || (ResponseLength == NULL_PTR) ||
        (mock_routine_results_payload_len > ResponseBufSize)) {
        if (ErrorCode != NULL_PTR) {
            *ErrorCode = DCM_NRC_REQUEST_OUT_OF_RANGE;
        }
        return E_NOT_OK;
    }

    *NextState = mock_routine_results_next_state;
    *ResponseLength = mock_routine_results_payload_len;
    (void)memcpy(ResponseData, mock_routine_results_payload, mock_routine_results_payload_len);
    return E_OK;
}

/* ==================================================================
 * DID Read Callbacks (test DIDs)
 * ================================================================== */

static uint8 did_ecu_id_data[4] = {0x01u, 0x02u, 0x03u, 0x04u};
static uint8 did_sw_ver_data[2] = {0x01u, 0x00u};

static Std_ReturnType DID_ReadEcuId(uint8* Data, uint8 Length)
{
    uint8 i;
    if (Data == NULL_PTR) {
        return E_NOT_OK;
    }
    for (i = 0u; (i < Length) && (i < 4u); i++) {
        Data[i] = did_ecu_id_data[i];
    }
    return E_OK;
}

static Std_ReturnType DID_ReadSwVer(uint8* Data, uint8 Length)
{
    uint8 i;
    if (Data == NULL_PTR) {
        return E_NOT_OK;
    }
    for (i = 0u; (i < Length) && (i < 2u); i++) {
        Data[i] = did_sw_ver_data[i];
    }
    return E_OK;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static const Dcm_DidTableType test_did_table[] = {
    { 0xF190u, DID_ReadEcuId, 4u },   /* VIN / ECU ID */
    { 0xF195u, DID_ReadSwVer, 2u },   /* SW version   */
};

static const Dcm_RoutineEntryType test_routine_table[] = {
    { 0x0201u, Mock_Routine_CheckStart, Mock_Routine_Start, Mock_Routine_Stop, Mock_Routine_Results },
};

static Dcm_ConfigType test_config;

void setUp(void)
{
    mock_tx_count  = 0u;
    mock_tx_result = E_OK;
    mock_tx_dlc    = 0u;
    (void)memset(mock_tx_data, 0, sizeof(mock_tx_data));

    mock_bswm_called = FALSE;
    mock_bswm_mode   = 0xFFu;

    mock_dem_filter_mask = 0x00u;
    mock_dem_report_supported_only = FALSE;
    mock_dem_set_filter_result = E_OK;
    mock_dem_get_count_result = E_OK;
    mock_dem_filtered_count = 0u;
    mock_dem_filtered_entry_count = 0u;
    mock_dem_filtered_next_index = 0u;
    mock_dem_clear_selector = 0u;
    mock_dem_clear_result = DEM_CLEAR_DTC_OK;
    (void)memset(mock_dem_filtered_dtcs, 0, sizeof(mock_dem_filtered_dtcs));
    (void)memset(mock_dem_filtered_status, 0, sizeof(mock_dem_filtered_status));

    mock_routine_precondition_ok = TRUE;
    mock_routine_precondition_nrc = DCM_NRC_CONDITIONS_NOT_CORRECT;
    mock_routine_start_call_count = 0u;
    mock_routine_stop_call_count = 0u;
    mock_routine_results_call_count = 0u;
    mock_routine_start_next_state = DCM_ROUTINE_STATE_RUNNING;
    mock_routine_stop_next_state = DCM_ROUTINE_STATE_STOPPED;
    mock_routine_results_next_state = DCM_ROUTINE_STATE_RUNNING;
    mock_routine_start_payload[0] = 0xA5u;
    mock_routine_start_payload_len = 1u;
    mock_routine_stop_payload[0] = 0x5Au;
    mock_routine_stop_payload_len = 1u;
    mock_routine_results_payload[0] = 0x11u;
    mock_routine_results_payload[1] = 0x22u;
    mock_routine_results_payload_len = 2u;
    mock_routine_start_error = 0u;
    mock_routine_stop_error = 0u;
    mock_routine_results_error = 0u;

    test_config.DidTable     = test_did_table;
    test_config.DidCount     = 2u;
    test_config.TxPduId      = 0u;
    test_config.S3TimeoutMs  = 5000u;
    test_config.RoutineTable = test_routine_table;
    test_config.RoutineCount = 1u;

    Dcm_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-017: DiagnosticSessionControl (SID 0x10)
 * ================================================================== */

/** @verifies SWR-BSW-017 — switch to extended session */
void test_Dcm_SessionControl_extended(void)
{
    uint8 req[] = {0x10u, 0x03u};  /* SID 0x10, sub=ExtendedSession */
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Positive response: 0x50, 0x03 */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x50u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03u, mock_tx_data[1]);
}

/** @verifies SWR-BSW-017 — switch to default session */
void test_Dcm_SessionControl_default(void)
{
    uint8 req[] = {0x10u, 0x01u};  /* SID 0x10, sub=DefaultSession */
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x50u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[1]);
}

/** @verifies SWR-BSW-017 — session control wrong length */
void test_Dcm_SessionControl_wrong_length(void)
{
    uint8 req[] = {0x10u};  /* Too short — needs 2 bytes */
    PduInfoType pdu = { req, 1u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* NRC 0x13: incorrectMessageLengthOrInvalidFormat */
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x10u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x13u, mock_tx_data[2]);
}

/* ==================================================================
 * SWR-BSW-017: ReadDataByIdentifier (SID 0x22)
 * ================================================================== */

/** @verifies SWR-BSW-017 — read DID 0xF190 (ECU ID) */
void test_Dcm_ReadDID_EcuId(void)
{
    uint8 req[] = {0x22u, 0xF1u, 0x90u};  /* SID 0x22, DID 0xF190 */
    PduInfoType pdu = { req, 3u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Positive response: 0x62, DID_H, DID_L, data... */
    TEST_ASSERT_EQUAL_HEX8(0x62u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF1u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x90u, mock_tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[3]);
}

/** @verifies SWR-BSW-017 — read DID 0xF195 (SW version) */
void test_Dcm_ReadDID_SwVersion(void)
{
    uint8 req[] = {0x22u, 0xF1u, 0x95u};
    PduInfoType pdu = { req, 3u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x62u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF1u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x95u, mock_tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[4]);
}

/** @verifies SWR-BSW-017 — read unsupported DID -> NRC 0x31 */
void test_Dcm_ReadDID_unsupported(void)
{
    uint8 req[] = {0x22u, 0xFFu, 0xFFu};  /* Non-existent DID */
    PduInfoType pdu = { req, 3u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* NRC 0x31: requestOutOfRange */
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x31u, mock_tx_data[2]);
}

/** @verifies SWR-BSW-017 — read DID wrong length */
void test_Dcm_ReadDID_wrong_length(void)
{
    uint8 req[] = {0x22u, 0xF1u};  /* Too short — needs 3 bytes */
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x13u, mock_tx_data[2]);
}

/* ==================================================================
 * SWR-BSW-017: TesterPresent (SID 0x3E)
 * ================================================================== */

/** @verifies SWR-BSW-017 — tester present positive response */
void test_Dcm_TesterPresent(void)
{
    uint8 req[] = {0x3Eu, 0x00u};  /* SID 0x3E, sub=0x00 */
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Positive response: 0x7E, 0x00 */
    TEST_ASSERT_EQUAL_HEX8(0x7Eu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[1]);
}

/** @verifies SWR-BSW-017 — tester present suppress response (sub=0x80) */
void test_Dcm_TesterPresent_suppress(void)
{
    uint8 req[] = {0x3Eu, 0x80u};  /* suppressPosRspMsgIndicationBit */
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* No response should be sent */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_count);
}

/* ==================================================================
 * SWR-BSW-017: NRC and Error Handling
 * ================================================================== */

/** @verifies SWR-BSW-017 — unknown SID -> NRC 0x11 */
void test_Dcm_unknown_SID(void)
{
    uint8 req[] = {0xFFu, 0x00u};  /* Unknown SID */
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* NRC 0x11: serviceNotSupported */
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x11u, mock_tx_data[2]);
}

/** @verifies SWR-BSW-017 — null PDU info */
void test_Dcm_RxIndication_null_pdu(void)
{
    Dcm_RxIndication(0u, NULL_PTR);
    Dcm_MainFunction();

    /* Should not crash, no response */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_count);
}

/** @verifies SWR-BSW-017 — init null config */
void test_Dcm_Init_null_config(void)
{
    Dcm_Init(NULL_PTR);

    uint8 req[] = {0x3Eu, 0x00u};
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Not initialized — no response */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_count);
}

/** @verifies SWR-BSW-017 — S3 session timeout */
void test_Dcm_S3_timeout_resets_session(void)
{
    /* Enter extended session */
    uint8 req[] = {0x10u, 0x03u};
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();
    mock_tx_count = 0u;

    /* Simulate S3 timeout: call MainFunction enough times (5000ms / 10ms = 500 ticks) */
    uint16 i;
    for (i = 0u; i < 501u; i++) {
        Dcm_MainFunction();
    }

    /* Session should revert to DEFAULT */
    TEST_ASSERT_EQUAL(DCM_DEFAULT_SESSION, Dcm_GetCurrentSession());
}

/** @verifies SWR-BSW-017 — zero-length request */
void test_Dcm_empty_request(void)
{
    uint8 req[] = {0x00u};
    PduInfoType pdu = { req, 0u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Empty request — no response */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_count);
}

/* ==================================================================
 * SWR-BSW-017: Hardened Boundary / Fault Injection Tests
 * ================================================================== */

/** @verifies SWR-BSW-017
 *  Equivalence class: NULL request data pointer inside PduInfoType */
void test_Dcm_RxIndication_null_data_ptr(void)
{
    PduInfoType pdu = { NULL_PTR, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Should not crash, no response */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_count);
}

/** @verifies SWR-BSW-017
 *  Equivalence class: Request length — 1 byte (SID only, no sub-function)
 *  SID 0x10 requires 2 bytes, so SID-only should get NRC 0x13 */
void test_Dcm_SessionControl_sid_only(void)
{
    uint8 req[] = {0x10u};
    PduInfoType pdu = { req, 1u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* NRC 0x13: incorrectMessageLengthOrInvalidFormat */
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x10u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x13u, mock_tx_data[2]);
}

/** @verifies SWR-BSW-017
 *  Equivalence class: Request with max-length payload (8 bytes for CAN 2.0B) */
void test_Dcm_MaxLength_request(void)
{
    uint8 req[] = {0x3Eu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u};
    PduInfoType pdu = { req, 8u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* TesterPresent should still work with extra trailing bytes */
    TEST_ASSERT_EQUAL_HEX8(0x7Eu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[1]);
}

/** @verifies SWR-BSW-017
 *  Equivalence class: Unknown SID (0x00 — not a valid UDS SID) -> NRC 0x11 */
void test_Dcm_unknown_SID_zero(void)
{
    uint8 req[] = {0x00u, 0x00u};
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* NRC 0x11: serviceNotSupported */
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x11u, mock_tx_data[2]);
}

/** @verifies SWR-BSW-017
 *  Session transition: DEFAULT -> EXTENDED -> DEFAULT round trip */
void test_Dcm_Session_default_extended_default(void)
{
    /* Start in DEFAULT */
    TEST_ASSERT_EQUAL(DCM_DEFAULT_SESSION, Dcm_GetCurrentSession());

    /* Switch to EXTENDED */
    uint8 req_ext[] = {0x10u, 0x03u};
    PduInfoType pdu_ext = { req_ext, 2u };
    Dcm_RxIndication(0u, &pdu_ext);
    Dcm_MainFunction();
    TEST_ASSERT_EQUAL(DCM_EXTENDED_SESSION, Dcm_GetCurrentSession());

    mock_tx_count = 0u;

    /* Switch back to DEFAULT */
    uint8 req_def[] = {0x10u, 0x01u};
    PduInfoType pdu_def = { req_def, 2u };
    Dcm_RxIndication(0u, &pdu_def);
    Dcm_MainFunction();
    TEST_ASSERT_EQUAL(DCM_DEFAULT_SESSION, Dcm_GetCurrentSession());

    /* Verify positive response */
    TEST_ASSERT_EQUAL_HEX8(0x50u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[1]);
}

/** @verifies SWR-BSW-017
 *  Session transition: Invalid sub-function for SessionControl
 *  Sub-function 0xFF is not a valid session type */
void test_Dcm_SessionControl_invalid_sub(void)
{
    uint8 req[] = {0x10u, 0xFFu};
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Should get NRC (sub-function not supported or request out of range) */
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x10u, mock_tx_data[1]);
}

/** @verifies SWR-BSW-017
 *  TesterPresent keepalive resets S3 timer —
 *  extended session should NOT timeout if TesterPresent is sent periodically */
void test_Dcm_TesterPresent_resets_S3(void)
{
    /* Enter extended session */
    uint8 req_ext[] = {0x10u, 0x03u};
    PduInfoType pdu_ext = { req_ext, 2u };
    Dcm_RxIndication(0u, &pdu_ext);
    Dcm_MainFunction();
    mock_tx_count = 0u;

    /* Run half the S3 timeout (250 cycles * 10ms = 2500ms < 5000ms) */
    uint16 i;
    for (i = 0u; i < 250u; i++) {
        Dcm_MainFunction();
    }

    /* Send TesterPresent to reset S3 timer */
    uint8 req_tp[] = {0x3Eu, 0x00u};
    PduInfoType pdu_tp = { req_tp, 2u };
    Dcm_RxIndication(0u, &pdu_tp);
    Dcm_MainFunction();

    /* Run another 250 cycles — total 500 but S3 was reset at 250 */
    for (i = 0u; i < 250u; i++) {
        Dcm_MainFunction();
    }

    /* Session should still be EXTENDED because TesterPresent reset the timer */
    TEST_ASSERT_EQUAL(DCM_EXTENDED_SESSION, Dcm_GetCurrentSession());
}

/** @verifies SWR-BSW-017
 *  DID read: verify second DID (0xF195) data content fully */
void test_Dcm_ReadDID_SwVersion_full_data(void)
{
    uint8 req[] = {0x22u, 0xF1u, 0x95u};
    PduInfoType pdu = { req, 3u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Positive response: 0x62, F1, 95, data[0]=0x01, data[1]=0x00 */
    TEST_ASSERT_EQUAL_HEX8(0x62u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF1u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x95u, mock_tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[4]);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_tx_count);
}

/** @verifies SWR-BSW-017
 *  Fault injection: consecutive rapid requests — second overwrites first */
void test_Dcm_consecutive_requests(void)
{
    /* Send first request (ReadDID) */
    uint8 req1[] = {0x22u, 0xF1u, 0x90u};
    PduInfoType pdu1 = { req1, 3u };
    Dcm_RxIndication(0u, &pdu1);

    /* Send second request (TesterPresent) before MainFunction runs */
    uint8 req2[] = {0x3Eu, 0x00u};
    PduInfoType pdu2 = { req2, 2u };
    Dcm_RxIndication(0u, &pdu2);

    Dcm_MainFunction();

    /* Implementation-specific: either last request wins or first is processed.
     * No crash is the minimum requirement. */
    TEST_ASSERT_TRUE(mock_tx_count >= 1u);
}

/* ==================================================================
 * SWR-BSW-017: ECUReset (SID 0x11)
 * ================================================================== */

/** @verifies SWR-BSW-017 — ECU hard reset */
void test_Dcm_EcuReset_hard(void)
{
    uint8 req[] = {0x11u, 0x01u};  /* SID 0x11, sub=hardReset */
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Positive response: 0x51, 0x01 */
    TEST_ASSERT_EQUAL_HEX8(0x51u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[1]);

    /* BswM_RequestMode should have been called */
    TEST_ASSERT_TRUE(mock_bswm_called);
}

/** @verifies SWR-BSW-017 — ECU soft reset */
void test_Dcm_EcuReset_soft(void)
{
    uint8 req[] = {0x11u, 0x03u};  /* SID 0x11, sub=softReset */
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x51u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03u, mock_tx_data[1]);
    TEST_ASSERT_TRUE(mock_bswm_called);
}

/** @verifies SWR-BSW-017 — ECUReset invalid sub-function */
void test_Dcm_EcuReset_invalid_sub(void)
{
    uint8 req[] = {0x11u, 0xFFu};
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* NRC 0x12: subFunctionNotSupported */
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x12u, mock_tx_data[2]);
    TEST_ASSERT_FALSE(mock_bswm_called);
}

/* ==================================================================
 * SWR-BSW-017: SecurityAccess (SID 0x27)
 * ================================================================== */

/** @verifies SWR-BSW-017 — SecurityAccess requires extended session */
void test_Dcm_SecurityAccess_default_session_rejected(void)
{
    uint8 req[] = {0x27u, 0x01u};  /* Request seed */
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* NRC: serviceNotSupported (not in extended session) */
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x27u, mock_tx_data[1]);
}

/** @verifies SWR-BSW-017 — SecurityAccess seed request */
void test_Dcm_SecurityAccess_request_seed(void)
{
    /* First enter extended session */
    uint8 sess[] = {0x10u, 0x03u};
    PduInfoType sess_pdu = { sess, 2u };
    Dcm_RxIndication(0u, &sess_pdu);
    Dcm_MainFunction();
    mock_tx_count = 0u;

    /* Request seed */
    uint8 req[] = {0x27u, 0x01u};
    PduInfoType pdu = { req, 2u };
    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Positive response: 0x67, 0x01, seed[4] */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x67u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL(6u, mock_tx_dlc);  /* 2 + 4 seed bytes */
}

/** @verifies SWR-BSW-017 — SecurityAccess full unlock flow */
void test_Dcm_SecurityAccess_unlock(void)
{
    /* Enter extended session */
    uint8 sess[] = {0x10u, 0x03u};
    PduInfoType sess_pdu = { sess, 2u };
    Dcm_RxIndication(0u, &sess_pdu);
    Dcm_MainFunction();
    mock_tx_count = 0u;

    /* Request seed */
    uint8 seed_req[] = {0x27u, 0x01u};
    PduInfoType seed_pdu = { seed_req, 2u };
    Dcm_RxIndication(0u, &seed_pdu);
    Dcm_MainFunction();

    /* Extract seed from response */
    uint8 seed[4];
    seed[0] = mock_tx_data[2];
    seed[1] = mock_tx_data[3];
    seed[2] = mock_tx_data[4];
    seed[3] = mock_tx_data[5];

    /* Compute key: seed XOR secret (0x5A, 0xA5, 0x3C, 0xC3) */
    uint8 key_req[6];
    key_req[0] = 0x27u;
    key_req[1] = 0x02u;
    key_req[2] = seed[0] ^ 0x5Au;
    key_req[3] = seed[1] ^ 0xA5u;
    key_req[4] = seed[2] ^ 0x3Cu;
    key_req[5] = seed[3] ^ 0xC3u;

    PduInfoType key_pdu = { key_req, 6u };
    mock_tx_count = 0u;
    Dcm_RxIndication(0u, &key_pdu);
    Dcm_MainFunction();

    /* Positive response: 0x67, 0x02 */
    TEST_ASSERT_EQUAL_HEX8(0x67u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02u, mock_tx_data[1]);
    TEST_ASSERT_TRUE(Dcm_IsSecurityUnlocked());
}

/** @verifies SWR-BSW-017 — SecurityAccess invalid key */
void test_Dcm_SecurityAccess_invalid_key(void)
{
    /* Enter extended session */
    uint8 sess[] = {0x10u, 0x03u};
    PduInfoType sess_pdu = { sess, 2u };
    Dcm_RxIndication(0u, &sess_pdu);
    Dcm_MainFunction();

    /* Request seed */
    uint8 seed_req[] = {0x27u, 0x01u};
    PduInfoType seed_pdu = { seed_req, 2u };
    Dcm_RxIndication(0u, &seed_pdu);
    Dcm_MainFunction();

    /* Send wrong key */
    uint8 key_req[] = {0x27u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u};
    PduInfoType key_pdu = { key_req, 6u };
    mock_tx_count = 0u;
    Dcm_RxIndication(0u, &key_pdu);
    Dcm_MainFunction();

    /* NRC 0x35: invalidKey */
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x27u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x35u, mock_tx_data[2]);
    TEST_ASSERT_FALSE(Dcm_IsSecurityUnlocked());
}

/** @verifies SWR-BSW-017 — SecurityAccess already unlocked returns zero seed */
void test_Dcm_SecurityAccess_already_unlocked(void)
{
    /* Unlock first */
    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;

    /* Request seed again — should get zero seed */
    uint8 req[] = {0x27u, 0x01u};
    PduInfoType pdu = { req, 2u };
    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x67u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[1]);
    /* Seed should be all zeros */
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[5]);
}

/** @verifies SWR-BSW-017 — SecurityAccess lock on session change */
void test_Dcm_SecurityAccess_lock_on_default_session(void)
{
    /* Unlock first */
    test_Dcm_SecurityAccess_unlock();
    TEST_ASSERT_TRUE(Dcm_IsSecurityUnlocked());

    /* Switch to default session */
    uint8 req[] = {0x10u, 0x01u};
    PduInfoType pdu = { req, 2u };
    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    /* Security should be locked */
    TEST_ASSERT_FALSE(Dcm_IsSecurityUnlocked());
}

/* ==================================================================
 * SWR-BSW-017: TpRxIndication (CanTp integration)
 * ================================================================== */

/** @verifies SWR-BSW-017 — TpRxIndication delegates to RxIndication */
void test_Dcm_TpRxIndication_success(void)
{
    uint8 req[] = {0x3Eu, 0x00u};
    PduInfoType pdu = { req, 2u };

    Dcm_TpRxIndication(0u, &pdu, NTFRSLT_OK);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Eu, mock_tx_data[0]);
}

/** @verifies SWR-BSW-017 — TpRxIndication error discards request */
void test_Dcm_TpRxIndication_error_discards(void)
{
    uint8 req[] = {0x3Eu, 0x00u};
    PduInfoType pdu = { req, 2u };

    Dcm_TpRxIndication(0u, &pdu, NTFRSLT_E_TIMEOUT);
    Dcm_MainFunction();

    /* Error result — should not process */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_tx_count);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

/* ==================================================================
 * SWR-BSW-017: ReadDTCInformation (SID 0x19)
 * ================================================================== */

void test_Dcm_ReadDtcInfo_count_by_status_mask(void)
{
    uint8 req[] = {0x19u, 0x01u, 0x08u};
    PduInfoType pdu = { req, 3u };

    mock_dem_filtered_count = 2u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x59u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x0Du, mock_tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x02u, mock_tx_data[5]);
    TEST_ASSERT_EQUAL_HEX8(0x08u, mock_dem_filter_mask);
    TEST_ASSERT_FALSE(mock_dem_report_supported_only);
}

void test_Dcm_ReadDtcInfo_list_by_status_mask_empty(void)
{
    uint8 req[] = {0x19u, 0x02u, 0x04u};
    PduInfoType pdu = { req, 3u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x59u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x0Du, mock_tx_data[2]);
    TEST_ASSERT_EQUAL_UINT32(3u, mock_tx_dlc);
}

void test_Dcm_ReadDtcInfo_list_by_status_mask_records(void)
{
    uint8 req[] = {0x19u, 0x02u, 0x08u};
    PduInfoType pdu = { req, 3u };

    mock_dem_filtered_entry_count = 2u;
    mock_dem_filtered_dtcs[0] = 0xC00100u;
    mock_dem_filtered_status[0] = 0x08u;
    mock_dem_filtered_dtcs[1] = 0xC10100u;
    mock_dem_filtered_status[1] = 0x09u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_tx_count);
    TEST_ASSERT_EQUAL_HEX8(0x59u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x0Du, mock_tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0xC0u, mock_tx_data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[5]);
    TEST_ASSERT_EQUAL_HEX8(0x08u, mock_tx_data[6]);
    TEST_ASSERT_EQUAL_HEX8(0xC1u, mock_tx_data[7]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[8]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[9]);
    TEST_ASSERT_EQUAL_HEX8(0x09u, mock_tx_data[10]);
}

void test_Dcm_ReadDtcInfo_zero_mask_passthrough(void)
{
    uint8 req[] = {0x19u, 0x02u, 0x00u};
    PduInfoType pdu = { req, 3u };

    mock_dem_filtered_entry_count = 1u;
    mock_dem_filtered_dtcs[0] = 0xC20100u;
    mock_dem_filtered_status[0] = 0x01u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_dem_filter_mask);
    TEST_ASSERT_FALSE(mock_dem_report_supported_only);
    TEST_ASSERT_EQUAL_HEX8(0xC2u, mock_tx_data[3]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[5]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[6]);
}

void test_Dcm_ReadDtcInfo_report_supported_dtcs(void)
{
    uint8 req[] = {0x19u, 0x0Au};
    PduInfoType pdu = { req, 2u };

    mock_dem_filtered_entry_count = 2u;
    mock_dem_filtered_dtcs[0] = 0xC00100u;
    mock_dem_filtered_status[0] = 0x00u;
    mock_dem_filtered_dtcs[1] = 0xC60100u;
    mock_dem_filtered_status[1] = 0x08u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_TRUE(mock_dem_report_supported_only);
    TEST_ASSERT_EQUAL_HEX8(0x59u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x0Au, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x0Du, mock_tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, mock_tx_data[6]);
    TEST_ASSERT_EQUAL_HEX8(0x08u, mock_tx_data[10]);
}

void test_Dcm_ReadDtcInfo_invalid_subfunction(void)
{
    uint8 req[] = {0x19u, 0x03u};
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x19u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x12u, mock_tx_data[2]);
}

void test_Dcm_ReadDtcInfo_wrong_length(void)
{
    uint8 req[] = {0x19u, 0x01u};
    PduInfoType pdu = { req, 2u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x19u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x13u, mock_tx_data[2]);
}

void test_Dcm_ReadDtcInfo_overflow_returns_nrc(void)
{
    uint8 req[] = {0x19u, 0x0Au};
    PduInfoType pdu = { req, 2u };
    uint8 i;

    mock_dem_filtered_entry_count = 32u;
    for (i = 0u; i < 32u; i++) {
        mock_dem_filtered_dtcs[i] = 0xC00000u + i;
        mock_dem_filtered_status[i] = 0x08u;
    }

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x19u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x31u, mock_tx_data[2]);
}

/* ==================================================================
 * SWR-BSW-017: ClearDiagnosticInformation (SID 0x14)
 * ================================================================== */

void test_Dcm_ClearDtc_wrong_length(void)
{
    uint8 req[] = {0x14u, 0xFFu, 0xFFu};
    PduInfoType pdu = { req, 3u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x14u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x13u, mock_tx_data[2]);
}

void test_Dcm_ClearDtc_default_session_rejected(void)
{
    uint8 req[] = {0x14u, 0xFFu, 0xFFu, 0xFFu};
    PduInfoType pdu = { req, 4u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x14u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, mock_tx_data[2]);
}

void test_Dcm_ClearDtc_security_locked_rejected(void)
{
    uint8 sess[] = {0x10u, 0x03u};
    uint8 req[] = {0x14u, 0xFFu, 0xFFu, 0xFFu};
    PduInfoType sess_pdu = { sess, 2u };
    PduInfoType pdu = { req, 4u };

    Dcm_RxIndication(0u, &sess_pdu);
    Dcm_MainFunction();
    mock_tx_count = 0u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x14u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x33u, mock_tx_data[2]);
}

void test_Dcm_ClearDtc_all_dtcs(void)
{
    uint8 req[] = {0x14u, 0xFFu, 0xFFu, 0xFFu};
    PduInfoType pdu = { req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFu, mock_dem_clear_selector);
    TEST_ASSERT_EQUAL_HEX8(0x54u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_tx_dlc);
}

void test_Dcm_ClearDtc_exact_selector(void)
{
    uint8 req[] = {0x14u, 0xC0u, 0x01u, 0x00u};
    PduInfoType pdu = { req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_UINT32(0xC00100u, mock_dem_clear_selector);
    TEST_ASSERT_EQUAL_HEX8(0x54u, mock_tx_data[0]);
}

void test_Dcm_ClearDtc_invalid_selector(void)
{
    uint8 req[] = {0x14u, 0xDEu, 0xADu, 0xBEu};
    PduInfoType pdu = { req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;
    mock_dem_clear_result = DEM_CLEAR_DTC_INVALID_SELECTOR;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x14u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x31u, mock_tx_data[2]);
}

void test_Dcm_ClearDtc_nvm_failure(void)
{
    uint8 req[] = {0x14u, 0xFFu, 0xFFu, 0xFFu};
    PduInfoType pdu = { req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;
    mock_dem_clear_result = DEM_CLEAR_DTC_NVM_FAILED;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x14u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x72u, mock_tx_data[2]);
}

/* ==================================================================
 * SWR-BSW-017: RoutineControl (SID 0x31)
 * ================================================================== */

void test_Dcm_RoutineControl_start_running(void)
{
    uint8 req[] = {0x31u, 0x01u, 0x02u, 0x01u, 0x12u};
    PduInfoType pdu = { req, 5u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_routine_start_call_count);
    TEST_ASSERT_EQUAL_HEX8(0x71u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02u, mock_tx_data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, mock_tx_data[3]);
    TEST_ASSERT_EQUAL_HEX8(DCM_ROUTINE_STATE_RUNNING, mock_tx_data[4]);
    TEST_ASSERT_EQUAL_HEX8(0xA5u, mock_tx_data[5]);
}

void test_Dcm_RoutineControl_results_idle_before_start(void)
{
    uint8 req[] = {0x31u, 0x03u, 0x02u, 0x01u};
    PduInfoType pdu = { req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;
    mock_routine_results_next_state = DCM_ROUTINE_STATE_IDLE;
    mock_routine_results_payload_len = 0u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x71u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(DCM_ROUTINE_STATE_IDLE, mock_tx_data[4]);
}

void test_Dcm_RoutineControl_results_transition_to_completed(void)
{
    uint8 start_req[] = {0x31u, 0x01u, 0x02u, 0x01u};
    uint8 results_req[] = {0x31u, 0x03u, 0x02u, 0x01u};
    PduInfoType start_pdu = { start_req, 4u };
    PduInfoType results_pdu = { results_req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;

    Dcm_RxIndication(0u, &start_pdu);
    Dcm_MainFunction();

    mock_tx_count = 0u;
    mock_routine_results_next_state = DCM_ROUTINE_STATE_COMPLETED;
    mock_routine_results_payload[0] = 0xBEu;
    mock_routine_results_payload[1] = 0xEFu;
    mock_routine_results_payload_len = 2u;

    Dcm_RxIndication(0u, &results_pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_routine_results_call_count);
    TEST_ASSERT_EQUAL_HEX8(0x71u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x03u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(DCM_ROUTINE_STATE_COMPLETED, mock_tx_data[4]);
    TEST_ASSERT_EQUAL_HEX8(0xBEu, mock_tx_data[5]);
    TEST_ASSERT_EQUAL_HEX8(0xEFu, mock_tx_data[6]);
}

void test_Dcm_RoutineControl_stop_idempotent(void)
{
    uint8 req[] = {0x31u, 0x02u, 0x02u, 0x01u};
    PduInfoType pdu = { req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_routine_stop_call_count);
    TEST_ASSERT_EQUAL_HEX8(0x71u, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(DCM_ROUTINE_STATE_STOPPED, mock_tx_data[4]);
}

void test_Dcm_RoutineControl_invalid_routine_id(void)
{
    uint8 req[] = {0x31u, 0x01u, 0x12u, 0x34u};
    PduInfoType pdu = { req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x31u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x31u, mock_tx_data[2]);
}

void test_Dcm_RoutineControl_start_duplicate_rejected(void)
{
    uint8 req[] = {0x31u, 0x01u, 0x02u, 0x01u};
    PduInfoType pdu = { req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    mock_tx_count = 0u;
    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x31u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x24u, mock_tx_data[2]);
}

void test_Dcm_RoutineControl_precondition_failure(void)
{
    uint8 req[] = {0x31u, 0x01u, 0x02u, 0x01u};
    PduInfoType pdu = { req, 4u };

    test_Dcm_SecurityAccess_unlock();
    mock_tx_count = 0u;
    mock_routine_precondition_ok = FALSE;
    mock_routine_precondition_nrc = DCM_NRC_CONDITIONS_NOT_CORRECT;

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_routine_start_call_count);
    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x31u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, mock_tx_data[2]);
}

void test_Dcm_RoutineControl_default_session_rejected(void)
{
    uint8 req[] = {0x31u, 0x01u, 0x02u, 0x01u};
    PduInfoType pdu = { req, 4u };

    Dcm_RxIndication(0u, &pdu);
    Dcm_MainFunction();

    TEST_ASSERT_EQUAL_HEX8(0x7Fu, mock_tx_data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x31u, mock_tx_data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x22u, mock_tx_data[2]);
}

int main(void)
{
    UNITY_BEGIN();

    /* Session control */
    RUN_TEST(test_Dcm_SessionControl_extended);
    RUN_TEST(test_Dcm_SessionControl_default);
    RUN_TEST(test_Dcm_SessionControl_wrong_length);

    /* ReadDataByIdentifier */
    RUN_TEST(test_Dcm_ReadDID_EcuId);
    RUN_TEST(test_Dcm_ReadDID_SwVersion);
    RUN_TEST(test_Dcm_ReadDID_unsupported);
    RUN_TEST(test_Dcm_ReadDID_wrong_length);

    /* TesterPresent */
    RUN_TEST(test_Dcm_TesterPresent);
    RUN_TEST(test_Dcm_TesterPresent_suppress);

    /* NRC / error handling */
    RUN_TEST(test_Dcm_unknown_SID);
    RUN_TEST(test_Dcm_RxIndication_null_pdu);
    RUN_TEST(test_Dcm_Init_null_config);
    RUN_TEST(test_Dcm_S3_timeout_resets_session);
    RUN_TEST(test_Dcm_empty_request);

    /* Hardened boundary / fault injection tests */
    RUN_TEST(test_Dcm_RxIndication_null_data_ptr);
    RUN_TEST(test_Dcm_SessionControl_sid_only);
    RUN_TEST(test_Dcm_MaxLength_request);
    RUN_TEST(test_Dcm_unknown_SID_zero);
    RUN_TEST(test_Dcm_Session_default_extended_default);
    RUN_TEST(test_Dcm_SessionControl_invalid_sub);
    RUN_TEST(test_Dcm_TesterPresent_resets_S3);
    RUN_TEST(test_Dcm_ReadDID_SwVersion_full_data);
    RUN_TEST(test_Dcm_consecutive_requests);

    /* ECUReset */
    RUN_TEST(test_Dcm_EcuReset_hard);
    RUN_TEST(test_Dcm_EcuReset_soft);
    RUN_TEST(test_Dcm_EcuReset_invalid_sub);

    /* SecurityAccess */
    RUN_TEST(test_Dcm_SecurityAccess_default_session_rejected);
    RUN_TEST(test_Dcm_SecurityAccess_request_seed);
    RUN_TEST(test_Dcm_SecurityAccess_unlock);
    RUN_TEST(test_Dcm_SecurityAccess_invalid_key);
    RUN_TEST(test_Dcm_SecurityAccess_already_unlocked);
    RUN_TEST(test_Dcm_SecurityAccess_lock_on_default_session);

    /* ReadDTCInformation */
    RUN_TEST(test_Dcm_ReadDtcInfo_count_by_status_mask);
    RUN_TEST(test_Dcm_ReadDtcInfo_list_by_status_mask_empty);
    RUN_TEST(test_Dcm_ReadDtcInfo_list_by_status_mask_records);
    RUN_TEST(test_Dcm_ReadDtcInfo_zero_mask_passthrough);
    RUN_TEST(test_Dcm_ReadDtcInfo_report_supported_dtcs);
    RUN_TEST(test_Dcm_ReadDtcInfo_invalid_subfunction);
    RUN_TEST(test_Dcm_ReadDtcInfo_wrong_length);
    RUN_TEST(test_Dcm_ReadDtcInfo_overflow_returns_nrc);

    /* ClearDiagnosticInformation */
    RUN_TEST(test_Dcm_ClearDtc_wrong_length);
    RUN_TEST(test_Dcm_ClearDtc_default_session_rejected);
    RUN_TEST(test_Dcm_ClearDtc_security_locked_rejected);
    RUN_TEST(test_Dcm_ClearDtc_all_dtcs);
    RUN_TEST(test_Dcm_ClearDtc_exact_selector);
    RUN_TEST(test_Dcm_ClearDtc_invalid_selector);
    RUN_TEST(test_Dcm_ClearDtc_nvm_failure);

    /* RoutineControl */
    RUN_TEST(test_Dcm_RoutineControl_start_running);
    RUN_TEST(test_Dcm_RoutineControl_results_idle_before_start);
    RUN_TEST(test_Dcm_RoutineControl_results_transition_to_completed);
    RUN_TEST(test_Dcm_RoutineControl_stop_idempotent);
    RUN_TEST(test_Dcm_RoutineControl_invalid_routine_id);
    RUN_TEST(test_Dcm_RoutineControl_start_duplicate_rejected);
    RUN_TEST(test_Dcm_RoutineControl_precondition_failure);
    RUN_TEST(test_Dcm_RoutineControl_default_session_rejected);

    /* TpRxIndication (CanTp integration) */
    RUN_TEST(test_Dcm_TpRxIndication_success);
    RUN_TEST(test_Dcm_TpRxIndication_error_discards);

    return UNITY_END();
}
