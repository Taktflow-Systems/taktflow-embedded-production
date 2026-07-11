/**
 * @file    Can_Hw_STM32F4.c
 * @brief   STM32F4 bxCAN1 bare-metal hardware backend for CAN MCAL driver
 * @date    2026-03-23
 *
 * @details Bare-metal bxCAN1 driver for STM32F413ZH (NUCLEO-F413ZH).
 *          No HAL dependency for CAN — direct register access to avoid
 *          HAL_CAN_Init sleep-mode deadlock (HAL doesn't clear SLEEP
 *          before requesting INRQ, causing init to hang if CAN_RX has
 *          no recessive level at boot).
 *
 *          Pin assignment: PD0 = CAN1_RX, PD1 = CAN1_TX (AF9)
 *          Baudrate: 500 kbps @ 48 MHz APB1 (PSC=6, BS1=13, BS2=2)
 *
 * @safety_req SWR-BSW-001..005
 * @copyright Taktflow Systems 2026
 */

#include "Platform_Types.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can.h"
#include "stm32f4xx_hal.h"

/* ==================================================================
 * bxCAN1 Register Definitions (from RM0430 Section 32.9)
 *
 * CAN1 base: 0x40006400
 * MCR  +0x00  Master Control
 * MSR  +0x04  Master Status
 * TSR  +0x08  Transmit Status
 * RF0R +0x0C  Receive FIFO 0
 * IER  +0x14  Interrupt Enable
 * ESR  +0x18  Error Status
 * BTR  +0x1C  Bit Timing
 * TI0R +0x180 TX Mailbox 0 Identifier
 * TDT0R+0x184 TX Mailbox 0 Data Length/Timestamp
 * TDL0R+0x188 TX Mailbox 0 Data Low
 * TDH0R+0x18C TX Mailbox 0 Data High
 * RI0R +0x1B0 RX FIFO 0 Identifier
 * RDT0R+0x1B4 RX FIFO 0 Data Length/Timestamp
 * RDL0R+0x1B8 RX FIFO 0 Data Low
 * RDH0R+0x1BC RX FIFO 0 Data High
 * FMR  +0x200 Filter Master
 * FM1R +0x204 Filter Mode
 * FS1R +0x20C Filter Scale
 * FFA1R+0x214 Filter FIFO Assignment
 * FA1R +0x21C Filter Activation
 * FR1  +0x240 Filter Bank 0 Register 1
 * FR2  +0x244 Filter Bank 0 Register 2
 * ================================================================== */

/* MCR bits */
#define MCR_INRQ    (1u << 0)
#define MCR_SLEEP   (1u << 1)
#define MCR_TXFP    (1u << 2)
#define MCR_RFLM    (1u << 3)
#define MCR_NART    (1u << 4)
#define MCR_AWUM    (1u << 5)
#define MCR_ABOM    (1u << 6)
#define MCR_TTCM    (1u << 7)
#define MCR_DBF     (1u << 16)

/* MSR bits */
#define MSR_INAK    (1u << 0)
#define MSR_SLAK    (1u << 1)

/* TSR bits */
#define TSR_TME0    (1u << 26)
#define TSR_TME1    (1u << 27)
#define TSR_TME2    (1u << 28)

/* RF0R bits */
#define RF0R_FMP0   (3u << 0)
#define RF0R_RFOM0  (1u << 5)

/* ESR bits */
#define ESR_BOFF    (1u << 2)

/* TIxR bits */
#define TIR_TXRQ    (1u << 0)

/* BTR: BRP=5 (prescaler-1=5→PSC=6), BS1=12 (seg1-1=12→BS1=13), BS2=1 (seg2-1=1→BS2=2), SJW=0
 * Bit time = 6 * (1 + 13 + 2) = 6 * 16 = 96
 * Baudrate = 48,000,000 / 96 = 500,000 bps
 * Sample point = (1+13)/16 = 87.5% — matches proven-working HAL config */
#define BTR_500K_48MHZ  (0x001C0005u)  /* SJW=0, BS2=1, BS1=12, BRP=5 */

/* Timeout for register waits */
#define CAN_TIMEOUT  1000000u

/* ==================================================================
 * GPIO Configuration (bare-metal)
 * ================================================================== */

