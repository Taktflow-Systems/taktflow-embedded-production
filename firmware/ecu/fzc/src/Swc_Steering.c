/**
 * @file    Swc_Steering.c
 * @brief   Steering servo control — plausibility, rate limit, RTC, PWM output
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-001 to SWR-FZC-008, SWR-FZC-028
 * @traces_to  SSR-FZC-001 to SSR-FZC-008
 *
 * @details  Implements the steering servo control SWC for the FZC:
 *           1.  Reads commanded steering angle from RTE
 *           2.  Tracks command freshness (resets on fresh RTE read, increments on no data)
 *           3.  Reads actual angle from IoHwAb_ReadSteeringAngle
 *           4.  Range check on command angle (-45..+45 deg)
 *           5.  Plausibility check (|cmd - actual| > 5 deg for 5 cycles)
 *           6.  Command timeout detection (10 cycles = 100ms at 10ms period)
 *           7.  Return-to-center: move toward 0 at 30 deg/s on timeout
 *           8.  Rate limiter: cap angle increase to 0.3 deg/10ms
 *           9.  Convert angle to PWM: linear map -45..+45 -> 1000..2000 us
 *           10. Fault handling: any fault -> neutral PWM, latch active
 *           11. 3-level PWM disable: consecutive faults escalate
 *           12. Write outputs: RTE angle, fault, PWM, Dio
 *           13. Report DTCs via Dem
 *
 *           All variables are static file-scope. No dynamic memory.
 *           Angles stored internally as tenths of a degree (sint16 * 10)
 *           for sub-degree precision in rate limiting and RTC.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Steering.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "IoHwAb.h"
#include "Rte.h"
#include "Dem.h"

/* SIL diagnostic logging — compile with -DSIL_DIAG to enable */
#ifdef SIL_DIAG
#include <stdio.h>
#define STR_DIAG(fmt, ...) (void)fprintf(stderr, "[STEER] " fmt "\n", ##__VA_ARGS__)
#else
#define STR_DIAG(fmt, ...) ((void)0)
#endif

/* ==================================================================
 * Constants
 * ================================================================== */

/** PWM channel for steering servo */
#define STEER_PWM_CHANNEL          1u

/** Dio channel for PWM hardware disable */
#define STEER_DIO_DISABLE_CH       10u

/** Second Dio channel for double-disable (level 3) */
#define STEER_DIO_DISABLE_CH2      11u

/** Internal scaling: degrees -> tenths of degrees */
#define DEG_TO_TENTHS              10

/** Timeout cycles: cmdTimeoutMs / 10ms period */
#define STEER_TIMEOUT_CYCLES       10u

/* ==================================================================
 * Module State (all static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8                              Steering_Initialized;
static const Swc_Steering_ConfigType*     Steering_CfgPtr;

/** Current output angle in tenths of degrees (internal precision) */
static sint16   Steering_CurrentAngle10;

/** Commanded angle in whole degrees from RTE */
static sint16   Steering_CommandAngle;

/** Previous commanded angle for new-command detection */
static sint16   Steering_PrevCommandAngle;

/** Current fault code */
static uint8    Steering_Fault;

/** Plausibility debounce counter */
static uint8    Steering_PlausDebounce;

/** Command timeout counter (cycles since last new command) */
static uint16   Steering_CmdTimeoutCounter;

/** Flag: command timeout currently active (RTC mode) */
static uint8    Steering_CmdTimedOut;

/** Fault latch active flag */
static uint8    Steering_FaultLatched;

/** Latch clear counter (counts fault-free cycles) */
static uint8    Steering_LatchCounter;

/** Previous output angle in tenths for rate limiting */
static sint16   Steering_PrevAngle10;

/** Plausibility check armed — FALSE until first output written */
static uint8    Steering_PlausArmed;

/** PWM disable level (0=none, 1=neutral, 2=Dio, 3=double-Dio) */
static uint8    Steering_PwmDisableLevel;

/** Number of distinct fault episodes (for escalation) */
static uint8    Steering_FaultEpisodeCount;

