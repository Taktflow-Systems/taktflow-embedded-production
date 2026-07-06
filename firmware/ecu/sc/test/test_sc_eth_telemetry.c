/**
 * @file    test_sc_eth_telemetry.c
 * @brief   Unit tests for SC Ethernet UDP telemetry producer
 * @date    2026-07-06
 *
 * @verifies S-UDP-03
 */
#include "unity.h"

#define UNIT_TEST 1
#define SC_ETH_ENABLE 1

#include "sc_types.h"
#include "Sc_Hw_Cfg.h"
#include "sc_eth.h"
#include "sc_eth_udp.h"
#include "sc_eth_telemetry.h"
#include "sc_monitoring.h"

static uint8 mock_eth_init_mac[SC_ETH_MAC_ADDR_LEN];
static uint32 mock_eth_init_count;
static Std_ReturnType mock_eth_init_result;
static sc_eth_udp_config_t mock_udp_config;
static uint32 mock_udp_init_count;
static boolean mock_udp_cleared;
static uint8 mock_send_dst_ip[SC_ETH_UDP_IPV4_ADDR_LEN];
static uint16 mock_send_dst_port;
static uint8 mock_send_payload[SC_ETH_TELEMETRY_PAYLOAD_LEN];
static uint16 mock_send_payload_len;
static uint32 mock_send_count;
static Std_ReturnType mock_send_result;
static uint8 mock_monitoring_status[SC_RELAY_STATUS_DLC];
static uint8 mock_monitoring_alive;
static uint32 mock_monitoring_build_count;
static Std_ReturnType mock_monitoring_result;

Std_ReturnType Sc_Eth_Init(const uint8 *mac)
{
    uint16 i;

    mock_eth_init_count++;
    for (i = 0u; i < SC_ETH_MAC_ADDR_LEN; i++) {
        mock_eth_init_mac[i] = mac[i];
    }

    return mock_eth_init_result;
}

void Sc_EthUdp_Init(const sc_eth_udp_config_t *config)
{
    uint16 i;

    mock_udp_init_count++;
    if (config == NULL_PTR) {
        mock_udp_cleared = TRUE;
        return;
    }

    mock_udp_cleared = FALSE;
    for (i = 0u; i < SC_ETH_UDP_MAC_ADDR_LEN; i++) {
        mock_udp_config.src_mac[i] = config->src_mac[i];
        mock_udp_config.unicast_dst_mac[i] = config->unicast_dst_mac[i];
    }
    for (i = 0u; i < SC_ETH_UDP_IPV4_ADDR_LEN; i++) {
        mock_udp_config.src_ip[i] = config->src_ip[i];
    }
    mock_udp_config.src_port = config->src_port;
}

Std_ReturnType Sc_EthUdp_Send(const uint8 *dst_ip,
                              uint16 dst_port,
                              const uint8 *payload,
                              uint16 payload_len)
{
    uint16 i;

    mock_send_count++;
    mock_send_dst_port = dst_port;
    mock_send_payload_len = payload_len;
    for (i = 0u; i < SC_ETH_UDP_IPV4_ADDR_LEN; i++) {
        mock_send_dst_ip[i] = dst_ip[i];
    }
    for (i = 0u; i < payload_len; i++) {
        mock_send_payload[i] = payload[i];
    }

    return mock_send_result;
}

Std_ReturnType SC_Monitoring_BuildStatusPayload(uint8 *frame,
                                                uint8 len,
                                                uint8 alive_counter)
{
    uint8 i;

    mock_monitoring_build_count++;
    mock_monitoring_alive = alive_counter;
    if ((frame == NULL_PTR) || (len < SC_RELAY_STATUS_DLC)) {
        return E_NOT_OK;
    }
    if (mock_monitoring_result != E_OK) {
        return mock_monitoring_result;
    }

    for (i = 0u; i < SC_RELAY_STATUS_DLC; i++) {
        frame[i] = mock_monitoring_status[i];
    }
    frame[0u] = (uint8)((alive_counter & 0x0Fu) << 4u) |
                (uint8)(frame[0u] & 0x0Fu);

    return E_OK;
}

