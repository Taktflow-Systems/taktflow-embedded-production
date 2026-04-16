/**
 * @file    test_eth_ping.c
 * @brief   Standalone Ethernet ping test for TMS570LC4357 LaunchPad
 * @date    2026-03-28
 *
 * @details Minimal firmware that initializes EMAC + DP83630 PHY and responds
 *          to ARP requests and ICMP echo (ping) requests. No safety stack,
 *          no CAN, no scheduler — just Ethernet.
 *
 *          Build:  make -f firmware/platform/tms570/Makefile.tms570 eth-ping
 *          Flash:  make -f firmware/platform/tms570/Makefile.tms570 flash-eth-ping
 *          Test:   ping 203.0.113.200
 *
 * @note    GIOA[3]=PHY_PWRDOWN (LOW=normal, HIGH=powerdown) — schematic pin 7
 *          GIOA[4]=PHY_RESET_N (HIGH=release, LOW=reset) — schematic pin 29
 *          These pins conflict with SC LED assignments — this test is standalone.
 *
 * @note    Cache must be write-through for EMAC DMA coherency (TI E2E known issue).
 */

#include "HL_sys_common.h"
#include "HL_system.h"
#include "HL_gio.h"
#include "HL_sci.h"
#include "HL_reg_sci.h"
#include "HL_emac.h"
#include "HL_mdio.h"
#include "HL_phy_dp83640.h"
#include "HL_sys_vim.h"

/* ================================================================
 * HALCoGen notification stubs (HL_notification.c excluded to avoid
 * WEAK pragma issues with tiarmclang — provide all required symbols)
 * ================================================================ */

/* ESM notifications */
void esmGroup1Notification(uint32 channel) { (void)channel; }
void esmGroup2Notification(uint32 channel) { (void)channel; }
void esmGroup3Notification(uint32 channel) { (void)channel; }

/* Other HALCoGen notifications that may be referenced */
void sciNotification(sciBASE_t *sci, uint32 flags) { (void)sci; (void)flags; }
void gioNotification(gioPORT_t *port, uint32 bit) { (void)port; (void)bit; }
void canMessageNotification(void *node, uint32 messageBox) { (void)node; (void)messageBox; }
void canErrorNotification(void *node, uint32 notification) { (void)node; (void)notification; }

/* ================================================================
 * Network Configuration
 * ================================================================ */

/* TMS570 static IP: 203.0.113.200 */
static const uint8 g_my_ip[4]  = { 192U, 168U, 1U, 200U };

/* MAC address — locally administered, unique on bench */
static uint8 g_my_mac[6] = { 0x02U, 0x00U, 0x4BU, 0x57U, 0x01U, 0x00U };

/* ================================================================
 * Ethernet Protocol Constants
 * ================================================================ */

#define ETH_TYPE_ARP    0x0806U
#define ETH_TYPE_IP     0x0800U
#define IP_PROTO_ICMP   1U
#define ARP_OP_REQUEST  1U
#define ARP_OP_REPLY    2U
#define ICMP_ECHO_REQ   8U
#define ICMP_ECHO_REPLY 0U

/* ================================================================
 * Global state
 * ================================================================ */

static hdkif_t *g_hdkif = NULL;

/* RX packet buffer — filled by emacRxNotification */
static volatile uint8  g_rx_buf[1520];
static volatile uint32 g_rx_len = 0U;
static volatile uint32 g_rx_ready = 0U;

/* TX packet buffer */
static uint8 g_tx_buf[1520];

/* Debug counters */
static volatile uint32 g_rx_count = 0U;
static volatile uint32 g_tx_count = 0U;
static volatile uint32 g_arp_count = 0U;
static volatile uint32 g_icmp_count = 0U;

/* ================================================================
 * SCI (UART) — raw register access, same as working SC firmware
 * ================================================================ */

