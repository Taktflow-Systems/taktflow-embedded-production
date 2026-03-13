/**
 * @file    Swc_Encoder.c
 * @brief   RZC quadrature encoder SWC — speed/RPM calculation, stall
 *          detection, direction plausibility checking (ASIL C, TIM4)
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-012, SWR-RZC-013, SWR-RZC-014
 * @traces_to  SSR-RZC-012, SSR-RZC-013, SSR-RZC-014
 *
 * @details  Implements the RZC encoder SWC:
 *           1. Reads quadrature encoder count and direction via IoHwAb
 *           2. Calculates RPM from delta counts per 10ms cycle
 *           3. Stall detection: PWM > 10% but zero encoder delta for
 *              50 consecutive cycles (500ms) triggers stall fault
 *           4. Direction plausibility: commanded vs encoder direction
 *              mismatch for 5 consecutive cycles (50ms) triggers fault
 *           5. Grace periods after direction change suppress false faults
 *           6. On fault: disables motor via Dio (R_EN/L_EN LOW), reports DTC
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Encoder.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"
#include "IoHwAb.h"
#include "Dem.h"

/** Stall grace cycles: 200ms / 10ms = 20 */
#define ENC_STALL_GRACE_CYCLES  (RZC_ENCODER_STALL_GRACE_MS / 10u)

/** Direction grace cycles: 100ms / 10ms = 10 */
#define ENC_DIR_GRACE_CYCLES    (RZC_ENCODER_DIR_GRACE_MS / 10u)

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8   Enc_Initialized;
static uint32  Enc_PrevCount;
static uint16  Enc_SpeedRpm;
static uint8   Enc_Direction;
static uint8   Enc_PrevDirection;
static uint8   Enc_StallCounter;
static uint8   Enc_StallFault;
static uint8   Enc_DirMismatchCounter;
static uint8   Enc_DirFault;
static uint8   Enc_StallGraceCounter;
static uint8   Enc_DirGraceCounter;

/* ==================================================================
 * Internal: Disable motor via independent GPIO path
 * ================================================================== */

static void Enc_DisableMotor(void)
{
    Dio_WriteChannel(RZC_MOTOR_R_EN_CHANNEL, 0u);
    Dio_WriteChannel(RZC_MOTOR_L_EN_CHANNEL, 0u);
}

/* ==================================================================
 * API: Swc_Encoder_Init
 * ================================================================== */

void Swc_Encoder_Init(void)
{
    Enc_PrevCount          = 0u;
    Enc_SpeedRpm           = 0u;
    Enc_Direction          = RZC_DIR_STOP;
    Enc_PrevDirection      = RZC_DIR_STOP;
    Enc_StallCounter       = 0u;
    Enc_StallFault         = 0u;
    Enc_DirMismatchCounter = 0u;
    Enc_DirFault           = 0u;
    Enc_StallGraceCounter  = 0u;
    Enc_DirGraceCounter    = 0u;
    Enc_Initialized        = TRUE;
}

/* ==================================================================
 * API: Swc_Encoder_MainFunction (10ms cyclic)
 * ================================================================== */

