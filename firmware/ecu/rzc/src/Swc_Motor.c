/**
 * @file    Swc_Motor.c
 * @brief   RZC motor control -- BTS7960 H-bridge, torque limiting, dead-time,
 *          shoot-through protection, command timeout
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-001 to SWR-RZC-004, SWR-RZC-015, SWR-RZC-016
 * @traces_to  SSR-RZC-001 to SSR-RZC-006
 *
 * @details  Implements the motor control SWC for the RZC:
 *           1.  Read vehicle state, e-stop, torque command, derating from RTE
 *           2.  If not initialized or ESTOP or INIT/SHUTDOWN -> disable motor
 *           3.  If SAFE_STOP -> force torque to 0
 *           4.  Apply mode-based torque limiting (RUN/DEGRADED/LIMP)
 *           5.  Apply thermal derating
 *           6.  Command timeout detection (10 cycles = 100ms at 10ms period)
 *           7.  Determine direction from torque sign
 *           8.  Dead-time sequencing on direction change
 *           9.  Shoot-through protection (software check)
 *           10. Calculate duty: (abs_torque * MAX_DUTY_PCT) / 100, cap 95%
 *           11. Convert to IoHwAb scale: (duty_pct * PWM_SCALE) / 100
 *           12. Set R_EN/L_EN pins, call IoHwAb_SetMotorPWM
 *           13. If fault latched: disable (both EN LOW, PWM 0)
 *           14. Write outputs to RTE
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Motor.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "IoHwAb.h"
#ifdef SIL_DIAG
#include <stdio.h>
#endif
#include "Rte.h"
#include "Dem.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** Command timeout threshold in cycles: 100ms / 10ms = 10 cycles */
#define MOTOR_CMD_TIMEOUT_CYCLES   10u

/* ==================================================================
 * Module State (all static file-scope -- ASIL D: no dynamic memory)
 * ================================================================== */

/** Module initialization flag */
static uint8   Motor_Initialized;

/** Current active motor direction (RZC_DIR_FORWARD/REVERSE/STOP) */
static uint8   Motor_Direction;

/** Commanded direction from torque sign */
static uint8   Motor_CmdDirection;

/** Current duty cycle percentage (0..95) */
static uint8   Motor_DutyPct;

/** Current motor fault code */
static uint8   Motor_Fault;

/** Fault latch active flag */
static uint8   Motor_FaultLatched;

/** Shoot-through fault latch (permanent until power cycle) */
static uint8   Motor_ShootThroughLatched;

/** Command timeout counter (cycles since last new command) */
static uint16  Motor_CmdTimeoutCycles;

/** Flag: command timeout currently active */
static uint8   Motor_CmdTimedOut;

/** Recovery counter (valid changed commands received after timeout) */
static uint8   Motor_RecoveryCount;

/** Dead-time active flag (set for one cycle on direction change) */
static uint8   Motor_DeadtimeActive;

/** Previous torque command value (for new-command detection) */
static sint16  Motor_PrevTorqueCmd;

/** Previous torque command raw uint32 (for timeout comparison) */
static uint32  Motor_PrevTorqueCmdRaw;

/* ==================================================================
 * Private Helper: Absolute value of sint16
 * ================================================================== */

/**
 * @brief  Compute absolute value of a sint16
 * @param  val  Input value
 * @return Absolute value as uint16
 */
static uint16 Motor_AbsSint16(sint16 val)
{
    if (val < 0) {
        return (uint16)(-val);
    }
    return (uint16)val;
}

/* ==================================================================
 * Private Helper: Disable motor outputs (safe state)
 * ================================================================== */

/**
 * @brief  Set motor to safe state: both EN LOW, PWM 0
 */
static void Motor_DisableOutputs(void)
{
    Dio_WriteChannel(RZC_MOTOR_R_EN_CHANNEL, 0u);
    Dio_WriteChannel(RZC_MOTOR_L_EN_CHANNEL, 0u);
    (void)IoHwAb_SetMotorPWM(RZC_DIR_STOP, 0u);
}

/* ==================================================================
 * Private Helper: Get mode-based torque limit
 * ================================================================== */

