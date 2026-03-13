/**
 * @file    sc_heartbeat.c
 * @brief   Per-ECU heartbeat monitoring for Safety Controller
 * @date    2026-02-23
 *
 * @safety_req SWR-SC-004, SWR-SC-005, SWR-SC-006
 * @traces_to  SSR-SC-004, SSR-SC-005, SSR-SC-006
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_heartbeat.h"
#include "Sc_Hw_Cfg.h"
#include "sc_gio.h"

/* ==================================================================
 * Module State
 * ================================================================== */

/** Per-ECU heartbeat timeout counter (10ms ticks) */
static uint16 hb_counter[SC_ECU_COUNT];

/** Per-ECU timeout detected flag (>= 150ms) */
static boolean hb_timed_out[SC_ECU_COUNT];

/** Per-ECU confirmation counter (ticks after timeout detection) */
static uint8 hb_confirm_counter[SC_ECU_COUNT];

/** Per-ECU confirmed timeout flag (latched — requires power cycle) */
static boolean hb_confirmed[SC_ECU_COUNT];

/** Per-ECU recovery counter — consecutive HBs received during timeout */
static uint8 hb_recovery_count[SC_ECU_COUNT];

/** Startup grace counter — skip monitoring until ECUs have time to boot */
static uint16 hb_startup_grace;

/* --- Content validation state (SWR-SC-027, SWR-SC-028) ------------------ */

/** Consecutive heartbeats received while ECU is in DEGRADED or LIMP mode */
static uint8 hb_stuck_degraded_cnt[SC_ECU_COUNT];

/** Consecutive heartbeats received with >=2 FaultStatus bits set */
static uint8 hb_fault_escalate_cnt[SC_ECU_COUNT];

/** Content validation fault flag (latched until power cycle) */
static boolean hb_content_fault[SC_ECU_COUNT];

/** Last observed FaultStatus bitmask per ECU (for IsFzcBrakeFault) */
static uint8 hb_last_fault_status[SC_ECU_COUNT];

/* ==================================================================
 * LED pin lookup by ECU index
 * ================================================================== */

static const uint8 ecu_led_pin[SC_ECU_COUNT] = {
    SC_PIN_LED_CVC,     /* GIO_A1 */
    SC_PIN_LED_FZC,     /* GIO_A2 */
    SC_PIN_LED_RZC      /* GIO_A3 */
};

/* ==================================================================
 * Public API
 * ================================================================== */

void SC_Heartbeat_Init(void)
{
    uint8 i;
    for (i = 0u; i < SC_ECU_COUNT; i++) {
        hb_counter[i]              = 0u;
        hb_timed_out[i]            = FALSE;
        hb_stuck_degraded_cnt[i]   = 0u;
        hb_fault_escalate_cnt[i]   = 0u;
        hb_content_fault[i]        = FALSE;
        hb_last_fault_status[i]    = 0u;
        hb_confirm_counter[i] = 0u;
        hb_confirmed[i]       = FALSE;
        hb_recovery_count[i]  = 0u;
    }
    hb_startup_grace = SC_HB_STARTUP_GRACE_TICKS;
}

void SC_Heartbeat_NotifyRx(uint8 ecuIndex)
{
    if (ecuIndex >= SC_ECU_COUNT) {
        return;
    }

    hb_counter[ecuIndex] = 0u;

    /* Confirmed is latched — no recovery possible after relay kill */
    if (hb_confirmed[ecuIndex] == TRUE) {
        return;
    }

    if (hb_timed_out[ecuIndex] == TRUE) {
        /* Recovery debounce: require SC_HB_RECOVERY_THRESHOLD
         * consecutive heartbeats before canceling timeout */
        hb_recovery_count[ecuIndex]++;
        if (hb_recovery_count[ecuIndex] >= SC_HB_RECOVERY_THRESHOLD) {
            hb_timed_out[ecuIndex]       = FALSE;
            hb_confirm_counter[ecuIndex] = 0u;
            hb_recovery_count[ecuIndex]  = 0u;
            gioSetBit(SC_GIO_PORT_A, ecu_led_pin[ecuIndex], 0u);
        }
    }
    /* If not timed out, counter reset above is sufficient */
}

