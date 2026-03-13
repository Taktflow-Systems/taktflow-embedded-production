/**
 * @file    Dio_Posix.c
 * @brief   POSIX DIO stub — implements Dio_Hw_* externs from Dio.h
 * @date    2026-02-23
 *
 * @details Simulated digital I/O using a static array of channel values.
 *          No real GPIO hardware — channels are software-emulated for
 *          simulated ECUs (BCM, ICU, TCU) running in Docker containers.
 *
 * @safety_req SWR-BSW-009: DIO Driver for Digital I/O
 * @traces_to  SYS-050, TSR-005, TSR-033
 *
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"

/* ---- Module state ---- */

#define DIO_POSIX_MAX_CHANNELS  256u

static uint8 dio_channel_state[DIO_POSIX_MAX_CHANNELS];

/* ---- Dio_Hw_* implementations ---- */

/**
 * @brief  Read pin level from simulated DIO channel
 * @param  ChannelId  Channel index (0..255)
 * @return STD_HIGH or STD_LOW
 */
uint8 Dio_Hw_ReadPin(uint8 ChannelId)
{
    /* uint8 range 0..255 fits in 256-element array — no bounds check needed */
    return dio_channel_state[ChannelId];
}

/**
 * @brief  Write pin level to simulated DIO channel
 * @param  ChannelId  Channel index (0..255)
 * @param  Level      STD_HIGH or STD_LOW
 */
void Dio_Hw_WritePin(uint8 ChannelId, uint8 Level)
{
    /* uint8 range 0..255 fits in 256-element array — no bounds check needed */
    dio_channel_state[ChannelId] = (Level != STD_LOW) ? STD_HIGH : STD_LOW;
}
