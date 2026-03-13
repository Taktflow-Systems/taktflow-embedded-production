/**
 * @file    Swc_BcmCan.h
 * @brief   BCM CAN interface — init, state reception, command reception, status TX
 * @date    2026-02-24
 *
 * @safety_req SWR-BCM-001, SWR-BCM-002, SWR-BCM-010, SWR-BCM-011
 * @traces_to  SSR-BCM-001, SSR-BCM-002, SSR-BCM-010, SSR-BCM-011
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_BCMCAN_H
#define SWC_BCMCAN_H

#include "Std_Types.h"

/**
 * @brief  Initialize SocketCAN — create socket, bind vcan0, set filters, retry
 * @return E_OK on success, E_NOT_OK if init fails after all retries
 *
 * @safety_req SWR-BCM-001
 */
Std_ReturnType BCM_CAN_Init(void);

/**
 * @brief  Receive vehicle state from CAN ID 0x100 with E2E check
 * @return E_OK on valid reception, E_NOT_OK on E2E failure or timeout
 *
 * @safety_req SWR-BCM-002
 * @details  500ms timeout triggers SHUTDOWN state
 */
Std_ReturnType BCM_CAN_ReceiveState(void);

/**
 * @brief  Receive body control command from CAN ID 0x350
 * @return E_OK on valid reception, E_NOT_OK on timeout
 *
 * @safety_req SWR-BCM-011
 * @details  Fields: headlight, indicator, door. 2s timeout reverts to defaults
 */
Std_ReturnType BCM_CAN_ReceiveCommand(void);

/**
 * @brief  Transmit body status on CAN ID 0x360
 * @return E_OK on successful transmission, E_NOT_OK on failure
 *
 * @safety_req SWR-BCM-010
 * @details  100ms period, alive counter, CRC-8, headlight/indicator/door bits
 */
Std_ReturnType BCM_CAN_TransmitStatus(void);

#endif /* SWC_BCMCAN_H */
