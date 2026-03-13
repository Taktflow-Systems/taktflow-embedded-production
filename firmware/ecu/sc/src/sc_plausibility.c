/**
 * @file    sc_plausibility.c
 * @brief   Torque-vs-current cross-plausibility check for Safety Controller
 * @date    2026-02-23
 *
 * @safety_req SWR-SC-007, SWR-SC-008, SWR-SC-009, SWR-SC-024
 * @traces_to  SSR-SC-007, SSR-SC-008, SSR-SC-009, SSR-SC-018
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_plausibility.h"
#include "Sc_Hw_Cfg.h"
#include "sc_can.h"
#include "sc_gio.h"

/* ==================================================================
 * SC module includes
 * ================================================================== */

#include "sc_heartbeat.h"

/* ==================================================================
 * Torque-to-Current Lookup Table
 * ================================================================== */

/**
 * Torque percentage entries (0%, 7%, 13%, 20%, 27%, 33%, 40%, 47%,
 * 53%, 60%, 67%, 73%, 80%, 87%, 93%, 100%)
 */
static const uint8 torque_pct_lut[SC_TORQUE_LUT_SIZE] = {
    0u, 7u, 13u, 20u, 27u, 33u, 40u, 47u,
    53u, 60u, 67u, 73u, 80u, 87u, 93u, 100u
};

/**
 * Expected current in mA at each torque percentage.
 * Linear interpolation used between entries.
 */
static const uint16 current_ma_lut[SC_TORQUE_LUT_SIZE] = {
    0u,    1750u,  3250u,  5000u,  6750u,  8250u,  10000u, 11750u,
    13250u, 15000u, 16750u, 18250u, 20000u, 21750u, 23250u, 25000u
};

/* ==================================================================
 * Module State
 * ================================================================== */

/** Plausibility debounce counter */
static uint8 plaus_debounce;

/** Plausibility fault latch */
static boolean plaus_faulted;

/** Backup cutoff counter */
static uint8 backup_cutoff_counter;

/** Startup grace counter — skip plausibility until signals stabilize */
static uint16 plaus_startup_grace;

/** Creep guard debounce counter (SSR-SC-018) */
static uint8 creep_debounce;

/** Creep guard fault latch — non-clearable, power cycle only (SSR-SC-018) */
static boolean creep_faulted;

/* ==================================================================
 * Internal: Lookup expected current for given torque percentage
 * ================================================================== */

/**
 * @brief  Linear interpolation in the torque-to-current lookup table
 * @param  torque_pct  Torque percentage (0-100)
 * @return Expected motor current in mA
 */
static uint16 lookup_expected_current(uint8 torque_pct)
{
    uint8 i;

    if (torque_pct == 0u) {
        return 0u;
    }
    if (torque_pct >= 100u) {
        return 25000u;
    }

    /* Find the bracketing LUT entries */
    for (i = 1u; i < SC_TORQUE_LUT_SIZE; i++) {
        if (torque_pct <= torque_pct_lut[i]) {
            /* Linear interpolation between [i-1] and [i] */
            uint16 pct_low  = (uint16)torque_pct_lut[i - 1u];
            uint16 pct_high = (uint16)torque_pct_lut[i];
            uint16 cur_low  = current_ma_lut[i - 1u];
            uint16 cur_high = current_ma_lut[i];
            uint16 pct_range = pct_high - pct_low;

            if (pct_range == 0u) {
                return cur_low;
            }

            uint16 frac = (uint16)torque_pct - pct_low;
            uint32 interp = (uint32)cur_low +
                            (((uint32)(cur_high - cur_low) * (uint32)frac) /
                             (uint32)pct_range);
            return (uint16)interp;
        }
    }

    return 25000u;  /* Should not reach here */
}

/* ==================================================================
 * Internal: Check if current deviates from expected
 * ================================================================== */

static boolean is_implausible(uint16 expected_ma, uint16 actual_ma)
{
    uint32 diff;
    uint32 threshold;

    /* Compute absolute difference */
    if (actual_ma > expected_ma) {
        diff = (uint32)actual_ma - (uint32)expected_ma;
    } else {
        diff = (uint32)expected_ma - (uint32)actual_ma;
    }

    /* Near-zero expected: use absolute threshold */
    if (expected_ma < 100u) {
        return (diff > (uint32)SC_PLAUS_ABS_THRESHOLD_MA) ? TRUE : FALSE;
    }

    /* Relative threshold: 20% of expected */
    threshold = ((uint32)expected_ma * (uint32)SC_PLAUS_REL_THRESHOLD) / 100u;

    /* Also apply absolute threshold as floor */
    if (threshold < (uint32)SC_PLAUS_ABS_THRESHOLD_MA) {
        threshold = (uint32)SC_PLAUS_ABS_THRESHOLD_MA;
    }

    return (diff > threshold) ? TRUE : FALSE;
}

