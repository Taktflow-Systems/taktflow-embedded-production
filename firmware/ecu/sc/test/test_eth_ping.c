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
 *          Test:   ping 192.168.1.200
 *
 * @note    GIOA[3] (ball E1) = DP83630 PWRDOWN/INTN pin 7, ACTIVE LOW, with
 *          2.2k pulldown RP11B on board (schematic SPRR397 sheet 12) — the
 *          PHY boots held in power-down until GIOA[3] drives HIGH, and
 *          BMCR bit 11 is OR'd/latched with this pin. MICR.INT_OE (reg
 *          0x11 bit 0) disables the pin's power-down function (DS 5.9.1).
 *          GIOA[4] (ball A6) = PHY RESET_N pin 29 (HIGH=release, LOW=reset)
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
#include "HL_sys_vim.h"
#include "sc_eth.h"

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

/* TMS570 static IP: 192.168.1.200 */
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

/* RX packet buffer — filled by emacRxNotification */
static uint8           g_rx_buf[1520];
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
    if ((len <= SC_ETH_FRAME_MAX_LEN) &&
        (Sc_Eth_Tx(frame, (uint16)len) == E_OK)) {
        g_tx_count++;
    }
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

    /* IP: swap src/dst IP (src @ offset 12, dst @ offset 16) */
    uint8 *tx_ip = &g_tx_buf[14];
    for (i = 0U; i < 4U; i++) {
        tx_ip[12U + i] = g_my_ip[i];         /* src = us */
        tx_ip[16U + i] = ip_hdr[12U + i];    /* dst = original sender */
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
 * Polled RX — this test never enables VIM/IRQs, so emacRxNotification
 * (ISR-driven) never fires. EMACReceive() alone only RECYCLES completed
 * descriptors — it does not deliver data. Poll the descriptor chain
 * ourselves: copy a completed frame out FIRST, then let EMACReceive
 * re-arm the chain.
 * ================================================================ */

static void eth_poll_rx(void)
{
    uint16 pkt_len = 0U;

    if (Sc_Eth_PollRx(g_rx_buf, (uint16)sizeof(g_rx_buf), &pkt_len) == TRUE) {
        g_rx_len = (uint32)pkt_len;
        g_rx_ready = 1U;
        g_rx_count++;
    }
}

/* ================================================================
 * Entry Point
 * ================================================================ */

int main(void)
{
    Std_ReturnType eth_status;

    /* 1. System init — PLL to 300 MHz */
    systemInit();
    gioInit();

    /* 2. UART for debug output */
    uart_init();
    uart_puts("\r\n=== TMS570 Ethernet Ping Test ===\r\n");

    /* 3. Initialize reusable Ethernet driver */
    uart_puts("Ethernet: init (MAC=");
    {
        uint32 i;
        for (i = 0U; i < 6U; i++) {
            if (i > 0U) {
                uart_puts(":");
            }
            uart_put_hex8(g_my_mac[i]);
        }
    }
    uart_puts(")... ");
    eth_status = Sc_Eth_Init(g_my_mac);
    if (eth_status == E_OK) {
        uart_puts("OK\r\n");
    } else {
        uart_puts("WARN - continuing with EMAC DMA initialized\r\n");
    }

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
            /* Poll RX descriptors — copies one completed frame into
             * g_rx_buf (sets g_rx_ready) and recycles the descriptors */
            eth_poll_rx();

            if (g_rx_ready != 0U) {
                process_packet(g_rx_buf, g_rx_len);
                g_rx_ready = 0U;
            }

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

                uart_puts(" link=");
                uart_puts((Sc_Eth_LinkUp() == TRUE) ? "UP" : "DOWN");
                uart_puts("\r\n");
            }
        }
    }

    return 0;
}
