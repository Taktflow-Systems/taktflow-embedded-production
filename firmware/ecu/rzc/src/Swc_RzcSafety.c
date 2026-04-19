/**
 * @file    Swc_RzcSafety.c
 * @brief   RZC local safety monitoring -- watchdog feed, fault aggregation, CAN bus loss
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-023, SWR-RZC-024
 * @traces_to  SSR-RZC-023, SSR-RZC-024, TSR-046
 *
 * @details  Implements the RZC safety monitoring SWC:
 *           1. Watchdog feed (TPS3823 WDI toggle on PB4) with 4-condition check
 *           2. Fault aggregation from overcurrent, overtemp, direction, stall,
 *              battery, self-test, e-stop signals
 *           3. CAN bus loss detection: bus-off, silence (200ms), error warning (500ms)
 *           4. Motor disable on CAN loss: R_EN and L_EN driven LOW
 *           5. CAN loss latch: once latched, stays latched (requires power cycle)
 *           6. Combined fault bitmask written to RTE
 *           7. DTC reporting for watchdog failures
 *
 *           Watchdog feed conditions (all must be true):
 *           - No critical fault (overcurrent, overtemp, direction, CAN)
 *           - Vehicle state is not SHUTDOWN
 *           - Self-test passed (if completed)
 *           - CAN not bus-off
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_RzcSafety.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"
#include "IoHwAb.h"
#include "Dem.h"

/* ==================================================================
 * BSW Includes (additional)
 * ================================================================== */

#include "Can.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** DIO output levels */
#define DIO_LEVEL_LOW       0u
#define DIO_LEVEL_HIGH      1u

/** Safety status codes */
#define SAFETY_STATUS_OK        0u
#define SAFETY_STATUS_DEGRADED  1u
#define SAFETY_STATUS_FAULT     2u

/** CAN controller error state values */
#define CAN_ERRORSTATE_ACTIVE   0u
#define CAN_ERRORSTATE_WARNING  1u
#define CAN_ERRORSTATE_BUSOFF   2u

/** CAN silence threshold in 10ms cycles: 200ms / 10ms = 20 */
#define CAN_SILENCE_CYCLES      20u

/** CAN error warning threshold in 10ms cycles: 500ms / 10ms = 50 */
/** CAN error warning threshold in 10ms cycles.
 *  Production: 50 (500ms). SIL: 500 (5s) to avoid racing E2E DTCs. */
#ifdef SIL_DIAG
#define CAN_ERR_WARN_CYCLES    500u
#else
#define CAN_ERR_WARN_CYCLES     50u
#endif

/** CAN controller ID (only one CAN controller on RZC) */
#define CAN_CONTROLLER_0        0u

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8   Safety_Initialized;
static uint8   Safety_WdiToggle;          /* Alternates 0/1 for WDI pin */
static uint8   Safety_Status;
static uint8   Safety_CanLossLatched;     /* Once set, stays set until power cycle */
static uint8   Safety_CanSilenceCounter;  /* Incremented each cycle, reset by NotifyCanRx */
static uint16  Safety_CanErrWarnCounter;  /* Incremented while in error warning state */
static uint8   Safety_WdgFailLatched;     /* Edge-report RZC_DTC_WATCHDOG_FAIL once per entry */

/* ==================================================================
 * API: Swc_RzcSafety_Init
 * ================================================================== */

void Swc_RzcSafety_Init(void)
{
    Safety_WdiToggle         = 0u;
    Safety_WdgFailLatched    = FALSE;
    Safety_Status            = SAFETY_STATUS_OK;
    Safety_CanLossLatched    = FALSE;
    Safety_CanSilenceCounter = 0u;
    Safety_CanErrWarnCounter = 0u;
    Safety_Initialized       = TRUE;
}

/* ==================================================================
 * API: Swc_RzcSafety_MainFunction (10ms cyclic)
 * ================================================================== */

