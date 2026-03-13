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

/** @brief Total number of CAN broadcasts on activation */
#define ESTOP_BROADCAST_COUNT     4u

/** @brief E-stop CAN PDU length in bytes */
#define ESTOP_PDU_LENGTH          8u

/* ====================================================================
 * Static module state
 * ==================================================================== */

static boolean active;
static uint8   debounce_counter;
static uint8   repeat_counter;
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
    repeat_counter    = 0u;
    initialized       = TRUE;
}

/**
 * @brief  10ms cyclic — read, debounce, latch, broadcast
 *
 * @details Execution flow:
 *   1. Read E-stop pin via IoHwAb (read failure = fail-safe active)
 *   2. Debounce: HIGH sustained for ESTOP_DEBOUNCE_THRESHOLD cycles
 *   3. On first activation: set latch, report DTC, write RTE, send CAN
 *   4. Repeat CAN send for ESTOP_BROADCAST_COUNT total transmissions
 *   5. After all broadcasts: latch remains, no further sends
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

    /* --- 2. Debounce logic ---------------------------------------- */
    if (active == FALSE) {
        if (pin_state == STD_HIGH) {
            debounce_counter++;

            if (debounce_counter >= ESTOP_DEBOUNCE_THRESHOLD) {
                /* --- 3. First activation -------------------------- */
                active = TRUE;
                repeat_counter = 1u;  /* First broadcast counted */

                /* Report DTC */
                Dem_ReportErrorStatus(CVC_DTC_ESTOP_ACTIVATED,
                                      DEM_EVENT_STATUS_FAILED);

                /* Write to RTE */
                (void)Rte_Write(CVC_SIG_ESTOP_ACTIVE, (uint32)TRUE);

                /* Build E-stop PDU and send with E2E */
                {
                    uint8 pdu[ESTOP_PDU_LENGTH] = {0u};
                    pdu[0] = TRUE;  /* E-stop active flag */

                    (void)E2E_Protect(NULL_PTR, NULL_PTR,
                                      pdu, ESTOP_PDU_LENGTH);
                    (void)Com_SendSignal(CVC_COM_TX_ESTOP, pdu);
                }
            }
        } else {
            /* LOW and not yet latched — reset debounce */
            debounce_counter = 0u;
        }
    } else {
        /* --- 4. Already latched — repeat broadcasts --------------- */
        if (repeat_counter < ESTOP_BROADCAST_COUNT) {
            uint8 pdu[ESTOP_PDU_LENGTH] = {0u};
            pdu[0] = TRUE;

            (void)E2E_Protect(NULL_PTR, NULL_PTR,
                              pdu, ESTOP_PDU_LENGTH);
            (void)Com_SendSignal(CVC_COM_TX_ESTOP, pdu);

            repeat_counter++;
        }
        /* After ESTOP_BROADCAST_COUNT: no more sends, latch remains */
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
