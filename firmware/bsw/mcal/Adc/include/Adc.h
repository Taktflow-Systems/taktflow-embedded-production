/**
 * @file    Adc.h
 * @brief   ADC MCAL driver — AUTOSAR-like ADC interface
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-007: ADC Driver for Analog Sensing
 * @traces_to  SYS-049, TSR-006, TSR-008
 *
 * @standard AUTOSAR_SWS_ADCDriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef ADC_H
#define ADC_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define ADC_MAX_GROUPS              4u    /**< Max channel groups           */
#define ADC_MAX_CHANNELS_PER_GROUP  8u    /**< Max channels in one group    */

/* ---- Types ---- */

/** ADC driver status */
typedef enum {
    ADC_UNINIT    = 0u,
    ADC_IDLE      = 1u,
    ADC_BUSY      = 2u,
    ADC_COMPLETED = 3u
} Adc_StatusType;

/** ADC channel group configuration */
typedef struct {
    uint8   numChannels;     /**< Number of channels in this group  */
    uint8   triggerSource;   /**< 0=SW triggered, 1=timer triggered */
} Adc_GroupConfigType;

/** ADC driver configuration */
typedef struct {
    uint8                       numGroups;    /**< Number of groups         */
    const Adc_GroupConfigType*  groups;       /**< Pointer to group configs */
    uint8                       resolution;   /**< ADC resolution in bits   */
} Adc_ConfigType;

/* ---- Hardware Abstraction (implemented per platform) ----
 * STM32: mcal/Adc_Hw_STM32.c
 * Test:  Mocked in test/test_Adc.c
 */
extern Std_ReturnType Adc_Hw_Init(void);
extern Std_ReturnType Adc_Hw_StartConversion(uint8 Group);
extern Std_ReturnType Adc_Hw_ReadResult(uint8 Group, uint16* ResultBuffer,
                                         uint8 NumChannels);
extern uint8          Adc_Hw_GetStatus(uint8 Group);

/* ---- API Functions ---- */

/**
 * @brief  Initialize ADC driver and hardware
 * @param  ConfigPtr  Configuration (must not be NULL, groups must not be NULL)
 */
void Adc_Init(const Adc_ConfigType* ConfigPtr);

/**
 * @brief  De-initialize ADC driver
 */
void Adc_DeInit(void);

/**
 * @brief  Get ADC driver status
 * @return Current Adc_StatusType
 */
Adc_StatusType Adc_GetStatus(void);

/**
 * @brief  Start conversion for a channel group
 * @param  Group  Group index (0..ADC_MAX_GROUPS-1)
 * @return E_OK on success, E_NOT_OK on invalid params or not initialized
 */
Std_ReturnType Adc_StartGroupConversion(uint8 Group);

/**
 * @brief  Read conversion results for a group
 * @param  Group          Group index (0..ADC_MAX_GROUPS-1)
 * @param  DataBufferPtr  Pointer to result buffer (must not be NULL)
 * @return E_OK on success, E_NOT_OK on invalid params or not initialized
 */
Std_ReturnType Adc_ReadGroup(uint8 Group, uint16* DataBufferPtr);

#endif /* ADC_H */
