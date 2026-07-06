/**
 * @file    test_sc_eth.c
 * @brief   Unit tests for sc_eth descriptor parsing and frame bounds
 * @date    2026-07-06
 *
 * @verifies S-ETH-01
 *
 * Tests the pure RX descriptor readiness checks, RX copy bounds, link-state
 * wrapper, and TX padding logic for the SC Ethernet bench driver.
 */
#include "unity.h"

#define UNIT_TEST 1
#define SC_ETH_ENABLE 1

#include "sc_types.h"
#include "sc_eth.h"

/* ==================================================================
 * Mock HALCoGen Ethernet/MDIO types and constants
 * ================================================================== */

#define MAX_EMAC_INSTANCE       1u
#define MIN_PKT_LEN             60u
#define EMAC_ERR_OK             0u
#define EMAC_CHANNELNUMBER      0u
#define EMAC_BUF_DESC_OWNER     0x20000000u
#define EMAC_BUF_DESC_SOP       0x80000000u
#define EMAC_BUF_DESC_EOP       0x40000000u
#define EMAC_BUF_DESC_EOQ       0x10000000u
#define MDIO_0_BASE             0u
#define EMAC_PHYADDRESS         1u

typedef struct emac_rx_bd {
    volatile struct emac_rx_bd *next;
    volatile uint32 bufptr;
    volatile uint32 bufoff_len;
    volatile uint32 flags_pktlen;
} emac_rx_bd_t;

typedef struct rxch_struct {
    volatile emac_rx_bd_t *free_head;
    volatile emac_rx_bd_t *active_head;
    volatile emac_rx_bd_t *active_tail;
} rxch_t;

typedef struct txch_struct {
    uint8 unused;
} txch_t;

typedef struct hdkif_struct {
    uint8 mac_addr[6];
    txch_t txchptr;
    rxch_t rxchptr;
    uint32 emac_base;
    uint32 emac_ctrl_base;
} hdkif_t;

typedef struct pbuf {
    struct pbuf *next;
    void *payload;
    uint16 tot_len;
    uint16 len;
} pbuf_t;

/* ==================================================================
 * Mock HALCoGen functions
 * ================================================================== */

hdkif_t hdkif_data[MAX_EMAC_INSTANCE];
static emac_rx_bd_t mock_rx_bd;
static uint8 mock_dma_buf[SC_ETH_FRAME_MAX_LEN];
static uint32 mock_recycle_count;
static uint32 mock_clean_count;
static uint32 mock_cinv_count;
static uint32 mock_mdio_init_count;
static uint32 mock_eclk_count;
static uint32 mock_release_count;
static uint32 mock_delay_count;
static uint32 mock_tx_int_count;
static uint32 mock_rx_int_count;
static uint32 mock_transmit_count;
static uint32 mock_emac_init_count;
static uint16 mock_tx_len;
static uint16 mock_tx_tot_len;
static void *mock_tx_payload;
static boolean mock_transmit_result;
static uint32 mock_emac_init_result;
static volatile uint16 mock_phy_bsr;

uint32 EMACSwizzleData(uint32 word)
{
    return word;
}

void EMACReceive(hdkif_t *hdkif)
{
    (void)hdkif;
    mock_recycle_count++;
}

boolean EMACTransmit(hdkif_t *hdkif, pbuf_t *pbuf)
{
    (void)hdkif;
    mock_transmit_count++;
    mock_tx_len = pbuf->len;
    mock_tx_tot_len = pbuf->tot_len;
    mock_tx_payload = pbuf->payload;
    return mock_transmit_result;
}

uint32 EMACHWInit(uint8 macaddr[6])
{
    (void)macaddr;
    mock_emac_init_count++;
    return mock_emac_init_result;
}

void EMACTxIntPulseEnable(uint32 emacBase, uint32 emacCtrlBase,
                          uint32 core, uint32 channel)
{
    (void)emacBase;
    (void)emacCtrlBase;
    (void)core;
    (void)channel;
    mock_tx_int_count++;
}

void EMACRxIntPulseEnable(uint32 emacBase, uint32 emacCtrlBase,
                          uint32 core, uint32 channel)
{
    (void)emacBase;
    (void)emacCtrlBase;
    (void)core;
    (void)channel;
    mock_rx_int_count++;
}

void MDIOPhyRegRead(uint32 base, uint32 phyAddr, uint32 regNum,
                    volatile uint16 *data)
{
    (void)base;
    (void)phyAddr;
    if (regNum == 1u) {
        *data = mock_phy_bsr;
    } else {
        *data = 0u;
    }
}

void MDIOPhyRegWrite(uint32 base, uint32 phyAddr, uint32 regNum,
                     uint16 data)
{
    (void)base;
    (void)phyAddr;
    (void)regNum;
    (void)data;
}

const uint8 *Sc_Eth_Test_ResolveRxBuffer(uint32 bufPtr)
{
    if (bufPtr == 0x1234u) {
        return mock_dma_buf;
    }
    return NULL_PTR;
}

void Sc_Eth_Test_CacheClean(uint32 start, uint32 len)
{
    (void)start;
    (void)len;
    mock_clean_count++;
}

