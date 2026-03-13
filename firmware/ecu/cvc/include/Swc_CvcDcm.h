/**
 * @file    Swc_CvcDcm.h
 * @brief   CVC-specific UDS service routing, DID reading, and DTC reporting
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-033, SWR-CVC-034, SWR-CVC-035
 * @traces_to  SSR-CVC-033, SSR-CVC-034, SSR-CVC-035, TSR-038, TSR-039
 *
 * @details  CVC application-layer diagnostic module:
 *
 *           UDS Service Routing (SWR-CVC-033):
 *           - Services 0x10, 0x11, 0x22, 0x2E, 0x19, 0x14, 0x27, 0x3E
 *           - Physical addressing: 0x7E0
 *           - Functional addressing: 0x7DF
 *
 *           DTC Reporting via Dem (SWR-CVC-034):
 *           - SetEventStatus, GetFilteredDTC, ClearDTC
 *           - 20 DTCs in RAM
 *
 *           CVC DIDs (SWR-CVC-035):
 *           - 10 DIDs (0xF190 SW version to 0xF016 pedal sensors)
 *
 * @standard AUTOSAR Dcm/Dem integration, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_CVCDCM_H
#define SWC_CVCDCM_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** Supported UDS service IDs */
#define CVCDCM_SID_DIAG_SESSION       0x10u
#define CVCDCM_SID_ECU_RESET          0x11u
#define CVCDCM_SID_READ_DID           0x22u
#define CVCDCM_SID_WRITE_DID          0x2Eu
#define CVCDCM_SID_READ_DTC_INFO      0x19u
#define CVCDCM_SID_CLEAR_DTC          0x14u
#define CVCDCM_SID_SECURITY_ACCESS    0x27u
#define CVCDCM_SID_TESTER_PRESENT     0x3Eu

#define CVCDCM_NUM_SERVICES             8u

/** CVC DID identifiers */
#define CVCDCM_DID_SW_VERSION        0xF190u
#define CVCDCM_DID_ECU_SERIAL        0xF18Cu
#define CVCDCM_DID_HW_VERSION        0xF191u
#define CVCDCM_DID_VEHICLE_STATE     0xF010u
#define CVCDCM_DID_PEDAL_POSITION    0xF011u
#define CVCDCM_DID_PEDAL_FAULT       0xF012u
#define CVCDCM_DID_TORQUE_REQUEST    0xF013u
#define CVCDCM_DID_CAN_STATUS        0xF014u
#define CVCDCM_DID_BATTERY_VOLTAGE   0xF015u
#define CVCDCM_DID_PEDAL_SENSORS     0xF016u

#define CVCDCM_NUM_DIDS                10u

/** UDS addressing */
#define CVCDCM_PHYS_REQ_ID          0x7E0u
#define CVCDCM_FUNC_REQ_ID          0x7DFu

/** NRC codes */
#define CVCDCM_NRC_SERVICE_NOT_SUPPORTED   0x11u
#define CVCDCM_NRC_REQUEST_OUT_OF_RANGE    0x31u
#define CVCDCM_NRC_GENERAL_REJECT          0x10u

/** Max DTC slots for reporting */
#define CVCDCM_MAX_DTC_SLOTS          20u

/* ==================================================================
 * Types
 * ================================================================== */

/** UDS request buffer */
typedef struct {
    uint8   data[8];          /**< UDS request data (CAN 2.0 max)    */
    uint8   length;           /**< Number of valid bytes              */
} Swc_CvcDcm_RequestType;

/** UDS response buffer */
typedef struct {
    uint8   data[8];          /**< UDS response data                  */
    uint8   length;           /**< Number of valid bytes              */
    uint8   nrc;              /**< Negative response code (0 = pos)   */
} Swc_CvcDcm_ResponseType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the CVC DCM SWC
 */
void Swc_CvcDcm_Init(void);

/**
 * @brief  Process a UDS request and produce a response
 * @param  request   Pointer to incoming UDS request
 * @param  response  Pointer to response buffer (filled by function)
 * @return E_OK if processed, E_NOT_OK on invalid input
 */
Std_ReturnType Swc_CvcDcm_ProcessRequest(const Swc_CvcDcm_RequestType* request,
                                          Swc_CvcDcm_ResponseType* response);

/**
 * @brief  Read a DID value
 * @param  did      16-bit DID identifier
 * @param  data     Output buffer for DID data
 * @param  length   On input: buffer size. On output: bytes written
 * @return E_OK if DID found, E_NOT_OK if unsupported
 */
Std_ReturnType Swc_CvcDcm_ReadDid(uint16 did, uint8* data, uint8* length);

/**
 * @brief  Report DTC status via Dem snapshot
 * @param  dtcBuffer  Output buffer for DTC status entries
 * @param  maxEntries Maximum entries the buffer can hold
 * @param  count      Output: actual number of DTCs reported
 * @return E_OK on success
 */
Std_ReturnType Swc_CvcDcm_ReportDtc(uint8* dtcBuffer, uint8 maxEntries,
                                      uint8* count);

/**
 * @brief  Clear all stored DTCs
 * @return E_OK on success
 */
Std_ReturnType Swc_CvcDcm_ClearDtc(void);

#endif /* SWC_CVCDCM_H */
