/**
 * @file    sc_eth_telemetry.c
 * @brief   SC Ethernet UDP telemetry producer
 * @date    2026-07-06
 *
 * @details Builds the S-UDP-01 `SCET` payload from the SC_Status status
 *          surface and transmits it over the S-UDP-02 IPv4/UDP encoder.
 *
 * @verifies S-UDP-03
 * @note    Safety level: QM instrumentation only; excluded from safety builds.
 * @standard MISRA C:2012
 * @copyright Taktflow Systems 2026
 */
#include "sc_eth_telemetry.h"

#ifdef SC_ETH_ENABLE

#include "Sc_Hw_Cfg.h"
#include "sc_eth.h"
#include "sc_eth_udp.h"
#include "sc_monitoring.h"

#ifndef SC_ETH_TELEMETRY_SRC_MAC0
#define SC_ETH_TELEMETRY_SRC_MAC0 0x02u
#endif
#ifndef SC_ETH_TELEMETRY_SRC_MAC1
#define SC_ETH_TELEMETRY_SRC_MAC1 0x54u
#endif
#ifndef SC_ETH_TELEMETRY_SRC_MAC2
#define SC_ETH_TELEMETRY_SRC_MAC2 0x46u
#endif
#ifndef SC_ETH_TELEMETRY_SRC_MAC3
#define SC_ETH_TELEMETRY_SRC_MAC3 0x00u
#endif
#ifndef SC_ETH_TELEMETRY_SRC_MAC4
#define SC_ETH_TELEMETRY_SRC_MAC4 0x00u
#endif
#ifndef SC_ETH_TELEMETRY_SRC_MAC5
#define SC_ETH_TELEMETRY_SRC_MAC5 0x03u
#endif

#ifndef SC_ETH_TELEMETRY_SRC_IP0
#define SC_ETH_TELEMETRY_SRC_IP0 0u
#endif
#ifndef SC_ETH_TELEMETRY_SRC_IP1
#define SC_ETH_TELEMETRY_SRC_IP1 0u
#endif
#ifndef SC_ETH_TELEMETRY_SRC_IP2
#define SC_ETH_TELEMETRY_SRC_IP2 0u
#endif
#ifndef SC_ETH_TELEMETRY_SRC_IP3
#define SC_ETH_TELEMETRY_SRC_IP3 0u
#endif

#ifndef SC_ETH_TELEMETRY_DST_IP0
#define SC_ETH_TELEMETRY_DST_IP0 255u
#endif
#ifndef SC_ETH_TELEMETRY_DST_IP1
#define SC_ETH_TELEMETRY_DST_IP1 255u
#endif
#ifndef SC_ETH_TELEMETRY_DST_IP2
#define SC_ETH_TELEMETRY_DST_IP2 255u
#endif
#ifndef SC_ETH_TELEMETRY_DST_IP3
#define SC_ETH_TELEMETRY_DST_IP3 255u
#endif

#define SC_ETH_TELEMETRY_MAGIC0      ((uint8)'S')
#define SC_ETH_TELEMETRY_MAGIC1      ((uint8)'C')
#define SC_ETH_TELEMETRY_MAGIC2      ((uint8)'E')
#define SC_ETH_TELEMETRY_MAGIC3      ((uint8)'T')
#define SC_ETH_TELEMETRY_PERIOD_MS   ((uint16)(SC_ETH_TELEMETRY_PERIOD_TICKS * \
                                               SC_ETH_TELEMETRY_TICK_MS))

static const uint8 g_sc_eth_telemetry_src_mac[SC_ETH_MAC_ADDR_LEN] = {
    SC_ETH_TELEMETRY_SRC_MAC0,
    SC_ETH_TELEMETRY_SRC_MAC1,
    SC_ETH_TELEMETRY_SRC_MAC2,
    SC_ETH_TELEMETRY_SRC_MAC3,
    SC_ETH_TELEMETRY_SRC_MAC4,
    SC_ETH_TELEMETRY_SRC_MAC5
};

static const uint8 g_sc_eth_telemetry_src_ip[SC_ETH_UDP_IPV4_ADDR_LEN] = {
    SC_ETH_TELEMETRY_SRC_IP0,
    SC_ETH_TELEMETRY_SRC_IP1,
    SC_ETH_TELEMETRY_SRC_IP2,
    SC_ETH_TELEMETRY_SRC_IP3
};

static const uint8 g_sc_eth_telemetry_dst_ip[SC_ETH_UDP_IPV4_ADDR_LEN] = {
    SC_ETH_TELEMETRY_DST_IP0,
    SC_ETH_TELEMETRY_DST_IP1,
    SC_ETH_TELEMETRY_DST_IP2,
    SC_ETH_TELEMETRY_DST_IP3
};