/** First valid steering command received (timeout armed only after TRUE) */
static uint8    Steering_FirstCmdReceived;

#ifdef SIL_DIAG
/** Diagnostic cycle counter for startup tracing */
static uint16   Steering_DiagCycle;
#endif

/* ==================================================================
 * Private Helper: Absolute difference of sint16 values
 * ================================================================== */

/**
 * @brief  Compute |a - b| for sint16 values
 * @param  a  First value
 * @param  b  Second value
 * @return Absolute difference as uint16
 */
static uint16 Steering_AbsDiffSint16(sint16 a, sint16 b)
{
    sint16 diff;

    if (a >= b) {
        diff = (sint16)(a - b);
    } else {
        diff = (sint16)(b - a);
    }

    return (uint16)diff;
}

/* ==================================================================
 * Private Helper: Angle to PWM mapping
 * ================================================================== */

/**
 * @brief  Linear map angle (tenths of degrees) to PWM duty cycle (us)
 * @param  angle10  Steering angle in tenths of degrees (-450..+450)
 * @return PWM duty in microseconds (1000..2000)
 *
 * @details  Map: -450 -> 1000us, 0 -> 1500us, +450 -> 2000us
 *           pwm = 1500 + (angle10 * 500) / 450
 */
static uint16 Steering_AngleToPwm(sint16 angle10)
{
    sint16 pwm_offset;
    sint16 pwm;

    /* Clamp angle to valid range */
    if (angle10 > (sint16)(FZC_STEER_ANGLE_MAX * DEG_TO_TENTHS)) {
        angle10 = (sint16)(FZC_STEER_ANGLE_MAX * DEG_TO_TENTHS);
    }
    if (angle10 < (sint16)(FZC_STEER_ANGLE_MIN * DEG_TO_TENTHS)) {
        angle10 = (sint16)(FZC_STEER_ANGLE_MIN * DEG_TO_TENTHS);
    }

    /* Linear interpolation: offset = angle10 * 500 / 450 */
    pwm_offset = (sint16)(((sint32)angle10 * (sint32)500) / (sint32)450);
    pwm = (sint16)((sint16)FZC_STEER_PWM_CENTER_US + pwm_offset);

    /* Defensive clamp to min/max */
    if (pwm < (sint16)FZC_STEER_PWM_MIN_US) {
        pwm = (sint16)FZC_STEER_PWM_MIN_US;
    }
    if (pwm > (sint16)FZC_STEER_PWM_MAX_US) {
        pwm = (sint16)FZC_STEER_PWM_MAX_US;
    }

    return (uint16)pwm;
}

/* ==================================================================
 * Private Helper: Apply Rate Limit
 * ================================================================== */

/**
 * @brief  Limit the rate of angle change per cycle (increase only)
 * @param  target10  Desired angle in tenths of degrees
 * @return Rate-limited angle in tenths of degrees
 *
 * @details  Rate limit only applies to INCREASING magnitude (away from center).
 *           Decreasing toward center is unrestricted for safety.
 */
static sint16 Steering_ApplyRateLimit(sint16 target10)
{
    sint16 limited;
    sint16 diff;
    sint16 max_step;

    if (Steering_CfgPtr == NULL_PTR) {
        return 0;
    }

    max_step = (sint16)Steering_CfgPtr->rateLimitDeg10ms;

    diff = (sint16)(target10 - Steering_PrevAngle10);

    if (diff > max_step) {
        /* Positive increase beyond rate limit: cap.
         * But if target is closer to 0 than previous, this is actually
         * a decrease toward center — allow unrestricted for safety. */
        if (Steering_AbsDiffSint16(target10, 0) < Steering_AbsDiffSint16(Steering_PrevAngle10, 0)) {
            limited = target10;
        } else {
            limited = (sint16)(Steering_PrevAngle10 + max_step);
        }
    } else if (diff < (sint16)(-max_step)) {
        /* Negative increase (moving more negative) beyond rate limit */
        /* Check if this is a decrease toward center (allowed) or
         * increase away from center (limited). */
        /* If target is closer to 0 than previous, allow it (decrease) */
        if (Steering_AbsDiffSint16(target10, 0) < Steering_AbsDiffSint16(Steering_PrevAngle10, 0)) {
            /* Moving toward center: unrestricted */
            limited = target10;
        } else {
            /* Moving away from center in negative direction: limit */
            limited = (sint16)(Steering_PrevAngle10 - max_step);
        }
    } else {
        /* Within rate limit: pass through */
        limited = target10;
    }

    return limited;
}

