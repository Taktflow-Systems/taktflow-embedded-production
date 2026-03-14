/**
 * @file    sc_hw_tms570.c
 * @brief   TMS570LC43x hardware implementation for SC (Safety Controller)
 * @date    2026-03-04
 *
 * @details Implements all hardware externs used across SC source files
 *          for the TMS570LC4357 LaunchPad target:
 *          - HALCoGen system/GIO/RTI wrappers (from sc_main.c)
 *          - DCAN1 register access for listen-only CAN (from sc_can.c)
 *          - Self-test hardware stubs (from sc_selftest.c)
 *          - ESM error signaling (from sc_esm.c)
 *          - SCI debug UART output (from sc_main.c)
 *
 *          The SC is bare-metal — no AUTOSAR BSW. Uses sc_types.h
 *          for type definitions. All HALCoGen-generated code is in
 *          firmware/sc/cfg/halcogen/source/ and linked at build time.
 *
 * @safety_req SWR-SC-001 to SWR-SC-026
 * @traces_to  SSR-SC-001 to SSR-SC-017
 * @note    Safety level: ASIL D (self-test stubs to be replaced with
 *          real implementations after CAN bring-up)
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#ifdef PLATFORM_TMS570

#include "sc_types.h"
#include "Sc_Hw_Cfg.h"
#include "sc_esm.h"       /* SC_ESM_HighLevelInterrupt — runtime safety response */


/* ==================================================================
 * TMS570LC43x Register Base Addresses
 * ================================================================== */

/** DCAN1 base address (TMS570LC43x TRM Table 2-2) */
#define DCAN1_BASE              0xFFF7DC00u

/** DCAN message object RAM base (TMS570LC43x, DCAN1) */
#define DCAN1_MSG_RAM_BASE      0xFFF7E000u

/** GIO base address */
#define GIO_BASE                0xFFF7BC00u

/** RTI base address */
#define RTI_BASE                0xFFFFFC00u

/** ESM base address */
#define ESM_BASE                0xFFFFF500u

/** SCI/LIN base address (SCI1 for debug UART via XDS110) */
#define SCI_BASE                0xFFF7E400u

/** IOMM (I/O Multiplexing Module) base address */
#define IOMM_BASE               0xFFFF1C00u
#define IOMM_KICKER0            0x38u   /* Kicker 0 (unlock) */
#define IOMM_KICKER1            0x3Cu   /* Kicker 1 (unlock) */
/* PINMUX[n] offset = 0x110 + n*4 */
#define IOMM_PINMUX83           0x25Cu  /* 0x110 + 83*4 — output mux for ball A5 */
#define IOMM_PINMUX84           0x260u  /* 0x110 + 84*4 — output mux for ball C2 */

/* ==================================================================
 * GIO Register Offsets (from GIO_BASE)
 * ================================================================== */

#define GIO_GCR0                0x00u   /* Global control */
#define GIO_POL                 0x0Cu   /* Interrupt polarity */
#define GIO_ENASET              0x10u   /* Interrupt enable set */
#define GIO_ENACLR              0x14u   /* Interrupt enable clear */
#define GIO_LVLSET              0x18u   /* Interrupt level set */
#define GIO_LVLCLR              0x1Cu   /* Interrupt level clear */
#define GIO_FLG                 0x20u   /* Interrupt flag (W1C) */
#define GIO_DIRA                0x34u   /* Port A direction */
#define GIO_DINA                0x38u   /* Port A data input */
#define GIO_DOUTA               0x3Cu   /* Port A data output */
#define GIO_DSETA               0x40u   /* Port A data set */
#define GIO_DCLRA               0x44u   /* Port A data clear */
#define GIO_PDRA                0x48u   /* Port A open drain */
#define GIO_PULDISA             0x4Cu   /* Port A pullup disable */
#define GIO_PSLA                0x50u   /* Port A pull up/down select */
#define GIO_DIRB                0x54u   /* Port B direction */
#define GIO_DINB                0x58u   /* Port B data input */
#define GIO_DOUTB               0x5Cu   /* Port B data output */
#define GIO_DSETB               0x60u   /* Port B data set */
#define GIO_DCLRB               0x64u   /* Port B data clear */
#define GIO_PDRB                0x68u   /* Port B open drain */
#define GIO_PULDISB             0x6Cu   /* Port B pullup disable */
#define GIO_PSLB                0x70u   /* Port B pull up/down select */

/* ==================================================================
 * RTI Register Offsets (from RTI_BASE)
 * ================================================================== */

#define RTI_GCTRL               0x00u   /* Global control */
#define RTI_TBCTRL              0x04u   /* Timebase control */
#define RTI_CAPCTRL             0x08u   /* Capture control */
#define RTI_COMPCTRL            0x0Cu   /* Compare control */
#define RTI_FRC0                0x10u   /* Free running counter 0 */
#define RTI_UC0                 0x14u   /* Up counter 0 */
#define RTI_CPUC0               0x18u   /* Compare up counter 0 (prescaler) */
#define RTI_FRC1                0x30u   /* Free running counter 1 */
#define RTI_UC1                 0x34u   /* Up counter 1 */
#define RTI_CPUC1               0x38u   /* Compare up counter 1 (prescaler) */
#define RTI_CMP0                0x50u   /* Compare 0 value */
#define RTI_UDCP0               0x54u   /* Update compare 0 */
#define RTI_CMP1                0x58u   /* Compare 1 value */
#define RTI_UDCP1               0x5Cu   /* Update compare 1 */
#define RTI_CMP2                0x60u   /* Compare 2 value */
#define RTI_UDCP2               0x64u   /* Update compare 2 */
#define RTI_CMP3                0x68u   /* Compare 3 value */
#define RTI_UDCP3               0x6Cu   /* Update compare 3 */
#define RTI_SETINTENA           0x80u   /* Set interrupt enable */
#define RTI_CLEARINTENA         0x84u   /* Clear interrupt enable */
#define RTI_INTFLAG             0x88u   /* Interrupt flag (W1C) */

/** RTI compare 0 interrupt flag bit */
#define RTI_CMP0_FLAG           0x01u

/* ==================================================================
 * ESM Register Offsets (from ESM_BASE)
 * ================================================================== */

#define ESM_EEPAPR1             0x00u   /* Group 1 enable set */
#define ESM_DEPAPR1             0x04u   /* Group 1 enable clear */
#define ESM_SR1                 0x18u   /* Group 1 status */
#define ESM_SR4                 0x24u   /* Group 1 status clear (write-1-to-clear) */
#define ESM_SR2                 0x1Cu   /* Group 2 status (read-only) */
/* Group 2 clear: write to ESM_SR2 (some TMS570 variants) or use EKR */

/* ==================================================================
 * SCI Register Offsets (from SCI_BASE)
 * ================================================================== */

#define SCI_GCR0                0x00u   /* Global control 0 */
#define SCI_GCR1                0x04u   /* Global control 1 */
#define SCI_FORMAT              0x28u   /* Format control (char length) */
#define SCI_BRS                 0x2Cu   /* Baud rate selection */
#define SCI_FLR                 0x1Cu   /* Flags register */
#define SCI_TD                  0x38u   /* Transmit data */
#define SCI_PIO0                0x3Cu   /* Pin function select (0=GIO, 1=SCI) */

/** SCI TX ready flag */
#define SCI_FLR_TXRDY           0x00000100u

/* ==================================================================
 * DCAN Message Object RAM Layout
 *
 * Each message object is 32 bytes (8 words) starting at DCAN1_MSG_RAM_BASE.
 * Message object N (1-indexed) starts at:
 *   DCAN1_MSG_RAM_BASE + ((N-1) * 0x20)
 *
 * Word offsets within a message object:
 *   0x00: IF1CMD / (unused in RAM view)
 *   0x04: IF1MSK / (unused in RAM view)
 *   0x08: IF1ARB — Arbitration (CAN ID in bits 28:18 for std)
 *   0x0C: IF1MCTL — Message control (DLC bits 3:0, NewDat bit 15)
 *   0x10: IF1DATA — Data bytes 0-3 (byte 0 in bits 7:0)
 *   0x14: IF1DATB — Data bytes 4-7 (byte 4 in bits 7:0)
 *
 * For DCAN hardware, we use Interface Registers (IF1/IF2) to access
 * message object RAM. Direct RAM access is also possible on TMS570.
 * ================================================================== */

/** GIOB LED bitmask for pins 6 and 7 (LaunchPad user LEDs) */
#define LED_MASK  (((uint32)1u << 6u) | ((uint32)1u << 7u))

/** DCAN control/status register offsets (from DCAN1_BASE) */
#define DCAN_CTL                0x00u   /* Control */
#define DCAN_ES                 0x04u   /* Error and Status */
#define DCAN_TEST               0x14u   /* Test mode */

