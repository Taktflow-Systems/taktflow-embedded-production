/**
 * @file    sc_eth_rx_dispatch.h
 * @brief   QM UDP port dispatch for SC bench Ethernet RX (S-XCP-02)
 * @date    2026-07-07
 *
 * @details Polls one raw frame per call from Sc_Eth_PollRx, decodes it via
 *          Sc_EthUdp_ParseFrame, and routes the UDP payload to the handler
 *          registered for the destination port. Static table, no
 *          allocation. Non-matching or malformed frames are dropped
 *          (fail-closed). Structured so an ARP responder can be added at
 *          the EtherType level later without redesign.
 *
 * @note    Safety level: QM instrumentation only; excluded from safety
 *          builds (SC_ETH_ENABLE). Call only from the SC main task.
 * @standard MISRA C:2012
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_ETH_RX_DISPATCH_H
#define SC_ETH_RX_DISPATCH_H

#include "sc_eth_udp.h"

/** Maximum number of registered UDP port handlers. */
#define SC_ETH_RX_MAX_PORTS            4u

/**
 * @brief  UDP payload handler.
 * @param  payload  UDP payload bytes (valid only during the call).
 * @param  len      UDP payload length in bytes.
 * @param  meta     Source/destination addressing of the request.
 * @note   Called from the SC main task context only (no ISR).
 */
typedef void (*sc_eth_rx_handler_t)(const uint8 *payload, uint16 len,
                                    const sc_eth_udp_rx_meta_t *meta);

/**
 * @brief  Reset the dispatch table.
 */
void Sc_EthRx_Init(void);

/**
 * @brief  Register a handler for one UDP destination port.
 * @param  udp_port  Destination port to match.
 * @param  handler   Non-NULL handler invoked from Sc_EthRx_Poll.
 * @return TRUE on success; FALSE when the table is full or handler is NULL.
 */
boolean Sc_EthRx_RegisterPort(uint16 udp_port, sc_eth_rx_handler_t handler);

/**
 * @brief  Poll and dispatch at most one received frame.
 *
 * Reads one frame from Sc_Eth_PollRx; silently drops anything that is not
 * valid IPv4/UDP addressed to a registered port. Intended to run once per
 * 10 ms main-loop tick next to SC_UdsShim_Poll.
 */
void Sc_EthRx_Poll(void);

#endif /* SC_ETH_RX_DISPATCH_H */
