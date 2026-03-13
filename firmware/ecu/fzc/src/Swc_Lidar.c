/**
 * @file    Swc_Lidar.c
 * @brief   TFMini-S lidar obstacle detection — frame parse, zones, fault handling
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-013 to SWR-FZC-016
 * @traces_to  SSR-FZC-013 to SSR-FZC-016, TSR-030, TSR-031
 *
 * @details  Implements the lidar processing SWC for the FZC:
 *           1. Reads TFMini-S UART frames via UART MCAL driver
 *           2. Validates 9-byte frame: header (0x59 0x59), checksum
 *           3. Extracts distance (cm) and signal strength
 *           4. Plausibility: range (2-1200cm), signal (>=100), stuck detect
 *           5. Classifies into graduated response zones
 *           6. Fault safe: 0cm on any fault, zone = FAULT
 *           7. Reports DTCs and writes signals to RTE
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Lidar.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Uart.h"
#include "Rte.h"
#include "Com.h"
#include "Dem.h"

/* ==================================================================
 * Module State (all static file-scope — ASIL C: no dynamic memory)
 * ================================================================== */

static uint8                          Lidar_Initialized;
static const Swc_Lidar_ConfigType*    Lidar_CfgPtr;

/* Parsed values */
static uint16  Lidar_Distance;
static uint16  Lidar_Signal;
static uint8   Lidar_Zone;
static uint8   Lidar_Fault;

/* Timeout monitoring */
static uint16  Lidar_TimeoutCounter;

/* Stuck detection */
static uint16  Lidar_PrevDistance;
static uint16  Lidar_StuckCounter;

/* Persistent fault counter for degradation request */
static uint16  Lidar_PersistentFaultCounter;

/* Checksum error flag — set by ParseFrame, consumed by MainFunction */
static uint8   Lidar_ChecksumError;

/* Frame buffer */
static uint8   Lidar_FrameBuf[FZC_LIDAR_FRAME_SIZE];

/* ==================================================================
 * Private Helper: Validate and Parse TFMini-S Frame
 * ================================================================== */

/**
 * @brief  Attempt to read and parse a TFMini-S frame from UART
 * @param  dist_out      Output: distance in cm
 * @param  strength_out  Output: signal strength
 * @return E_OK if valid frame parsed, E_NOT_OK otherwise
 */
static Std_ReturnType Lidar_ParseFrame(uint16* dist_out, uint16* strength_out)
{
    uint8 bytes_read;
    uint8 checksum;
    uint8 i;
    Std_ReturnType ret;

    if ((dist_out == NULL_PTR) || (strength_out == NULL_PTR)) {
        return E_NOT_OK;
    }

    /* Read from UART */
    bytes_read = 0u;
    ret = Uart_ReadRxData(Lidar_FrameBuf, FZC_LIDAR_FRAME_SIZE, &bytes_read);

    if (ret != E_OK) {
        return E_NOT_OK;
    }

    /* Need exactly 9 bytes for a complete frame */
    if (bytes_read < FZC_LIDAR_FRAME_SIZE) {
        return E_NOT_OK;
    }

    /* Validate header bytes */
    if ((Lidar_FrameBuf[0] != FZC_LIDAR_HEADER_BYTE) ||
        (Lidar_FrameBuf[1] != FZC_LIDAR_HEADER_BYTE)) {
        return E_NOT_OK;
    }

    /* Validate checksum: low byte of sum of bytes 0-7 */
    checksum = 0u;
    for (i = 0u; i < 8u; i++) {
        checksum = (uint8)(checksum + Lidar_FrameBuf[i]);
    }

    if (checksum != Lidar_FrameBuf[8]) {
        Lidar_ChecksumError = TRUE;
        Dem_ReportErrorStatus(FZC_DTC_LIDAR_CHECKSUM, DEM_EVENT_STATUS_FAILED);
        return E_NOT_OK;
    }

    /* Extract distance (little-endian, bytes 2-3) */
    *dist_out = (uint16)((uint16)Lidar_FrameBuf[2] |
                         ((uint16)Lidar_FrameBuf[3] << 8u));

    /* Extract signal strength (little-endian, bytes 4-5) */
    *strength_out = (uint16)((uint16)Lidar_FrameBuf[4] |
                             ((uint16)Lidar_FrameBuf[5] << 8u));

    return E_OK;
}

