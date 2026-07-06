/**
 * @file    sc_eth.c
 * @brief   QM Ethernet bench driver for TMS570 SC instrumentation builds
 * @date    2026-07-06
 *
 * @details Extracted from the proven standalone TMS570 Ethernet ping test.
 *          The entire implementation is compiled only when SC_ETH_ENABLE is
 *          defined so default SC safety builds contain no Ethernet path.
 *
 * @verifies S-ETH-01
 * @note    Safety level: QM instrumentation only; excluded from safety builds.
 * @standard MISRA C:2012
 * @copyright Taktflow Systems 2026
 */
#include "sc_eth.h"

#ifdef SC_ETH_ENABLE

#ifndef UNIT_TEST
#define MAX_EMAC_INSTANCE       1u
#define EMAC_CHANNELNUMBER      0u
#define EMAC_PHYADDRESS         1u
#define MDIO_0_BASE             0xFCF78900u
#define EMAC_ERR_OK             0x1u
#define EMAC_BUF_DESC_OWNER     0x20000000u
#define EMAC_BUF_DESC_SOP       0x80000000u
#define EMAC_BUF_DESC_EOP       0x40000000u

typedef struct emac_tx_bd {
    volatile struct emac_tx_bd *next;
    volatile uint32 bufptr;
    volatile uint32 bufoff_len;
    volatile uint32 flags_pktlen;
} emac_tx_bd_t;

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
    volatile emac_tx_bd_t *free_head;
    volatile emac_tx_bd_t *active_tail;
    volatile emac_tx_bd_t *next_bd_to_process;
} txch_t;

typedef struct hdkif_struct {
    uint8 mac_addr[SC_ETH_MAC_ADDR_LEN];
    uint32 emac_base;
    volatile uint32 emac_ctrl_base;
    volatile uint32 emac_ctrl_ram;
    volatile uint32 mdio_base;
    uint32 phy_addr;
    boolean (*phy_autoneg)(uint32 param1, uint32 param2, uint16 param3);
    boolean (*phy_partnerability)(uint32 param4, uint32 param5, uint16 *param6);
    txch_t txchptr;
    rxch_t rxchptr;
} hdkif_t;

typedef struct pbuf_struct {
    struct pbuf_struct *next;
    uint8 *payload;
    uint16 tot_len;
    uint16 len;
} pbuf_t;

typedef volatile struct gioPort {
    uint32 DIR;
    uint32 DIN;
    uint32 DOUT;
    uint32 DSET;
    uint32 DCLR;
    uint32 PDR;
    uint32 PULDIS;
    uint32 PSL;
} gioPORT_t;

typedef volatile struct systemBase1 {
    uint32 SYSPC1;
    uint32 rsvd0[52u];
    uint32 ECPCNTL;
} systemBASE1_t;

#define gioPORTA   ((gioPORT_t *)0xFFF7BC34u)
#define systemREG1 ((systemBASE1_t *)0xFFFFFF00u)

extern uint32 EMACSwizzleData(uint32 word);
extern uint32 EMACHWInit(uint8 macaddr[SC_ETH_MAC_ADDR_LEN]);
extern boolean EMACTransmit(hdkif_t *hdkif, pbuf_t *pbuf);
extern void EMACReceive(hdkif_t *hdkif);
extern void EMACTxIntPulseEnable(uint32 emacBase, uint32 emacCtrlBase,
                                 uint32 core, uint32 channel);
extern void EMACRxIntPulseEnable(uint32 emacBase, uint32 emacCtrlBase,
                                 uint32 core, uint32 channel);
extern boolean MDIOPhyRegRead(uint32 baseAddr, uint32 phyAddr,
                              uint32 regNum, volatile uint16 *dataPtr);
extern void MDIOPhyRegWrite(uint32 baseAddr, uint32 phyAddr,
                            uint32 regNum, uint16 regVal);
#endif

/* ==================================================================
 * Ethernet/PHY constants
 * ================================================================== */