void Sc_Eth_Test_CacheCleanInvalidate(uint32 start, uint32 len)
{
    (void)start;
    (void)len;
    mock_cinv_count++;
}

void Sc_Eth_Test_InitMdioManual(void) { mock_mdio_init_count++; }
void Sc_Eth_Test_EnableEclk25MHz(void) { mock_eclk_count++; }
void Sc_Eth_Test_ReleasePhyPins(void) { mock_release_count++; }
void Sc_Eth_Test_Delay(uint32 loops) { (void)loops; mock_delay_count++; }

/* ==================================================================
 * Include source under test
 * ================================================================== */

#include "../src/sc_eth.c"

/* ==================================================================
 * Test setup / teardown
 * ================================================================== */

void setUp(void)
{
    uint16 i;

    for (i = 0u; i < SC_ETH_FRAME_MAX_LEN; i++) {
        mock_dma_buf[i] = (uint8)(i & 0xFFu);
    }

    mock_rx_bd.next = NULL_PTR;
    mock_rx_bd.bufptr = 0x1234u;
    mock_rx_bd.bufoff_len = 0u;
    mock_rx_bd.flags_pktlen = 0u;

    hdkif_data[0].rxchptr.free_head = NULL_PTR;
    hdkif_data[0].rxchptr.active_head = &mock_rx_bd;
    hdkif_data[0].rxchptr.active_tail = &mock_rx_bd;
    hdkif_data[0].emac_base = 0u;
    hdkif_data[0].emac_ctrl_base = 0u;

    mock_recycle_count = 0u;
    mock_clean_count = 0u;
    mock_cinv_count = 0u;
    mock_mdio_init_count = 0u;
    mock_eclk_count = 0u;
    mock_release_count = 0u;
    mock_delay_count = 0u;
    mock_tx_int_count = 0u;
    mock_rx_int_count = 0u;
    mock_transmit_count = 0u;
    mock_emac_init_count = 0u;
    mock_tx_len = 0u;
    mock_tx_tot_len = 0u;
    mock_tx_payload = NULL_PTR;
    mock_transmit_result = TRUE;
    mock_emac_init_result = EMAC_ERR_OK;
    mock_phy_bsr = 0u;

    g_sc_eth_hdkif = &hdkif_data[0];
    g_sc_eth_initialized = TRUE;
}

void tearDown(void) { }

/* ==================================================================
 * Descriptor readiness
 * ================================================================== */

void test_desc_ready_accepts_complete_unowned_frame(void)
{
    uint16 length = 0u;
    boolean ready = sc_eth_desc_ready(EMAC_BUF_DESC_SOP | EMAC_BUF_DESC_EOP | 64u,
                                      0x1234u, SC_ETH_FRAME_MAX_LEN, &length);

    TEST_ASSERT_TRUE(ready);
    TEST_ASSERT_EQUAL_UINT16(64u, length);
}

void test_desc_ready_rejects_owner_bit(void)
{
    uint16 length = 0u;
    boolean ready = sc_eth_desc_ready(EMAC_BUF_DESC_SOP | EMAC_BUF_DESC_EOP |
                                      EMAC_BUF_DESC_OWNER | 64u,
                                      0x1234u, SC_ETH_FRAME_MAX_LEN, &length);

    TEST_ASSERT_FALSE(ready);
    TEST_ASSERT_EQUAL_UINT16(64u, length);
}

void test_desc_ready_rejects_partial_frame(void)
{
    uint16 length = 0u;
    boolean ready = sc_eth_desc_ready(EMAC_BUF_DESC_SOP | 64u,
                                      0x1234u, SC_ETH_FRAME_MAX_LEN, &length);

    TEST_ASSERT_FALSE(ready);
}

void test_desc_ready_rejects_zero_length(void)
{
    uint16 length = 0u;
    boolean ready = sc_eth_desc_ready(EMAC_BUF_DESC_SOP | EMAC_BUF_DESC_EOP,
                                      0x1234u, SC_ETH_FRAME_MAX_LEN, &length);

    TEST_ASSERT_FALSE(ready);
    TEST_ASSERT_EQUAL_UINT16(0u, length);
}

void test_desc_ready_rejects_destination_too_small(void)
{
    uint16 length = 0u;
    boolean ready = sc_eth_desc_ready(EMAC_BUF_DESC_SOP | EMAC_BUF_DESC_EOP | 128u,
                                      0x1234u, 64u, &length);

    TEST_ASSERT_FALSE(ready);
    TEST_ASSERT_EQUAL_UINT16(128u, length);
}

void test_desc_ready_rejects_null_buffer_pointer(void)
{
    uint16 length = 0u;
    boolean ready = sc_eth_desc_ready(EMAC_BUF_DESC_SOP | EMAC_BUF_DESC_EOP | 64u,
                                      0u, SC_ETH_FRAME_MAX_LEN, &length);

    TEST_ASSERT_FALSE(ready);
}

/* ==================================================================
 * Public RX/TX/link wrappers
 * ================================================================== */

