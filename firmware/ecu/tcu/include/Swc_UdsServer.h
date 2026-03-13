/**
 * @file    Swc_UdsServer.h
 * @brief   UDS Diagnostic Server SWC -- ISO 14229 service dispatch
 * @date    2026-02-23
 *
 * @safety_req SWR-TCU-002, SWR-TCU-003, SWR-TCU-004, SWR-TCU-005,
 *             SWR-TCU-006, SWR-TCU-007, SWR-TCU-011, SWR-TCU-012, SWR-TCU-013
 * @traces_to  TSR-038, TSR-039, TSR-040
 *
 * @standard ISO 14229 (UDS), AUTOSAR_SWS_DiagnosticCommunicationManager
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_UDSSERVER_H
#define SWC_UDSSERVER_H

#include "Std_Types.h"

/* ---- UDS Service IDs ---- */

#define UDS_SID_DIAG_SESSION_CTRL   0x10u
#define UDS_SID_READ_DID            0x22u
#define UDS_SID_WRITE_DID           0x2Eu
#define UDS_SID_CLEAR_DTC           0x14u
#define UDS_SID_READ_DTC_INFO       0x19u
#define UDS_SID_SECURITY_ACCESS     0x27u
#define UDS_SID_TESTER_PRESENT      0x3Eu

/* ---- UDS Session types ---- */

#define UDS_SESSION_DEFAULT     0x01u
#define UDS_SESSION_PROGRAMMING 0x02u
#define UDS_SESSION_EXTENDED    0x03u

/* ---- UDS Negative Response Codes (NRC) ---- */

#define UDS_NRC_SERVICE_NOT_SUPPORTED       0x11u
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED   0x12u
#define UDS_NRC_INCORRECT_MSG_LENGTH        0x13u
#define UDS_NRC_CONDITIONS_NOT_CORRECT      0x22u
#define UDS_NRC_REQUEST_OUT_OF_RANGE        0x31u
#define UDS_NRC_SECURITY_ACCESS_DENIED      0x33u
#define UDS_NRC_INVALID_KEY                 0x35u
#define UDS_NRC_EXCEEDED_ATTEMPTS           0x36u

/* Negative response SID */
#define UDS_NRC_SID                         0x7Fu

/* ---- API Functions ---- */

/**
 * @brief  Initialize UDS server to default session, locked security
 */
void Swc_UdsServer_Init(void);

/**
 * @brief  Cyclic 10ms runnable -- manages session timeout
 */
void Swc_UdsServer_10ms(void);

/**
 * @brief  Process a UDS request and produce a response
 * @param  reqData  Pointer to request bytes (must not be NULL)
 * @param  reqLen   Length of request in bytes
 * @param  rspData  Pointer to response buffer (must not be NULL, min 256 bytes)
 * @param  rspLen   Pointer to receive response length (must not be NULL)
 */
void Swc_UdsServer_ProcessRequest(const uint8* reqData, uint16 reqLen,
                                   uint8* rspData, uint16* rspLen);

/**
 * @brief  Get current diagnostic session
 * @return UDS_SESSION_DEFAULT, UDS_SESSION_PROGRAMMING, or UDS_SESSION_EXTENDED
 */
uint8 Swc_UdsServer_GetSession(void);

/* ---- DCM DID Read Callbacks ----
 * Referenced by DID table in Dcm_Cfg_Tcu.c for ReadDataByIdentifier (0x22).
 */

Std_ReturnType Dcm_ReadDid_Vin(uint8* Data, uint8 Length);
Std_ReturnType Dcm_ReadDid_SwVersion(uint8* Data, uint8 Length);
Std_ReturnType Dcm_ReadDid_HwVersion(uint8* Data, uint8 Length);
Std_ReturnType Dcm_ReadDid_VehicleSpeed(uint8* Data, uint8 Length);
Std_ReturnType Dcm_ReadDid_MotorTemp(uint8* Data, uint8 Length);
Std_ReturnType Dcm_ReadDid_BatteryVoltage(uint8* Data, uint8 Length);
Std_ReturnType Dcm_ReadDid_MotorCurrent(uint8* Data, uint8 Length);
Std_ReturnType Dcm_ReadDid_MotorRpm(uint8* Data, uint8 Length);

#endif /* SWC_UDSSERVER_H */
