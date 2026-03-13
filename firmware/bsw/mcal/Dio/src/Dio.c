/**
 * @file    Dio.c
 * @brief   DIO MCAL driver implementation
 * @date    2026-02-21
 *
 * @details Platform-independent DIO driver logic. Hardware access is
 *          abstracted through Dio_Hw_* functions (implemented per platform).
 *          On STM32, Dio_Hw_WritePin uses BSRR for atomic set/reset.
 *          DIO does NOT configure pin direction — that is done by port init.
 *
 * @safety_req SWR-BSW-009
 * @traces_to  SYS-050, TSR-005, TSR-033
 *
 * @standard AUTOSAR_SWS_DIODriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "Dio.h"
#include "Det.h"

/* ---- Internal State ---- */

static boolean dio_initialized = FALSE;

/* ---- API Implementation ---- */

void Dio_Init(void)
{
    dio_initialized = TRUE;
}

void Dio_DeInit(void)
{
    dio_initialized = FALSE;
}

uint8 Dio_ReadChannel(uint8 ChannelId)
{
    if (dio_initialized == FALSE) {
        Det_ReportError(DET_MODULE_DIO, 0u, DIO_API_READ_CHANNEL, DET_E_UNINIT);
        return STD_LOW;
    }

    if (ChannelId >= DIO_MAX_CHANNELS) {
        Det_ReportError(DET_MODULE_DIO, 0u, DIO_API_READ_CHANNEL, DET_E_PARAM_VALUE);
        return STD_LOW;
    }

    return Dio_Hw_ReadPin(ChannelId);
}

void Dio_WriteChannel(uint8 ChannelId, uint8 Level)
{
    if (dio_initialized == FALSE) {
        Det_ReportError(DET_MODULE_DIO, 0u, DIO_API_WRITE_CHANNEL, DET_E_UNINIT);
        return;
    }

    if (ChannelId >= DIO_MAX_CHANNELS) {
        Det_ReportError(DET_MODULE_DIO, 0u, DIO_API_WRITE_CHANNEL, DET_E_PARAM_VALUE);
        return;
    }

    /* Sanitize level: only STD_HIGH (1) is accepted as high */
    uint8 safe_level = (Level == STD_HIGH) ? STD_HIGH : STD_LOW;

    Dio_Hw_WritePin(ChannelId, safe_level);
}

uint8 Dio_FlipChannel(uint8 ChannelId)
{
    if (dio_initialized == FALSE) {
        Det_ReportError(DET_MODULE_DIO, 0u, DIO_API_READ_CHANNEL, DET_E_UNINIT);
        return STD_LOW;
    }

    if (ChannelId >= DIO_MAX_CHANNELS) {
        Det_ReportError(DET_MODULE_DIO, 0u, DIO_API_READ_CHANNEL, DET_E_PARAM_VALUE);
        return STD_LOW;
    }

    /* Read current level, compute inverse, write back */
    uint8 current = Dio_Hw_ReadPin(ChannelId);
    uint8 new_level = (current == STD_HIGH) ? STD_LOW : STD_HIGH;

    Dio_Hw_WritePin(ChannelId, new_level);

    return new_level;
}
