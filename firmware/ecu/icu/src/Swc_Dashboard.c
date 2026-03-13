/**
 * @file    Swc_Dashboard.c
 * @brief   Instrument cluster dashboard — gauges, warnings, ECU health
 * @date    2026-02-23
 *
 * @safety_req SWR-ICU-002, SWR-ICU-003, SWR-ICU-004, SWR-ICU-005,
 *             SWR-ICU-006, SWR-ICU-007, SWR-ICU-009
 * @traces_to  SSR-ICU-002 to SSR-ICU-007, SSR-ICU-009
 *
 * @details  Reads all vehicle signals via Rte_Read, computes display
 *           values (speed from RPM, torque %, temperature zones,
 *           battery zones, warning flags, ECU heartbeat health), and
 *           renders the dashboard via ncurses terminal UI.
 *
 *           ncurses rendering is guarded by PLATFORM_POSIX_TEST so
 *           unit tests exercise data logic only.
 *
 * @copyright Taktflow Systems 2026
 * @standard  AUTOSAR, ISO 26262 Part 6
 */

#include "Swc_Dashboard.h"
#include "Icu_Cfg.h"

/* ==================================================================
 * BSW includes
 * ================================================================== */

#include "Rte.h"

/* ==================================================================
 * Vehicle state name lookup table
 * ================================================================== */

#define ICU_VSTATE_INIT       0u
#define ICU_VSTATE_RUN        1u
#define ICU_VSTATE_DEGRADED   2u
#define ICU_VSTATE_LIMP       3u
#define ICU_VSTATE_SAFE_STOP  4u
#define ICU_VSTATE_SHUTDOWN   5u
#define ICU_VSTATE_COUNT      6u

static const char* const vstate_names[ICU_VSTATE_COUNT] = {
    "INIT",       /* 0 */
    "RUN",        /* 1 */
    "DEGRADED",   /* 2 */
    "LIMP",       /* 3 */
    "SAFE_STOP",  /* 4 */
    "SHUTDOWN"    /* 5 */
};

/* ==================================================================
 * Dashboard computed data (static struct for getter access in tests)
 * ================================================================== */

typedef struct {
    uint32  display_speed;      /**< Computed speed from RPM (km/h)     */
    uint32  display_torque;     /**< Torque percentage 0-100            */
    uint8   temp_zone;          /**< Temperature zone enum              */
    uint8   battery_zone;       /**< Battery voltage zone enum          */
    uint8   warning_flags;      /**< Bitmask of DASH_WARN_* flags      */
    uint32  motor_temp;         /**< Raw motor temperature (C)          */
    uint32  battery_mv;         /**< Raw battery voltage (mV)           */
    uint8   vehicle_state;      /**< Current vehicle state enum         */
    boolean ecu_health_cvc;     /**< TRUE if CVC heartbeat alive        */
    boolean ecu_health_fzc;     /**< TRUE if FZC heartbeat alive        */
    boolean ecu_health_rzc;     /**< TRUE if RZC heartbeat alive        */
} DashData_t;

static DashData_t dash_data;

/* ==================================================================
 * Heartbeat tracking for ECU health
 * ================================================================== */

static uint32 last_hb_cvc;         /**< Last seen CVC heartbeat counter  */
static uint32 last_hb_fzc;         /**< Last seen FZC heartbeat counter  */
static uint32 last_hb_rzc;         /**< Last seen RZC heartbeat counter  */
static uint8  hb_stale_cvc;        /**< Consecutive stale ticks for CVC  */
static uint8  hb_stale_fzc;        /**< Consecutive stale ticks for FZC  */
static uint8  hb_stale_rzc;        /**< Consecutive stale ticks for RZC  */

static boolean initialized;        /**< TRUE after successful Init       */

/* ==================================================================
 * Dashboard_GetTempZone
 * ================================================================== */

uint8 Dashboard_GetTempZone(uint32 temp)
{
    uint8 zone;

    if (temp <= ICU_TEMP_GREEN_MAX) {
        zone = DASH_ZONE_GREEN;
    } else if (temp <= ICU_TEMP_YELLOW_MAX) {
        zone = DASH_ZONE_YELLOW;
    } else if (temp <= ICU_TEMP_ORANGE_MAX) {
        zone = DASH_ZONE_ORANGE;
    } else {
        zone = DASH_ZONE_RED;
    }

    return zone;
}