/** DCAN CTL register bits */
#define DCAN_CTL_INIT           ((uint32)1u << 0u)   /* Init mode */
#define DCAN_CTL_CCE            ((uint32)1u << 6u)   /* Config Change Enable */
#define DCAN_CTL_TEST           ((uint32)1u << 7u)   /* Test mode enable */

/** DCAN TEST register bits */
#define DCAN_TEST_LBACK         ((uint32)1u << 4u)   /* Internal loopback */

/** DCAN MCTL TxRqst bit */
#define DCAN_MCTL_TXRQST        ((uint32)1u << 8u)

/** Self-test loopback mailboxes (unused in normal operation) */
#define DCAN_SELFTEST_TX_MB     8u
#define DCAN_SELFTEST_RX_MB     9u
#define DCAN_SELFTEST_CAN_ID    0x7FFu

/** DCAN IF1 command register offsets (from DCAN1_BASE) */
#define DCAN_IF1CMD             0x100u
#define DCAN_IF1MSK             0x104u
#define DCAN_IF1ARB             0x108u
#define DCAN_IF1MCTL            0x10Cu
#define DCAN_IF1DATA            0x110u
#define DCAN_IF1DATB            0x114u

/** DCAN IF2 command register offsets (from DCAN1_BASE) — used for RX */
#define DCAN_IF2CMD             0x120u
#define DCAN_IF2MSK             0x124u
#define DCAN_IF2ARB             0x128u
#define DCAN_IF2MCTL            0x12Cu
#define DCAN_IF2DATA            0x130u
#define DCAN_IF2DATB            0x134u

/** IF command register bits — TMS570 big-endian: command byte is bits [23:16]
 *  (MISRA 12.2: use uint32 literal for shift) */
#define DCAN_IFCMD_BUSY         ((uint32)1u << 15u)  /* IF busy flag */
#define DCAN_IFCMD_DATAB        ((uint32)1u << 16u)  /* Access data B */
#define DCAN_IFCMD_DATAA        ((uint32)1u << 17u)  /* Access data A */
#define DCAN_IFCMD_NEWDAT       ((uint32)1u << 18u)  /* Access NewDat/TxRqst */
#define DCAN_IFCMD_CLRINTPND    ((uint32)1u << 19u)  /* Clear IntPnd */
#define DCAN_IFCMD_CONTROL      ((uint32)1u << 20u)  /* Access control bits */
#define DCAN_IFCMD_ARB          ((uint32)1u << 21u)  /* Access arbitration */
#define DCAN_IFCMD_MASK         ((uint32)1u << 22u)  /* Access mask */
#define DCAN_IFCMD_WR           ((uint32)1u << 23u)  /* Write (1) / Read (0) */

/** MCTL NewDat bit */
#define DCAN_MCTL_NEWDAT        ((uint32)1u << 15u)

/** MCTL UMask bit — use acceptance mask for filtering */
#define DCAN_MCTL_UMASK         ((uint32)1u << 12u)

/** ARB MsgVal bit — message object is valid */
#define DCAN_ARB_MSGVAL         ((uint32)1u << 31u)

/** ARB Xtd bit — extended ID (0 = standard 11-bit) */
#define DCAN_ARB_XTD            ((uint32)1u << 30u)

/** ARB Dir bit — 0=receive, 1=transmit */
#define DCAN_ARB_DIR            ((uint32)1u << 29u)

/* ==================================================================
 * Helper: volatile register access
 * ================================================================== */

/* MISRA Rule 11.4 deviation: integer-to-pointer cast is required for
 * memory-mapped peripheral register access on bare-metal TMS570.
 * The addresses are fixed hardware register addresses from the TRM. */
static uint32 reg_read(uint32 base, uint32 offset)
{
    /* cppcheck-suppress misra-c2012-11.4 */
    volatile const uint32 *addr = (volatile const uint32 *)(base + offset);
    return *addr;
}

static void reg_write(uint32 base, uint32 offset, uint32 value)
{
    /* cppcheck-suppress misra-c2012-11.4 */
    volatile uint32 *addr = (volatile uint32 *)(base + offset);
    *addr = value;
}

/* ==================================================================
 * HALCoGen replacement functions
 *
 * systemInit() is provided by HALCoGen's system.c — not redefined here.
 *
 * gioInit(), rtiInit(), rtiStartCounter() are provided HERE instead of
 * HALCoGen HL_gio.c / HL_rti.c because the HALCoGen function signatures
 * for gioSetBit/gioSetDirection/rtiStartCounter conflict with the SC
 * platform-independent API (uint8 port,pin vs gioPORT_t pointer).
 *
 * These implementations replicate the exact register writes that
 * HALCoGen v04.07.01 generates for the SC .hcg project configuration.
 * ================================================================== */

/* systemInit() — provided by HALCoGen system.c, not defined here */

/**
 * @brief  Initialize GIO module (ports A and B)
 *
 * Replicates HALCoGen gioInit() register writes for the SC config:
 *   Port A: bits 0-5 output (relay, LEDs, WDI), bits 6-7 input
 *   Port B: bit 1 output (heartbeat LED), rest input
 *   All outputs LOW, no open drain, no interrupts
 */
void gioInit(void)
{
    /* Bring GIO out of reset */
    reg_write(GIO_BASE, GIO_GCR0, 1u);

    /* Disable all interrupts, clear levels */
    reg_write(GIO_BASE, GIO_ENACLR, 0xFFu);
    reg_write(GIO_BASE, GIO_LVLCLR, 0xFFu);

    /* Port A: output values = 0, direction bits 0-5 = output */
    reg_write(GIO_BASE, GIO_DOUTA, 0x00u);
    reg_write(GIO_BASE, GIO_DIRA, 0x3Fu);
    reg_write(GIO_BASE, GIO_PDRA, 0x00u);
    reg_write(GIO_BASE, GIO_PSLA, 0x00u);
    reg_write(GIO_BASE, GIO_PULDISA, 0x00u);

    /* Port B: output values = 0, direction bit 1 = output */
    reg_write(GIO_BASE, GIO_DOUTB, 0x00u);
    reg_write(GIO_BASE, GIO_DIRB, 0x02u);
    reg_write(GIO_BASE, GIO_PDRB, 0x00u);
    reg_write(GIO_BASE, GIO_PSLB, 0x00u);
    reg_write(GIO_BASE, GIO_PULDISB, 0x00u);

    /* Interrupt config: all disabled, clear pending flags */
    reg_write(GIO_BASE, GIO_POL, 0x00u);
    reg_write(GIO_BASE, GIO_LVLSET, 0x00u);
    reg_write(GIO_BASE, GIO_FLG, 0xFFu);
    reg_write(GIO_BASE, GIO_ENASET, 0x00u);
}

/**
 * @brief  Initialize RTI timer for 10ms tick
 *
 * Replicates HALCoGen rtiInit() for the SC config:
 *   RTICLK = VCLK (75 MHz) / prescaler (8) = 9.375 MHz
 *   Compare 0 = 93750 counts = 10ms period
 *   No interrupts enabled (SC polls INTFLAG)
 */
void rtiInit(void)
{
    /* NTU source = 5, both counters disabled */
    reg_write(RTI_BASE, RTI_GCTRL, (uint32)(5u << 16u));

    /* Timebase and capture: disabled */
    reg_write(RTI_BASE, RTI_TBCTRL, 0u);
    reg_write(RTI_BASE, RTI_CAPCTRL, 0u);

    /* Compare control: CMP0→cnt0, CMP1→cnt1, CMP2→cnt1, CMP3→cnt0 */
    reg_write(RTI_BASE, RTI_COMPCTRL, 0x00001100u);

    /* Counter 0: reset, prescaler = 8 (CPUCx = 7 → divide by 8) */
    reg_write(RTI_BASE, RTI_UC0, 0u);
    reg_write(RTI_BASE, RTI_FRC0, 0u);
    reg_write(RTI_BASE, RTI_CPUC0, 7u);

    /* Counter 1: reset, prescaler = 8 */
    reg_write(RTI_BASE, RTI_UC1, 0u);
    reg_write(RTI_BASE, RTI_FRC1, 0u);
    reg_write(RTI_BASE, RTI_CPUC1, 7u);

    /* Compare 0: 93750 ticks = 10ms @ 9.375 MHz (SC main loop tick) */
    reg_write(RTI_BASE, RTI_CMP0, 93750u);
    reg_write(RTI_BASE, RTI_UDCP0, 93750u);

    /* Compare 1-3: HALCoGen defaults (not used by SC) */
    reg_write(RTI_BASE, RTI_CMP1, 46875u);
    reg_write(RTI_BASE, RTI_UDCP1, 46875u);
    reg_write(RTI_BASE, RTI_CMP2, 75000u);
    reg_write(RTI_BASE, RTI_UDCP2, 75000u);
    reg_write(RTI_BASE, RTI_CMP3, 93750u);
    reg_write(RTI_BASE, RTI_UDCP3, 93750u);

    /* Clear all pending interrupts, disable all */
    reg_write(RTI_BASE, RTI_INTFLAG, 0x0007000Fu);
    reg_write(RTI_BASE, RTI_CLEARINTENA, 0x00070F0Fu);
}

