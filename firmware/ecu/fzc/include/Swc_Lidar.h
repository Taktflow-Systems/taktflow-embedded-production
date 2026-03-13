/**
 * @file    Swc_Lidar.h
 * @brief   TFMini-S lidar obstacle detection SWC
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-013 to SWR-FZC-016
 * @traces_to  SSR-FZC-013 to SSR-FZC-016, TSR-030, TSR-031
 *
 * @details  Reads TFMini-S 9-byte UART frames, validates checksum and
 *           plausibility, classifies distance into graduated response zones
 *           (clear/warning/braking/emergency), detects stuck/timeout faults,
 *           and requests degradation after persistent failures.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_LIDAR_H
#define SWC_LIDAR_H

#include "Std_Types.h"

/* ==================================================================
 * Configuration Type
 * ================================================================== */

typedef struct {
    uint16  warnDistCm;         /**< Warning zone threshold (cm)          */
    uint16  brakeDistCm;        /**< Braking zone threshold (cm)          */
    uint16  emergencyDistCm;    /**< Emergency zone threshold (cm)        */
    uint16  timeoutMs;          /**< Frame timeout (ms)                   */
    uint16  stuckCycles;        /**< Identical readings before stuck      */
    uint16  rangeMinCm;         /**< Minimum valid range (cm)             */
    uint16  rangeMaxCm;         /**< Maximum valid range (cm)             */
    uint16  signalMin;          /**< Minimum signal strength              */
    uint16  degradeCycles;      /**< Persistent fault cycles for degrade  */
} Swc_Lidar_ConfigType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the Swc_Lidar module
 * @param  ConfigPtr  Pointer to configuration (must not be NULL)
 */
void Swc_Lidar_Init(const Swc_Lidar_ConfigType* ConfigPtr);

/**
 * @brief  Swc_Lidar cyclic main function — call every 10ms
 */
void Swc_Lidar_MainFunction(void);

/**
 * @brief  Get current lidar distance for diagnostics
 * @param  dist  Output: distance in cm
 * @return E_OK on success, E_NOT_OK if not initialized or dist is NULL
 */
Std_ReturnType Swc_Lidar_GetDistance(uint16* dist);

#endif /* SWC_LIDAR_H */
