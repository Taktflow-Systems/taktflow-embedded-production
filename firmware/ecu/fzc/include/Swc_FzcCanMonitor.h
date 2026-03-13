/**
 * @file    Swc_FzcCanMonitor.h
 * @brief   FZC CAN bus loss detection — bus-off, silence, error warning
 * @date    2026-02-24
 *
 * @safety_req SWR-FZC-024
 * @traces_to  SSR-FZC-024
 *
 * @details  Monitors the CAN bus for loss conditions:
 *           - Bus-off detection via CanIf status
 *           - Silence detection (no message received for 200ms)
 *           - Error warning (sustained error counter threshold)
 *           On any detection: auto-brake 100%, center steering,
 *           continuous buzzer. NO recovery — maintain safe state
 *           until power cycle.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_FZC_CAN_MONITOR_H
#define SWC_FZC_CAN_MONITOR_H

#include "Std_Types.h"

/* ==================================================================
 * CAN Monitor Status
 * ================================================================== */

#define FZC_CAN_OK                  0u
#define FZC_CAN_BUS_OFF             1u
#define FZC_CAN_SILENCE             2u
#define FZC_CAN_ERROR_WARNING       3u

/* Silence threshold: 200ms / 10ms cycle = 20 cycles */
#define FZC_CAN_SILENCE_CYCLES     20u

/* Error warning sustained threshold: 50 cycles = 500ms */
#define FZC_CAN_ERR_WARN_CYCLES    50u

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the CAN bus monitor
 * @note   Must be called once at startup.
 */
void Swc_FzcCanMonitor_Init(void);

/**
 * @brief  Check CAN bus health — cyclic 10ms
 * @note   Monitors bus-off, silence, and error warning conditions.
 *         On any fault: applies safe state (auto-brake 100%,
 *         center steering, continuous buzzer). NO automatic recovery.
 */
void Swc_FzcCanMonitor_Check(void);

/**
 * @brief  Get current CAN monitor status
 * @return FZC_CAN_OK, FZC_CAN_BUS_OFF, FZC_CAN_SILENCE,
 *         or FZC_CAN_ERROR_WARNING
 */
uint8 Swc_FzcCanMonitor_GetStatus(void);

/**
 * @brief  Notify the monitor that a CAN message was received
 * @note   Called by the COM layer on every successful RX.
 *         Resets the silence counter.
 */
void Swc_FzcCanMonitor_NotifyRx(void);

#endif /* SWC_FZC_CAN_MONITOR_H */