#define IOMM_BASE       0xFFFF1C00U
#define SCI1_BASE       0xFFF7E400U
#define SCI_GCR0        0x00U
#define SCI_GCR1        0x04U
#define SCI_BRS         0x2CU
#define SCI_FORMAT      0x28U
#define SCI_FLR         0x1CU
#define SCI_TD          0x38U
#define SCI_PIO0        0x3CU
#define SCI_FLR_TXRDY   0x00000100U

static void reg_write(uint32 base, uint32 off, uint32 val)
{
    *(volatile uint32 *)(base + off) = val;
}

static uint32 reg_read(uint32 base, uint32 off)
{
    return *(volatile uint32 *)(base + off);
}

static void uart_init(void)
{
    /* 1. Route LIN1TX to ball A5 (XDS110 UART on COM11) */
    reg_write(IOMM_BASE, 0x38U, 0x83E70B13U);  /* KICKER0 unlock */
    reg_write(IOMM_BASE, 0x3CU, 0x95A4F1E0U);  /* KICKER1 unlock */
    reg_write(IOMM_BASE, 0x15CU, 0x02020202U); /* PINMUX83: LIN1TX on A5 */
    reg_write(IOMM_BASE, 0x38U, 0U);            /* lock */

    /* 2. Init SCI1/LIN1: 115200 baud, 8N1, async mode
     * Same register sequence as working SC firmware (sc_hw_tms570.c) */
    reg_write(SCI1_BASE, SCI_GCR0, 0U);  /* reset */
    reg_write(SCI1_BASE, SCI_GCR0, 1U);  /* release reset */
    reg_write(SCI1_BASE, 0x10U, 0xFFFFFFFFU);  /* clear interrupts */
    reg_write(SCI1_BASE, 0x18U, 0xFFFFFFFFU);  /* clear int levels */

    /* GCR1: TX+RX enable, internal clock, async, 1 stop bit
     * NOTE: bit 6 must be CLEAR — it enables LIN mode on SCI1 */
    reg_write(SCI1_BASE, SCI_GCR1,
              ((uint32)1U << 25U) |   /* TXENA */
              ((uint32)1U << 24U) |   /* RXENA */
              ((uint32)1U << 5U)  |   /* CLOCK = internal */
              ((uint32)1U << 1U));    /* TIMING = async */

    reg_write(SCI1_BASE, SCI_BRS, 40U);     /* 75MHz / (40+1) / 16 = 114329 */
    reg_write(SCI1_BASE, SCI_FORMAT, 7U);    /* 8 data bits */
    reg_write(SCI1_BASE, SCI_PIO0, 6U);     /* TX+RX functional */
    reg_write(SCI1_BASE, 0x40U, 0U);        /* PIO1 */
    reg_write(SCI1_BASE, 0x48U, 0U);        /* PIO3 */
    reg_write(SCI1_BASE, 0x54U, 0U);        /* PIO6 */
    reg_write(SCI1_BASE, 0x58U, 0U);        /* PIO7 */
    reg_write(SCI1_BASE, 0x5CU, 6U);        /* PIO8: pull select */

    /* Release from reset */
    {
        uint32 gcr1 = reg_read(SCI1_BASE, SCI_GCR1);
        gcr1 |= 0x80U;
        reg_write(SCI1_BASE, SCI_GCR1, gcr1);
    }
}

static void uart_puts(const char *s)
{
    while (*s != '\0') {
        volatile uint32 timeout = 100000U;
        while (((reg_read(SCI1_BASE, SCI_FLR) & SCI_FLR_TXRDY) == 0U) && (timeout > 0U)) {
            timeout--;
        }
        if (timeout > 0U) {
            reg_write(SCI1_BASE, SCI_TD, (uint32)(uint8)*s);
        }
        s++;
    }
}

static void uart_put_hex8(uint8 val)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[3];
    buf[0] = hex[(val >> 4U) & 0x0FU];
    buf[1] = hex[val & 0x0FU];
    buf[2] = '\0';
    uart_puts(buf);
}

