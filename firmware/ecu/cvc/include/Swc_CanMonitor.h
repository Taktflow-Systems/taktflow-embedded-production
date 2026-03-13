/**
 * @file    Swc_CanMonitor.h
 * @brief   CAN bus loss detection and recovery SWC
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-024, SWR-CVC-025
 * @traces_to  SSR-CVC-024, SSR-CVC-025, TSR-022, TSR-023
 *
 * @details  Monitors the CAN bus for three fault conditions:
 *           1. Bus-off state (immediate SAFE_STOP)
 *           2. 200ms silence (no message received) -> SAFE_STOP
 *           3. Error warning sustained 500ms -> SAFE_STOP
 *
 *           Recovery: up to 3 attempts within 10s, then SHUTDOWN.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_CANMONITOR_H
#define SWC_CANMONITOR_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

#define CANMON_SILENCE_TIMEOUT_MS       200u
#define CANMON_ERROR_WARN_TIMEOUT_MS    500u
#define CANMON_MAX_RECOVERY_ATTEMPTS      3u
#define CANMON_RECOVERY_WINDOW_MS     10000u

/* ==================================================================
 * Status Enum
 * ================================================================== */

#define CANMON_STATUS_OK               0u
#define CANMON_STATUS_BUSOFF            1u
#define CANMON_STATUS_SILENCE           2u
#define CANMON_STATUS_ERROR_WARNING     3u
#define CANMON_STATUS_SAFE_STOP         4u
#define CANMON_STATUS_SHUTDOWN          5u

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the CAN monitor SWC
 */
void Swc_CanMonitor_Init(void);

/**
 * @brief  Periodic CAN bus health check (call every 10ms)
 * @param  isBusOff       TRUE if CAN controller is in bus-off state
 * @param  rxMsgCount     Total received message count (monotonic)
 * @param  errorWarning   TRUE if CAN error warning flag is set
 * @param  currentTimeMs  Current system time in milliseconds
 * @return CAN monitor status code
 */
uint8 Swc_CanMonitor_Check(uint8  isBusOff,
                            uint32 rxMsgCount,
                            uint8  errorWarning,
                            uint32 currentTimeMs);

/**
 * @brief  Attempt CAN bus recovery
 * @param  currentTimeMs  Current system time in milliseconds
 * @return E_OK if recovery attempt initiated, E_NOT_OK if max attempts exhausted
 */
Std_ReturnType Swc_CanMonitor_Recovery(uint32 currentTimeMs);

/**
 * @brief  Get current CAN monitor status
 * @return Status code (CANMON_STATUS_xxx)
 */
uint8 Swc_CanMonitor_GetStatus(void);

#endif /* SWC_CANMONITOR_H */