/* ==================================================================
 * Dashboard_GetBatteryZone
 * ================================================================== */

uint8 Dashboard_GetBatteryZone(uint32 millivolts)
{
    uint8 zone;

    if (millivolts > ICU_BATT_GREEN_MIN) {
        zone = DASH_ZONE_GREEN;
    } else if (millivolts >= ICU_BATT_YELLOW_MIN) {
        zone = DASH_ZONE_YELLOW;
    } else {
        zone = DASH_ZONE_RED;
    }

    return zone;
}

/* ==================================================================
 * Dashboard_GetWarnings — compute warning bitmask from signals
 * ================================================================== */

uint8 Dashboard_GetWarnings(void)
{
    uint8 flags = 0u;

    /* Check engine: any DTC broadcast non-zero */
    {
        uint32 dtc_bcast = 0u;
        (void)Rte_Read(ICU_SIG_DTC_BROADCAST, &dtc_bcast);
        if (dtc_bcast != 0u) {
            flags |= DASH_WARN_CHECK_ENGINE;
        }
    }

    /* Temperature warning: orange or red zone */
    if ((dash_data.temp_zone == DASH_ZONE_ORANGE) ||
        (dash_data.temp_zone == DASH_ZONE_RED)) {
        flags |= DASH_WARN_TEMPERATURE;
    }

    /* Battery warning: red zone */
    if (dash_data.battery_zone == DASH_ZONE_RED) {
        flags |= DASH_WARN_BATTERY;
    }

    /* E-stop active */
    {
        uint32 estop = 0u;
        (void)Rte_Read(ICU_SIG_ESTOP_ACTIVE, &estop);
        if (estop != 0u) {
            flags |= DASH_WARN_ESTOP;
        }
    }

    /* Overcurrent */
    {
        uint32 overcurrent = 0u;
        (void)Rte_Read(ICU_SIG_OVERCURRENT_FLAG, &overcurrent);
        if (overcurrent != 0u) {
            flags |= DASH_WARN_OVERCURRENT;
        }
    }

    return flags;
}

/* ==================================================================
 * Dashboard_GetVehicleStateStr
 * ================================================================== */

const char* Dashboard_GetVehicleStateStr(uint8 state)
{
    if (state < ICU_VSTATE_COUNT) {
        return vstate_names[state];
    }
    return "UNKNOWN";
}

/* ==================================================================
 * Heartbeat health check — detect stale counters
 * ================================================================== */

static void Dashboard_UpdateEcuHealth(void)
{
    uint32 hb_cvc = 0u;
    uint32 hb_fzc = 0u;
    uint32 hb_rzc = 0u;

    (void)Rte_Read(ICU_SIG_HEARTBEAT_CVC, &hb_cvc);
    (void)Rte_Read(ICU_SIG_HEARTBEAT_FZC, &hb_fzc);
    (void)Rte_Read(ICU_SIG_HEARTBEAT_RZC, &hb_rzc);

    /* CVC heartbeat check */
    if (hb_cvc != last_hb_cvc) {
        last_hb_cvc   = hb_cvc;
        hb_stale_cvc  = 0u;
        dash_data.ecu_health_cvc = TRUE;
    } else {
        if (hb_stale_cvc < 255u) {
            hb_stale_cvc++;
        }
        if (hb_stale_cvc > ICU_HB_TIMEOUT_TICKS) {
            dash_data.ecu_health_cvc = FALSE;
        }
    }

    /* FZC heartbeat check */
    if (hb_fzc != last_hb_fzc) {
        last_hb_fzc   = hb_fzc;
        hb_stale_fzc  = 0u;
        dash_data.ecu_health_fzc = TRUE;
    } else {
        if (hb_stale_fzc < 255u) {
            hb_stale_fzc++;
        }
        if (hb_stale_fzc > ICU_HB_TIMEOUT_TICKS) {
            dash_data.ecu_health_fzc = FALSE;
        }
    }

    /* RZC heartbeat check */
    if (hb_rzc != last_hb_rzc) {
        last_hb_rzc   = hb_rzc;
        hb_stale_rzc  = 0u;
        dash_data.ecu_health_rzc = TRUE;
    } else {
        if (hb_stale_rzc < 255u) {
            hb_stale_rzc++;
        }
        if (hb_stale_rzc > ICU_HB_TIMEOUT_TICKS) {
            dash_data.ecu_health_rzc = FALSE;
        }
    }
}

