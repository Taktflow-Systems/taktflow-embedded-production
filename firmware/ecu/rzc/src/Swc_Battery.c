/**
 * @file    Swc_Battery.c
 * @brief   RZC battery voltage monitoring -- 4-sample average, hysteresis, CAN TX
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-017, SWR-RZC-018
 * @traces_to  SSR-RZC-017, SSR-RZC-018, TSR-038
 *
 * @details  Implements the RZC battery monitoring SWC:
 *           1. Reads battery voltage via IoHwAb (voltage divider ADC)
 *           2. 4-sample moving average for noise reduction
 *           3. Threshold-based status: DISABLE_LOW / WARN_LOW / NORMAL /
 *              WARN_HIGH / DISABLE_HIGH
 *           4. Hysteresis on recovery: +500mV from threshold to clear fault
 *           5. DTC reporting on DISABLE states
 *           6. CAN broadcast: [voltage_hi, voltage_lo, status, alive, 0,0,0,0]
 *           7. RTE signals: RZC_SIG_BATTERY_MV, RZC_SIG_BATTERY_STATUS
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Battery.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "IoHwAb.h"
#include "Rte.h"
#include "Dem.h"

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8   Batt_Initialized;
static uint16  Batt_Voltage_mV;
static uint8   Batt_Status;
static uint8   Batt_Soc;
static uint16  Batt_AvgBuffer[RZC_BATT_AVG_WINDOW];
static uint8   Batt_AvgIndex;

/* ==================================================================
 * Internal: Compute average from buffer
 * ================================================================== */

static uint16 Batt_ComputeAverage(void)
{
    uint32 sum;
    uint8  i;

    sum = 0u;
    for (i = 0u; i < RZC_BATT_AVG_WINDOW; i++) {
        sum += (uint32)Batt_AvgBuffer[i];
    }

    return (uint16)(sum / (uint32)RZC_BATT_AVG_WINDOW);
}

/* ==================================================================
 * Internal: Determine status with hysteresis
 * ================================================================== */

static uint8 Batt_DetermineStatus(uint16 avg_mV, uint8 prev_status)
{
    uint8 new_status;

    /* --- Evaluate from low to high --- */

    if (avg_mV < RZC_BATT_DISABLE_LOW_MV) {
        /* Below absolute low disable threshold */
        new_status = RZC_BATT_STATUS_DISABLE_LOW;
    } else if (avg_mV < RZC_BATT_WARN_LOW_MV) {
        /* Between disable-low and warn-low */
        new_status = RZC_BATT_STATUS_WARN_LOW;
    } else if (avg_mV < RZC_BATT_WARN_HIGH_MV) {
        /* Normal operating range */
        new_status = RZC_BATT_STATUS_NORMAL;
    } else if (avg_mV < RZC_BATT_DISABLE_HIGH_MV) {
        /* Between warn-high and disable-high */
        new_status = RZC_BATT_STATUS_WARN_HIGH;
    } else {
        /* Above absolute high disable threshold */
        new_status = RZC_BATT_STATUS_DISABLE_HIGH;
    }

    /* --- Apply hysteresis on recovery --- */

    /* To recover from DISABLE_LOW, voltage must exceed threshold + hysteresis */
    if (prev_status == RZC_BATT_STATUS_DISABLE_LOW) {
        if (avg_mV < (RZC_BATT_DISABLE_LOW_MV + RZC_BATT_HYSTERESIS_MV)) {
            new_status = RZC_BATT_STATUS_DISABLE_LOW;
        }
    }

    /* To recover from WARN_LOW, voltage must exceed threshold + hysteresis */
    if (prev_status == RZC_BATT_STATUS_WARN_LOW) {
        if ((new_status == RZC_BATT_STATUS_NORMAL) &&
            (avg_mV < (RZC_BATT_WARN_LOW_MV + RZC_BATT_HYSTERESIS_MV))) {
            new_status = RZC_BATT_STATUS_WARN_LOW;
        }
    }

    /* To recover from DISABLE_HIGH, voltage must drop below threshold - hysteresis */
    if (prev_status == RZC_BATT_STATUS_DISABLE_HIGH) {
        if (avg_mV >= (RZC_BATT_DISABLE_HIGH_MV - RZC_BATT_HYSTERESIS_MV)) {
            new_status = RZC_BATT_STATUS_DISABLE_HIGH;
        }
    }

    /* To recover from WARN_HIGH, voltage must drop below threshold - hysteresis */
    if (prev_status == RZC_BATT_STATUS_WARN_HIGH) {
        if ((new_status == RZC_BATT_STATUS_NORMAL) &&
            (avg_mV >= (RZC_BATT_WARN_HIGH_MV - RZC_BATT_HYSTERESIS_MV))) {
            new_status = RZC_BATT_STATUS_WARN_HIGH;
        }
    }

    return new_status;
}