/* ==================================================================
 * Private Helper: Classify Distance into Zone
 * ================================================================== */

static uint8 Lidar_ClassifyZone(uint16 distance)
{
    if (Lidar_CfgPtr == NULL_PTR) {
        return FZC_LIDAR_ZONE_FAULT;
    }

    if (distance <= Lidar_CfgPtr->emergencyDistCm) {
        return FZC_LIDAR_ZONE_EMERGENCY;
    }

    if (distance <= Lidar_CfgPtr->brakeDistCm) {
        return FZC_LIDAR_ZONE_BRAKING;
    }

    if (distance <= Lidar_CfgPtr->warnDistCm) {
        return FZC_LIDAR_ZONE_WARNING;
    }

    return FZC_LIDAR_ZONE_CLEAR;
}

/* ==================================================================
 * API: Swc_Lidar_Init
 * ================================================================== */

void Swc_Lidar_Init(const Swc_Lidar_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Lidar_Initialized = FALSE;
        Lidar_CfgPtr      = NULL_PTR;
        return;
    }

    Lidar_CfgPtr               = ConfigPtr;
    Lidar_Distance             = 0u;
    Lidar_Signal               = 0u;
    Lidar_Zone                 = FZC_LIDAR_ZONE_FAULT;
    Lidar_Fault                = 0u;
    Lidar_TimeoutCounter       = 0u;
    Lidar_PrevDistance          = 0u;
    Lidar_StuckCounter         = 0u;
    Lidar_PersistentFaultCounter = 0u;
    Lidar_ChecksumError          = FALSE;

    Lidar_Initialized          = TRUE;
}

/* ==================================================================
 * API: Swc_Lidar_MainFunction (10ms cyclic)
 * ================================================================== */

