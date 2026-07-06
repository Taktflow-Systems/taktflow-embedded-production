/**
 * @file    sc_eth_udp.c
 * @brief   QM Ethernet IPv4/UDP encoder for SC bench telemetry
 * @date    2026-07-06
 *
 * @details TX-only frame builder for SC_ETH_ENABLE instrumentation builds.
 *          It constructs Ethernet II, IPv4, and UDP headers with deterministic
 *          fields and checksums, then hands the raw frame to sc_eth.
 *
 * @verifies S-UDP-02
 * @note    Safety level: QM instrumentation only; excluded from safety builds.
 * @standard MISRA C:2012
 * @copyright Taktflow Systems 2026
 */
#include "sc_eth_udp.h"

#ifdef SC_ETH_ENABLE

/* ==================================================================
 * Header offsets and protocol constants
 * ================================================================== */

#define SC_ETH_UDP_DST_MAC_OFF        0u
#define SC_ETH_UDP_SRC_MAC_OFF        6u
#define SC_ETH_UDP_ETHERTYPE_OFF      12u
#define SC_ETH_UDP_IPV4_OFF           SC_ETH_UDP_ETH_HDR_LEN
#define SC_ETH_UDP_UDP_OFF            (SC_ETH_UDP_IPV4_OFF + SC_ETH_UDP_IPV4_HDR_LEN)
#define SC_ETH_UDP_PAYLOAD_OFF        SC_ETH_UDP_FRAME_HDR_LEN

#define SC_ETH_UDP_ETHERTYPE_IPV4     0x0800u
#define SC_ETH_UDP_IPV4_VERSION_IHL   0x45u
#define SC_ETH_UDP_IPV4_DSCP_ECN      0x00u
#define SC_ETH_UDP_IPV4_ID            0x0000u
#define SC_ETH_UDP_IPV4_FLAGS_DF      0x4000u
#define SC_ETH_UDP_IPV4_TTL           64u
#define SC_ETH_UDP_IPV4_PROTO_UDP     17u
#define SC_ETH_UDP_CHECKSUM_ZERO      0x0000u
#define SC_ETH_UDP_CHECKSUM_ONES      0xFFFFu
#define SC_ETH_UDP_BROADCAST_OCTET    0xFFu

/* ==================================================================
 * Module state
 * ================================================================== */

static sc_eth_udp_config_t g_sc_eth_udp_config;
static boolean g_sc_eth_udp_configured = FALSE;
static uint8 g_sc_eth_udp_tx_frame[SC_ETH_FRAME_MAX_LEN];
static const uint8 g_sc_eth_udp_broadcast_mac[SC_ETH_UDP_MAC_ADDR_LEN] = {
    SC_ETH_UDP_BROADCAST_OCTET,
    SC_ETH_UDP_BROADCAST_OCTET,
    SC_ETH_UDP_BROADCAST_OCTET,
    SC_ETH_UDP_BROADCAST_OCTET,
    SC_ETH_UDP_BROADCAST_OCTET,
    SC_ETH_UDP_BROADCAST_OCTET
};

/* ==================================================================
 * Static prototypes
 * ================================================================== */

static void sc_eth_udp_copy(uint8 *dst, const uint8 *src, uint16 len);
static void sc_eth_udp_write_u16(uint8 *dst, uint16 value);
static boolean sc_eth_udp_is_broadcast_ip(const uint8 *ip);
static uint32 sc_eth_udp_add_bytes(uint32 sum, const uint8 *data, uint16 len);
static uint16 sc_eth_udp_finish_checksum(uint32 sum);
static uint16 sc_eth_udp_ipv4_checksum(const uint8 *ipv4_header);
static uint16 sc_eth_udp_udp_checksum(const sc_eth_udp_config_t *config,
                                      const uint8 *dst_ip,
                                      const uint8 *udp_header,
                                      uint16 udp_len);

/* ==================================================================
 * Public API
 * ================================================================== */