/* ==================================================================
 * API: Swc_Battery_Init
 * ================================================================== */

void Swc_Battery_Init(void)
{
    uint8 i;

    Batt_Voltage_mV    = RZC_BATT_NOMINAL_MV;
    Batt_Status        = RZC_BATT_STATUS_NORMAL;
    Batt_AvgIndex      = 0u;
    for (i = 0u; i < RZC_BATT_AVG_WINDOW; i++) {
        Batt_AvgBuffer[i] = RZC_BATT_NOMINAL_MV;
    }

    Batt_Soc           = 100u;
    Batt_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_Battery_MainFunction (100ms cyclic)
 * ================================================================== */

void Swc_Battery_MainFunction(void)
{
    uint16 raw_voltage;
    uint16 avg_voltage;

    if (Batt_Initialized != TRUE) {
        return;
    }

    /* ----------------------------------------------------------
     * Step 1: Read voltage via IoHwAb, add to 4-sample average
     * ---------------------------------------------------------- */
    raw_voltage = 0u;
    (void)IoHwAb_ReadBatteryVoltage(&raw_voltage);

    Batt_AvgBuffer[Batt_AvgIndex] = raw_voltage;
    Batt_AvgIndex++;
    if (Batt_AvgIndex >= RZC_BATT_AVG_WINDOW) {
        Batt_AvgIndex = 0u;
    }

    avg_voltage = Batt_ComputeAverage();
    Batt_Voltage_mV = avg_voltage;

    /* ----------------------------------------------------------
     * Step 2: Determine status with thresholds and hysteresis
     * ---------------------------------------------------------- */
    Batt_Status = Batt_DetermineStatus(avg_voltage, Batt_Status);

    /* ----------------------------------------------------------
     * Step 3: Report DTC on DISABLE states
     * ---------------------------------------------------------- */
    if ((Batt_Status == RZC_BATT_STATUS_DISABLE_LOW) ||
        (Batt_Status == RZC_BATT_STATUS_DISABLE_HIGH)) {
        Dem_ReportErrorStatus(RZC_DTC_BATTERY, DEM_EVENT_STATUS_FAILED);
    }

    /* ----------------------------------------------------------
     * Step 4: Compute SOC from voltage (linear map, monotonic)
     *         12600 mV = 100%, 10500 mV = 0%, clamped [0,100]
     * ---------------------------------------------------------- */
    {
        uint8 target_soc;
        if (avg_voltage >= 12600u) {
            target_soc = 100u;
        } else if (avg_voltage <= 10500u) {
            target_soc = 0u;
        } else {
            target_soc = (uint8)(((uint32)(avg_voltage - 10500u) * 100u) / 2100u);
        }

        /* Monotonic decrease guard — SOC can only go down, never up
         * (unless re-initialized via Swc_Battery_Init on power cycle). */
        if (target_soc < Batt_Soc) {
            Batt_Soc = target_soc;
        }
    }

    /* ----------------------------------------------------------
     * Step 5: Write RTE signals
     * ---------------------------------------------------------- */
    (void)Rte_Write(RZC_SIG_BATTERY_MV, (uint32)Batt_Voltage_mV);
    (void)Rte_Write(RZC_SIG_BATTERY_STATUS, (uint32)Batt_Status);
    (void)Rte_Write(RZC_SIG_BATTERY_SOC, (uint32)Batt_Soc);

    /* CAN TX handled by Swc_RzcCom (reads RTE signals, sends via Com) */
}