#define SC_ETH_DESC_LENGTH_MASK     0x0000FFFFu
#define SC_ETH_PHY_BMCR             0x00u
#define SC_ETH_PHY_BSR              0x01u
#define SC_ETH_PHY_MICR             0x11u
#define SC_ETH_PHY_BSR_LINK_UP      0x0004u
#define SC_ETH_PHY_BMCR_POWER_DOWN  0x0800u
#define SC_ETH_PHY_BMCR_RESTART_AN  0x0200u
#define SC_ETH_PHY_MICR_INT_OE      0x0001u
#define SC_ETH_LINK_WAIT_TRIES      80u
#define SC_ETH_LINK_WAIT_DELAY      10000000u
#define SC_ETH_PHY_WAKE_DELAY       3000000u
#define SC_ETH_PHY_PLL_DELAY        20000000u
#define SC_ETH_ECLK_DIVIDER_FIELD   2u
#define SC_ETH_CACHE_LINE_BYTES     32u
#define SC_ETH_GIOA_PHY_MASK        (((uint32)1u << 3u) | ((uint32)1u << 4u))
#define SC_ETH_MDIO_CONTROL_WORD    1u
#define SC_ETH_MDIO_CONTROL_INIT    0x4010001Fu
#define SC_ETH_MDIO_INIT_DELAY      1000000u

/* ==================================================================
 * Module state
 * ================================================================== */

static hdkif_t *g_sc_eth_hdkif = NULL_PTR;
static boolean g_sc_eth_initialized = FALSE;

#ifndef UNIT_TEST
extern hdkif_t hdkif_data[MAX_EMAC_INSTANCE];
#endif

/* ==================================================================
 * Static prototypes
 * ================================================================== */

static boolean sc_eth_desc_complete(uint32 flags);
static boolean sc_eth_desc_ready(uint32 flags, uint32 buf_ptr,
                                 uint16 dst_len, uint16 *pkt_len);
static boolean sc_eth_wait_link(uint32 tries);
static void sc_eth_init_mdio_manual(void);
static void sc_eth_wake_phy(void);
static void sc_eth_enable_eclk_25mhz(void);
static void sc_eth_release_phy_pins(void);
static void sc_eth_delay(uint32 loops);
static void sc_eth_cache_cinv_range(uint32 start, uint32 len);
static void sc_eth_cache_clean_range(uint32 start, uint32 len);

#ifdef UNIT_TEST
extern const uint8 *Sc_Eth_Test_ResolveRxBuffer(uint32 bufPtr);
extern void Sc_Eth_Test_CacheClean(uint32 start, uint32 len);
extern void Sc_Eth_Test_CacheCleanInvalidate(uint32 start, uint32 len);
extern void Sc_Eth_Test_InitMdioManual(void);
extern void Sc_Eth_Test_EnableEclk25MHz(void);
extern void Sc_Eth_Test_ReleasePhyPins(void);
extern void Sc_Eth_Test_Delay(uint32 loops);
#endif

/* ==================================================================
 * Public API
 * ================================================================== */

Std_ReturnType Sc_Eth_Init(const uint8 *mac)
{
    uint8 mac_copy[SC_ETH_MAC_ADDR_LEN];
    uint32 i;
    uint32 emac_result;
    boolean link_seen;

    if (mac == NULL_PTR) {
        return E_NOT_OK;
    }

    for (i = 0u; i < SC_ETH_MAC_ADDR_LEN; i++) {
        mac_copy[i] = mac[i];
    }

    sc_eth_enable_eclk_25mhz();
    sc_eth_release_phy_pins();
    sc_eth_delay(SC_ETH_PHY_PLL_DELAY);
    sc_eth_init_mdio_manual();
    sc_eth_wake_phy();
    link_seen = sc_eth_wait_link(SC_ETH_LINK_WAIT_TRIES);

    emac_result = EMACHWInit(mac_copy);
    g_sc_eth_hdkif = &hdkif_data[0u];
    g_sc_eth_initialized = TRUE;

    EMACTxIntPulseEnable(g_sc_eth_hdkif->emac_base,
                         g_sc_eth_hdkif->emac_ctrl_base,
                         0u,
                         EMAC_CHANNELNUMBER);
    EMACRxIntPulseEnable(g_sc_eth_hdkif->emac_base,
                         g_sc_eth_hdkif->emac_ctrl_base,
                         0u,
                         EMAC_CHANNELNUMBER);

    return ((emac_result == EMAC_ERR_OK) && (link_seen == TRUE)) ? E_OK : E_NOT_OK;
}

