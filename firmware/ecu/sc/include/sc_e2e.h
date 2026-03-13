/**
 * @file    sc_e2e.h
 * @brief   E2E CRC-8 validation for Safety Controller CAN messages
 * @date    2026-02-23
 *
 * @details Recomputes CRC-8 (poly 0x1D, init 0xFF) over Data ID byte +
 *          payload bytes 2..DLC-1. Verifies alive counter increment.
 *          Tracks per-message consecutive E2E failure count.
 *
 * @safety_req SWR-SC-003
 * @traces_to  SSR-SC-003
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6, AUTOSAR E2E Profile P01
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_E2E_H
#define SC_E2E_H

#include "sc_types.h"

/**
 * @brief  Initialize E2E module — reset all per-message states
 */
void SC_E2E_Init(void);

/**
 * @brief  Validate E2E protection on a received CAN message
 *
 * Recomputes CRC-8 over Data ID + payload bytes 2..DLC-1, compares with
 * byte 1. Extracts alive counter from byte 0 upper nibble, verifies
 * increment by exactly 1 (with wrap 15->0).
 *
 * @param  data      Pointer to CAN message data (8 bytes)
 * @param  dlc       Data length code
 * @param  dataId    E2E Data ID for this message type
 * @param  msgIndex  Mailbox index (0-based) for per-message state
 * @return TRUE if CRC and alive counter are valid, FALSE otherwise
 */
boolean SC_E2E_Check(const uint8* data, uint8 dlc, uint8 dataId,
                     uint8 msgIndex);

/**
 * @brief  Check if a message type has reached persistent E2E failure
 *
 * @param  msgIndex  Mailbox index (0-based)
 * @return TRUE if 3+ consecutive E2E failures on this message
 */
boolean SC_E2E_IsMsgFailed(uint8 msgIndex);

/**
 * @brief  Check if any safety-critical mailbox has persistent E2E failure
 *
 * Checks E-Stop + all three heartbeat mailboxes (CVC, FZC, RZC).
 * Persistent failure = 3+ consecutive E2E check failures on that message.
 *
 * @return TRUE if any critical mailbox has persistent E2E failure
 * @safety_req SWR-SC-003
 */
boolean SC_E2E_IsAnyCriticalFailed(void);

/**
 * @brief  Compute CRC-8 over arbitrary bytes (for SC_Status TX, SWR-SC-030)
 *
 * Same polynomial (0x1D, init 0xFF, XOR-out 0xFF) as the RX validation path.
 * Called by sc_monitoring.c to protect the outgoing SC_Status frame.
 *
 * @param  data  Pointer to input bytes (must not be NULL if len > 0)
 * @param  len   Number of bytes
 * @return Computed CRC-8 value
 * @note   ASIL C — TX diagnostic path only. Not on the ASIL D RX path.
 */
uint8 SC_E2E_ComputeCRC8(const uint8* data, uint8 len);

#endif /* SC_E2E_H */
