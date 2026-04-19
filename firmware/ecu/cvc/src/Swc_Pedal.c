/**
 * @file    Swc_Pedal.c
 * @brief   Dual pedal sensor processing — plausibility, stuck detect, torque mapping
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-001 to SWR-CVC-008
 * @traces_to  SSR-CVC-001 to SSR-CVC-008, TSR-030, TSR-031
 *
 * @details  Implements the pedal processing SWC for the CVC:
 *           1. Reads dual AS5048A pedal angle sensors via IoHwAb
 *           2. Checks plausibility (|S1-S2| with debounce)
 *           3. Detects stuck sensor (no change for N cycles)
 *           4. Calculates average position scaled 0-1000
 *           5. Looks up torque from 16-entry table
 *           6. Applies ramp rate limiting
 *           7. Applies vehicle-state mode limits
 *           8. Enforces zero-torque latch on any fault
 *           9. Reports DTCs and writes signals to RTE
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Pedal.h"
#include "Cvc_Cfg.h"
#include "IoHwAb.h"
#include "Rte.h"
#include "Dem.h"
#include "Com.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** 14-bit sensor maximum value (AS5048A) */
#define PEDAL_SENSOR_MAX        16383u

/** Scaled position range */
#define PEDAL_POSITION_MAX      1000u

/** Number of entries in torque lookup table */
#define TORQUE_LUT_SIZE         16u

/** Torque lookup table: maps position index to torque output.
 *  Index = position * (TORQUE_LUT_SIZE - 1) / PEDAL_POSITION_MAX
 *  Entries provide linear mapping with dead zone at low end.
 *
 *  position:   0   67  133  200  267  333  400  467  533  600  667  733  800  867  933  1000
 *  torque:     0    0   33  100  200  300  400  467  533  600  667  733  800  867  933  1000
 */
static const uint16 Pedal_TorqueLut[TORQUE_LUT_SIZE] = {
      0u,    0u,   33u,  100u,  200u,  300u,  400u,  467u,
    533u,  600u,  667u,  733u,  800u,  867u,  933u, 1000u
};

/* ==================================================================
 * Module State (all static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8                         Pedal_Initialized;
static const Swc_Pedal_ConfigType*   Pedal_CfgPtr;

/* Raw sensor values */
static uint16  Pedal_Raw1;
static uint16  Pedal_Raw2;

/* Calculated position 0-1000 */
static uint16  Pedal_Position;

/* Current fault code */
static uint8   Pedal_Fault;

/* Previous raw1 for stuck detection */
static uint16  Pedal_PrevRaw1;

/* Plausibility debounce counter */
static uint8   Pedal_PlausDebounce;

/* Stuck detection counter */
static uint16  Pedal_StuckCounter;

/* Stuck detection: first-cycle initialization flag */
static uint8   Pedal_StuckReady;

/* Stuck detection: one-shot flag (prevents re-trigger at same value) */
static uint8   Pedal_StuckFired;

/* Zero-torque latch */
static uint8   Pedal_FaultLatched;
static uint8   Pedal_LatchCounter;

/* Previous torque output for ramp limiting */
static uint16  Pedal_PrevTorque;

/* ==================================================================
 * Private Helper: Absolute difference of uint16 values
 * ================================================================== */

/**
 * @brief  Compute |a - b| for uint16 values without signed overflow
 * @param  a  First value
 * @param  b  Second value
 * @return Absolute difference
 */
static uint16 Pedal_AbsDiff16(uint16 a, uint16 b)
{
    if (a >= b) {
        return (uint16)(a - b);
    }
    return (uint16)(b - a);
}

/* ==================================================================
 * Private Helper: Torque Lookup
 * ================================================================== */

/**
 * @brief  Look up torque from the 16-entry table with linear interpolation
 * @param  position  Pedal position 0-1000
 * @return Torque value 0-1000
 */