static void uart_put_dec(uint32 val)
{
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (val == 0U) {
        buf[--i] = '0';
    } else {
        while (val > 0U) {
            buf[--i] = (char)('0' + (val % 10U));
            val /= 10U;
        }
    }
    uart_puts(&buf[i]);
}

static void uart_put_ip(const uint8 *ip)
{
    uart_put_dec(ip[0]);
    uart_puts(".");
    uart_put_dec(ip[1]);
    uart_puts(".");
    uart_put_dec(ip[2]);
    uart_puts(".");
    uart_put_dec(ip[3]);
}

/* ================================================================
 * Byte-order helpers (TMS570 is big-endian, network is big-endian)
 * ================================================================ */

static uint16 read_u16(const volatile uint8 *p)
{
    return (uint16)(((uint16)p[0] << 8U) | (uint16)p[1]);
}

static void write_u16(uint8 *p, uint16 val)
{
    p[0] = (uint8)(val >> 8U);
    p[1] = (uint8)(val & 0xFFU);
}

static uint32 read_u32(const volatile uint8 *p)
{
    return ((uint32)p[0] << 24U) | ((uint32)p[1] << 16U) |
           ((uint32)p[2] << 8U)  | (uint32)p[3];
}

/* ================================================================
 * ICMP checksum (RFC 1071)
 * ================================================================ */

static uint16 icmp_checksum(const uint8 *data, uint32 len)
{
    uint32 sum = 0U;
    uint32 i;

    for (i = 0U; i < (len & ~1U); i += 2U) {
        sum += (uint32)read_u16((const volatile uint8 *)&data[i]);
    }
    if ((len & 1U) != 0U) {
        sum += (uint32)data[len - 1U] << 8U;
    }
    while ((sum >> 16U) != 0U) {
        sum = (sum & 0xFFFFU) + (sum >> 16U);
    }
    return (uint16)(~sum);
}

/* ================================================================
 * IP header checksum
 * ================================================================ */

static uint16 ip_checksum(const uint8 *hdr, uint32 hdr_len)
{
    return icmp_checksum(hdr, hdr_len);
}

/* ================================================================
 * Transmit a raw Ethernet frame
 * ================================================================ */

static void eth_transmit(uint8 *frame, uint32 len)
{
    pbuf_t pbuf;

    if (len < MIN_PKT_LEN + 14U) {
        /* Pad to minimum Ethernet frame size */
        uint32 i;
        for (i = len; i < (MIN_PKT_LEN + 14U); i++) {
            frame[i] = 0U;
        }
        len = MIN_PKT_LEN + 14U;
    }

    pbuf.payload = frame;
    pbuf.len = (uint16)len;
    pbuf.tot_len = (uint16)len;
    pbuf.next = NULL;

    (void)EMACTransmit(g_hdkif, &pbuf);
    g_tx_count++;
}

/* ================================================================
 * Handle ARP request → send ARP reply
 * ================================================================ */