#include "../src/sc_eth_telemetry.c"

void setUp(void)
{
    uint16 i;

    for (i = 0u; i < SC_ETH_MAC_ADDR_LEN; i++) {
        mock_eth_init_mac[i] = 0u;
    }
    for (i = 0u; i < SC_ETH_UDP_MAC_ADDR_LEN; i++) {
        mock_udp_config.src_mac[i] = 0u;
        mock_udp_config.unicast_dst_mac[i] = 0u;
    }
    for (i = 0u; i < SC_ETH_UDP_IPV4_ADDR_LEN; i++) {
        mock_udp_config.src_ip[i] = 0u;
        mock_send_dst_ip[i] = 0u;
    }
    for (i = 0u; i < SC_ETH_TELEMETRY_PAYLOAD_LEN; i++) {
        mock_send_payload[i] = 0u;
    }

    mock_eth_init_count = 0u;
    mock_eth_init_result = E_OK;
    mock_udp_init_count = 0u;
    mock_udp_cleared = FALSE;
    mock_udp_config.src_port = 0u;
    mock_send_dst_port = 0u;
    mock_send_payload_len = 0u;
    mock_send_count = 0u;
    mock_send_result = E_OK;
    mock_monitoring_status[0u] = 0x0Au;
    mock_monitoring_status[1u] = 0x5Cu;
    mock_monitoring_status[2u] = 0x21u;
    mock_monitoring_status[3u] = 0x87u;
    mock_monitoring_alive = 0xFFu;
    mock_monitoring_build_count = 0u;
    mock_monitoring_result = E_OK;
}

void tearDown(void) { }

void test_init_configures_raw_eth_and_udp_defaults(void)
{
    Std_ReturnType status;

    status = SC_EthTelemetry_Init();

    TEST_ASSERT_EQUAL_UINT8(E_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_eth_init_count);
    TEST_ASSERT_EQUAL_UINT8(SC_ETH_TELEMETRY_SRC_MAC0, mock_eth_init_mac[0u]);
    TEST_ASSERT_EQUAL_UINT8(SC_ETH_TELEMETRY_SRC_MAC5, mock_eth_init_mac[5u]);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_udp_init_count);
    TEST_ASSERT_FALSE(mock_udp_cleared);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_udp_config.src_ip[0u]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_udp_config.src_ip[3u]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, mock_udp_config.unicast_dst_mac[0u]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, mock_udp_config.unicast_dst_mac[5u]);
    TEST_ASSERT_EQUAL_UINT16(SC_ETH_TELEMETRY_SRC_PORT, mock_udp_config.src_port);
}

void test_build_payload_matches_scet_wire_format(void)
{
    uint8 payload[SC_ETH_TELEMETRY_PAYLOAD_LEN];
    Std_ReturnType status;

    status = SC_EthTelemetry_BuildPayload(payload, sizeof(payload), 0x23u);

    TEST_ASSERT_EQUAL_UINT8(E_OK, status);
    TEST_ASSERT_EQUAL_UINT8((uint8)'S', payload[0u]);
    TEST_ASSERT_EQUAL_UINT8((uint8)'C', payload[1u]);
    TEST_ASSERT_EQUAL_UINT8((uint8)'E', payload[2u]);
    TEST_ASSERT_EQUAL_UINT8((uint8)'T', payload[3u]);
    TEST_ASSERT_EQUAL_UINT8(SC_ETH_TELEMETRY_VERSION, payload[4u]);
    TEST_ASSERT_EQUAL_UINT8(SC_ETH_TELEMETRY_HEADER_LEN, payload[5u]);
    TEST_ASSERT_EQUAL_UINT8(0u, payload[6u]);
    TEST_ASSERT_EQUAL_UINT8(SC_ETH_TELEMETRY_STATUS_LEN, payload[7u]);
    TEST_ASSERT_EQUAL_UINT8(0x03u, payload[8u]);
    TEST_ASSERT_EQUAL_UINT8(SC_ETH_TELEMETRY_TIMESTAMP_SOURCE, payload[9u]);
    TEST_ASSERT_EQUAL_UINT8(0u, payload[10u]);
    TEST_ASSERT_EQUAL_UINT8(SC_ETH_TELEMETRY_TICK_MS, payload[11u]);
    TEST_ASSERT_EQUAL_UINT8(0x3Au, payload[12u]);
    TEST_ASSERT_EQUAL_UINT8(0x5Cu, payload[13u]);
    TEST_ASSERT_EQUAL_UINT8(0x21u, payload[14u]);
    TEST_ASSERT_EQUAL_UINT8(0x87u, payload[15u]);
    TEST_ASSERT_EQUAL_UINT8(0x03u, mock_monitoring_alive);
}

