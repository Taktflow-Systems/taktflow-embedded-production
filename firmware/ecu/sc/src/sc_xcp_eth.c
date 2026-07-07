/**
 * @file    sc_xcp_eth.c
 * @brief   Minimal XCP V1.5 slave over Ethernet/UDP for the SC (S-XCP-02)
 * @date    2026-07-07
 *
 * @traces_to docs/plans/memo-sc-xcp-eth-transport.md (approved minimal-slave),
 *            docs/plans/plan-sc-ethernet-roadmap.md S-XCP-02
 * @details Command logic is copied-portable from
 *          firmware/bsw/services/Xcp/src/Xcp.c (CAN slave) — the two MUST
 *          stay behaviourally identical for the shared Seed&Key and
 *          command semantics; the master key derivation in tools/xcp
 *          depends on it. Differences from the CAN slave: UDP transport
 *          with the ASAM 4-byte header, a TMS570-tight address whitelist,
 *          and big-endian COMM_MODE advertised at CONNECT.
 * @note    Safety level: QM instrumentation only; excluded from safety
 *          builds. SC main task context only (no ISR, no reentrancy).
 * @standard MISRA C:2012, ASAM MCD-1 XCP V1.5
 * @copyright Taktflow Systems 2026
 */
#ifdef SC_ETH_ENABLE

#include "sc_xcp_eth.h"
#include "sc_eth_udp.h"
#include "sc_eth_rx_dispatch.h"
#ifndef UNIT_TEST
#include <stdint.h>            /* uintptr_t for the validated address read */
#endif

/* ---- XCP command / response / error codes (match Xcp.h) ---- */
#define SC_XCP_CMD_CONNECT             0xFFu
#define SC_XCP_CMD_DISCONNECT          0xFEu
#define SC_XCP_CMD_GET_STATUS          0xFDu
#define SC_XCP_CMD_GET_COMM_MODE_INFO  0xFBu
#define SC_XCP_CMD_SHORT_UPLOAD        0xF4u
#define SC_XCP_CMD_SET_MTA             0xF6u
#define SC_XCP_CMD_UPLOAD              0xF5u
#define SC_XCP_CMD_GET_SEED            0xF8u
#define SC_XCP_CMD_UNLOCK              0xF7u

#define SC_XCP_RES_OK                  0xFFu
#define SC_XCP_RES_ERR                 0xFEu

#define SC_XCP_ERR_CMD_UNKNOWN         0x20u
#define SC_XCP_ERR_CMD_SYNTAX          0x21u
#define SC_XCP_ERR_OUT_OF_RANGE        0x22u
#define SC_XCP_ERR_ACCESS_DENIED       0x24u
#define SC_XCP_ERR_KEY_REJECTED        0x25u
#define SC_XCP_ERR_SEQUENCE            0x29u

#define SC_XCP_RESOURCE_CAL_PAG        0x01u
#define SC_XCP_MAX_CTO                 8u
#define SC_XCP_MAX_DTO                 8u
#define SC_XCP_MAX_UNLOCK_ATTEMPTS     3u

/** ASAM XCP-on-Ethernet transport header: LEN(u16 LE) + CTR(u16 LE). */
#define SC_XCP_ETH_HDR_LEN             4u

/* Host-under-test hook: real target dereferences the validated address.
 * DEV-003 (MISRA Rule 11.4, Advisory, Approved): XCP requires an
 * integer-to-pointer conversion for memory upload; uintptr_t is the
 * portable idiom (C99 7.20.1.4). The address is range-checked by
 * sc_xcp_validate_address() before every read. Mirrors Xcp.c. */
#ifdef UNIT_TEST
extern uint8 test_xcp_mem_read(uint32 addr);
#define SC_XCP_READ8(addr)  test_xcp_mem_read(addr)
#else
/* cppcheck-suppress misra-c2012-11.4 */
#define SC_XCP_READ8(addr)  (*(const volatile uint8 *)(uintptr_t)(addr))
#endif

/* ==================================================================
 * Module state (single-threaded, SC main task)
 * ================================================================== */

static boolean xcp_connected;
static boolean xcp_unlocked;
static boolean xcp_seed_pending;
static uint32  xcp_seed;
static uint32  xcp_mta;
static uint8   xcp_unlock_fail_count;
static uint32  xcp_lfsr_state = 0xDEADBEEFu;

/** Current request's requester, valid only during a dispatch call. */
static sc_eth_udp_rx_meta_t xcp_req_meta;
static uint16  xcp_req_ctr;

/** Response staging: [4-byte transport header][XCP response]. */
static uint8   xcp_tx_payload[SC_XCP_ETH_HDR_LEN + SC_XCP_MAX_CTO];