static void handle_arp(const volatile uint8 *pkt, uint32 len)
{
    /* ARP starts at offset 14 (after Ethernet header) */
    const volatile uint8 *arp = &pkt[14];
    uint16 op;
    const volatile uint8 *target_ip;

    if (len < 42U) {
        return; /* Too short for ARP */
    }

    op = read_u16(&arp[6]);
    if (op != ARP_OP_REQUEST) {
        return;
    }

    /* Check if target IP matches ours */
    target_ip = &arp[24];
    if ((target_ip[0] != g_my_ip[0]) || (target_ip[1] != g_my_ip[1]) ||
        (target_ip[2] != g_my_ip[2]) || (target_ip[3] != g_my_ip[3])) {
        return;
    }

    g_arp_count++;
    uart_puts("[ARP] who-has ");
    uart_put_ip(g_my_ip);
    uart_puts(" from ");
    uart_put_ip((const uint8 *)&arp[14]);
    uart_puts("\r\n");

    /* Build ARP reply */
    /* Ethernet header */
    uint32 i;
    for (i = 0U; i < 6U; i++) {
        g_tx_buf[i] = pkt[6U + i];        /* dst = sender's MAC */
        g_tx_buf[6U + i] = g_my_mac[i];   /* src = our MAC */
    }
    write_u16(&g_tx_buf[12], ETH_TYPE_ARP);

    /* ARP payload */
    g_tx_buf[14] = 0x00U; g_tx_buf[15] = 0x01U; /* HW type: Ethernet */
    g_tx_buf[16] = 0x08U; g_tx_buf[17] = 0x00U; /* Proto: IPv4 */
    g_tx_buf[18] = 6U;                            /* HW addr len */
    g_tx_buf[19] = 4U;                            /* Proto addr len */
    write_u16(&g_tx_buf[20], ARP_OP_REPLY);

    /* Sender: us */
    for (i = 0U; i < 6U; i++) {
        g_tx_buf[22U + i] = g_my_mac[i];
    }
    for (i = 0U; i < 4U; i++) {
        g_tx_buf[28U + i] = g_my_ip[i];
    }

    /* Target: original sender */
    for (i = 0U; i < 6U; i++) {
        g_tx_buf[32U + i] = pkt[6U + i];   /* sender's MAC */
    }
    for (i = 0U; i < 4U; i++) {
        g_tx_buf[38U + i] = arp[14U + i];  /* sender's IP */
    }

    eth_transmit(g_tx_buf, 42U);
    uart_puts("[ARP] reply sent\r\n");
}

/* ================================================================
 * Handle ICMP Echo Request → send Echo Reply
 * ================================================================ */

static void handle_icmp(const volatile uint8 *pkt, uint32 len)
{
    /* IP header at offset 14 */
    const volatile uint8 *ip_hdr = &pkt[14];
    uint32 ip_hdr_len;
    uint32 ip_total_len;
    const volatile uint8 *icmp_hdr;
    uint32 icmp_len;
    uint32 i;

    if (len < 34U) {
        return; /* Too short for IP + ICMP */
    }

    /* Check IPv4 */
    if ((ip_hdr[0] & 0xF0U) != 0x40U) {
        return;
    }

    ip_hdr_len = (uint32)(ip_hdr[0] & 0x0FU) * 4U;
    ip_total_len = (uint32)read_u16(&ip_hdr[2]);

    /* Check protocol = ICMP */
    if (ip_hdr[9] != IP_PROTO_ICMP) {
        return;
    }

    /* Check destination IP = ours */
    if ((ip_hdr[16] != g_my_ip[0]) || (ip_hdr[17] != g_my_ip[1]) ||
        (ip_hdr[18] != g_my_ip[2]) || (ip_hdr[19] != g_my_ip[3])) {
        return;
    }

    icmp_hdr = &ip_hdr[ip_hdr_len];
    icmp_len = ip_total_len - ip_hdr_len;

    /* Check ICMP Echo Request */
    if (icmp_hdr[0] != ICMP_ECHO_REQ) {
        return;
    }

    g_icmp_count++;
    uart_puts("[ICMP] echo request from ");
    uart_put_ip((const uint8 *)&ip_hdr[12]);
    uart_puts(" seq=");
    uart_put_dec((uint32)read_u16(&icmp_hdr[6]));
    uart_puts("\r\n");

    /* Build ICMP Echo Reply — copy entire packet and modify in place */
    uint32 total_len = 14U + ip_total_len;
    if (total_len > sizeof(g_tx_buf)) {
        return;
    }

    for (i = 0U; i < total_len; i++) {
        g_tx_buf[i] = pkt[i];
    }

    /* Ethernet: swap src/dst MAC */
    for (i = 0U; i < 6U; i++) {
        g_tx_buf[i] = pkt[6U + i];
        g_tx_buf[6U + i] = g_my_mac[i];
    }

    /* IP: swap src/dst IP */
    uint8 *tx_ip = &g_tx_buf[14];
    for (i = 0U; i < 4U; i++) {
        tx_ip[16U + i] = g_my_ip[i];         /* src = us */
        tx_ip[12U + i] = ip_hdr[12U + i];    /* dst = sender (already there from copy) */
    }

    /* IP: recalculate header checksum */
    tx_ip[10] = 0U;
    tx_ip[11] = 0U;
    uint16 ip_cksum = ip_checksum(tx_ip, ip_hdr_len);
    write_u16(&tx_ip[10], ip_cksum);

    /* ICMP: change type to Echo Reply, recalculate checksum */
    uint8 *tx_icmp = &tx_ip[ip_hdr_len];
    tx_icmp[0] = ICMP_ECHO_REPLY;
    tx_icmp[2] = 0U;
    tx_icmp[3] = 0U;
    uint16 icmp_cksum = icmp_checksum(tx_icmp, icmp_len);
    write_u16(&tx_icmp[2], icmp_cksum);

    eth_transmit(g_tx_buf, total_len);
    uart_puts("[ICMP] echo reply sent\r\n");
}

