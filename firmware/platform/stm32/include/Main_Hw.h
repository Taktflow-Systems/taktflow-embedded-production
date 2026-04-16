/**
 * @file    Main_Hw.h
 * @brief   Hardware abstraction interface — per-platform implementations
 * @date    2026-03-27
 *
 * @details Declares the hardware abstraction functions called from each
 *          ECU's main.c. Implementations live in:
 *            firmware/platform/stm32/src/{cvc,fzc,rzc}_hw_stm32.c
 *            firmware/platform/posix/src/{cvc,fzc,rzc}_hw_posix.c
 *
 *          This header ensures each external function is declared in
 *          exactly one file (MISRA C:2012 Rule 8.5 compliance).
 *
 *          ECU-specific self-test functions that do not apply to a
 *          particular ECU are simply not called; the unused-function
 *          warning is suppressed globally (Rule 8.7 deviation DEV suppressed
 *          in tools/misra/suppressions.txt).
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef MAIN_HW_H
#define MAIN_HW_H

#include "Std_Types.h"

/* ---- Common platform interface (all ECUs) ---- */

/** @brief System clock initialization (PLL configuration) */
void Main_Hw_SystemClockInit(void);

/** @brief MPU region configuration */
void Main_Hw_MpuConfig(void);

/** @brief SysTick timer initialization
 *  @param periodUs  Tick period in microseconds (e.g., 1000 for 1ms) */
void Main_Hw_SysTickInit(uint32 periodUs);

/** @brief Wait for interrupt (low-power idle) */
void Main_Hw_Wfi(void);

/** @brief Get current monotonic tick (microsecond resolution)
 *  @return Tick count in microseconds since startup */
uint32 Main_Hw_GetTick(void);

/** @brief Plant stack canary value for overflow detection */
void Main_Hw_PlantStackCanary(void);

/** @brief 5s periodic debug status — UART print on STM32, no-op on POSIX
 *  @param tick_us  Current system tick in microseconds */
void Main_Hw_DebugPrintStatus(uint32 tick_us);

/* ---- Self-test functions (shared across multiple ECUs) ---- */

/** @brief CAN controller loopback self-test (CVC, FZC, RZC) */
Std_ReturnType Main_Hw_CanLoopbackTest(void);

/** @brief MPU region configuration verify test (FZC, RZC) */
Std_ReturnType Main_Hw_MpuVerifyTest(void);

/** @brief RAM pattern write/read integrity test (CVC, FZC, RZC) */
Std_ReturnType Main_Hw_RamPatternTest(void);

/* ---- CVC-specific self-test functions ---- */

/** @brief SPI loopback test for AS5048A angle sensors (CVC only) */
Std_ReturnType Main_Hw_SpiLoopbackTest(void);

/** @brief OLED display I2C ACK test (CVC only) */
Std_ReturnType Main_Hw_OledAckTest(void);

/* ---- FZC-specific self-test functions ---- */

/** @brief Servo neutral position test — steering PWM centers (FZC only) */
Std_ReturnType Main_Hw_ServoNeutralTest(void);

/** @brief SPI sensor test — AS5048A angle sensor responds (FZC only) */
Std_ReturnType Main_Hw_SpiSensorTest(void);

/** @brief UART lidar handshake test — TFMini-S responds (FZC only) */
Std_ReturnType Main_Hw_UartLidarTest(void);

/* ---- RZC-specific self-test functions ---- */

/** @brief CAN TX diagnostic (RZC only) */
void Main_Hw_CanTxDiagTest(void);

/** @brief Periodic direct FDCAN probe for HIL physical-path triage (RZC only)
 *  @param tick_us  Current system tick in microseconds */
void Main_Hw_RunCanPhysicalProbe(uint32 tick_us);

/** @brief BTS7960 motor driver GPIO test (RZC only) */
Std_ReturnType Main_Hw_Bts7960GpioTest(void);

/** @brief ACS723 current sensor zero-calibration test (RZC only) */
Std_ReturnType Main_Hw_Acs723ZeroCalTest(void);

/** @brief NTC thermistor range check test (RZC only) */
Std_ReturnType Main_Hw_NtcRangeTest(void);

/** @brief Motor encoder stuck detection test (RZC only) */
Std_ReturnType Main_Hw_EncoderStuckTest(void);

/** @brief Get FDCAN1 HAL state for debug diagnostics (RZC only)
 *  @return HAL state: 0=RESET, 1=READY, 2=LISTENING, 3=ERROR */
uint8 Main_Hw_GetCanHalState(void);

#endif /* MAIN_HW_H */
