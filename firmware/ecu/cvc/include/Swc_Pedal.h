/**
 * @file    Swc_Pedal.h
 * @brief   Dual pedal sensor processing SWC
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-001 to SWR-CVC-008
 * @traces_to  SSR-CVC-001 to SSR-CVC-008, TSR-030, TSR-031
 *
 * @details  Reads dual redundant AS5048A pedal angle sensors via IoHwAb,
 *           performs plausibility checking, stuck detection, fault latching
 *           with zero-torque safe state, torque lookup with ramp limiting,
 *           and vehicle-state-dependent mode limits (RUN/DEGRADED/LIMP/SAFE_STOP).
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_PEDAL_H
#define SWC_PEDAL_H

#include "Std_Types.h"

/* ==================================================================
 * Configuration Type
 * ================================================================== */

/**
 * @brief  Swc_Pedal module configuration
 *
 * All thresholds and timing parameters for pedal processing.
 * Passed to Swc_Pedal_Init at startup.
 */
typedef struct {
    uint16  plausThreshold;     /**< |S1-S2| count threshold for plausibility fault */
    uint8   plausDebounce;      /**< Number of consecutive cycles before plausibility fault confirmed */
    uint16  stuckThreshold;     /**< |raw1 - prev_raw1| < this counts as stuck candidate */
    uint16  stuckCycles;        /**< Consecutive stuck cycles before STUCK fault declared */
    uint8   latchClearCycles;   /**< Fault-free cycles required to clear zero-torque latch */
    uint16  rampLimit;          /**< Maximum torque increase per MainFunction cycle (units of 0-1000) */
} Swc_Pedal_ConfigType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the Swc_Pedal module
 * @param  ConfigPtr  Pointer to configuration (must not be NULL)
 * @note   Must be called once at startup before Swc_Pedal_MainFunction.
 *         If ConfigPtr is NULL, the module enters a failed state where
 *         MainFunction produces zero torque output.
 */
void Swc_Pedal_Init(const Swc_Pedal_ConfigType* ConfigPtr);

/**
 * @brief  Swc_Pedal cyclic main function — call every 10ms
 * @note   Reads dual pedal sensors, performs plausibility and stuck checks,
 *         calculates position, looks up torque, applies ramp limit and
 *         mode limit, and writes position/fault/torque to RTE.
 *         Reports DTCs via Dem on fault transitions.
 */
void Swc_Pedal_MainFunction(void);

/**
 * @brief  Get current pedal position for diagnostics
 * @param  pos  Output: pedal position as percentage 0-100 (scaled from 0-1000 internal)
 * @return E_OK on success, E_NOT_OK if not initialized or pos is NULL
 */
Std_ReturnType Swc_Pedal_GetPosition(uint8* pos);

#endif /* SWC_PEDAL_H */
