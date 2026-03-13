/**
 * @file    Swc_FzcDcm.c
 * @brief   FZC UDS diagnostic service handler
 * @date    2026-02-24
 *
 * @safety_req SWR-FZC-030
 * @traces_to  SSR-FZC-030
 *
 * @details  UDS service support for the FZC:
 *           - Physical request on CAN ID 0x7E1
 *           - Functional request on CAN ID 0x7DF
 *           - Response on CAN ID 0x7E9
 *           - Services: 0x10, 0x11, 0x14, 0x19, 0x22, 0x28, 0x3E, 0x85
 *           - DIDs: 0xF190 VIN, 0xF191 HW ver, 0xF195 SW ver,
 *             0xF020 steering, 0xF021 brake, 0xF022-F024 lidar
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR DCM pattern, UDS ISO 14229, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_FzcDcm.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** UDS positive response offset */
#define UDS_POSITIVE_RESP_SID_OFFSET   0x40u

/** UDS negative response SID */
#define UDS_NEGATIVE_RESP_SID          0x7Fu

/** Supported service ID table */
static const uint8 FzcDcm_SupportedSids[FZC_UDS_SUPPORTED_COUNT] = {
    FZC_UDS_SID_DIAG_SESSION,
    FZC_UDS_SID_ECU_RESET,
    FZC_UDS_SID_CLEAR_DTC,
    FZC_UDS_SID_READ_DTC,
    FZC_UDS_SID_READ_DID,
    FZC_UDS_SID_COMM_CTRL,
    FZC_UDS_SID_TESTER_PRESENT,
    FZC_UDS_SID_CTRL_DTC_SETTING
};

/* Static version strings */
static const uint8 FzcDcm_Vin[17]    = "TAKTFLOW0FZC02026";
static const uint8 FzcDcm_HwVer[4]   = "1.0";
static const uint8 FzcDcm_SwVer[8]   = "0.9.0";

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8 FzcDcm_Initialized;

/* ==================================================================
 * API: Swc_FzcDcm_Init
 * ================================================================== */