/* ==================================================================
 * Response transport
 * ================================================================== */

static void sc_xcp_send_response(const uint8 *packet, uint16 packet_len)
{
    uint16 i;

    xcp_tx_payload[0] = (uint8)(packet_len & 0xFFu);
    xcp_tx_payload[1] = (uint8)((packet_len >> 8u) & 0xFFu);
    xcp_tx_payload[2] = (uint8)(xcp_req_ctr & 0xFFu);
    xcp_tx_payload[3] = (uint8)((xcp_req_ctr >> 8u) & 0xFFu);
    for (i = 0u; i < packet_len; i++) {
        xcp_tx_payload[SC_XCP_ETH_HDR_LEN + i] = packet[i];
    }

    /* Reply to the requester's learned MAC/IP/port — the SC has no ARP
     * responder, so it cannot resolve the master by IP alone. */
    (void)Sc_EthUdp_SendTo(xcp_req_meta.src_mac, xcp_req_meta.src_ip,
                           xcp_req_meta.src_port, xcp_tx_payload,
                           (uint16)(SC_XCP_ETH_HDR_LEN + packet_len));
}

static void sc_xcp_send_error(uint8 err_code)
{
    uint8 packet[2];
    packet[0] = SC_XCP_RES_ERR;
    packet[1] = err_code;
    sc_xcp_send_response(packet, 2u);
}

/* ==================================================================
 * Seed & Key (must match Xcp.c xcp_generate_seed / xcp_compute_key)
 * ================================================================== */

static uint32 sc_xcp_generate_seed(void)
{
    uint8 i;
    for (i = 0u; i < 32u; i++) {
        if ((xcp_lfsr_state & 1u) != 0u) {
            xcp_lfsr_state = (xcp_lfsr_state >> 1u) ^ 0x80200003u;
        } else {
            xcp_lfsr_state = xcp_lfsr_state >> 1u;
        }
    }
    if (xcp_lfsr_state == 0u) {
        xcp_lfsr_state = 0xDEADBEEFu;
    }
    return xcp_lfsr_state;
}

static uint32 sc_xcp_compute_key(uint32 seed)
{
    uint32 key = seed ^ 0x54414B54u;          /* "TAKT" */
    key = ((key << 13u) | (key >> 19u));      /* ROL 13 */
    key ^= 0x464C4F57u;                        /* "FLOW" */
    return key;
}

/* ==================================================================
 * Address whitelist (TMS570-specific — tighter than the CAN slave's
 * multi-MCU superset): Flash 0x00000000-0x003FFFFF, SRAM
 * 0x08000000-0x0803FFFF. Null page rejected.
 * ================================================================== */

static boolean sc_xcp_validate_address(uint32 addr, uint8 num_bytes)
{
    uint32 end = addr + (uint32)num_bytes;
    boolean in_flash;
    boolean in_sram;

    if ((addr < 0x00000100u) || (end < addr)) {
        return FALSE;
    }
    in_flash = (addr >= 0x00000100u) && (end <= 0x00400000u);
    in_sram  = (addr >= 0x08000000u) && (end <= 0x08040000u);
    return (in_flash || in_sram) ? TRUE : FALSE;
}

/* ==================================================================
 * Command handlers (ported from Xcp.c, UDP transport)
 * ================================================================== */

static void sc_xcp_cmd_connect(void)
{
    uint8 packet[8];

    xcp_connected    = TRUE;
    xcp_unlocked     = FALSE;   /* Seed&Key required after every CONNECT */
    xcp_seed_pending = FALSE;
    xcp_seed         = 0u;

    packet[0] = SC_XCP_RES_OK;
    packet[1] = SC_XCP_RESOURCE_CAL_PAG;
    packet[2] = 0x01u;          /* COMM_MODE_BASIC: big-endian (TMS570) */
    packet[3] = SC_XCP_MAX_CTO;
    packet[4] = (uint8)(SC_XCP_MAX_DTO & 0xFFu);
    packet[5] = (uint8)(SC_XCP_MAX_DTO >> 8u);
    packet[6] = 1u;             /* protocol version major */
    packet[7] = 1u;             /* transport version major */
    sc_xcp_send_response(packet, 8u);
}

static void sc_xcp_cmd_disconnect(void)
{
    xcp_connected    = FALSE;
    xcp_unlocked     = FALSE;
    xcp_seed_pending = FALSE;
    xcp_seed         = 0u;
    xcp_mta          = 0u;
    {
        uint8 packet[1];
        packet[0] = SC_XCP_RES_OK;
        sc_xcp_send_response(packet, 1u);
    }
}

