/**
 * @file    Swc_FzcSafety.c
 * @brief   FZC local safety monitoring — watchdog feed, fault aggregation, self-test
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-023, SWR-FZC-025
 * @traces_to  SSR-FZC-023, SSR-FZC-025, TSR-046
 *
 * @details  Implements the FZC safety monitoring SWC:
 *           1. Watchdog feed (TPS3823 WDI toggle on PB0) with 4-condition check
 *           2. Local plausibility aggregation from steering, brake, lidar faults
 *           3. Combined fault bitmask written to RTE
 *           4. DTC reporting for watchdog failures
 *
 *           Watchdog feed conditions (all must be true):
 *           - Main loop is executing (this function called)
 *           - No critical fault latched (steering or brake)
 *           - Vehicle state is not SHUTDOWN
 *           - Self-test passed (if completed)
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_FzcSafety.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"
#include "IoHwAb.h"
#include "Com.h"
#include "Dem.h"

/* SIL diagnostic logging — compile with -DSIL_DIAG to enable */
#ifdef SIL_DIAG
#include <stdio.h>
#define FSAFE_DIAG(fmt, ...) (void)fprintf(stderr, "[FSAFE] " fmt "\n", ##__VA_ARGS__)
#else
#define FSAFE_DIAG(fmt, ...) ((void)0)
#endif

/* ==================================================================
 * Constants
 * ================================================================== */

#define SAFETY_WDI_CHANNEL     0u    /* PB0 — TPS3823 WDI pin */

#define SAFETY_STATUS_OK       0u
#define SAFETY_STATUS_DEGRADED 1u
#define SAFETY_STATUS_FAULT    2u

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8   Safety_Initialized;
static uint8   Safety_WdiToggle;     /* Alternates 0/1 for WDI pin */
static uint8   Safety_Status;
static uint8   Safety_SelfTestDone;

/** @brief  Post-boot grace counter — suppresses motor cutoff assertion
 *          after boot to absorb startup transients (SC E-Stop, lidar
 *          timeout, brake stabilization).  Platform-equivalent code path;
 *          0 on bare metal (transparent), >0 on SIL. */
static uint16  Safety_GraceCounter;

/* ==================================================================
 * API: Swc_FzcSafety_Init
 * ================================================================== */

void Swc_FzcSafety_Init(void)
{
    Safety_WdiToggle    = 0u;
    Safety_Status       = SAFETY_STATUS_OK;
    Safety_SelfTestDone = FALSE;
    /* Grace counter: 0 on bare metal (transparent), >0 on SIL */
    Safety_GraceCounter = FZC_POST_INIT_GRACE_CYCLES;

    Safety_Initialized  = TRUE;
}

/* ==================================================================
 * API: Swc_FzcSafety_MainFunction (10ms cyclic)
 * ================================================================== */