boolean Sc_Eth_PollRx(uint8 *frame, uint16 frame_size, uint16 *frame_len)
{
    rxch_t *rxch;
    volatile emac_rx_bd_t *bd;
    uint32 flags;
    uint32 buf_ptr;
    uint16 pkt_len;
    boolean ready;
    boolean complete;

    if ((g_sc_eth_initialized == FALSE) || (g_sc_eth_hdkif == NULL_PTR) ||
        (frame == NULL_PTR) || (frame_len == NULL_PTR)) {
        return FALSE;
    }

    rxch = &(g_sc_eth_hdkif->rxchptr);
    bd = rxch->active_head;
    if (bd == NULL_PTR) {
        return FALSE;
    }

    flags = EMACSwizzleData(bd->flags_pktlen);
    buf_ptr = EMACSwizzleData(bd->bufptr);
    complete = sc_eth_desc_complete(flags);
    ready = sc_eth_desc_ready(flags, buf_ptr, frame_size, &pkt_len);
    *frame_len = pkt_len;

    if (ready == TRUE) {
        uint16 i;
        const volatile uint8 *src;

        sc_eth_cache_cinv_range(buf_ptr, (uint32)pkt_len);
#ifdef UNIT_TEST
        src = Sc_Eth_Test_ResolveRxBuffer(buf_ptr);
#else
        src = (const volatile uint8 *)buf_ptr;
#endif
        if (src != NULL_PTR) {
            for (i = 0u; i < pkt_len; i++) {
                frame[i] = src[i];
            }
        } else {
            ready = FALSE;
        }
    }

    if (complete == TRUE) {
        EMACReceive(g_sc_eth_hdkif);
    }

    return ready;
}

Std_ReturnType Sc_Eth_Tx(uint8 *frame, uint16 frame_len)
{
    pbuf_t pbuf;
    uint16 tx_len;

    if ((g_sc_eth_initialized == FALSE) || (g_sc_eth_hdkif == NULL_PTR) ||
        (frame == NULL_PTR) || (frame_len == 0u) ||
        (frame_len > SC_ETH_FRAME_MAX_LEN)) {
        return E_NOT_OK;
    }

    tx_len = frame_len;
    if (tx_len < SC_ETH_MIN_TX_FRAME_LEN) {
        uint16 i;

        for (i = tx_len; i < SC_ETH_MIN_TX_FRAME_LEN; i++) {
            frame[i] = 0u;
        }
        tx_len = SC_ETH_MIN_TX_FRAME_LEN;
    }

    pbuf.payload = frame;
    pbuf.len = tx_len;
    pbuf.tot_len = tx_len;
    pbuf.next = NULL_PTR;

#ifdef UNIT_TEST
    sc_eth_cache_clean_range(0u, (uint32)tx_len);
#else
    sc_eth_cache_clean_range((uint32)frame, (uint32)tx_len);
#endif

    return (EMACTransmit(g_sc_eth_hdkif, &pbuf) == TRUE) ? E_OK : E_NOT_OK;
}

boolean Sc_Eth_LinkUp(void)
{
    volatile uint16 bsr = 0u;

    (void)MDIOPhyRegRead(MDIO_0_BASE, EMAC_PHYADDRESS, SC_ETH_PHY_BSR, &bsr);

    return ((bsr & SC_ETH_PHY_BSR_LINK_UP) != 0u) ? TRUE : FALSE;
}

/* ==================================================================
 * Static helpers
 * ================================================================== */

static boolean sc_eth_desc_complete(uint32 flags)
{
    return (((flags & EMAC_BUF_DESC_SOP) != 0u) &&
            ((flags & EMAC_BUF_DESC_EOP) != 0u) &&
            ((flags & EMAC_BUF_DESC_OWNER) == 0u)) ? TRUE : FALSE;
}

static boolean sc_eth_desc_ready(uint32 flags, uint32 buf_ptr,
                                 uint16 dst_len, uint16 *pkt_len)
{
    uint32 length = flags & SC_ETH_DESC_LENGTH_MASK;

    if (pkt_len != NULL_PTR) {
        *pkt_len = (uint16)length;
    }

    if (sc_eth_desc_complete(flags) == FALSE) {
        return FALSE;
    }
    if ((length == 0u) || (length > (uint32)SC_ETH_FRAME_MAX_LEN) ||
        (length > (uint32)dst_len) || (buf_ptr == 0u)) {
        return FALSE;
    }

    return TRUE;
}

