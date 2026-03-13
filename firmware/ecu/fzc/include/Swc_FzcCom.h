/**
 * @file    Swc_FzcCom.h
 * @brief   FZC CAN communication SWC — E2E protection, message RX/TX
 * @date    2026-02-24
 *
 * @safety_req SWR-FZC-019, SWR-FZC-020, SWR-FZC-026, SWR-FZC-027
 * @traces_to  SSR-FZC-019, SSR-FZC-020, SSR-FZC-026, SSR-FZC-027
 *
 * @details  CAN communication SWC for the FZC:
 *           - E2E protection (CRC-8 0x1D, alive counter, Data ID) on TX
 *           - E2E verification on RX with safe defaults on failure
 *           - CAN message reception: 0x001 E-stop, 0x100 vehicle state,
 *             0x200 brake cmd, 0x201 steering cmd
 *           - CAN message transmission: 0x011 heartbeat 50ms,
 *             0x210 brake fault, 0x211 motor cutoff, 0x220 lidar warning
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_FZC_COM_H
#define SWC_FZC_COM_H

#include "Std_Types.h"

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the FZC COM module
 * @note   Must be called once at startup before cyclic functions.
 */
void Swc_FzcCom_Init(void);

/**
 * @brief  Apply E2E protection to an outgoing CAN message
 * @param  data      Pointer to message data buffer (min 8 bytes)
 * @param  length    Message data length
 * @param  dataId    FZC-specific E2E Data ID
 * @return E_OK on success, E_NOT_OK if data is NULL or length < 2
 *
 * @details  Inserts CRC-8 (poly 0x1D) at byte[0] and alive counter
 *           at byte[1] bits[3:0]. Data ID XORed into CRC seed.
 */
Std_ReturnType Swc_FzcCom_E2eProtect(uint8* data, uint8 length, uint8 dataId);

/**
 * @brief  Verify E2E protection on an incoming CAN message
 * @param  data      Pointer to message data buffer (min 8 bytes)
 * @param  length    Message data length
 * @param  dataId    Expected FZC-specific E2E Data ID
 * @return E_OK if CRC and alive counter valid, E_NOT_OK otherwise
 *
 * @details  On failure, caller must apply safe defaults:
 *           - Brake: 100% braking (max braking)
 *           - Steering: 0 deg center
 */
Std_ReturnType Swc_FzcCom_E2eCheck(const uint8* data, uint8 length, uint8 dataId);

/**
 * @brief  Process all incoming CAN messages — cyclic 10ms
 * @note   Reads from Com RX buffers, applies E2E check, routes
 *         validated signals to RTE. On E2E failure, applies safe defaults.
 */
void Swc_FzcCom_Receive(void);

/**
 * @brief  Schedule outgoing CAN message transmissions — cyclic 10ms
 * @note   Heartbeat at 50ms period, event-driven messages (brake fault,
 *         motor cutoff, lidar warning) within 10ms of trigger.
 */
void Swc_FzcCom_TransmitSchedule(void);

#endif /* SWC_FZC_COM_H */