/* ==================================================================
 * API: Swc_Steering_Init
 * ================================================================== */

void Swc_Steering_Init(const Swc_Steering_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Steering_Initialized = FALSE;
        Steering_CfgPtr      = NULL_PTR;
        return;
    }

    Steering_CfgPtr             = ConfigPtr;

    Steering_CurrentAngle10     = 0;
    Steering_CommandAngle       = 0;
    Steering_PrevCommandAngle   = 0;
    Steering_Fault              = FZC_STEER_NO_FAULT;
    Steering_PlausDebounce      = 0u;
    Steering_CmdTimeoutCounter  = 0u;
    Steering_CmdTimedOut        = FALSE;
    Steering_FaultLatched       = FALSE;
    Steering_LatchCounter       = 0u;
    Steering_PrevAngle10        = 0;
    Steering_PlausArmed         = FALSE;
    Steering_PwmDisableLevel    = 0u;
    Steering_FaultEpisodeCount  = 0u;
    Steering_FirstCmdReceived   = FALSE;
#ifdef SIL_DIAG
    Steering_DiagCycle          = 0u;
#endif

    Steering_Initialized        = TRUE;
}

/* ==================================================================
 * API: Swc_Steering_MainFunction (10ms cyclic)
 * ================================================================== */

void Swc_Steering_MainFunction(void)
{
    Std_ReturnType  ret;
    uint32          rte_cmd_raw;
    sint16          cmd_angle;
    sint16          actual_angle;
    sint16          target10;
    sint16          output10;
    uint16          pwm_duty;
    uint8           new_fault;
    uint16          plaus_diff;
    uint8           new_cmd_received;

    if (Steering_Initialized != TRUE) {
        return;
    }

    if (Steering_CfgPtr == NULL_PTR) {
        return;
    }

    new_fault = FZC_STEER_NO_FAULT;

    /* ----------------------------------------------------------
     * Step 1: Read steering command from RTE
     * ---------------------------------------------------------- */
    rte_cmd_raw = 0u;
    ret = Rte_Read(FZC_SIG_STEER_CMD, &rte_cmd_raw);

    if (ret == E_OK) {
        /* Cast uint32 to sint16 via uint16 intermediate */
        cmd_angle = (sint16)((uint16)rte_cmd_raw);
    } else {
        /* RTE read failure: keep previous command */
        cmd_angle = Steering_CommandAngle;
    }

    /* ----------------------------------------------------------
     * Step 2: Track command freshness for timeout detection
     *         Fresh RTE read (E_OK) = reset counter
     *         No data (E_NOT_OK) = increment toward timeout
     * ---------------------------------------------------------- */
    if (ret == E_OK) {
        /* Fresh command received via RTE: reset timeout */
        new_cmd_received = TRUE;
        Steering_CmdTimeoutCounter = 0u;
        Steering_CmdTimedOut = FALSE;
        Steering_FirstCmdReceived = TRUE;
    } else {
        /* No fresh command data: increment timeout counter */
        new_cmd_received = FALSE;
        if (Steering_CmdTimeoutCounter < 0xFFFFu) {
            Steering_CmdTimeoutCounter++;
        }
    }

    Steering_PrevCommandAngle = cmd_angle;
    Steering_CommandAngle     = cmd_angle;
    (void)new_cmd_received; /* TODO:POST-BETA — use for rate-of-change plausibility */

    /* ----------------------------------------------------------
     * Step 3: Read actual angle from IoHwAb (SPI)
     * ---------------------------------------------------------- */
    {
        uint16 raw_angle = 8191U;  /* 14-bit center = 0° if IoHwAb uninitialized */
        ret = IoHwAb_ReadSteeringAngle(&raw_angle);
        /* Convert 14-bit SPI raw (0-16383) to degrees (-45..+45).
         * Formula: deg = (raw * 90 + half) / 16383 - 45
         * The +8191 rounds to nearest integer degree. */
        actual_angle = (sint16)((((sint32)raw_angle * 90) + 8191) / 16383 - 45);
    }

    if (ret != E_OK) {
        /* SPI failure: immediate fault */
        new_fault = FZC_STEER_SPI_FAIL;
    }

#ifdef SIL_DIAG
    Steering_DiagCycle++;
    if (Steering_DiagCycle <= 100u) {
        STR_DIAG("c=%u cmd=%d act=%d latch=%u fault=%u",
                 (unsigned)Steering_DiagCycle,
                 (int)cmd_angle, (int)actual_angle,
                 (unsigned)Steering_FaultLatched,
                 (unsigned)Steering_Fault);
    }
#endif

    /* ----------------------------------------------------------
     * Step 4: Range check on command angle (-45..+45)
     * ---------------------------------------------------------- */
    if (new_fault == FZC_STEER_NO_FAULT) {
        if ((cmd_angle > (sint16)FZC_STEER_ANGLE_MAX) ||
            (cmd_angle < (sint16)FZC_STEER_ANGLE_MIN)) {
            new_fault = FZC_STEER_OUT_OF_RANGE;
        }
    }

    /* ----------------------------------------------------------
     * Step 5: Plausibility check (only if no prior fault this cycle)
     *         |prev_output - actual_feedback| > threshold
     *         Uses previous cycle's rate-limited output (what was actually
     *         commanded to the servo) vs current sensor feedback.
     *         This catches actuator faults without false-tripping on the
     *         rate limiter constraining the command.
     * ---------------------------------------------------------- */
    if ((new_fault == FZC_STEER_NO_FAULT) && (Steering_PlausArmed == TRUE)) {
        sint16 prev_output_deg = (sint16)(Steering_PrevAngle10 / (sint16)DEG_TO_TENTHS);
        plaus_diff = Steering_AbsDiffSint16(prev_output_deg, actual_angle);

        if (plaus_diff >= (uint16)Steering_CfgPtr->plausThreshold) {
            Steering_PlausDebounce++;
            STR_DIAG("PLAUS out=%d act=%d diff=%u deb=%u/%u",
                     (int)prev_output_deg, (int)actual_angle,
                     (unsigned)plaus_diff,
                     (unsigned)Steering_PlausDebounce,
                     (unsigned)Steering_CfgPtr->plausDebounce);
            if (Steering_PlausDebounce >= Steering_CfgPtr->plausDebounce) {
                new_fault = FZC_STEER_PLAUSIBILITY;
                STR_DIAG("!! PLAUS FAULT out=%d act=%d diff=%u",
                         (int)prev_output_deg, (int)actual_angle,
                         (unsigned)plaus_diff);
            }
        } else {
            Steering_PlausDebounce = 0u;
        }
    }

    /* ----------------------------------------------------------
     * Step 6: Check command timeout
     *         Timeout = cmdTimeoutMs / 10ms = 10 cycles
     * ---------------------------------------------------------- */
    if ((Steering_FirstCmdReceived == TRUE) && (Steering_CmdTimeoutCounter >= STEER_TIMEOUT_CYCLES)) {
        Steering_CmdTimedOut = TRUE;
        if (new_fault == FZC_STEER_NO_FAULT) {
            new_fault = FZC_STEER_CMD_TIMEOUT;
        }
    }

    /* ----------------------------------------------------------
     * Step 7: Determine target angle
     *         If timeout: activate return-to-center
     *         Else: use commanded angle
     * ---------------------------------------------------------- */
    if (Steering_CmdTimedOut == TRUE) {
        /* Return-to-center: move CurrentAngle10 toward 0 */
        /* RTC rate: rtcRateDegS deg/s = rtcRateDegS * 10 tenths / 100 cycles
         *         = rtcRateDegS / 10 tenths per cycle
         * E.g. 30 deg/s = 3 tenths per 10ms cycle */
        sint16 rtc_step = (sint16)(Steering_CfgPtr->rtcRateDegS / 10u);

        if (rtc_step < 1) {
            rtc_step = 1;
        }

        if (Steering_CurrentAngle10 > rtc_step) {
            target10 = (sint16)(Steering_CurrentAngle10 - rtc_step);
        } else if (Steering_CurrentAngle10 < (sint16)(-rtc_step)) {
            target10 = (sint16)(Steering_CurrentAngle10 + rtc_step);
        } else {
            /* Close enough to center: snap to 0 */
            target10 = 0;
        }
    } else {
        /* Normal operation: use commanded angle, converted to tenths */
        target10 = (sint16)(cmd_angle * (sint16)DEG_TO_TENTHS);
    }

    /* ----------------------------------------------------------
     * Step 8: Apply rate limiter (increase only, decrease allowed)
     * ---------------------------------------------------------- */
    output10 = Steering_ApplyRateLimit(target10);

    /* ----------------------------------------------------------
     * Step 9: Fault handling — neutral PWM latch
     * ---------------------------------------------------------- */
    Steering_Fault = new_fault;

    if ((new_fault != FZC_STEER_NO_FAULT) && (new_fault != FZC_STEER_CMD_TIMEOUT)) {
        /* Active non-timeout fault: latch, force neutral */
        if (Steering_FaultLatched == FALSE) {
            /* New fault episode */
            Steering_FaultEpisodeCount++;
        }
        Steering_FaultLatched = TRUE;
        Steering_LatchCounter = 0u;
        output10 = 0;
    } else if (Steering_FaultLatched == TRUE) {
        /* No new hardware fault but latch still active */
        Steering_LatchCounter++;
        if (Steering_LatchCounter >= Steering_CfgPtr->latchClearCycles) {
            /* Latch cleared after sufficient fault-free cycles */
            Steering_FaultLatched = FALSE;
            Steering_LatchCounter = 0u;
            /* Fault clears */
            Steering_Fault = FZC_STEER_NO_FAULT;
        } else {
            /* Latch still active: force neutral */
            output10 = 0;
        }
    } else {
        /* No fault, no latch — normal operation */
    }

    /* ----------------------------------------------------------
     * Step 10: Update current angle state
     * ---------------------------------------------------------- */
    Steering_CurrentAngle10 = output10;
    Steering_PrevAngle10    = output10;
    Steering_PlausArmed     = TRUE;

    /* ----------------------------------------------------------
     * Step 11: Convert angle to PWM
     * ---------------------------------------------------------- */
    pwm_duty = Steering_AngleToPwm(output10);

    /* ----------------------------------------------------------
     * Step 12: 3-level PWM disable
     * ---------------------------------------------------------- */
    if (Steering_FaultLatched == TRUE) {
        /* Determine disable level based on fault episode count */
        if (Steering_FaultEpisodeCount >= 3u) {
            Steering_PwmDisableLevel = 3u;
        } else if (Steering_FaultEpisodeCount >= 2u) {
            Steering_PwmDisableLevel = 2u;
        } else {
            Steering_PwmDisableLevel = 1u;
        }
    } else if (Steering_PwmDisableLevel > 0u) {
        /* Latch cleared but disable level persists until explicitly cleared.
         * For safety, keep the disable level from the last fault. */
    } else {
        Steering_PwmDisableLevel = 0u;
    }

    /* Apply PWM disable levels */
    switch (Steering_PwmDisableLevel) {
        case 3u:
            /* Level 3: Double Dio disable — both channels forced low */
            pwm_duty = FZC_STEER_PWM_CENTER_US;
            Pwm_SetDutyCycle(STEER_PWM_CHANNEL, pwm_duty);
            Dio_WriteChannel(STEER_DIO_DISABLE_CH, 0u);
            Dio_WriteChannel(STEER_DIO_DISABLE_CH2, 0u);
            break;

        case 2u:
            /* Level 2: Dio disable — primary channel forced low */
            pwm_duty = FZC_STEER_PWM_CENTER_US;
            Pwm_SetDutyCycle(STEER_PWM_CHANNEL, pwm_duty);
            Dio_WriteChannel(STEER_DIO_DISABLE_CH, 0u);
            break;

        case 1u:
            /* Level 1: Neutral PWM only */
            pwm_duty = FZC_STEER_PWM_CENTER_US;
            Pwm_SetDutyCycle(STEER_PWM_CHANNEL, pwm_duty);
            break;

        case 0u:
            /* FALLTHROUGH */
        default:
            /* Normal: output computed PWM */
            Pwm_SetDutyCycle(STEER_PWM_CHANNEL, pwm_duty);
            break;
    }

    /* ----------------------------------------------------------
     * Step 13: Write signals to RTE
     * ---------------------------------------------------------- */
    /* Write current angle (as sint16 cast to uint32) */
    (void)Rte_Write(FZC_SIG_STEER_ANGLE, (uint32)((uint16)((sint16)(Steering_CurrentAngle10 / (sint16)DEG_TO_TENTHS))));
    (void)Rte_Write(FZC_SIG_STEER_FAULT, (uint32)Steering_Fault);
    (void)Rte_Write(FZC_SIG_STEER_PWM_DISABLE, (uint32)Steering_PwmDisableLevel);

    /* ----------------------------------------------------------
     * Step 14: Report DTCs via Dem
     * ---------------------------------------------------------- */
    /* Plausibility DTC */
    if (new_fault == FZC_STEER_PLAUSIBILITY) {
        Dem_ReportErrorStatus(FZC_DTC_STEER_PLAUSIBILITY, DEM_EVENT_STATUS_FAILED);
    } else if (Steering_FaultLatched == FALSE) {
        Dem_ReportErrorStatus(FZC_DTC_STEER_PLAUSIBILITY, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Latch active, no new plausibility fault — no change */
    }

    /* Range DTC */
    if (new_fault == FZC_STEER_OUT_OF_RANGE) {
        Dem_ReportErrorStatus(FZC_DTC_STEER_RANGE, DEM_EVENT_STATUS_FAILED);
    } else if (Steering_FaultLatched == FALSE) {
        Dem_ReportErrorStatus(FZC_DTC_STEER_RANGE, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Latch active — no change */
    }

    /* Timeout DTC */
    if (new_fault == FZC_STEER_CMD_TIMEOUT) {
        Dem_ReportErrorStatus(FZC_DTC_STEER_TIMEOUT, DEM_EVENT_STATUS_FAILED);
    } else if (Steering_FaultLatched == FALSE) {
        Dem_ReportErrorStatus(FZC_DTC_STEER_TIMEOUT, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Latch active — no change */
    }

    /* SPI fail DTC */
    if (new_fault == FZC_STEER_SPI_FAIL) {
        Dem_ReportErrorStatus(FZC_DTC_STEER_SPI_FAIL, DEM_EVENT_STATUS_FAILED);
    } else if (Steering_FaultLatched == FALSE) {
        Dem_ReportErrorStatus(FZC_DTC_STEER_SPI_FAIL, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Latch active — no change */
    }
}

/* ==================================================================
 * API: Swc_Steering_GetAngle
 * ================================================================== */

Std_ReturnType Swc_Steering_GetAngle(sint16* angle)
{
    if (Steering_Initialized != TRUE) {
        return E_NOT_OK;
    }

    if (angle == NULL_PTR) {
        return E_NOT_OK;
    }

    /* Convert from tenths of degrees back to whole degrees */
    *angle = (sint16)(Steering_CurrentAngle10 / (sint16)DEG_TO_TENTHS);

    return E_OK;
}
