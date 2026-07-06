/**
 * @file    test_sc_eth_udp.c
 * @brief   Unit tests for SC Ethernet IPv4/UDP frame encoder
 * @date    2026-07-06
 *
 * @verifies S-UDP-02
 *
 * Tests Ethernet, IPv4, and UDP header placement, IPv4/UDP checksums,
 * length fields, and broadcast versus unicast destination MAC selection.
 */
#include "unity.h"

#define UNIT_TEST 1
#define SC_ETH_ENABLE 1

#include "sc_types.h"
#include "sc_eth_udp.h"

/* ==================================================================
 * Test constants
 * ================================================================== */

#define TEST_SRC_PORT            55000u
#define TEST_DST_PORT            55001u
#define TEST_FRAME_LEN_4_BYTES   46u
#define TEST_IPV4_TOTAL_LEN_4    32u
#define TEST_UDP_LEN_4           12u
#define TEST_ETHERTYPE_IPV4_HI   0x08u
#define TEST_ETHERTYPE_IPV4_LO   0x00u
#define TEST_IPV4_VERSION_IHL    0x45u
#define TEST_IPV4_TTL            64u
#define TEST_IPV4_PROTO_UDP      17u
#define TEST_IP_CHECKSUM_HI      0xE6u
#define TEST_IP_CHECKSUM_LO      0x47u
#define TEST_UDP_CHECKSUM_HI     0x60u
#define TEST_UDP_CHECKSUM_LO     0x00u

/* ==================================================================
 * Mock sc_eth TX dependency
 * ================================================================== */

static uint8 mock_tx_frame[SC_ETH_FRAME_MAX_LEN];
static uint16 mock_tx_len;
static uint32 mock_tx_count;
static Std_ReturnType mock_tx_result;

Std_ReturnType Sc_Eth_Tx(uint8 *frame, uint16 frame_len)
{
    uint16 i;

    mock_tx_count++;
    mock_tx_len = frame_len;
    for (i = 0u; i < frame_len; i++) {
        mock_tx_frame[i] = frame[i];
    }

    return mock_tx_result;
}

/* ==================================================================
 * Source under test
 * ================================================================== */

#include "../src/sc_eth_udp.c"

/* ==================================================================
 * Test fixture
 * ================================================================== */

static const sc_eth_udp_config_t test_config = {
    {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x11u},
    {198u, 51u, 100u, 10u},
    {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x22u},
    TEST_SRC_PORT
};

static const uint8 test_dst_ip[SC_ETH_UDP_IPV4_ADDR_LEN] = {
    198u, 51u, 100u, 20u
};

static const uint8 test_payload[4u] = {
    0xDEu, 0xADu, 0xBEu, 0xEFu
};