static void sc_xcp_cmd_get_status(void)
{
    uint8 packet[6];
    packet[0] = SC_XCP_RES_OK;
    packet[1] = 0x00u;
    packet[2] = SC_XCP_RESOURCE_CAL_PAG;
    packet[3] = 0x00u;
    packet[4] = 0x00u;
    packet[5] = 0x00u;
    sc_xcp_send_response(packet, 6u);
}

static void sc_xcp_cmd_get_comm_mode_info(void)
{
    uint8 packet[8];
    packet[0] = SC_XCP_RES_OK;
    packet[1] = 0x00u;
    packet[2] = 0x00u;
    packet[3] = 0x00u;
    packet[4] = 0x00u;
    packet[5] = 0x00u;
    packet[6] = 0x00u;
    packet[7] = 0x00u;          /* driver version minor */
    sc_xcp_send_response(packet, 8u);
}

static void sc_xcp_cmd_get_seed(const uint8 *data, uint16 length)
{
    uint8 packet[6];

    if (length < 3u) {
        sc_xcp_send_error(SC_XCP_ERR_CMD_SYNTAX);
        return;
    }
    (void)data;

    if (xcp_unlock_fail_count >= SC_XCP_MAX_UNLOCK_ATTEMPTS) {
        sc_xcp_send_error(SC_XCP_ERR_ACCESS_DENIED);
        return;
    }
    if (xcp_unlocked == TRUE) {
        packet[0] = SC_XCP_RES_OK;
        packet[1] = 0u;         /* already unlocked — no seed */
        sc_xcp_send_response(packet, 2u);
        return;
    }

    xcp_seed = sc_xcp_generate_seed();
    xcp_seed_pending = TRUE;

    packet[0] = SC_XCP_RES_OK;
    packet[1] = 4u;
    packet[2] = (uint8)((xcp_seed >> 24u) & 0xFFu);
    packet[3] = (uint8)((xcp_seed >> 16u) & 0xFFu);
    packet[4] = (uint8)((xcp_seed >>  8u) & 0xFFu);
    packet[5] = (uint8)((xcp_seed >>  0u) & 0xFFu);
    sc_xcp_send_response(packet, 6u);
}

static void sc_xcp_cmd_unlock(const uint8 *data, uint16 length)
{
    uint32 received_key;
    uint32 expected_key;
    uint8  packet[2];

    if (length < 6u) {
        sc_xcp_send_error(SC_XCP_ERR_CMD_SYNTAX);
        return;
    }
    if (xcp_seed_pending != TRUE) {
        sc_xcp_send_error(SC_XCP_ERR_SEQUENCE);
        return;
    }
    if (xcp_unlock_fail_count >= SC_XCP_MAX_UNLOCK_ATTEMPTS) {
        sc_xcp_send_error(SC_XCP_ERR_ACCESS_DENIED);
        return;
    }

    received_key = ((uint32)data[2] << 24u) | ((uint32)data[3] << 16u) |
                   ((uint32)data[4] <<  8u) | (uint32)data[5];
    expected_key = sc_xcp_compute_key(xcp_seed);

    if (received_key == expected_key) {
        xcp_unlocked          = TRUE;
        xcp_seed_pending      = FALSE;
        xcp_seed              = 0u;
        xcp_unlock_fail_count = 0u;
        packet[0] = SC_XCP_RES_OK;
        packet[1] = SC_XCP_RESOURCE_CAL_PAG;
        sc_xcp_send_response(packet, 2u);
    } else {
        xcp_unlock_fail_count++;
        xcp_seed_pending = FALSE;   /* force a fresh GET_SEED */
        xcp_seed         = 0u;
        sc_xcp_send_error(SC_XCP_ERR_KEY_REJECTED);
    }
}

static void sc_xcp_read_to_response(uint32 addr, uint8 num_bytes)
{
    uint8 packet[SC_XCP_MAX_CTO];
    uint8 i;

    if ((num_bytes == 0u) || (num_bytes > (SC_XCP_MAX_CTO - 1u))) {
        sc_xcp_send_error(SC_XCP_ERR_OUT_OF_RANGE);
        return;
    }
    if (sc_xcp_validate_address(addr, num_bytes) == FALSE) {
        sc_xcp_send_error(SC_XCP_ERR_OUT_OF_RANGE);
        return;
    }

    packet[0] = SC_XCP_RES_OK;
    for (i = 0u; i < num_bytes; i++) {
        packet[1u + i] = SC_XCP_READ8(addr + (uint32)i);
    }
    xcp_mta = addr + (uint32)num_bytes;
    sc_xcp_send_response(packet, (uint16)(1u + num_bytes));
}

