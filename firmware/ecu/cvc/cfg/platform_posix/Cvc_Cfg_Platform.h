/**
 * @file    Cvc_Cfg_Platform.h
 * @brief   CVC platform-specific constants — POSIX (SIL/Docker)
 * @date    2026-03-10
 *
 * @details Timing and threshold constants tuned for Docker SIL simulation.
 *          Selected via -I path ordering in Makefile.posix.
 *          Target version: firmware/cvc/cfg/platform_target/Cvc_Cfg_Platform.h
 *
 * @standard AUTOSAR EcuC pre-compile parameter pattern
 * @copyright Taktflow Systems 2026
 */
#ifndef CVC_CFG_PLATFORM_H
#define CVC_CFG_PLATFORM_H

/** @brief  INIT hold: 1000 × 10ms = 10s (Docker boot is slower) */
#ifndef CVC_INIT_HOLD_CYCLES
  #define CVC_INIT_HOLD_CYCLES           1000u
#endif

/** @brief  Post-INIT grace: 1000 × 10ms = 10s (Docker fault signal settling) */
#ifndef CVC_POST_INIT_GRACE_CYCLES
  #define CVC_POST_INIT_GRACE_CYCLES     1000u
#endif

/** @brief  E2E SM FZC window: 16 slots (max E2E_SM_MAX_WINDOW) — absorbs
 *          Docker CPU scheduling jitter at SIL_TIME_SCALE=10 */
#ifndef CVC_E2E_SM_FZC_WINDOW
  #define CVC_E2E_SM_FZC_WINDOW          16u
#endif

/** @brief  E2E SM FZC max errors: tolerate 14/16 missed — need 2+ OKs */
#ifndef CVC_E2E_SM_FZC_MAX_ERR_VALID
  #define CVC_E2E_SM_FZC_MAX_ERR_VALID   14u
#endif

/** @brief  E2E SM RZC window: 16 slots — same Docker jitter tolerance */
#ifndef CVC_E2E_SM_RZC_WINDOW
  #define CVC_E2E_SM_RZC_WINDOW          16u
#endif

/** @brief  E2E SM RZC max errors: tolerate 14/16 missed — need 2+ OKs */
#ifndef CVC_E2E_SM_RZC_MAX_ERR_VALID
  #define CVC_E2E_SM_RZC_MAX_ERR_VALID   14u
#endif

/** @brief  Creep guard debounce: 200 × 10ms = 2s SIL.
 *          Motor RPM feedback via CAN has 100-200ms wall latency in CI. */
#define CVC_CREEP_DEBOUNCE_TICKS         200u

#endif /* CVC_CFG_PLATFORM_H */
