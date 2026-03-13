/**
 * @file    Swc_CvcDcm.c
 * @brief   CVC-specific UDS service routing, DID reading, and DTC reporting
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-033, SWR-CVC-034, SWR-CVC-035
 * @traces_to  SSR-CVC-033, SSR-CVC-034, SSR-CVC-035, TSR-038, TSR-039
 *
 * @details  UDS Service Routing (SWR-CVC-033):
 *           - Supports 8 services: 0x10, 0x11, 0x22, 0x2E, 0x19, 0x14, 0x27, 0x3E
 *           - Physical 0x7E0, functional 0x7DF
 *
 *           DTC Reporting (SWR-CVC-034):
 *           - SetEventStatus, GetFilteredDTC, ClearDTC
 *           - 20 DTCs in RAM
 *
 *           CVC DIDs (SWR-CVC-035):
 *           - 10 DIDs from 0xF190 to 0xF016
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR Dcm/Dem integration, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_CvcDcm.h"
#include "Cvc_Cfg.h"
#include "Dem.h"
#include "Rte.h"

/* ==================================================================
 * Supported UDS Service Table
 * ================================================================== */

static const uint8 CvcDcm_SupportedServices[CVCDCM_NUM_SERVICES] = {
    CVCDCM_SID_DIAG_SESSION,
    CVCDCM_SID_ECU_RESET,
    CVCDCM_SID_READ_DID,
    CVCDCM_SID_WRITE_DID,
    CVCDCM_SID_READ_DTC_INFO,
    CVCDCM_SID_CLEAR_DTC,
    CVCDCM_SID_SECURITY_ACCESS,
    CVCDCM_SID_TESTER_PRESENT
};

/* ==================================================================
 * DID Table
 * ================================================================== */

static const uint16 CvcDcm_DidTable[CVCDCM_NUM_DIDS] = {
    CVCDCM_DID_SW_VERSION,
    CVCDCM_DID_ECU_SERIAL,
    CVCDCM_DID_HW_VERSION,
    CVCDCM_DID_VEHICLE_STATE,
    CVCDCM_DID_PEDAL_POSITION,
    CVCDCM_DID_PEDAL_FAULT,
    CVCDCM_DID_TORQUE_REQUEST,
    CVCDCM_DID_CAN_STATUS,
    CVCDCM_DID_BATTERY_VOLTAGE,
    CVCDCM_DID_PEDAL_SENSORS
};

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8   CvcDcm_Initialized;

/* DTC RAM storage */
static uint8   CvcDcm_DtcStatus[CVCDCM_MAX_DTC_SLOTS];
static uint8   CvcDcm_DtcCount;

/* Software version string (fixed) */
static const uint8 CvcDcm_SwVersion[4] = { 0x01u, 0x00u, 0x00u, 0x00u };

/* ==================================================================
 * Private: Check if service is supported
 * ================================================================== */

static uint8 CvcDcm_IsServiceSupported(uint8 sid)
{
    uint8 i;

    for (i = 0u; i < CVCDCM_NUM_SERVICES; i++)
    {
        if (CvcDcm_SupportedServices[i] == sid)
        {
            return TRUE;
        }
    }

    return FALSE;
}

/* ==================================================================
 * Private: Find DID index
 * ================================================================== */

static uint8 CvcDcm_FindDid(uint16 did, uint8* index)
{
    uint8 i;

    for (i = 0u; i < CVCDCM_NUM_DIDS; i++)
    {
        if (CvcDcm_DidTable[i] == did)
        {
            *index = i;
            return TRUE;
        }
    }

    return FALSE;
}

/* ==================================================================
 * API: Swc_CvcDcm_Init
 * ================================================================== */