/* ================================================================
 * Process one received Ethernet frame
 * ================================================================ */

static void process_packet(const volatile uint8 *pkt, uint32 len)
{
    uint16 eth_type;

    if (len < 14U) {
        return;
    }

    eth_type = read_u16(&pkt[12]);

    switch (eth_type) {
        case ETH_TYPE_ARP:
            handle_arp(pkt, len);
            break;
        case ETH_TYPE_IP:
            handle_icmp(pkt, len);
            break;
        default:
            /* Ignore other protocols */
            break;
    }
}

/* ================================================================
 * EMAC Notification Callbacks (override WEAK symbols in HALCoGen)
 * ================================================================ */

void emacRxNotification(hdkif_t *hdkif)
{
    rxch_t *rxch = &(hdkif->rxchptr);
    volatile emac_rx_bd_t *bd = rxch->free_head;

    if (bd != NULL) {
        uint32 pkt_len = EMACSwizzleData(bd->flags_pktlen) & 0xFFFFU;
        uint32 buf_ptr = EMACSwizzleData(bd->bufptr);

        if ((pkt_len > 0U) && (pkt_len <= sizeof(g_rx_buf))) {
            /* Copy packet data from DMA buffer */
            uint32 i;
            const volatile uint8 *src = (const volatile uint8 *)buf_ptr;
            for (i = 0U; i < pkt_len; i++) {
                g_rx_buf[i] = src[i];
            }
            g_rx_len = pkt_len;
            g_rx_ready = 1U;
            g_rx_count++;
        }
    }
}

void emacTxNotification(hdkif_t *hdkif)
{
    /* Nothing to do — TX complete */
    (void)hdkif;
}

/* ================================================================
 * ECLK — 25 MHz reference clock for DP83630 PHY
 * ================================================================ */

#include "HL_reg_system.h"

static void eclk_25mhz(void)
{
    /* SYSPC1 = 1: ECLK pin in ECLK function mode (not GIO) */
    systemREG1->SYSPC1 = 1U;

    /* ECPCNTL: divider=3 (VCLK1=75MHz / 3 = 25MHz), continue on suspend
     * Bit 23: ECPCOS = 1 (continue clock during debug suspend)
     * Bits 15:0: ECPDIV = 3-1 = 2 */
    systemREG1->ECPCNTL = ((uint32)1U << 23U)
                        | ((uint32)(3U - 1U) & 0xFFFFU);
}

/* ================================================================
 * PINMUX fix — route MDIO and MDCLK to balls G3 and V5
 *
 * HALCoGen sets G3=MIBSPI1NCS_2 and V5=MIBSPI3NCS_1, but these
 * balls must be MDIO and MDCLK for the EMAC to talk to the PHY.
 * Without this, EMACHWInit returns EMAC_ERR_CONNECT because MDIO
 * bus has no output path.
 * ================================================================ */

#include "HL_pinmux.h"

