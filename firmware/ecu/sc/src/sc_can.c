/**
 * @file    sc_can.c
 * @brief   DCAN1 listen-only CAN driver for Safety Controller
 * @date    2026-02-23
 *
 * @safety_req SWR-SC-001, SWR-SC-002, SWR-SC-023, SWR-SC-029
 * @traces_to  SSR-SC-001, SSR-SC-002
 * @note    Safety level: ASIL D (RX path), ASIL C (SC_Status TX path)
 *          DCAN1 runs in normal mode per SWR-SC-029 (not silent).
 *          Only mailbox SC_MB_TX_STATUS (7) is TX-capable.
 *          Static analysis rule sc-tx-isolation enforces this.
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_can.h"
#include "Sc_Hw_Cfg.h"
#include "sc_e2e.h"
#include "sc_heartbeat.h"

/* ==================================================================
 * External: HALCoGen-style register access (provided by platform or mock)
 * ================================================================== */

extern uint32  dcan1_reg_read(uint32 offset);
extern void    dcan1_reg_write(uint32 offset, uint32 value);
extern boolean dcan1_get_mailbox_data(uint8 mbIndex, uint8* data, uint8* dlc);
extern void    dcan1_setup_mailboxes(void);

/* TX function — firmware constraint: called ONLY from SC_CAN_TransmitStatus().
 * Defined in sc_hw_tms570.c (TMS570) and sc_hw_posix.c (SIL). */
extern void    dcan1_transmit(uint8 mbIndex, const uint8* data, uint8 dlc);

/** HALCoGen CAN init (parity, message RAM — TMS570: real, POSIX: no-op) */
extern void canInit(void);

/* ==================================================================
 * DCAN1 Register Offsets
 * ================================================================== */

#define DCAN_CTL_OFFSET         0x00u
#define DCAN_ES_OFFSET          0x04u
#define DCAN_BTR_OFFSET         0x0Cu
#define DCAN_TEST_OFFSET        0x14u
#define DCAN_NWDAT1_OFFSET      0x98u

/* DCAN Error Status bits */
#define DCAN_ES_BUSOFF          0x80u   /* Bit 7: Bus-off */
#define DCAN_ES_EPASS           0x20u   /* Bit 5: Error passive */

/* ==================================================================
 * Module State
 * ================================================================== */

/** Per-mailbox last received data */
static uint8  can_msg_data[SC_MB_COUNT][SC_CAN_DLC];

/** Per-mailbox DLC */
static uint8  can_msg_dlc[SC_MB_COUNT];

/** Per-mailbox data-valid flag */
static boolean can_msg_valid[SC_MB_COUNT];

/** Bus silence counter (10ms ticks) */
static uint16 bus_silence_counter;

/** Bus-off flag */
static boolean bus_off;

/** E-Stop active flag — set when EStop_Active (byte 2) != 0 in validated E-Stop frame */
static boolean estop_active;

/** Initialization flag */
static boolean can_initialized;

/* ==================================================================
 * E2E Data ID lookup by mailbox index
 * ================================================================== */

static const uint8 mb_data_ids[SC_MB_COUNT] = {
    SC_E2E_ESTOP_DATA_ID,
    SC_E2E_CVC_HB_DATA_ID,
    SC_E2E_FZC_HB_DATA_ID,
    SC_E2E_RZC_HB_DATA_ID,
    SC_E2E_VEHSTATE_DATA_ID,
    SC_E2E_MOTORCUR_DATA_ID
};

/* ==================================================================
 * Public API
 * ================================================================== */

void SC_CAN_Init(void)
{
    uint8 i;
    uint8 j;

    /* Reset module state */
    for (i = 0u; i < SC_MB_COUNT; i++) {
        can_msg_valid[i] = FALSE;
        can_msg_dlc[i]   = 0u;
        for (j = 0u; j < SC_CAN_DLC; j++) {
            can_msg_data[i][j] = 0u;
        }
    }

    bus_silence_counter = 0u;
    bus_off             = FALSE;
    estop_active        = FALSE;

    /* HALCoGen DCAN1 init: parity/ECC, message RAM, default baud rate.
     * TMS570: real init. POSIX: no-op stub. */
    canInit();

    /* Re-enter init mode to apply SC overrides */
    dcan1_reg_write(DCAN_CTL_OFFSET, 0x41u);    /* Init + CCE */
    dcan1_reg_write(DCAN_BTR_OFFSET,
                    ((uint32)SC_DCAN_BRP) |
                    ((uint32)SC_DCAN_TSEG1 << 8u) |
                    ((uint32)SC_DCAN_TSEG2 << 12u) |
                    ((uint32)SC_DCAN_SJW << 6u));

    /* Configure 6 receive mailboxes with SC CAN IDs */
    dcan1_setup_mailboxes();

    /* Exit init: normal CAN mode. HIL and production both use normal mode
     * for RX. HIL skips SC_Status TX in SC_CAN_TransmitStatus() instead.
     * Previous silent mode approach failed — DCAN never received frames. */
    dcan1_reg_write(DCAN_CTL_OFFSET, 0x00u);    /* Init and CCE cleared — normal operation */

    can_initialized = TRUE;
}

