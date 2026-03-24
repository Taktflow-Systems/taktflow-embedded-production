/**
 * @file    Cvc_Cfg_Platform.h
 * @brief   CVC platform-specific constants — STM32 target (bare metal)
 * @date    2026-03-10
 *
 * @details Timing and threshold constants for real hardware.
 *          Selected via -I path ordering in Makefile.stm32.
 *          POSIX version: firmware/cvc/cfg/platform_posix/Cvc_Cfg_Platform.h
 *
 * @standard AUTOSAR EcuC pre-compile parameter pattern
 * @copyright Taktflow Systems 2026
 */
#ifndef CVC_CFG_PLATFORM_H
#define CVC_CFG_PLATFORM_H

/** @brief  INIT hold: 500 × 10ms = 5s */
#ifndef CVC_INIT_HOLD_CYCLES
  #define CVC_INIT_HOLD_CYCLES           500u
#endif

/** @brief  Post-INIT grace: absorb SC startup delay before checking relay.
 *          0 on production (all ECUs power simultaneously).
 *          1500 on HIL (SC needs 10s grace + CVC self-test delay). */
#ifndef CVC_POST_INIT_GRACE_CYCLES
  #ifdef PLATFORM_HIL
    #define CVC_POST_INIT_GRACE_CYCLES   1500u   /* 15s @ 10ms */
  #else
    #define CVC_POST_INIT_GRACE_CYCLES   0u
  #endif
#endif

/** @brief  E2E SM FZC window: 4 × 50ms = 200ms */
#ifndef CVC_E2E_SM_FZC_WINDOW
  #define CVC_E2E_SM_FZC_WINDOW          4u
#endif

/** @brief  E2E SM FZC max errors: >1 error → INVALID */
#ifndef CVC_E2E_SM_FZC_MAX_ERR_VALID
  #define CVC_E2E_SM_FZC_MAX_ERR_VALID   1u
#endif

/** @brief  E2E SM RZC window: 6 × 50ms = 300ms */
#ifndef CVC_E2E_SM_RZC_WINDOW
  #define CVC_E2E_SM_RZC_WINDOW          6u
#endif

/** @brief  E2E SM RZC max errors: >2 errors → INVALID */
#ifndef CVC_E2E_SM_RZC_MAX_ERR_VALID
  #define CVC_E2E_SM_RZC_MAX_ERR_VALID   2u
#endif

/** @brief  Creep guard debounce: 20 × 10ms = 200ms HW */
#define CVC_CREEP_DEBOUNCE_TICKS         20u

#endif /* CVC_CFG_PLATFORM_H */