void test_update_sends_one_payload_per_default_tick_and_advances_sequence(void)
{
    (void)SC_EthTelemetry_Init();

    SC_EthTelemetry_Update();
    SC_EthTelemetry_Update();

    TEST_ASSERT_EQUAL_UINT32(2u, mock_send_count);
    TEST_ASSERT_EQUAL_UINT16(SC_ETH_TELEMETRY_DST_PORT, mock_send_dst_port);
    TEST_ASSERT_EQUAL_UINT16(SC_ETH_TELEMETRY_PAYLOAD_LEN, mock_send_payload_len);
    TEST_ASSERT_EQUAL_UINT8(255u, mock_send_dst_ip[0u]);
    TEST_ASSERT_EQUAL_UINT8(255u, mock_send_dst_ip[3u]);
    TEST_ASSERT_EQUAL_UINT8(0x01u, mock_monitoring_alive);
    TEST_ASSERT_EQUAL_UINT8(0x1Au, mock_send_payload[12u]);
}

void test_update_does_not_advance_sequence_when_send_fails(void)
{
    (void)SC_EthTelemetry_Init();
    mock_send_result = E_NOT_OK;

    SC_EthTelemetry_Update();
    SC_EthTelemetry_Update();

    TEST_ASSERT_EQUAL_UINT32(2u, mock_send_count);
    TEST_ASSERT_EQUAL_UINT8(0x00u, mock_monitoring_alive);
    TEST_ASSERT_EQUAL_UINT8(0x0Au, mock_send_payload[12u]);
}

void test_update_is_silent_when_eth_init_fails(void)
{
    mock_eth_init_result = E_NOT_OK;

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, SC_EthTelemetry_Init());
    SC_EthTelemetry_Update();

    TEST_ASSERT_EQUAL_UINT32(0u, mock_send_count);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_udp_init_count);
    TEST_ASSERT_TRUE(mock_udp_cleared);
}

void test_build_payload_rejects_invalid_arguments(void)
{
    uint8 payload[SC_ETH_TELEMETRY_PAYLOAD_LEN];

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        SC_EthTelemetry_BuildPayload(NULL_PTR, sizeof(payload), 0u));
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        SC_EthTelemetry_BuildPayload(payload,
                                     (uint16)(SC_ETH_TELEMETRY_PAYLOAD_LEN - 1u),
                                     0u));

    mock_monitoring_result = E_NOT_OK;
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        SC_EthTelemetry_BuildPayload(payload, sizeof(payload), 0u));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_configures_raw_eth_and_udp_defaults);
    RUN_TEST(test_build_payload_matches_scet_wire_format);
    RUN_TEST(test_update_sends_one_payload_per_default_tick_and_advances_sequence);
    RUN_TEST(test_update_does_not_advance_sequence_when_send_fails);
    RUN_TEST(test_update_is_silent_when_eth_init_fails);
    RUN_TEST(test_build_payload_rejects_invalid_arguments);

    return UNITY_END();
}