void Swc_RzcSafety_MainFunction(void)
{
    uint32 overcurrent;
    uint32 overtemp;
    uint32 direction_fault;
    uint32 stall_fault;
    uint32 battery_fault;
    uint32 self_test_result;
    uint32 estop_active;
    uint32 vehicle_state;
    uint8  can_error_state;
    uint8  fault_mask;
    uint8  wdg_feed_ok;
    uint8  can_fault;

    if (Safety_Initialized != TRUE) {
        return;
    }

    /* ----------------------------------------------------------
     * Step 1: Read fault signals from RTE
     * ---------------------------------------------------------- */
    overcurrent     = 0u;
    overtemp        = 0u;
    direction_fault = 0u;
    stall_fault     = 0u;
    battery_fault   = 0u;
    self_test_result = RZC_SELF_TEST_PASS;
    estop_active    = 0u;
    vehicle_state   = RZC_STATE_INIT;

    (void)Rte_Read(RZC_SIG_OVERCURRENT, &overcurrent);
    (void)Rte_Read(RZC_SIG_TEMP_FAULT, &overtemp);
    (void)Rte_Read(RZC_SIG_ENCODER_DIR, &direction_fault);
    (void)Rte_Read(RZC_SIG_ENCODER_STALL, &stall_fault);
    (void)Rte_Read(RZC_SIG_BATTERY_STATUS, &battery_fault);
    (void)Rte_Read(RZC_SIG_SELF_TEST_RESULT, &self_test_result);
    (void)Rte_Read(RZC_SIG_ESTOP_ACTIVE, &estop_active);
    (void)Rte_Read(RZC_SIG_VEHICLE_STATE, &vehicle_state);

    /* ----------------------------------------------------------
     * Step 2: Aggregate fault bitmask
     * ---------------------------------------------------------- */
    fault_mask = RZC_FAULT_NONE;

    if (overcurrent != 0u) {
        fault_mask |= RZC_FAULT_OVERCURRENT;
    }

    if (overtemp != 0u) {
        fault_mask |= RZC_FAULT_OVERTEMP;
    }

    if (direction_fault != 0u) {
        fault_mask |= RZC_FAULT_DIRECTION;
    }

    if (stall_fault != 0u) {
        fault_mask |= RZC_FAULT_STALL;
    }

    if (battery_fault != 0u) {
        fault_mask |= RZC_FAULT_BATTERY;
    }

    if (self_test_result == RZC_SELF_TEST_FAIL) {
        fault_mask |= RZC_FAULT_SELF_TEST;
    }

    /* ----------------------------------------------------------
     * Step 3: CAN bus loss detection
     * ---------------------------------------------------------- */
    can_fault = FALSE;
    can_error_state = CAN_ERRORSTATE_ACTIVE;
    (void)Can_GetControllerErrorState(CAN_CONTROLLER_0, &can_error_state);

    /* 3a. Bus-off detection */
    if (can_error_state == CAN_ERRORSTATE_BUSOFF) {
        can_fault = TRUE;
        Safety_CanLossLatched = TRUE;
    }

    /* 3b. CAN silence detection (no new RX for 200ms) */
    Safety_CanSilenceCounter++;
    if (Safety_CanSilenceCounter >= CAN_SILENCE_CYCLES) {
        can_fault = TRUE;
        Safety_CanLossLatched = TRUE;
    }

    /* 3c. Error warning state > 500ms */
    if (can_error_state == CAN_ERRORSTATE_WARNING) {
        Safety_CanErrWarnCounter++;
        if (Safety_CanErrWarnCounter >= CAN_ERR_WARN_CYCLES) {
            can_fault = TRUE;
            Safety_CanLossLatched = TRUE;
        }
    } else {
        Safety_CanErrWarnCounter = 0u;
    }

    /* 5. CAN loss latch: once latched, stays latched */
    if (Safety_CanLossLatched == TRUE) {
        can_fault = TRUE;
    }

    if (can_fault == TRUE) {
        fault_mask |= RZC_FAULT_CAN;
    }

    /* 4. Motor disable on CAN loss */
    if (can_fault == TRUE) {
        Dio_WriteChannel(RZC_MOTOR_R_EN_CHANNEL, DIO_LEVEL_LOW);
        Dio_WriteChannel(RZC_MOTOR_L_EN_CHANNEL, DIO_LEVEL_LOW);
    }

    /* ----------------------------------------------------------
     * Step 4: Determine safety status
     *   Critical: overcurrent, overtemp, direction, CAN, estop -> FAULT
     *   Non-critical: stall, battery, self-test -> DEGRADED
     *   None -> OK
     * ---------------------------------------------------------- */
    if ((fault_mask & (RZC_FAULT_OVERCURRENT | RZC_FAULT_OVERTEMP |
                       RZC_FAULT_DIRECTION | RZC_FAULT_CAN)) != 0u) {
        Safety_Status = SAFETY_STATUS_FAULT;
    } else if (estop_active != 0u) {
        /* E-stop is a critical safety event -> FAULT */
        Safety_Status = SAFETY_STATUS_FAULT;
    } else if (fault_mask != RZC_FAULT_NONE) {
        Safety_Status = SAFETY_STATUS_DEGRADED;
    } else {
        Safety_Status = SAFETY_STATUS_OK;
    }

    /* ----------------------------------------------------------
     * Step 5: Watchdog feed -- 4 conditions must all be true
     * ---------------------------------------------------------- */
    wdg_feed_ok = TRUE;

    /* Condition 1: No critical fault */
    if ((fault_mask & (RZC_FAULT_OVERCURRENT | RZC_FAULT_OVERTEMP |
                       RZC_FAULT_DIRECTION | RZC_FAULT_CAN)) != 0u) {
        wdg_feed_ok = FALSE;
    }

    /* Condition 2: Vehicle not in SHUTDOWN */
    if (vehicle_state == RZC_STATE_SHUTDOWN) {
        wdg_feed_ok = FALSE;
    }

    /* Condition 3: Self-test passed (or not yet run) */
    if (self_test_result == RZC_SELF_TEST_FAIL) {
        wdg_feed_ok = FALSE;
    }

    /* Condition 4: CAN not bus-off */
    if (can_error_state == CAN_ERRORSTATE_BUSOFF) {
        wdg_feed_ok = FALSE;
    }

    if (wdg_feed_ok == TRUE) {
        /* Toggle WDI pin */
        Safety_WdiToggle = (uint8)(Safety_WdiToggle ^ 1u);
        Dio_WriteChannel(RZC_SAFETY_WDI_CHANNEL, Safety_WdiToggle);
        Safety_WdgFailLatched = FALSE;
    } else {
        /* Do not feed watchdog -- will trigger HW reset.
         * Report DTC once per edge: continuous reporting at 10ms produces a
         * flood of 0x00E802 broadcasts on 0x500 (tens of Hz) that drown
         * other DTCs in the MQTT rate-limiter on the gateway side. The
         * watchdog fault is latched; a single edge report per fault entry
         * is sufficient for diagnostics. */
        if (Safety_WdgFailLatched == FALSE) {
            Dem_ReportErrorStatus(RZC_DTC_WATCHDOG_FAIL, DEM_EVENT_STATUS_FAILED);
            Safety_WdgFailLatched = TRUE;
        }
        fault_mask |= RZC_FAULT_WATCHDOG;
    }

    /* ----------------------------------------------------------
     * Step 6: Write outputs to RTE
     * ---------------------------------------------------------- */
    (void)Rte_Write(RZC_SIG_FAULT_MASK, (uint32)fault_mask);
    (void)Rte_Write(RZC_SIG_SAFETY_STATUS, (uint32)Safety_Status);
}

/* ==================================================================
 * API: Swc_RzcSafety_GetStatus
 * ================================================================== */

uint8 Swc_RzcSafety_GetStatus(void)
{
    return Safety_Status;
}

/* ==================================================================
 * API: Swc_RzcSafety_NotifyCanRx
 * ================================================================== */

void Swc_RzcSafety_NotifyCanRx(void)
{
    Safety_CanSilenceCounter = 0u;
}