void Swc_Encoder_MainFunction(void)
{
    uint32 current_count;
    uint8  encoder_dir;
    uint32 delta;
    uint32 rpm;
    uint32 commanded_dir;
    uint32 torque_echo;

    /* ---- Guard: not initialized ---------------------------------- */
    if (Enc_Initialized != TRUE) {
        return;
    }

    /* ---- Step 1-2: Read encoder hardware ------------------------- */
    current_count = 0u;
    (void)IoHwAb_ReadEncoderCount(&current_count);

    encoder_dir = RZC_DIR_STOP;
    (void)IoHwAb_ReadEncoderDirection(&encoder_dir);

    /* ---- Step 3-4: Calculate delta and store previous ------------ */
    if (current_count >= Enc_PrevCount) {
        delta = current_count - Enc_PrevCount;
    } else {
        /* Counter wrapped (uint32 overflow — unlikely but defensive) */
        delta = current_count + (0xFFFFFFFFu - Enc_PrevCount) + 1u;
    }
    Enc_PrevCount = current_count;

    /* ---- Step 5: Calculate RPM ----------------------------------- */
    /* Period = 10ms = 0.01s
     * RPM = (delta / PPR) * (1 / 0.01) * 60
     *     = (delta * 6000) / PPR                                    */
    rpm = (delta * 6000u) / RZC_ENCODER_PPR;
    Enc_SpeedRpm  = (uint16)rpm;
    Enc_Direction = encoder_dir;

    /* ---- Step 6-7: Read commanded direction and torque echo ------ */
    commanded_dir = (uint32)RZC_DIR_STOP;
    (void)Rte_Read(RZC_SIG_MOTOR_DIR, &commanded_dir);

    torque_echo = 0u;
    (void)Rte_Read(RZC_SIG_TORQUE_ECHO, &torque_echo);

    /* ---- Step 10: Direction change detection (before fault checks)
     *       Set grace periods only on actual direction reversals
     *       (FORWARD <-> REVERSE), not on transitions from/to STOP -- */
    if ((uint8)commanded_dir != Enc_PrevDirection) {
        if ((Enc_PrevDirection != RZC_DIR_STOP) &&
            ((uint8)commanded_dir != RZC_DIR_STOP)) {
            /* Actual reversal: suppress false faults during transition */
            Enc_StallGraceCounter = ENC_STALL_GRACE_CYCLES;
            Enc_DirGraceCounter   = ENC_DIR_GRACE_CYCLES;
        }
        Enc_PrevDirection = (uint8)commanded_dir;
    }

    /* ---- Step 8: Stall detection --------------------------------- */
    if (Enc_StallFault == 0u) {
        if (Enc_StallGraceCounter > 0u) {
            /* Grace period active — decrement and skip stall check */
            Enc_StallGraceCounter--;
        } else {
            if ((torque_echo > (uint32)RZC_ENCODER_STALL_MIN_PWM) &&
                (delta == 0u)) {
                /* PWM active but no movement — possible stall */
                Enc_StallCounter++;

                if (Enc_StallCounter >= RZC_ENCODER_STALL_CHECKS) {
                    /* Stall confirmed */
                    Enc_StallFault = 1u;
                    Enc_DisableMotor();
                    Dem_ReportErrorStatus(RZC_DTC_STALL,
                                          DEM_EVENT_STATUS_FAILED);
                }
            } else if (delta > 0u) {
                /* Movement detected — reset stall counter */
                Enc_StallCounter = 0u;
            } else {
                /* No PWM demand and no movement — intentional stop.
                 * Do not increment stall counter. */
            }
        }
    }

    /* ---- Step 9: Direction plausibility -------------------------- */
    if (Enc_DirFault == 0u) {
        if (Enc_DirGraceCounter > 0u) {
            /* Grace period active — decrement and skip dir check */
            Enc_DirGraceCounter--;
        } else {
            if (((uint8)commanded_dir != encoder_dir) &&
                ((uint8)commanded_dir != RZC_DIR_STOP) &&
                (delta > 0u)) {
                /* Motor is moving in wrong direction (commanded is active) */
                Enc_DirMismatchCounter++;

                if (Enc_DirMismatchCounter >= RZC_ENCODER_DIR_CHECKS) {
                    /* Direction mismatch confirmed */
                    Enc_DirFault = 1u;
                    Enc_DisableMotor();
                    Dem_ReportErrorStatus(RZC_DTC_DIRECTION,
                                          DEM_EVENT_STATUS_FAILED);
                }
            } else {
                /* Directions match or motor is stopped — reset counter */
                Enc_DirMismatchCounter = 0u;
            }
        }
    }

    /* ---- Step 11: Write outputs to RTE --------------------------- */
    (void)Rte_Write(RZC_SIG_ENCODER_SPEED, (uint32)Enc_SpeedRpm);
    (void)Rte_Write(RZC_SIG_ENCODER_DIR,   (uint32)Enc_Direction);
    (void)Rte_Write(RZC_SIG_ENCODER_STALL, (uint32)Enc_StallFault);
}