/**
 * @brief  Start RTI counter block 0
 *
 * Sets GCTRL bit 0 to enable counter block 0.
 * Called from main() after module init, before entering main loop.
 */
void rtiStartCounter(void)
{
    uint32 gctrl = reg_read(RTI_BASE, RTI_GCTRL);
    gctrl |= 1u;  /* Enable counter block 0 */
    reg_write(RTI_BASE, RTI_GCTRL, gctrl);
}

/* ==================================================================
 * GIO pin access (from sc_gio.h / sc_main.c)
 * ================================================================== */

/**
 * @brief  Set GIO pin direction
 * @param  port       Port number (0=A, 1=B)
 * @param  pin        Pin number within port (0-7)
 * @param  direction  0=input, 1=output
 */
void gioSetDirection(uint8 port, uint8 pin, uint8 direction)
{
    uint32 dir_offset;
    uint32 dir_val;

    if (pin > 7u) {
        return;
    }

    if (port == 0u) {
        dir_offset = GIO_DIRA;
    } else if (port == 1u) {
        dir_offset = GIO_DIRB;
    } else {
        return;
    }

    dir_val = reg_read(GIO_BASE, dir_offset);

    if (direction != 0u) {
        dir_val |= ((uint32)1u << (uint32)pin);
    } else {
        dir_val &= ~((uint32)1u << (uint32)pin);
    }

    reg_write(GIO_BASE, dir_offset, dir_val);
}

/**
 * @brief  Set GIO pin value
 * @param  port   Port number (0=A, 1=B)
 * @param  pin    Pin number within port (0-7)
 * @param  value  0=low, 1=high
 */
void gioSetBit(uint8 port, uint8 pin, uint8 value)
{
    uint32 set_offset;
    uint32 clr_offset;

    if (pin > 7u) {
        return;
    }

    if (port == 0u) {
        set_offset = GIO_DSETA;
        clr_offset = GIO_DCLRA;
    } else if (port == 1u) {
        set_offset = GIO_DSETB;
        clr_offset = GIO_DCLRB;
    } else {
        return;
    }

    if (value != 0u) {
        reg_write(GIO_BASE, set_offset, ((uint32)1u << (uint32)pin));
    } else {
        reg_write(GIO_BASE, clr_offset, ((uint32)1u << (uint32)pin));
    }
}

/**
 * @brief  Get GIO pin value (readback from DIN register)
 * @param  port  Port number (0=A, 1=B)
 * @param  pin   Pin number within port (0-7)
 * @return Pin value (0 or 1)
 */
uint8 gioGetBit(uint8 port, uint8 pin)
{
    uint32 din_offset;
    uint32 din_val;

    if (pin > 7u) {
        return 0u;
    }

    if (port == 0u) {
        din_offset = GIO_DINA;
    } else if (port == 1u) {
        din_offset = GIO_DINB;
    } else {
        return 0u;
    }

    din_val = reg_read(GIO_BASE, din_offset);

    return ((din_val >> (uint32)pin) & 1u) != 0u ? 1u : 0u;
}

/* ==================================================================
 * RTI tick checking (from sc_main.c)
 * ================================================================== */

/**
 * @brief  Check if RTI compare 0 flag is set (10ms elapsed)
 * @return TRUE if 10ms has elapsed since last clear
 */
boolean rtiIsTickPending(void)
{
    uint32 flags = reg_read(RTI_BASE, RTI_INTFLAG);
    return ((flags & RTI_CMP0_FLAG) != 0u) ? TRUE : FALSE;
}

/**
 * @brief  Clear RTI compare 0 interrupt flag
 *
 * INTFLAG register (0x88) is write-1-to-clear.
 */
void rtiClearTick(void)
{
    reg_write(RTI_BASE, RTI_INTFLAG, RTI_CMP0_FLAG);
}

/* ==================================================================
 * DCAN1 register access (from sc_can.c)
 * ================================================================== */

/**
 * @brief  Read DCAN1 register
 * @param  offset  Register offset from DCAN1 base
 * @return Register value
 */
uint32 dcan1_reg_read(uint32 offset)
{
    return reg_read(DCAN1_BASE, offset);
}

/**
 * @brief  Write DCAN1 register
 * @param  offset  Register offset from DCAN1 base
 * @param  value   Value to write
 */
void dcan1_reg_write(uint32 offset, uint32 value)
{
    reg_write(DCAN1_BASE, offset, value);
}

/**
 * @brief  Wait for DCAN IF1 to be ready (not busy)
 */
static void dcan1_wait_if1_ready(void)
{
    volatile uint32 timeout = 10000u;
    while (((reg_read(DCAN1_BASE, DCAN_IF1CMD) & DCAN_IFCMD_BUSY) != 0u) &&
           (timeout > 0u)) {
        timeout--;
    }
}

/**
 * @brief  Configure one DCAN1 RX message object via IF1
 *
 * Programs a message object to receive standard-ID CAN frames
 * with exact ID matching (mask = all 1s for 11-bit ID).
 *
 * @param  msg_num  Message object number (1-indexed)
 * @param  can_id   Standard 11-bit CAN ID
 * @param  dlc      Expected DLC (1-8)
 */
static void dcan1_config_rx_mailbox(uint8 msg_num, uint16 can_id, uint8 dlc)
{
    uint32 arb;
    uint32 mctl;

    dcan1_wait_if1_ready();

    /* Mask: match all 11 bits of standard ID (bits 28:18 of mask register) */
    reg_write(DCAN1_BASE, DCAN_IF1MSK,
              ((uint32)0x7FFu << 18u) | ((uint32)1u << 14u));
    /* bit 14 = MDir: also match direction bit */

    /* Arbitration: standard ID in bits 28:18, MsgVal=1, Dir=0 (RX), Xtd=0 */
    arb = ((uint32)can_id << 18u) | DCAN_ARB_MSGVAL;
    reg_write(DCAN1_BASE, DCAN_IF1ARB, arb);

    /* Message control: DLC, UMask=1 (use acceptance mask), EOB=1 */
    mctl = ((uint32)dlc & 0x0Fu) | DCAN_MCTL_UMASK | ((uint32)1u << 7u);
    reg_write(DCAN1_BASE, DCAN_IF1MCTL, mctl);

    /* Transfer IF1 → message object RAM: write mask + arb + control */
    reg_write(DCAN1_BASE, DCAN_IF1CMD,
              DCAN_IFCMD_WR | DCAN_IFCMD_MASK | DCAN_IFCMD_ARB |
              DCAN_IFCMD_CONTROL | ((uint32)msg_num & 0xFFu));

    dcan1_wait_if1_ready();
}

/**
 * @brief  Configure DCAN1 mailbox SC_MB_TX_STATUS (7) for transmission
 *
 * Programs message object 7 as a TX-only mailbox for SC_Status (0x013).
 * Sets ARB with Dir=1 (TX), MsgVal=1, and standard CAN ID.
 * TxRqst is NOT set here — it is set per-frame in dcan1_transmit().
 *
 * @param  msg_num  Hardware message object number (1-indexed), must be 7
 * @param  can_id   Standard 11-bit CAN ID for SC_Status (0x013)
 * @param  dlc      Data length code (4 for SC_Status)
 */
static void dcan1_config_tx_mailbox(uint8 msg_num, uint16 can_id, uint8 dlc)
{
    uint32 arb;
    uint32 mctl;

    dcan1_wait_if1_ready();

    /* No mask filtering for TX objects */
    reg_write(DCAN1_BASE, DCAN_IF1MSK, 0xFFFFFFFFu);

    /* ARB: standard ID in bits 28:18, MsgVal=1, Dir=1 (TX), Xtd=0 */
    arb = ((uint32)can_id << 18u) | DCAN_ARB_MSGVAL | DCAN_ARB_DIR;
    reg_write(DCAN1_BASE, DCAN_IF1ARB, arb);

    /* MCTL: DLC, EOB=1 (bit 7). TxRqst cleared — frame not pending yet. */
    mctl = ((uint32)dlc & 0x0Fu) | ((uint32)1u << 7u);
    reg_write(DCAN1_BASE, DCAN_IF1MCTL, mctl);

    /* Clear data registers — no payload yet */
    reg_write(DCAN1_BASE, DCAN_IF1DATA, 0u);
    reg_write(DCAN1_BASE, DCAN_IF1DATB, 0u);

    /* Transfer IF1 → message object RAM: write all fields */
    reg_write(DCAN1_BASE, DCAN_IF1CMD,
              DCAN_IFCMD_WR | DCAN_IFCMD_MASK | DCAN_IFCMD_ARB |
              DCAN_IFCMD_CONTROL | DCAN_IFCMD_DATAA | DCAN_IFCMD_DATAB |
              ((uint32)msg_num & 0xFFu));

    dcan1_wait_if1_ready();
}