static boolean g_sc_eth_telemetry_ready = FALSE;
static uint16 g_sc_eth_telemetry_tick = 0u;
static uint8 g_sc_eth_telemetry_sequence = 0u;

static void sc_eth_telemetry_configure_udp(void);
static void sc_eth_telemetry_write_u16(uint8 *dst, uint16 value);

Std_ReturnType SC_EthTelemetry_Init(void)
{
    g_sc_eth_telemetry_ready = FALSE;
    g_sc_eth_telemetry_tick = 0u;
    g_sc_eth_telemetry_sequence = 0u;

    if (Sc_Eth_Init(g_sc_eth_telemetry_src_mac) != E_OK) {
        Sc_EthUdp_Init(NULL_PTR);
        return E_NOT_OK;
    }

    sc_eth_telemetry_configure_udp();
    g_sc_eth_telemetry_ready = TRUE;

    return E_OK;
}

void SC_EthTelemetry_Update(void)
{
    uint8 payload[SC_ETH_TELEMETRY_PAYLOAD_LEN];

    if (g_sc_eth_telemetry_ready == FALSE) {
        return;
    }

    g_sc_eth_telemetry_tick++;
    if (g_sc_eth_telemetry_tick < SC_ETH_TELEMETRY_PERIOD_TICKS) {
        return;
    }
    g_sc_eth_telemetry_tick = 0u;

    if (SC_EthTelemetry_BuildPayload(payload,
                                     (uint16)sizeof(payload),
                                     g_sc_eth_telemetry_sequence) != E_OK) {
        return;
    }

    if (Sc_EthUdp_Send(g_sc_eth_telemetry_dst_ip,
                       SC_ETH_TELEMETRY_DST_PORT,
                       payload,
                       (uint16)sizeof(payload)) == E_OK) {
        g_sc_eth_telemetry_sequence =
            (uint8)((g_sc_eth_telemetry_sequence + 1u) & 0x0Fu);
    }
}

Std_ReturnType SC_EthTelemetry_BuildPayload(uint8 *payload,
                                            uint16 payload_len,
                                            uint8 sequence_mod16)
{
    uint8 status[SC_RELAY_STATUS_DLC];

    if ((payload == NULL_PTR) ||
        (payload_len < SC_ETH_TELEMETRY_PAYLOAD_LEN)) {
        return E_NOT_OK;
    }

    sequence_mod16 = (uint8)(sequence_mod16 & 0x0Fu);
    if (SC_Monitoring_BuildStatusPayload(status,
                                         SC_RELAY_STATUS_DLC,
                                         sequence_mod16) != E_OK) {
        return E_NOT_OK;
    }

    payload[0u] = SC_ETH_TELEMETRY_MAGIC0;
    payload[1u] = SC_ETH_TELEMETRY_MAGIC1;
    payload[2u] = SC_ETH_TELEMETRY_MAGIC2;
    payload[3u] = SC_ETH_TELEMETRY_MAGIC3;
    payload[4u] = SC_ETH_TELEMETRY_VERSION;
    payload[5u] = SC_ETH_TELEMETRY_HEADER_LEN;
    sc_eth_telemetry_write_u16(&payload[6u], SC_ETH_TELEMETRY_STATUS_LEN);
    payload[8u] = sequence_mod16;
    payload[9u] = SC_ETH_TELEMETRY_TIMESTAMP_SOURCE;
    sc_eth_telemetry_write_u16(&payload[10u], SC_ETH_TELEMETRY_PERIOD_MS);
    payload[12u] = status[0u];
    payload[13u] = status[1u];
    payload[14u] = status[2u];
    payload[15u] = status[3u];

    return E_OK;
}

static void sc_eth_telemetry_configure_udp(void)
{
    sc_eth_udp_config_t config;
    uint16 i;

    for (i = 0u; i < SC_ETH_UDP_MAC_ADDR_LEN; i++) {
        config.src_mac[i] = g_sc_eth_telemetry_src_mac[i];
        config.unicast_dst_mac[i] = 0xFFu;
    }
    for (i = 0u; i < SC_ETH_UDP_IPV4_ADDR_LEN; i++) {
        config.src_ip[i] = g_sc_eth_telemetry_src_ip[i];
    }
    config.src_port = SC_ETH_TELEMETRY_SRC_PORT;

    Sc_EthUdp_Init(&config);
}

static void sc_eth_telemetry_write_u16(uint8 *dst, uint16 value)
{
    dst[0u] = (uint8)((value >> 8u) & 0xFFu);
    dst[1u] = (uint8)(value & 0xFFu);
}

#endif /* SC_ETH_ENABLE */
