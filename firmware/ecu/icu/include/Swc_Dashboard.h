/**
 * @file    Swc_Dashboard.h
 * @brief   Instrument cluster dashboard SWC — gauges, warnings, ECU health
 * @date    2026-02-23
 *
 * @safety_req SWR-ICU-002, SWR-ICU-003, SWR-ICU-004, SWR-ICU-005,
 *             SWR-ICU-006, SWR-ICU-007, SWR-ICU-009
 * @traces_to  SSR-ICU-002 to SSR-ICU-007, SSR-ICU-009
 *
 * @note    QM safety level — display fault does not affect vehicle operation.
 *          ICU is a listen-only consumer.
 *
 * @copyright Taktflow Systems 2026
 * @standard  AUTOSAR, ISO 26262 Part 6
 */
#ifndef SWC_DASHBOARD_H
#define SWC_DASHBOARD_H

#include "Std_Types.h"

/* ==================================================================
 * Temperature zones
 * ================================================================== */

#define DASH_ZONE_GREEN   0u
#define DASH_ZONE_YELLOW  1u
#define DASH_ZONE_ORANGE  2u
#define DASH_ZONE_RED     3u

/* ==================================================================
 * Warning flags (bitmask)
 * ================================================================== */

#define DASH_WARN_CHECK_ENGINE  0x01u
#define DASH_WARN_TEMPERATURE   0x02u
#define DASH_WARN_BATTERY       0x04u
#define DASH_WARN_ESTOP         0x08u
#define DASH_WARN_OVERCURRENT   0x10u

/* ==================================================================
 * Public API
 * ================================================================== */

/**
 * @brief  Initialize dashboard — reset all computed values
 */
void Swc_Dashboard_Init(void);

/**
 * @brief  Dashboard cyclic main function — called every 50ms (20 Hz)
 *
 * Reads all vehicle signals via Rte_Read, computes display values
 * (speed, torque, zones, warnings, ECU health), and renders to
 * ncurses terminal UI.
 */
void Swc_Dashboard_50ms(void);

/**
 * @brief  Get temperature zone from temperature value
 * @param  temp  Motor temperature in degrees C
 * @return Zone enum: DASH_ZONE_GREEN, YELLOW, ORANGE, or RED
 */
uint8 Dashboard_GetTempZone(uint32 temp);

/**
 * @brief  Get battery voltage zone from millivolt reading
 * @param  millivolts  Battery voltage in mV
 * @return Zone enum: DASH_ZONE_GREEN, YELLOW, or RED
 */
uint8 Dashboard_GetBatteryZone(uint32 millivolts);

/**
 * @brief  Compute warning flags from current signal state
 * @return Bitmask of DASH_WARN_* flags
 */
uint8 Dashboard_GetWarnings(void);

/**
 * @brief  Get human-readable string for vehicle state enum
 * @param  state  Vehicle state value (0-5)
 * @return Pointer to static string, "UNKNOWN" if out of range
 */
const char* Dashboard_GetVehicleStateStr(uint8 state);

#endif /* SWC_DASHBOARD_H */