/**
 * @brief  Configure all SC receive mailboxes on DCAN1, and TX mailbox 7
 *
 * Called from SC_CAN_Init() after baud rate and normal mode are set,
 * but before exiting init mode.
 *
 * Mailbox assignments (from Sc_Hw_Cfg.h):
 *   MB1: E-Stop       (0x001) RX
 *   MB2: CVC HB       (0x010) RX
 *   MB3: FZC HB       (0x011) RX
 *   MB4: RZC HB       (0x012) RX
 *   MB5: VehicleState  (0x100) RX
 *   MB6: MotorCurrent  (0x301) RX
 *   MB7: SC_Status     (0x013) TX only (SWR-SC-030)
 */
void dcan1_setup_mailboxes(void)
{
    dcan1_config_rx_mailbox(SC_MB_ESTOP,         SC_CAN_ID_ESTOP,         SC_CAN_DLC);
    dcan1_config_rx_mailbox(SC_MB_CVC_HB,        SC_CAN_ID_CVC_HB,       SC_CAN_DLC);
    dcan1_config_rx_mailbox(SC_MB_FZC_HB,        SC_CAN_ID_FZC_HB,       SC_CAN_DLC);
    dcan1_config_rx_mailbox(SC_MB_RZC_HB,        SC_CAN_ID_RZC_HB,       SC_CAN_DLC);
    dcan1_config_rx_mailbox(SC_MB_VEHICLE_STATE,  SC_CAN_ID_VEHICLE_STATE, SC_CAN_DLC);
    dcan1_config_rx_mailbox(SC_MB_MOTOR_CURRENT,  SC_CAN_ID_MOTOR_CURRENT, SC_CAN_DLC);
    dcan1_config_tx_mailbox(SC_MB_TX_STATUS,     SC_CAN_ID_RELAY_STATUS,  SC_CAN_DLC);
}

/**
 * @brief  Wait for DCAN IF2 to be ready (not busy)
 *
 * DCAN IF registers have a busy flag while transferring data
 * between the CPU interface and message object RAM.
 */
static void dcan1_wait_if2_ready(void)
{
    volatile uint32 timeout = 10000u;
    while (((reg_read(DCAN1_BASE, DCAN_IF2CMD) & DCAN_IFCMD_BUSY) != 0u) &&
           (timeout > 0u)) {
        timeout--;
    }
}

/**
 * @brief  Read CAN data from DCAN1 message object
 *
 * Uses DCAN IF2 interface registers to read from the message object
 * RAM. Checks NewDat bit to determine if new data is available.
 * Clears NewDat after successful read.
 *
 * @param  mbIndex  Mailbox index (0-based, 0-5)
 * @param  data     Output buffer (minimum 8 bytes)
 * @param  dlc      Output: data length code
 * @return TRUE if valid new data available, FALSE otherwise
 */
boolean dcan1_get_mailbox_data(uint8 mbIndex, uint8* data, uint8* dlc)
{
    uint32 msg_num;
    uint32 mctl;
    uint32 data_a;
    uint32 data_b;
    uint8 msg_dlc;

    if (mbIndex >= SC_MB_COUNT) {
        return FALSE;
    }
    if ((data == NULL_PTR) || (dlc == NULL_PTR)) {
        return FALSE;
    }

    /* Message objects are 1-indexed in DCAN hardware */
    msg_num = (uint32)mbIndex + 1u;

    /* Wait for IF2 to be available */
    dcan1_wait_if2_ready();

    /* Request transfer from message object to IF2 registers:
     * Read data A + data B + control (includes NewDat, DLC) */
    reg_write(DCAN1_BASE, DCAN_IF2CMD,
              DCAN_IFCMD_DATAA | DCAN_IFCMD_DATAB |
              DCAN_IFCMD_CONTROL | DCAN_IFCMD_NEWDAT |
              (msg_num & 0xFFu));  /* Message number, WR=0 (read) */

    /* Wait for transfer to complete */
    dcan1_wait_if2_ready();

    /* Check NewDat bit in MCTL */
    mctl = reg_read(DCAN1_BASE, DCAN_IF2MCTL);
    if ((mctl & DCAN_MCTL_NEWDAT) == 0u) {
        return FALSE;  /* No new data */
    }

    /* Extract DLC (bits 3:0) */
    msg_dlc = (uint8)(mctl & 0x0Fu);
    if (msg_dlc > 8u) {
        msg_dlc = 8u;
    }

    /* Read data registers — DCAN stores data in big-endian word order:
     * DATA register: byte0 in bits 7:0, byte1 in 15:8, byte2 in 23:16, byte3 in 31:24
     * DATB register: byte4 in bits 7:0, byte5 in 15:8, byte6 in 23:16, byte7 in 31:24 */
    data_a = reg_read(DCAN1_BASE, DCAN_IF2DATA);
    data_b = reg_read(DCAN1_BASE, DCAN_IF2DATB);

    data[0] = (uint8)(data_a & 0xFFu);
    data[1] = (uint8)((data_a >> 8u) & 0xFFu);
    data[2] = (uint8)((data_a >> 16u) & 0xFFu);
    data[3] = (uint8)((data_a >> 24u) & 0xFFu);
    data[4] = (uint8)(data_b & 0xFFu);
    data[5] = (uint8)((data_b >> 8u) & 0xFFu);
    data[6] = (uint8)((data_b >> 16u) & 0xFFu);
    data[7] = (uint8)((data_b >> 24u) & 0xFFu);

    *dlc = msg_dlc;

    /* NewDat is already cleared atomically by the read command above
     * (DCAN_IFCMD_NEWDAT with WR=0 clears NewDat in the message object) */

    return TRUE;
}

/**
 * @brief  Transmit a CAN frame via DCAN1 message object SC_MB_TX_STATUS
 *
 * Loads data into IF1 registers and sets TxRqst to trigger hardware
 * transmission on the pre-configured mailbox 7 (SC_Status, CAN ID 0x013).
 *
 * @param  mbIndex  Mailbox index — must be SC_MB_TX_STATUS (7)
 * @param  data     Payload bytes (must be non-NULL, length >= dlc)
 * @param  dlc      Data length code (0-8)
 * @note   Thread safety: call only from SC main task (no ISR context).
 *         SWR-SC-030: SC_Status broadcast.
 */
void dcan1_transmit(uint8 mbIndex, const uint8* data, uint8 dlc)
{
    uint32 data_a;
    uint32 data_b;
    uint32 mctl;
    uint8  tx_dlc;

    if (mbIndex != SC_MB_TX_STATUS) {
        return;  /* Only mailbox 7 is configured for TX */
    }
    if (data == NULL_PTR) {
        return;
    }

    tx_dlc = (dlc > 8u) ? 8u : dlc;

    /* Pack bytes into 32-bit words: byte0 in bits 7:0, byte1 in 15:8, etc. */
    data_a = ((uint32)data[0])              |
             ((uint32)data[1] << 8u)        |
             ((uint32)data[2] << 16u)       |
             ((uint32)data[3] << 24u);

    if (tx_dlc > 4u) {
        data_b = ((uint32)data[4])          |
                 ((uint32)data[5] << 8u)    |
                 ((uint32)data[6] << 16u)   |
                 ((uint32)data[7] << 24u);
    } else {
        data_b = 0u;
    }

    dcan1_wait_if1_ready();

    /* Write payload into IF1 data registers */
    reg_write(DCAN1_BASE, DCAN_IF1DATA, data_a);
    reg_write(DCAN1_BASE, DCAN_IF1DATB, data_b);

    /* MCTL: DLC + TxRqst (bit 8) + EOB (bit 7) — triggers transmission */
    mctl = ((uint32)tx_dlc & 0x0Fu) | ((uint32)1u << 8u) | ((uint32)1u << 7u);
    reg_write(DCAN1_BASE, DCAN_IF1MCTL, mctl);

    /* Transfer IF1 → message object 7: write control + data only
     * (ARB already set by dcan1_config_tx_mailbox at init) */
    reg_write(DCAN1_BASE, DCAN_IF1CMD,
              DCAN_IFCMD_WR | DCAN_IFCMD_CONTROL |
              DCAN_IFCMD_DATAA | DCAN_IFCMD_DATAB |
              ((uint32)mbIndex & 0xFFu));

    dcan1_wait_if1_ready();
}

