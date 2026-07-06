/**
 * @file    sc_monitoring.h
 * @brief   SC_Status broadcast for Safety Controller (GAP-1/2 hardening)
 * @author  System
 * @date    2026-03-07
 *
 * @details Transmits CAN ID 0x013 (SC_Status) at the configured monitoring
 *          period on DCAN1.
 *          Provides SC health visibility to gateway and ICU so that
 *          an SC firmware halt is detectable via missing alive counter.
 *          Addresses: GAP-1 (no SC heartbeat TX) and GAP-2 (no fault message).
 *
 * @safety_req SWR-SC-029, SWR-SC-030
 * @note    Safety level: ASIL C (diagnostic TX path, not the ASIL D RX path)
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_MONITORING_H
#define SC_MONITORING_H

#include "sc_types.h"

/**
 * @brief  Initialize SC_Status monitoring — reset alive counter and tick
 *
 * Must be called once during SC_Init() before the main loop starts.
 */
void SC_Monitoring_Init(void);

/**
 * @brief  Build the 4-byte SC_Status payload without transmitting it.
 *
 * This helper is shared by the CAN SC_Status broadcaster and QM Ethernet
 * telemetry so both transports expose the same status layout.
 *
 * @param  frame          Output buffer.
 * @param  len            Output buffer size in bytes.
 * @param  alive_counter  Transport-local modulo-16 alive counter value.
 * @return E_OK when frame was populated; E_NOT_OK on invalid arguments.
 */
Std_ReturnType SC_Monitoring_BuildStatusPayload(uint8 *frame,
                                                uint8 len,
                                                uint8 alive_counter);

/**
 * @brief  10ms cyclic update — build and transmit SC_Status periodically
 *
 * Queries SC state from heartbeat, plausibility, relay, and selftest modules.
 * Builds the 4-byte SC_Status frame and calls SC_CAN_TransmitStatus().
 * Only transmits when the configured monitoring period expires.
 *
 * @note   Call after SC_Relay_CheckTriggers() so relay state is current.
 *         Call before SC_LED_Update() to keep frame content consistent.
 */
void SC_Monitoring_Update(void);

#endif /* SC_MONITORING_H */
