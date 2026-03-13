/**
 * @file    Swc_Obd2Pids.h
 * @brief   OBD-II PID handler SWC -- SAE J1979 diagnostic interface
 * @date    2026-02-23
 *
 * @safety_req SWR-TCU-010
 * @traces_to  TSR-038, TSR-040
 *
 * @standard SAE J1979 (OBD-II), ISO 15031-5
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_OBD2PIDS_H
#define SWC_OBD2PIDS_H

#include "Std_Types.h"

/* ---- OBD-II modes ---- */

#define OBD_MODE_CURRENT_DATA   0x01u
#define OBD_MODE_CONFIRMED_DTC  0x03u
#define OBD_MODE_CLEAR_DTC      0x04u
#define OBD_MODE_VEHICLE_INFO   0x09u

/* ---- Supported PIDs ---- */

#define OBD_PID_SUPPORTED_00    0x00u
#define OBD_PID_ENGINE_LOAD     0x04u
#define OBD_PID_COOLANT_TEMP    0x05u
#define OBD_PID_ENGINE_RPM      0x0Cu
#define OBD_PID_VEHICLE_SPEED   0x0Du
#define OBD_PID_CONTROL_VOLTAGE 0x42u
#define OBD_PID_AMBIENT_TEMP    0x46u
#define OBD_PID_VIN             0x02u

/* ---- API Functions ---- */

/**
 * @brief  Initialize OBD-II PID handler
 */
void Swc_Obd2Pids_Init(void);

/**
 * @brief  Handle an OBD-II request
 * @param  mode      OBD-II service mode (0x01, 0x03, 0x04, 0x09)
 * @param  pid       PID within the mode
 * @param  response  Output buffer for response data
 * @param  len       Pointer to receive response length
 * @return E_OK on success, E_NOT_OK if mode/PID not supported
 */
Std_ReturnType Swc_Obd2Pids_HandleRequest(uint8 mode, uint8 pid,
                                            uint8* response, uint16* len);

#endif /* SWC_OBD2PIDS_H */