static void sc_xcp_cmd_short_upload(const uint8 *data, uint16 length)
{
    uint32 addr;
    uint8  num_bytes;

    if (xcp_unlocked != TRUE) {
        sc_xcp_send_error(SC_XCP_ERR_ACCESS_DENIED);
        return;
    }
    if (length < 8u) {
        sc_xcp_send_error(SC_XCP_ERR_CMD_SYNTAX);
        return;
    }
    num_bytes = data[1];
    addr = ((uint32)data[4] << 24u) | ((uint32)data[5] << 16u) |
           ((uint32)data[6] <<  8u) | (uint32)data[7];
    sc_xcp_read_to_response(addr, num_bytes);
}

static void sc_xcp_cmd_set_mta(const uint8 *data, uint16 length)
{
    uint8 packet[1];

    if (length < 8u) {
        sc_xcp_send_error(SC_XCP_ERR_CMD_SYNTAX);
        return;
    }
    xcp_mta = ((uint32)data[4] << 24u) | ((uint32)data[5] << 16u) |
              ((uint32)data[6] <<  8u) | (uint32)data[7];
    packet[0] = SC_XCP_RES_OK;
    sc_xcp_send_response(packet, 1u);
}

static void sc_xcp_cmd_upload(const uint8 *data, uint16 length)
{
    uint8 num_bytes;

    if (xcp_unlocked != TRUE) {
        sc_xcp_send_error(SC_XCP_ERR_ACCESS_DENIED);
        return;
    }
    if (length < 2u) {
        sc_xcp_send_error(SC_XCP_ERR_CMD_SYNTAX);
        return;
    }
    num_bytes = data[1];
    sc_xcp_read_to_response(xcp_mta, num_bytes);
}

/* ==================================================================
 * RX handler (bound to SC_XCP_ETH_PORT)
 * ================================================================== */

static void sc_xcp_eth_handler(const uint8 *payload, uint16 len,
                               const sc_eth_udp_rx_meta_t *meta)
{
    uint16 xcp_len;
    const uint8 *packet;
    uint8 cmd;

    /* ASAM XCP-on-Ethernet transport header: LEN(u16 LE) + CTR(u16 LE) */
    if (len < SC_XCP_ETH_HDR_LEN) {
        return;   /* not an XCP-on-Eth frame — drop */
    }
    xcp_len = (uint16)payload[0] | ((uint16)payload[1] << 8u);
    if ((xcp_len == 0u) ||
        ((uint32)SC_XCP_ETH_HDR_LEN + (uint32)xcp_len > (uint32)len)) {
        return;   /* length field inconsistent — drop (fail-closed) */
    }

    xcp_req_meta = *meta;
    xcp_req_ctr  = (uint16)payload[2] | ((uint16)payload[3] << 8u);
    packet       = &payload[SC_XCP_ETH_HDR_LEN];
    cmd          = packet[0];

    if (cmd == SC_XCP_CMD_CONNECT) {
        sc_xcp_cmd_connect();
        return;
    }
    if (xcp_connected == FALSE) {
        sc_xcp_send_error(SC_XCP_ERR_ACCESS_DENIED);
        return;
    }

    switch (cmd) {
    case SC_XCP_CMD_DISCONNECT:
        sc_xcp_cmd_disconnect();
        break;
    case SC_XCP_CMD_GET_STATUS:
        sc_xcp_cmd_get_status();
        break;
    case SC_XCP_CMD_GET_COMM_MODE_INFO:
        sc_xcp_cmd_get_comm_mode_info();
        break;
    case SC_XCP_CMD_SHORT_UPLOAD:
        sc_xcp_cmd_short_upload(packet, xcp_len);
        break;
    case SC_XCP_CMD_SET_MTA:
        sc_xcp_cmd_set_mta(packet, xcp_len);
        break;
    case SC_XCP_CMD_UPLOAD:
        sc_xcp_cmd_upload(packet, xcp_len);
        break;
    case SC_XCP_CMD_GET_SEED:
        sc_xcp_cmd_get_seed(packet, xcp_len);
        break;
    case SC_XCP_CMD_UNLOCK:
        sc_xcp_cmd_unlock(packet, xcp_len);
        break;
    default:
        sc_xcp_send_error(SC_XCP_ERR_CMD_UNKNOWN);
        break;
    }
}

/* ==================================================================
 * Public API
 * ================================================================== */

void Sc_XcpEth_Init(void)
{
    xcp_connected         = FALSE;
    xcp_unlocked          = FALSE;
    xcp_seed_pending      = FALSE;
    xcp_seed              = 0u;
    xcp_mta               = 0u;
    xcp_unlock_fail_count = 0u;
    (void)Sc_EthRx_RegisterPort(SC_XCP_ETH_PORT, sc_xcp_eth_handler);
}

boolean Sc_XcpEth_IsConnected(void)
{
    return xcp_connected;
}

#endif /* SC_ETH_ENABLE */