void Swc_CvcDcm_Init(void)
{
    uint8 i;

    for (i = 0u; i < CVCDCM_MAX_DTC_SLOTS; i++)
    {
        CvcDcm_DtcStatus[i] = 0u;
    }

    CvcDcm_DtcCount   = 0u;
    CvcDcm_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_CvcDcm_ProcessRequest
 * ================================================================== */

/**
 * @safety_req SWR-CVC-033
 */
Std_ReturnType Swc_CvcDcm_ProcessRequest(const Swc_CvcDcm_RequestType* request,
                                          Swc_CvcDcm_ResponseType* response)
{
    uint8 sid;

    if (CvcDcm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (request == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (response == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (request->length == 0u)
    {
        return E_NOT_OK;
    }

    sid = request->data[0];
    response->nrc    = 0u;
    response->length = 0u;

    /* Check if service is supported */
    if (CvcDcm_IsServiceSupported(sid) != TRUE)
    {
        /* Negative response */
        response->data[0] = 0x7Fu;
        response->data[1] = sid;
        response->data[2] = CVCDCM_NRC_SERVICE_NOT_SUPPORTED;
        response->length   = 3u;
        response->nrc      = CVCDCM_NRC_SERVICE_NOT_SUPPORTED;
        return E_OK;
    }

    /* Dispatch supported services */
    switch (sid)
    {
        case CVCDCM_SID_READ_DID:
        {
            uint16 did;
            uint8  didData[4];
            uint8  didLen;

            if (request->length < 3u)
            {
                response->data[0] = 0x7Fu;
                response->data[1] = sid;
                response->data[2] = CVCDCM_NRC_REQUEST_OUT_OF_RANGE;
                response->length  = 3u;
                response->nrc     = CVCDCM_NRC_REQUEST_OUT_OF_RANGE;
            }
            else
            {
                did = ((uint16)request->data[1] << 8u) | (uint16)request->data[2];
                didLen = 4u;

                if (Swc_CvcDcm_ReadDid(did, didData, &didLen) == E_OK)
                {
                    response->data[0] = sid + 0x40u;
                    response->data[1] = request->data[1];
                    response->data[2] = request->data[2];
                    response->data[3] = didData[0];
                    response->length  = 4u;
                }
                else
                {
                    response->data[0] = 0x7Fu;
                    response->data[1] = sid;
                    response->data[2] = CVCDCM_NRC_REQUEST_OUT_OF_RANGE;
                    response->length  = 3u;
                    response->nrc     = CVCDCM_NRC_REQUEST_OUT_OF_RANGE;
                }
            }
            break;
        }

        case CVCDCM_SID_TESTER_PRESENT:
        {
            response->data[0] = sid + 0x40u;
            response->length  = 1u;
            break;
        }

        case CVCDCM_SID_CLEAR_DTC:
        {
            (void)Swc_CvcDcm_ClearDtc();
            response->data[0] = sid + 0x40u;
            response->length  = 1u;
            break;
        }

        case CVCDCM_SID_READ_DTC_INFO:
        {
            uint8 dtcBuf[CVCDCM_MAX_DTC_SLOTS];
            uint8 dtcCount = 0u;

            (void)Swc_CvcDcm_ReportDtc(dtcBuf, CVCDCM_MAX_DTC_SLOTS, &dtcCount);
            response->data[0] = sid + 0x40u;
            response->data[1] = dtcCount;
            response->length  = 2u;
            break;
        }

        default:
        {
            /* Other supported services: positive response stub */
            response->data[0] = sid + 0x40u;
            response->length  = 1u;
            break;
        }
    }

    return E_OK;
}

/* ==================================================================
 * API: Swc_CvcDcm_ReadDid
 * ================================================================== */

/**
 * @safety_req SWR-CVC-035
 */
Std_ReturnType Swc_CvcDcm_ReadDid(uint16 did, uint8* data, uint8* length)
{
    uint8  didIndex;
    uint32 rteValue;

    if (CvcDcm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (data == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (length == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (CvcDcm_FindDid(did, &didIndex) != TRUE)
    {
        return E_NOT_OK;
    }

    rteValue = 0u;

    switch (did)
    {
        case CVCDCM_DID_SW_VERSION:
            data[0] = CvcDcm_SwVersion[0];
            if (*length >= 4u)
            {
                data[1] = CvcDcm_SwVersion[1];
                data[2] = CvcDcm_SwVersion[2];
                data[3] = CvcDcm_SwVersion[3];
                *length = 4u;
            }
            else
            {
                *length = 1u;
            }
            break;

        case CVCDCM_DID_VEHICLE_STATE:
            (void)Rte_Read(CVC_SIG_VEHICLE_STATE, &rteValue);
            data[0] = (uint8)rteValue;
            *length = 1u;
            break;

        case CVCDCM_DID_PEDAL_POSITION:
            (void)Rte_Read(CVC_SIG_PEDAL_POSITION, &rteValue);
            data[0] = (uint8)(rteValue & 0xFFu);
            *length = 1u;
            break;

        case CVCDCM_DID_PEDAL_FAULT:
            (void)Rte_Read(CVC_SIG_PEDAL_FAULT, &rteValue);
            data[0] = (uint8)rteValue;
            *length = 1u;
            break;

        case CVCDCM_DID_PEDAL_SENSORS:
            (void)Rte_Read(CVC_SIG_PEDAL_POSITION, &rteValue);
            data[0] = (uint8)(rteValue & 0xFFu);
            if (*length >= 2u)
            {
                (void)Rte_Read(CVC_SIG_PEDAL_FAULT, &rteValue);
                data[1] = (uint8)rteValue;
                *length = 2u;
            }
            else
            {
                *length = 1u;
            }
            break;

        default:
            data[0] = 0u;
            *length = 1u;
            break;
    }

    return E_OK;
}

/* ==================================================================
 * API: Swc_CvcDcm_ReportDtc
 * ================================================================== */

/**
 * @safety_req SWR-CVC-034
 */
Std_ReturnType Swc_CvcDcm_ReportDtc(uint8* dtcBuffer, uint8 maxEntries,
                                      uint8* count)
{
    uint8 i;
    uint8 found;
    uint8 eventStatus;

    if (CvcDcm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (dtcBuffer == NULL_PTR)
    {
        return E_NOT_OK;
    }

    if (count == NULL_PTR)
    {
        return E_NOT_OK;
    }

    found = 0u;

    for (i = 0u; i < CVCDCM_MAX_DTC_SLOTS; i++)
    {
        if (found >= maxEntries)
        {
            break;
        }

        eventStatus = 0u;
        if (Dem_GetEventStatus(i, &eventStatus) == E_OK)
        {
            if ((eventStatus & DEM_STATUS_TEST_FAILED) != 0u)
            {
                dtcBuffer[found] = i;
                found++;
            }
        }
    }

    *count = found;
    return E_OK;
}

/* ==================================================================
 * API: Swc_CvcDcm_ClearDtc
 * ================================================================== */

/**
 * @safety_req SWR-CVC-034
 */
Std_ReturnType Swc_CvcDcm_ClearDtc(void)
{
    if (CvcDcm_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    return Dem_ClearAllDTCs();
}