/* ==================================================================
 * Swc_Dashboard_Init
 * ================================================================== */

void Swc_Dashboard_Init(void)
{
    dash_data.display_speed   = 0u;
    dash_data.display_torque  = 0u;
    dash_data.temp_zone       = DASH_ZONE_GREEN;
    dash_data.battery_zone    = DASH_ZONE_GREEN;
    dash_data.warning_flags   = 0u;
    dash_data.motor_temp      = 0u;
    dash_data.battery_mv      = 0u;
    dash_data.vehicle_state   = 0u;
    dash_data.ecu_health_cvc  = TRUE;
    dash_data.ecu_health_fzc  = TRUE;
    dash_data.ecu_health_rzc  = TRUE;

    last_hb_cvc   = 0u;
    last_hb_fzc   = 0u;
    last_hb_rzc   = 0u;
    hb_stale_cvc  = 0u;
    hb_stale_fzc  = 0u;
    hb_stale_rzc  = 0u;

    initialized = TRUE;
}

/* ==================================================================
 * Swc_Dashboard_50ms — called every 50ms (20 Hz refresh)
 * ================================================================== */

void Swc_Dashboard_50ms(void)
{
    uint32 motor_rpm    = 0u;
    uint32 torque_pct   = 0u;
    uint32 motor_temp   = 0u;
    uint32 battery_mv   = 0u;
    uint32 vstate       = 0u;

    if (initialized != TRUE) {
        return;
    }

    /* ---- Read all vehicle signals ---- */
    (void)Rte_Read(ICU_SIG_MOTOR_RPM,       &motor_rpm);
    (void)Rte_Read(ICU_SIG_TORQUE_PCT,      &torque_pct);
    (void)Rte_Read(ICU_SIG_MOTOR_TEMP,      &motor_temp);
    (void)Rte_Read(ICU_SIG_BATTERY_VOLTAGE, &battery_mv);
    (void)Rte_Read(ICU_SIG_VEHICLE_STATE,   &vstate);

    /* ---- Compute display values ---- */

    /* Speed: motor_rpm * 60 / 1000 (rough km/h for demo wheel) */
    dash_data.display_speed = (motor_rpm * 60u) / 1000u;

    /* Torque: direct percentage 0-100 */
    if (torque_pct > 100u) {
        dash_data.display_torque = 100u;
    } else {
        dash_data.display_torque = torque_pct;
    }

    /* Temperature zone */
    dash_data.motor_temp = motor_temp;
    dash_data.temp_zone  = Dashboard_GetTempZone(motor_temp);

    /* Battery zone */
    dash_data.battery_mv   = battery_mv;
    dash_data.battery_zone = Dashboard_GetBatteryZone(battery_mv);

    /* Vehicle state */
    dash_data.vehicle_state = (uint8)vstate;

    /* Warning flags */
    dash_data.warning_flags = Dashboard_GetWarnings();

    /* ECU health monitoring */
    Dashboard_UpdateEcuHealth();

    /* ---- ncurses rendering (excluded from test builds) ---- */
#ifndef PLATFORM_POSIX_TEST
    {
        /* ncurses rendering would go here:
         * - clear screen
         * - draw speed gauge bar
         * - draw torque gauge bar
         * - draw temperature indicator with zone color
         * - draw battery indicator with zone color
         * - draw warning icons
         * - draw vehicle state text
         * - draw ECU health indicators (CVC / FZC / RZC)
         * - refresh screen
         *
         * TODO:HARDWARE — implement ncurses rendering when
         * Docker container environment is set up
         */
    }
#endif /* PLATFORM_POSIX_TEST */
}