static boolean sc_eth_wait_link(uint32 tries)
{
    uint32 i;

    for (i = 0u; i < tries; i++) {
        if (Sc_Eth_LinkUp() == TRUE) {
            return TRUE;
        }
        sc_eth_delay(SC_ETH_LINK_WAIT_DELAY);
    }

    return FALSE;
}

static void sc_eth_init_mdio_manual(void)
{
#ifdef UNIT_TEST
    Sc_Eth_Test_InitMdioManual();
#else
    volatile uint32 *mdio = (volatile uint32 *)MDIO_0_BASE;

    mdio[SC_ETH_MDIO_CONTROL_WORD] = SC_ETH_MDIO_CONTROL_INIT;
#endif
    sc_eth_delay(SC_ETH_MDIO_INIT_DELAY);
}

static void sc_eth_wake_phy(void)
{
    volatile uint16 reg = 0u;

    MDIOPhyRegWrite(MDIO_0_BASE, EMAC_PHYADDRESS,
                    SC_ETH_PHY_MICR, SC_ETH_PHY_MICR_INT_OE);
    (void)MDIOPhyRegRead(MDIO_0_BASE, EMAC_PHYADDRESS, SC_ETH_PHY_MICR, &reg);

    (void)MDIOPhyRegRead(MDIO_0_BASE, EMAC_PHYADDRESS, SC_ETH_PHY_BMCR, &reg);
    MDIOPhyRegWrite(MDIO_0_BASE,
                    EMAC_PHYADDRESS,
                    SC_ETH_PHY_BMCR,
                    (uint16)((reg & (uint16)(~SC_ETH_PHY_BMCR_POWER_DOWN)) |
                             SC_ETH_PHY_BMCR_RESTART_AN));
    sc_eth_delay(SC_ETH_PHY_WAKE_DELAY);
}

static void sc_eth_enable_eclk_25mhz(void)
{
#ifdef UNIT_TEST
    Sc_Eth_Test_EnableEclk25MHz();
#else
    systemREG1->SYSPC1 = 1u;
    systemREG1->ECPCNTL = ((uint32)1u << 23u)
                        | SC_ETH_ECLK_DIVIDER_FIELD;
#endif
}

static void sc_eth_release_phy_pins(void)
{
#ifdef UNIT_TEST
    Sc_Eth_Test_ReleasePhyPins();
#else
    gioPORTA->DIR |= SC_ETH_GIOA_PHY_MASK;
    gioPORTA->DSET = SC_ETH_GIOA_PHY_MASK;
#endif
}

static void sc_eth_delay(uint32 loops)
{
#ifdef UNIT_TEST
    Sc_Eth_Test_Delay(loops);
#else
    volatile uint32 i;

    for (i = 0u; i < loops; i++) {
        /* Busy wait for PHY stabilization on the standalone TMS570 target. */
    }
#endif
}

static void sc_eth_cache_cinv_range(uint32 start, uint32 len)
{
#ifdef UNIT_TEST
    Sc_Eth_Test_CacheCleanInvalidate(start, len);
#else
    uint32 addr = start & ~(SC_ETH_CACHE_LINE_BYTES - 1u);
    uint32 end = start + len;

    while (addr < end) {
        __asm__ volatile("mcr p15, #0, %0, c7, c14, #1" : : "r"(addr));
        addr += SC_ETH_CACHE_LINE_BYTES;
    }
    __asm__ volatile("dsb" : : : "memory");
#endif
}

static void sc_eth_cache_clean_range(uint32 start, uint32 len)
{
#ifdef UNIT_TEST
    Sc_Eth_Test_CacheClean(start, len);
#else
    uint32 addr = start & ~(SC_ETH_CACHE_LINE_BYTES - 1u);
    uint32 end = start + len;

    while (addr < end) {
        __asm__ volatile("mcr p15, #0, %0, c7, c10, #1" : : "r"(addr));
        addr += SC_ETH_CACHE_LINE_BYTES;
    }
    __asm__ volatile("dsb" : : : "memory");
#endif
}

#endif /* SC_ETH_ENABLE */
