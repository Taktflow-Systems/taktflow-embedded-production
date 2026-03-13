/**
 * @file    Dio_Hw_STM32.c
 * @brief   STM32 hardware backend for DIO MCAL driver (stub — TODO:HARDWARE)
 * @date    2026-03-03
 *
 * @details Stub implementation for initial STM32 build. All functions return
 *          minimal defaults. Replace with real STM32 HAL calls (GPIO IDR/BSRR)
 *          in Phase F2/F3 when hardware is available.
 *
 * @safety_req SWR-BSW-009: DIO Driver for Digital I/O
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "Dio.h"

/* ---- Dio_Hw_* stub implementations ---- */

/**
 * @brief  Read pin level from GPIO (STM32 stub: always STD_LOW)
 * @param  ChannelId  Channel index
 * @return STD_LOW (0u) always
 */
uint8 Dio_Hw_ReadPin(uint8 ChannelId)
{
    (void)ChannelId; /* TODO:HARDWARE — read GPIOx_IDR for pin state */
    return STD_LOW;
}

/**
 * @brief  Write pin level to GPIO (STM32 stub: no-op)
 * @param  ChannelId  Channel index
 * @param  Level      STD_HIGH or STD_LOW
 */
void Dio_Hw_WritePin(uint8 ChannelId, uint8 Level)
{
    (void)ChannelId; /* TODO:HARDWARE — write GPIOx_BSRR for atomic set/reset */
    (void)Level;
}