static uint16 Pedal_LookupTorque(uint16 position)
{
    uint16 idx;
    uint16 frac;
    uint16 span;
    uint16 lower;
    uint16 upper;
    uint32 interp;

    if (position >= PEDAL_POSITION_MAX) {
        return Pedal_TorqueLut[TORQUE_LUT_SIZE - 1u];
    }

    /* Calculate table index and fractional position */
    /* idx = position * 15 / 1000 */
    idx = (uint16)((uint32)position * (uint32)(TORQUE_LUT_SIZE - 1u) / (uint32)PEDAL_POSITION_MAX);

    if (idx >= (TORQUE_LUT_SIZE - 1u)) {
        return Pedal_TorqueLut[TORQUE_LUT_SIZE - 1u];
    }

    /* Span between table entries: 1000 / 15 = ~66.67 */
    span = (uint16)((uint32)PEDAL_POSITION_MAX / (uint32)(TORQUE_LUT_SIZE - 1u));

    /* Fractional position within this span */
    frac = position - (uint16)((uint32)idx * (uint32)span);

    lower = Pedal_TorqueLut[idx];
    upper = Pedal_TorqueLut[idx + 1u];

    /* Linear interpolation: lower + (upper - lower) * frac / span */
    if (upper >= lower) {
        interp = (uint32)lower + (((uint32)(upper - lower) * (uint32)frac) / (uint32)span);
    } else {
        interp = (uint32)lower - (((uint32)(lower - upper) * (uint32)frac) / (uint32)span);
    }

    if (interp > (uint32)PEDAL_POSITION_MAX) {
        interp = (uint32)PEDAL_POSITION_MAX;
    }

    return (uint16)interp;
}

/* ==================================================================
 * Private Helper: Apply Ramp Limit
 * ================================================================== */

/**
 * @brief  Limit the rate of torque change per cycle
 * @param  target  Desired torque value
 * @return Ramp-limited torque value
 */
static uint16 Pedal_ApplyRamp(uint16 target)
{
    uint16 limited;
    uint16 ramp;

    if (Pedal_CfgPtr == NULL_PTR) {
        return 0u;
    }

    ramp = Pedal_CfgPtr->rampLimit;

    if (target > Pedal_PrevTorque) {
        /* Increasing: cap the increase */
        if ((target - Pedal_PrevTorque) > ramp) {
            limited = Pedal_PrevTorque + ramp;
        } else {
            limited = target;
        }
    } else {
        /* Decreasing: allow immediate decrease for safety */
        limited = target;
    }

    return limited;
}

/* ==================================================================
 * Private Helper: Get Mode Limit
 * ================================================================== */

/**
 * @brief  Return the maximum allowed torque for the current vehicle state
 * @param  state  Vehicle state from RTE
 * @return Maximum torque 0-1000
 */
static uint16 Pedal_GetModeLimit(uint32 state)
{
    uint16 limit;

    switch (state) {
        case CVC_STATE_RUN:
            limit = CVC_PEDAL_MAX_RUN;
            break;
        case CVC_STATE_DEGRADED:
            limit = CVC_PEDAL_MAX_DEGRADED;
            break;
        case CVC_STATE_LIMP:
            limit = CVC_PEDAL_MAX_LIMP;
            break;
        case CVC_STATE_SAFE_STOP:
            /* FALLTHROUGH */
        case CVC_STATE_SHUTDOWN:
            /* FALLTHROUGH */
        case CVC_STATE_INIT:
            /* FALLTHROUGH */
        default:
            limit = 0u;
            break;
    }

    return limit;
}

/* ==================================================================
 * API: Swc_Pedal_Init
 * ================================================================== */

void Swc_Pedal_Init(const Swc_Pedal_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Pedal_Initialized = FALSE;
        Pedal_CfgPtr      = NULL_PTR;
        return;
    }

    Pedal_CfgPtr        = ConfigPtr;

    Pedal_Raw1          = 0u;
    Pedal_Raw2          = 0u;
    Pedal_Position      = 0u;
    Pedal_Fault         = CVC_PEDAL_NO_FAULT;
    Pedal_PrevRaw1      = 0u;
    Pedal_PlausDebounce = 0u;
    Pedal_StuckCounter  = 0u;
    Pedal_StuckReady    = FALSE;
    Pedal_StuckFired    = FALSE;
    Pedal_FaultLatched  = FALSE;
    Pedal_LatchCounter  = 0u;
    Pedal_PrevTorque    = 0u;

    Pedal_Initialized   = TRUE;
}

