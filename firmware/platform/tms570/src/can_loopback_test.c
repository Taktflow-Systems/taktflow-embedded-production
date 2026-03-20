/**
 * @file    can_loopback_test.c
 * @brief   DCAN1 diagnostic — check RX + TX + error status
 *
 * Phase 1: Try to receive a frame (FZC sends 0x200 every 100ms)
 * Phase 2: If RX works, try TX
 *
 * LED6: solid ON = RX success, blinking = waiting for RX, OFF = error
 * LED7: solid ON = TX success, blinking = TX attempt, OFF = error
 */

#include "HL_sys_common.h"
#include "HL_system.h"
#include "HL_gio.h"
#include "HL_can.h"

static void delay(volatile uint32 count)
{
    while (count > 0u) { count--; }
}

/* Blink LED N times then pause — for error code display */
static void blink_code(uint8 pin, uint8 count)
{
    uint8 i;
    for (i = 0u; i < count; i++) {
        gioPORTB->DSET = (1u << pin);
        delay(3000000u);
        gioPORTB->DCLR = (1u << pin);
        delay(3000000u);
    }
    delay(10000000u);
}

void _c_int00(void)
{
    uint8 rx_data[8];
    uint8 tx_data[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0u, 0u, 0u, 0u};
    uint32 es;
    uint32 rx_result;
    uint32 tx_result;
    volatile uint32 timeout;

    systemInit();
    gioInit();

    gioPORTB->DIR |= (1u << 6u) | (1u << 7u);
    gioPORTB->DCLR = (1u << 6u) | (1u << 7u);

    canInit();

    /* Read DCAN1 Error Status immediately after init */
    es = canREG1->ES;

    /* Blink error status on LED6: number of blinks = ES lower nibble */
    if (es & 0x80u) {
        /* Bus-off — blink 8 times fast */
        blink_code(6u, 8u);
    } else if (es & 0x20u) {
        /* Error passive — blink 5 times */
        blink_code(6u, 5u);
    } else if (es & 0x07u) {
        /* LEC error — blink LEC count */
        blink_code(6u, (uint8)(es & 0x07u));
    } else {
        /* No error — solid ON briefly */
        gioPORTB->DSET = (1u << 6u);
        delay(5000000u);
        gioPORTB->DCLR = (1u << 6u);
    }

    /* Phase 1: RX test — try to receive any frame for 5 seconds
     * Mailbox 2 should be configured as RX by HALCoGen (accept ID=16) */
    timeout = 500u;  /* 500 * 10ms = 5s */
    while (timeout > 0u) {
        rx_result = canGetData(canREG1, canMESSAGE_BOX2, rx_data);
        if (rx_result != 0u) {
            /* Received a frame! */
            gioPORTB->DSET = (1u << 6u);  /* LED6 solid ON = RX works */
            break;
        }
        delay(1000000u);  /* ~10ms */
        timeout--;
        /* Blink LED6 while waiting */
        if (timeout & 1u) {
            gioPORTB->DSET = (1u << 6u);
        } else {
            gioPORTB->DCLR = (1u << 6u);
        }
    }

    if (timeout == 0u) {
        /* No RX in 5 seconds — transceiver or wiring issue */
        /* Show current ES register as blink code on LED7 */
        es = canREG1->ES;
        for (;;) {
            blink_code(7u, (uint8)((es & 0x07u) + 1u));
        }
    }

    /* Phase 2: TX test — configure mailbox 1 as TX and send */
    while ((canREG1->IF1STAT & 0x80u) != 0u) {}
    canREG1->IF1MSK  = 0xC0000000u | (0x7FFu << 18u);
    canREG1->IF1ARB  = 0x80000000u | 0x20000000u | (0x013u << 18u);
    canREG1->IF1MCTL = 0x00001000u | 4u;
    canREG1->IF1CMD  = 0xF8u;
    canREG1->IF1NO   = 1u;
    while ((canREG1->IF1STAT & 0x80u) != 0u) {}

    for (;;) {
        tx_result = canTransmit(canREG1, canMESSAGE_BOX1, tx_data);
        if (tx_result != 0u) {
            gioPORTB->DSET = (1u << 7u);  /* LED7 ON = TX queued */
        } else {
            gioPORTB->DCLR = (1u << 7u);  /* LED7 OFF = TX failed */
        }
        tx_data[4]++;
        delay(5000000u);

        /* Check ES after TX */
        es = canREG1->ES;
        if (es & 0x80u) {
            /* Bus-off — stop */
            gioPORTB->DCLR = (1u << 7u);
            for (;;) { blink_code(7u, 8u); }
        }
    }
}