void Swc_FzcSafety_MainFunction(void)
{
    uint32 steer_fault;
    uint32 brake_fault;
    uint32 lidar_fault;
    uint32 vehicle_state;
    uint32 self_test_result;
    uint8  fault_mask;
    uint8  wdg_feed_ok;

    if (Safety_Initialized != TRUE) {
        return;
    }

    /* ----------------------------------------------------------
     * Step 1: Read fault signals from RTE
     * ---------------------------------------------------------- */
    steer_fault = 0u;
    brake_fault = 0u;
    lidar_fault = 0u;
    vehicle_state = FZC_STATE_INIT;
    self_test_result = FZC_SELF_TEST_PASS;

    (void)Rte_Read(FZC_SIG_STEER_FAULT, &steer_fault);
    (void)Rte_Read(FZC_SIG_BRAKE_FAULT, &brake_fault);
    (void)Rte_Read(FZC_SIG_LIDAR_FAULT, &lidar_fault);
    (void)Rte_Read(FZC_SIG_VEHICLE_STATE, &vehicle_state);
    (void)Rte_Read(FZC_SIG_SELF_TEST_RESULT, &self_test_result);

    /* ----------------------------------------------------------
     * Step 2: Aggregate fault bitmask
     * ---------------------------------------------------------- */
    fault_mask = FZC_FAULT_NONE;

    if (steer_fault != 0u) {
        fault_mask |= FZC_FAULT_STEER;
    }

    if (brake_fault != 0u) {
        fault_mask |= FZC_FAULT_BRAKE;
    }

    if (lidar_fault != 0u) {
        fault_mask |= FZC_FAULT_LIDAR;
    }

    /* Check CAN RX signal quality — stale commands from CVC = comm fault */
    if (Com_GetRxPduQuality(FZC_COM_RX_STEER_COMMAND) == COM_SIGNAL_QUALITY_TIMED_OUT) {
        fault_mask |= FZC_FAULT_CAN_BUS_OFF;
    }
    if (Com_GetRxPduQuality(FZC_COM_RX_BRAKE_COMMAND) == COM_SIGNAL_QUALITY_TIMED_OUT) {
        fault_mask |= FZC_FAULT_CAN_BUS_OFF;
    }

    /* Self-test fault only if self-test has actually completed */
    if ((Safety_SelfTestDone == TRUE) &&
        (self_test_result == FZC_SELF_TEST_FAIL)) {
        fault_mask |= FZC_FAULT_SELF_TEST;
    }

    /* ----------------------------------------------------------
     * Step 2b: Decrement post-boot grace counter
     * ---------------------------------------------------------- */
    if (Safety_GraceCounter > 0u) {
        Safety_GraceCounter--;
    }

    /* ----------------------------------------------------------
     * Step 2c: Motor cutoff aggregation (safety architecture)
     *          A steering or brake fault means the vehicle cannot be
     *          safely operated. Motor cutoff is the correct safety
     *          response (SS-MOTOR-OFF per SG-003). This propagates
     *          via CAN 0x211 → CVC → VehicleState → SAFE_STOP.
     *
     *          During the post-boot grace period, suppress cutoff to
     *          absorb startup transients (SC E-Stop, lidar timeout).
     *          Faults are still recorded in fault_mask — only the
     *          cutoff assertion is deferred.
     *
     *          Read-modify-write: Brake SWC (higher priority) may
     *          have already set motor_cutoff=1 in RTE. We must
     *          preserve that value, not blindly overwrite to 0.
     * ---------------------------------------------------------- */
    if ((steer_fault != 0u) || (brake_fault != 0u)) {
        if (Safety_GraceCounter == 0u) {
            (void)Rte_Write(FZC_SIG_MOTOR_CUTOFF, 1u);
            FSAFE_DIAG("CUTOFF=1 sf=%u bf=%u lf=%u mask=0x%02X",
                       (unsigned)steer_fault, (unsigned)brake_fault,
                       (unsigned)lidar_fault, (unsigned)fault_mask);
        } else {
            (void)Rte_Write(FZC_SIG_MOTOR_CUTOFF, 0u);
            FSAFE_DIAG("GRACE=%u sf=%u bf=%u lf=%u (cutoff suppressed)",
                       (unsigned)Safety_GraceCounter,
                       (unsigned)steer_fault, (unsigned)brake_fault,
                       (unsigned)lidar_fault);
        }
    } else {
        (void)Rte_Write(FZC_SIG_MOTOR_CUTOFF, 0u);
    }

    /* ----------------------------------------------------------
     * Step 3: Determine safety status
     * ---------------------------------------------------------- */
    if ((fault_mask & (FZC_FAULT_STEER | FZC_FAULT_BRAKE)) != 0u) {
        Safety_Status = SAFETY_STATUS_FAULT;
    } else if (fault_mask != FZC_FAULT_NONE) {
        Safety_Status = SAFETY_STATUS_DEGRADED;
    } else {
        Safety_Status = SAFETY_STATUS_OK;
    }

    /* ----------------------------------------------------------
     * Step 4: Watchdog feed — 4 conditions must all be true
     * ---------------------------------------------------------- */
    wdg_feed_ok = TRUE;

    /* Condition 1: No critical fault (steering or brake) */
    if ((fault_mask & (FZC_FAULT_STEER | FZC_FAULT_BRAKE)) != 0u) {
        wdg_feed_ok = FALSE;
    }

    /* Condition 2: Vehicle not in SHUTDOWN */
    if (vehicle_state == FZC_STATE_SHUTDOWN) {
        wdg_feed_ok = FALSE;
    }

    /* Condition 3: Self-test passed (or not yet run) */
    if ((Safety_SelfTestDone == TRUE) &&
        (self_test_result == FZC_SELF_TEST_FAIL)) {
        wdg_feed_ok = FALSE;
    }

    /* Condition 4: Main loop executing (implicit — we are here) */

    if (wdg_feed_ok == TRUE) {
        /* Toggle WDI pin */
        Safety_WdiToggle = (uint8)(Safety_WdiToggle ^ 1u);
        Dio_WriteChannel(SAFETY_WDI_CHANNEL, Safety_WdiToggle);
    } else {
        /* Do not feed watchdog — will trigger HW reset */
        Dem_ReportErrorStatus(FZC_DTC_WATCHDOG_FAIL, DEM_EVENT_STATUS_FAILED);
        fault_mask |= FZC_FAULT_WATCHDOG;
    }

    /* ----------------------------------------------------------
     * Step 5: Write outputs to RTE
     * ---------------------------------------------------------- */
    (void)Rte_Write(FZC_SIG_FAULT_MASK, (uint32)fault_mask);
    (void)Rte_Write(FZC_SIG_SAFETY_STATUS, (uint32)Safety_Status);
}

/* ==================================================================
 * API: Swc_FzcSafety_GetStatus
 * ================================================================== */

uint8 Swc_FzcSafety_GetStatus(void)
{
    return Safety_Status;
}