/* ==================================================================
 * ESM (Error Signaling Module) — from sc_esm.c
 * ================================================================== */

/** Runtime mode flag — FALSE during startup (clear-and-continue),
 *  TRUE after SC_ESM_Init() completes (errors trigger safe state). */
static boolean esm_runtime_active = FALSE;

/**
 * @brief  Mark ESM as initialized — switch from startup to runtime mode
 *
 * Called by SC_ESM_Init() after enabling lockstep monitoring.
 * From this point, esmGroup3Notification() will trigger safe state
 * instead of clearing errors and continuing boot.
 */
void esm_set_runtime_mode(void)
{
    esm_runtime_active = TRUE;
}

/**
 * @brief  Enable ESM group 1 channel
 * @param  channel  ESM channel number (0-31)
 */
void esm_enable_group1_channel(uint8 channel)
{
    if (channel > 31u) {
        return;
    }
    reg_write(ESM_BASE, ESM_EEPAPR1, ((uint32)1u << (uint32)channel));
}

/**
 * @brief  Clear ESM flag
 * @param  group    ESM group (1 or 2)
 * @param  channel  ESM channel (0-31)
 */
void esm_clear_flag(uint8 group, uint8 channel)
{
    if (channel > 31u) {
        return;
    }

    if (group == 1u) {
        /* Write 1 to clear the flag in group 1 status register */
        reg_write(ESM_BASE, ESM_SR4, ((uint32)1u << (uint32)channel));
    } else {
        /* Group 2: no direct clear on some TMS570 variants —
         * cleared by ESM key register or hardware reset */
        (void)group;
    }
}

/**
 * @brief  Check if ESM flag is set
 * @param  group    ESM group (1 or 2)
 * @param  channel  ESM channel (0-31)
 * @return TRUE if the ESM flag is set, FALSE otherwise
 */
boolean esm_is_flag_set(uint8 group, uint8 channel)
{
    uint32 status;

    if (channel > 31u) {
        return FALSE;
    }

    if (group == 1u) {
        status = reg_read(ESM_BASE, ESM_SR1);
    } else if (group == 2u) {
        status = reg_read(ESM_BASE, ESM_SR2);
    } else {
        return FALSE;
    }

    return ((status & ((uint32)1u << (uint32)channel)) != 0u) ? TRUE : FALSE;
}

/* ==================================================================
 * Self-test hardware functions (from sc_selftest.c)
 *
 * Phase 1: DONE — stubs for initial bring-up.
 * Phase 2: DONE — real DCAN loopback, GPIO readback, lamp test,
 *          watchdog toggle test.
 * Phase 3 (after validation): Implement STC, PBIST, flash CRC.
 * ================================================================== */

/**
 * @brief  Lockstep CPU BIST via STC module
 * @return TRUE (stub — real STC implementation deferred)
 * @note   TODO:HARDWARE — implement STC self-test
 */
boolean hw_lockstep_bist(void)
{
    /* TODO:HARDWARE — STC (Self-Test Controller) runs lockstep
     * compare test. Requires careful sequencing and takes ~100ms.
     * Deferred until after CAN bring-up. */
    return TRUE;
}

/**
 * @brief  RAM PBIST (Programmable Built-In Self-Test)
 * @return TRUE (stub — real PBIST implementation deferred)
 * @note   TODO:HARDWARE — implement PBIST
 */
boolean hw_ram_pbist(void)
{
    /* TODO:HARDWARE — PBIST runs hardware pattern generator on RAM.
     * Must save/restore tested region. Deferred. */
    return TRUE;
}

/**
 * @brief  Flash CRC integrity check via CRC module
 * @return TRUE (stub — real flash CRC implementation deferred)
 * @note   TODO:HARDWARE — implement flash CRC check
 */
boolean hw_flash_crc_check(void)
{
    /* TODO:HARDWARE — use TMS570 CRC module (MCRC) to verify
     * flash integrity against golden CRC stored at link time.
     * Deferred. */
    return TRUE;
}

/**
 * @brief  DCAN1 internal loopback test
 * @return TRUE (stub — implement after CAN mailbox config verified)
 * @note   TODO:HARDWARE — implement DCAN loopback test (priority 1)
 */
boolean hw_dcan_loopback_test(void)
{
    uint32 ctl_saved;
    uint32 arb;
    uint32 mctl;
    uint32 data_a;
    uint32 mctl_rx;
    volatile uint32 timeout;
    boolean pass = FALSE;

    /* Save current CTL state (should be 0x00 after SC_CAN_Init) */
    ctl_saved = reg_read(DCAN1_BASE, DCAN_CTL);

    /* --- Enter init mode with test enabled --- */
    reg_write(DCAN1_BASE, DCAN_CTL,
              DCAN_CTL_INIT | DCAN_CTL_CCE | DCAN_CTL_TEST);

    /* Wait for Init to be acknowledged */
    timeout = 10000u;
    while (((reg_read(DCAN1_BASE, DCAN_CTL) & DCAN_CTL_INIT) == 0u) &&
           (timeout > 0u)) {
        timeout--;
    }

    /* Enable internal loopback */
    reg_write(DCAN1_BASE, DCAN_TEST, DCAN_TEST_LBACK);

    /* --- Configure RX mailbox (MB9) for test CAN ID --- */
    dcan1_wait_if1_ready();
    reg_write(DCAN1_BASE, DCAN_IF1MSK,
              ((uint32)0x7FFu << 18u) | ((uint32)1u << 14u));
    arb = ((uint32)DCAN_SELFTEST_CAN_ID << 18u) | DCAN_ARB_MSGVAL;
    reg_write(DCAN1_BASE, DCAN_IF1ARB, arb);
    mctl = 8u | DCAN_MCTL_UMASK | ((uint32)1u << 7u);  /* DLC=8, UMask, EOB */
    reg_write(DCAN1_BASE, DCAN_IF1MCTL, mctl);
    reg_write(DCAN1_BASE, DCAN_IF1CMD,
              DCAN_IFCMD_WR | DCAN_IFCMD_MASK | DCAN_IFCMD_ARB |
              DCAN_IFCMD_CONTROL | (uint32)DCAN_SELFTEST_RX_MB);
    dcan1_wait_if1_ready();

    /* --- Configure TX mailbox (MB8) for test CAN ID --- */
    reg_write(DCAN1_BASE, DCAN_IF1MSK, 0xFFFFFFFFu);
    arb = ((uint32)DCAN_SELFTEST_CAN_ID << 18u) |
          DCAN_ARB_MSGVAL | DCAN_ARB_DIR;
    reg_write(DCAN1_BASE, DCAN_IF1ARB, arb);
    /* Write test pattern: 0xA5A5A5A5 */
    reg_write(DCAN1_BASE, DCAN_IF1DATA, 0xA5A5A5A5u);
    reg_write(DCAN1_BASE, DCAN_IF1DATB, 0x5A5A5A5Au);
    /* DLC=8, TxRqst=1, EOB=1 */
    mctl = 8u | DCAN_MCTL_TXRQST | ((uint32)1u << 7u);
    reg_write(DCAN1_BASE, DCAN_IF1MCTL, mctl);
    reg_write(DCAN1_BASE, DCAN_IF1CMD,
              DCAN_IFCMD_WR | DCAN_IFCMD_MASK | DCAN_IFCMD_ARB |
              DCAN_IFCMD_CONTROL | DCAN_IFCMD_DATAA | DCAN_IFCMD_DATAB |
              (uint32)DCAN_SELFTEST_TX_MB);
    dcan1_wait_if1_ready();

    /* --- Exit init mode to allow loopback transmission --- */
    reg_write(DCAN1_BASE, DCAN_CTL, DCAN_CTL_TEST);

    /* Wait for transmission + internal loopback reception */
    timeout = 100000u;
    do {
        /* Read RX mailbox (MB9) via IF2 */
        dcan1_wait_if2_ready();
        reg_write(DCAN1_BASE, DCAN_IF2CMD,
                  DCAN_IFCMD_DATAA | DCAN_IFCMD_DATAB |
                  DCAN_IFCMD_CONTROL | (uint32)DCAN_SELFTEST_RX_MB);
        dcan1_wait_if2_ready();
        mctl_rx = reg_read(DCAN1_BASE, DCAN_IF2MCTL);
        if ((mctl_rx & DCAN_MCTL_NEWDAT) != 0u) {
            break;
        }
        timeout--;
    } while (timeout > 0u);

    /* Verify received data */
    if ((mctl_rx & DCAN_MCTL_NEWDAT) != 0u) {
        data_a = reg_read(DCAN1_BASE, DCAN_IF2DATA);
        if (data_a == 0xA5A5A5A5u) {
            uint32 data_b = reg_read(DCAN1_BASE, DCAN_IF2DATB);
            if (data_b == 0x5A5A5A5Au) {
                pass = TRUE;
            }
        }
    }

    /* --- Cleanup: re-enter init, disable loopback, invalidate test MBs --- */
    reg_write(DCAN1_BASE, DCAN_CTL,
              DCAN_CTL_INIT | DCAN_CTL_CCE | DCAN_CTL_TEST);
    timeout = 10000u;
    while (((reg_read(DCAN1_BASE, DCAN_CTL) & DCAN_CTL_INIT) == 0u) &&
           (timeout > 0u)) {
        timeout--;
    }

    /* Disable loopback */
    reg_write(DCAN1_BASE, DCAN_TEST, 0u);

    /* Invalidate test mailboxes (MsgVal = 0) */
    dcan1_wait_if1_ready();
    reg_write(DCAN1_BASE, DCAN_IF1ARB, 0u);
    reg_write(DCAN1_BASE, DCAN_IF1MCTL, 0u);
    reg_write(DCAN1_BASE, DCAN_IF1CMD,
              DCAN_IFCMD_WR | DCAN_IFCMD_ARB | DCAN_IFCMD_CONTROL |
              (uint32)DCAN_SELFTEST_TX_MB);
    dcan1_wait_if1_ready();
    reg_write(DCAN1_BASE, DCAN_IF1CMD,
              DCAN_IFCMD_WR | DCAN_IFCMD_ARB | DCAN_IFCMD_CONTROL |
              (uint32)DCAN_SELFTEST_RX_MB);
    dcan1_wait_if1_ready();

    /* Restore original CTL — back to normal mode */
    reg_write(DCAN1_BASE, DCAN_CTL, ctl_saved);

    return pass;
}

