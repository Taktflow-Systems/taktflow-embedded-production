/**
 * @file    Adc_Posix.c
 * @brief   POSIX ADC stub — implements Adc_Hw_* externs from Adc.h
 * @date    2026-02-23
 *
 * @details Simulated ADC using static arrays of injectable values.
 *          No real ADC hardware — values can be set externally for
 *          simulation/testing of simulated ECUs in Docker containers.
 *
 * @safety_req SWR-BSW-007: ADC Driver for Analog Sensing
 * @traces_to  SYS-049, TSR-006, TSR-008
 *
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"

/* ---- Module state ---- */

#define ADC_POSIX_MAX_GROUPS    16u
#define ADC_POSIX_MAX_CHANNELS  8u

static uint16 adc_values[ADC_POSIX_MAX_GROUPS][ADC_POSIX_MAX_CHANNELS];
static uint8  adc_status[ADC_POSIX_MAX_GROUPS]; /* 0=idle, 3=completed */

/* ---- Public helper for simulation ---- */

/**
 * @brief  Inject a simulated ADC value (for testing/simulation)
 * @param  Group    Group index
 * @param  Channel  Channel within group
 * @param  Value    ADC raw value to inject
 */
void Adc_Posix_InjectValue(uint8 Group, uint8 Channel, uint16 Value)
{
    if ((Group < ADC_POSIX_MAX_GROUPS) && (Channel < ADC_POSIX_MAX_CHANNELS)) {
        adc_values[Group][Channel] = Value;
    }
}

/* ---- Adc_Hw_* implementations ---- */

/**
 * @brief  Initialize ADC hardware (POSIX: zero all values)
 * @return E_OK always
 */
Std_ReturnType Adc_Hw_Init(void)
{
    uint8 g, c;
    for (g = 0u; g < ADC_POSIX_MAX_GROUPS; g++) {
        adc_status[g] = 0u;
        for (c = 0u; c < ADC_POSIX_MAX_CHANNELS; c++) {
            adc_values[g][c] = 0u;
        }
    }
    return E_OK;
}

/**
 * @brief  Start ADC conversion (POSIX: mark as completed immediately)
 * @param  Group  Group index
 * @return E_OK on success, E_NOT_OK on invalid group
 */
Std_ReturnType Adc_Hw_StartConversion(uint8 Group)
{
    if (Group >= ADC_POSIX_MAX_GROUPS) {
        return E_NOT_OK;
    }
    /* Simulated: conversion completes instantly */
    adc_status[Group] = 3u; /* ADC_COMPLETED */
    return E_OK;
}

/**
 * @brief  Read ADC conversion results
 * @param  Group         Group index
 * @param  ResultBuffer  Output buffer for results
 * @param  NumChannels   Number of channels to read
 * @return E_OK on success, E_NOT_OK on invalid params
 */
Std_ReturnType Adc_Hw_ReadResult(uint8 Group, uint16* ResultBuffer,
                                  uint8 NumChannels)
{
    if (Group >= ADC_POSIX_MAX_GROUPS) {
        return E_NOT_OK;
    }
    if (ResultBuffer == NULL_PTR) {
        return E_NOT_OK;
    }
    if (NumChannels > ADC_POSIX_MAX_CHANNELS) {
        NumChannels = ADC_POSIX_MAX_CHANNELS;
    }

    uint8 c;
    for (c = 0u; c < NumChannels; c++) {
        ResultBuffer[c] = adc_values[Group][c];
    }

    adc_status[Group] = 0u; /* Back to idle */
    return E_OK;
}

/**
 * @brief  Get ADC conversion status
 * @param  Group  Group index
 * @return Status value (0=idle, 3=completed)
 */
uint8 Adc_Hw_GetStatus(uint8 Group)
{
    if (Group >= ADC_POSIX_MAX_GROUPS) {
        return 0u;
    }
    return adc_status[Group];
}
