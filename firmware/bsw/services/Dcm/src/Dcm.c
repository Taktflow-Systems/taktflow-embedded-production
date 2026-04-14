/**
 * @file    Dcm.c
 * @brief   Diagnostic Communication Manager implementation
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-017
 * @traces_to  TSR-038, TSR-039, TSR-040
 *
 * Implements UDS diagnostic services for physical ECUs:
 * - 0x10 DiagnosticSessionControl (Default + Extended sessions)
 * - 0x11 ECUReset (hard + soft reset via BswM)
 * - 0x22 ReadDataByIdentifier (configurable DID table)
 * - 0x27 SecurityAccess (seed-key challenge, XOR placeholder)
 * - 0x3E TesterPresent (with suppress-positive-response support)
 *
 * Responses >7 bytes are routed through CanTp for multi-frame TX.
 *
 * @standard AUTOSAR_SWS_DiagnosticCommunicationManager, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Dcm.h"
#include "Dcm_ClearDtc.h"
#include "Dcm_ReadDtcInfo.h"
#include "Dcm_RoutineControl.h"
#include "Det.h"

#include <string.h>  /* memcpy */

/* ---- Internal State ---- */

static const Dcm_ConfigType*  dcm_config = NULL_PTR;
static boolean                dcm_initialized = FALSE;

/* Request buffer */
static uint8          dcm_rx_buf[DCM_TX_BUF_SIZE];
static PduLengthType  dcm_rx_len;
static boolean        dcm_request_pending;

/* Response buffer */
static uint8   dcm_tx_buf[DCM_TX_BUF_SIZE];

/* Session state */
static Dcm_SessionType dcm_current_session;
static uint16          dcm_s3_timer_ms;

/* SecurityAccess state */
static boolean dcm_security_unlocked;
static uint8   dcm_security_seed[DCM_SECURITY_SEED_LEN];
static boolean dcm_seed_active;
static uint8   dcm_security_fail_count;

/* RoutineControl runtime state */
static Dcm_RoutineStateType dcm_routine_states[DCM_MAX_ROUTINES];

/* SecurityAccess XOR secret (placeholder — real crypto for STM32 later) */
static const uint8 dcm_security_secret[DCM_SECURITY_SEED_LEN] = {
    0x5Au, 0xA5u, 0x3Cu, 0xC3u
};

/* Pseudo-random seed state (simple LCG for SIL — NOT cryptographically secure) */
static uint32 dcm_prng_state = 0x12345678u;

/* ---- Forward Declarations ---- */

static void dcm_send_response(const uint8* data, PduLengthType length);
static void dcm_send_nrc(uint8 sid, uint8 nrc);
static void dcm_reset_s3_timer(void);
static void dcm_process_request(const uint8* data, PduLengthType length);
static void dcm_handle_session_control(const uint8* data, PduLengthType length);
static void dcm_handle_ecu_reset(const uint8* data, PduLengthType length);
static void dcm_handle_clear_dtc(const uint8* data, PduLengthType length);
static void dcm_handle_read_dtc_info(const uint8* data, PduLengthType length);
static void dcm_handle_read_did(const uint8* data, PduLengthType length);
static void dcm_handle_security_access(const uint8* data, PduLengthType length);
static void dcm_handle_routine_control(const uint8* data, PduLengthType length);
static void dcm_handle_tester_present(const uint8* data, PduLengthType length);

/* ---- Private Helpers ---- */

static uint8 dcm_prng_next(void)
{
    dcm_prng_state = dcm_prng_state * 1103515245u + 12345u;
    return (uint8)((dcm_prng_state >> 16) & 0xFFu);
}

/**
 * @brief  Send a UDS response — routes through CanTp for >7 bytes
 */
static void dcm_send_response(const uint8* data, PduLengthType length)
{
    PduInfoType pdu_info;

    if ((data == NULL_PTR) || (length == 0u)) {
        return;
    }

    pdu_info.SduDataPtr = (uint8*)data;  /* cast away const for AUTOSAR PduInfoType */
    pdu_info.SduLength  = length;

    if (length > 7u) {
        /* Multi-frame: route through CanTp for ISO-TP segmentation */
        (void)CanTp_Transmit(dcm_config->TxPduId, &pdu_info);
    } else {
        /* Single-frame: direct PduR path */
        (void)PduR_DcmTransmit(dcm_config->TxPduId, &pdu_info);
    }
}