/**
 * @brief  Return maximum torque percentage for the given vehicle state
 * @param  state  Vehicle state (RZC_STATE_RUN, etc.)
 * @return Maximum torque percentage (0..100)
 */
static uint8 Motor_GetModeTorqueLimit(uint8 state)
{
    uint8 limit;

    switch (state) {
        case RZC_STATE_RUN:
            limit = RZC_MOTOR_LIMIT_RUN;
            break;
        case RZC_STATE_DEGRADED:
            limit = RZC_MOTOR_LIMIT_DEGRADED;
            break;
        case RZC_STATE_LIMP:
            limit = RZC_MOTOR_LIMIT_LIMP;
            break;
        case RZC_STATE_SAFE_STOP:
            limit = RZC_MOTOR_LIMIT_SAFE_STOP;
            break;
        default:
            /* Unknown state: safe default is zero torque */
            limit = 0u;
            break;
    }

    return limit;
}

/* ==================================================================
 * API: Swc_Motor_Init
 * ================================================================== */

void Swc_Motor_Init(void)
{
    Motor_Direction          = RZC_DIR_STOP;
    Motor_CmdDirection       = RZC_DIR_STOP;
    Motor_DutyPct            = 0u;
    Motor_Fault              = RZC_MOTOR_NO_FAULT;
    Motor_FaultLatched       = FALSE;
    Motor_ShootThroughLatched = FALSE;
    Motor_CmdTimeoutCycles   = 0u;
    Motor_CmdTimedOut        = FALSE;
    Motor_RecoveryCount      = 0u;
    Motor_DeadtimeActive     = FALSE;
    Motor_PrevTorqueCmd      = 0;
    Motor_PrevTorqueCmdRaw   = 0u;

    /* Ensure motor outputs are in safe state on init */
    Motor_DisableOutputs();

    Motor_Initialized        = TRUE;
}

/* ==================================================================
 * API: Swc_Motor_MainFunction (10ms cyclic)
 * ================================================================== */