/**
 * @brief  GPIO readback test — write output, read back DIN
 * @return TRUE (stub)
 * @note   TODO:HARDWARE — implement GPIO readback (priority 2)
 */
boolean hw_gpio_readback_test(void)
{
    uint32 saved_dout;
    uint32 dout_val;
    boolean pass = TRUE;

    /* Verify GIO output register (DOUT) reflects DSET/DCLR writes.
     *
     * On TMS570 LaunchPad, GIOB[6:7] are muxed through N2HET pads —
     * DIN reads the pad state, not the GIO output latch, so DIN-based
     * readback is unreliable. Instead, verify the DOUT register itself
     * responds to SET/CLR operations, which confirms the GIO module
     * and register bus are functional.
     *
     * Production PCB will have direct GIO pins where DIN readback works. */

    /* Save current output state */
    saved_dout = reg_read(GIO_BASE, GIO_DOUTB);

    /* Phase 1: SET bits, verify DOUT reflects HIGH */
    reg_write(GIO_BASE, GIO_DSETB, LED_MASK);
    dout_val = reg_read(GIO_BASE, GIO_DOUTB);
    if ((dout_val & LED_MASK) != LED_MASK) {
        pass = FALSE;
    }

    /* Phase 2: CLR bits, verify DOUT reflects LOW */
    reg_write(GIO_BASE, GIO_DCLRB, LED_MASK);
    dout_val = reg_read(GIO_BASE, GIO_DOUTB);
    if ((dout_val & LED_MASK) != 0u) {
        pass = FALSE;
    }

    /* Restore original state */
    reg_write(GIO_BASE, GIO_DCLRB, LED_MASK);
    reg_write(GIO_BASE, GIO_DSETB, saved_dout & LED_MASK);

    return pass;
}

/**
 * @brief  LED lamp test — all LEDs ON, brief delay, all OFF
 * @return TRUE (stub)
 * @note   TODO:HARDWARE — implement lamp test (priority 3)
 */
boolean hw_lamp_test(void)
{
    volatile uint32 delay;

    /* Use GIOB[6:7] user LEDs — guaranteed present on LaunchPad.
     * GIOA fault LEDs are for production PCB only. */

    /* Both user LEDs ON */
    reg_write(GIO_BASE, GIO_DSETB, LED_MASK);

    /* Busy-wait ~200ms at 300 MHz (Cortex-R5 pipeline ~3 cycles/iteration) */
    for (delay = 0u; delay < 20000000u; delay++) {
        /* spin */
    }

    /* Both user LEDs OFF */
    reg_write(GIO_BASE, GIO_DCLRB, LED_MASK);

    /* Lamp test is visual — always passes if we get here */
    return TRUE;
}

/**
 * @brief  Watchdog (TPS3823) toggle test
 * @return TRUE (stub)
 * @note   TODO:HARDWARE — implement watchdog test
 */
boolean hw_watchdog_test(void)
{
    uint32 dout_val;
    uint32 wdi_mask = ((uint32)1u << SC_PIN_WDI);

    /* Verify WDI pin (GIO_A5) output register responds to SET/CLR.
     * DIN readback unreliable on LaunchPad (N2HET pad muxing) —
     * verify DOUTA register instead. */

    /* Drive WDI HIGH, verify DOUTA reflects */
    reg_write(GIO_BASE, GIO_DSETA, wdi_mask);
    dout_val = reg_read(GIO_BASE, GIO_DOUTA);
    if ((dout_val & wdi_mask) == 0u) {
        return FALSE;
    }

    /* Drive WDI LOW, verify DOUTA reflects */
    reg_write(GIO_BASE, GIO_DCLRA, wdi_mask);
    dout_val = reg_read(GIO_BASE, GIO_DOUTA);
    if ((dout_val & wdi_mask) != 0u) {
        return FALSE;
    }

    /* Toggle back HIGH — a valid watchdog feed pulse.
     * TPS3823 timeout is 1.6s, so we are well within budget. */
    reg_write(GIO_BASE, GIO_DSETA, wdi_mask);

    return TRUE;
}

/**
 * @brief  Flash CRC incremental (runtime) — one sector per call
 * @return TRUE (stub)
 * @note   TODO:HARDWARE — implement incremental flash CRC
 */
boolean hw_flash_crc_incremental(void)
{
    /* TODO:HARDWARE — CRC one flash sector using MCRC module.
     * Called once per 60s runtime self-test cycle. */
    return TRUE;
}

/**
 * @brief  DCAN error check (runtime) — read ES register
 * @return TRUE if no critical errors, FALSE if bus-off or error passive
 */
boolean hw_dcan_error_check(void)
{
    uint32 es = reg_read(DCAN1_BASE, DCAN_ES);

    /* Check bus-off (bit 7) and error passive (bit 5) */
    if ((es & 0xA0u) != 0u) {
        return FALSE;
    }
    return TRUE;
}

/* ==================================================================
 * GIO LED control (LaunchPad user LEDs)
 *
 * LAUNCHXL2-570LC43 user LEDs are on GIO Port B:
 *   LED2 = GIOB[6] — active HIGH
 *   LED3 = GIOB[7] — active HIGH
 *
 * Used for firmware execution verification during bring-up.
 * ================================================================== */

/**
 * @brief  Turn on LaunchPad user LEDs (LED2 + LED3) via GIOB[6:7]
 *
 * Sets GIOB pins 6 and 7 as outputs and drives HIGH.
 * gioInit() must have been called first.
 */
void sc_het_led_on(void)
{
    /* Set GIOB[6] and GIOB[7] as outputs (OR into existing DIRB) */
    reg_write(GIO_BASE, GIO_DIRB,
              reg_read(GIO_BASE, GIO_DIRB) | LED_MASK);

    /* Turn on both LEDs */
    reg_write(GIO_BASE, GIO_DSETB, LED_MASK);
}

/**
 * @brief  Turn off LaunchPad user LEDs (LED2 + LED3)
 */
void sc_het_led_off(void)
{
    reg_write(GIO_BASE, GIO_DCLRB, LED_MASK);
}

/**
 * @brief  Set individual user LEDs (LED2=GIOB[6], LED3=GIOB[7])
 * @param  led2  1=ON, 0=OFF
 * @param  led3  1=ON, 0=OFF
 */
void sc_het_led_set(uint8 led2, uint8 led3)
{
    uint32 set = 0u;
    uint32 clr = 0u;

    if (led2 != 0u) { set |= ((uint32)1u << 6u); }
    else             { clr |= ((uint32)1u << 6u); }
    if (led3 != 0u) { set |= ((uint32)1u << 7u); }
    else             { clr |= ((uint32)1u << 7u); }

    if (set != 0u) { reg_write(GIO_BASE, GIO_DSETB, set); }
    if (clr != 0u) { reg_write(GIO_BASE, GIO_DCLRB, clr); }
}