void setUp(void)
{
    uint16 i;

    for (i = 0u; i < SC_ETH_FRAME_MAX_LEN; i++) {
        mock_tx_frame[i] = 0u;
    }
    mock_tx_len = 0u;
    mock_tx_count = 0u;
    mock_tx_result = E_OK;
    Sc_EthUdp_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * Header placement and checksums
 * ================================================================== */

void test_build_frame_places_unicast_ethernet_ipv4_udp_headers(void)
{
    uint8 frame[SC_ETH_FRAME_MAX_LEN];
    uint16 frame_len = 0u;
    Std_ReturnType status;

    status = Sc_EthUdp_BuildFrame(frame, sizeof(frame), &test_config,
                                  test_dst_ip, TEST_DST_PORT,
                                  test_payload, sizeof(test_payload),
                                  &frame_len);

    TEST_ASSERT_EQUAL_UINT8(E_OK, status);
    TEST_ASSERT_EQUAL_UINT16(TEST_FRAME_LEN_4_BYTES, frame_len);

    TEST_ASSERT_EQUAL_UINT8(0x02u, frame[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[3]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[4]);
    TEST_ASSERT_EQUAL_UINT8(0x22u, frame[5]);
    TEST_ASSERT_EQUAL_UINT8(0x02u, frame[6]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[7]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[8]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[9]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[10]);
    TEST_ASSERT_EQUAL_UINT8(0x11u, frame[11]);
    TEST_ASSERT_EQUAL_UINT8(TEST_ETHERTYPE_IPV4_HI, frame[12]);
    TEST_ASSERT_EQUAL_UINT8(TEST_ETHERTYPE_IPV4_LO, frame[13]);

    TEST_ASSERT_EQUAL_UINT8(TEST_IPV4_VERSION_IHL, frame[14]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[15]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[16]);
    TEST_ASSERT_EQUAL_UINT8((uint8)TEST_IPV4_TOTAL_LEN_4, frame[17]);
    TEST_ASSERT_EQUAL_UINT8(TEST_IPV4_TTL, frame[22]);
    TEST_ASSERT_EQUAL_UINT8(TEST_IPV4_PROTO_UDP, frame[23]);
    TEST_ASSERT_EQUAL_UINT8(TEST_IP_CHECKSUM_HI, frame[24]);
    TEST_ASSERT_EQUAL_UINT8(TEST_IP_CHECKSUM_LO, frame[25]);
    TEST_ASSERT_EQUAL_UINT8(198u, frame[26]);
    TEST_ASSERT_EQUAL_UINT8(51u, frame[27]);
    TEST_ASSERT_EQUAL_UINT8(100u, frame[28]);
    TEST_ASSERT_EQUAL_UINT8(10u, frame[29]);
    TEST_ASSERT_EQUAL_UINT8(198u, frame[30]);
    TEST_ASSERT_EQUAL_UINT8(51u, frame[31]);
    TEST_ASSERT_EQUAL_UINT8(100u, frame[32]);
    TEST_ASSERT_EQUAL_UINT8(20u, frame[33]);

    TEST_ASSERT_EQUAL_UINT8(0xD6u, frame[34]);
    TEST_ASSERT_EQUAL_UINT8(0xD8u, frame[35]);
    TEST_ASSERT_EQUAL_UINT8(0xD6u, frame[36]);
    TEST_ASSERT_EQUAL_UINT8(0xD9u, frame[37]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[38]);
    TEST_ASSERT_EQUAL_UINT8((uint8)TEST_UDP_LEN_4, frame[39]);
    TEST_ASSERT_EQUAL_UINT8(TEST_UDP_CHECKSUM_HI, frame[40]);
    TEST_ASSERT_EQUAL_UINT8(TEST_UDP_CHECKSUM_LO, frame[41]);

    TEST_ASSERT_EQUAL_UINT8(0xDEu, frame[42]);
    TEST_ASSERT_EQUAL_UINT8(0xADu, frame[43]);
    TEST_ASSERT_EQUAL_UINT8(0xBEu, frame[44]);
    TEST_ASSERT_EQUAL_UINT8(0xEFu, frame[45]);
}

void test_build_frame_sets_length_fields_from_payload_length(void)
{
    uint8 payload[16u];
    uint8 frame[SC_ETH_FRAME_MAX_LEN];
    uint16 frame_len = 0u;
    Std_ReturnType status;
    uint16 i;

    for (i = 0u; i < sizeof(payload); i++) {
        payload[i] = (uint8)i;
    }

    status = Sc_EthUdp_BuildFrame(frame, sizeof(frame), &test_config,
                                  test_dst_ip, TEST_DST_PORT,
                                  payload, sizeof(payload), &frame_len);

    TEST_ASSERT_EQUAL_UINT8(E_OK, status);
    TEST_ASSERT_EQUAL_UINT16(58u, frame_len);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[16]);
    TEST_ASSERT_EQUAL_UINT8(44u, frame[17]);
    TEST_ASSERT_EQUAL_UINT8(0x00u, frame[38]);
    TEST_ASSERT_EQUAL_UINT8(24u, frame[39]);
}

void test_build_frame_uses_broadcast_mac_for_ipv4_broadcast(void)
{
    const uint8 broadcast_ip[SC_ETH_UDP_IPV4_ADDR_LEN] = {
        255u, 255u, 255u, 255u
    };
    uint8 frame[SC_ETH_FRAME_MAX_LEN];
    uint16 frame_len = 0u;
    Std_ReturnType status;

    status = Sc_EthUdp_BuildFrame(frame, sizeof(frame), &test_config,
                                  broadcast_ip, TEST_DST_PORT,
                                  test_payload, sizeof(test_payload),
                                  &frame_len);

    TEST_ASSERT_EQUAL_UINT8(E_OK, status);
    TEST_ASSERT_EQUAL_UINT16(TEST_FRAME_LEN_4_BYTES, frame_len);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, frame[0]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, frame[1]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, frame[2]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, frame[3]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, frame[4]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, frame[5]);
    TEST_ASSERT_EQUAL_UINT8(255u, frame[30]);
    TEST_ASSERT_EQUAL_UINT8(255u, frame[31]);
    TEST_ASSERT_EQUAL_UINT8(255u, frame[32]);
    TEST_ASSERT_EQUAL_UINT8(255u, frame[33]);
}