static void Can_Hw_ConfigGpio(void)
{
    /* Enable GPIOD clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

    /* PD0 = CAN1_RX (AF9), PD1 = CAN1_TX (AF9)
     * MODER: alternate function (10)
     * OSPEEDR: very high speed (11)
     * PUPDR: no pull (00)
     * AFR[0]: AF9 = 0x9 for pins 0 and 1 */

    /* MODER: bits [1:0]=10 (PD0), bits [3:2]=10 (PD1) */
    GPIOD->MODER = (GPIOD->MODER & ~(0xFu << 0)) | (0xAu << 0);

    /* OSPEEDR: very high for both */
    GPIOD->OSPEEDR |= (0xFu << 0);

    /* No pull-up/pull-down */
    GPIOD->PUPDR &= ~(0xFu << 0);

    /* AFR[0]: PD0 = AF9, PD1 = AF9 */
    GPIOD->AFR[0] = (GPIOD->AFR[0] & ~(0xFFu << 0)) | (0x99u << 0);
}

/* ==================================================================
 * CAN Init (bare-metal)
 * ================================================================== */

Std_ReturnType Can_Hw_Init(uint32 baudrate)
{
    volatile uint32_t timeout;
    (void)baudrate;

    /* 1. Enable CAN1 peripheral clock */
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    (void)(RCC->APB1ENR & RCC_APB1ENR_CAN1EN);

    /* 2. Configure GPIO PD0/PD1 for CAN */
    Can_Hw_ConfigGpio();

    /* 3. Exit SLEEP and enter INIT mode.
     * Safe bring-up: start in SILENT mode (SILM=1 in BTR) to avoid
     * bus disruption during init. NART=1 to disable auto-retransmission
     * so a single TX failure doesn't flood error frames. */
    CAN1->MCR = MCR_DBF | MCR_ABOM | MCR_NART | MCR_INRQ;

    /* 4. Wait for INAK=1 and SLAK=0 */
    timeout = CAN_TIMEOUT;
    while (timeout > 0u)
    {
        uint32_t msr = CAN1->MSR;
        if (((msr & MSR_INAK) != 0u) && ((msr & MSR_SLAK) == 0u))
        {
            break;
        }
        timeout--;
    }
    if (timeout == 0u)
    {
        return E_NOT_OK;
    }

    /* 5. Configure bit timing in SILENT mode first — listens but won't TX.
     * SILM=1 (bit 31): silent mode, no TX on bus.
     * This lets us verify the bus is healthy before transmitting. */
    CAN1->BTR = BTR_500K_48MHZ | (1u << 31);  /* SILM=1 */

    /* 6. Configure accept-all filter (bank 0, mask mode, 32-bit, FIFO0) */
    CAN1->FMR |= 1u;          /* FINIT — enter filter init mode */
    CAN1->FA1R &= ~(1u << 0); /* Deactivate filter 0 */
    CAN1->FS1R |= (1u << 0);  /* 32-bit scale for filter 0 */
    CAN1->FM1R &= ~(1u << 0); /* Mask mode for filter 0 */
    CAN1->FFA1R &= ~(1u << 0);/* Assign filter 0 to FIFO0 */
    /* Filter ID and mask = 0 → accept all */
    *(volatile uint32_t *)(0x40006640u) = 0u; /* Filter bank 0 register 1 */
    *(volatile uint32_t *)(0x40006644u) = 0u; /* Filter bank 0 register 2 */
    CAN1->FA1R |= (1u << 0);  /* Activate filter 0 */
    CAN1->FMR &= ~1u;         /* Exit filter init mode */

    /* 7. Leave INIT mode — clear INRQ, wait for INAK=0 */
    CAN1->MCR &= ~MCR_INRQ;
    timeout = CAN_TIMEOUT;
    while (((CAN1->MSR & MSR_INAK) != 0u) && (timeout > 0u))
    {
        timeout--;
    }

    return E_OK;
}

/* ==================================================================
 * Start / Stop
 * ================================================================== */

