/**
 * @file    Swc_DtcStore.c
 * @brief   DTC Store SWC -- in-memory diagnostic trouble code management
 * @date    2026-02-23
 *
 * @safety_req SWR-TCU-008: DTC storage with 64-entry circular buffer
 * @safety_req SWR-TCU-009: Freeze-frame capture, aging, auto-capture
 * @traces_to  TSR-038, TSR-039
 *
 * @standard ISO 14229 DTC storage, AUTOSAR DEM concepts
 * @copyright Taktflow Systems 2026
 */

#include "Swc_DtcStore.h"
#include "Tcu_Cfg.h"

/* ---- BSW Includes ---- */

#include "Rte.h"

/* ---- Internal State ---- */

static DtcStoreEntry_t dtc_entries[DTC_STORE_MAX_ENTRIES];
static uint8  dtc_count;        /**< Current number of stored DTCs */
static uint8  dtc_write_idx;    /**< Next write position (circular) */
static uint32 tick_counter;     /**< Monotonic tick for timestamps  */
static boolean dtc_initialized;

/* ---- Internal: Capture freeze frame from RTE signals ---- */

static void capture_freeze_frame(DtcStoreEntry_t* entry)
{
    uint32 val = 0u;

    (void)Rte_Read(TCU_SIG_VEHICLE_SPEED, &val);
    entry->ff_speed = (uint16)val;

    val = 0u;
    (void)Rte_Read(TCU_SIG_MOTOR_CURRENT, &val);
    entry->ff_current = (uint16)val;

    val = 0u;
    (void)Rte_Read(TCU_SIG_BATTERY_VOLTAGE, &val);
    entry->ff_voltage = (uint16)val;

    val = 0u;
    (void)Rte_Read(TCU_SIG_MOTOR_TEMP, &val);
    entry->ff_temp = (uint8)val;

    entry->ff_timestamp = tick_counter;
}

/* ---- Internal: Find existing DTC by code ---- */

static uint8 find_dtc(uint32 dtcCode)
{
    uint8 i;
    for (i = 0u; i < dtc_count; i++) {
        if (dtc_entries[i].dtcCode == dtcCode) {
            return i;
        }
    }
    return 0xFFu; /* Not found sentinel */
}

/* ---- Public API ---- */

void Swc_DtcStore_Init(void)
{
    uint8 i;
    for (i = 0u; i < DTC_STORE_MAX_ENTRIES; i++) {
        dtc_entries[i].dtcCode      = 0u;
        dtc_entries[i].status       = 0u;
        dtc_entries[i].agingCounter = 0u;
        dtc_entries[i].ff_speed     = 0u;
        dtc_entries[i].ff_current   = 0u;
        dtc_entries[i].ff_voltage   = 0u;
        dtc_entries[i].ff_temp      = 0u;
        dtc_entries[i].ff_timestamp = 0u;
    }
    dtc_count       = 0u;
    dtc_write_idx   = 0u;
    tick_counter    = 0u;
    dtc_initialized = TRUE;
}

void Swc_DtcStore_10ms(void)
{
    if (dtc_initialized == FALSE) {
        return;
    }

    tick_counter++;

    /* Auto-capture from DTC broadcast */
    uint32 broadcast_dtc = 0u;
    (void)Rte_Read(TCU_SIG_DTC_BROADCAST, &broadcast_dtc);

    if (broadcast_dtc != 0u) {
        (void)Swc_DtcStore_Add(broadcast_dtc,
                                DTC_STATUS_TEST_FAILED | DTC_STATUS_CONFIRMED);
    }

    /* Aging: increment counter for DTCs not currently failing */
    uint8 i = 0u;
    while (i < dtc_count) {
        boolean removed = FALSE;

        if ((dtc_entries[i].status & DTC_STATUS_TEST_FAILED) == 0u) {
            if (dtc_entries[i].agingCounter < 0xFFFFu) {
                dtc_entries[i].agingCounter++;
            }
            /* Auto-clear after aging threshold */
            if (dtc_entries[i].agingCounter >= TCU_DTC_AGING_CLEAR_CYCLES) {
                /* Remove by shifting entries down */
                uint8 j;
                for (j = i; j < (dtc_count - 1u); j++) {
                    dtc_entries[j] = dtc_entries[j + 1u];
                }
                dtc_count--;
                if (dtc_write_idx > 0u) {
                    dtc_write_idx--;
                }
                removed = TRUE;
            }
        }

        if (removed == FALSE) {
            i++;
        }
        /* If removed, re-check same index (shifted entry) */
    }
}

uint8 Swc_DtcStore_GetCount(void)
{
    return dtc_count;
}

const DtcStoreEntry_t* Swc_DtcStore_GetByIndex(uint8 index)
{
    if (index >= dtc_count) {
        return NULL_PTR;
    }
    return &dtc_entries[index];
}

void Swc_DtcStore_Clear(void)
{
    uint8 i;
    for (i = 0u; i < DTC_STORE_MAX_ENTRIES; i++) {
        dtc_entries[i].dtcCode      = 0u;
        dtc_entries[i].status       = 0u;
        dtc_entries[i].agingCounter = 0u;
        dtc_entries[i].ff_speed     = 0u;
        dtc_entries[i].ff_current   = 0u;
        dtc_entries[i].ff_voltage   = 0u;
        dtc_entries[i].ff_temp      = 0u;
        dtc_entries[i].ff_timestamp = 0u;
    }
    dtc_count     = 0u;
    dtc_write_idx = 0u;
}

Std_ReturnType Swc_DtcStore_Add(uint32 dtcCode, uint8 status)
{
    if (dtcCode == 0u) {
        return E_NOT_OK;
    }

    /* Check if DTC already exists */
    uint8 existing = find_dtc(dtcCode);
    if (existing != 0xFFu) {
        /* Update status (OR in new bits) */
        dtc_entries[existing].status |= status;
        /* Reset aging counter on re-detection */
        dtc_entries[existing].agingCounter = 0u;
        return E_OK;
    }

    /* Add new entry */
    if (dtc_count < DTC_STORE_MAX_ENTRIES) {
        /* Space available: append */
        dtc_entries[dtc_count].dtcCode      = dtcCode;
        dtc_entries[dtc_count].status       = status;
        dtc_entries[dtc_count].agingCounter = 0u;
        capture_freeze_frame(&dtc_entries[dtc_count]);
        dtc_count++;
        dtc_write_idx = dtc_count;
    } else {
        /* Buffer full: overwrite oldest (circular at dtc_write_idx) */
        if (dtc_write_idx >= DTC_STORE_MAX_ENTRIES) {
            dtc_write_idx = 0u;
        }
        dtc_entries[dtc_write_idx].dtcCode      = dtcCode;
        dtc_entries[dtc_write_idx].status       = status;
        dtc_entries[dtc_write_idx].agingCounter = 0u;
        capture_freeze_frame(&dtc_entries[dtc_write_idx]);
        dtc_write_idx++;
    }

    return E_OK;
}

uint8 Swc_DtcStore_GetByMask(uint8 statusMask, uint32* dtcCodes, uint8 maxCount)
{
    if ((dtcCodes == NULL_PTR) || (maxCount == 0u)) {
        return 0u;
    }

    uint8 found = 0u;
    uint8 i;
    for (i = 0u; i < dtc_count; i++) {
        if ((dtc_entries[i].status & statusMask) != 0u) {
            dtcCodes[found] = dtc_entries[i].dtcCode;
            found++;
            if (found >= maxCount) {
                break;
            }
        }
    }

    return found;
}