static void dcm_send_nrc(uint8 sid, uint8 nrc)
{
    dcm_tx_buf[0] = DCM_NEGATIVE_RESPONSE_SID;
    dcm_tx_buf[1] = sid;
    dcm_tx_buf[2] = nrc;

    dcm_send_response(dcm_tx_buf, 3u);
}

static void dcm_reset_s3_timer(void)
{
    dcm_s3_timer_ms = 0u;
}

/* ---- UDS Service Handlers ---- */

static void dcm_handle_session_control(const uint8* data, PduLengthType length)
{
    uint8 sub_function;

    if (length < 2u) {
        dcm_send_nrc(DCM_SID_SESSION_CTRL, DCM_NRC_INCORRECT_MSG_LENGTH);
        return;
    }

    sub_function = data[1];

    switch (sub_function) {
    case (uint8)DCM_DEFAULT_SESSION:
        dcm_current_session = DCM_DEFAULT_SESSION;
        /* Lock security on session change to default */
        dcm_security_unlocked = FALSE;
        dcm_seed_active = FALSE;
        break;

    case (uint8)DCM_EXTENDED_SESSION:
        dcm_current_session = DCM_EXTENDED_SESSION;
        dcm_reset_s3_timer();
        break;

    default:
        dcm_send_nrc(DCM_SID_SESSION_CTRL, DCM_NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    dcm_tx_buf[0] = DCM_SID_SESSION_CTRL + DCM_POSITIVE_RESPONSE_OFFSET;
    dcm_tx_buf[1] = sub_function;
    dcm_send_response(dcm_tx_buf, 2u);
}

static void dcm_handle_ecu_reset(const uint8* data, PduLengthType length)
{
    uint8 sub_function;

    if (length < 2u) {
        dcm_send_nrc(DCM_SID_ECU_RESET, DCM_NRC_INCORRECT_MSG_LENGTH);
        return;
    }

    sub_function = data[1];

    switch (sub_function) {
    case DCM_RESET_HARD:
    case DCM_RESET_SOFT:
        /* Send positive response first */
        dcm_tx_buf[0] = DCM_SID_ECU_RESET + DCM_POSITIVE_RESPONSE_OFFSET;
        dcm_tx_buf[1] = sub_function;
        dcm_send_response(dcm_tx_buf, 2u);

        /* Request BswM to transition to STARTUP (triggers reset) */
        (void)BswM_RequestMode(0u, 0u);  /* BSWM_STARTUP = 0 */
        break;

    default:
        dcm_send_nrc(DCM_SID_ECU_RESET, DCM_NRC_SUBFUNCTION_NOT_SUPPORTED);
        break;
    }
}

static void dcm_handle_read_did(const uint8* data, PduLengthType length)
{
    uint16 requested_did;
    uint8 i;

    if (length < 3u) {
        dcm_send_nrc(DCM_SID_READ_DID, DCM_NRC_INCORRECT_MSG_LENGTH);
        return;
    }

    requested_did = ((uint16)data[1] << 8u) | (uint16)data[2];

    for (i = 0u; i < dcm_config->DidCount; i++) {
        if (dcm_config->DidTable[i].Did == requested_did) {
            const Dcm_DidTableType* did_entry = &dcm_config->DidTable[i];

            if ((3u + (PduLengthType)did_entry->DataLength) > DCM_TX_BUF_SIZE) {
                dcm_send_nrc(DCM_SID_READ_DID, DCM_NRC_REQUEST_OUT_OF_RANGE);
                return;
            }

            dcm_tx_buf[0] = DCM_SID_READ_DID + DCM_POSITIVE_RESPONSE_OFFSET;
            dcm_tx_buf[1] = (uint8)(requested_did >> 8u);
            dcm_tx_buf[2] = (uint8)(requested_did & 0xFFu);

            if ((did_entry->ReadFunc != NULL_PTR) &&
                (did_entry->ReadFunc(&dcm_tx_buf[3], did_entry->DataLength) == E_OK)) {
                dcm_send_response(dcm_tx_buf,
                                  (PduLengthType)(3u + did_entry->DataLength));
            } else {
                dcm_send_nrc(DCM_SID_READ_DID, DCM_NRC_REQUEST_OUT_OF_RANGE);
            }
            return;
        }
    }

    dcm_send_nrc(DCM_SID_READ_DID, DCM_NRC_REQUEST_OUT_OF_RANGE);
}

static void dcm_handle_read_dtc_info(const uint8* data, PduLengthType length)
{
    PduLengthType response_length = 0u;
    uint8 nrc = DCM_NRC_REQUEST_OUT_OF_RANGE;

    if (Dcm_ReadDtcInfo_Process(data, length, dcm_tx_buf, DCM_TX_BUF_SIZE,
                                &response_length, &nrc) == E_OK) {
        dcm_send_response(dcm_tx_buf, response_length);
    } else {
        dcm_send_nrc(DCM_SID_READ_DTC_INFO, nrc);
    }
}

static void dcm_handle_clear_dtc(const uint8* data, PduLengthType length)
{
    PduLengthType response_length = 0u;
    uint8 nrc = DCM_NRC_REQUEST_OUT_OF_RANGE;

    if (length != 4u) {
        dcm_send_nrc(DCM_SID_CLEAR_DTC, DCM_NRC_INCORRECT_MSG_LENGTH);
        return;
    }

    if (dcm_current_session != DCM_EXTENDED_SESSION) {
        dcm_send_nrc(DCM_SID_CLEAR_DTC, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    if (dcm_security_unlocked == FALSE) {
        dcm_send_nrc(DCM_SID_CLEAR_DTC, DCM_NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    if (Dcm_ClearDtc_Process(data, length, dcm_tx_buf, DCM_TX_BUF_SIZE,
                             &response_length, &nrc) == E_OK) {
        dcm_send_response(dcm_tx_buf, response_length);
    } else {
        dcm_send_nrc(DCM_SID_CLEAR_DTC, nrc);
    }
}

static void dcm_handle_routine_control(const uint8* data, PduLengthType length)
{
    PduLengthType response_length = 0u;
    uint8 nrc = DCM_NRC_REQUEST_OUT_OF_RANGE;

    if (length < 4u) {
        dcm_send_nrc(DCM_SID_ROUTINE_CONTROL, DCM_NRC_INCORRECT_MSG_LENGTH);
        return;
    }

    if (dcm_current_session != DCM_EXTENDED_SESSION) {
        dcm_send_nrc(DCM_SID_ROUTINE_CONTROL, DCM_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    if (dcm_security_unlocked == FALSE) {
        dcm_send_nrc(DCM_SID_ROUTINE_CONTROL, DCM_NRC_SECURITY_ACCESS_DENIED);
        return;
    }

    if (Dcm_RoutineControl_Process(dcm_config->RoutineTable,
                                   dcm_config->RoutineCount,
                                   dcm_routine_states,
                                   data,
                                   length,
                                   dcm_tx_buf,
                                   DCM_TX_BUF_SIZE,
                                   &response_length,
                                   &nrc) == E_OK) {
        dcm_send_response(dcm_tx_buf, response_length);
    } else {
        dcm_send_nrc(DCM_SID_ROUTINE_CONTROL, nrc);
    }
}

static void dcm_handle_security_access(const uint8* data, PduLengthType length)
{
    uint8 sub_function;

    if (length < 2u) {
        dcm_send_nrc(DCM_SID_SECURITY_ACCESS, DCM_NRC_INCORRECT_MSG_LENGTH);
        return;
    }

    /* SecurityAccess requires extended session */
    if (dcm_current_session != DCM_EXTENDED_SESSION) {
        dcm_send_nrc(DCM_SID_SECURITY_ACCESS, DCM_NRC_SERVICE_NOT_SUPPORTED);
        return;
    }

    sub_function = data[1];

    if (sub_function == DCM_SA_REQUEST_SEED) {
        /* Request Seed (0x27 01) */
        if (dcm_security_unlocked == TRUE) {
            /* Already unlocked — return zero seed per ISO 14229 */
            dcm_tx_buf[0] = DCM_SID_SECURITY_ACCESS + DCM_POSITIVE_RESPONSE_OFFSET;
            dcm_tx_buf[1] = DCM_SA_REQUEST_SEED;
            (void)memset(&dcm_tx_buf[2], 0, DCM_SECURITY_SEED_LEN);
            dcm_send_response(dcm_tx_buf,
                              (PduLengthType)(2u + DCM_SECURITY_SEED_LEN));
            return;
        }

        /* Check failed attempt limit */
        if (dcm_security_fail_count >= DCM_SECURITY_MAX_ATTEMPTS) {
            dcm_send_nrc(DCM_SID_SECURITY_ACCESS, DCM_NRC_EXCEEDED_ATTEMPTS);
            return;
        }

        /* Generate seed */
        {
            uint8 s;
            for (s = 0u; s < DCM_SECURITY_SEED_LEN; s++) {
                dcm_security_seed[s] = dcm_prng_next();
            }
        }
        dcm_seed_active = TRUE;

        /* Positive response: 0x67, 0x01, seed[4] */
        dcm_tx_buf[0] = DCM_SID_SECURITY_ACCESS + DCM_POSITIVE_RESPONSE_OFFSET;
        dcm_tx_buf[1] = DCM_SA_REQUEST_SEED;
        (void)memcpy(&dcm_tx_buf[2], dcm_security_seed, DCM_SECURITY_SEED_LEN);
        dcm_send_response(dcm_tx_buf,
                          (PduLengthType)(2u + DCM_SECURITY_SEED_LEN));

    } else if (sub_function == DCM_SA_SEND_KEY) {
        /* Send Key (0x27 02) */
        uint8 expected_key[DCM_SECURITY_SEED_LEN];
        uint8 k;

        if (dcm_seed_active == FALSE) {
            dcm_send_nrc(DCM_SID_SECURITY_ACCESS, DCM_NRC_REQUEST_OUT_OF_RANGE);
            return;
        }

        if (length < (PduLengthType)(2u + DCM_SECURITY_SEED_LEN)) {
            dcm_send_nrc(DCM_SID_SECURITY_ACCESS, DCM_NRC_INCORRECT_MSG_LENGTH);
            return;
        }

        /* Compute expected key: seed XOR secret */
        for (k = 0u; k < DCM_SECURITY_SEED_LEN; k++) {
            expected_key[k] = dcm_security_seed[k] ^ dcm_security_secret[k];
        }

        /* Compare key */
        if (memcmp(&data[2], expected_key, DCM_SECURITY_SEED_LEN) == 0) {
            /* Key valid — unlock */
            dcm_security_unlocked = TRUE;
            dcm_seed_active = FALSE;
            dcm_security_fail_count = 0u;

            dcm_tx_buf[0] = DCM_SID_SECURITY_ACCESS + DCM_POSITIVE_RESPONSE_OFFSET;
            dcm_tx_buf[1] = DCM_SA_SEND_KEY;
            dcm_send_response(dcm_tx_buf, 2u);
        } else {
            /* Key invalid */
            dcm_seed_active = FALSE;
            dcm_security_fail_count++;

            if (dcm_security_fail_count >= DCM_SECURITY_MAX_ATTEMPTS) {
                dcm_send_nrc(DCM_SID_SECURITY_ACCESS, DCM_NRC_EXCEEDED_ATTEMPTS);
            } else {
                dcm_send_nrc(DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);
            }
        }
    } else {
        dcm_send_nrc(DCM_SID_SECURITY_ACCESS, DCM_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
}

static void dcm_handle_tester_present(const uint8* data, PduLengthType length)
{
    uint8 sub_function;
    boolean suppress_response;

    if (length < 2u) {
        dcm_send_nrc(DCM_SID_TESTER_PRESENT, DCM_NRC_INCORRECT_MSG_LENGTH);
        return;
    }

    sub_function = data[1];
    suppress_response = ((sub_function & DCM_SUPPRESS_POS_RSP_BIT) != 0u) ? TRUE : FALSE;

    dcm_reset_s3_timer();

    if (suppress_response == FALSE) {
        dcm_tx_buf[0] = DCM_SID_TESTER_PRESENT + DCM_POSITIVE_RESPONSE_OFFSET;
        dcm_tx_buf[1] = sub_function & (uint8)(~DCM_SUPPRESS_POS_RSP_BIT);
        dcm_send_response(dcm_tx_buf, 2u);
    }
}

static void dcm_process_request(const uint8* data, PduLengthType length)
{
    uint8 sid;

    if ((data == NULL_PTR) || (length == 0u)) {
        return;
    }

    sid = data[0];
    dcm_reset_s3_timer();

    switch (sid) {
    case DCM_SID_SESSION_CTRL:
        dcm_handle_session_control(data, length);
        break;

    case DCM_SID_ECU_RESET:
        dcm_handle_ecu_reset(data, length);
        break;

    case DCM_SID_CLEAR_DTC:
        dcm_handle_clear_dtc(data, length);
        break;

    case DCM_SID_READ_DTC_INFO:
        dcm_handle_read_dtc_info(data, length);
        break;

    case DCM_SID_READ_DID:
        dcm_handle_read_did(data, length);
        break;

    case DCM_SID_SECURITY_ACCESS:
        dcm_handle_security_access(data, length);
        break;

    case DCM_SID_ROUTINE_CONTROL:
        dcm_handle_routine_control(data, length);
        break;

    case DCM_SID_TESTER_PRESENT:
        dcm_handle_tester_present(data, length);
        break;

    default:
        dcm_send_nrc(sid, DCM_NRC_SERVICE_NOT_SUPPORTED);
        break;
    }
}

/* ---- API Implementation ---- */

void Dcm_Init(const Dcm_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_DCM, 0u, DCM_API_INIT, DET_E_PARAM_POINTER);
        dcm_initialized = FALSE;
        dcm_config = NULL_PTR;
        return;
    }

    if ((ConfigPtr->RoutineCount > DCM_MAX_ROUTINES) ||
        ((ConfigPtr->RoutineCount > 0u) && (ConfigPtr->RoutineTable == NULL_PTR))) {
        Det_ReportError(DET_MODULE_DCM, 0u, DCM_API_INIT, DET_E_PARAM_VALUE);
        dcm_initialized = FALSE;
        dcm_config = NULL_PTR;
        return;
    }

    dcm_config = ConfigPtr;

    (void)memset(dcm_rx_buf, 0, sizeof(dcm_rx_buf));
    (void)memset(dcm_tx_buf, 0, sizeof(dcm_tx_buf));
    (void)memset(dcm_routine_states, 0, sizeof(dcm_routine_states));

    dcm_rx_len           = 0u;
    dcm_request_pending  = FALSE;
    dcm_current_session  = DCM_DEFAULT_SESSION;
    dcm_s3_timer_ms      = 0u;

    /* SecurityAccess state */
    dcm_security_unlocked  = FALSE;
    dcm_seed_active        = FALSE;
    dcm_security_fail_count = 0u;
    (void)memset(dcm_security_seed, 0, sizeof(dcm_security_seed));

    dcm_initialized = TRUE;
}

void Dcm_MainFunction(void)
{
    if ((dcm_initialized == FALSE) || (dcm_config == NULL_PTR)) {
        return;
    }

    if (dcm_request_pending == TRUE) {
        dcm_process_request(dcm_rx_buf, dcm_rx_len);
        dcm_request_pending = FALSE;
    }

    if (dcm_current_session != DCM_DEFAULT_SESSION) {
        dcm_s3_timer_ms += DCM_MAIN_CYCLE_MS;

        if (dcm_s3_timer_ms >= dcm_config->S3TimeoutMs) {
            dcm_current_session = DCM_DEFAULT_SESSION;
            dcm_s3_timer_ms = 0u;
            /* Lock security on session timeout */
            dcm_security_unlocked = FALSE;
            dcm_seed_active = FALSE;
        }
    }
}

void Dcm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
    (void)RxPduId;

    if ((dcm_initialized == FALSE) || (dcm_config == NULL_PTR)) {
        Det_ReportError(DET_MODULE_DCM, 0u, DCM_API_RX_INDICATION, DET_E_UNINIT);
        return;
    }

    if ((PduInfoPtr == NULL_PTR) || (PduInfoPtr->SduDataPtr == NULL_PTR)) {
        Det_ReportError(DET_MODULE_DCM, 0u, DCM_API_RX_INDICATION, DET_E_PARAM_POINTER);
        return;
    }

    if ((PduInfoPtr->SduLength == 0u) || (PduInfoPtr->SduLength > DCM_TX_BUF_SIZE)) {
        Det_ReportError(DET_MODULE_DCM, 0u, DCM_API_RX_INDICATION, DET_E_PARAM_VALUE);
        return;
    }

    (void)memcpy(dcm_rx_buf, PduInfoPtr->SduDataPtr, PduInfoPtr->SduLength);
    dcm_rx_len = PduInfoPtr->SduLength;
    dcm_request_pending = TRUE;
}

void Dcm_TpRxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr,
                          NotifResultType Result)
{
    if (Result != NTFRSLT_OK) {
        return;  /* TP error — discard */
    }

    /* Delegate to standard RX path */
    Dcm_RxIndication(RxPduId, PduInfoPtr);
}

Dcm_SessionType Dcm_GetCurrentSession(void)
{
    return dcm_current_session;
}

boolean Dcm_IsSecurityUnlocked(void)
{
    return dcm_security_unlocked;
}
