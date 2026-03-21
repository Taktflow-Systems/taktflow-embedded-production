/**
 * @file    Swc_Brake.c
 * @brief   Brake servo control — PWM output, feedback, auto-brake, motor cutoff
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-009 to SWR-FZC-012
 * @traces_to  SSR-FZC-009 to SSR-FZC-012
 *
 * @details  Implements the brake servo control SWC for the FZC:
 *           1. Reads brake command from RTE (0-100 %)
 *           2. Reads E-stop flag — immediate 100 % brake if active
 *           3. Monitors command timeout (100 ms default) — auto-brake + latch
 *           4. Drives brake servo PWM (channel 2, TIM2_CH2)
 *           5. Feedback verification: commanded vs actual, debounce 3 cycles
 *           6. On brake fault: force 100 % brake, start motor cutoff sequence
 *           7. Motor cutoff: Com_SendSignal repeated 10 times at 10 ms interval
 *           8. Writes RTE signals: position, fault, motor cutoff, PWM disable
 *           9. Reports DTCs via Dem on fault transitions
 *
 *           All variables are static file-scope.  No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Brake.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "IoHwAb.h"
#include "Rte.h"
#include "Com.h"
#include "Dem.h"

/* SIL diagnostic logging — compile with -DSIL_DIAG to enable */
#ifdef SIL_DIAG
#include <stdio.h>
#define BRAKE_DIAG(fmt, ...) (void)fprintf(stderr, "[BRAKE] " fmt "\n", ##__VA_ARGS__)
#else
#define BRAKE_DIAG(fmt, ...) ((void)0)
#endif

/* ==================================================================
 * Constants
 * ================================================================== */

/** Brake servo PWM channel (TIM2_CH2) */
#define BRAKE_PWM_CHANNEL          2u

/** Timeout threshold in cycles (autoTimeoutMs / 10ms per cycle).
 *  First cycle establishes the baseline, so threshold = (100ms / 10ms) - 1 = 9
 *  to trigger on the 10th consecutive identical command. */
#define BRAKE_TIMEOUT_CYCLES        9u

/** Maximum brake command value */
#define BRAKE_CMD_MAX              100u

/* ==================================================================
 * Module State (all static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8                          Brake_Initialized;
static const Swc_Brake_ConfigType*    Brake_CfgPtr;

/** Current brake position 0-100 percent */
static uint8   Brake_Position;

/** Current brake fault code */
static uint8   Brake_Fault;

/** Command timeout counter (increments each cycle if no fresh RTE data) */
static uint16  Brake_CmdTimeoutCounter;

/** Fault latch flag (TRUE = brake locked at 100%) */
static uint8   Brake_FaultLatched;

/** Fault-free cycle counter for latch clearing */
static uint8   Brake_LatchCounter;

/** Feedback deviation debounce counter */
static uint8   Brake_FaultDebounceCounter;

/** Auto-brake active flag */
static uint8   Brake_AutoBrakeActive;

/** Motor cutoff message repetition counter */
static uint8   Brake_CutoffCounter;

/** Motor cutoff sending flag (TRUE while sending sequence) */
static uint8   Brake_CutoffSending;

/** First valid brake command received (timeout armed only after TRUE) */
static uint8   Brake_FirstCmdReceived;

/** Previous brake command for timeout and feedback detection */
static uint32  Brake_PrevBrakeCmd;

#ifdef SIL_DIAG
/** Diagnostic tick counter for periodic status dumps */
static uint16  Brake_DiagTick;
#endif

/** Previous clamped brake command for oscillation detection */
static uint8   Brake_OscPrevCmd;

/** Oscillation detection counter (consecutive large-delta cycles) */
static uint8   Brake_OscillationCounter;

/* ==================================================================
 * API: Swc_Brake_Init
 * ================================================================== */

