/**
 * @file    Fzc_Cfg_Platform.h
 * @brief   FZC platform-specific constants — STM32 target (bare metal)
 * @date    2026-03-10
 *
 * @details Timing and threshold constants for real hardware.
 *          Selected via -I path ordering in Makefile.stm32.
 *          POSIX version: firmware/fzc/cfg/platform_posix/Fzc_Cfg_Platform.h
 *
 * @standard AUTOSAR EcuC pre-compile parameter pattern
 * @copyright Taktflow Systems 2026
 */
#ifndef FZC_CFG_PLATFORM_H
#define FZC_CFG_PLATFORM_H

/** @brief  Steering plausibility debounce: 5 cycles (direct ADC, no CAN jitter) */
#define FZC_STEER_PLAUS_DEBOUNCE         5u

/** @brief  Brake fault debounce: 3 cycles (direct ADC) */
#define FZC_BRAKE_FAULT_DEBOUNCE         3u

/** @brief  Post-INIT grace: 0 (bare metal — no grace needed) */
#ifndef FZC_POST_INIT_GRACE_CYCLES
  #define FZC_POST_INIT_GRACE_CYCLES     0u
#endif

#endif /* FZC_CFG_PLATFORM_H */
