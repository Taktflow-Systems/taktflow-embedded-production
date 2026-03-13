/**
 * @file    Sil_Time.h
 * @brief   SIL time acceleration via Linux timerfd
 * @date    2026-03-08
 *
 * @details Provides accelerated timing for SIL (Software-in-the-Loop) builds.
 *          Reads SIL_TIME_SCALE environment variable at init (default 1 = real-time).
 *          Uses timerfd_create(CLOCK_MONOTONIC) for precise periodic sleeps.
 *
 *          Scale factor N means: ECU virtual time runs N× faster than wall clock.
 *          - Sil_Time_Sleep(10000) with scale=10 → sleeps 1000 us wall clock
 *          - Sil_Time_GetTickUs() with scale=10 → returns elapsed_us * 10
 *
 * @note    SIL simulation only — not compiled for STM32/TMS570 targets.
 *          Guarded by PLATFORM_POSIX and excluded from PLATFORM_POSIX_TEST.
 *
 * @safety_req N/A — SIL simulation only, not for production
 * @copyright Taktflow Systems 2026
 */

#ifndef SIL_TIME_H
#define SIL_TIME_H

#include "Platform_Types.h"

#if defined(PLATFORM_POSIX) && !defined(PLATFORM_POSIX_TEST)

/**
 * @brief  Initialize time acceleration subsystem
 * @note   Reads SIL_TIME_SCALE env var. Call once at startup before any sleep.
 *         Thread-safe: uses internal guard against double-init.
 */
void Sil_Time_Init(void);

/**
 * @brief  Accelerated sleep — sleeps for period_us / scale wall-clock microseconds
 * @param  period_us  Desired virtual sleep duration in microseconds
 * @note   Uses timerfd for sub-millisecond precision when scale > 1.
 *         Falls back to usleep if timerfd unavailable.
 */
void Sil_Time_Sleep(uint32 period_us);

/**
 * @brief  Get virtual elapsed time since Sil_Time_Init in microseconds
 * @return Virtual elapsed microseconds (wall_elapsed × scale). Wraps at uint32 max.
 */
uint32 Sil_Time_GetTickUs(void);

/**
 * @brief  Get the active time scale factor
 * @return Scale factor (1 = real-time, 10 = 10× acceleration)
 */
uint32 Sil_Time_GetScale(void);

#else

/* No-op stubs for unit tests and non-POSIX builds */
#define Sil_Time_Init()          ((void)0)
#define Sil_Time_Sleep(us)       ((void)0)
#define Sil_Time_GetTickUs()     (0u)
#define Sil_Time_GetScale()      (1u)

#endif /* PLATFORM_POSIX && !PLATFORM_POSIX_TEST */

#endif /* SIL_TIME_H */
