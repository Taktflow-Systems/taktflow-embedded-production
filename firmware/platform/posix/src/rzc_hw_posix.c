/**
 * @file    rzc_hw_posix.c
 * @brief   POSIX hardware stubs for RZC (Rear Zone Controller) SIL simulation
 * @date    2026-02-23
 *
 * @details Implements all Main_Hw_* externs declared in rzc/src/main.c:70-84.
 *          Timing uses clock_gettime(CLOCK_MONOTONIC) for tick measurement.
 *          Self-test functions return E_OK (simulation = no hardware faults).
 *
 * @safety_req N/A — SIL simulation only, not for production
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "Sil_Time.h"

#ifndef PLATFORM_POSIX_TEST
#include <time.h>
#include <unistd.h>
#endif

/* ==================================================================
 * Module state
 * ================================================================== */

static uint32 tick_period_us = 10000u;  /* Default 10ms tick */

/* ==================================================================
 * Timing stubs (Main_Hw_* from main.c:70-74)
 * ================================================================== */

/**
 * @brief  Initialize system clocks — no-op on POSIX
 */
void Main_Hw_SystemClockInit(void)
{
    /* POSIX: no PLL configuration needed */
}

/**
 * @brief  Configure MPU — no-op on POSIX
 */
void Main_Hw_MpuConfig(void)
{
    /* POSIX: no MPU hardware */
}

/**
 * @brief  Initialize SysTick timer — record base timestamp
 * @param  periodUs  Tick period in microseconds
 */
void Main_Hw_SysTickInit(uint32 periodUs)
{
    tick_period_us = periodUs;
#ifndef PLATFORM_POSIX_TEST
    Sil_Time_Init();
#endif
}

/**
 * @brief  Wait for interrupt — timerfd-accelerated sleep on POSIX
 */
void Main_Hw_Wfi(void)
{
#ifndef PLATFORM_POSIX_TEST
    Sil_Time_Sleep(1000u); /* 1ms virtual — actual duration = 1ms / SIL_TIME_SCALE */
#endif
}

/**
 * @brief  Get virtual elapsed time since SysTickInit in microseconds
 * @return Virtual elapsed microseconds (wall_elapsed × SIL_TIME_SCALE)
 */
uint32 Main_Hw_GetTick(void)
{
#ifndef PLATFORM_POSIX_TEST
    return Sil_Time_GetTickUs();
#else
    return 0u;
#endif
}

/* ==================================================================
 * Self-test stubs (Main_Hw_* from main.c:76-84)
 * ================================================================== */

/**
 * @brief  BTS7960 motor driver GPIO self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType Main_Hw_Bts7960GpioTest(void)
{
    return E_OK;
}

/**
 * @brief  ACS723 current sensor zero calibration self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType Main_Hw_Acs723ZeroCalTest(void)
{
    return E_OK;
}

/**
 * @brief  NTC thermistor range self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType Main_Hw_NtcRangeTest(void)
{
    return E_OK;
}

/**
 * @brief  Encoder stuck self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType Main_Hw_EncoderStuckTest(void)
{
    return E_OK;
}

/**
 * @brief  CAN loopback self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType Main_Hw_CanLoopbackTest(void)
{
    return E_OK;
}

/**
 * @brief  MPU verify self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType Main_Hw_MpuVerifyTest(void)
{
    return E_OK;
}

/**
 * @brief  RAM pattern self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType Main_Hw_RamPatternTest(void)
{
    return E_OK;
}

/**
 * @brief  Plant stack canary — no-op on POSIX
 */
void Main_Hw_PlantStackCanary(void)
{
    /* POSIX: stack protection handled by OS */
}

/**
 * @brief  Debug status print — no-op on POSIX (info available via CAN monitor)
 * @param  tick_us  Current tick in microseconds (unused)
 */
void Main_Hw_DebugPrintStatus(uint32 tick_us)
{
    (void)tick_us;
}

/**
 * @brief  CAN TX diagnostic test — no-op on POSIX
 */
void Main_Hw_CanTxDiagTest(void)
{
    /* POSIX: CAN TX always succeeds via vcan */
}
