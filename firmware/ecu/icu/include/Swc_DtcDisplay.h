/**
 * @file    Swc_DtcDisplay.h
 * @brief   DTC display SWC — maintains and renders DTC list for ICU
 * @date    2026-02-23
 *
 * @safety_req SWR-ICU-008
 * @traces_to  SSR-ICU-008
 *
 * @note    QM safety level — display-only, no safety impact.
 *          Receives DTC broadcasts from CAN 0x500 and maintains a
 *          circular buffer of up to 16 entries.
 *
 * @copyright Taktflow Systems 2026
 * @standard  AUTOSAR, ISO 26262 Part 6
 */
#ifndef SWC_DTCDISPLAY_H
#define SWC_DTCDISPLAY_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

#define DTC_MAX_ENTRIES  16u

/* ==================================================================
 * Types
 * ================================================================== */

typedef struct {
    uint32  code;      /**< 24-bit DTC code                     */
    uint8   status;    /**< ISO 14229 status byte               */
    uint32  timestamp; /**< Tick count when received/updated     */
} DtcEntry_t;

/* ==================================================================
 * Public API
 * ================================================================== */

/**
 * @brief  Initialize DTC display — clear all entries
 */
void Swc_DtcDisplay_Init(void);

/**
 * @brief  DTC display cyclic main function — called every 50ms
 *
 * Reads DTC broadcast signal from RTE. If non-zero, extracts
 * DTC code and status. Updates existing entry or adds new.
 * Renders DTC list via ncurses (guarded by PLATFORM_POSIX_TEST).
 */
void Swc_DtcDisplay_50ms(void);

/**
 * @brief  Get current number of DTCs in buffer
 * @return Count of active DTC entries (0 to DTC_MAX_ENTRIES)
 */
uint8 Swc_DtcDisplay_GetCount(void);

/**
 * @brief  Get DTC entry by index
 * @param  index  Entry index (0 to count-1)
 * @return Pointer to DTC entry, or NULL_PTR if index >= count
 */
const DtcEntry_t* Swc_DtcDisplay_GetEntry(uint8 index);

/**
 * @brief  Clear all DTC entries from the display buffer
 */
void Swc_DtcDisplay_ClearAll(void);

#endif /* SWC_DTCDISPLAY_H */