void test_build_frame_rejects_invalid_arguments_and_oversize_payload(void)
{
    uint8 frame[SC_ETH_FRAME_MAX_LEN];
    uint16 frame_len = 0u;

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        Sc_EthUdp_BuildFrame(NULL_PTR, sizeof(frame), &test_config,
                             test_dst_ip, TEST_DST_PORT,
                             test_payload, sizeof(test_payload), &frame_len));
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        Sc_EthUdp_BuildFrame(frame, sizeof(frame), NULL_PTR,
                             test_dst_ip, TEST_DST_PORT,
                             test_payload, sizeof(test_payload), &frame_len));
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        Sc_EthUdp_BuildFrame(frame, sizeof(frame), &test_config,
                             NULL_PTR, TEST_DST_PORT,
                             test_payload, sizeof(test_payload), &frame_len));
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        Sc_EthUdp_BuildFrame(frame, sizeof(frame), &test_config,
                             test_dst_ip, TEST_DST_PORT,
                             NULL_PTR, sizeof(test_payload), &frame_len));
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        Sc_EthUdp_BuildFrame(frame, 45u, &test_config,
                             test_dst_ip, TEST_DST_PORT,
                             test_payload, sizeof(test_payload), &frame_len));
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        Sc_EthUdp_BuildFrame(frame, sizeof(frame), &test_config,
                             test_dst_ip, TEST_DST_PORT,
                             frame, (uint16)(SC_ETH_UDP_MAX_PAYLOAD_LEN + 1u),
                             &frame_len));
}

void test_send_builds_frame_and_calls_raw_ethernet_tx(void)
{
    Std_ReturnType status;

    status = Sc_EthUdp_Send(test_dst_ip, TEST_DST_PORT,
                            test_payload, sizeof(test_payload));

    TEST_ASSERT_EQUAL_UINT8(E_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_tx_count);
    TEST_ASSERT_EQUAL_UINT16(TEST_FRAME_LEN_4_BYTES, mock_tx_len);
    TEST_ASSERT_EQUAL_UINT8(TEST_IP_CHECKSUM_HI, mock_tx_frame[24]);
    TEST_ASSERT_EQUAL_UINT8(TEST_IP_CHECKSUM_LO, mock_tx_frame[25]);
    TEST_ASSERT_EQUAL_UINT8(TEST_UDP_CHECKSUM_HI, mock_tx_frame[40]);
    TEST_ASSERT_EQUAL_UINT8(TEST_UDP_CHECKSUM_LO, mock_tx_frame[41]);
}

void test_send_rejects_before_configuration(void)
{
    Sc_EthUdp_Init(NULL_PTR);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK,
        Sc_EthUdp_Send(test_dst_ip, TEST_DST_PORT,
                       test_payload, sizeof(test_payload)));
    TEST_ASSERT_EQUAL_UINT32(0u, mock_tx_count);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_build_frame_places_unicast_ethernet_ipv4_udp_headers);
    RUN_TEST(test_build_frame_sets_length_fields_from_payload_length);
    RUN_TEST(test_build_frame_uses_broadcast_mac_for_ipv4_broadcast);
    RUN_TEST(test_build_frame_rejects_invalid_arguments_and_oversize_payload);
    RUN_TEST(test_send_builds_frame_and_calls_raw_ethernet_tx);
    RUN_TEST(test_send_rejects_before_configuration);

    return UNITY_END();
}
