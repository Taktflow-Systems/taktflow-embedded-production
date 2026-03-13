/**
 * @file    sc_monitoring.c
 * @brief   SC_Status broadcast for Safety Controller (GAP-1/2 hardening)
 * @author  System
 * @date    2026-03-07
 *
 * @safety_req SWR-SC-029, SWR-SC-030
 * @traces_to  plan-sc-can-hardening.md
 * @note    Safety level: ASIL C — diagnostic TX, not on the ASIL D RX path.
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_monitoring.h"
#include "Sc_Hw_Cfg.h"
#include "sc_can.h"
#include "sc_e2e.h"
#include "sc_heartbeat.h"
#include "sc_plausibility.h"
#include "sc_relay.h"
#include "sc_selftest.h"
#include "sc_state.h"

/* ==================================================================
 * Module State
 * ================================================================== */

/** Wrapping 8-bit alive counter — increments each SC_Status TX */
static uint8 mon_alive_counter;

/** Tick counter — increments each 10ms main loop iteration */
static uint8 mon_tick;

/* ==================================================================
 * Internal: Build SC_Status payload (SWR-SC-030 signal layout)
 *
 * Byte 0: SC_AliveCounter (8-bit)
 * Byte 1: SC_CRC8 over bytes 0, 2, 3
 * Byte 2: SC_Mode [3:0] | SC_FaultFlags [7:4]
 * Byte 3: ECU_Health [2:0] | FaultReason [6:3] | RelayState [7]
 * ================================================================== */

static void mon_build_payload(uint8* frame)
{
    uint8 sc_mode;
    uint8 fault_flags;
    uint8 ecu_health;
    uint8 fault_reason;
    uint8 relay_state;
    boolean killed;
    uint8 crc_input[3];

    killed = SC_Relay_IsKilled();

    /* SC operating mode — read from authoritative state machine (GAP-SC-006).
     * SC_STATE_KILL maps to SC_STATUS_MODE_SAFE_STOP for CAN broadcast. */
    {
        uint8 state = SC_State_Get();
        if (state == SC_STATE_KILL) {
            sc_mode = SC_STATUS_MODE_SAFE_STOP;
        } else if (state == SC_STATE_FAULT) {
            sc_mode = SC_STATUS_MODE_FAULT;
        } else if (state == SC_STATE_MONITORING) {
            sc_mode = SC_STATUS_MODE_MONITORING;
        } else {
            sc_mode = SC_STATUS_MODE_INIT;
        }
    }

    /* SC fault flags (which ECU is faulted) */
    fault_flags = 0u;
    if ((SC_Heartbeat_IsTimedOut(SC_ECU_CVC) == TRUE) ||
        (SC_Heartbeat_IsContentFault(SC_ECU_CVC) == TRUE)) {
        fault_flags |= SC_STATUS_FAULT_CVC_HB;
    }
    if ((SC_Heartbeat_IsTimedOut(SC_ECU_FZC) == TRUE) ||
        (SC_Heartbeat_IsContentFault(SC_ECU_FZC) == TRUE)) {
        fault_flags |= SC_STATUS_FAULT_FZC_HB;
    }
    if ((SC_Heartbeat_IsTimedOut(SC_ECU_RZC) == TRUE) ||
        (SC_Heartbeat_IsContentFault(SC_ECU_RZC) == TRUE)) {
        fault_flags |= SC_STATUS_FAULT_RZC_HB;
    }
    if (SC_Plausibility_IsFaulted() == TRUE) {
        fault_flags |= SC_STATUS_FAULT_PLAUS;
    }

    /* ECU health (bit set = healthy) */
    ecu_health = 0u;
    if (SC_Heartbeat_IsTimedOut(SC_ECU_CVC) == FALSE) { ecu_health |= 0x01u; }
    if (SC_Heartbeat_IsTimedOut(SC_ECU_FZC) == FALSE) { ecu_health |= 0x02u; }
    if (SC_Heartbeat_IsTimedOut(SC_ECU_RZC) == FALSE) { ecu_health |= 0x04u; }

    /* Fault reason — pass kill reason directly (4-bit enum, values 0-9).
     * Previously collapsed ESM/busoff/readback into SELFTEST (GAP-SC-007).
     * Content fault is visible through fault_flags per-ECU bits. */
    fault_reason = (killed == TRUE) ? (SC_Relay_GetKillReason() & 0x0Fu) : 0u;

    relay_state = (killed == FALSE) ? 1u : 0u;  /* 1=energized, 0=de-energized */

    /* Pack frame */
    frame[0] = mon_alive_counter;
    frame[2] = (uint8)(sc_mode & 0x0Fu) | (uint8)((fault_flags & 0x0Fu) << 4u);
    frame[3] = (uint8)(ecu_health & 0x07u)
             | (uint8)((fault_reason & 0x0Fu) << 3u)
             | (uint8)((relay_state & 0x01u) << 7u);

    /* CRC over bytes 0, 2, 3 (byte 1 is the CRC itself) */
    crc_input[0] = frame[0];
    crc_input[1] = frame[2];
    crc_input[2] = frame[3];
    frame[1] = SC_E2E_ComputeCRC8(crc_input, 3u);
}

/* ==================================================================
 * Public API
 * ================================================================== */

void SC_Monitoring_Init(void)
{
    mon_alive_counter = 0u;
    mon_tick          = 0u;
}

void SC_Monitoring_Update(void)
{
    uint8 frame[SC_RELAY_STATUS_DLC];  /* 4 bytes */

    mon_tick++;
    if (mon_tick < SC_MONITORING_TX_PERIOD) {
        return;  /* Not yet time to transmit */
    }
    mon_tick = 0u;

    mon_build_payload(frame);
    SC_CAN_TransmitStatus(frame, SC_RELAY_STATUS_DLC);

    /* Increment alive counter after TX so receivers detect a halt on the NEXT frame */
    mon_alive_counter++;
}
