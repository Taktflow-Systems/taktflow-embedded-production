/**
 * @file    sc_eth_rx_dispatch.c
 * @brief   QM UDP port dispatch for SC bench Ethernet RX (S-XCP-02)
 * @date    2026-07-07
 *
 * @traces_to docs/plans/memo-sc-xcp-eth-transport.md (approved minimal-slave
 *            architecture), docs/plans/plan-sc-ethernet-roadmap.md S-XCP-02
 * @note    Safety level: QM instrumentation only; excluded from safety
 *          builds. Single-threaded: SC main task only.
 * @standard MISRA C:2012
 * @copyright Taktflow Systems 2026
 */
#ifdef SC_ETH_ENABLE

#include "sc_eth_rx_dispatch.h"

/* ==================================================================
 * Module State
 * ================================================================== */

typedef struct sc_eth_rx_entry {
    uint16 udp_port;
    sc_eth_rx_handler_t handler;
} sc_eth_rx_entry_t;

static sc_eth_rx_entry_t rx_table[SC_ETH_RX_MAX_PORTS];
static uint8 rx_table_count;

/** RX frame staging buffer — static, not stack: frames are up to 1518
 *  bytes and the SC task stack is 2 KB. Main-task-only access. */
static uint8 rx_frame_buf[SC_ETH_FRAME_MAX_LEN];

/* ==================================================================
 * Public API
 * ================================================================== */

void Sc_EthRx_Init(void)
{
    uint8 i;
    for (i = 0u; i < (uint8)SC_ETH_RX_MAX_PORTS; i++) {
        rx_table[i].udp_port = 0u;
        rx_table[i].handler  = NULL_PTR;
    }
    rx_table_count = 0u;
}

boolean Sc_EthRx_RegisterPort(uint16 udp_port, sc_eth_rx_handler_t handler)
{
    if ((handler == NULL_PTR) ||
        (rx_table_count >= (uint8)SC_ETH_RX_MAX_PORTS)) {
        return FALSE;
    }
    rx_table[rx_table_count].udp_port = udp_port;
    rx_table[rx_table_count].handler  = handler;
    rx_table_count++;
    return TRUE;
}

void Sc_EthRx_Poll(void)
{
    uint16 frame_len = 0u;
    sc_eth_udp_rx_meta_t meta;
    const uint8 *payload = NULL_PTR;
    uint16 payload_len = 0u;
    uint8 i;

    if (Sc_Eth_PollRx(rx_frame_buf, (uint16)sizeof(rx_frame_buf),
                      &frame_len) == FALSE) {
        return;
    }

    if (Sc_EthUdp_ParseFrame(rx_frame_buf, frame_len, &meta,
                             &payload, &payload_len) == FALSE) {
        return;  /* Not IPv4/UDP or malformed — drop (fail-closed) */
    }

    for (i = 0u; i < rx_table_count; i++) {
        if (rx_table[i].udp_port == meta.dst_port) {
            rx_table[i].handler(payload, payload_len, &meta);
            return;
        }
    }
    /* No handler registered for this port — drop silently */
}

#endif /* SC_ETH_ENABLE */
