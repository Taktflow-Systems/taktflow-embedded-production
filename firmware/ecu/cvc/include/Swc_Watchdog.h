/**
 * @file    Swc_Watchdog.h
 * @brief   External watchdog feed SWC — TPS3823 WDI toggle with 4-condition gate
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-023
 * @traces_to  SSR-CVC-023, TSR-046
 *
 * @details  Feeds the TPS3823 external watchdog by toggling the WDI GPIO pin,
 *           but ONLY when all four safety conditions are met:
 *           1. Main loop completed (loop_complete flag)
 *           2. Stack canary intact
 *           3. RAM pattern test passed
 *           4. CAN bus not in bus-off state
 *
 *           If any condition fails, WDI is NOT toggled and the external
 *           watchdog will reset the MCU after its timeout period.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_WATCHDOG_H
#define SWC_WATCHDOG_H

#include "Std_Types.h"

/* ==================================================================
 * Configuration Type
 * ================================================================== */

typedef struct {
    uint8   wdiDioChannel;      /**< DIO channel for TPS3823 WDI pin       */
} Swc_Watchdog_ConfigType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the watchdog SWC
 * @param  ConfigPtr  Pointer to configuration (must not be NULL)
 */
void Swc_Watchdog_Init(const Swc_Watchdog_ConfigType* ConfigPtr);

/**
 * @brief  Attempt to feed the external watchdog
 * @param  loopComplete   TRUE if main loop iteration completed
 * @param  canaryOk       TRUE if stack canary is intact
 * @param  ramOk          TRUE if RAM pattern test passed
 * @param  canOk          TRUE if CAN bus is not in bus-off
 * @return E_OK if watchdog was fed, E_NOT_OK if any condition failed
 *
 * @note   Only toggles the WDI pin when ALL four conditions are TRUE.
 *         Called from the main loop after all runnables have executed.
 */
Std_ReturnType Swc_Watchdog_Feed(uint8 loopComplete,
                                  uint8 canaryOk,
                                  uint8 ramOk,
                                  uint8 canOk);

#endif /* SWC_WATCHDOG_H */
