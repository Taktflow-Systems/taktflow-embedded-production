/**
 * @file    Fzc_Cfg_Platform.h
 * @brief   FZC platform-specific constants — POSIX (SIL/Docker)
 * @date    2026-03-10
 *
 * @details Timing and threshold constants tuned for Docker SIL simulation.
 *          Selected via -I path ordering in Makefile.posix.
 *          Target version: firmware/fzc/cfg/platform_target/Fzc_Cfg_Platform.h
 *
 * @standard AUTOSAR EcuC pre-compile parameter pattern
 * @copyright Taktflow Systems 2026
 */
#ifndef FZC_CFG_PLATFORM_H
#define FZC_CFG_PLATFORM_H

/** @brief  Steering plausibility debounce: 50 cycles = 500ms (CAN feedback jitter) */
#define FZC_STEER_PLAUS_DEBOUNCE         50u

/** @brief  Brake fault debounce: 50 cycles = 500ms (CAN feedback jitter) */
#define FZC_BRAKE_FAULT_DEBOUNCE         50u

/** @brief  Post-INIT grace: 1500 × 10ms = 15s (startup transient absorption).
 *          Must exceed the SIL reset boot fan-out: FZC t=0, SC t=+3s,
 *          CVC t=+6s, then CVC's own post-INIT grace (10s) before STEER/BRAKE
 *          commands flow. 15s gives comfortable margin on shared-CPU VPS. */
#ifndef FZC_POST_INIT_GRACE_CYCLES
  #define FZC_POST_INIT_GRACE_CYCLES     1500u
#endif

#endif /* FZC_CFG_PLATFORM_H */