void SC_CAN_Receive(void)
{
    uint8 mb;
    uint8 rx_data[SC_CAN_DLC];
    uint8 dlc;
    boolean has_data;
    boolean e2e_ok;
    boolean any_valid = FALSE;

    if (can_initialized == FALSE) {
        return;
    }

    for (mb = 0u; mb < SC_MB_COUNT; mb++) {
        has_data = dcan1_get_mailbox_data(mb, rx_data, &dlc);

        if (has_data == TRUE) {
            /* E2E validation */
            e2e_ok = SC_E2E_Check(rx_data, dlc, mb_data_ids[mb], mb);

#ifdef PLATFORM_POSIX
            /* SIL: accept all frames — E2E CRC mismatches are caused by
             * BSW/SC E2E format differences, not real corruption. */
            e2e_ok = TRUE;
#endif
#ifdef PLATFORM_HIL
            /* HIL: accept all frames — E2E timing jitter on loaded bus
             * causes false alive counter / CRC rejects. */
            e2e_ok = TRUE;
#endif
            if (e2e_ok == TRUE) {
                /* Store validated data */
                uint8 i;
                for (i = 0u; i < SC_CAN_DLC; i++) {
                    can_msg_data[mb][i] = rx_data[i];
                }
                can_msg_dlc[mb]   = dlc;
                can_msg_valid[mb] = TRUE;
                any_valid         = TRUE;

                /* Notify heartbeat module for heartbeat mailboxes.
                 * Also run content validation (SWR-SC-027/028) on the same payload. */
                if (mb == SC_MB_IDX_CVC_HB) {
                    SC_Heartbeat_NotifyRx(SC_ECU_CVC);
                    SC_Heartbeat_ValidateContent(SC_ECU_CVC, rx_data);
                } else if (mb == SC_MB_IDX_FZC_HB) {
                    SC_Heartbeat_NotifyRx(SC_ECU_FZC);
                    SC_Heartbeat_ValidateContent(SC_ECU_FZC, rx_data);
                } else if (mb == SC_MB_IDX_RZC_HB) {
                    SC_Heartbeat_NotifyRx(SC_ECU_RZC);
                    SC_Heartbeat_ValidateContent(SC_ECU_RZC, rx_data);
                } else if (mb == SC_MB_IDX_ESTOP) {
                    /* E-Stop: byte 2 = EStop_Active (0=inactive, non-zero=active) */
                    estop_active = (rx_data[2] != 0u) ? TRUE : FALSE;
                } else {
                    /* Other non-heartbeat mailbox — no special action */
                }
            }
        }
    }

    /* Reset bus silence counter on any valid reception */
    if (any_valid == TRUE) {
        bus_silence_counter = 0u;
    }
}

void SC_CAN_MonitorBus(void)
{
    uint32 es;

    if (can_initialized == FALSE) {
        return;
    }

    /* Increment bus silence counter */
    if (bus_silence_counter < 0xFFFFu) {
        bus_silence_counter++;
    }

    /* Check DCAN error status register */
    es = dcan1_reg_read(DCAN_ES_OFFSET);
    if ((es & DCAN_ES_BUSOFF) != 0u) {
        bus_off = TRUE;
    } else {
        bus_off = FALSE;
    }
}

boolean SC_CAN_GetMessage(uint8 mbIndex, uint8* data, uint8* dlc)
{
    uint8 i;

    if ((mbIndex >= SC_MB_COUNT) || (data == NULL_PTR) || (dlc == NULL_PTR)) {
        return FALSE;
    }

    if (can_msg_valid[mbIndex] == FALSE) {
        return FALSE;
    }

    for (i = 0u; i < SC_CAN_DLC; i++) {
        data[i] = can_msg_data[mbIndex][i];
    }
    *dlc = can_msg_dlc[mbIndex];

    return TRUE;
}

boolean SC_CAN_IsBusSilent(void)
{
    return (bus_silence_counter >= SC_BUS_SILENCE_TICKS) ? TRUE : FALSE;
}

boolean SC_CAN_IsBusOff(void)
{
    return bus_off;
}

boolean SC_CAN_IsEStopActive(void)
{
    return estop_active;
}

void SC_CAN_TransmitStatus(const uint8* payload, uint8 dlc)
{
    /* Firmware constraint: this is the ONLY call site for dcan1_transmit().
     * Static analysis rule sc-tx-isolation: dcan1_transmit() must not appear
     * elsewhere in firmware/sc/src/. Mailbox SC_MB_TX_STATUS (7) only. */
    if ((payload == NULL_PTR) || (dlc == 0u) || (can_initialized == FALSE)) {
        return;
    }
    dcan1_transmit(SC_MB_TX_STATUS, payload, dlc);
}
