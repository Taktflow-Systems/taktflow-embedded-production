/**
 * @file    Swc_CvcCom.c
 * @brief   CVC CAN communication — E2E protect/check, RX routing, TX scheduling
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-014, SWR-CVC-015, SWR-CVC-016, SWR-CVC-017
 * @traces_to  SSR-CVC-014, SSR-CVC-015, SSR-CVC-016, SSR-CVC-017,
 *             TSR-022, TSR-023, TSR-024
 *
 * @details  E2E Protect: CRC-8 0x1D, alive counter, Data ID per message
 *           E2E Check: CRC verify, alive counter verify, 3-fail safe default
 *           RX Routing: CAN ID table from FZC/RZC with E2E check
 *           TX Scheduling: CAN ID table with configurable periods
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR Com/E2E integration, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_CvcCom.h"
#include "Cvc_Cfg.h"
#include "Com.h"
#include "Rte.h"
#include "Swc_VehicleState.h"
/* NOTE: PduR.h and E2E.h intentionally NOT included here.
 * SWCs access CAN only through Com_SendSignal/Com_ReceiveSignal.
 * E2E protection is applied by Com_MainFunction_Tx (Phase 2).
 * Direct PduR_Transmit/E2E_Protect calls from SWCs are forbidden. */

/* SIL diagnostic logging — compile with -DSIL_DIAG to enable */
#ifdef SIL_DIAG
#include <stdio.h>
#define CVCCOM_DIAG(fmt, ...) (void)fprintf(stderr, "[CVC_COM] " fmt "\n", ##__VA_ARGS__)
#else
#define CVCCOM_DIAG(fmt, ...) ((void)0)
#endif

#define CVC_SAFE_BRAKE_CMD   100u   /**< Max brake for safe-state TX */

/* ==================================================================
 * Module State
 *
 * NOTE: Legacy TX/RX tables and E2E state arrays removed (Phase 2).
 * E2E protection is now handled by Com_MainFunction_Tx/Rx.
 * TX scheduling uses Com_SendSignal + PERIODIC mode in Com.
 * ================================================================== */

static uint8   CvcCom_Initialized;

/* ==================================================================
 * API: Swc_CvcCom_Init
 * ================================================================== */

void Swc_CvcCom_Init(void)
{
    CvcCom_Initialized = TRUE;
}

/* ==================================================================
 * NOTE: Swc_CvcCom_E2eProtect, E2eCheck, Receive removed (Phase 2).
 * E2E is now in Com_MainFunction_Tx/Rx. SWCs use Com_SendSignal only.
 * ================================================================== */

/* ==================================================================
 * API: Swc_CvcCom_TransmitSchedule
 * ================================================================== */

/**
 * @safety_req SWR-CVC-017
 */
