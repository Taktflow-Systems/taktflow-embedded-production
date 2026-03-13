/**
 * @file    Swc_Brake.h
 * @brief   Brake servo control SWC — ASIL D
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-009 to SWR-FZC-012
 * @traces_to  SSR-FZC-009 to SSR-FZC-012
 *
 * @details  Brake servo control for the FZC.  Reads brake command from RTE,
 *           drives PWM to the brake servo (TIM2_CH2), performs feedback
 *           verification with debounce, implements command timeout with
 *           auto-brake latch, and triggers motor cutoff CAN sequence on
 *           brake fault.  E-stop forces immediate 100 % brake.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_BRAKE_H
#define SWC_BRAKE_H

#include "Std_Types.h"

/* ==================================================================
 * Configuration Type
 * ================================================================== */

/**
 * @brief  Swc_Brake module configuration
 *
 * All thresholds and timing parameters for brake servo control.
 * Passed to Swc_Brake_Init at startup.
 */
typedef struct {
    uint16  autoTimeoutMs;      /**< Command timeout before auto-brake (default 100 ms) */
    uint8   pwmFaultThreshold;  /**< PWM deviation threshold in percent (default 2)     */
    uint8   faultDebounce;      /**< Consecutive fault cycles before confirmed (default 3) */
    uint8   latchClearCycles;   /**< Fault-free cycles to clear latch (default 50)       */
    uint8   cutoffRepeatCount;  /**< Motor cutoff CAN message repeats (default 10)       */
} Swc_Brake_ConfigType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the Swc_Brake module
 * @param  ConfigPtr  Pointer to configuration (must not be NULL)
 * @note   Must be called once at startup before Swc_Brake_MainFunction.
 *         If ConfigPtr is NULL the module stays uninitialized and
 *         MainFunction is a safe no-op.
 */
void Swc_Brake_Init(const Swc_Brake_ConfigType* ConfigPtr);

/**
 * @brief  Swc_Brake cyclic main function — call every 10 ms
 * @note   Reads brake command, drives PWM, checks feedback,
 *         handles timeout / auto-brake / e-stop, triggers motor
 *         cutoff on fault, writes RTE signals, reports DTCs.
 */
void Swc_Brake_MainFunction(void);

/**
 * @brief  Get current brake position for diagnostics
 * @param  pos  Output: brake position 0-100 percent
 * @return E_OK on success, E_NOT_OK if not initialized or pos is NULL
 */
Std_ReturnType Swc_Brake_GetPosition(uint8* pos);

#endif /* SWC_BRAKE_H */
