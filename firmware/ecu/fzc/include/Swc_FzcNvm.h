/**
 * @file    Swc_FzcNvm.h
 * @brief   FZC NVM — DTC persistence and servo calibration data storage
 * @date    2026-02-24
 *
 * @safety_req SWR-FZC-031, SWR-FZC-032
 * @traces_to  SSR-FZC-031, SSR-FZC-032
 *
 * @details  Non-volatile memory management for the FZC:
 *           DTC persistence (SWR-FZC-031):
 *           - 20 DTC slots with CRC-16 integrity
 *           - Freeze-frame data: steering angle, brake position,
 *             lidar distance at time of fault
 *           Servo calibration (SWR-FZC-032):
 *           - Steering center offset, steering gain
 *           - Brake servo offsets
 *           - Lidar distance thresholds (warn, brake, emergency)
 *           - CRC-16 protected, defaults on corruption
 *
 * @standard AUTOSAR NvM pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_FZC_NVM_H
#define SWC_FZC_NVM_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** Maximum number of DTC slots in NVM */
#define FZC_NVM_DTC_MAX_SLOTS       20u

/** CRC-16 polynomial (CRC-CCITT) */
#define FZC_NVM_CRC16_POLY       0x1021u

/** CRC-16 initial value */
#define FZC_NVM_CRC16_INIT       0xFFFFu

/* ==================================================================
 * DTC Record Type
 * ================================================================== */

/**
 * @brief  Single DTC record with freeze-frame data
 */
typedef struct {
    uint8   dtcId;          /**< DTC event ID (from Fzc_Cfg.h)            */
    uint8   status;         /**< DTC status (active/passive/cleared)      */
    sint16  freezeSteer;    /**< Steering angle at time of fault (deg)    */
    uint8   freezeBrake;    /**< Brake position at time of fault (%)      */
    uint16  freezeLidar;    /**< Lidar distance at time of fault (cm)     */
    uint16  crc;            /**< CRC-16 over the above fields             */
} Swc_FzcNvm_DtcRecord;

/* DTC status values */
#define FZC_NVM_DTC_EMPTY           0u
#define FZC_NVM_DTC_ACTIVE          1u
#define FZC_NVM_DTC_PASSIVE         2u
#define FZC_NVM_DTC_CLEARED         3u

/* ==================================================================
 * Calibration Data Type
 * ================================================================== */

/**
 * @brief  Servo calibration parameters stored in NVM
 */
typedef struct {
    sint16  steerCenterOffset;  /**< Steering center offset (tenths of deg)   */
    uint16  steerGain;          /**< Steering gain (scale factor x100)        */
    sint16  brakePosOffset;     /**< Brake servo position offset              */
    uint16  brakeGain;          /**< Brake servo gain (scale factor x100)     */
    uint16  lidarWarnCm;        /**< Lidar warning threshold (cm)             */
    uint16  lidarBrakeCm;       /**< Lidar braking threshold (cm)             */
    uint16  lidarEmergencyCm;   /**< Lidar emergency threshold (cm)           */
    uint16  crc;                /**< CRC-16 over all above fields             */
} Swc_FzcNvm_CalData;

/* ==================================================================
 * Default Calibration Values
 * ================================================================== */

#define FZC_NVM_CAL_STEER_OFFSET_DEFAULT     0
#define FZC_NVM_CAL_STEER_GAIN_DEFAULT     100u
#define FZC_NVM_CAL_BRAKE_OFFSET_DEFAULT     0
#define FZC_NVM_CAL_BRAKE_GAIN_DEFAULT     100u
#define FZC_NVM_CAL_LIDAR_WARN_DEFAULT     100u
#define FZC_NVM_CAL_LIDAR_BRAKE_DEFAULT     50u
#define FZC_NVM_CAL_LIDAR_EMERG_DEFAULT     20u

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the NVM module, load DTC and calibration data
 * @note   Loads from NVM backend. On CRC failure, applies defaults.
 */
void Swc_FzcNvm_Init(void);

/**
 * @brief  Store a DTC record with freeze-frame data
 * @param  dtcId       DTC event ID
 * @param  steerAngle  Current steering angle (deg)
 * @param  brakePos    Current brake position (%)
 * @param  lidarDist   Current lidar distance (cm)
 * @return E_OK on success, E_NOT_OK if DTC slots full
 */
Std_ReturnType Swc_FzcNvm_StoreDtc(
    uint8 dtcId,
    sint16 steerAngle,
    uint8 brakePos,
    uint16 lidarDist);

/**
 * @brief  Load a DTC record by index
 * @param  index    DTC slot index (0..FZC_NVM_DTC_MAX_SLOTS-1)
 * @param  record   Output: DTC record data
 * @return E_OK if valid record found, E_NOT_OK if empty/corrupt/out-of-range
 */
Std_ReturnType Swc_FzcNvm_LoadDtc(uint8 index, Swc_FzcNvm_DtcRecord* record);

/**
 * @brief  Load calibration data from NVM
 * @param  cal  Output: calibration data (defaults applied if NVM corrupt)
 * @return E_OK if loaded from NVM, E_NOT_OK if defaults were applied
 */
Std_ReturnType Swc_FzcNvm_LoadCal(Swc_FzcNvm_CalData* cal);

/**
 * @brief  Store calibration data to NVM
 * @param  cal  Calibration data to store (CRC computed internally)
 * @return E_OK on success, E_NOT_OK if cal is NULL
 */
Std_ReturnType Swc_FzcNvm_StoreCal(const Swc_FzcNvm_CalData* cal);

/**
 * @brief  Compute CRC-16 over a data buffer
 * @param  data   Pointer to data
 * @param  length Number of bytes
 * @return CRC-16 value
 */
uint16 Swc_FzcNvm_Crc16(const uint8* data, uint16 length);

#endif /* SWC_FZC_NVM_H */
