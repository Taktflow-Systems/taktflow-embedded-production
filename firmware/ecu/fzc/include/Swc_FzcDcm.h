/**
 * @file    Swc_FzcDcm.h
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
 *           - Supported services: DiagnosticSessionControl (0x10),
 *             ECUReset (0x11), ReadDataByIdentifier (0x22),
 *             ClearDTC (0x14), ReadDTC (0x19),
 *             CommunicationControl (0x28),
 *             TesterPresent (0x3E), ControlDTCSetting (0x85)
 *           - 8 DIDs: 0xF190 VIN, 0xF191 HW version,
 *             0xF195 SW version, 0xF020 steering angle,
 *             0xF021 brake position, 0xF022 lidar distance,
 *             0xF023 lidar signal, 0xF024 lidar zone
 *
 * @standard AUTOSAR DCM pattern, UDS ISO 14229, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_FZC_DCM_H
#define SWC_FZC_DCM_H

#include "Std_Types.h"

/* ==================================================================
 * UDS Service IDs
 * ================================================================== */

#define FZC_UDS_SID_DIAG_SESSION      0x10u
#define FZC_UDS_SID_ECU_RESET         0x11u
#define FZC_UDS_SID_CLEAR_DTC         0x14u
#define FZC_UDS_SID_READ_DTC          0x19u
#define FZC_UDS_SID_READ_DID          0x22u
#define FZC_UDS_SID_COMM_CTRL         0x28u
#define FZC_UDS_SID_TESTER_PRESENT    0x3Eu
#define FZC_UDS_SID_CTRL_DTC_SETTING  0x85u

/** Number of supported UDS services */
#define FZC_UDS_SUPPORTED_COUNT        8u

/* ==================================================================
 * DID Identifiers
 * ================================================================== */

#define FZC_DID_VIN                 0xF190u
#define FZC_DID_HW_VERSION         0xF191u
#define FZC_DID_SW_VERSION         0xF195u
#define FZC_DID_STEERING_ANGLE     0xF020u
#define FZC_DID_BRAKE_POSITION     0xF021u
#define FZC_DID_LIDAR_DISTANCE     0xF022u
#define FZC_DID_LIDAR_SIGNAL       0xF023u
#define FZC_DID_LIDAR_ZONE         0xF024u

/** Number of supported DIDs */
#define FZC_DID_COUNT                  8u

/* ==================================================================
 * UDS Negative Response Codes (NRC)
 * ================================================================== */

#define FZC_UDS_NRC_SERVICE_NOT_SUPPORTED   0x11u
#define FZC_UDS_NRC_SUBFUNCTION_NOT_SUPP    0x12u
#define FZC_UDS_NRC_REQUEST_OUT_OF_RANGE    0x31u
#define FZC_UDS_NRC_CONDITIONS_NOT_CORRECT  0x22u

/* ==================================================================
 * CAN IDs
 * ================================================================== */

#define FZC_UDS_RX_PHYS              0x7E1u
#define FZC_UDS_RX_FUNC              0x7DFu
#define FZC_UDS_TX_RESP              0x7E9u

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the FZC DCM handler
 * @note   Must be called once at startup.
 */
void Swc_FzcDcm_Init(void);

/**
 * @brief  Process an incoming UDS request
 * @param  reqData   Pointer to request data (SID + sub-function + data)
 * @param  reqLen    Length of request data
 * @param  respData  Pointer to response buffer (caller-provided, min 64 bytes)
 * @param  respLen   Output: length of response data written
 * @return E_OK if request processed (positive or negative response),
 *         E_NOT_OK if reqData/respData is NULL
 */
Std_ReturnType Swc_FzcDcm_ProcessRequest(
    const uint8* reqData,
    uint8 reqLen,
    uint8* respData,
    uint8* respLen);

/**
 * @brief  Check if a UDS service ID is supported
 * @param  sid  UDS service identifier
 * @return TRUE if supported, FALSE otherwise
 */
uint8 Swc_FzcDcm_IsServiceSupported(uint8 sid);

#endif /* SWC_FZC_DCM_H */
