/**
 * @file    Swc_RzcDcm.h
 * @brief   RZC UDS diagnostic service support -- DIDs F030-F036
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-029
 * @traces_to  SSR-RZC-029, TSR-048
 *
 * @details  Implements UDS service support for the RZC:
 *           - Physical addressing on 0x7E2, functional on 0x7DF
 *           - Response address 0x7EA
 *           - ReadDataByIdentifier (0x22) for DIDs:
 *               0xF030  Motor current (mA)
 *               0xF031  Motor temperature (deci-degrees C)
 *               0xF032  Motor speed (RPM)
 *               0xF033  Battery voltage (mV)
 *               0xF034  Torque command echo (%)
 *               0xF035  Derating factor (%)
 *               0xF036  ACS723 zero-cal offset (ADC counts)
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6, ISO 14229 UDS
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_RZC_DCM_H
#define SWC_RZC_DCM_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** UDS addressing */
#define RZC_UDS_PHYS_REQ_ID     0x7E2u
#define RZC_UDS_FUNC_REQ_ID     0x7DFu
#define RZC_UDS_RESP_ID         0x7EAu

/** UDS service IDs */
#define RZC_UDS_SID_READ_DID    0x22u
#define RZC_UDS_SID_DIAG_SESS   0x10u
#define RZC_UDS_SID_ECU_RESET   0x11u
#define RZC_UDS_SID_TESTER_PRESENT 0x3Eu

/** Negative response codes */
#define RZC_UDS_NRC_SERVICE_NOT_SUPPORTED     0x11u
#define RZC_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED 0x12u
#define RZC_UDS_NRC_REQUEST_OUT_OF_RANGE      0x31u

/** DID identifiers */
#define RZC_DID_MOTOR_CURRENT   0xF030u
#define RZC_DID_MOTOR_TEMP      0xF031u
#define RZC_DID_MOTOR_SPEED     0xF032u
#define RZC_DID_BATTERY_VOLTAGE 0xF033u
#define RZC_DID_TORQUE_ECHO     0xF034u
#define RZC_DID_DERATING        0xF035u
#define RZC_DID_ACS723_OFFSET   0xF036u

/** Number of supported DIDs */
#define RZC_DID_COUNT           7u

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the RZC diagnostic module
 */
void Swc_RzcDcm_Init(void);

/**
 * @brief  Handle an incoming UDS request
 * @param  reqData    Pointer to UDS request bytes (SID + data)
 * @param  reqLen     Length of request in bytes
 * @param  respData   Pointer to response buffer (filled by this function)
 * @param  respLen    Pointer to response length (output)
 * @return E_OK if response generated, E_NOT_OK on error
 *
 * @safety_req SWR-RZC-029
 */
Std_ReturnType Swc_RzcDcm_HandleRequest(const uint8 *reqData,
                                         uint8 reqLen,
                                         uint8 *respData,
                                         uint8 *respLen);

#endif /* SWC_RZC_DCM_H */
