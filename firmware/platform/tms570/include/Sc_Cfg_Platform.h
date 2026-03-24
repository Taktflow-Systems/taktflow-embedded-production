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

/** @brief  Heartbeat timeout: 100ms production, 500ms HIL.
 *          HIL: DCAN silent mode + gs_usb bridge adds jitter. */
#ifndef SC_HB_TIMEOUT_TICKS
  #ifdef PLATFORM_HIL
    #define SC_HB_TIMEOUT_TICKS     50u   /* 500ms — tolerates HIL jitter */
  #else
    #define SC_HB_TIMEOUT_TICKS     10u   /* 100ms — 2x 50ms heartbeat (SG-008 FTTI) */
  #endif
#endif

/** @brief  Heartbeat confirmation: 30ms production, 100ms HIL */
#ifndef SC_HB_CONFIRM_TICKS
  #ifdef PLATFORM_HIL
    #define SC_HB_CONFIRM_TICKS     10u
  #else
    #define SC_HB_CONFIRM_TICKS     3u
  #endif
#endif

/** @brief  Startup grace: 5s production, 10s HIL (Docker vECUs need time) */
#ifndef SC_HB_STARTUP_GRACE_TICKS
  #ifdef PLATFORM_HIL
    #define SC_HB_STARTUP_GRACE_TICKS 1000u  /* 10s — wait for Docker vECUs */
  #else
    #define SC_HB_STARTUP_GRACE_TICKS 500u   /* 5s — production */
  #endif
#endif

/** @brief  Plausibility debounce: 50ms (real FOC inverter <1ms response) */
#ifndef SC_PLAUS_DEBOUNCE_TICKS
  #define SC_PLAUS_DEBOUNCE_TICKS   5u
#endif

#endif /* SC_CFG_PLATFORM_H */
