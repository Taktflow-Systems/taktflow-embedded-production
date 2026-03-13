/**
 * @file    Swc_DataAggregator.c
 * @brief   TCU CAN data aggregation — cache latest values with timeout
 * @date    2026-02-24
 *
 * @safety_req SWR-TCU-014
 * @traces_to  SSR-TCU-014, TSR-038, TSR-039
 *
 * @details  Caches the latest CAN data from 6 sources:
 *           - Vehicle State (0x100), 500ms timeout
 *           - Motor Current (0x301), 500ms timeout
 *           - Motor Temperature (0x302), 2000ms timeout
 *           - Battery Voltage (0x303), 2000ms timeout
 *           - DTC Broadcast (0x500), 2000ms timeout
 *           - CVC Heartbeat (0x010), 200ms timeout
 *
 *           Each source has an independent timeout. Stale data is flagged
 *           but still accessible (last known value). A new update clears
 *           the stale flag and resets the timeout timer.
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#include "Swc_DataAggregator.h"

/* MISRA 20.1: All #include directives must precede code/declarations.
 * POSIX header needed by the real (non-mock) Docker-simulated TCU ECU. */
#ifndef TCU_DATA_USE_MOCK
/* cppcheck-suppress misra-c2012-21.10
 * Deviation: time.h is required for clock_gettime() in the Docker-simulated
 * TCU ECU (POSIX simulation, not safety-critical firmware). */
#include <time.h>
#endif /* !TCU_DATA_USE_MOCK */

/* ====================================================================
 * Platform Abstraction (real or mock)
 * ==================================================================== */

#ifdef TCU_DATA_USE_MOCK

extern uint32 mock_get_tick_ms(void);
#define GET_TICK_MS()  mock_get_tick_ms()

#else

static uint32 real_get_tick_ms(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32)((ts.tv_sec * 1000u) + (ts.tv_nsec / 1000000u));
}
#define GET_TICK_MS()  real_get_tick_ms()

#endif /* TCU_DATA_USE_MOCK */

/* ====================================================================
 * Internal Constants
 * ==================================================================== */

#ifndef TCU_TIMEOUT_VEHICLE_STATE_MS
#define TCU_TIMEOUT_VEHICLE_STATE_MS   500u
#endif
#ifndef TCU_TIMEOUT_MOTOR_CURRENT_MS
#define TCU_TIMEOUT_MOTOR_CURRENT_MS   500u
#endif
#ifndef TCU_TIMEOUT_MOTOR_TEMP_MS
#define TCU_TIMEOUT_MOTOR_TEMP_MS      2000u
#endif
#ifndef TCU_TIMEOUT_BATTERY_MS
#define TCU_TIMEOUT_BATTERY_MS         2000u
#endif
#ifndef TCU_TIMEOUT_DTC_BCAST_MS
#define TCU_TIMEOUT_DTC_BCAST_MS       2000u
#endif
#ifndef TCU_TIMEOUT_HB_CVC_MS
#define TCU_TIMEOUT_HB_CVC_MS          200u
#endif

#define TCU_CAN_DLC  8u

/* ====================================================================
 * Per-source timeout configuration
 * ==================================================================== */

static const uint32 source_timeout_ms[TCU_DATA_SOURCE_COUNT] = {
    TCU_TIMEOUT_VEHICLE_STATE_MS,
    TCU_TIMEOUT_MOTOR_CURRENT_MS,
    TCU_TIMEOUT_MOTOR_TEMP_MS,
    TCU_TIMEOUT_BATTERY_MS,
    TCU_TIMEOUT_DTC_BCAST_MS,
    TCU_TIMEOUT_HB_CVC_MS
};

/* ====================================================================
 * Module State
 * ==================================================================== */

/** Cached CAN data per source */
static uint8   data_cache[TCU_DATA_SOURCE_COUNT][TCU_CAN_DLC];

/** Timestamp of last update per source (ms) */
static uint32  data_timestamp[TCU_DATA_SOURCE_COUNT];

/** Data-ever-received flag */
static boolean data_received[TCU_DATA_SOURCE_COUNT];

/** Stale flag per source */
static boolean data_stale[TCU_DATA_SOURCE_COUNT];

/* ====================================================================
 * Public API
 * ==================================================================== */

void TCU_Data_Init(void)
{
    uint8 src;
    uint8 i;

    for (src = 0u; src < TCU_DATA_SOURCE_COUNT; src++) {
        for (i = 0u; i < TCU_CAN_DLC; i++) {
            data_cache[src][i] = 0u;
        }
        data_timestamp[src] = 0u;
        data_received[src]  = FALSE;
        data_stale[src]     = FALSE;
    }
}

Std_ReturnType TCU_Data_Update(uint8 sourceId, const uint8* data, uint8 dlc)
{
    uint8 i;
    uint8 copy_len;

    if (sourceId >= TCU_DATA_SOURCE_COUNT) {
        return E_NOT_OK;
    }

    if (data == NULL_PTR) {
        return E_NOT_OK;
    }

    /* Copy data (up to 8 bytes) */
    copy_len = (dlc <= TCU_CAN_DLC) ? dlc : TCU_CAN_DLC;
    for (i = 0u; i < copy_len; i++) {
        data_cache[sourceId][i] = data[i];
    }
    /* Zero-fill remainder */
    for (i = copy_len; i < TCU_CAN_DLC; i++) {
        data_cache[sourceId][i] = 0u;
    }

    data_timestamp[sourceId] = GET_TICK_MS();
    data_received[sourceId]  = TRUE;
    data_stale[sourceId]     = FALSE;

    return E_OK;
}

void TCU_Data_CheckTimeouts(void)
{
    uint8  src;
    uint32 now_ms = GET_TICK_MS();
    uint32 elapsed;

    for (src = 0u; src < TCU_DATA_SOURCE_COUNT; src++) {
        if (data_received[src] == TRUE) {
            elapsed = now_ms - data_timestamp[src];
            if (elapsed > source_timeout_ms[src]) {
                data_stale[src] = TRUE;
            }
        }
    }
}

Std_ReturnType TCU_Data_Get(uint8 sourceId, uint8* data, boolean* isStale)
{
    uint8 i;

    if ((sourceId >= TCU_DATA_SOURCE_COUNT) ||
        (data == NULL_PTR) || (isStale == NULL_PTR)) {
        return E_NOT_OK;
    }

    if (data_received[sourceId] == FALSE) {
        return E_NOT_OK;
    }

    for (i = 0u; i < TCU_CAN_DLC; i++) {
        data[i] = data_cache[sourceId][i];
    }
    *isStale = data_stale[sourceId];

    return E_OK;
}