void Swc_Brake_Init(const Swc_Brake_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Brake_Initialized = FALSE;
        Brake_CfgPtr      = NULL_PTR;
        return;
    }

    Brake_CfgPtr              = ConfigPtr;

    Brake_Position            = 0u;
    Brake_Fault               = FZC_BRAKE_NO_FAULT;
    Brake_CmdTimeoutCounter   = 0u;
    Brake_FaultLatched        = FALSE;
    Brake_LatchCounter        = 0u;
    Brake_FaultDebounceCounter = 0u;
    Brake_AutoBrakeActive     = FALSE;
    Brake_CutoffCounter       = 0u;
    Brake_CutoffSending       = FALSE;
    Brake_FirstCmdReceived    = FALSE;
    Brake_PrevBrakeCmd        = 0xFFFFFFFFu;  /* Sentinel: no previous command */
    Brake_OscPrevCmd          = 0u;
    Brake_OscillationCounter  = 0u;
#ifdef SIL_DIAG
    Brake_DiagTick            = 0u;
#endif

    Brake_Initialized         = TRUE;
}

/* ==================================================================
 * API: Swc_Brake_MainFunction (10ms cyclic)
 * ================================================================== */

void Swc_Brake_MainFunction(void)
{
    uint32 brake_cmd_raw;
    uint32 estop_active;
    uint8  brake_cmd;
    uint8  new_fault;
    uint8  deviation;
    uint32 cutoff_data;

    /* ----------------------------------------------------------
     * Guard: not initialized -> safe no-op
     * ---------------------------------------------------------- */
    if (Brake_Initialized != TRUE) {
        return;
    }

    if (Brake_CfgPtr == NULL_PTR) {
        return;
    }

    new_fault     = FZC_BRAKE_NO_FAULT;
    brake_cmd_raw = 0u;
    estop_active  = 0u;

    /* ----------------------------------------------------------
     * Step 1: Read brake command from RTE
     * ---------------------------------------------------------- */
    {
        Std_ReturnType rte_ret;
        rte_ret = Rte_Read(FZC_SIG_BRAKE_CMD, &brake_cmd_raw);

        /* Track command freshness for timeout (same pattern as
         * Swc_Steering — RTE return value, not value-change).
         * Value-change detection triggers false positives when
         * the commanded value is legitimately constant (e.g. 0). */
        if (rte_ret == E_OK) {
            Brake_CmdTimeoutCounter = 0u;
            Brake_FirstCmdReceived  = TRUE;
        } else {
            if (Brake_CmdTimeoutCounter < 0xFFFFu) {
                Brake_CmdTimeoutCounter++;
            }
        }
    }

    /* Clamp to 0-100 */
    if (brake_cmd_raw > (uint32)BRAKE_CMD_MAX) {
        brake_cmd = BRAKE_CMD_MAX;
    } else {
        brake_cmd = (uint8)brake_cmd_raw;
    }

    /* ----------------------------------------------------------
     * Step 2: Read E-stop flag from RTE
     * ---------------------------------------------------------- */
    (void)Rte_Read(FZC_SIG_ESTOP_ACTIVE, &estop_active);

    if (estop_active != 0u) {
        /* E-stop: immediate 100% brake, latched */
        brake_cmd             = BRAKE_CMD_MAX;
        Brake_AutoBrakeActive = TRUE;
        Brake_FaultLatched    = TRUE;
        Brake_LatchCounter    = 0u;
    }

    /* ----------------------------------------------------------
     * Step 3: Command timeout detection
     *         If no fresh RTE data for BRAKE_TIMEOUT_CYCLES
     *         consecutive cycles, trigger auto-brake.
     * ---------------------------------------------------------- */
    if ((Brake_AutoBrakeActive == FALSE) && (Brake_FirstCmdReceived == TRUE)) {
        if (Brake_CmdTimeoutCounter >= BRAKE_TIMEOUT_CYCLES) {
            new_fault             = FZC_BRAKE_CMD_TIMEOUT;
            Brake_AutoBrakeActive = TRUE;
            Brake_FaultLatched    = TRUE;
            Brake_LatchCounter    = 0u;
        }
    }

    /* ----------------------------------------------------------
     * Step 3b: Command oscillation detection
     *          Track absolute change between consecutive clamped
     *          commands.  If delta exceeds threshold for N
     *          consecutive cycles, declare oscillation fault.
     *          Gated on no existing fault to avoid interference
     *          with timeout/PWM-deviation detection.
     * ---------------------------------------------------------- */
    if ((new_fault == FZC_BRAKE_NO_FAULT) && (Brake_FaultLatched == FALSE)) {
        uint8 osc_delta;

        if (brake_cmd >= Brake_OscPrevCmd) {
            osc_delta = brake_cmd - Brake_OscPrevCmd;
        } else {
            osc_delta = Brake_OscPrevCmd - brake_cmd;
        }

        if (osc_delta > FZC_BRAKE_OSCILLATION_DELTA_THRESH) {
            if (Brake_OscillationCounter < 0xFFu) {
                Brake_OscillationCounter++;
            }
        } else {
            Brake_OscillationCounter = 0u;
        }

        if (Brake_OscillationCounter >= FZC_BRAKE_OSCILLATION_DEBOUNCE) {
            new_fault = FZC_BRAKE_CMD_OSCILLATION;
        }
    } else {
        Brake_OscillationCounter = 0u;
    }

    Brake_OscPrevCmd = brake_cmd;

    Brake_PrevBrakeCmd = brake_cmd_raw;

    /* ----------------------------------------------------------
     * Step 4: Auto-brake override (latched: cannot be cleared by
     *         new commands alone — requires latch clear cycles)
     * ---------------------------------------------------------- */
    if (Brake_AutoBrakeActive == TRUE) {
        brake_cmd = BRAKE_CMD_MAX;
    }

    /* ----------------------------------------------------------
     * Step 5: Drive PWM for brake servo
     *         Command 0-100 maps directly to duty cycle percentage.
     *         0% = no brake, 100% = full brake.
     * ---------------------------------------------------------- */
    Pwm_SetDutyCycle(BRAKE_PWM_CHANNEL, (uint16)brake_cmd);

    /* ----------------------------------------------------------
     * Step 5b: Read actual brake position from sensor (IoHwAb -> ADC).
     *          Must read BEFORE deviation check so comparison uses
     *          current-cycle feedback, not previous cycle's stale value.
     *          In SIL: ADC returns value injected by Swc_FzcSensorFeeder.
     *          On real HW: ADC reads physical potentiometer feedback.
     * ---------------------------------------------------------- */
    {
        uint16 raw_pos = 0u;
        Std_ReturnType pos_ret = IoHwAb_ReadBrakePosition(&raw_pos);
        if (pos_ret == E_OK) {
            /* IoHwAb returns 0-1000 (ADC counts), scale to 0-100% */
            Brake_Position = (uint8)(raw_pos / 10u);
        }
        /* If read fails, keep previous Brake_Position (fail-safe: stale value) */
    }

    /* ----------------------------------------------------------
     * Step 6: Feedback verification
     *         Compare commanded duty vs current sensor position.
     *         If deviation exceeds threshold for N debounce cycles,
     *         declare PWM deviation fault.
     * ---------------------------------------------------------- */
    if ((new_fault == FZC_BRAKE_NO_FAULT) && (Brake_FirstCmdReceived == TRUE)) {
        if (brake_cmd >= Brake_Position) {
            deviation = brake_cmd - Brake_Position;
        } else {
            deviation = Brake_Position - brake_cmd;
        }

        if (deviation > Brake_CfgPtr->pwmFaultThreshold) {
            Brake_FaultDebounceCounter++;
            BRAKE_DIAG("DEV cmd=%u pos=%u dev=%u thresh=%u dbc=%u/%u",
                       (unsigned)brake_cmd, (unsigned)Brake_Position,
                       (unsigned)deviation, (unsigned)Brake_CfgPtr->pwmFaultThreshold,
                       (unsigned)Brake_FaultDebounceCounter,
                       (unsigned)Brake_CfgPtr->faultDebounce);
            if (Brake_FaultDebounceCounter >= Brake_CfgPtr->faultDebounce) {
                new_fault = FZC_BRAKE_PWM_DEVIATION;
            }
        } else {
            Brake_FaultDebounceCounter = 0u;
        }
    }

#ifdef SIL_DIAG
    Brake_DiagTick++;
    /* Log every 1s (100 cycles) to track cmd/pos over time */
    if ((Brake_DiagTick % 100u) == 1u) {
        BRAKE_DIAG("t=%u cmd=%u pos=%u fault=%u latch=%u auto=%u tmo=%u first=%u",
                   (unsigned)Brake_DiagTick, (unsigned)brake_cmd,
                   (unsigned)Brake_Position, (unsigned)Brake_Fault,
                   (unsigned)Brake_FaultLatched, (unsigned)Brake_AutoBrakeActive,
                   (unsigned)Brake_CmdTimeoutCounter,
                   (unsigned)Brake_FirstCmdReceived);
    }
#endif

    /* ----------------------------------------------------------
     * Step 7: Fault handling — force full brake on any fault
     * ---------------------------------------------------------- */
    if (new_fault != FZC_BRAKE_NO_FAULT) {
        Brake_Fault        = new_fault;
        Brake_FaultLatched = TRUE;
        Brake_LatchCounter = 0u;

        /* Force 100% brake */
        Brake_Position = BRAKE_CMD_MAX;
        Pwm_SetDutyCycle(BRAKE_PWM_CHANNEL, (uint16)BRAKE_CMD_MAX);

        /* Start motor cutoff sequence if not already sending */
        if (Brake_CutoffSending == FALSE) {
            Brake_CutoffSending = TRUE;
            Brake_CutoffCounter = 0u;
        }
    } else if (Brake_FaultLatched == TRUE) {
        /* No new fault but latch still active: count fault-free cycles */
        Brake_LatchCounter++;
        if (Brake_LatchCounter >= Brake_CfgPtr->latchClearCycles) {
            /* Latch cleared after sufficient fault-free cycles.
             * Reset FirstCmdReceived so timeout and deviation checks
             * stay disarmed until a fresh CVC command arrives.  This
             * prevents the timeout from re-firing immediately (the
             * counter is already above threshold) and deviation from
             * triggering against a stale command reference. */
            Brake_FaultLatched      = FALSE;
            Brake_LatchCounter      = 0u;
            Brake_AutoBrakeActive   = FALSE;
            Brake_Fault             = FZC_BRAKE_NO_FAULT;
            Brake_FirstCmdReceived  = FALSE;
            Brake_CmdTimeoutCounter = 0u;
        } else {
            /* Latch still active: keep fault code and force 100% brake */
            if (Brake_Fault == FZC_BRAKE_NO_FAULT) {
                Brake_Fault = FZC_BRAKE_LATCHED;
            }
            Brake_Position = BRAKE_CMD_MAX;
            Pwm_SetDutyCycle(BRAKE_PWM_CHANNEL, (uint16)BRAKE_CMD_MAX);
        }
    } else {
        /* No fault, no latch — normal operation */
        Brake_Fault = FZC_BRAKE_NO_FAULT;
    }

    /* ----------------------------------------------------------
     * Step 8: Motor cutoff CAN sequence
     *         Write motor cutoff to RTE, repeated N times (one per
     *         10ms cycle). Swc_FzcCom reads RTE and sends via Com.
     * ---------------------------------------------------------- */
    if (Brake_CutoffSending == TRUE) {
        if (Brake_CutoffCounter < Brake_CfgPtr->cutoffRepeatCount) {
            (void)Rte_Write(FZC_SIG_MOTOR_CUTOFF, 1u);
            Brake_CutoffCounter++;
        } else {
            Brake_CutoffSending = FALSE;
        }
    }

    /* ----------------------------------------------------------
     * Step 9: Write RTE signals + send brake fault on CAN 0x210
     * ---------------------------------------------------------- */
#ifdef SIL_DIAG
    if (Brake_Fault != FZC_BRAKE_NO_FAULT) {
        BRAKE_DIAG("cmd=%u pos=%u fault=%u latch=%u estop=%u auto=%u",
                   (unsigned)brake_cmd, (unsigned)Brake_Position,
                   (unsigned)Brake_Fault, (unsigned)Brake_FaultLatched,
                   (unsigned)estop_active, (unsigned)Brake_AutoBrakeActive);
    }
#endif
    (void)Rte_Write(FZC_SIG_BRAKE_POS,   (uint32)Brake_Position);
    (void)Rte_Write(FZC_SIG_BRAKE_FAULT,  (uint32)Brake_Fault);

    /* Brake fault TX is handled by Swc_FzcCom_TransmitSchedule which reads
     * FZC_SIG_BRAKE_FAULT from RTE and sends on 0x210 with E2E protection.
     * Do NOT use Com_SendSignal here — it bypasses E2E and the duplicate
     * frame causes CVC's E2E check to fail. */

    if (Brake_CutoffSending == TRUE) {
        (void)Rte_Write(FZC_SIG_MOTOR_CUTOFF, 1u);
    } else if (Brake_Fault != FZC_BRAKE_NO_FAULT) {
        (void)Rte_Write(FZC_SIG_MOTOR_CUTOFF, 1u);
    } else {
        (void)Rte_Write(FZC_SIG_MOTOR_CUTOFF, 0u);
    }

    /* Write PWM disable flag when faulted */
    if (Brake_FaultLatched == TRUE) {
        (void)Rte_Write(FZC_SIG_BRAKE_PWM_DISABLE, 1u);
    } else {
        (void)Rte_Write(FZC_SIG_BRAKE_PWM_DISABLE, 0u);
    }

    /* ----------------------------------------------------------
     * Step 10: Report DTCs via Dem
     * ---------------------------------------------------------- */
    if (new_fault == FZC_BRAKE_PWM_DEVIATION) {
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_PWM_FAIL, DEM_EVENT_STATUS_FAILED);
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_FAULT, DEM_EVENT_STATUS_FAILED);
    } else if (new_fault == FZC_BRAKE_CMD_TIMEOUT) {
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_TIMEOUT, DEM_EVENT_STATUS_FAILED);
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_FAULT, DEM_EVENT_STATUS_FAILED);
    } else if (new_fault == FZC_BRAKE_CMD_OSCILLATION) {
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_OSCILLATION, DEM_EVENT_STATUS_FAILED);
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_FAULT, DEM_EVENT_STATUS_FAILED);
    } else if (Brake_FaultLatched == FALSE) {
        /* All clear: report PASSED for all brake DTCs */
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_FAULT, DEM_EVENT_STATUS_PASSED);
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_TIMEOUT, DEM_EVENT_STATUS_PASSED);
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_PWM_FAIL, DEM_EVENT_STATUS_PASSED);
        Dem_ReportErrorStatus(FZC_DTC_BRAKE_OSCILLATION, DEM_EVENT_STATUS_PASSED);
    } else {
        /* Latch still active but no new fault — no DTC report change */
    }
}

/* ==================================================================
 * API: Swc_Brake_GetPosition
 * ================================================================== */

Std_ReturnType Swc_Brake_GetPosition(uint8* pos)
{
    if (Brake_Initialized != TRUE) {
        return E_NOT_OK;
    }

    if (pos == NULL_PTR) {
        return E_NOT_OK;
    }

    *pos = Brake_Position;

    return E_OK;
}
