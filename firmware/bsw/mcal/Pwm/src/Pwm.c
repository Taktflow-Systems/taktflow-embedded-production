/**
 * @file    Pwm.c
 * @brief   PWM MCAL driver implementation
 * @date    2026-02-21
 *
 * @details Platform-independent PWM driver logic. Hardware access is
 *          abstracted through Pwm_Hw_* functions (implemented per platform).
 *          Supports motor PWM (20 kHz) and servo PWM (50 Hz).
 *          Duty cycle uses 16-bit fixed-point: 0x0000=0%, 0x8000=100%.
 *
 * @safety_req SWR-BSW-008
 * @traces_to  SYS-050, TSR-005, TSR-012
 *
 * @standard AUTOSAR_SWS_PWMDriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Pwm.h"
#include "Det.h"

/* ---- Internal State ---- */

static Pwm_StatusType pwm_status = PWM_UNINIT;
static uint8          pwm_num_channels = 0u;

/* ---- API Implementation ---- */

void Pwm_Init(const Pwm_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_PWM, 0u, PWM_API_INIT, DET_E_PARAM_POINTER);
        pwm_status = PWM_UNINIT;
        return;
    }

    if (ConfigPtr->channels == NULL_PTR) {
        Det_ReportError(DET_MODULE_PWM, 0u, PWM_API_INIT, DET_E_PARAM_POINTER);
        pwm_status = PWM_UNINIT;
        return;
    }

    if (ConfigPtr->numChannels > PWM_MAX_CHANNELS) {
        Det_ReportError(DET_MODULE_PWM, 0u, PWM_API_INIT, DET_E_PARAM_VALUE);
        pwm_status = PWM_UNINIT;
        return;
    }

    if (Pwm_Hw_Init() != E_OK) {
        pwm_status = PWM_UNINIT;
        return;
    }

    pwm_num_channels = ConfigPtr->numChannels;

    /* Set default duty cycles for all configured channels */
    for (uint8 ch = 0u; ch < pwm_num_channels; ch++) {
        (void)Pwm_Hw_SetDuty(ch, ConfigPtr->channels[ch].defaultDuty);
    }

    pwm_status = PWM_INITIALIZED;
}

void Pwm_DeInit(void)
{
    /* Force all channels to idle before de-initializing */
    for (uint8 ch = 0u; ch < pwm_num_channels; ch++) {
        (void)Pwm_Hw_SetIdle(ch);
    }

    pwm_status = PWM_UNINIT;
    pwm_num_channels = 0u;
}

Pwm_StatusType Pwm_GetStatus(void)
{
    return pwm_status;
}

void Pwm_SetDutyCycle(uint8 ChannelNumber, uint16 DutyCycle)
{
    if (pwm_status != PWM_INITIALIZED) {
        Det_ReportError(DET_MODULE_PWM, 0u, PWM_API_SET_DUTY_CYCLE, DET_E_UNINIT);
        return;
    }

    if (ChannelNumber >= pwm_num_channels) {
        Det_ReportError(DET_MODULE_PWM, 0u, PWM_API_SET_DUTY_CYCLE, DET_E_PARAM_VALUE);
        return;
    }

    /* Clamp duty cycle to maximum (0x8000 = 100%) */
    uint16 clamped = DutyCycle;
    if (clamped > PWM_DUTY_100) {
        clamped = PWM_DUTY_100;
    }

    (void)Pwm_Hw_SetDuty(ChannelNumber, clamped);
}

void Pwm_SetOutputToIdle(uint8 ChannelNumber)
{
    if (pwm_status != PWM_INITIALIZED) {
        Det_ReportError(DET_MODULE_PWM, 0u, PWM_API_SET_DUTY_CYCLE, DET_E_UNINIT);
        return;
    }

    if (ChannelNumber >= pwm_num_channels) {
        Det_ReportError(DET_MODULE_PWM, 0u, PWM_API_SET_DUTY_CYCLE, DET_E_PARAM_VALUE);
        return;
    }

    (void)Pwm_Hw_SetIdle(ChannelNumber);
}
