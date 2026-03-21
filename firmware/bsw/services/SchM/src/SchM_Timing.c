/**
 * @file    SchM_Timing.c
 * @brief   WCET measurement implementation
 * @date    2026-03-21
 *
 * @details Platform-abstracted cycle counter:
 *          - Cortex-M4/M7 (STM32): DWT->CYCCNT (CPU clock cycles)
 *          - TMS570: RTI free-running counter
 *          - POSIX (SIL): clock_gettime(CLOCK_MONOTONIC)
 *
 * @copyright Taktflow Systems 2026
 */
#include "SchM_Timing.h"

/* ---- Results (global, volatile for XCP access) ---- */

volatile uint32 g_timing_max_us[TIMING_MAX_IDS];
volatile uint32 g_timing_last_us[TIMING_MAX_IDS];
volatile uint32 g_timing_count[TIMING_MAX_IDS];

/* ---- Internal: per-point start timestamp ---- */
static uint32 timing_start[TIMING_MAX_IDS];

/* ---- Platform-specific cycle counter ---- */

#if defined(PLATFORM_POSIX)

#include <time.h>

static uint32 get_cycles(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    /* Return microseconds (wraps every ~4295 seconds, fine for WCET) */
    return (uint32)(ts.tv_sec * 1000000u + ts.tv_nsec / 1000u);
}

#define CYCLES_TO_US(cycles)  (cycles)  /* Already in microseconds */

#elif defined(PLATFORM_STM32)

/* Cortex-M DWT cycle counter */
#define DWT_CTRL   (*(volatile uint32*)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32*)0xE0001004u)
#define SCB_DEMCR  (*(volatile uint32*)0xE000EDFCu)

static uint32 cpu_freq_mhz = 170u;  /* STM32G474 = 170 MHz, F413 = 100 MHz */

static uint32 get_cycles(void)
{
    return DWT_CYCCNT;
}

#define CYCLES_TO_US(cycles)  ((cycles) / cpu_freq_mhz)

#elif defined(PLATFORM_TMS570)

/* RTI free-running counter (up counter, RTICLK = VCLK/2) */
#define RTIFRC0  (*(volatile uint32*)0xFFFFFC10u)

static uint32 rticlk_mhz = 40u;  /* TMS570LC4357: VCLK=80MHz, RTICLK=40MHz */

static uint32 get_cycles(void)
{
    return RTIFRC0;
}

#define CYCLES_TO_US(cycles)  ((cycles) / rticlk_mhz)

#else

/* Fallback: no cycle counter available */
static uint32 get_cycles(void) { return 0u; }
#define CYCLES_TO_US(cycles)  (0u)

#endif

/* ---- API Implementation ---- */

void SchM_TimingInit(void)
{
    uint8 i;
    for (i = 0u; i < TIMING_MAX_IDS; i++) {
        g_timing_max_us[i]  = 0u;
        g_timing_last_us[i] = 0u;
        g_timing_count[i]   = 0u;
        timing_start[i]     = 0u;
    }

#if defined(PLATFORM_STM32)
    /* Enable DWT cycle counter */
    SCB_DEMCR |= 0x01000000u;  /* TRCENA */
    DWT_CTRL  |= 0x00000001u;  /* CYCCNTENA */
    DWT_CYCCNT = 0u;
#endif
}

void SchM_TimingStart(uint8 TimingId)
{
    if (TimingId < TIMING_MAX_IDS) {
        timing_start[TimingId] = get_cycles();
    }
}

void SchM_TimingStop(uint8 TimingId)
{
    uint32 elapsed_us;

    if (TimingId >= TIMING_MAX_IDS) {
        return;
    }

    elapsed_us = CYCLES_TO_US(get_cycles() - timing_start[TimingId]);

    g_timing_last_us[TimingId] = elapsed_us;
    g_timing_count[TimingId]++;

    if (elapsed_us > g_timing_max_us[TimingId]) {
        g_timing_max_us[TimingId] = elapsed_us;
    }
}

void SchM_TimingReset(void)
{
    uint8 i;
    for (i = 0u; i < TIMING_MAX_IDS; i++) {
        g_timing_max_us[i]  = 0u;
        g_timing_last_us[i] = 0u;
        g_timing_count[i]   = 0u;
    }
}
