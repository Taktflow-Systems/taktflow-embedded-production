/**
 * @file    Sc_Cfg_Platform.h
 * @brief   SC platform-specific constants — TMS570 target (bare metal)
 * @date    2026-03-10
 *
 * @details Timing constants for real hardware.
 *          Selected via -I path ordering in Makefile (TMS570 build).
 *          POSIX version: firmware/sc/cfg/platform_posix/Sc_Cfg_Platform.h
 *
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_CFG_PLATFORM_H
#define SC_CFG_PLATFORM_H

/** @brief  Heartbeat timeout: 100ms = 2x 50ms heartbeat period (SG-008 FTTI) */
#ifndef SC_HB_TIMEOUT_TICKS
  #define SC_HB_TIMEOUT_TICKS       10u
#endif

/** @brief  Heartbeat confirmation: 30ms */
#ifndef SC_HB_CONFIRM_TICKS
  #define SC_HB_CONFIRM_TICKS       3u
#endif

/** @brief  Startup grace: 5s (must >= CVC INIT hold) */
#ifndef SC_HB_STARTUP_GRACE_TICKS
  #define SC_HB_STARTUP_GRACE_TICKS 500u
#endif

/** @brief  Plausibility debounce: 50ms (real FOC inverter <1ms response) */
#ifndef SC_PLAUS_DEBOUNCE_TICKS
  #define SC_PLAUS_DEBOUNCE_TICKS   5u
#endif

#endif /* SC_CFG_PLATFORM_H */