void Can_Hw_Start(void)
{
    volatile uint32_t timeout;

    /* Transition from SILENT mode → NORMAL mode.
     * Re-enter INIT, change BTR (clear SILM), exit INIT.
     * Also switch from NART=1 (single-shot) to NART=0 (auto-retransmit)
     * now that we know the bus is alive. */
    CAN1->MCR |= MCR_INRQ;
    timeout = CAN_TIMEOUT;
    while (((CAN1->MSR & MSR_INAK) == 0u) && (timeout > 0u)) { timeout--; }

    /* Clear SILM — normal mode. Keep NART=1 (single-shot TX) to avoid
     * bus flooding on error. Dropped frames are retried by Com at the
     * next cycle, not by the CAN controller. */
    CAN1->BTR = BTR_500K_48MHZ;  /* SILM=0, LBKM=0 */
    CAN1->MCR = MCR_DBF | MCR_ABOM | MCR_NART;  /* NART=1, ABOM=1, clear INRQ */

    timeout = CAN_TIMEOUT;
    while (((CAN1->MSR & MSR_INAK) != 0u) && (timeout > 0u)) { timeout--; }
}

void Can_Hw_Stop(void)
{
    /* Enter INIT mode */
    CAN1->MCR |= MCR_INRQ;
    volatile uint32_t timeout = CAN_TIMEOUT;
    while (((CAN1->MSR & MSR_INAK) == 0u) && (timeout > 0u)) { timeout--; }
}

/* ==================================================================
 * Transmit
 * ================================================================== */

Std_ReturnType Can_Hw_Transmit(Can_IdType id, const uint8* data, uint8 dlc)
{
    uint32_t mailbox = 0u;
    uint32_t tsr;
    uint16 retry;

    if (dlc > 8u)
    {
        return E_NOT_OK;
    }

    /* Find an empty TX mailbox */
    for (retry = 0u; retry < 5000u; retry++)
    {
        tsr = CAN1->TSR;
        if (tsr & TSR_TME0)      { mailbox = 0u; break; }
        else if (tsr & TSR_TME1) { mailbox = 1u; break; }
        else if (tsr & TSR_TME2) { mailbox = 2u; break; }
    }
    if (retry >= 5000u)
    {
        return E_NOT_OK;
    }

    /* Write identifier (standard 11-bit, shifted left by 21) */
    CAN1->sTxMailBox[mailbox].TIR = ((uint32_t)id << 21);

    /* Write DLC */
    CAN1->sTxMailBox[mailbox].TDTR = (uint32_t)dlc;

    /* Write data bytes */
    {
        uint32_t dl = 0u, dh = 0u;
        uint8 i;
        for (i = 0u; i < dlc && i < 4u; i++)
        {
            dl |= ((uint32_t)data[i]) << (i * 8u);
        }
        for (i = 4u; i < dlc && i < 8u; i++)
        {
            dh |= ((uint32_t)data[i]) << ((i - 4u) * 8u);
        }
        CAN1->sTxMailBox[mailbox].TDLR = dl;
        CAN1->sTxMailBox[mailbox].TDHR = dh;
    }

    /* Request transmission */
    CAN1->sTxMailBox[mailbox].TIR |= TIR_TXRQ;

    return E_OK;
}

/* ==================================================================
 * Receive
 * ================================================================== */

boolean Can_Hw_Receive(Can_IdType* id, uint8* data, uint8* dlc)
{
    uint32_t rir, rdtr, rdlr, rdhr;
    uint8 len, i;

    /* Check if FIFO0 has messages */
    if ((CAN1->RF0R & RF0R_FMP0) == 0u)
    {
        return FALSE;
    }

    /* Read mailbox */
    rir  = CAN1->sFIFOMailBox[0].RIR;
    rdtr = CAN1->sFIFOMailBox[0].RDTR;
    rdlr = CAN1->sFIFOMailBox[0].RDLR;
    rdhr = CAN1->sFIFOMailBox[0].RDHR;

    /* Release FIFO */
    CAN1->RF0R |= RF0R_RFOM0;

    /* Extract ID (standard only) */
    *id = (Can_IdType)((rir >> 21) & 0x7FFu);

    /* Extract DLC */
    len = (uint8)(rdtr & 0xFu);
    if (len > 8u) { len = 8u; }
    *dlc = len;

    /* Extract data */
    for (i = 0u; i < len && i < 4u; i++)
    {
        data[i] = (uint8)(rdlr >> (i * 8u));
    }
    for (i = 4u; i < len; i++)
    {
        data[i] = (uint8)(rdhr >> ((i - 4u) * 8u));
    }

    return TRUE;
}

