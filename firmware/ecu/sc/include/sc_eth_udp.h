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

/** RX metadata extracted by Sc_EthUdp_ParseFrame. */
typedef struct sc_eth_udp_rx_meta {
    uint8  src_mac[SC_ETH_UDP_MAC_ADDR_LEN];
    uint8  src_ip[SC_ETH_UDP_IPV4_ADDR_LEN];
    uint16 src_port;
    uint16 dst_port;
} sc_eth_udp_rx_meta_t;

/**
 * @brief  Build and transmit one frame to an explicit destination MAC.
 *
 * Like Sc_EthUdp_Send but overrides the destination MAC instead of using
 * config->unicast_dst_mac — used to reply to a request whose sender MAC
 * was learned from Sc_EthUdp_ParseFrame (no ARP responder needed on the
 * bench). The MAC is not covered by any checksum.
 *
 * @param  dst_mac      Destination MAC (non-NULL, 6 bytes).
 * @param  dst_ip       Destination IPv4 address, network byte order.
 * @param  dst_port     Destination UDP port, host integer value.
 * @param  payload      UDP payload. May be NULL only when payload_len is 0.
 * @param  payload_len  UDP payload length in bytes.
 * @return E_OK when Sc_Eth_Tx accepted the frame; E_NOT_OK otherwise.
 */
Std_ReturnType Sc_EthUdp_SendTo(const uint8 *dst_mac,
                                const uint8 *dst_ip,
                                uint16 dst_port,
                                const uint8 *payload,
                                uint16 payload_len);

/**
 * @brief  Decode one received Ethernet II + IPv4 + UDP frame (S-XCP-02).
 *
 * Mirror of Sc_EthUdp_BuildFrame. Fail-closed: rejects frames that are not
 * plain IPv4/UDP (EtherType 0x0800, IHL 5, protocol 17), whose length
 * fields are inconsistent with the received frame, or whose IPv4 header
 * checksum does not verify. Trailing Ethernet padding is tolerated (payload
 * length comes from the UDP header). No reassembly, no options, no IPv6.
 *
 * @param  frame        Received Ethernet frame.
 * @param  frame_len    Received frame length in bytes.
 * @param  meta         Output source/destination addressing.
 * @param  payload      Output pointer into frame at the UDP payload.
 * @param  payload_len  Output UDP payload length in bytes.
 * @return TRUE when the frame parsed as valid IPv4/UDP; FALSE otherwise.
 */
boolean Sc_EthUdp_ParseFrame(const uint8 *frame,
                             uint16 frame_len,
                             sc_eth_udp_rx_meta_t *meta,
                             const uint8 **payload,
                             uint16 *payload_len);

#endif /* SC_ETH_UDP_H */
