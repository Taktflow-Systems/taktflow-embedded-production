/**
 * @file    Pwm.h
 * @brief   PWM MCAL driver — AUTOSAR-like PWM generation interface
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-008: PWM Driver for Motor and Servo Control
 * @traces_to  SYS-050, TSR-005, TSR-012
 *
 * @standard AUTOSAR_SWS_PWMDriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef PWM_H
#define PWM_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define PWM_MAX_CHANNELS    8u        /**< Max PWM output channels         */
#define PWM_DUTY_0          0x0000u   /**< 0% duty cycle                   */
#define PWM_DUTY_100        0x8000u   /**< 100% duty cycle                 */

/* ---- Types ---- */

/** PWM driver status */
typedef enum {
    PWM_UNINIT      = 0u,
    PWM_INITIALIZED = 1u
} Pwm_StatusType;

/** PWM output polarity */
typedef enum {
    PWM_HIGH = 0u,   /**< Active high */
    PWM_LOW  = 1u    /**< Active low  */
} Pwm_OutputStateType;

/** PWM channel configuration */
typedef struct {
    uint32              frequency;    /**< PWM frequency in Hz             */
    uint16              defaultDuty;  /**< Default duty (0x0000..0x8000)   */
    Pwm_OutputStateType polarity;     /**< Active high or low              */
    Pwm_OutputStateType idleState;    /**< Output state when idle          */
} Pwm_ChannelConfigType;

/** PWM driver configuration */
typedef struct {
    uint8                       numChannels;  /**< Number of channels      */
    const Pwm_ChannelConfigType* channels;    /**< Channel config array    */
} Pwm_ConfigType;

/* ---- Hardware Abstraction (implemented per platform) ----
 * STM32: mcal/Pwm_Hw_STM32.c
 * Test:  Mocked in test/test_Pwm.c
 */
extern Std_ReturnType Pwm_Hw_Init(void);
extern Std_ReturnType Pwm_Hw_SetDuty(uint8 Channel, uint16 DutyCycle);
extern Std_ReturnType Pwm_Hw_SetIdle(uint8 Channel);

/* ---- API Functions ---- */

/**
 * @brief  Initialize PWM driver and hardware
 * @param  ConfigPtr  Configuration (must not be NULL, channels must not be NULL)
 */
void Pwm_Init(const Pwm_ConfigType* ConfigPtr);

/**
 * @brief  De-initialize PWM driver, set all outputs to idle
 */
void Pwm_DeInit(void);

/**
 * @brief  Get PWM driver status
 * @return Current Pwm_StatusType
 */
Pwm_StatusType Pwm_GetStatus(void);

/**
 * @brief  Set duty cycle for a PWM channel
 * @param  ChannelNumber  Channel index (0..PWM_MAX_CHANNELS-1)
 * @param  DutyCycle      Duty (0x0000=0%, 0x8000=100%, clamped if above)
 */
void Pwm_SetDutyCycle(uint8 ChannelNumber, uint16 DutyCycle);

/**
 * @brief  Force PWM output to its idle state
 * @param  ChannelNumber  Channel index (0..PWM_MAX_CHANNELS-1)
 */
void Pwm_SetOutputToIdle(uint8 ChannelNumber);

#endif /* PWM_H */