void Swc_CvcCom_TransmitSchedule(uint32 currentTimeMs)
{
    (void)currentTimeMs;  /* Timing now handled by Com PERIODIC mode */

    if (CvcCom_Initialized != TRUE)
    {
        return;
    }

    /* ---- TX: Heartbeat signals → Com (50ms cycle enforced by Com config) ---- */
    {
        uint8 ecu_id = CVC_ECU_ID_CVC;
        /* Read state directly from VSM — avoids RTE signal latency that
         * could cause heartbeat to report stale state (e.g. INIT after
         * VSM already transitioned to DEGRADED in the same 10ms cycle). */
        uint8 mode = Swc_VehicleState_GetState();
        (void)Com_SendSignal(CVC_COM_SIG_CVC_HEARTBEAT_ECU_ID, &ecu_id);
        (void)Com_SendSignal(CVC_COM_SIG_CVC_HEARTBEAT_OPERATING_MODE, &mode);
    }

    /* ---- TX: 0x100 Vehicle State (E2E protected, cyclic) ---- */
    {
        uint8  txBuf[8];
        uint8  j;
        uint32 faultSig;
        uint16 faultMask = 0u;  /* 12-bit signal → must be uint16 for Com_SendSignal */

        for (j = 0u; j < 8u; j++) { txBuf[j] = 0u; }

        /* Byte 2: Vehicle state */
        txBuf[2] = Swc_VehicleState_GetState();

        /* Byte 3: Fault mask (composed from individual RTE signals) */
        (void)Rte_Read(CVC_SIG_ESTOP_ACTIVE, &faultSig);
        if (faultSig != 0u) { faultMask |= 0x01u; }
        (void)Rte_Read(CVC_SIG_SC_RELAY_KILL, &faultSig);
        if (faultSig == 0u) { faultMask |= 0x02u; }  /* 0=relay killed */
        (void)Rte_Read(CVC_SIG_MOTOR_CUTOFF, &faultSig);
        if (faultSig != 0u) { faultMask |= 0x04u; }
        (void)Rte_Read(CVC_SIG_BRAKE_FAULT, &faultSig);
        if (faultSig != 0u) { faultMask |= 0x08u; }
        (void)Rte_Read(CVC_SIG_STEERING_FAULT, &faultSig);
        if (faultSig != 0u) { faultMask |= 0x10u; }
        (void)Rte_Read(CVC_SIG_PEDAL_FAULT, &faultSig);
        if (faultSig != 0u) { faultMask |= 0x20u; }
        (void)Rte_Read(CVC_SIG_FZC_COMM_STATUS, &faultSig);
        if (faultSig == CVC_COMM_TIMEOUT) { faultMask |= 0x40u; }
        (void)Rte_Read(CVC_SIG_RZC_COMM_STATUS, &faultSig);
        if (faultSig == CVC_COMM_TIMEOUT) { faultMask |= 0x80u; }
        txBuf[3] = faultMask;

        /* Byte 4: Torque limit (scaled from 0-1000 RTE to 0-100%) */
        {
            uint32 torque = 0u;
            (void)Rte_Read(CVC_SIG_TORQUE_REQUEST, &torque);
            txBuf[4] = (uint8)(torque / 10u);
        }

        /* Write FaultMask to RTE for other consumers */
        (void)Rte_Write(CVC_SIG_FAULT_MASK, (uint32)faultMask);

        /* Pack Vehicle_State signals via Com (E2E applied by Com_MainFunction_Tx) */
        {
            uint8 vs_mode = Swc_VehicleState_GetState();
            uint8 tl8     = txBuf[4];
            (void)Com_SendSignal(CVC_COM_SIG_VEHICLE_STATE_MODE, &vs_mode);
            (void)Com_SendSignal(CVC_COM_SIG_VEHICLE_STATE_FAULT_MASK, &faultMask);
            (void)Com_SendSignal(CVC_COM_SIG_VEHICLE_STATE_TORQUE_LIMIT, &tl8);
        }
    }

    /* Publish steering and brake commands.
     * In SAFE_STOP / SHUTDOWN: override to safe-state values
     * (max brake, center steering) so FZC clears its fault latch. */
    {
        uint8  vs = Swc_VehicleState_GetState();
        sint16 tx_steer = 0;      /* Center = 0 degrees (plain degrees, no DBC) */
        uint8  tx_brake;

        if (vs >= CVC_STATE_SAFE_STOP)
        {
            tx_brake = CVC_SAFE_BRAKE_CMD;
        }
        else
        {
            tx_brake = 0u;
        }

        if (tx_brake != 0u) {
            CVCCOM_DIAG("TX brake=%u vs=%u", (unsigned)tx_brake, (unsigned)vs);
        }
        (void)Com_SendSignal(CVC_COM_SIG_STEER_COMMAND_STEER_ANGLE_CMD, &tx_steer);
        (void)Com_SendSignal(CVC_COM_SIG_BRAKE_COMMAND_BRAKE_FORCE_CMD, &tx_brake);
    }

    /* Bridge E-Stop broadcast — always update signals (PERIODIC PDU).
     * Com_MainFunction_Tx sends 0x001 on timer regardless of pending. */
    {
        uint32 estop_val = 0u;
        uint8  estop_active;
        uint8  estop_source = 1u;  /* CVC = source 1 */
        (void)Rte_Read(CVC_SIG_ESTOP_ACTIVE, &estop_val);
        estop_active = (estop_val != 0u) ? 1u : 0u;
        (void)Com_SendSignal(CVC_COM_SIG_ESTOP_BROADCAST_ACTIVE, &estop_active);
        (void)Com_SendSignal(CVC_COM_SIG_ESTOP_BROADCAST_SOURCE, &estop_source);
    }

    /* Bridge Body_Control_Cmd (0x350, PERIODIC 100ms, QM)
     * Signals populated from RTE body control state.
     * Com_MainFunction_Tx handles timing — just keep signals fresh. */
    {
        uint8 headlight  = 0u;
        uint8 taillight  = 0u;
        uint8 hazard     = 0u;
        uint8 turn_sig   = 0u;
        uint8 door_lock  = 0u;
        /* TODO:POST-BETA — read actual body control state from RTE */
        (void)Com_SendSignal(CVC_COM_SIG_BODY_CONTROL_CMD_HEADLIGHT_CMD, &headlight);
        (void)Com_SendSignal(CVC_COM_SIG_BODY_CONTROL_CMD_TAIL_LIGHT_ON, &taillight);
        (void)Com_SendSignal(CVC_COM_SIG_BODY_CONTROL_CMD_HAZARD_ACTIVE, &hazard);
        (void)Com_SendSignal(CVC_COM_SIG_BODY_CONTROL_CMD_TURN_SIGNAL_CMD, &turn_sig);
        (void)Com_SendSignal(CVC_COM_SIG_BODY_CONTROL_CMD_DOOR_LOCK_CMD, &door_lock);
    }

    /* Bridge Torque Request — read from RTE, send via Com */
    {
        uint32 torque_val = 0u;
        uint16 tx_torque;
        (void)Rte_Read(CVC_SIG_TORQUE_REQUEST, &torque_val);
        tx_torque = (uint16)torque_val;
        (void)Com_SendSignal(CVC_COM_SIG_TORQUE_REQUEST_COMMAND_PCT, &tx_torque);
    }
}

