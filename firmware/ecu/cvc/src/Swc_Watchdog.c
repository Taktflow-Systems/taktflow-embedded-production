/**
 * @file    Swc_Watchdog.c
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
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Watchdog.h"
#include "Dio.h"

/* ==================================================================
 * Module State (all static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8                            Wdg_Initialized;
static const Swc_Watchdog_ConfigType*   Wdg_CfgPtr;
static uint32                           Wdg_FeedCount;

/* ==================================================================
 * API: Swc_Watchdog_Init
 * ================================================================== */

void Swc_Watchdog_Init(const Swc_Watchdog_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR)
    {
        Wdg_Initialized = FALSE;
        Wdg_CfgPtr      = NULL_PTR;
        return;
    }

    Wdg_CfgPtr      = ConfigPtr;
    Wdg_FeedCount    = 0u;
    Wdg_Initialized  = TRUE;
}

/* ==================================================================
 * API: Swc_Watchdog_Feed
 * ================================================================== */

/**
 * @brief  Feed external watchdog only when all 4 conditions are met
 *
 * @safety_req SWR-CVC-023
 *
 * Condition gate:
 *   - loopComplete == TRUE  (main loop iteration finished)
 *   - canaryOk     == TRUE  (stack canary not corrupted)
 *   - ramOk        == TRUE  (RAM pattern test passed)
 *   - canOk        == TRUE  (CAN bus not in bus-off)
 *
 * If ANY condition is FALSE, the WDI pin is NOT toggled.
 * The TPS3823 will trigger a hardware reset after its timeout.
 */
Std_ReturnType Swc_Watchdog_Feed(uint8 loopComplete,
                                  uint8 canaryOk,
                                  uint8 ramOk,
                                  uint8 canOk)
{
    if (Wdg_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (Wdg_CfgPtr == NULL_PTR)
    {
        return E_NOT_OK;
    }

    /* All four conditions must be TRUE to feed the watchdog */
    if (loopComplete != TRUE)
    {
        return E_NOT_OK;
    }

    if (canaryOk != TRUE)
    {
        return E_NOT_OK;
    }

    if (ramOk != TRUE)
    {
        return E_NOT_OK;
    }

    if (canOk != TRUE)
    {
        return E_NOT_OK;
    }

    /* All conditions met — toggle WDI pin */
    (void)Dio_FlipChannel(Wdg_CfgPtr->wdiDioChannel);

    Wdg_FeedCount++;

    return E_OK;
}