static void fix_mdio_pinmux(void)
{
    /* MDIO (F4) and MDCLK (T9) are PRIMARY functions on those balls
     * and work by default — no override needed.
     *
     * However, HALCoGen's MII enable may have muxed G3/V5 to MDIO/MDCLK
     * as a side effect, fighting the primary balls. Restore them to
     * their defaults (MIBSPI chip selects) per the Hackster.io guide. */

    /* Restore balls that MII enable incorrectly overrides.
     * The key insight from Jan Cumps: after enabling MII in HALCoGen,
     * uncheck signals on A14, B4, B11, D19, E18, F3, G3, G19, H18,
     * H19, J18, J19, K19, N19, P1, R2, V5. Our HALCoGen project
     * already has most of these correct. Just verify G3 and V5
     * are NOT set to MDIO/MDCLK (they should be SPI chip selects). */

    /* Nothing to do — our HALCoGen already has G3=MIBSPI1NCS_2 and
     * V5=MIBSPI3NCS_1 (correct defaults). Primary balls F4/T9 handle
     * MDIO/MDCLK automatically when EMAC module is enabled. */
}

/* ================================================================
 * PHY Release — GIOA[3]=reset, GIOA[4]=powerdown, then 200ms wait
 * ================================================================ */

static void phy_release(void)
{
    volatile uint32 delay;

    /* 1. Fix MDIO/MDCLK pin muxing (HALCoGen has them as SPI chip selects) */
    fix_mdio_pinmux();

    /* 2. Enable 25 MHz ECLK output — PHY needs this as reference clock */
    eclk_25mhz();

    /* 3. Match Jan Cumps' HALCoGen config: both GIOA[3] and GIOA[4] = OUTPUT HIGH.
     *    gioInit() now sets them HIGH from boot (patched HL_gio.c).
     *    Just reinforce here and wait for PHY to stabilize. */
    gioPORTA->DIR |= (uint32)(1U << 3U) | (uint32)(1U << 4U);
    gioPORTA->DSET = (uint32)(1U << 3U) | (uint32)(1U << 4U);

    /* 4. Wait 200ms for PHY PLL lock */
    for (delay = 0U; delay < 20000000U; delay++) {}
}

/* ================================================================
 * Entry Point
 * ================================================================ */