void Swc_Lidar_MainFunction(void)
{
    uint16 raw_dist;
    uint16 raw_signal;
    Std_ReturnType parse_result;
    uint8  new_fault;
    uint8  frame_received;

    if (Lidar_Initialized != TRUE) {
        return;
    }

    if (Lidar_CfgPtr == NULL_PTR) {
        return;
    }

    new_fault      = 0u;
    frame_received = FALSE;
    Lidar_ChecksumError = FALSE;

    /* ----------------------------------------------------------
     * Step 1: Attempt to parse a UART frame
     * ---------------------------------------------------------- */
    raw_dist   = 0u;
    raw_signal = 0u;
    parse_result = Lidar_ParseFrame(&raw_dist, &raw_signal);

    if (parse_result == E_OK) {
        frame_received = TRUE;
        Lidar_TimeoutCounter = 0u;
    } else {
#if defined(PLATFORM_POSIX) && !defined(UNIT_TEST)
        /* SIL: No physical TFMini-S sensor, UART POSIX stub returns 0
         * bytes every cycle.  Inject a synthetic "clear" reading with
         * small variation to avoid stuck detection (50 identical readings).
         * NOTE: This guard is transparent on bare metal (compiled out).
         * NOTE: Guard excludes UNIT_TEST — tests exercise real parse paths. */
        {
            static uint16 sil_lidar_counter = 0u;
            /* Oscillate ±2 cm around 500 to defeat stuck detection */
            raw_dist = 500u + (sil_lidar_counter % 5u) - 2u;
            sil_lidar_counter++;
        }
        raw_signal     = 500u;   /* Good signal strength             */
        parse_result   = E_OK;
        frame_received = TRUE;
        Lidar_TimeoutCounter = 0u;
#else
        /* No valid frame — increment timeout */
        Lidar_TimeoutCounter++;

        /* Checksum error is an immediate fault (corrupt data received) */
        if (Lidar_ChecksumError == TRUE) {
            new_fault = 1u;
        }
#endif /* defined(PLATFORM_POSIX) && !defined(UNIT_TEST) */
    }

    /* ----------------------------------------------------------
     * Step 2: Timeout check
     * ---------------------------------------------------------- */
    if (Lidar_TimeoutCounter >= (Lidar_CfgPtr->timeoutMs)) {
        new_fault = 1u;
        Dem_ReportErrorStatus(FZC_DTC_LIDAR_TIMEOUT, DEM_EVENT_STATUS_FAILED);
        Lidar_TimeoutCounter = Lidar_CfgPtr->timeoutMs; /* Clamp */
    }

    /* ----------------------------------------------------------
     * Step 3: Plausibility checks (only on valid frame)
     * ---------------------------------------------------------- */
    if ((frame_received == TRUE) && (new_fault == 0u)) {
        /* Range check */
        if ((raw_dist < Lidar_CfgPtr->rangeMinCm) ||
            (raw_dist > Lidar_CfgPtr->rangeMaxCm)) {
            new_fault = 1u;
        }

        /* Signal strength check */
        if (raw_signal < Lidar_CfgPtr->signalMin) {
            new_fault = 1u;
            Dem_ReportErrorStatus(FZC_DTC_LIDAR_SIGNAL_LOW, DEM_EVENT_STATUS_FAILED);
        }

        /* Stuck detection: first cycle establishes baseline, so the
         * counter reaches (stuckCycles - 1) after stuckCycles total
         * identical readings.  Trigger at (stuckCycles - 1). */
        if (new_fault == 0u) {
            if (raw_dist == Lidar_PrevDistance) {
                Lidar_StuckCounter++;
                if (Lidar_StuckCounter >= (Lidar_CfgPtr->stuckCycles - 1u)) {
                    new_fault = 1u;
                    Dem_ReportErrorStatus(FZC_DTC_LIDAR_STUCK, DEM_EVENT_STATUS_FAILED);
                }
            } else {
                Lidar_StuckCounter = 0u;
            }
            Lidar_PrevDistance = raw_dist;
        }
    }

    /* ----------------------------------------------------------
     * Step 4: Update outputs based on fault status
     * ---------------------------------------------------------- */
    Lidar_Fault = new_fault;

    if (new_fault != 0u) {
        /* Fault safe default: 0cm, zone = FAULT */
        Lidar_Distance = 0u;
        Lidar_Signal   = 0u;
        Lidar_Zone     = FZC_LIDAR_ZONE_FAULT;

        /* Track persistent faults for degradation */
        Lidar_PersistentFaultCounter++;
    } else if (frame_received == TRUE) {
        /* Valid frame, no fault */
        Lidar_Distance = raw_dist;
        Lidar_Signal   = raw_signal;
        Lidar_Zone     = Lidar_ClassifyZone(raw_dist);

        /* Reset persistent fault counter on good data */
        Lidar_PersistentFaultCounter = 0u;

        /* Clear DTCs */
        Dem_ReportErrorStatus(FZC_DTC_LIDAR_TIMEOUT, DEM_EVENT_STATUS_PASSED);
        Dem_ReportErrorStatus(FZC_DTC_LIDAR_CHECKSUM, DEM_EVENT_STATUS_PASSED);
        Dem_ReportErrorStatus(FZC_DTC_LIDAR_STUCK, DEM_EVENT_STATUS_PASSED);
        Dem_ReportErrorStatus(FZC_DTC_LIDAR_SIGNAL_LOW, DEM_EVENT_STATUS_PASSED);
    } else {
        /* No frame, no fault (yet) — keep previous values */
    }

    /* ----------------------------------------------------------
     * Step 5: Write signals to RTE
     * ---------------------------------------------------------- */
    (void)Rte_Write(FZC_SIG_LIDAR_DIST, (uint32)Lidar_Distance);
    (void)Rte_Write(FZC_SIG_LIDAR_SIGNAL, (uint32)Lidar_Signal);
    (void)Rte_Write(FZC_SIG_LIDAR_ZONE, (uint32)Lidar_Zone);
    (void)Rte_Write(FZC_SIG_LIDAR_FAULT, (uint32)Lidar_Fault);
}

/* ==================================================================
 * API: Swc_Lidar_GetDistance
 * ================================================================== */

Std_ReturnType Swc_Lidar_GetDistance(uint16* dist)
{
    if (Lidar_Initialized != TRUE) {
        return E_NOT_OK;
    }

    if (dist == NULL_PTR) {
        return E_NOT_OK;
    }

    *dist = Lidar_Distance;
    return E_OK;
}