/* ==================================================================
 * Public API
 * ================================================================== */

void SC_Plausibility_Init(void)
{
    plaus_debounce        = 0u;
    plaus_faulted         = FALSE;
    backup_cutoff_counter = 0u;
    plaus_startup_grace   = SC_HB_STARTUP_GRACE_TICKS;
    creep_debounce        = 0u;
    creep_faulted         = FALSE;
}

void SC_Plausibility_Check(void)
{
    uint8 veh_data[SC_CAN_DLC];
    uint8 cur_data[SC_CAN_DLC];
    uint8 dlc;
    boolean veh_ok;
    boolean cur_ok;
    uint8 torque_pct;
    uint16 actual_current_ma;
    uint16 expected_current_ma;

    /* If already faulted, nothing more to do (latched) */
    if (plaus_faulted == TRUE) {
        return;
    }

    /* Startup grace — let CAN signals stabilize before checking */
    if (plaus_startup_grace > 0u) {
        plaus_startup_grace--;
        return;
    }

    /* Read torque from Vehicle_State (0x100) byte 4 */
    veh_ok = SC_CAN_GetMessage(SC_MB_IDX_VEHICLE_STATE, veh_data, &dlc);

    /* Read current from Motor_Current (0x301) bytes 2-3 (little-endian) */
    cur_ok = SC_CAN_GetMessage(SC_MB_IDX_MOTOR_CURRENT, cur_data, &dlc);

    if ((veh_ok == TRUE) && (cur_ok == TRUE)) {
        torque_pct = veh_data[4];
        actual_current_ma = ((uint16)cur_data[3] << 8u) |
                            (uint16)cur_data[2];

        expected_current_ma = lookup_expected_current(torque_pct);

        if (is_implausible(expected_current_ma, actual_current_ma) == TRUE) {
            plaus_debounce++;
        } else {
            plaus_debounce = 0u;
        }

        if (plaus_debounce >= SC_PLAUS_DEBOUNCE_TICKS) {
            plaus_faulted = TRUE;
            gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_SYS, 1u);
        }
    }

    /* Backup cutoff check (SWR-SC-024):
     * If FZC brake fault AND motor current > 1000mA for 100ms */
    if ((cur_ok == TRUE) && (SC_Heartbeat_IsFzcBrakeFault() == TRUE)) {
        actual_current_ma = ((uint16)cur_data[3] << 8u) |
                            (uint16)cur_data[2];

        if (actual_current_ma > SC_BACKUP_CUTOFF_CURRENT_MA) {
            backup_cutoff_counter++;
            if (backup_cutoff_counter >= SC_BACKUP_CUTOFF_TICKS) {
                plaus_faulted = TRUE;
                gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_SYS, 1u);
            }
        } else {
            backup_cutoff_counter = 0u;
        }
    } else {
        backup_cutoff_counter = 0u;
    }
}

boolean SC_Plausibility_IsFaulted(void)
{
    return plaus_faulted;
}

void SC_CreepGuard_Check(void)
{
    uint8 veh_data[SC_CAN_DLC];
    uint8 cur_data[SC_CAN_DLC];
    uint8 dlc;
    boolean veh_ok;
    boolean cur_ok;
    uint8 torque_pct;
    uint16 motor_current_ma;

    /* Non-clearable latch — once faulted, stay faulted until power cycle */
    if (creep_faulted == TRUE) {
        return;
    }

    /* Share startup grace with main plausibility — signals must stabilize first */
    if (plaus_startup_grace > 0u) {
        return;
    }

    veh_ok = SC_CAN_GetMessage(SC_MB_IDX_VEHICLE_STATE, veh_data, &dlc);
    cur_ok = SC_CAN_GetMessage(SC_MB_IDX_MOTOR_CURRENT, cur_data, &dlc);

    if ((veh_ok != TRUE) || (cur_ok != TRUE)) {
        /* Missing CAN data — heartbeat/bus-silence monitors handle this */
        return;
    }

    torque_pct = veh_data[4];
    motor_current_ma = ((uint16)cur_data[3] << 8u) | (uint16)cur_data[2];

    /* SSR-SC-018: torque command is zero but motor draws current → FET short */
    if ((torque_pct == 0u) && (motor_current_ma > SC_CREEP_CURRENT_THRESH)) {
        creep_debounce++;
        if (creep_debounce >= SC_CREEP_DEBOUNCE_CYCLES) {
            creep_faulted = TRUE;
            gioSetBit(SC_GIO_PORT_A, SC_PIN_LED_SYS, 1u);
        }
    } else {
        creep_debounce = 0u;
    }
}

boolean SC_Plausibility_IsCreepFaulted(void)
{
    return creep_faulted;
}