int main(void)
{
    uint32 emac_result;
    extern hdkif_t hdkif_data[MAX_EMAC_INSTANCE];

    /* 1. System init — PLL to 300 MHz */
    systemInit();
    gioInit();

    /* 2. UART for debug output */
    uart_init();
    uart_puts("\r\n=== TMS570 Ethernet Ping Test ===\r\n");

    /* 3. Release PHY from reset/powerdown */
    uart_puts("PHY: releasing reset... ");
    phy_release();
    uart_puts("done\r\n");

    /* 4. MDIO diagnostics — read registers before EMACHWInit */
    {
        volatile uint32 *mdio = (volatile uint32 *)0xFCF78900U;
        uart_puts("MDIO diag:\r\n");
        uart_puts("  REVID=0x"); uart_put_hex8((uint8)(mdio[0] >> 24U));
        uart_put_hex8((uint8)(mdio[0] >> 16U));
        uart_put_hex8((uint8)(mdio[0] >> 8U));
        uart_put_hex8((uint8)(mdio[0])); uart_puts("\r\n");

        /* Init MDIO manually: enable, preamble, clkdiv=0x1F */
        mdio[1] = 0x40000000U | 0x00100000U | 0x1FU;
        volatile uint32 w;
        for (w = 0U; w < 1000000U; w++) {}

        uart_puts("  CTRL=0x"); uart_put_hex8((uint8)(mdio[1] >> 24U));
        uart_put_hex8((uint8)(mdio[1] >> 16U));
        uart_put_hex8((uint8)(mdio[1] >> 8U));
        uart_put_hex8((uint8)(mdio[1])); uart_puts("\r\n");

        uart_puts("  ALIVE=0x"); uart_put_hex8((uint8)(mdio[2] >> 24U));
        uart_put_hex8((uint8)(mdio[2] >> 16U));
        uart_put_hex8((uint8)(mdio[2] >> 8U));
        uart_put_hex8((uint8)(mdio[2])); uart_puts("\r\n");

        uart_puts("  LINK=0x"); uart_put_hex8((uint8)(mdio[3] >> 24U));
        uart_put_hex8((uint8)(mdio[3] >> 16U));
        uart_put_hex8((uint8)(mdio[3] >> 8U));
        uart_put_hex8((uint8)(mdio[3])); uart_puts("\r\n");

        /* GIOA DOUT state */
        uart_puts("  GIOA_DIR=0x"); uart_put_hex8((uint8)(gioPORTA->DIR));
        uart_puts(" DOUT=0x"); uart_put_hex8((uint8)(gioPORTA->DOUT));
        uart_puts("\r\n");

        /* ECLK state */
        uart_puts("  SYSPC1=0x"); uart_put_hex8((uint8)(systemREG1->SYSPC1));
        uart_puts(" ECPCNTL=0x"); uart_put_hex8((uint8)(systemREG1->ECPCNTL >> 16U));
        uart_put_hex8((uint8)(systemREG1->ECPCNTL >> 8U));
        uart_put_hex8((uint8)(systemREG1->ECPCNTL));
        uart_puts("\r\n");
    }

    /* 5. Quick PHY check — do NOT toggle GPIOs before EMACHWInit */
    {
        volatile uint16 phyReg = 0U;
        MDIOPhyRegRead(MDIO_0_BASE, EMAC_PHYADDRESS, 2U, &phyReg);
        uart_puts("PHY ID1=0x");
        uart_put_hex8((uint8)(phyReg >> 8U));
        uart_put_hex8((uint8)(phyReg));
        MDIOPhyRegRead(MDIO_0_BASE, EMAC_PHYADDRESS, 0U, &phyReg);
        uart_puts(" BMCR=0x");
        uart_put_hex8((uint8)(phyReg >> 8U));
        uart_put_hex8((uint8)(phyReg));
        uart_puts(" PWRDN=");
        uart_puts((phyReg & 0x0800U) ? "YES" : "no");
        uart_puts("\r\n");
    }

    /* 6. Initialize EMAC + PHY */
    uart_puts("EMAC: init (MAC=");
    {
        uint32 i;
        for (i = 0U; i < 6U; i++) {
            if (i > 0U) uart_puts(":");
            uart_put_hex8(g_my_mac[i]);
        }
    }
    uart_puts(")... ");

    emac_result = EMACHWInit(g_my_mac);
    if (emac_result != EMAC_ERR_OK) {
        uart_puts("WARN (");
        uart_put_dec(emac_result);
        uart_puts(") — EMACHWInit returned error, but EMAC DMA is initialized.\r\n");

        /* Dump diagnostic registers */
        {
            volatile uint32 *emac = (volatile uint32 *)0xFCF78000U;
            uart_puts("  MACCONTROL=0x");
            uart_put_hex8((uint8)(emac[0x104U/4U] >> 24U));
            uart_put_hex8((uint8)(emac[0x104U/4U] >> 16U));
            uart_put_hex8((uint8)(emac[0x104U/4U] >> 8U));
            uart_put_hex8((uint8)(emac[0x104U/4U]));

            volatile uint16 bmcr = 0U;
            MDIOPhyRegRead(MDIO_0_BASE, EMAC_PHYADDRESS, 0U, &bmcr);
            uart_puts(" BMCR=0x");
            uart_put_hex8((uint8)(bmcr >> 8U));
            uart_put_hex8((uint8)(bmcr));
            uart_puts(" (pwrdn=");
            uart_puts((bmcr & 0x0800U) ? "YES" : "no");
            uart_puts(")\r\n");

            /* If PWRDN still stuck, try one last clear and force-continue */
            if (bmcr & 0x0800U) {
                uart_puts("  Force-clearing PWRDN post-EMACHWInit...\r\n");
                MDIOPhyRegWrite(MDIO_0_BASE, EMAC_PHYADDRESS, 0U,
                                (uint16)(bmcr & ~0x0800U));
                volatile uint32 d;
                for (d = 0U; d < 3000000U; d++) {}
                MDIOPhyRegRead(MDIO_0_BASE, EMAC_PHYADDRESS, 0U, &bmcr);
                uart_puts("  BMCR now=0x");
                uart_put_hex8((uint8)(bmcr >> 8U));
                uart_put_hex8((uint8)(bmcr));
                uart_puts("\r\n");
            }
        }
        /* NOTE: GIOA[3] = DP83630 pin 7 = IO_VDD (power supply pin!)
         * Do NOT drive LOW — it kills the PHY. Keep HIGH always.
         * PWRDN is controlled by Energy Detect (EDCR 0x1E = 0x3F80).
         * ED auto-powers-down when no cable connected.
         * Connect Ethernet cable and the PHY should wake up. */
        uart_puts("EDCR=0x");
        {
            volatile uint16 edcr = 0U;
            MDIOPhyRegRead(MDIO_0_BASE, EMAC_PHYADDRESS, 0x1EU, &edcr);
            uart_put_hex8((uint8)(edcr >> 8U));
            uart_put_hex8((uint8)(edcr));
            uart_puts(" (ED_EN=");
            uart_puts(((edcr >> 8U) & 0x03U) ? "ON" : "off");
            uart_puts(" ED_PWR=");
            uart_puts((edcr & 0x0080U) ? "PWRDN" : "normal");
            uart_puts(")\r\n");
        }
        uart_puts(">>> CONNECT ETHERNET CABLE — ED will auto-wake PHY <<<\r\n");
        uart_puts("Continuing — will poll for link in main loop.\r\n");
    } else {
        uart_puts("OK\r\n");
    }

    /* 5. Enable RX/TX interrupts */
    g_hdkif = &hdkif_data[0U];
    EMACTxIntPulseEnable(g_hdkif->emac_base, g_hdkif->emac_ctrl_base,
                         0U, EMAC_CHANNELNUMBER);
    EMACRxIntPulseEnable(g_hdkif->emac_base, g_hdkif->emac_ctrl_base,
                         0U, EMAC_CHANNELNUMBER);

    uart_puts("IP:   ");
    uart_put_ip(g_my_ip);
    uart_puts("\r\n");
    uart_puts("Ready — ping ");
    uart_put_ip(g_my_ip);
    uart_puts(" from PC\r\n\r\n");

    /* Turn on GIOB[6:7] LEDs = firmware running */
    gioPORTB->DIR |= (uint32)(1U << 6U) | (uint32)(1U << 7U);
    gioPORTB->DSET = (uint32)(1U << 6U) | (uint32)(1U << 7U);

    /* 6. Main loop — poll for received packets */
    {
        volatile uint32 heartbeat = 0U;
        for (;;) {
            if (g_rx_ready != 0U) {
                process_packet(g_rx_buf, g_rx_len);
                g_rx_ready = 0U;
            }

            /* Call EMACReceive to process RX descriptors (polled mode) */
            EMACReceive(g_hdkif);

            /* Periodic heartbeat every ~5 seconds */
            heartbeat++;
            if (heartbeat >= 50000000U) {
                heartbeat = 0U;
                uart_puts("[HB] rx=");
                uart_put_dec(g_rx_count);
                uart_puts(" tx=");
                uart_put_dec(g_tx_count);
                uart_puts(" arp=");
                uart_put_dec(g_arp_count);
                uart_puts(" icmp=");
                uart_put_dec(g_icmp_count);

                /* Check link status */
                volatile uint16 bsr = 0U;
                MDIOPhyRegRead(MDIO_0_BASE, EMAC_PHYADDRESS, 1U, &bsr);
                uart_puts(" link=");
                uart_puts((bsr & 0x0004U) ? "UP" : "DOWN");
                uart_puts("\r\n");
            }
        }
    }

    return 0;
}
