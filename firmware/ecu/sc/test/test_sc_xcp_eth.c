/**
 * @file    test_sc_xcp_eth.c
 * @brief   Unit tests for SC UDP RX dispatch + minimal XCP-on-Ethernet slave
 * @date    2026-07-07
 *
 * @verifies S-XCP-02
 *
 * Written FIRST per TDD. Covers: UDP frame decode (round trip against the
 * S-UDP-02 encoder + malformed-frame rejection), the UDP port dispatch
 * table, and the XCP slave command subset (CONNECT / DISCONNECT /
 * GET_STATUS / SHORT_UPLOAD / GET_SEED / UNLOCK) including the ASAM
 * XCP-on-Ethernet 4-byte transport header, connected-state gating, the
 * Seed&Key flow and the TMS570 address whitelist. Protocol semantics must
 * match firmware/bsw/services/Xcp/src/Xcp.c.
 */
#include "unity.h"

#define UNIT_TEST 1
#define SC_ETH_ENABLE 1

#include "sc_types.h"
#include "sc_eth_udp.h"
#include "sc_eth_rx_dispatch.h"
#include "sc_xcp_eth.h"

/* ==================================================================
 * Mock sc_eth dependencies
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

static uint8 mock_rx_frame[SC_ETH_FRAME_MAX_LEN];
static uint16 mock_rx_len;
static boolean mock_rx_pending;

boolean Sc_Eth_PollRx(uint8 *frame, uint16 frame_size, uint16 *frame_len)
{
    uint16 i;
    if ((mock_rx_pending == FALSE) || (mock_rx_len > frame_size)) {
        return FALSE;
    }
    for (i = 0u; i < mock_rx_len; i++) {
        frame[i] = mock_rx_frame[i];
    }
    *frame_len = mock_rx_len;
    mock_rx_pending = FALSE;
    return TRUE;
}

/* Fake target memory for SHORT_UPLOAD (whitelist-valid SRAM address) */
#define TEST_MEM_BASE 0x08001000u
static uint8 test_mem[16];

uint8 test_xcp_mem_read(uint32 addr)
{
    return test_mem[addr - TEST_MEM_BASE];
}

/* ==================================================================
 * Sources under test
 * ================================================================== */

#include "../src/sc_eth_udp.c"
#include "../src/sc_eth_rx_dispatch.c"
#include "../src/sc_xcp_eth.c"

/* ==================================================================
 * Fixtures and helpers
 * ================================================================== */

/* SC-side addressing (the slave) */
static const sc_eth_udp_config_t slave_cfg = {
    {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x11u},
    {198u, 51u, 100u, 10u},
    {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x22u},
    SC_XCP_ETH_PORT
};

/* Master-side addressing (builds request frames toward the slave) */
static const sc_eth_udp_config_t master_cfg = {
    {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x22u},
    {198u, 51u, 100u, 20u},
    {0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x11u},
    61000u
};

static const uint8 slave_ip[SC_ETH_UDP_IPV4_ADDR_LEN] = {198u, 51u, 100u, 10u};

static uint16 handler_calls;
static uint16 handler_last_len;
static sc_eth_udp_rx_meta_t handler_last_meta;

static void test_port_handler(const uint8 *payload, uint16 len,
                              const sc_eth_udp_rx_meta_t *meta)
{
    (void)payload;
    handler_calls++;
    handler_last_len = len;
    handler_last_meta = *meta;
}

/** Build a UDP frame from the master and stage it in the RX mock. */
static void inject_udp(uint16 dst_port, const uint8 *payload, uint16 len)
{
    uint16 frame_len = 0u;
    TEST_ASSERT_EQUAL_UINT8(E_OK,
        Sc_EthUdp_BuildFrame(mock_rx_frame, sizeof(mock_rx_frame),
                             &master_cfg, slave_ip, dst_port,
                             payload, len, &frame_len));
    mock_rx_len = frame_len;
    mock_rx_pending = TRUE;
}

