/**
 * @file    Sc_Cfg_Platform.h
 * @brief   SC platform-specific constants — POSIX (SIL/Docker)
 * @date    2026-03-10
 *
 * @details Timing constants tuned for Docker SIL simulation.
 *          Selected via -I path ordering in Makefile.posix.
 *          Target version: firmware/sc/cfg/platform_target/Sc_Cfg_Platform.h
 *
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_CFG_PLATFORM_H
#define SC_CFG_PLATFORM_H

/** @brief  Heartbeat timeout: 1500ms (SIL_TIME_SCALE=10, Docker jitter) */
#ifndef SC_HB_TIMEOUT_TICKS
  #define SC_HB_TIMEOUT_TICKS       150u
#endif

/** @brief  Heartbeat confirmation: 200ms (wider for SIL scheduling) */
#ifndef SC_HB_CONFIRM_TICKS
  #define SC_HB_CONFIRM_TICKS       20u
#endif

/** @brief  Startup grace: 15s (sequential Docker container restarts) */
#ifndef SC_HB_STARTUP_GRACE_TICKS
  #define SC_HB_STARTUP_GRACE_TICKS 1500u
#endif

/** @brief  Plausibility debounce: 100ms (SIL CAN round-trip ~20-30ms) */
#ifndef SC_PLAUS_DEBOUNCE_TICKS
  #define SC_PLAUS_DEBOUNCE_TICKS   10u
#endif

#endif /* SC_CFG_PLATFORM_H */
