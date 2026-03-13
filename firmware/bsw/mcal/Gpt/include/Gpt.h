/**
 * @file    Gpt.h
 * @brief   GPT MCAL driver — AUTOSAR-like general purpose timer interface
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-010: GPT Driver for Timing
 * @traces_to  SYS-053, TSR-046, TSR-047
 *
 * @standard AUTOSAR_SWS_GPTDriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef GPT_H
#define GPT_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define GPT_MAX_CHANNELS    4u    /**< Max timer channels               */

/* ---- Types ---- */

/** GPT driver status */
typedef enum {
    GPT_UNINIT      = 0u,
    GPT_INITIALIZED = 1u
} Gpt_StatusType;

/** GPT channel mode */
typedef enum {
    GPT_MODE_ONESHOT    = 0u,   /**< Timer stops after period expires  */
    GPT_MODE_CONTINUOUS = 1u    /**< Timer auto-reloads (free-running) */
} Gpt_ModeType;

/** GPT channel configuration */
typedef struct {
    uint16       prescaler;   /**< Timer prescaler value               */
    uint32       period;      /**< Timer period (in ticks)             */
    Gpt_ModeType mode;        /**< One-shot or continuous              */
} Gpt_ChannelConfigType;

/** GPT driver configuration */
typedef struct {
    uint8                       numChannels;  /**< Number of channels   */
    const Gpt_ChannelConfigType* channels;    /**< Channel config array */
} Gpt_ConfigType;

/* ---- Hardware Abstraction (implemented per platform) ----
 * STM32: mcal/Gpt_Hw_STM32.c
 * Test:  Mocked in test/test_Gpt.c
 */
extern Std_ReturnType Gpt_Hw_Init(void);
extern Std_ReturnType Gpt_Hw_StartTimer(uint8 Channel, uint32 Value);
extern Std_ReturnType Gpt_Hw_StopTimer(uint8 Channel);
extern uint32         Gpt_Hw_GetCounter(uint8 Channel);

/* ---- API Functions ---- */

/**
 * @brief  Initialize GPT driver and hardware
 * @param  ConfigPtr  Configuration (must not be NULL, channels must not be NULL)
 */
void Gpt_Init(const Gpt_ConfigType* ConfigPtr);

/**
 * @brief  De-initialize GPT driver, stop all timers
 */
void Gpt_DeInit(void);

/**
 * @brief  Get GPT driver status
 * @return Current Gpt_StatusType
 */
Gpt_StatusType Gpt_GetStatus(void);

/**
 * @brief  Start a timer channel
 * @param  Channel  Channel index (0..GPT_MAX_CHANNELS-1)
 * @param  Value    Timer target value in ticks (must be > 0)
 * @return E_OK on success, E_NOT_OK on invalid params or not initialized
 */
Std_ReturnType Gpt_StartTimer(uint8 Channel, uint32 Value);

/**
 * @brief  Stop a timer channel
 * @param  Channel  Channel index (0..GPT_MAX_CHANNELS-1)
 * @return E_OK on success, E_NOT_OK on invalid params or not initialized
 */
Std_ReturnType Gpt_StopTimer(uint8 Channel);

/**
 * @brief  Get elapsed time on a timer channel (microseconds)
 * @param  Channel  Channel index (0..GPT_MAX_CHANNELS-1)
 * @return Elapsed time in microseconds; 0 if invalid or not initialized
 */
uint32 Gpt_GetTimeElapsed(uint8 Channel);

#endif /* GPT_H */
