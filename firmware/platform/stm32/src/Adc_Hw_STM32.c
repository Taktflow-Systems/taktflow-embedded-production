/**
 * @file    Adc_Hw_STM32.c
 * @brief   STM32 hardware backend for ADC MCAL driver (stub — TODO:HARDWARE)
 * @date    2026-03-03
 *
 * @details Stub implementation for initial STM32 build. All functions return
 *          E_OK / minimal defaults. Replace with real STM32 HAL calls in
 *          Phase F2/F3 when hardware is available.
 *
 * @safety_req SWR-BSW-007: ADC Driver for Analog Sensing
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "Adc.h"

/* ---- Adc_Hw_* stub implementations ---- */

/**
 * @brief  Initialize ADC hardware (STM32 stub: no-op)
 * @return E_OK always
 */
Std_ReturnType Adc_Hw_Init(void)
{
    /* TODO:HARDWARE — configure ADC peripheral (resolution, clock, channels) */
    return E_OK;
}

/**
 * @brief  Start ADC conversion for a channel group (STM32 stub: no-op)
 * @param  Group  Group index
 * @return E_OK always
 */
Std_ReturnType Adc_Hw_StartConversion(uint8 Group)
{
    (void)Group; /* TODO:HARDWARE — start conversion on ADC group */
    return E_OK;
}

/**
 * @brief  Read ADC conversion results (STM32 stub: zero-fill buffer)
 * @param  Group         Group index
 * @param  ResultBuffer  Output: conversion results
 * @param  NumChannels   Number of channels in group
 * @return E_OK always
 */
Std_ReturnType Adc_Hw_ReadResult(uint8 Group, uint16* ResultBuffer,
                                  uint8 NumChannels)
{
    uint8 i;

    (void)Group; /* TODO:HARDWARE — read ADC data registers */

    if (ResultBuffer != NULL_PTR) {
        for (i = 0u; i < NumChannels; i++) {
            ResultBuffer[i] = 0u;
        }
    }

    return E_OK;
}

/**
 * @brief  Get ADC group conversion status (STM32 stub: always idle)
 * @param  Group  Group index
 * @return ADC_IDLE (1u)
 */
uint8 Adc_Hw_GetStatus(uint8 Group)
{
    (void)Group; /* TODO:HARDWARE — read ADC SR register EOC flag */
    return 1u; /* ADC_IDLE */
}