void test_poll_rx_copies_ready_frame_and_recycles_descriptor(void)
{
    uint8 dst[128];
    uint16 length = 0u;
    boolean got_frame;

    mock_rx_bd.flags_pktlen = EMAC_BUF_DESC_SOP | EMAC_BUF_DESC_EOP | 64u;
    got_frame = Sc_Eth_PollRx(dst, sizeof(dst), &length);

    TEST_ASSERT_TRUE(got_frame);
    TEST_ASSERT_EQUAL_UINT16(64u, length);
    TEST_ASSERT_EQUAL_UINT8(mock_dma_buf[0], dst[0]);
    TEST_ASSERT_EQUAL_UINT8(mock_dma_buf[63], dst[63]);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_cinv_count);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_recycle_count);
}

void test_poll_rx_recycles_oversize_frame_without_copying(void)
{
    uint8 dst[64];
    uint16 length = 0u;
    boolean got_frame;

    dst[0] = 0xAAu;
    mock_rx_bd.flags_pktlen = EMAC_BUF_DESC_SOP | EMAC_BUF_DESC_EOP | 128u;
    got_frame = Sc_Eth_PollRx(dst, sizeof(dst), &length);

    TEST_ASSERT_FALSE(got_frame);
    TEST_ASSERT_EQUAL_UINT16(128u, length);
    TEST_ASSERT_EQUAL_UINT8(0xAAu, dst[0]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_cinv_count);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_recycle_count);
}

void test_poll_rx_rejects_null_outputs(void)
{
    uint8 dst[64];
    uint16 length = 0u;

    TEST_ASSERT_FALSE(Sc_Eth_PollRx(NULL_PTR, sizeof(dst), &length));
    TEST_ASSERT_FALSE(Sc_Eth_PollRx(dst, sizeof(dst), NULL_PTR));
}

void test_tx_pads_short_frame_before_transmit(void)
{
    uint8 frame[SC_ETH_MIN_TX_FRAME_LEN];
    Std_ReturnType status;

    frame[0] = 0x11u;
    status = Sc_Eth_Tx(frame, 42u);

    TEST_ASSERT_EQUAL_UINT8(E_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_clean_count);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_transmit_count);
    TEST_ASSERT_EQUAL_UINT16(SC_ETH_MIN_TX_FRAME_LEN, mock_tx_len);
    TEST_ASSERT_EQUAL_UINT16(SC_ETH_MIN_TX_FRAME_LEN, mock_tx_tot_len);
    TEST_ASSERT_EQUAL_PTR(frame, mock_tx_payload);
    TEST_ASSERT_EQUAL_UINT8(0u, frame[SC_ETH_MIN_TX_FRAME_LEN - 1u]);
}

void test_tx_rejects_invalid_arguments(void)
{
    uint8 frame[SC_ETH_MIN_TX_FRAME_LEN];

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, Sc_Eth_Tx(NULL_PTR, 42u));
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, Sc_Eth_Tx(frame, 0u));
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, Sc_Eth_Tx(frame, (uint16)(SC_ETH_FRAME_MAX_LEN + 1u)));
    TEST_ASSERT_EQUAL_UINT32(0u, mock_transmit_count);
}

void test_link_up_reads_phy_bsr_link_bit(void)
{
    mock_phy_bsr = 0x0004u;
    TEST_ASSERT_TRUE(Sc_Eth_LinkUp());

    mock_phy_bsr = 0x0000u;
    TEST_ASSERT_FALSE(Sc_Eth_LinkUp());
}

void test_init_sets_mdio_before_phy_wake_and_enables_emac(void)
{
    const uint8 mac[SC_ETH_MAC_ADDR_LEN] = {0x02u, 0x00u, 0x4Bu, 0x57u, 0x01u, 0x00u};
    Std_ReturnType status;

    g_sc_eth_hdkif = NULL_PTR;
    g_sc_eth_initialized = FALSE;
    mock_phy_bsr = 0x0004u;

    status = Sc_Eth_Init(mac);

    TEST_ASSERT_EQUAL_UINT8(E_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_eclk_count);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_release_count);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_mdio_init_count);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_emac_init_count);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_tx_int_count);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rx_int_count);
    TEST_ASSERT_TRUE(mock_delay_count >= 2u);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_desc_ready_accepts_complete_unowned_frame);
    RUN_TEST(test_desc_ready_rejects_owner_bit);
    RUN_TEST(test_desc_ready_rejects_partial_frame);
    RUN_TEST(test_desc_ready_rejects_zero_length);
    RUN_TEST(test_desc_ready_rejects_destination_too_small);
    RUN_TEST(test_desc_ready_rejects_null_buffer_pointer);

    RUN_TEST(test_poll_rx_copies_ready_frame_and_recycles_descriptor);
    RUN_TEST(test_poll_rx_recycles_oversize_frame_without_copying);
    RUN_TEST(test_poll_rx_rejects_null_outputs);
    RUN_TEST(test_tx_pads_short_frame_before_transmit);
    RUN_TEST(test_tx_rejects_invalid_arguments);
    RUN_TEST(test_link_up_reads_phy_bsr_link_bit);
    RUN_TEST(test_init_sets_mdio_before_phy_wake_and_enables_emac);

    return UNITY_END();
}