void Sc_EthUdp_Init(const sc_eth_udp_config_t *config)
{
    uint16 i;

    if (config == NULL_PTR) {
        for (i = 0u; i < SC_ETH_UDP_MAC_ADDR_LEN; i++) {
            g_sc_eth_udp_config.src_mac[i] = 0u;
            g_sc_eth_udp_config.unicast_dst_mac[i] = 0u;
        }
        for (i = 0u; i < SC_ETH_UDP_IPV4_ADDR_LEN; i++) {
            g_sc_eth_udp_config.src_ip[i] = 0u;
        }
        g_sc_eth_udp_config.src_port = 0u;
        g_sc_eth_udp_configured = FALSE;
        return;
    }

    for (i = 0u; i < SC_ETH_UDP_MAC_ADDR_LEN; i++) {
        g_sc_eth_udp_config.src_mac[i] = config->src_mac[i];
        g_sc_eth_udp_config.unicast_dst_mac[i] = config->unicast_dst_mac[i];
    }
    for (i = 0u; i < SC_ETH_UDP_IPV4_ADDR_LEN; i++) {
        g_sc_eth_udp_config.src_ip[i] = config->src_ip[i];
    }
    g_sc_eth_udp_config.src_port = config->src_port;
    g_sc_eth_udp_configured = TRUE;
}

Std_ReturnType Sc_EthUdp_BuildFrame(uint8 *frame,
                                    uint16 frame_size,
                                    const sc_eth_udp_config_t *config,
                                    const uint8 *dst_ip,
                                    uint16 dst_port,
                                    const uint8 *payload,
                                    uint16 payload_len,
                                    uint16 *frame_len)
{
    uint16 required_len;
    uint16 ipv4_total_len;
    uint16 udp_len;
    uint16 checksum;

    if ((frame == NULL_PTR) || (config == NULL_PTR) ||
        (dst_ip == NULL_PTR) || (frame_len == NULL_PTR)) {
        return E_NOT_OK;
    }
    if ((payload == NULL_PTR) && (payload_len > 0u)) {
        return E_NOT_OK;
    }
    if (payload_len > (uint16)SC_ETH_UDP_MAX_PAYLOAD_LEN) {
        return E_NOT_OK;
    }

    required_len = (uint16)(SC_ETH_UDP_FRAME_HDR_LEN + payload_len);
    if (frame_size < required_len) {
        return E_NOT_OK;
    }

    *frame_len = required_len;
    ipv4_total_len = (uint16)(SC_ETH_UDP_IPV4_HDR_LEN +
                              SC_ETH_UDP_UDP_HDR_LEN +
                              payload_len);
    udp_len = (uint16)(SC_ETH_UDP_UDP_HDR_LEN + payload_len);

    if (sc_eth_udp_is_broadcast_ip(dst_ip) == TRUE) {
        sc_eth_udp_copy(&frame[SC_ETH_UDP_DST_MAC_OFF],
                        g_sc_eth_udp_broadcast_mac,
                        SC_ETH_UDP_MAC_ADDR_LEN);
    } else {
        sc_eth_udp_copy(&frame[SC_ETH_UDP_DST_MAC_OFF],
                        config->unicast_dst_mac,
                        SC_ETH_UDP_MAC_ADDR_LEN);
    }
    sc_eth_udp_copy(&frame[SC_ETH_UDP_SRC_MAC_OFF],
                    config->src_mac,
                    SC_ETH_UDP_MAC_ADDR_LEN);
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_ETHERTYPE_OFF],
                         SC_ETH_UDP_ETHERTYPE_IPV4);

    frame[SC_ETH_UDP_IPV4_OFF + 0u] = SC_ETH_UDP_IPV4_VERSION_IHL;
    frame[SC_ETH_UDP_IPV4_OFF + 1u] = SC_ETH_UDP_IPV4_DSCP_ECN;
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_IPV4_OFF + 2u], ipv4_total_len);
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_IPV4_OFF + 4u], SC_ETH_UDP_IPV4_ID);
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_IPV4_OFF + 6u], SC_ETH_UDP_IPV4_FLAGS_DF);
    frame[SC_ETH_UDP_IPV4_OFF + 8u] = SC_ETH_UDP_IPV4_TTL;
    frame[SC_ETH_UDP_IPV4_OFF + 9u] = SC_ETH_UDP_IPV4_PROTO_UDP;
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_IPV4_OFF + 10u],
                         SC_ETH_UDP_CHECKSUM_ZERO);
    sc_eth_udp_copy(&frame[SC_ETH_UDP_IPV4_OFF + 12u],
                    config->src_ip,
                    SC_ETH_UDP_IPV4_ADDR_LEN);
    sc_eth_udp_copy(&frame[SC_ETH_UDP_IPV4_OFF + 16u],
                    dst_ip,
                    SC_ETH_UDP_IPV4_ADDR_LEN);
    checksum = sc_eth_udp_ipv4_checksum(&frame[SC_ETH_UDP_IPV4_OFF]);
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_IPV4_OFF + 10u], checksum);

    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_UDP_OFF + 0u], config->src_port);
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_UDP_OFF + 2u], dst_port);
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_UDP_OFF + 4u], udp_len);
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_UDP_OFF + 6u],
                         SC_ETH_UDP_CHECKSUM_ZERO);

    if (payload_len > 0u) {
        sc_eth_udp_copy(&frame[SC_ETH_UDP_PAYLOAD_OFF], payload, payload_len);
    }

    checksum = sc_eth_udp_udp_checksum(config, dst_ip,
                                       &frame[SC_ETH_UDP_UDP_OFF],
                                       udp_len);
    sc_eth_udp_write_u16(&frame[SC_ETH_UDP_UDP_OFF + 6u], checksum);

    return E_OK;
}

