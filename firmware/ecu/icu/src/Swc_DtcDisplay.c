/**
 * @file    Swc_DtcDisplay.c
 * @brief   DTC display — maintains circular buffer of DTCs for ICU
 * @date    2026-02-23
 *
 * @safety_req SWR-ICU-008
 * @traces_to  SSR-ICU-008
 *
 * @details  Listens to DTC broadcast signal (CAN 0x500) via Rte_Read.
 *           Maintains a circular buffer of up to 16 DTC entries.
 *           Each entry stores the 24-bit DTC code, ISO 14229 status
 *           byte, and timestamp.
 *
 *           DTC_Broadcast format (packed in uint32 signal):
 *             bits 31..8 = 24-bit DTC code
 *             bits  7..0 = status byte
 *
 *           Status bits per ISO 14229:
 *             bit 0 = testFailed (active)
 *             bit 2 = pendingDTC
 *             bit 3 = confirmedDTC
 *
 *           ncurses rendering guarded by PLATFORM_POSIX_TEST.
 *
 * @copyright Taktflow Systems 2026
 * @standard  AUTOSAR, ISO 26262 Part 6
 */

#include "Swc_DtcDisplay.h"
#include "Icu_Cfg.h"

/* ==================================================================
 * BSW includes
 * ================================================================== */

#include "Rte.h"

/* ==================================================================
 * Module-local state
 * ================================================================== */

static DtcEntry_t dtc_buffer[DTC_MAX_ENTRIES];  /**< Circular DTC buffer  */
static uint8      dtc_count;                     /**< Current entry count  */
static uint8      dtc_write_idx;                 /**< Next write position  */
static uint32     tick_counter;                  /**< Monotonic tick count */
static uint32     last_broadcast;                /**< Last processed value */
static boolean    initialized;                   /**< Init flag            */

/* ==================================================================
 * Helper: find existing DTC by code
 * Returns index if found, DTC_MAX_ENTRIES if not found
 * ================================================================== */

static uint8 DtcDisplay_FindByCode(uint32 code)
{
    uint8 i;

    for (i = 0u; i < dtc_count; i++) {
        if (dtc_buffer[i].code == code) {
            return i;
        }
    }

    return DTC_MAX_ENTRIES;  /* Not found sentinel */
}

/* ==================================================================
 * Swc_DtcDisplay_Init
 * ================================================================== */

void Swc_DtcDisplay_Init(void)
{
    uint8 i;

    for (i = 0u; i < DTC_MAX_ENTRIES; i++) {
        dtc_buffer[i].code      = 0u;
        dtc_buffer[i].status    = 0u;
        dtc_buffer[i].timestamp = 0u;
    }

    dtc_count      = 0u;
    dtc_write_idx  = 0u;
    tick_counter   = 0u;
    last_broadcast = 0u;
    initialized    = TRUE;
}

/* ==================================================================
 * Swc_DtcDisplay_50ms — called every 50ms
 * ================================================================== */

void Swc_DtcDisplay_50ms(void)
{
    uint32 broadcast_raw = 0u;
    uint32 dtc_code;
    uint8  dtc_status;
    uint8  existing_idx;

    if (initialized != TRUE) {
        return;
    }

    tick_counter++;

    /* Read DTC broadcast signal */
    (void)Rte_Read(ICU_SIG_DTC_BROADCAST, &broadcast_raw);

    /* Only process non-zero broadcasts that differ from last */
    if ((broadcast_raw != 0u) && (broadcast_raw != last_broadcast)) {
        last_broadcast = broadcast_raw;

        /* Extract DTC code (bits 31..8) and status (bits 7..0) */
        dtc_code   = (broadcast_raw >> 8u) & 0x00FFFFFFu;
        dtc_status = (uint8)(broadcast_raw & 0xFFu);

        /* Check if this DTC already exists in the buffer */
        existing_idx = DtcDisplay_FindByCode(dtc_code);

        if (existing_idx < DTC_MAX_ENTRIES) {
            /* Update existing entry */
            dtc_buffer[existing_idx].status    = dtc_status;
            dtc_buffer[existing_idx].timestamp = tick_counter;
        } else if (dtc_count < DTC_MAX_ENTRIES) {
            /* Add new entry — buffer not full */
            dtc_buffer[dtc_count].code      = dtc_code;
            dtc_buffer[dtc_count].status    = dtc_status;
            dtc_buffer[dtc_count].timestamp = tick_counter;
            dtc_count++;
            dtc_write_idx = dtc_count;
        } else {
            /* Buffer full — replace oldest entry (circular) */
            /* Find oldest by lowest timestamp */
            uint8  oldest_idx = 0u;
            uint32 oldest_ts  = dtc_buffer[0u].timestamp;
            uint8  i;

            for (i = 1u; i < DTC_MAX_ENTRIES; i++) {
                if (dtc_buffer[i].timestamp < oldest_ts) {
                    oldest_ts  = dtc_buffer[i].timestamp;
                    oldest_idx = i;
                }
            }

            dtc_buffer[oldest_idx].code      = dtc_code;
            dtc_buffer[oldest_idx].status    = dtc_status;
            dtc_buffer[oldest_idx].timestamp = tick_counter;
        }
    } else if (broadcast_raw == 0u) {
        /* Reset last_broadcast so a re-sent DTC can be processed */
        last_broadcast = 0u;
    } else {
        /* No action — same DTC already processed */
    }

    /* ---- ncurses rendering (excluded from test builds) ---- */
#ifndef PLATFORM_POSIX_TEST
    {
        /* ncurses rendering would go here:
         * - Display DTC count header
         * - For each DTC entry: show code (hex), status flags,
         *   active/pending/confirmed badges
         * - Color-code: active = red, pending = yellow, confirmed = white
         *
         * TODO:HARDWARE — implement ncurses rendering when
         * Docker container environment is set up
         */
    }
#endif /* PLATFORM_POSIX_TEST */
}

/* ==================================================================
 * Swc_DtcDisplay_GetCount
 * ================================================================== */

uint8 Swc_DtcDisplay_GetCount(void)
{
    return dtc_count;
}

/* ==================================================================
 * Swc_DtcDisplay_GetEntry
 * ================================================================== */

const DtcEntry_t* Swc_DtcDisplay_GetEntry(uint8 index)
{
    if (index >= dtc_count) {
        return NULL_PTR;
    }

    return &dtc_buffer[index];
}

/* ==================================================================
 * Swc_DtcDisplay_ClearAll
 * ================================================================== */

void Swc_DtcDisplay_ClearAll(void)
{
    uint8 i;

    for (i = 0u; i < DTC_MAX_ENTRIES; i++) {
        dtc_buffer[i].code      = 0u;
        dtc_buffer[i].status    = 0u;
        dtc_buffer[i].timestamp = 0u;
    }

    dtc_count      = 0u;
    dtc_write_idx  = 0u;
    last_broadcast = 0u;
}