/* ==================================================================
 * API: Swc_Pedal_MainFunction (10ms cyclic)
 * ================================================================== */

void Swc_Pedal_MainFunction(void)
{
    Std_ReturnType ret1;
    Std_ReturnType ret2;
    uint16 raw1_local;
    uint16 raw2_local;
    uint16 delta;
    uint16 avg;
    uint16 position;
    uint16 torque;
    uint16 mode_limit;
    uint32 vehicle_state;
    uint8  new_fault;

    if (Pedal_Initialized != TRUE) {
        return;
    }

    if (Pedal_CfgPtr == NULL_PTR) {
        return;
    }

    new_fault   = CVC_PEDAL_NO_FAULT;
    raw1_local  = 0u;
    raw2_local  = 0u;

    /* ----------------------------------------------------------
     * Step 1: Read both sensors via IoHwAb
     * ---------------------------------------------------------- */
    ret1 = IoHwAb_ReadPedalAngle(0u, &raw1_local);
    ret2 = IoHwAb_ReadPedalAngle(1u, &raw2_local);

    /* ----------------------------------------------------------
     * Step 2: Check SPI failures (immediate — no debounce)
     * ---------------------------------------------------------- */
    if (ret1 != E_OK) {
        new_fault = CVC_PEDAL_SENSOR1_FAIL;
    } else if (ret2 != E_OK) {
        new_fault = CVC_PEDAL_SENSOR2_FAIL;
    } else {
        /* Both sensors OK — no fault */
    }

    /* Store raw values */
    Pedal_Raw1 = raw1_local;
    Pedal_Raw2 = raw2_local;

    /* ----------------------------------------------------------
     * Step 3: Plausibility check (only if both sensors read OK)
     * ---------------------------------------------------------- */
    if (new_fault == CVC_PEDAL_NO_FAULT) {
        delta = Pedal_AbsDiff16(raw1_local, raw2_local);

        if (delta >= Pedal_CfgPtr->plausThreshold) {
            Pedal_PlausDebounce++;
            if (Pedal_PlausDebounce >= Pedal_CfgPtr->plausDebounce) {
                new_fault = CVC_PEDAL_PLAUSIBILITY;
            }
        } else {
            Pedal_PlausDebounce = 0u;
        }
    }

    /* ----------------------------------------------------------
     * Step 4: Stuck detection (only if no other fault)
     *
     * First-cycle initialization: on the very first valid reading
     * after Init, set PrevRaw1 to the current value so that cycle 1
     * counts toward the stuck window (delta == 0).
     *
     * One-shot: once stuck fires, the StuckFired flag prevents
     * re-triggering until the sensor value actually changes.
     * This allows the zero-torque latch to clear and torque to
     * recover after a transient stuck condition.
     * ---------------------------------------------------------- */
    if (new_fault == CVC_PEDAL_NO_FAULT) {
        /* First valid reading: seed PrevRaw1 so first cycle counts */
        if (Pedal_StuckReady == FALSE) {
            Pedal_PrevRaw1  = raw1_local;
            Pedal_StuckReady = TRUE;
        }

        delta = Pedal_AbsDiff16(raw1_local, Pedal_PrevRaw1);

        if (delta < Pedal_CfgPtr->stuckThreshold) {
            if (Pedal_StuckFired == FALSE) {
                Pedal_StuckCounter++;
                if (Pedal_StuckCounter >= Pedal_CfgPtr->stuckCycles) {
                    new_fault = CVC_PEDAL_STUCK;
                    Pedal_StuckFired = TRUE;
                }
            }
            /* If StuckFired: sensor still stuck but don't re-trigger */
        } else {
            /* Sensor value changed: reset stuck window */
            Pedal_StuckCounter = 0u;
            Pedal_StuckFired   = FALSE;
        }
    }

    /* Save previous raw1 for next cycle's stuck detection */
    Pedal_PrevRaw1 = raw1_local;

    /* ----------------------------------------------------------
     * Step 5: Calculate position (only if sensors are OK)
     *         position = (raw1 + raw2) / 2, scaled from 0-16383 to 0-1000
     * ---------------------------------------------------------- */
    if ((ret1 == E_OK) && (ret2 == E_OK)) {
        avg = (uint16)(((uint32)raw1_local + (uint32)raw2_local) / 2u);
        position = (uint16)(((uint32)avg * (uint32)PEDAL_POSITION_MAX) / (uint32)PEDAL_SENSOR_MAX);
        if (position > PEDAL_POSITION_MAX) {
            position = PEDAL_POSITION_MAX;
        }
    } else {
        position = 0u;
    }

    Pedal_Position = position;

    /* ----------------------------------------------------------
     * Step 6: Torque lookup
     * ---------------------------------------------------------- */
    torque = Pedal_LookupTorque(position);

    /* ----------------------------------------------------------
     * Step 7: Apply ramp limit
     * ---------------------------------------------------------- */
    torque = Pedal_ApplyRamp(torque);

    /* Save ramp-limited torque BEFORE mode limit and fault handling.
     * This preserves the ramp state through fault/latch periods so
     * that torque can resume ramping from the correct baseline after
     * the zero-torque latch clears. */
    Pedal_PrevTorque = torque;

    /* ----------------------------------------------------------
     * Step 8: Read vehicle state and apply mode limit
     * ---------------------------------------------------------- */
    vehicle_state = CVC_STATE_INIT;
    (void)Rte_Read(CVC_SIG_VEHICLE_STATE, &vehicle_state);

    mode_limit = Pedal_GetModeLimit(vehicle_state);

    if (torque > mode_limit) {
        torque = mode_limit;
    }

    /* ----------------------------------------------------------
     * Step 9: Fault handling — zero-torque latch
     *
     * SPI and plausibility faults zero torque immediately on the
     * detection cycle. Stuck faults activate the latch but do NOT
     * zero torque on the detection cycle itself — the latch takes
     * effect starting from the next cycle. This ensures the torque
     * output remains valid on the cycle the stuck condition is first
     * detected, while still entering the zero-torque safe state.
     *
     * The fault code (Pedal_Fault) persists during the latch period
     * so that diagnostic readers see the original fault until the
     * latch clears after sufficient fault-free cycles.
     * ---------------------------------------------------------- */
    if (new_fault != CVC_PEDAL_NO_FAULT) {
        /* New fault detected this cycle */
        Pedal_Fault = new_fault;

        if (Pedal_FaultLatched == FALSE) {
            /* First fault — activate latch */
            Pedal_FaultLatched = TRUE;
            Pedal_LatchCounter = 0u;
        } else {
            /* Fault during active latch — restart latch counter */
            Pedal_LatchCounter = 0u;
        }

        /* Zero torque immediately for all faults EXCEPT stuck.
         * Stuck activates the latch (torque zeroed next cycle)
         * but does not zero torque on the detection cycle itself. */
        if (new_fault != CVC_PEDAL_STUCK) {
            torque = 0u;
        }
    } else if (Pedal_FaultLatched == TRUE) {
        /* No new fault but latch still active: count fault-free cycles.
         * Pedal_Fault retains the previous fault code so diagnostics
         * can identify which fault caused the latch. */
        Pedal_LatchCounter++;
        if (Pedal_LatchCounter >= Pedal_CfgPtr->latchClearCycles) {
            /* Latch cleared after sufficient fault-free cycles */
            Pedal_FaultLatched = FALSE;
            Pedal_LatchCounter = 0u;
            Pedal_Fault        = CVC_PEDAL_NO_FAULT;
        } else {
            /* Latch still active: force zero torque */
            torque = 0u;
        }
    } else {
        /* No fault, no latch — normal operation */
        Pedal_Fault = CVC_PEDAL_NO_FAULT;
    }

    /* ----------------------------------------------------------
     * Step 10: Write signals to RTE
     * ---------------------------------------------------------- */
    (void)Rte_Write(CVC_SIG_PEDAL_POSITION, (uint32)Pedal_Position);
    (void)Rte_Write(CVC_SIG_PEDAL_FAULT, (uint32)Pedal_Fault);
    /* CVC_SIG_TORQUE_REQUEST aliases the Com TX slot for
     * Torque_Request.Command_pct, which is an 8-bit 0..100 percentage
     * on the DBC. Write the scaled percentage directly so Com's
     * auto-pull doesn't truncate the internal 0..1000 value. */
    {
        uint32 torque_pct_scaled = (uint32)torque / 10u;
        if (torque_pct_scaled > 100u) {
            torque_pct_scaled = 100u;
        }
        (void)Rte_Write(CVC_SIG_TORQUE_REQUEST, torque_pct_scaled);
    }
    /* Direction: 1=Forward when torque is being requested, 0=Neutral otherwise.
     * Without this, plant-sim (and any downstream RZC direction check) sees
     * direction=0 and keeps the motor disabled even with a valid torque
     * command — the signal was configured in Com_Cfg but never written. */
    (void)Rte_Write(CVC_SIG_TORQUE_REQUEST_DIRECTION,
                    (uint32)((torque > 0u) ? 1u : 0u));

    /* Torque TX: Rte_Write above → Swc_CvcCom reads + sends via Com */

    /* ----------------------------------------------------------
     * Step 11: Report DTCs via Dem
     * ---------------------------------------------------------- */
    if (new_fault == CVC_PEDAL_PLAUSIBILITY) {
        Dem_ReportErrorStatus(CVC_DTC_PEDAL_PLAUSIBILITY, DEM_EVENT_STATUS_FAILED);
    } else if (Pedal_FaultLatched == FALSE) {
        Dem_ReportErrorStatus(CVC_DTC_PEDAL_PLAUSIBILITY, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Latch still active but no new plausibility fault — no report change */
    }

    if (new_fault == CVC_PEDAL_STUCK) {
        Dem_ReportErrorStatus(CVC_DTC_PEDAL_STUCK, DEM_EVENT_STATUS_FAILED);
    } else if (Pedal_FaultLatched == FALSE) {
        Dem_ReportErrorStatus(CVC_DTC_PEDAL_STUCK, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Latch still active — no change */
    }

    if (new_fault == CVC_PEDAL_SENSOR1_FAIL) {
        Dem_ReportErrorStatus(CVC_DTC_PEDAL_SENSOR1_FAIL, DEM_EVENT_STATUS_FAILED);
    } else if (Pedal_FaultLatched == FALSE) {
        Dem_ReportErrorStatus(CVC_DTC_PEDAL_SENSOR1_FAIL, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Latch still active — no change */
    }

    if (new_fault == CVC_PEDAL_SENSOR2_FAIL) {
        Dem_ReportErrorStatus(CVC_DTC_PEDAL_SENSOR2_FAIL, DEM_EVENT_STATUS_FAILED);
    } else if (Pedal_FaultLatched == FALSE) {
        Dem_ReportErrorStatus(CVC_DTC_PEDAL_SENSOR2_FAIL, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Latch still active — no change */
    }
}

/* ==================================================================
 * API: Swc_Pedal_GetPosition
 * ================================================================== */

Std_ReturnType Swc_Pedal_GetPosition(uint8* pos)
{
    if (Pedal_Initialized != TRUE) {
        return E_NOT_OK;
    }

    if (pos == NULL_PTR) {
        return E_NOT_OK;
    }

    /* Scale from 0-1000 to 0-100 percentage */
    *pos = (uint8)(Pedal_Position / 10u);

    return E_OK;
}
