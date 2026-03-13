/**
 * @file    Gpt_Hw_STM32.c
 * @brief   STM32 hardware backend for GPT MCAL driver (stub — TODO:HARDWARE)
 * @date    2026-03-03
 *
 * @details Stub implementation for initial STM32 build. All functions return
 *          E_OK / minimal defaults. Replace with real STM32 HAL calls (TIM
 *          in input capture / timebase mode) in Phase F2/F3 when hardware
 *          is available.
 *
 * @safety_req SWR-BSW-010: GPT Driver for Timing
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "Gpt.h"

/* ---- Gpt_Hw_* stub implementations ---- */

/**
 * @brief  Initialize GPT hardware (STM32 stub: no-op)
 * @return E_OK always
 */
Std_ReturnType Gpt_Hw_Init(void)
{
    /* TODO:HARDWARE — configure TIM peripheral for timebase (PSC, ARR) */
    return E_OK;
}

/**
 * @brief  Start a hardware timer channel (STM32 stub: no-op)
 * @param  Channel  Channel index
 * @param  Value    Timer target value in ticks
 * @return E_OK always
 */
Std_ReturnType Gpt_Hw_StartTimer(uint8 Channel, uint32 Value)
{
    (void)Channel; /* TODO:HARDWARE — load ARR, reset CNT, enable TIMx */
    (void)Value;
    return E_OK;
}

/**
 * @brief  Stop a hardware timer channel (STM32 stub: no-op)
 * @param  Channel  Channel index
 * @return E_OK always
 */
Std_ReturnType Gpt_Hw_StopTimer(uint8 Channel)
{
    (void)Channel; /* TODO:HARDWARE — disable TIMx counter */
    return E_OK;
}

/**
 * @brief  Get current timer counter value (STM32 stub: returns 0)
 * @param  Channel  Channel index
 * @return 0u always
 */
uint32 Gpt_Hw_GetCounter(uint8 Channel)
{
    (void)Channel; /* TODO:HARDWARE — read TIMx_CNT register */
    return 0u;
}
