/**
 * @file    sc_xcp_eth.h
 * @brief   Minimal XCP V1.5 slave over Ethernet/UDP for the SC (S-XCP-02)
 * @date    2026-07-07
 *
 * @details BSW-free XCP slave following the sc_uds_shim pattern. Binds to a
 *          UDP port via sc_eth_rx_dispatch and speaks the ASAM
 *          XCP-on-Ethernet transport (4-byte LEN/CTR header) over the
 *          existing sc_eth_udp encoder. Command subset: CONNECT,
 *          DISCONNECT, GET_STATUS, GET_COMM_MODE_INFO, SHORT_UPLOAD,
 *          SET_MTA, UPLOAD, GET_SEED, UNLOCK (polling, no DAQ/STIM).
 *          Protocol semantics mirror firmware/bsw/services/Xcp/src/Xcp.c;
 *          the memory-read whitelist is TMS570-specific.
 *
 * @note    Safety level: QM instrumentation only; excluded from safety
 *          builds (SC_ETH_ENABLE). SC main task context only.
 * @standard MISRA C:2012, ASAM MCD-1 XCP V1.5
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_XCP_ETH_H
#define SC_XCP_ETH_H

#include "sc_types.h"

/** UDP port for XCP-on-Ethernet (telemetry uses 55001). */
#define SC_XCP_ETH_PORT                55002u

/**
 * @brief  Initialise the XCP slave and register its UDP port.
 *
 * Requires Sc_EthRx_Init to have run first. Registers SC_XCP_ETH_PORT with
 * the RX dispatcher; resets connection/unlock state.
 */
void Sc_XcpEth_Init(void);

/**
 * @brief  Query connection state (for diagnostics/tests).
 * @return TRUE when an XCP master is connected.
 */
boolean Sc_XcpEth_IsConnected(void);

#endif /* SC_XCP_ETH_H */
