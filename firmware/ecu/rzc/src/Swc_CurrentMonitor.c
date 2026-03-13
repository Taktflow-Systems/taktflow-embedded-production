/**
 * @file    Swc_CurrentMonitor.c
 * @brief   RZC current monitor — 1kHz ACS723 sampling, 4-sample moving average,
 *          overcurrent detection with debounce, motor disable, DTC, recovery,
 *          10ms CAN broadcast
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-005, SWR-RZC-006, SWR-RZC-007, SWR-RZC-008
 * @traces_to  SSR-RZC-005, SSR-RZC-006, SSR-RZC-007, SSR-RZC-008
 *
 * @details  Implements the RZC motor current monitoring SWC (ASIL A):
 *           1. Init: zero-cal with 64 samples, validate within 2048+-200
 *           2. 1ms cyclic: read motor current via IoHwAb, 4-sample moving avg
 *           3. Overcurrent: >25A for 10 consecutive samples -> disable motor,
 *              report DTC, set RTE flag
 *           4. Recovery: 500ms below threshold clears overcurrent
 *           5. CAN broadcast every 10ms: 8-byte payload with current, direction,
 *              enable, alive counter, CRC placeholder
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_CurrentMonitor.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "IoHwAb.h"
#include "Rte.h"
#include "Dem.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8   CM_Initialized;
static uint16  CM_ZeroOffset;
static uint8   CM_ZeroCalDone;

/** 4-sample moving average */
static uint16  CM_AvgBuffer[RZC_CURRENT_AVG_WINDOW];
static uint8   CM_AvgIndex;
static uint8   CM_AvgCount;

/** Overcurrent detection */
static uint8   CM_OcDebounceCount;
static uint8   CM_OvercurrentActive;

/** Recovery */
static uint16  CM_RecoveryCycles;


/* ==================================================================
 * Internal: Compute moving average
 * ================================================================== */

static uint16 CM_ComputeAverage(void)
{
    uint32 sum;
    uint8  count;
    uint8  i;

    sum   = 0u;
    count = CM_AvgCount;

    if (count == 0u) {
        return 0u;
    }

    if (count > RZC_CURRENT_AVG_WINDOW) {
        count = RZC_CURRENT_AVG_WINDOW;
    }

    for (i = 0u; i < count; i++) {
        sum += (uint32)CM_AvgBuffer[i];
    }

    return (uint16)(sum / (uint32)count);
}

/* ==================================================================
 * Internal: Disable motor outputs
 * ================================================================== */

static void CM_DisableMotor(void)
{
    Dio_WriteChannel(RZC_MOTOR_R_EN_CHANNEL, 0u);
    Dio_WriteChannel(RZC_MOTOR_L_EN_CHANNEL, 0u);
}

/* ==================================================================
 * API: Swc_CurrentMonitor_Init
 * ================================================================== */