/* ==================================================================
 * API: Swc_CvcCom_BridgeRxToRte
 *
 * Bridges Com RX fault signals to RTE so VehicleState can read them.
 * Called periodically from the main 10ms task.
 * ================================================================== */

void Swc_CvcCom_BridgeRxToRte(void)
{
    uint8  brake_fault_val  = 0u;
    uint8  motor_cutoff_val = 0u;

    if (CvcCom_Initialized != TRUE)
    {
        return;
    }

    uint8  sc_relay_byte3 = 0x80u;   /* Default: bit7=1 (energized) */

    uint8  battery_status_val = 2u;  /* Default NORMAL if read fails */

    uint8  steering_fault_val = 0u;

    uint8  motor_fault_rzc_val = 0u;

    /* Read fault signals from Com shadow buffers */
    (void)Com_ReceiveSignal(CVC_COM_SIG_BRAKE_FAULT_FAULT_TYPE, &brake_fault_val);
    (void)Com_ReceiveSignal(CVC_COM_SIG_MOTOR_CUTOFF_REQ_REQUEST_TYPE, &motor_cutoff_val);
#ifdef SIL_DIAG
    {
        static uint8 prev_mc = 0xFFu;
        if (motor_cutoff_val != prev_mc) {
            fprintf(stderr, "[BRIDGE] mc=%u bf=%u sig=%u\n",
                    motor_cutoff_val, brake_fault_val,
                    (unsigned)CVC_COM_SIG_MOTOR_CUTOFF_REQ_REQUEST_TYPE);
            prev_mc = motor_cutoff_val;
        }
    }
#endif
    (void)Com_ReceiveSignal(CVC_COM_SIG_SC_STATUS_RELAY_ENERGIZED, &sc_relay_byte3);
    /* Com extracts the 1-bit RelayState signal as 0 or 1.
     * 1=energized (OK), 0=de-energized (killed).
     * On PDU timeout Com zeros shadow → 0 → kill. */
    {
        (void)Rte_Write(CVC_SIG_SC_RELAY_KILL, (uint32)sc_relay_byte3);
    }
    (void)Com_ReceiveSignal(CVC_COM_SIG_BATTERY_STATUS_LEVEL, &battery_status_val);
    (void)Com_ReceiveSignal(CVC_COM_SIG_STEERING_STATUS_STEER_FAULT_STATUS, &steering_fault_val);
    (void)Com_ReceiveSignal(CVC_COM_SIG_MOTOR_STATUS_MOTOR_FAULT_STATUS, &motor_fault_rzc_val);

    /* Bridge heartbeat alive counters to RTE for Swc_Heartbeat to poll.
     * Heartbeat comm STATUS is owned by Swc_Heartbeat — only bridge
     * the raw alive counter values, not the comm status. */
    {
        uint8 fzc_alive = 0u;
        uint8 rzc_alive = 0u;
        (void)Com_ReceiveSignal(CVC_COM_SIG_FZC_HEARTBEAT_E_2_E_ALIVE_COUNTER, &fzc_alive);
        (void)Com_ReceiveSignal(CVC_COM_SIG_RZC_HEARTBEAT_E_2_E_ALIVE_COUNTER, &rzc_alive);
        (void)Rte_Write(CVC_SIG_FZC_HEARTBEAT_E_2_E_ALIVE_COUNTER, (uint32)fzc_alive);
        (void)Rte_Write(CVC_SIG_RZC_HEARTBEAT_E_2_E_ALIVE_COUNTER, (uint32)rzc_alive);
    }

    /* Bridge fault signals to RTE for VehicleState to consume.
     * Heartbeat comm status is owned exclusively by Swc_Heartbeat.c
     * — do NOT write CVC_SIG_FZC/RZC_COMM_STATUS here. */
    (void)Rte_Write(CVC_SIG_BRAKE_FAULT,    (uint32)brake_fault_val);
    (void)Rte_Write(CVC_SIG_MOTOR_CUTOFF,   (uint32)motor_cutoff_val);
    /* SC_RELAY_KILL already written above after bit extraction */
    (void)Rte_Write(CVC_SIG_BATTERY_STATUS, (uint32)battery_status_val);
    (void)Rte_Write(CVC_SIG_STEERING_FAULT, (uint32)steering_fault_val);
    (void)Rte_Write(CVC_SIG_MOTOR_FAULT_RZC, (uint32)motor_fault_rzc_val);

    /* E-Stop injection: handled by Spi_Hw_PollUdp -> IoHwAb DIO ch 5.
     * Previous CAN-based injection (Com signal 19u) was resetting DIO5
     * to LOW every cycle because the signal ID was stale. Removed. */
}

/* GetRxStatus removed — RX status now tracked by Com E2E SM (Com_GetRxPduQuality). */
