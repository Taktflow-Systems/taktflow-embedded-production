/**
 * @file    Gpt.c
 * @brief   GPT MCAL driver implementation
 * @date    2026-02-21
 *
 * @details Platform-independent GPT driver logic. Hardware access is
 *          abstracted through Gpt_Hw_* functions (implemented per platform).
 *          Provides microsecond-resolution timing for timeouts, WCET
 *          measurement, and periodic scheduling.
 *
 * @safety_req SWR-BSW-010
 * @traces_to  SYS-053, TSR-046, TSR-047
 *
 * @standard AUTOSAR_SWS_GPTDriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Gpt.h"
#include "Det.h"

/* ---- Internal State ---- */

static Gpt_StatusType gpt_status = GPT_UNINIT;
static uint8          gpt_num_channels = 0u;
static boolean        gpt_ch_running[GPT_MAX_CHANNELS];

/* ---- API Implementation ---- */

void Gpt_Init(const Gpt_ConfigType* ConfigPtr)
{
    if (ConfigPtr == NULL_PTR) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_INIT, DET_E_PARAM_POINTER);
        gpt_status = GPT_UNINIT;
        return;
    }

    if (ConfigPtr->channels == NULL_PTR) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_INIT, DET_E_PARAM_POINTER);
        gpt_status = GPT_UNINIT;
        return;
    }

    if (ConfigPtr->numChannels > GPT_MAX_CHANNELS) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_INIT, DET_E_PARAM_VALUE);
        gpt_status = GPT_UNINIT;
        return;
    }

    if (Gpt_Hw_Init() != E_OK) {
        gpt_status = GPT_UNINIT;
        return;
    }

    gpt_num_channels = ConfigPtr->numChannels;

    /* Mark all channels as stopped */
    for (uint8 ch = 0u; ch < GPT_MAX_CHANNELS; ch++) {
        gpt_ch_running[ch] = FALSE;
    }

    gpt_status = GPT_INITIALIZED;
}

void Gpt_DeInit(void)
{
    /* Stop all running channels */
    for (uint8 ch = 0u; ch < gpt_num_channels; ch++) {
        if (gpt_ch_running[ch] == TRUE) {
            (void)Gpt_Hw_StopTimer(ch);
            gpt_ch_running[ch] = FALSE;
        }
    }

    gpt_status = GPT_UNINIT;
    gpt_num_channels = 0u;
}

Gpt_StatusType Gpt_GetStatus(void)
{
    return gpt_status;
}

Std_ReturnType Gpt_StartTimer(uint8 Channel, uint32 Value)
{
    if (gpt_status != GPT_INITIALIZED) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_START_TIMER, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel >= gpt_num_channels) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_START_TIMER, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    if (Value == 0u) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_START_TIMER, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    Std_ReturnType ret = Gpt_Hw_StartTimer(Channel, Value);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    gpt_ch_running[Channel] = TRUE;
    return E_OK;
}

Std_ReturnType Gpt_StopTimer(uint8 Channel)
{
    if (gpt_status != GPT_INITIALIZED) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_STOP_TIMER, DET_E_UNINIT);
        return E_NOT_OK;
    }

    if (Channel >= gpt_num_channels) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_STOP_TIMER, DET_E_PARAM_VALUE);
        return E_NOT_OK;
    }

    Std_ReturnType ret = Gpt_Hw_StopTimer(Channel);
    if (ret != E_OK) {
        return E_NOT_OK;
    }

    gpt_ch_running[Channel] = FALSE;
    return E_OK;
}

uint32 Gpt_GetTimeElapsed(uint8 Channel)
{
    if (gpt_status != GPT_INITIALIZED) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_GET_TIME_ELAPSED, DET_E_UNINIT);
        return 0u;
    }

    if (Channel >= gpt_num_channels) {
        Det_ReportError(DET_MODULE_GPT, 0u, GPT_API_GET_TIME_ELAPSED, DET_E_PARAM_VALUE);
        return 0u;
    }

    return Gpt_Hw_GetCounter(Channel);
}
