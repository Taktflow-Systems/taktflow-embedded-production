/**
 * @file    cvc_hw_posix.c
 * @brief   POSIX hardware stubs for CVC (Central Vehicle Computer) SIL simulation
 * @date    2026-02-23
 *
 * @details Implements all Main_Hw_* externs declared in cvc/src/main.c
 *          and the Ssd1306_Hw_I2cWrite extern from Ssd1306.c.
 *          Timing uses clock_gettime(CLOCK_MONOTONIC) for tick measurement.
 *          Self-test functions return E_OK (simulation = no hardware faults).
 *          OLED I2C writes are no-op (display output suppressed in SIL).
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
#include <stdio.h>
#endif

/* ==================================================================
 * Module state
 * ================================================================== */

static uint32 tick_period_us = 10000u;  /* Default 10ms tick */

/* ==================================================================
 * Timing stubs (Main_Hw_* from main.c:57-61)
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
 * Self-test stubs (Main_Hw_* from main.c:63-68)
 * ================================================================== */

/**
 * @brief  SPI loopback self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType Main_Hw_SpiLoopbackTest(void)
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
 * @brief  OLED I2C ACK self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType Main_Hw_OledAckTest(void)
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

/* ==================================================================
 * Self-test stubs (SelfTest_Hw_* from Swc_SelfTest.c)
 * ================================================================== */

/**
 * @brief  SPI loopback self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType SelfTest_Hw_SpiLoopback(void)
{
    return E_OK;
}

/**
 * @brief  CAN loopback self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType SelfTest_Hw_CanLoopback(void)
{
    return E_OK;
}

/**
 * @brief  NVM integrity check — always passes in SIL
 * @return E_OK
 */
Std_ReturnType SelfTest_Hw_NvmCheck(void)
{
    return E_OK;
}

/**
 * @brief  OLED I2C ACK self-test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType SelfTest_Hw_OledAck(void)
{
    return E_OK;
}

/**
 * @brief  MPU region verify — always passes in SIL
 * @return E_OK
 */
Std_ReturnType SelfTest_Hw_MpuVerify(void)
{
    return E_OK;
}

/**
 * @brief  Stack canary check — always passes in SIL
 * @return E_OK
 */
Std_ReturnType SelfTest_Hw_CanaryCheck(void)
{
    return E_OK;
}

/**
 * @brief  RAM pattern test — always passes in SIL
 * @return E_OK
 */
Std_ReturnType SelfTest_Hw_RamPattern(void)
{
    return E_OK;
}

/* ==================================================================
 * SSD1306 OLED I2C stub (from Ssd1306.c:19)
 * ================================================================== */

/**
 * @brief  Write data to I2C bus — no-op on POSIX (no display)
 * @param  addr  7-bit I2C slave address
 * @param  data  Pointer to data buffer
 * @param  len   Number of bytes to write
 * @return E_OK always (silently discards display data)
 */
Std_ReturnType Ssd1306_Hw_I2cWrite(uint8 addr, const uint8* data, uint8 len)
{
    (void)addr;
    (void)data;
    (void)len;
    return E_OK;
}

/* ==================================================================
 * CvcCom E-Stop Injection (SIL only)
 * ================================================================== */

#include "IoHwAb_Inject.h"

/**
 * @brief  Inject E-Stop DIO state from CAN signal into IoHwAb buffer
 * @param  Level  STD_HIGH (E-Stop active) or STD_LOW (inactive)
 */
void CvcCom_Hw_InjectEstop(uint8 Level)
{
    IoHwAb_Inject_SetDigitalPin(IOHWAB_INJECT_PIN_ESTOP, Level);
}

/* ==================================================================
 * Debug Status Print
 * ================================================================== */

/**
 * @brief  Debug status print — no-op on POSIX (info available via CAN monitor)
 * @param  tick_us  Current tick in microseconds (unused)
 */
void Main_Hw_DebugPrintStatus(uint32 tick_us)
{
    (void)tick_us;
}