void Swc_FzcDcm_Init(void)
{
    FzcDcm_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_FzcDcm_IsServiceSupported
 * ================================================================== */

uint8 Swc_FzcDcm_IsServiceSupported(uint8 sid)
{
    uint8 i;

    for (i = 0u; i < FZC_UDS_SUPPORTED_COUNT; i++) {
        if (FzcDcm_SupportedSids[i] == sid) {
            return TRUE;
        }
    }

    return FALSE;
}

/* ==================================================================
 * Private: Send negative response
 * ================================================================== */

static void FzcDcm_NegativeResponse(uint8 sid, uint8 nrc,
                                     uint8* respData, uint8* respLen)
{
    respData[0] = UDS_NEGATIVE_RESP_SID;
    respData[1] = sid;
    respData[2] = nrc;
    *respLen = 3u;
}

/* ==================================================================
 * Private: Handle ReadDataByIdentifier (0x22)
 * ================================================================== */

static void FzcDcm_HandleReadDid(const uint8* reqData, uint8 reqLen,
                                  uint8* respData, uint8* respLen)
{
    uint16 did;
    uint32 rteVal;
    uint8  pos;

    if (reqLen < 3u) {
        FzcDcm_NegativeResponse(FZC_UDS_SID_READ_DID,
            FZC_UDS_NRC_REQUEST_OUT_OF_RANGE, respData, respLen);
        return;
    }

    /* DID is big-endian in request: reqData[1] = MSB, reqData[2] = LSB */
    did = (uint16)((uint16)((uint16)reqData[1] << 8u) | (uint16)reqData[2]);

    /* Positive response header */
    respData[0] = (uint8)(FZC_UDS_SID_READ_DID + UDS_POSITIVE_RESP_SID_OFFSET);
    respData[1] = reqData[1];
    respData[2] = reqData[2];
    pos = 3u;

    switch (did) {
        case FZC_DID_VIN:
        {
            uint8 i;
            for (i = 0u; i < 17u; i++) {
                respData[pos] = FzcDcm_Vin[i];
                pos++;
            }
            break;
        }

        case FZC_DID_HW_VERSION:
        {
            uint8 i;
            for (i = 0u; i < 3u; i++) {
                respData[pos] = FzcDcm_HwVer[i];
                pos++;
            }
            break;
        }

        case FZC_DID_SW_VERSION:
        {
            uint8 i;
            for (i = 0u; i < 5u; i++) {
                respData[pos] = FzcDcm_SwVer[i];
                pos++;
            }
            break;
        }

        case FZC_DID_STEERING_ANGLE:
            rteVal = 0u;
            (void)Rte_Read(FZC_SIG_STEER_ANGLE, &rteVal);
            respData[pos] = (uint8)(rteVal & 0xFFu);
            pos++;
            respData[pos] = (uint8)((rteVal >> 8u) & 0xFFu);
            pos++;
            break;

        case FZC_DID_BRAKE_POSITION:
            rteVal = 0u;
            (void)Rte_Read(FZC_SIG_BRAKE_POS, &rteVal);
            respData[pos] = (uint8)(rteVal & 0xFFu);
            pos++;
            break;

        case FZC_DID_LIDAR_DISTANCE:
            rteVal = 0u;
            (void)Rte_Read(FZC_SIG_LIDAR_DIST, &rteVal);
            respData[pos] = (uint8)(rteVal & 0xFFu);
            pos++;
            respData[pos] = (uint8)((rteVal >> 8u) & 0xFFu);
            pos++;
            break;

        case FZC_DID_LIDAR_SIGNAL:
            rteVal = 0u;
            (void)Rte_Read(FZC_SIG_LIDAR_SIGNAL, &rteVal);
            respData[pos] = (uint8)(rteVal & 0xFFu);
            pos++;
            respData[pos] = (uint8)((rteVal >> 8u) & 0xFFu);
            pos++;
            break;

        case FZC_DID_LIDAR_ZONE:
            rteVal = 0u;
            (void)Rte_Read(FZC_SIG_LIDAR_ZONE, &rteVal);
            respData[pos] = (uint8)(rteVal & 0xFFu);
            pos++;
            break;

        default:
            FzcDcm_NegativeResponse(FZC_UDS_SID_READ_DID,
                FZC_UDS_NRC_REQUEST_OUT_OF_RANGE, respData, respLen);
            return;
    }

    *respLen = pos;
}

/* ==================================================================
 * API: Swc_FzcDcm_ProcessRequest
 * ================================================================== */

Std_ReturnType Swc_FzcDcm_ProcessRequest(
    const uint8* reqData,
    uint8 reqLen,
    uint8* respData,
    uint8* respLen)
{
    uint8 sid;

    if ((reqData == NULL_PTR) || (respData == NULL_PTR) || (respLen == NULL_PTR)) {
        return E_NOT_OK;
    }

    if (reqLen < 1u) {
        return E_NOT_OK;
    }

    if (FzcDcm_Initialized != TRUE) {
        return E_NOT_OK;
    }

    sid = reqData[0];

    /* Check if service is supported */
    if (Swc_FzcDcm_IsServiceSupported(sid) != TRUE) {
        FzcDcm_NegativeResponse(sid, FZC_UDS_NRC_SERVICE_NOT_SUPPORTED,
                                respData, respLen);
        return E_OK;
    }

    /* Dispatch by SID */
    switch (sid) {
        case FZC_UDS_SID_READ_DID:
            FzcDcm_HandleReadDid(reqData, reqLen, respData, respLen);
            break;

        case FZC_UDS_SID_TESTER_PRESENT:
            respData[0] = (uint8)(FZC_UDS_SID_TESTER_PRESENT + UDS_POSITIVE_RESP_SID_OFFSET);
            respData[1] = 0u;  /* sub-function echo */
            *respLen = 2u;
            break;

        case FZC_UDS_SID_DIAG_SESSION:
            respData[0] = (uint8)(FZC_UDS_SID_DIAG_SESSION + UDS_POSITIVE_RESP_SID_OFFSET);
            respData[1] = (reqLen >= 2u) ? reqData[1] : 0x01u;  /* Default session */
            *respLen = 2u;
            break;

        case FZC_UDS_SID_ECU_RESET:
            respData[0] = (uint8)(FZC_UDS_SID_ECU_RESET + UDS_POSITIVE_RESP_SID_OFFSET);
            respData[1] = (reqLen >= 2u) ? reqData[1] : 0x01u;
            *respLen = 2u;
            break;

        case FZC_UDS_SID_CLEAR_DTC:
            respData[0] = (uint8)(FZC_UDS_SID_CLEAR_DTC + UDS_POSITIVE_RESP_SID_OFFSET);
            *respLen = 1u;
            break;

        case FZC_UDS_SID_READ_DTC:
            respData[0] = (uint8)(FZC_UDS_SID_READ_DTC + UDS_POSITIVE_RESP_SID_OFFSET);
            respData[1] = (reqLen >= 2u) ? reqData[1] : 0u;
            *respLen = 2u;
            break;

        case FZC_UDS_SID_COMM_CTRL:
            respData[0] = (uint8)(FZC_UDS_SID_COMM_CTRL + UDS_POSITIVE_RESP_SID_OFFSET);
            respData[1] = (reqLen >= 2u) ? reqData[1] : 0u;
            *respLen = 2u;
            break;

        case FZC_UDS_SID_CTRL_DTC_SETTING:
            respData[0] = (uint8)(FZC_UDS_SID_CTRL_DTC_SETTING + UDS_POSITIVE_RESP_SID_OFFSET);
            respData[1] = (reqLen >= 2u) ? reqData[1] : 0u;
            *respLen = 2u;
            break;

        default:
            /* Should not reach here — IsServiceSupported already checked */
            FzcDcm_NegativeResponse(sid, FZC_UDS_NRC_SERVICE_NOT_SUPPORTED,
                                    respData, respLen);
            break;
    }

    return E_OK;
}