/* ==================================================================
 * Status / Diagnostics
 * ================================================================== */

boolean Can_Hw_IsBusOff(void)
{
    return ((CAN1->ESR & ESR_BOFF) != 0u) ? TRUE : FALSE;
}

uint8 Can_Hw_GetHalState(void)
{
    /* Map bxCAN state to a simple enum:
     * 0=RESET, 1=READY, 2=LISTENING, 3=SLEEP_PENDING, 4=SLEEP, 5=ERROR */
    if (CAN1->MSR & MSR_SLAK) return 4u;
    if (CAN1->MSR & MSR_INAK) return 0u;
    if (CAN1->ESR & ESR_BOFF) return 5u;
    return 1u; /* READY */
}

void Can_Hw_GetErrorCounters(uint8* tec, uint8* rec)
{
    uint32_t esr = CAN1->ESR;
    if (tec != NULL_PTR) { *tec = (uint8)((esr >> 16u) & 0xFFu); }
    if (rec != NULL_PTR) { *rec = (uint8)((esr >> 24u) & 0xFFu); }
}

/* ==================================================================
 * Loopback Self-Test (bare-metal)
 * ================================================================== */

Std_ReturnType Can_Hw_LoopbackTest(void)
{
    volatile uint32_t timeout;
    Can_IdType rxId;
    uint8 rxData[8];
    uint8 rxDlc;
    static const uint8 testData[8] = {0xCA,0xFE,0xBA,0xBE,0xDE,0xAD,0x01,0x02};
    uint8 i;

    /* Enter INIT mode */
    CAN1->MCR |= MCR_INRQ;
    timeout = CAN_TIMEOUT;
    while (((CAN1->MSR & MSR_INAK) == 0u) && (timeout > 0u)) { timeout--; }

    /* Set loopback + silent mode in BTR (LBKM=1, SILM=1) */
    CAN1->BTR = BTR_500K_48MHZ | (1u << 30) | (1u << 31);

    /* Leave INIT */
    CAN1->MCR &= ~MCR_INRQ;
    timeout = CAN_TIMEOUT;
    while (((CAN1->MSR & MSR_INAK) != 0u) && (timeout > 0u)) { timeout--; }

    /* Transmit test frame */
    if (Can_Hw_Transmit(0x7FFu, testData, 8u) != E_OK)
    {
        goto restore;
    }

    /* Wait for RX */
    timeout = 1000000u;
    while (timeout > 0u)
    {
        if (Can_Hw_Receive(&rxId, rxData, &rxDlc) == TRUE)
        {
            if (rxId == 0x7FFu && rxDlc == 8u)
            {
                boolean match = TRUE;
                for (i = 0u; i < 8u; i++)
                {
                    if (rxData[i] != testData[i]) { match = FALSE; break; }
                }
                if (match)
                {
                    /* Restore normal mode */
                    CAN1->MCR |= MCR_INRQ;
                    timeout = CAN_TIMEOUT;
                    while (((CAN1->MSR & MSR_INAK) == 0u) && (timeout > 0u)) { timeout--; }
                    CAN1->BTR = BTR_500K_48MHZ;
                    CAN1->MCR &= ~MCR_INRQ;
                    timeout = CAN_TIMEOUT;
                    while (((CAN1->MSR & MSR_INAK) != 0u) && (timeout > 0u)) { timeout--; }
                    return E_OK;
                }
            }
        }
        timeout--;
    }

restore:
    CAN1->MCR |= MCR_INRQ;
    timeout = CAN_TIMEOUT;
    while (((CAN1->MSR & MSR_INAK) == 0u) && (timeout > 0u)) { timeout--; }
    CAN1->BTR = BTR_500K_48MHZ;
    CAN1->MCR &= ~MCR_INRQ;
    timeout = CAN_TIMEOUT;
    while (((CAN1->MSR & MSR_INAK) != 0u) && (timeout > 0u)) { timeout--; }
    return E_NOT_OK;
}

/* Bringup debug stub — referenced by HardFault handler in stm32f4xx_it.c */
