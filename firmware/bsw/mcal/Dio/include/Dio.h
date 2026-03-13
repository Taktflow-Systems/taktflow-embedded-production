/**
 * @file    Dio.h
 * @brief   DIO MCAL driver — AUTOSAR-like digital I/O interface
 * @date    2026-02-21
 *
 * @safety_req SWR-BSW-009: DIO Driver for Digital I/O
 * @traces_to  SYS-050, TSR-005, TSR-033
 *
 * @standard AUTOSAR_SWS_DIODriver, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef DIO_H
#define DIO_H

#include "Std_Types.h"

/* ---- Constants ---- */

#define DIO_MAX_CHANNELS    32u   /**< Max DIO channels (GPIO pins)     */

/* ---- Hardware Abstraction (implemented per platform) ----
 * STM32: mcal/Dio_Hw_STM32.c  (uses BSRR for atomic writes)
 * Test:  Mocked in test/test_Dio.c
 */
extern uint8 Dio_Hw_ReadPin(uint8 ChannelId);
extern void  Dio_Hw_WritePin(uint8 ChannelId, uint8 Level);

/* ---- API Functions ---- */

/**
 * @brief  Initialize DIO driver
 * @note   DIO does not configure pins — that is done by port init.
 *         This only marks the driver as initialized.
 */
void Dio_Init(void);

/**
 * @brief  De-initialize DIO driver
 */
void Dio_DeInit(void);

/**
 * @brief  Read the level of a DIO channel
 * @param  ChannelId  Channel index (0..DIO_MAX_CHANNELS-1)
 * @return STD_HIGH or STD_LOW; STD_LOW if ChannelId is invalid
 */
uint8 Dio_ReadChannel(uint8 ChannelId);

/**
 * @brief  Write a level to a DIO channel (atomic via BSRR)
 * @param  ChannelId  Channel index (0..DIO_MAX_CHANNELS-1)
 * @param  Level      STD_HIGH or STD_LOW
 */
void Dio_WriteChannel(uint8 ChannelId, uint8 Level);

/**
 * @brief  Toggle (flip) a DIO channel
 * @param  ChannelId  Channel index (0..DIO_MAX_CHANNELS-1)
 * @return New level after flip; STD_LOW if ChannelId is invalid
 */
uint8 Dio_FlipChannel(uint8 ChannelId);

#endif /* DIO_H */
