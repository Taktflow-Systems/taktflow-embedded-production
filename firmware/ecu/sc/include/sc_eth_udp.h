/**
 * @file    sc_eth_udp.h
 * @brief   QM Ethernet IPv4/UDP encoder API for SC bench telemetry
 * @date    2026-07-06
 *
 * @details TX-only Ethernet + IPv4 + UDP frame builder for SC_ETH_ENABLE
 *          instrumentation builds. The module does not implement ARP,
 *          routing, fragmentation, or RX processing.
 *
 * @note    Safety level: QM instrumentation only; excluded from safety builds.
 * @standard MISRA C:2012
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_ETH_UDP_H
#define SC_ETH_UDP_H

#include "sc_eth.h"

/** IPv4 address length in bytes. */
#define SC_ETH_UDP_IPV4_ADDR_LEN       4u

/** MAC address length in bytes. */
#define SC_ETH_UDP_MAC_ADDR_LEN        SC_ETH_MAC_ADDR_LEN

/** Ethernet II header length in bytes. */
#define SC_ETH_UDP_ETH_HDR_LEN         14u

/** IPv4 header length in bytes; options are not used. */
#define SC_ETH_UDP_IPV4_HDR_LEN        20u

/** UDP header length in bytes. */
#define SC_ETH_UDP_UDP_HDR_LEN         8u

/** Total Ethernet + IPv4 + UDP header length in bytes. */
#define SC_ETH_UDP_FRAME_HDR_LEN       (SC_ETH_UDP_ETH_HDR_LEN + \
                                        SC_ETH_UDP_IPV4_HDR_LEN + \
                                        SC_ETH_UDP_UDP_HDR_LEN)

/** Maximum UDP payload length that fits in the SC raw Ethernet TX buffer. */
#define SC_ETH_UDP_MAX_PAYLOAD_LEN     (SC_ETH_FRAME_MAX_LEN - \
                                        SC_ETH_UDP_FRAME_HDR_LEN)

typedef struct sc_eth_udp_config {
    uint8 src_mac[SC_ETH_UDP_MAC_ADDR_LEN];
    uint8 src_ip[SC_ETH_UDP_IPV4_ADDR_LEN];
    uint8 unicast_dst_mac[SC_ETH_UDP_MAC_ADDR_LEN];
    uint16 src_port;
} sc_eth_udp_config_t;

/**
 * @brief  Configure source addressing and default unicast destination MAC.
 *
 * Passing NULL clears the configuration; subsequent Sc_EthUdp_Send calls fail
 * until a valid configuration is supplied.
 *
 * @param  config  Non-NULL configuration copied into module state.
 */
void Sc_EthUdp_Init(const sc_eth_udp_config_t *config);

/**
 * @brief  Build an Ethernet II + IPv4 + UDP frame around caller payload.
 *
 * This pure builder writes the frame into caller-provided storage and does not
 * touch EMAC hardware. IPv4 broadcast destination uses Ethernet broadcast MAC;
 * all other destinations use config->unicast_dst_mac.
 *
 * @param  frame        Output Ethernet frame buffer.
 * @param  frame_size   Output frame buffer capacity in bytes.
 * @param  config       Source and unicast destination addressing.
 * @param  dst_ip       Destination IPv4 address, network byte order.
 * @param  dst_port     Destination UDP port, host integer value.
 * @param  payload      UDP payload. May be NULL only when payload_len is zero.
 * @param  payload_len  UDP payload length in bytes.
 * @param  frame_len    Output Ethernet frame length before SC Ethernet padding.
 * @return E_OK when the frame was built; E_NOT_OK on invalid arguments.
 */
Std_ReturnType Sc_EthUdp_BuildFrame(uint8 *frame,
                                    uint16 frame_size,
                                    const sc_eth_udp_config_t *config,
                                    const uint8 *dst_ip,
                                    uint16 dst_port,
                                    const uint8 *payload,
                                    uint16 payload_len,
                                    uint16 *frame_len);

/**
 * @brief  Build and transmit one Ethernet II + IPv4 + UDP frame.
 *
 * @param  dst_ip       Destination IPv4 address, network byte order.
 * @param  dst_port     Destination UDP port, host integer value.
 * @param  payload      UDP payload. May be NULL only when payload_len is zero.
 * @param  payload_len  UDP payload length in bytes.
 * @return E_OK when Sc_Eth_Tx accepted the frame; E_NOT_OK otherwise.
 */
Std_ReturnType Sc_EthUdp_Send(const uint8 *dst_ip,
                              uint16 dst_port,
                              const uint8 *payload,
                              uint16 payload_len);

#endif /* SC_ETH_UDP_H */