/* ==================================================================
 * ESM Group 3 Notification Override
 *
 * HALCoGen's default esmGroup3Notification() enters an infinite loop,
 * which prevents the CPU from reaching main() if any ESM Group 3
 * error is set during power-up or after a debug reset.
 *
 * Common cause: CCM-R5F lockstep compare error after JTAG/XDS110
 * debug reset desynchronizes the dual CPUs.
 *
 * This override clears the error and resets the nERROR pin so the
 * startup can continue. The ESM Group 3 status register (SR1[2])
 * is at ESM_BASE + 0x20, and the Error Key Register (EKR) is at
 * ESM_BASE + 0x38 (write 5 to reset error pin).
 *
 * NOTE: For production, this should be replaced with proper
 * error handling per ISO 26262 requirements.
 * ================================================================== */

/** ESM status register offsets */
#define ESM_SR1_0               0x18u   /* Group 1 status (channels 0-31) */
#define ESM_SR1_1               0x1Cu   /* Group 1 status (channels 32-63) */
#define ESM_SR3                 0x20u   /* Group 3 status (write-1-to-clear) */
#define ESM_EKR_OFF             0x38u   /* Error Key Register */

/** CCM-R5F registers (from HL_reg_ccmr5.h, base 0xFFFFF600) */
#define CCMR5_BASE              0xFFFFF600u
#define CCMSR1_OFF              0x00u   /* Status Register 1 (CPU compare) */
#define CCMSR2_OFF              0x08u   /* Status Register 2 (VIM compare) */
#define CCMSR3_OFF              0x10u   /* Status Register 3 (periph compare) */
#define CCMSR4_OFF              0x1Cu   /* Status Register 4 (inactivity) */

/*
 * HALCoGen notification function replacements.
 *
 * HL_notification.c is excluded from the build because its
 * esmGroup3Notification contains for(;;){} which hangs the CPU
 * on any ESM Group 3 error (common after debug reset due to
 * CCM-R5F lockstep desync).
 *
 * We provide our own implementations for the 3 ESM notification
 * functions called by HALCoGen startup/ISR code.
 *
 * The prototypes in HL_esm.h use (esmBASE_t*, uint32). Since we
 * don't include that header, we use compatible (void*, uint32).
 * ARM C ABI passes both as 32-bit values in r0/r1 — identical.
 */

/**
 * @brief  ESM Group 1 notification (low/medium priority errors)
 *
 * Called from HL_esm.c (ISR) and HL_sys_vim.c. Empty stub for
 * bring-up — errors are cleared by the caller.
 */
void esmGroup1Notification(void *esm, uint32 channel)
{
    (void)esm;
    (void)channel;
}

/**
 * @brief  ESM Group 2 notification (high priority errors)
 *
 * Called from HL_esm.c (ISR) and HL_sys_vim.c. Empty stub.
 */
void esmGroup2Notification(void *esm, uint32 channel)
{
    (void)esm;
    (void)channel;
}

/**
 * @brief  ESM Group 3 notification (critical errors — nERROR pin)
 *
 * Called from HL_sys_startup.c if ESM Group 3 flags are set on
 * power-up (e.g., CCM-R5F lockstep error after JTAG debug reset).
 *
 * HALCoGen default: for(;;){} — hangs forever, CPU never reaches main().
 * Our override: clear the error flags and reset nERROR pin, allowing
 * startup to continue.
 *
 * NOTE: For production, replace with proper error handling per
 * ISO 26262 requirements.
 */
/* GAP-SC-005 debug: snapshot of CCM/ESM registers captured in
 * esmGroup3Notification BEFORE clearing. Readable from main(). */
static uint32 g3_ccmsr1;
static uint32 g3_ccmsr2;
static uint32 g3_ccmsr3;
static uint32 g3_ccmsr4;
static uint32 g3_esm_sr1;
static uint32 g3_esm_sr3;
static uint32 g3_esm_ekr;
static uint32 g3_channel;
static uint32 g3_call_count;

void sc_ccm_debug_get(uint32 *out)
{
    out[0] = g3_ccmsr1;
    out[1] = g3_ccmsr2;
    out[2] = g3_ccmsr3;
    out[3] = g3_ccmsr4;
    out[4] = g3_esm_sr1;
    out[5] = g3_esm_sr3;
    out[6] = g3_esm_ekr;
    out[7] = g3_channel;
    out[8] = g3_call_count;
}

void esmGroup3Notification(void *esm, uint32 channel)
{
    (void)esm;

    /* GAP-SC-005: snapshot registers BEFORE clearing */
    g3_ccmsr1  = reg_read(CCMR5_BASE, CCMSR1_OFF);
    g3_ccmsr2  = reg_read(CCMR5_BASE, CCMSR2_OFF);
    g3_ccmsr3  = reg_read(CCMR5_BASE, CCMSR3_OFF);
    g3_ccmsr4  = reg_read(CCMR5_BASE, CCMSR4_OFF);
    g3_esm_sr1 = reg_read(ESM_BASE, ESM_SR1_0);
    g3_esm_sr3 = reg_read(ESM_BASE, ESM_SR3);
    g3_esm_ekr = reg_read(ESM_BASE, ESM_EKR_OFF);
    g3_channel = channel;
    g3_call_count++;

    /* RUNTIME: If ESM init is complete, a Group 3 error is a genuine
     * safety fault (lockstep, CCM compare, etc). Enter safe state
     * immediately — relay off, LED on, halt for watchdog reset.
     * Do NOT clear the error — let it persist for post-mortem. */
    if (esm_runtime_active != FALSE) {
        SC_ESM_HighLevelInterrupt();
        /* Never returns */
    }

    /* STARTUP: Clear errors and continue boot.
     * Common cause: CCM-R5F lockstep desync after JTAG debug reset. */

    /* 1. Clear ALL CCM-R5F compare errors at their SOURCE.
     *    The CCM continuously drives ESM Group 3 until its status
     *    registers are cleared. Without this, ESM immediately
     *    re-latches after we clear it. */
    reg_write(CCMR5_BASE, CCMSR1_OFF, 0xFFFFFFFFu);
    reg_write(CCMR5_BASE, CCMSR2_OFF, 0xFFFFFFFFu);
    reg_write(CCMR5_BASE, CCMSR3_OFF, 0xFFFFFFFFu);
    reg_write(CCMR5_BASE, CCMSR4_OFF, 0xFFFFFFFFu);

    /* 2. Clear ALL ESM status registers (write-1-to-clear) */
    reg_write(ESM_BASE, ESM_SR1_0, 0xFFFFFFFFu);
    reg_write(ESM_BASE, ESM_SR1_1, 0xFFFFFFFFu);
    reg_write(ESM_BASE, ESM_SR3, channel);

    /* 3. Reset nERROR pin to inactive (HIGH) — key value 5 */
    reg_write(ESM_BASE, ESM_EKR_OFF, 0x00000005u);
}

/* ==================================================================
 * SCI Debug UART
 *
 * SCI1 is connected to XDS110 virtual COM port on the LaunchPad.
 * Used for boot diagnostics only — not safety-relevant.
 * ================================================================== */

/** SCI3 base address (standalone SCI module) */
#define SCI3_BASE               0xFFF7E500u

/**
 * @brief  Initialize ALL SCI modules for debug output
 *
 * We don't know which SCI module the XDS110 UART connects to on the
 * LAUNCHXL2-570LC43, so we initialize both:
 *   - SCI1/LIN1 (0xFFF7E400) at 115200 baud (ball A5 = LIN1TX)
 *   - SCI3 (0xFFF7E500) at 9600 baud (HALCoGen default via sciInit)
 *
 * Then sc_sci_puts() outputs on BOTH modules simultaneously.
 */