void Swc_CurrentMonitor_Init(void)
{
    uint32 sum;
    uint16 raw_mA;
    uint16 avg;
    uint8  i;

    /* Zero all state */
    CM_Initialized       = FALSE;
    CM_ZeroOffset        = 0u;
    CM_ZeroCalDone       = FALSE;
    CM_AvgIndex          = 0u;
    CM_AvgCount          = 0u;
    CM_OcDebounceCount   = 0u;
    CM_OvercurrentActive = FALSE;
    CM_RecoveryCycles    = 0u;

    for (i = 0u; i < RZC_CURRENT_AVG_WINDOW; i++) {
        CM_AvgBuffer[i] = 0u;
    }

    /* ----------------------------------------------------------
     * Zero-cal: read 64 samples, compute average, validate range
     * ---------------------------------------------------------- */
    sum = 0u;
    for (i = 0u; i < RZC_CURRENT_ZEROCAL_SAMPLES; i++) {
        raw_mA = 0u;
        (void)IoHwAb_ReadMotorCurrent(&raw_mA);
        sum += (uint32)raw_mA;
    }

    avg = (uint16)(sum / (uint32)RZC_CURRENT_ZEROCAL_SAMPLES);

    /* Validate within center +/- range */
    if ((avg >= (RZC_CURRENT_ZEROCAL_CENTER - RZC_CURRENT_ZEROCAL_RANGE)) &&
        (avg <= (RZC_CURRENT_ZEROCAL_CENTER + RZC_CURRENT_ZEROCAL_RANGE))) {
        CM_ZeroOffset  = avg;
        CM_ZeroCalDone = TRUE;
    } else {
        /* Out of range: report DTC, zero-cal failed */
        Dem_ReportErrorStatus((uint8)RZC_DTC_ZERO_CAL, DEM_EVENT_STATUS_FAILED);
        CM_ZeroCalDone = FALSE;
    }

    CM_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_CurrentMonitor_MainFunction (1ms cyclic)
 * ================================================================== */

void Swc_CurrentMonitor_MainFunction(void)
{
    uint16 raw_mA;
    uint16 avg_mA;

    /* -------------------------------------------------------
     * Guard: not initialized -> return immediately (safe)
     * ------------------------------------------------------- */
    if (CM_Initialized != TRUE) {
        return;
    }

    /* -------------------------------------------------------
     * Step 1: Read ADC via IoHwAb
     * IoHwAb_ReadMotorCurrent already returns milliamps.
     * Zero-cal validates the sensor at init; the mA value
     * is used directly for overcurrent comparison.
     * ------------------------------------------------------- */
    raw_mA = 0u;
    (void)IoHwAb_ReadMotorCurrent(&raw_mA);

    /* -------------------------------------------------------
     * Step 2: Add to 4-sample moving average buffer
     * ------------------------------------------------------- */
    CM_AvgBuffer[CM_AvgIndex] = raw_mA;
    CM_AvgIndex++;
    if (CM_AvgIndex >= RZC_CURRENT_AVG_WINDOW) {
        CM_AvgIndex = 0u;
    }
    if (CM_AvgCount < RZC_CURRENT_AVG_WINDOW) {
        CM_AvgCount++;
    }

    /* -------------------------------------------------------
     * Step 3: Compute average current
     * ------------------------------------------------------- */
    avg_mA = CM_ComputeAverage();

    /* -------------------------------------------------------
     * Step 4: Overcurrent check
     * If average > 25000mA, increment debounce counter.
     * If counter >= 10 -> overcurrent condition.
     * ------------------------------------------------------- */
    if (avg_mA > RZC_CURRENT_OC_THRESH_MA) {
        if (CM_OvercurrentActive == FALSE) {
            CM_OcDebounceCount++;
            if (CM_OcDebounceCount >= RZC_CURRENT_OC_DEBOUNCE) {
                /* Overcurrent confirmed */
                CM_OvercurrentActive = TRUE;
                CM_RecoveryCycles    = 0u;

                /* Disable motor: set R_EN and L_EN LOW */
                CM_DisableMotor();

                /* Report DTC */
                Dem_ReportErrorStatus((uint8)RZC_DTC_OVERCURRENT,
                                     DEM_EVENT_STATUS_FAILED);
            }
        } else {
            /* Already in overcurrent — any spike resets recovery counter */
            CM_RecoveryCycles = 0u;

            /* Keep reporting to Dem so debounce counter reaches confirm
             * threshold (DEM_DEBOUNCE_FAIL_THRESHOLD=3). */
            Dem_ReportErrorStatus((uint8)RZC_DTC_OVERCURRENT,
                                 DEM_EVENT_STATUS_FAILED);
        }
    } else {
        /* Current is below threshold */
        if (CM_OvercurrentActive == FALSE) {
            /* Normal operation — reset debounce counter */
            CM_OcDebounceCount = 0u;
        } else {
            /* -------------------------------------------------------
             * Step 5: Recovery logic
             * If current < threshold while overcurrent is active,
             * increment recovery counter. 500 consecutive cycles
             * below threshold clears the overcurrent condition.
             * A single raw reading above threshold resets recovery
             * even if the average remains below threshold.
             * ------------------------------------------------------- */
            if (raw_mA > RZC_CURRENT_OC_THRESH_MA) {
                CM_RecoveryCycles = 0u;
            } else {
                CM_RecoveryCycles++;
                if (CM_RecoveryCycles >= RZC_CURRENT_RECOVERY_MS) {
                    CM_OvercurrentActive = FALSE;
                    CM_OcDebounceCount   = 0u;
                    CM_RecoveryCycles    = 0u;
                }
            }
        }
    }

    /* -------------------------------------------------------
     * Step 6: Write current_mA and overcurrent flag to RTE
     * ------------------------------------------------------- */
    (void)Rte_Write(RZC_SIG_CURRENT_MA, (uint32)avg_mA);
    (void)Rte_Write(RZC_SIG_OVERCURRENT,
                    (CM_OvercurrentActive == TRUE) ? 1u : 0u);

    /* CAN TX handled by Swc_RzcCom (reads RTE signals, sends via Com) */
}