void SC_Heartbeat_Monitor(void)
{
    uint8 i;

    /* Startup grace period — let ECUs boot before monitoring */
    if (hb_startup_grace > 0u) {
        hb_startup_grace--;
        return;
    }

    for (i = 0u; i < SC_ECU_COUNT; i++) {
        /* Skip if already confirmed (latched) */
        if (hb_confirmed[i] == TRUE) {
            continue;
        }

        /* Increment counter */
        if (hb_counter[i] < 0xFFFFu) {
            hb_counter[i]++;
        }

        /* If not timed out, ensure fault LED is cleared */
        if ((hb_timed_out[i] == FALSE) && (hb_confirmed[i] == FALSE)) {
            gioSetBit(SC_GIO_PORT_A, ecu_led_pin[i], 0u);
        }

        /* Check for timeout threshold (150ms) */
        if (hb_counter[i] >= SC_HB_TIMEOUT_TICKS) {
            if (hb_timed_out[i] == FALSE) {
                /* First detection — enter confirmation window */
                hb_timed_out[i] = TRUE;
                hb_confirm_counter[i] = 0u;

                /* Drive fault LED HIGH immediately */
                gioSetBit(SC_GIO_PORT_A, ecu_led_pin[i], 1u);
            } else {
                /* Already timed out — reset recovery, increment confirmation */
                hb_recovery_count[i] = 0u;
                hb_confirm_counter[i]++;

                /* Check confirmation threshold (50ms additional) */
                if (hb_confirm_counter[i] >= SC_HB_CONFIRM_TICKS) {
                    hb_confirmed[i] = TRUE;
                }
            }
        }
    }
}

boolean SC_Heartbeat_IsTimedOut(uint8 ecuIndex)
{
    if (ecuIndex >= SC_ECU_COUNT) {
        return FALSE;
    }
    return hb_timed_out[ecuIndex];
}

boolean SC_Heartbeat_IsAnyConfirmed(void)
{
    uint8 i;
    for (i = 0u; i < SC_ECU_COUNT; i++) {
        if (hb_confirmed[i] == TRUE) {
            return TRUE;
        }
    }
    return FALSE;
}

boolean SC_Heartbeat_IsFzcBrakeFault(void)
{
    /* FZC_Heartbeat byte 3 FaultStatus bitmask: bit 1 = brake fault.
     * hb_last_fault_status is populated by SC_Heartbeat_ValidateContent (SWR-SC-024). */
    return ((hb_last_fault_status[SC_ECU_FZC] & 0x02u) != 0u) ? TRUE : FALSE;
}

void SC_Heartbeat_ValidateContent(uint8 ecuIndex, const uint8* payload)
{
    uint8 mode;
    uint8 faults;
    uint8 bit_count;

    if ((ecuIndex >= SC_ECU_COUNT) || (payload == NULL_PTR)) {
        return;
    }

    /* Heartbeat byte 3: bits [3:0] = OperatingMode, bits [7:4] = FaultStatus.
     * Per CAN matrix FZC/CVC/RZC_Heartbeat signal definitions. */
    mode   = payload[3] & 0x0Fu;
    faults = (payload[3] >> 4u) & 0x0Fu;

    hb_last_fault_status[ecuIndex] = faults;

    /* SWR-SC-027: count consecutive receptions in DEGRADED (2) or LIMP (3) */
    if ((mode == 2u) || (mode == 3u)) {
        if (hb_stuck_degraded_cnt[ecuIndex] < 0xFFu) {
            hb_stuck_degraded_cnt[ecuIndex]++;
        }
    } else {
        hb_stuck_degraded_cnt[ecuIndex] = 0u;
    }

    /* SWR-SC-028: count consecutive receptions with >=2 FaultStatus bits set */
    bit_count = 0u;
    bit_count += ((faults & 0x01u) != 0u) ? 1u : 0u;
    bit_count += ((faults & 0x02u) != 0u) ? 1u : 0u;
    bit_count += ((faults & 0x04u) != 0u) ? 1u : 0u;
    bit_count += ((faults & 0x08u) != 0u) ? 1u : 0u;

    if (bit_count >= 2u) {
        if (hb_fault_escalate_cnt[ecuIndex] < 0xFFu) {
            hb_fault_escalate_cnt[ecuIndex]++;
        }
    } else {
        hb_fault_escalate_cnt[ecuIndex] = 0u;
    }

    /* Latch content fault if either threshold exceeded */
    if ((hb_stuck_degraded_cnt[ecuIndex] >= SC_HB_STUCK_DEGRADED_MAX) ||
        (hb_fault_escalate_cnt[ecuIndex] >= SC_HB_FAULT_ESCALATE_MAX)) {
        hb_content_fault[ecuIndex] = TRUE;
    }
}

boolean SC_Heartbeat_IsContentFault(uint8 ecuIndex)
{
    if (ecuIndex >= SC_ECU_COUNT) {
        return FALSE;
    }
    return hb_content_fault[ecuIndex];
}
