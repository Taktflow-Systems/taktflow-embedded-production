/**
 * @file    Swc_Steering.h
 * @brief   Steering servo control SWC — ASIL D
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-001 to SWR-FZC-008, SWR-FZC-028
 * @traces_to  SSR-FZC-001 to SSR-FZC-008
 *
 * @details  Reads steering angle feedback via IoHwAb, receives commanded
 *           steering angle from RTE, performs plausibility checking,
 *           range checking, rate limiting, command timeout detection with
 *           return-to-center, fault latching with 3-level PWM disable,
 *           and outputs servo PWM via Pwm_SetDutyCycle / Dio_WriteChannel.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_STEERING_H
#define SWC_STEERING_H

#include "Std_Types.h"

/* ==================================================================
 * Configuration Type
 * ================================================================== */

/**
 * @brief  Swc_Steering module configuration
 *
 * All thresholds and timing parameters for steering servo processing.
 * Passed to Swc_Steering_Init at startup.
 */
typedef struct {
    uint16  plausThreshold;     /**< |cmd - feedback| threshold in degrees (default 5)      */
    uint8   plausDebounce;      /**< Consecutive cycles before plausibility fault (default 5) */
    uint16  rateLimitDeg10ms;   /**< Tenths of degree per 10ms rate limit (default 3)        */
    uint16  cmdTimeoutMs;       /**< Command timeout in ms (default 100)                      */
    uint16  rtcRateDegS;        /**< Return-to-center rate in deg/s (default 30)              */
    uint8   latchClearCycles;   /**< Fault-free cycles to clear latch (default 50)            */
} Swc_Steering_ConfigType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the Swc_Steering module
 * @param  ConfigPtr  Pointer to configuration (must not be NULL)
 * @note   Must be called once at startup before Swc_Steering_MainFunction.
 *         If ConfigPtr is NULL, the module stays uninitialized and
 *         MainFunction outputs neutral PWM.
 */
void Swc_Steering_Init(const Swc_Steering_ConfigType* ConfigPtr);

/**
 * @brief  Swc_Steering cyclic main function — call every 10ms
 * @note   Reads steering command from RTE, reads actual angle from IoHwAb,
 *         performs plausibility/range/rate checks, applies return-to-center
 *         on timeout, converts angle to PWM, handles fault latch and
 *         3-level PWM disable, writes outputs to RTE and PWM/Dio.
 */
void Swc_Steering_MainFunction(void);

/**
 * @brief  Get current steering angle for diagnostics
 * @param  angle  Output: current steering angle in degrees (-45 to +45)
 * @return E_OK on success, E_NOT_OK if not initialized or angle is NULL
 */
Std_ReturnType Swc_Steering_GetAngle(sint16* angle);

#endif /* SWC_STEERING_H */
