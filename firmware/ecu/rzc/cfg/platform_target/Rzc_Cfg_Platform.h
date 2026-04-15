/**
 * @file    Rzc_Cfg_Platform.h
 * @brief   RZC platform-specific constants — STM32 target (bare metal)
 * @date    2026-04-15
 *
 * @details Timing and threshold constants for real hardware.
 *          Selected via -I path ordering in Makefile.stm32
 *          (`-I$(ECU)/rzc/cfg/platform_target` is on the ARM include path).
 *
 *          POSIX version: firmware/ecu/rzc/cfg/platform_posix/Rzc_Cfg_Platform.h
 *
 *          Phase 5 Line B D7 seed — RZC currently carries no
 *          platform-parameterised thresholds, so this header only
 *          declares the include guard. As hardware-specific tuning
 *          lands in future phases (motor derating, temp cut-off, ACS
 *          zero-offset calibration windows), the platform constants go
 *          here and the POSIX sibling gets a relaxed tuning.
 *
 * @standard AUTOSAR EcuC pre-compile parameter pattern
 * @copyright Taktflow Systems 2026
 */
#ifndef RZC_CFG_PLATFORM_H
#define RZC_CFG_PLATFORM_H

/* No platform-specific constants yet — see file header. This header
 * exists so that TARGET=rzc has a parallel -I include path to cvc/fzc
 * and downstream code can add platform-gated defines without having to
 * create the directory in a follow-up PR. */

#endif /* RZC_CFG_PLATFORM_H */