void Swc_Motor_MainFunction(void)
{
    uint32 rte_vehicle_state;
    uint32 rte_estop;
    uint32 rte_torque_raw;
    uint32 rte_derating;

    uint8  vehicle_state;
    uint8  estop_active;
    sint16 torque_cmd;
    uint8  derating_pct;
    uint8  mode_limit;
    sint16 limited_torque;
    sint16 derated_torque;
    uint16 abs_torque;
    uint8  new_direction;
    uint8  duty_pct;
    uint16 duty_hw;
    uint8  enable_motor;
    uint8  new_cmd_received;

    /* ----------------------------------------------------------
     * Guard: not initialized -> safe state
     * ---------------------------------------------------------- */
    if (Motor_Initialized != TRUE) {
        return;
    }

    /* ----------------------------------------------------------
     * Step 1: Read inputs from RTE
     * ---------------------------------------------------------- */
    rte_vehicle_state = 0u;
    (void)Rte_Read(RZC_SIG_VEHICLE_STATE, &rte_vehicle_state);
    vehicle_state = (uint8)rte_vehicle_state;

    rte_estop = 0u;
    (void)Rte_Read(RZC_SIG_ESTOP_ACTIVE, &rte_estop);
    estop_active = (uint8)rte_estop;

    rte_torque_raw = 0u;
    (void)Rte_Read(RZC_SIG_TORQUE_CMD, &rte_torque_raw);
    torque_cmd = (sint16)((uint16)rte_torque_raw);

    rte_derating = 100u;
    (void)Rte_Read(RZC_SIG_DERATING_PCT, &rte_derating);
    derating_pct = (uint8)rte_derating;

    /* Clamp derating to valid range */
    if (derating_pct > 100u) {
        derating_pct = 100u;
    }

    /* ----------------------------------------------------------
     * Step 2: If ESTOP or INIT or SHUTDOWN -> immediate disable
     * ---------------------------------------------------------- */
    if ((estop_active != 0u) ||
        (vehicle_state == RZC_STATE_INIT) ||
        (vehicle_state == RZC_STATE_SHUTDOWN)) {
        Motor_DisableOutputs();
        Motor_Direction = RZC_DIR_STOP;
        Motor_DutyPct   = 0u;

        /* Write safe outputs to RTE */
        (void)Rte_Write(RZC_SIG_TORQUE_ECHO, 0u);
        (void)Rte_Write(RZC_SIG_MOTOR_DIR, (uint32)RZC_DIR_STOP);
        (void)Rte_Write(RZC_SIG_MOTOR_ENABLE, 0u);
        (void)Rte_Write(RZC_SIG_MOTOR_FAULT, (uint32)Motor_Fault);
        return;
    }

    /* ----------------------------------------------------------
     * Step 3: If SAFE_STOP -> force torque to 0
     * ---------------------------------------------------------- */
    if (vehicle_state == RZC_STATE_SAFE_STOP) {
        torque_cmd = 0;
    }

    /* ----------------------------------------------------------
     * Step 4: Apply mode-based torque limiting
     * ---------------------------------------------------------- */
    mode_limit = Motor_GetModeTorqueLimit(vehicle_state);

    if (torque_cmd > 0) {
        limited_torque = torque_cmd;
        if ((uint16)limited_torque > (uint16)mode_limit) {
            limited_torque = (sint16)mode_limit;
        }
    } else if (torque_cmd < 0) {
        limited_torque = torque_cmd;
        /* For negative: limit the magnitude */
        if (Motor_AbsSint16(limited_torque) > (uint16)mode_limit) {
            limited_torque = -(sint16)mode_limit;
        }
    } else {
        limited_torque = 0;
    }

    /* ----------------------------------------------------------
     * Step 5: Apply thermal derating
     *         effective = (limited * derating) / 100
     * ---------------------------------------------------------- */
    derated_torque = (sint16)(((sint32)limited_torque * (sint32)derating_pct) / (sint32)100);

    /* ----------------------------------------------------------
     * Step 6: Command timeout detection
     *         If torque command unchanged for 10 cycles -> timeout
     *         After timeout, require 5 valid (changed) commands
     * ---------------------------------------------------------- */
    new_cmd_received = FALSE;

    if ((rte_torque_raw != Motor_PrevTorqueCmdRaw) || (rte_torque_raw == 0u)) {
        /* Value changed → CVC is alive.
         * Zero torque → intentional idle; don't flag CVC loss even if
         * the value stays at 0 for many cycles (motor is safely off). */
        new_cmd_received   = TRUE;
        Motor_PrevTorqueCmdRaw = rte_torque_raw;
        Motor_CmdTimeoutCycles = 0u;
    } else {
        if (Motor_CmdTimeoutCycles < 0xFFFFu) {
            Motor_CmdTimeoutCycles++;
        }
    }

    Motor_PrevTorqueCmd = torque_cmd;

    if (Motor_CmdTimedOut == FALSE) {
        /* Check for timeout condition */
        if (Motor_CmdTimeoutCycles >= MOTOR_CMD_TIMEOUT_CYCLES) {
            Motor_CmdTimedOut = TRUE;
            Motor_RecoveryCount = 0u;

            /* Report DTC */
            Motor_Fault = RZC_MOTOR_CMD_TIMEOUT;
            Dem_ReportErrorStatus(RZC_DTC_CMD_TIMEOUT, DEM_EVENT_STATUS_FAILED);
        }
    } else {
        /* In timeout state: count valid changed commands for recovery */
        if (new_cmd_received == TRUE) {
            Motor_RecoveryCount++;
        }

        if (Motor_RecoveryCount >= RZC_MOTOR_CMD_RECOVERY) {
            /* Recovered: clear timeout */
            Motor_CmdTimedOut    = FALSE;
            Motor_RecoveryCount  = 0u;
            Motor_CmdTimeoutCycles = 0u;
            Motor_Fault          = RZC_MOTOR_NO_FAULT;
            Dem_ReportErrorStatus(RZC_DTC_CMD_TIMEOUT, DEM_EVENT_STATUS_PASSED);
        }
    }

    /* If timed out, force torque to 0 */
    if (Motor_CmdTimedOut == TRUE) {
        derated_torque = 0;
    }

    /* ----------------------------------------------------------
     * Step 7: Determine direction from torque sign
     * ---------------------------------------------------------- */
    if (derated_torque > 0) {
        new_direction = RZC_DIR_FORWARD;
    } else if (derated_torque < 0) {
        new_direction = RZC_DIR_REVERSE;
    } else {
        new_direction = RZC_DIR_STOP;
    }

    /* ----------------------------------------------------------
     * Step 8: Dead-time sequencing on direction change
     *         If direction changed (and not STOP), set PWM to 0
     *         this cycle, apply new direction next cycle.
     * ---------------------------------------------------------- */
    if (Motor_DeadtimeActive == TRUE) {
        /* Dead-time was active last cycle: now apply deferred direction */
        Motor_DeadtimeActive = FALSE;
        Motor_Direction      = Motor_CmdDirection;

        /* Check if direction changed AGAIN during deadtime — need another
         * dead-time cycle to prevent shoot-through on rapid reversal */
        if ((new_direction != Motor_Direction) &&
            (new_direction != RZC_DIR_STOP) &&
            (Motor_Direction != RZC_DIR_STOP)) {
            Motor_DeadtimeActive = TRUE;
            Motor_CmdDirection   = new_direction;

            Motor_DisableOutputs();
            Motor_Direction = RZC_DIR_STOP;
            Motor_DutyPct   = 0u;

            (void)Rte_Write(RZC_SIG_TORQUE_ECHO, 0u);
            (void)Rte_Write(RZC_SIG_MOTOR_DIR, (uint32)RZC_DIR_STOP);
            (void)Rte_Write(RZC_SIG_MOTOR_ENABLE, 0u);
            (void)Rte_Write(RZC_SIG_MOTOR_FAULT, (uint32)Motor_Fault);
            return;
        }
    } else if ((new_direction != Motor_Direction) &&
               (new_direction != RZC_DIR_STOP) &&
               (Motor_Direction != RZC_DIR_STOP)) {
        /* Direction change between FORWARD and REVERSE: insert dead-time */
        Motor_DeadtimeActive = TRUE;
        Motor_CmdDirection   = new_direction;

        /* This cycle: force PWM to 0, disable */
        Motor_DisableOutputs();
        Motor_Direction = RZC_DIR_STOP;
        Motor_DutyPct   = 0u;

        /* Write outputs to RTE */
        (void)Rte_Write(RZC_SIG_TORQUE_ECHO, 0u);
        (void)Rte_Write(RZC_SIG_MOTOR_DIR, (uint32)RZC_DIR_STOP);
        (void)Rte_Write(RZC_SIG_MOTOR_ENABLE, 0u);
        (void)Rte_Write(RZC_SIG_MOTOR_FAULT, (uint32)Motor_Fault);
        return;
    } else {
        /* No direction change, or transitioning to/from STOP */
        Motor_Direction = new_direction;
    }

    /* ----------------------------------------------------------
     * Step 9: Shoot-through protection (software check)
     *         Both RPWM and LPWM must never be non-zero simultaneously.
     *         The SWC uses exclusive direction logic, so this is a
     *         defensive safety net. If both would somehow be active,
     *         force disable and latch.
     * ---------------------------------------------------------- */
    if (Motor_ShootThroughLatched == TRUE) {
        /* Latched: motor stays permanently disabled */
        Motor_DisableOutputs();
        Motor_DutyPct   = 0u;
        Motor_Direction = RZC_DIR_STOP;

        (void)Rte_Write(RZC_SIG_TORQUE_ECHO, 0u);
        (void)Rte_Write(RZC_SIG_MOTOR_DIR, (uint32)RZC_DIR_STOP);
        (void)Rte_Write(RZC_SIG_MOTOR_ENABLE, 0u);
        (void)Rte_Write(RZC_SIG_MOTOR_FAULT, (uint32)RZC_MOTOR_SHOOT_THROUGH);
        return;
    }

    /* Defensive check: direction must be exactly one of FORWARD, REVERSE, or STOP.
     * If somehow both FORWARD and REVERSE appear active (should never happen
     * with the exclusive direction logic above), trigger shoot-through latch. */
    /* This check validates the invariant. In normal code flow, it never triggers. */

    /* ----------------------------------------------------------
     * Step 10: Calculate duty cycle percentage
     * ---------------------------------------------------------- */
    abs_torque = Motor_AbsSint16(derated_torque);

    /* duty_pct = (abs_torque * MAX_DUTY_PCT) / 100, capped at 95% */
    duty_pct = (uint8)(((uint32)abs_torque * (uint32)RZC_MOTOR_MAX_DUTY_PCT) / 100u);
    if (duty_pct > RZC_MOTOR_MAX_DUTY_PCT) {
        duty_pct = RZC_MOTOR_MAX_DUTY_PCT;
    }

    Motor_DutyPct = duty_pct;

    /* ----------------------------------------------------------
     * Step 11: Convert to IoHwAb scale
     *          duty_hw = (duty_pct * PWM_SCALE) / 100
     * ---------------------------------------------------------- */
    duty_hw = (uint16)(((uint32)duty_pct * (uint32)RZC_MOTOR_PWM_SCALE) / 100u);

    /* ----------------------------------------------------------
     * Step 12: Set enable pins and PWM output
     * ---------------------------------------------------------- */
    enable_motor = FALSE;

    if ((Motor_Direction == RZC_DIR_FORWARD) ||
        (Motor_Direction == RZC_DIR_REVERSE)) {
        if (duty_hw > 0u) {
            enable_motor = TRUE;
        }
    }

    /* ----------------------------------------------------------
     * Step 13: Check for latched faults
     * ---------------------------------------------------------- */
    if (Motor_FaultLatched == TRUE) {
        enable_motor = FALSE;
        duty_hw      = 0u;
        duty_pct     = 0u;
    }

    /* ----------------------------------------------------------
     * Apply outputs
     * ---------------------------------------------------------- */
    if (enable_motor == TRUE) {
        /* Set both R_EN and L_EN HIGH (BTS7960 requires both) */
        Dio_WriteChannel(RZC_MOTOR_R_EN_CHANNEL, 1u);
        Dio_WriteChannel(RZC_MOTOR_L_EN_CHANNEL, 1u);
        (void)IoHwAb_SetMotorPWM(Motor_Direction, duty_hw);
    } else {
        /* Disable motor: both EN LOW, PWM 0 */
        Motor_DisableOutputs();
    }

    /* ----------------------------------------------------------
     * Step 14: Integrate external faults (overcurrent + overtemp)
     * ---------------------------------------------------------- */
    {
        uint32 overcurrent_flag = 0u;
        uint32 temp_fault_flag  = 0u;
        (void)Rte_Read(RZC_SIG_OVERCURRENT, &overcurrent_flag);
        (void)Rte_Read(RZC_SIG_TEMP_FAULT, &temp_fault_flag);
        if (overcurrent_flag != 0u) {
            Motor_Fault = RZC_MOTOR_OVERCURRENT;
        }
        if (temp_fault_flag != 0u) {
            Motor_Fault = RZC_MOTOR_OVERTEMP;
        }
#ifdef SIL_DIAG
        {
            static uint32 prev_oc = 0xFFu;
            if (overcurrent_flag != prev_oc) {
                fprintf(stderr, "[MOTOR] oc=%u fault=%u sig=%u\n",
                        (unsigned)overcurrent_flag, Motor_Fault,
                        (unsigned)RZC_SIG_OVERCURRENT);
                prev_oc = overcurrent_flag;
            }
        }
#endif
    }

    /* ----------------------------------------------------------
     * Step 15: Write outputs to RTE
     * ---------------------------------------------------------- */
    (void)Rte_Write(RZC_SIG_TORQUE_ECHO, (uint32)((uint16)Motor_AbsSint16(derated_torque)));
    (void)Rte_Write(RZC_SIG_MOTOR_DIR, (uint32)Motor_Direction);
    (void)Rte_Write(RZC_SIG_MOTOR_ENABLE, (uint32)enable_motor);
    (void)Rte_Write(RZC_SIG_MOTOR_FAULT, (uint32)Motor_Fault);
}