/** Wrap an XCP packet in the 4-byte transport header and inject it. */
static void inject_xcp(const uint8 *packet, uint16 packet_len, uint16 ctr)
{
    uint8 payload[64];
    uint16 i;
    payload[0] = (uint8)(packet_len & 0xFFu);
    payload[1] = (uint8)(packet_len >> 8u);
    payload[2] = (uint8)(ctr & 0xFFu);
    payload[3] = (uint8)(ctr >> 8u);
    for (i = 0u; i < packet_len; i++) {
        payload[4u + i] = packet[i];
    }
    inject_udp(SC_XCP_ETH_PORT, payload, (uint16)(4u + packet_len));
}

/** Decode the last mock-TXed frame back into an XCP packet. */
static uint16 last_response(const uint8 **packet, uint16 *ctr,
                            sc_eth_udp_rx_meta_t *meta)
{
    static const uint8 *payload;
    static uint16 payload_len;
    uint16 xcp_len;

    TEST_ASSERT_TRUE(mock_tx_count > 0u);
    TEST_ASSERT_TRUE(
        Sc_EthUdp_ParseFrame(mock_tx_frame, mock_tx_len, meta,
                             &payload, &payload_len));
    TEST_ASSERT_TRUE(payload_len >= 4u);
    xcp_len = (uint16)payload[0] | ((uint16)payload[1] << 8u);
    *ctr = (uint16)payload[2] | ((uint16)payload[3] << 8u);
    TEST_ASSERT_EQUAL_UINT16(payload_len - 4u, xcp_len);
    *packet = &payload[4];
    return xcp_len;
}

/** Same key derivation as Xcp.c xcp_compute_key (must match). */
static uint32 master_compute_key(uint32 seed)
{
    uint32 key = seed ^ 0x54414B54u;
    key = ((key << 13u) | (key >> 19u));
    key ^= 0x464C4F57u;
    return key;
}

static void do_connect(void)
{
    static const uint8 connect_cmd[2] = {0xFFu, 0x00u};
    inject_xcp(connect_cmd, 2u, 1u);
    Sc_EthRx_Poll();
}