static void sc_sci_init_module(uint32 base, uint32 brs)
{
    /* GCR0 reset pulse */
    reg_write(base, 0x00u, 0u);
    reg_write(base, 0x00u, 1u);

    /* Disable all interrupts */
    reg_write(base, 0x10u, 0xFFFFFFFFu);
    reg_write(base, 0x18u, 0xFFFFFFFFu);

    /* GCR1: SCI async mode, TX+RX enabled, internal clock, 8N1
     * Bit 25: TXENA, Bit 24: RXENA, Bit 5: CLOCK=internal,
     * Bit 4: STOP=0 (1 stop bit), Bit 1: TIMING=async.
     *
     * NOTE: Do NOT set bit 6! On the LIN/SCI module (SCI1/LIN1),
     * bit 6 = ADAPT/LIN_MODE which switches to LIN protocol framing,
     * preventing standard UART operation. On standalone SCI (SCI3),
     * bit 6 = CONT, but leaving it clear is harmless. */
    reg_write(base, SCI_GCR1,
              ((uint32)1u << 25u) |   /* TXENA */
              ((uint32)1u << 24u) |   /* RXENA */
              ((uint32)1u << 5u)  |   /* CLOCK = internal */
              ((uint32)1u << 1u));    /* TIMING = async */

    /* Baud rate */
    reg_write(base, SCI_BRS, brs);

    /* FORMAT: 8-bit characters */
    reg_write(base, SCI_FORMAT, 7u);

    /* Pin config: TX+RX functional, pull-ups enabled */
    reg_write(base, SCI_PIO0, 6u);
    reg_write(base, 0x40u, 0u);       /* PIO1: direction */
    reg_write(base, 0x48u, 0u);       /* PIO3: output value */
    reg_write(base, 0x54u, 0u);       /* PIO6: open drain */
    reg_write(base, 0x58u, 0u);       /* PIO7: pull disable */
    reg_write(base, 0x5Cu, 6u);       /* PIO8: pull select */

    /* Release from reset */
    {
        uint32 gcr1 = reg_read(base, SCI_GCR1);
        gcr1 |= 0x80u;
        reg_write(base, SCI_GCR1, gcr1);
    }
}

void sc_sci_init(void)
{
    /* SCI1/LIN1: 115200 baud at VCLK1=75MHz, BRS=40 → 114329 baud */
    sc_sci_init_module(SCI_BASE, 40u);

    /* SCI3: 9600 baud at VCLK1=75MHz, BRS=487 → 9607 baud */
    sc_sci_init_module(SCI3_BASE, 487u);
}

/**
 * @brief  Send a single byte on a specific SCI module
 * @param  base  SCI module base address
 * @param  ch    Character to transmit
 */
static void sc_sci_putchar_on(uint32 base, uint8 ch)
{
    volatile uint32 timeout = 100000u;
    /* Wait for TX ready with timeout */
    while (((reg_read(base, SCI_FLR) & SCI_FLR_TXRDY) == 0u) && (timeout > 0u)) {
        timeout--;
    }
    if (timeout > 0u) {
        reg_write(base, SCI_TD, (uint32)ch);
    }
}

/**
 * @brief  Send a single byte over ALL SCI modules
 * @param  ch  Character to transmit
 */
void sc_sci_putchar(uint8 ch)
{
    sc_sci_putchar_on(SCI_BASE, ch);   /* SCI1/LIN1 at 115200 */
    sc_sci_putchar_on(SCI3_BASE, ch);  /* SCI3 at 9600 */
}

/**
 * @brief  Send a null-terminated string over SCI1
 * @param  str  String to transmit
 */
void sc_sci_puts(const char* str)
{
    if (str == NULL_PTR) {
        return;
    }
    while (*str != '\0') {
        sc_sci_putchar((uint8)*str);
        str++;
    }
}

/**
 * @brief  Send a uint32 as decimal string over SCI1
 * @param  val  Value to print
 */
void sc_sci_put_uint(uint32 val)
{
    char buf[11];  /* max 10 digits + null */
    uint8 i = 0u;
    uint8 j;

    if (val == 0u) {
        sc_sci_putchar((uint8)'0');
        return;
    }

    while (val > 0u) {
        buf[i] = (char)((uint8)'0' + (uint8)(val % 10u));
        val /= 10u;
        i++;
    }

    /* Print in reverse order */
    for (j = i; j > 0u; j--) {
        sc_sci_putchar((uint8)buf[j - 1u]);
    }
}

/**
 * @brief  Send a uint32 as 8-digit hex string ("0xNNNNNNNN") over SCI
 * @param  val  Value to print
 */
void sc_sci_put_hex32(uint32 val)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8 i;
    uint32 shift;
    uint32 idx;

    sc_sci_putchar((uint8)'0');
    sc_sci_putchar((uint8)'x');
    for (i = 0u; i < 8u; i++) {
        shift = 28u - ((uint32)i * 4u);
        idx = (val >> shift) & 0x0Fu;
        sc_sci_putchar((uint8)hex[idx]);
    }
}

/* ==================================================================
 * High-level debug functions (called unconditionally from sc_main.c)
 * ================================================================== */

void sc_hw_debug_boot_dump(void)
{
    uint32 ccm_dbg[9];
    sc_ccm_debug_get(ccm_dbg);
    sc_sci_puts("--- CCM/ESM G3 snapshot (pre-clear) ---\r\n");
    sc_sci_puts("G3_calls="); sc_sci_put_uint(ccm_dbg[8]); sc_sci_puts("\r\n");
    sc_sci_puts("G3_ch=");    sc_sci_put_uint(ccm_dbg[7]); sc_sci_puts("\r\n");
    sc_sci_puts("CCMSR1=");   sc_sci_put_hex32(ccm_dbg[0]); sc_sci_puts("\r\n");
    sc_sci_puts("CCMSR2=");   sc_sci_put_hex32(ccm_dbg[1]); sc_sci_puts("\r\n");
    sc_sci_puts("CCMSR3=");   sc_sci_put_hex32(ccm_dbg[2]); sc_sci_puts("\r\n");
    sc_sci_puts("CCMSR4=");   sc_sci_put_hex32(ccm_dbg[3]); sc_sci_puts("\r\n");
    sc_sci_puts("ESM_SR1=");  sc_sci_put_hex32(ccm_dbg[4]); sc_sci_puts("\r\n");
    sc_sci_puts("ESM_SR3=");  sc_sci_put_hex32(ccm_dbg[5]); sc_sci_puts("\r\n");
    sc_sci_puts("ESM_EKR=");  sc_sci_put_hex32(ccm_dbg[6]); sc_sci_puts("\r\n");
    sc_sci_puts("--- current registers (post-clear) ---\r\n");
    sc_sci_puts("CCMSR1=");   sc_sci_put_hex32(*(volatile uint32 *)0xFFFFF600u); sc_sci_puts("\r\n");
    sc_sci_puts("ESM_SR1=");  sc_sci_put_hex32(*(volatile uint32 *)0xFFFFF518u); sc_sci_puts("\r\n");
    sc_sci_puts("ESM_SR3=");  sc_sci_put_hex32(*(volatile uint32 *)0xFFFFF520u); sc_sci_puts("\r\n");
    sc_sci_puts("ESM_EKR=");  sc_sci_put_hex32(*(volatile uint32 *)0xFFFFF538u); sc_sci_puts("\r\n");
    sc_sci_puts("--- end dump ---\r\n");
}

void sc_hw_debug_periodic(void)
{
    uint32 ccm_dbg[9];

    sc_sci_puts("[5s] SC: ES=0x");
    sc_sci_put_uint(*(volatile uint32 *)0xFFF7DC04u);  /* DCAN1 ES */
    sc_sci_puts(" ND=0x");
    sc_sci_put_uint(*(volatile uint32 *)0xFFF7DC9Cu);  /* DCAN1 NEWDAT1 */
    sc_sci_puts("\r\n");

    sc_ccm_debug_get(ccm_dbg);
    sc_sci_puts("[CCM] G3_calls="); sc_sci_put_uint(ccm_dbg[8]);
    sc_sci_puts(" ch="); sc_sci_put_uint(ccm_dbg[7]);
    sc_sci_puts(" CCMSR1="); sc_sci_put_hex32(ccm_dbg[0]);
    sc_sci_puts(" CCMSR2="); sc_sci_put_hex32(ccm_dbg[1]);
    sc_sci_puts(" CCMSR3="); sc_sci_put_hex32(ccm_dbg[2]);
    sc_sci_puts(" CCMSR4="); sc_sci_put_hex32(ccm_dbg[3]);
    sc_sci_puts("\r\n");
    sc_sci_puts("[ESM] SR1="); sc_sci_put_hex32(ccm_dbg[4]);
    sc_sci_puts(" SR3="); sc_sci_put_hex32(ccm_dbg[5]);
    sc_sci_puts(" EKR="); sc_sci_put_hex32(ccm_dbg[6]);
    sc_sci_puts(" now_SR1="); sc_sci_put_hex32(*(volatile uint32 *)0xFFFFF518u);
    sc_sci_puts(" now_SR3="); sc_sci_put_hex32(*(volatile uint32 *)0xFFFFF520u);
    sc_sci_puts("\r\n");
}

#endif /* PLATFORM_TMS570 */

/* Override HALCoGen's emif_SDRAM_StartupInit() — LaunchPad has no SDRAM.
 * The real implementation in HL_emif.c configures EMIF registers for
 * external SDRAM that doesn't exist on LAUNCHXL2-570LC43, causing a hang. */
void emif_SDRAM_StartupInit(void) { /* no-op: no SDRAM on LaunchPad */ }
