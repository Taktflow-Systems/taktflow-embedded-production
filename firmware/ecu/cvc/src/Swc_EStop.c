/**
 * @file    Swc_EStop.c
 * @brief   E-stop detection — debounce, latch, CAN broadcast
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-018 to SWR-CVC-020
 * @traces_to  SSR-CVC-018 to SSR-CVC-020, TSR-022, TSR-046
 *
 * @details  Safety-critical SWC: reads the physical E-stop button through
 *           IoHwAb, applies a 1-cycle debounce, latches activation permanently
 *           (safety requirement — never clears without power-cycle), broadcasts
 *           E-stop status on CAN with E2E protection (4 transmissions for
 *           redundancy), writes to RTE, and reports DTC to Dem.
 *
 *           Fail-safe: if IoHwAb read fails, E-stop is treated as active.
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (ASIL D)
 * @copyright Taktflow Systems 2026
 */
#include "Swc_EStop.h"
#include "Cvc_Cfg.h"
#include "IoHwAb.h"
#include "Rte.h"
#include "Com.h"
#include "E2E.h"
#include "Dem.h"

/* ====================================================================
 * Internal constants
 * ==================================================================== */

/** @brief Debounce threshold in 10ms cycles (1 cycle = 10ms) */
#define ESTOP_DEBOUNCE_THRESHOLD  1u

/** @brief E-stop CAN PDU length in bytes */
#define ESTOP_PDU_LENGTH          8u

/* ====================================================================
 * Static module state
 * ==================================================================== */

static boolean active;
static uint8   debounce_counter;
static boolean initialized;

/* ====================================================================
 * Public functions
 * ==================================================================== */

/**
 * @brief  Initialise all E-stop state to safe defaults
 */
void Swc_EStop_Init(void)
{
    active            = FALSE;
    debounce_counter  = 0u;
    initialized       = TRUE;
}

/**
 * @brief  10ms cyclic — read, debounce, latch, broadcast
 *
 * @details Execution flow:
 *   1. Read E-stop pin via IoHwAb (read failure = fail-safe active)
 *   2. Debounce: HIGH sustained for ESTOP_DEBOUNCE_THRESHOLD cycles
 *   3. On first activation: set latch, report DTC, write RTE
 *   4. While latched: broadcast EStop_Broadcast (0x001) every cycle
 *      (IfActive pattern per automotive convention — ensures recovering
 *      nodes and late-joining testers see the current E-Stop state)
 *   5. Latch is permanent until power cycle (no software clear)
 */
void Swc_EStop_MainFunction(void)
{
    uint8          pin_state = STD_LOW;
    Std_ReturnType ret;

    if (initialized == FALSE) {
        return;
    }

    /* --- 1. Read E-stop button ------------------------------------ */
    ret = IoHwAb_ReadEStop(&pin_state);

    if (ret != E_OK) {
        /* Fail-safe: treat read failure as E-stop active */
        pin_state = STD_HIGH;
    }

    /* --- 2. Debounce + first activation --------------------------- */
    if (active == FALSE) {
        if (pin_state == STD_HIGH) {
            debounce_counter++;

            if (debounce_counter >= ESTOP_DEBOUNCE_THRESHOLD) {
                active = TRUE;

                /* Report DTC (once) */
                Dem_ReportErrorStatus(CVC_DTC_ESTOP_ACTIVATED,
                                      DEM_EVENT_STATUS_FAILED);

                /* Write to RTE (once — latched, never cleared) */
                (void)Rte_Write(CVC_SIG_ESTOP_ACTIVE, (uint32)TRUE);
            }
        } else {
            /* LOW and not yet latched — reset debounce */
            debounce_counter = 0u;
        }
    }

    /* --- 3. Cyclic broadcast while latched — via RTE → Swc_CvcCom bridge.
     * Also re-report the DTC each cycle: DEM has a 3-cycle FAIL threshold
     * before it sets CONFIRMED and broadcasts on 0x500, so a single
     * edge-report would leave debounceCounter at 1 forever and the DTC
     * would never reach the gateway. */
    if (active == TRUE) {
        (void)Rte_Write(CVC_SIG_ESTOP_ACTIVE, 1u);
        Dem_ReportErrorStatus(CVC_DTC_ESTOP_ACTIVATED,
                              DEM_EVENT_STATUS_FAILED);
    }
}

/**
 * @brief  Query E-stop latch status
 * @return TRUE if E-stop has been activated (permanent latch)
 */
boolean Swc_EStop_IsActive(void)
{
    return active;
}