static void do_unlock(void)
{
    static const uint8 seed_cmd[3] = {0xF8u, 0x00u, 0x01u};
    uint8 unlock_cmd[6];
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;
    uint32 seed;
    uint32 key;

    inject_xcp(seed_cmd, 3u, 2u);
    Sc_EthRx_Poll();
    TEST_ASSERT_EQUAL_UINT16(6u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(4u, resp[1]);
    seed = ((uint32)resp[2] << 24u) | ((uint32)resp[3] << 16u) |
           ((uint32)resp[4] << 8u) | (uint32)resp[5];

    key = master_compute_key(seed);
    unlock_cmd[0] = 0xF7u;
    unlock_cmd[1] = 4u;
    unlock_cmd[2] = (uint8)(key >> 24u);
    unlock_cmd[3] = (uint8)(key >> 16u);
    unlock_cmd[4] = (uint8)(key >> 8u);
    unlock_cmd[5] = (uint8)key;
    inject_xcp(unlock_cmd, 6u, 3u);
    Sc_EthRx_Poll();
    TEST_ASSERT_EQUAL_UINT16(2u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01u, resp[1]);
}

void setUp(void)
{
    uint16 i;
    mock_tx_count = 0u;
    mock_tx_len = 0u;
    mock_tx_result = E_OK;
    mock_rx_pending = FALSE;
    handler_calls = 0u;
    handler_last_len = 0u;
    for (i = 0u; i < (uint16)sizeof(test_mem); i++) {
        test_mem[i] = (uint8)(0xA0u + i);
    }
    Sc_EthUdp_Init(&slave_cfg);
    Sc_EthRx_Init();
    Sc_XcpEth_Init();
}

void tearDown(void)
{
}

/* ==================================================================
 * UDP decode tests
 * ================================================================== */

void test_parse_round_trips_encoder_output(void)
{
    static const uint8 payload[5] = {1u, 2u, 3u, 4u, 5u};
    const uint8 *out;
    uint16 out_len;
    sc_eth_udp_rx_meta_t meta;

    inject_udp(55002u, payload, 5u);
    TEST_ASSERT_TRUE(
        Sc_EthUdp_ParseFrame(mock_rx_frame, mock_rx_len, &meta,
                             &out, &out_len));
    TEST_ASSERT_EQUAL_UINT16(5u, out_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, out, 5u);
    TEST_ASSERT_EQUAL_UINT16(55002u, meta.dst_port);
    TEST_ASSERT_EQUAL_UINT16(61000u, meta.src_port);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(master_cfg.src_ip, meta.src_ip, 4u);
}

void test_parse_rejects_malformed_frames(void)
{
    static const uint8 payload[4] = {9u, 9u, 9u, 9u};
    const uint8 *out;
    uint16 out_len;
    sc_eth_udp_rx_meta_t meta;
    uint8 saved;

    inject_udp(55002u, payload, 4u);

    /* Too short */
    TEST_ASSERT_FALSE(Sc_EthUdp_ParseFrame(mock_rx_frame, 41u, &meta,
                                           &out, &out_len));
    /* Wrong EtherType */
    saved = mock_rx_frame[12];
    mock_rx_frame[12] = 0x86u;
    TEST_ASSERT_FALSE(Sc_EthUdp_ParseFrame(mock_rx_frame, mock_rx_len, &meta,
                                           &out, &out_len));
    mock_rx_frame[12] = saved;
    /* Wrong version/IHL */
    saved = mock_rx_frame[14];
    mock_rx_frame[14] = 0x46u;
    TEST_ASSERT_FALSE(Sc_EthUdp_ParseFrame(mock_rx_frame, mock_rx_len, &meta,
                                           &out, &out_len));
    mock_rx_frame[14] = saved;
    /* Wrong protocol */
    saved = mock_rx_frame[23];
    mock_rx_frame[23] = 6u;
    TEST_ASSERT_FALSE(Sc_EthUdp_ParseFrame(mock_rx_frame, mock_rx_len, &meta,
                                           &out, &out_len));
    mock_rx_frame[23] = saved;
    /* UDP length larger than what the frame carries */
    saved = mock_rx_frame[39];
    mock_rx_frame[39] = (uint8)(mock_rx_frame[39] + 32u);
    TEST_ASSERT_FALSE(Sc_EthUdp_ParseFrame(mock_rx_frame, mock_rx_len, &meta,
                                           &out, &out_len));
    mock_rx_frame[39] = saved;
    /* Intact frame still parses */
    TEST_ASSERT_TRUE(Sc_EthUdp_ParseFrame(mock_rx_frame, mock_rx_len, &meta,
                                          &out, &out_len));
}

/* ==================================================================
 * Dispatch tests
 * ================================================================== */

void test_dispatch_routes_registered_port(void)
{
    static const uint8 payload[3] = {7u, 8u, 9u};

    TEST_ASSERT_TRUE(Sc_EthRx_RegisterPort(50000u, test_port_handler));
    inject_udp(50000u, payload, 3u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT16(1u, handler_calls);
    TEST_ASSERT_EQUAL_UINT16(3u, handler_last_len);
    TEST_ASSERT_EQUAL_UINT16(50000u, handler_last_meta.dst_port);
    TEST_ASSERT_EQUAL_UINT16(61000u, handler_last_meta.src_port);
}

void test_dispatch_drops_unknown_port(void)
{
    static const uint8 payload[1] = {1u};

    TEST_ASSERT_TRUE(Sc_EthRx_RegisterPort(50000u, test_port_handler));
    inject_udp(50001u, payload, 1u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT16(0u, handler_calls);
}

void test_dispatch_rejects_when_table_full(void)
{
    uint16 port;
    /* One slot is used by the XCP handler from setUp. */
    for (port = 0u; port < (SC_ETH_RX_MAX_PORTS - 1u); port++) {
        TEST_ASSERT_TRUE(
            Sc_EthRx_RegisterPort((uint16)(40000u + port),
                                  test_port_handler));
    }
    TEST_ASSERT_FALSE(Sc_EthRx_RegisterPort(49999u, test_port_handler));
}

/* ==================================================================
 * XCP slave tests
 * ================================================================== */

void test_xcp_connect_response_layout(void)
{
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    do_connect();

    TEST_ASSERT_EQUAL_UINT16(8u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT16(1u, ctr);                 /* CTR echoed */
    TEST_ASSERT_EQUAL_UINT8(0xFFu, resp[0]);           /* RES_OK */
    TEST_ASSERT_EQUAL_UINT8(0x01u, resp[1]);           /* CAL/PAG */
    TEST_ASSERT_EQUAL_UINT8(0x01u, resp[2]);           /* big-endian slave */
    TEST_ASSERT_EQUAL_UINT8(8u, resp[3]);              /* MAX_CTO */
    TEST_ASSERT_EQUAL_UINT8(8u, resp[4]);              /* MAX_DTO lo */
    TEST_ASSERT_EQUAL_UINT8(0u, resp[5]);              /* MAX_DTO hi */
    TEST_ASSERT_EQUAL_UINT8(1u, resp[6]);              /* protocol ver */
    TEST_ASSERT_EQUAL_UINT8(1u, resp[7]);              /* transport ver */
    /* Response goes back to the requester */
    TEST_ASSERT_EQUAL_UINT16(61000u, meta.dst_port);
}

void test_xcp_commands_require_connection(void)
{
    static const uint8 status_cmd[1] = {0xFDu};
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    inject_xcp(status_cmd, 1u, 9u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT16(2u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFEu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x24u, resp[1]);           /* ACCESS_DENIED */
}

void test_xcp_get_status_after_connect(void)
{
    static const uint8 status_cmd[1] = {0xFDu};
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    do_connect();
    inject_xcp(status_cmd, 1u, 4u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT16(6u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT16(4u, ctr);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01u, resp[2]);           /* protection: CAL */
}

void test_xcp_short_upload_requires_unlock(void)
{
    static const uint8 upload_cmd[8] =
        {0xF4u, 4u, 0u, 0u, 0x08u, 0x00u, 0x10u, 0x00u};
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    do_connect();
    inject_xcp(upload_cmd, 8u, 5u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT16(2u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFEu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x24u, resp[1]);
}

void test_xcp_seed_key_then_short_upload(void)
{
    static const uint8 upload_cmd[8] =
        {0xF4u, 4u, 0u, 0u, 0x08u, 0x00u, 0x10u, 0x00u};
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    do_connect();
    do_unlock();

    inject_xcp(upload_cmd, 8u, 6u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT16(5u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT16(6u, ctr);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA0u, resp[1]);           /* test_mem[0..3] */
    TEST_ASSERT_EQUAL_UINT8(0xA1u, resp[2]);
    TEST_ASSERT_EQUAL_UINT8(0xA2u, resp[3]);
    TEST_ASSERT_EQUAL_UINT8(0xA3u, resp[4]);
}

void test_xcp_unlock_wrong_key_then_sequence_error(void)
{
    static const uint8 seed_cmd[3] = {0xF8u, 0x00u, 0x01u};
    static const uint8 bad_unlock[6] = {0xF7u, 4u, 0u, 0u, 0u, 0u};
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    do_connect();
    inject_xcp(seed_cmd, 3u, 7u);
    Sc_EthRx_Poll();

    inject_xcp(bad_unlock, 6u, 8u);
    Sc_EthRx_Poll();
    TEST_ASSERT_EQUAL_UINT16(2u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFEu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x25u, resp[1]);           /* KEY_REJECTED */

    /* Seed consumed — UNLOCK without a fresh GET_SEED is a sequence error */
    inject_xcp(bad_unlock, 6u, 9u);
    Sc_EthRx_Poll();
    TEST_ASSERT_EQUAL_UINT16(2u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFEu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x29u, resp[1]);           /* SEQUENCE */
}

void test_xcp_short_upload_out_of_range_address(void)
{
    static const uint8 upload_cmd[8] =
        {0xF4u, 4u, 0u, 0u, 0x20u, 0x00u, 0x00u, 0x00u};
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    do_connect();
    do_unlock();
    inject_xcp(upload_cmd, 8u, 10u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT16(2u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFEu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22u, resp[1]);           /* OUT_OF_RANGE */
}

void test_xcp_set_mta_then_upload(void)
{
    static const uint8 set_mta[8] =
        {0xF6u, 0u, 0u, 0u, 0x08u, 0x00u, 0x10u, 0x04u};  /* 0x08001004 */
    static const uint8 upload_cmd[2] = {0xF5u, 3u};
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    do_connect();
    do_unlock();

    inject_xcp(set_mta, 8u, 20u);
    Sc_EthRx_Poll();
    TEST_ASSERT_EQUAL_UINT16(1u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, resp[0]);

    inject_xcp(upload_cmd, 2u, 21u);
    Sc_EthRx_Poll();
    TEST_ASSERT_EQUAL_UINT16(4u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0xA4u, resp[1]);           /* test_mem[4..6] */
    TEST_ASSERT_EQUAL_UINT8(0xA5u, resp[2]);
    TEST_ASSERT_EQUAL_UINT8(0xA6u, resp[3]);
}

void test_xcp_get_comm_mode_info_after_connect(void)
{
    static const uint8 gcmi_cmd[1] = {0xFBu};
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    do_connect();
    inject_xcp(gcmi_cmd, 1u, 22u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT16(8u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFFu, resp[0]);
}

void test_xcp_unknown_command(void)
{
    static const uint8 bogus_cmd[1] = {0x00u};
    const uint8 *resp;
    uint16 ctr;
    sc_eth_udp_rx_meta_t meta;

    do_connect();
    inject_xcp(bogus_cmd, 1u, 11u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT16(2u, last_response(&resp, &ctr, &meta));
    TEST_ASSERT_EQUAL_UINT8(0xFEu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x20u, resp[1]);           /* CMD_UNKNOWN */
}

void test_xcp_malformed_transport_header_is_dropped(void)
{
    uint8 payload[6] = {200u, 0u, 1u, 0u, 0xFFu, 0x00u};  /* LEN=200, 2 bytes */

    do_connect();
    mock_tx_count = 0u;
    inject_udp(SC_XCP_ETH_PORT, payload, 6u);
    Sc_EthRx_Poll();

    TEST_ASSERT_EQUAL_UINT32(0u, mock_tx_count);       /* no response */
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_parse_round_trips_encoder_output);
    RUN_TEST(test_parse_rejects_malformed_frames);
    RUN_TEST(test_dispatch_routes_registered_port);
    RUN_TEST(test_dispatch_drops_unknown_port);
    RUN_TEST(test_dispatch_rejects_when_table_full);
    RUN_TEST(test_xcp_connect_response_layout);
    RUN_TEST(test_xcp_commands_require_connection);
    RUN_TEST(test_xcp_get_status_after_connect);
    RUN_TEST(test_xcp_short_upload_requires_unlock);
    RUN_TEST(test_xcp_seed_key_then_short_upload);
    RUN_TEST(test_xcp_unlock_wrong_key_then_sequence_error);
    RUN_TEST(test_xcp_short_upload_out_of_range_address);
    RUN_TEST(test_xcp_set_mta_then_upload);
    RUN_TEST(test_xcp_get_comm_mode_info_after_connect);
    RUN_TEST(test_xcp_unknown_command);
    RUN_TEST(test_xcp_malformed_transport_header_is_dropped);

    return UNITY_END();
}