Std_ReturnType Sc_EthUdp_Send(const uint8 *dst_ip,
                              uint16 dst_port,
                              const uint8 *payload,
                              uint16 payload_len)
{
    uint16 frame_len = 0u;

    if (g_sc_eth_udp_configured == FALSE) {
        return E_NOT_OK;
    }

    if (Sc_EthUdp_BuildFrame(g_sc_eth_udp_tx_frame,
                             SC_ETH_FRAME_MAX_LEN,
                             &g_sc_eth_udp_config,
                             dst_ip,
                             dst_port,
                             payload,
                             payload_len,
                             &frame_len) != E_OK) {
        return E_NOT_OK;
    }

    return Sc_Eth_Tx(g_sc_eth_udp_tx_frame, frame_len);
}

/* ==================================================================
 * Static helpers
 * ================================================================== */

static void sc_eth_udp_copy(uint8 *dst, const uint8 *src, uint16 len)
{
    uint16 i;

    for (i = 0u; i < len; i++) {
        dst[i] = src[i];
    }
}

static void sc_eth_udp_write_u16(uint8 *dst, uint16 value)
{
    dst[0u] = (uint8)((value >> 8u) & 0xFFu);
    dst[1u] = (uint8)(value & 0xFFu);
}

static boolean sc_eth_udp_is_broadcast_ip(const uint8 *ip)
{
    uint16 i;

    for (i = 0u; i < SC_ETH_UDP_IPV4_ADDR_LEN; i++) {
        if (ip[i] != SC_ETH_UDP_BROADCAST_OCTET) {
            return FALSE;
        }
    }

    return TRUE;
}

static uint32 sc_eth_udp_add_bytes(uint32 sum, const uint8 *data, uint16 len)
{
    uint16 i = 0u;

    while ((uint16)(i + 1u) < len) {
        sum += ((uint32)data[i] << 8u) | (uint32)data[(uint16)(i + 1u)];
        i = (uint16)(i + 2u);
    }

    if (i < len) {
        sum += ((uint32)data[i] << 8u);
    }

    return sum;
}

static uint16 sc_eth_udp_finish_checksum(uint32 sum)
{
    while ((sum >> 16u) != 0u) {
        sum = (sum & 0xFFFFu) + (sum >> 16u);
    }

    return (uint16)(~sum);
}

static uint16 sc_eth_udp_ipv4_checksum(const uint8 *ipv4_header)
{
    uint32 sum = 0u;

    sum = sc_eth_udp_add_bytes(sum, ipv4_header, SC_ETH_UDP_IPV4_HDR_LEN);

    return sc_eth_udp_finish_checksum(sum);
}

static uint16 sc_eth_udp_udp_checksum(const sc_eth_udp_config_t *config,
                                      const uint8 *dst_ip,
                                      const uint8 *udp_header,
                                      uint16 udp_len)
{
    uint32 sum = 0u;
    uint16 checksum;

    sum = sc_eth_udp_add_bytes(sum, config->src_ip, SC_ETH_UDP_IPV4_ADDR_LEN);
    sum = sc_eth_udp_add_bytes(sum, dst_ip, SC_ETH_UDP_IPV4_ADDR_LEN);
    sum += SC_ETH_UDP_IPV4_PROTO_UDP;
    sum += udp_len;
    sum = sc_eth_udp_add_bytes(sum, udp_header, udp_len);

    checksum = sc_eth_udp_finish_checksum(sum);
    if (checksum == SC_ETH_UDP_CHECKSUM_ZERO) {
        checksum = SC_ETH_UDP_CHECKSUM_ONES;
    }

    return checksum;
}

#endif /* SC_ETH_ENABLE */
