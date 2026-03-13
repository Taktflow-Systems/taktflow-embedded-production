/**
 * @file    Swc_RzcDcm.c
 * @brief   RZC UDS diagnostic service support -- DIDs F030-F036
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-029
 * @traces_to  SSR-RZC-029, TSR-048
 *
 * @details  Implements UDS services for the RZC:
 *           1. DiagnosticSessionControl (0x10) — default session only
 *           2. ECUReset (0x11) — acknowledged but not executed here
 *           3. TesterPresent (0x3E) — keep-alive response
 *           4. ReadDataByIdentifier (0x22) — 7 DIDs:
 *              0xF030 motor current (mA, 2 bytes)
 *              0xF031 motor temp (deci-deg C, 2 bytes)
 *              0xF032 motor speed (RPM, 2 bytes)
 *              0xF033 battery voltage (mV, 2 bytes)
 *              0xF034 torque echo (%, 2 bytes)
 *              0xF035 derating (%, 1 byte)
 *              0xF036 ACS723 offset (ADC counts, 2 bytes)
 *
 *           Unsupported services receive NRC 0x11.
 *           Unsupported DIDs receive NRC 0x31.
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6, ISO 14229 UDS
 * @copyright Taktflow Systems 2026
 */

#include "Swc_RzcDcm.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** UDS negative response SID */
#define UDS_NEG_RESP_SID        0x7Fu

/* ==================================================================
 * Module State
 * ================================================================== */

/** Module initialization flag */
static uint8   RzcDcm_Initialized;

/* ==================================================================
 * Private Helper: Read DID value from RTE
 * ================================================================== */

/**
 * @brief  Read a DID value and pack into response buffer
 * @param  did       DID identifier
 * @param  respData  Response buffer (offset after SID+DID)
 * @param  respLen   Pointer to response length (updated)
 * @return E_OK if DID known, E_NOT_OK otherwise
 */
static Std_ReturnType RzcDcm_ReadDid(uint16 did, uint8 *respData, uint8 *respLen)
{
    uint32 value;
    uint8  pos;

    value = 0u;
    pos   = *respLen;

    switch (did)
    {
        case RZC_DID_MOTOR_CURRENT:
            (void)Rte_Read(RZC_SIG_CURRENT_MA, &value);
            respData[pos]      = (uint8)(value & 0xFFu);
            respData[pos + 1u] = (uint8)((value >> 8u) & 0xFFu);
            *respLen = (uint8)(pos + 2u);
            break;

        case RZC_DID_MOTOR_TEMP:
            (void)Rte_Read(RZC_SIG_TEMP1_DC, &value);
            respData[pos]      = (uint8)(value & 0xFFu);
            respData[pos + 1u] = (uint8)((value >> 8u) & 0xFFu);
            *respLen = (uint8)(pos + 2u);
            break;

        case RZC_DID_MOTOR_SPEED:
            (void)Rte_Read(RZC_SIG_ENCODER_SPEED, &value);
            respData[pos]      = (uint8)(value & 0xFFu);
            respData[pos + 1u] = (uint8)((value >> 8u) & 0xFFu);
            *respLen = (uint8)(pos + 2u);
            break;

        case RZC_DID_BATTERY_VOLTAGE:
            (void)Rte_Read(RZC_SIG_BATTERY_MV, &value);
            respData[pos]      = (uint8)(value & 0xFFu);
            respData[pos + 1u] = (uint8)((value >> 8u) & 0xFFu);
            *respLen = (uint8)(pos + 2u);
            break;

        case RZC_DID_TORQUE_ECHO:
            (void)Rte_Read(RZC_SIG_TORQUE_ECHO, &value);
            respData[pos]      = (uint8)(value & 0xFFu);
            respData[pos + 1u] = (uint8)((value >> 8u) & 0xFFu);
            *respLen = (uint8)(pos + 2u);
            break;

        case RZC_DID_DERATING:
            (void)Rte_Read(RZC_SIG_DERATING_PCT, &value);
            respData[pos] = (uint8)(value & 0xFFu);
            *respLen = (uint8)(pos + 1u);
            break;

        case RZC_DID_ACS723_OFFSET:
            /* ACS723 offset stored as raw ADC counts in current signal */
            value = RZC_CURRENT_ZEROCAL_CENTER;  /* Default if not calibrated */
            respData[pos]      = (uint8)(value & 0xFFu);
            respData[pos + 1u] = (uint8)((value >> 8u) & 0xFFu);
            *respLen = (uint8)(pos + 2u);
            break;

        default:
            return E_NOT_OK;
    }

    return E_OK;
}

/* ==================================================================
 * API: Swc_RzcDcm_Init
 * ================================================================== */

void Swc_RzcDcm_Init(void)
{
    RzcDcm_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_RzcDcm_HandleRequest
 * ================================================================== */

Std_ReturnType Swc_RzcDcm_HandleRequest(const uint8 *reqData,
                                         uint8 reqLen,
                                         uint8 *respData,
                                         uint8 *respLen)
{
    uint8  sid;
    uint16 did;
    uint8  rLen;

    if ((reqData == NULL_PTR) || (respData == NULL_PTR) || (respLen == NULL_PTR))
    {
        return E_NOT_OK;
    }

    if (reqLen < 1u)
    {
        return E_NOT_OK;
    }

    sid = reqData[0];

    switch (sid)
    {
        /* ---- DiagnosticSessionControl (0x10) ---- */
        case RZC_UDS_SID_DIAG_SESS:
            respData[0] = (uint8)(sid + 0x40u);
            respData[1] = 0x01u;  /* Default session */
            *respLen = 2u;
            break;

        /* ---- ECUReset (0x11) ---- */
        case RZC_UDS_SID_ECU_RESET:
            respData[0] = (uint8)(sid + 0x40u);
            respData[1] = 0x01u;  /* Hard reset */
            *respLen = 2u;
            break;

        /* ---- TesterPresent (0x3E) ---- */
        case RZC_UDS_SID_TESTER_PRESENT:
            respData[0] = (uint8)(sid + 0x40u);
            respData[1] = 0x00u;
            *respLen = 2u;
            break;

        /* ---- ReadDataByIdentifier (0x22) ---- */
        case RZC_UDS_SID_READ_DID:
            if (reqLen < 3u)
            {
                /* Negative response: request out of range */
                respData[0] = UDS_NEG_RESP_SID;
                respData[1] = sid;
                respData[2] = RZC_UDS_NRC_REQUEST_OUT_OF_RANGE;
                *respLen = 3u;
                break;
            }

            did = (uint16)((uint16)reqData[1] << 8u) | (uint16)reqData[2];

            /* Positive response header: SID + 0x40, DID high, DID low */
            respData[0] = (uint8)(sid + 0x40u);
            respData[1] = reqData[1];
            respData[2] = reqData[2];
            rLen = 3u;

            if (RzcDcm_ReadDid(did, respData, &rLen) != E_OK)
            {
                /* Unsupported DID */
                respData[0] = UDS_NEG_RESP_SID;
                respData[1] = sid;
                respData[2] = RZC_UDS_NRC_REQUEST_OUT_OF_RANGE;
                *respLen = 3u;
            }
            else
            {
                *respLen = rLen;
            }
            break;

        /* ---- Unsupported service ---- */
        default:
            respData[0] = UDS_NEG_RESP_SID;
            respData[1] = sid;
            respData[2] = RZC_UDS_NRC_SERVICE_NOT_SUPPORTED;
            *respLen = 3u;
            break;
    }

    return E_OK;
}
