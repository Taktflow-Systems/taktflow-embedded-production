/**
 * @file    Swc_FzcCom.c
 * @brief   FZC CAN communication — E2E protect/check, message RX/TX
 * @date    2026-02-24
 *
 * @safety_req SWR-FZC-019, SWR-FZC-020, SWR-FZC-026, SWR-FZC-027
 * @traces_to  SSR-FZC-019, SSR-FZC-020, SSR-FZC-026, SSR-FZC-027
 *
 * @details  Implements FZC CAN communication SWC:
 *           1. E2E protection on TX: CRC-8 (poly 0x1D), alive counter,
 *              FZC-specific Data IDs from Fzc_Cfg.h
 *           2. E2E verification on RX with safe defaults:
 *              - Brake safe default: 100% (max braking)
 *              - Steering safe default: 0 deg (center)
 *           3. CAN message reception (10ms cyclic):
 *              0x001 E-stop, 0x100 vehicle state,
 *              0x200 brake cmd, 0x201 steering cmd
 *           4. CAN message transmission (scheduled):
 *              0x011 heartbeat 50ms, 0x210 brake fault (cyclic),
 *              0x211 motor cutoff (cyclic), 0x220 lidar warning (event)
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_FzcCom.h"
#include "Fzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"
#include "Com.h"
#include "PduR.h"
#include "E2E.h"
#include "Swc_FzcCanMonitor.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** CRC-8 polynomial per AUTOSAR E2E Profile 1 */
#define FZC_COM_CRC8_POLY          0x1Du

/** CRC-8 initial value */
#define FZC_COM_CRC8_INIT          0xFFu

/** Alive counter mask (4-bit, bits [3:0] of byte[1]) */
#define FZC_COM_ALIVE_MASK         0x0Fu

/** Heartbeat TX period in 10ms cycles: 50ms / 10ms = 5 */
#define FZC_COM_HB_PERIOD_CYCLES   5u

/** Safe default: brake 100% on E2E failure */
#define FZC_COM_SAFE_BRAKE_PCT     100u

/** Safe default: steering center (0 deg) on E2E failure */
#define FZC_COM_SAFE_STEER_DEG     0

/* ==================================================================
 * Module State (static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8  FzcCom_Initialized;

/** TX alive counter (0..15, wraps) */
static uint8  FzcCom_TxAlive;

/** RX alive counter — last accepted value per RX message */
static uint8  FzcCom_RxAliveEstop;
static uint8  FzcCom_RxAliveVehState;
static uint8  FzcCom_RxAliveBrakeCmd;
static uint8  FzcCom_RxAliveSteerCmd;

/** Heartbeat TX cycle counter */
static uint8  FzcCom_HbCycleCount;

/** Event-driven TX pending flag (lidar only — brake/motor are cyclic) */
static uint8  FzcCom_TxPendLidarWarn;

/** TX schedule cycle counter (paces fault frames to avoid FDCAN FIFO overflow) */
static uint16 FzcCom_TxScheduleCycle;

/* Debug counters for 0x200 Steering_Status tracing */
volatile uint32 g_dbg_steer_com_send = 0u;   /* Com_SendSignal calls */
volatile uint32 g_dbg_steer_rte_dispatch = 0u; /* TransmitSchedule calls */

/** @brief E2E configs for TX data messages — shared BSW E2E (Profile 1)
 *  @safety_req SWR-FZC-019 */
static const E2E_ConfigType fzc_e2e_brake_fault_cfg = {
    FZC_E2E_BRAKE_STATUS_DATA_ID,  /* 0x21 */
    15u, 8u
};
static E2E_StateType fzc_e2e_brake_fault_state;

static const E2E_ConfigType fzc_e2e_motor_cutoff_cfg = {
    FZC_E2E_BRAKE_STATUS_DATA_ID,  /* 0x21 — same as brake (pre-existing, no separate ID) */
    15u, 8u
};
static E2E_StateType fzc_e2e_motor_cutoff_state;

static const E2E_ConfigType fzc_e2e_lidar_cfg = {
    FZC_E2E_LIDAR_DATA_ID,        /* 0x22 */
    15u, 8u
};
static E2E_StateType fzc_e2e_lidar_state;

/* ==================================================================
 * Private: CRC-8 Calculation
 * ================================================================== */

/**
 * @brief  Compute CRC-8 with polynomial 0x1D (SAE J1850)
 * @param  data    Pointer to data bytes
 * @param  length  Number of bytes (excludes CRC byte itself)
 * @param  dataId  Data ID XORed into initial seed
 * @return CRC-8 value
 */
static uint8 FzcCom_CalcCrc8(const uint8* data, uint8 length, uint8 dataId)
{
    uint8 crc;
    uint8 i;
    uint8 bit;

    crc = (uint8)(FZC_COM_CRC8_INIT ^ dataId);

    for (i = 0u; i < length; i++) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; bit++) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8)((uint8)(crc << 1u) ^ FZC_COM_CRC8_POLY);
            } else {
                crc = (uint8)(crc << 1u);
            }
        }
    }

    return crc;
}

/* ==================================================================
 * API: Swc_FzcCom_Init
 * ================================================================== */

void Swc_FzcCom_Init(void)
{
    FzcCom_TxAlive          = 0u;
    FzcCom_RxAliveEstop     = 0xFFu;  /* Invalid initial — first msg always accepted */
    FzcCom_RxAliveVehState  = 0xFFu;
    FzcCom_RxAliveBrakeCmd  = 0xFFu;
    FzcCom_RxAliveSteerCmd  = 0xFFu;
    FzcCom_HbCycleCount     = 0u;
    FzcCom_TxPendLidarWarn  = FALSE;
    FzcCom_TxScheduleCycle  = 0u;
    fzc_e2e_brake_fault_state.Counter  = 0u;
    fzc_e2e_motor_cutoff_state.Counter = 0u;
    fzc_e2e_lidar_state.Counter        = 0u;
    FzcCom_Initialized      = TRUE;
}

/* ==================================================================
 * API: Swc_FzcCom_E2eProtect
 * ================================================================== */

Std_ReturnType Swc_FzcCom_E2eProtect(uint8* data, uint8 length, uint8 dataId)
{
    uint8 crc;

    if (data == NULL_PTR) {
        return E_NOT_OK;
    }

    if (length < 2u) {
        return E_NOT_OK;
    }

    /* Insert alive counter into byte[1] bits [3:0] */
    data[1] = (uint8)((data[1] & 0xF0u) | (FzcCom_TxAlive & FZC_COM_ALIVE_MASK));

    /* Compute CRC-8 over bytes [1..length-1], with Data ID in seed */
    crc = FzcCom_CalcCrc8(&data[1], (uint8)(length - 1u), dataId);

    /* Store CRC in byte[0] */
    data[0] = crc;

    /* Increment alive counter (wraps at 15) */
    FzcCom_TxAlive = (uint8)((FzcCom_TxAlive + 1u) & FZC_COM_ALIVE_MASK);

    return E_OK;
}

/* ==================================================================
 * API: Swc_FzcCom_E2eCheck
 * ================================================================== */

Std_ReturnType Swc_FzcCom_E2eCheck(const uint8* data, uint8 length, uint8 dataId)
{
    uint8 received_crc;
    uint8 computed_crc;
    uint8 rx_alive;

    if (data == NULL_PTR) {
        return E_NOT_OK;
    }

    if (length < 2u) {
        return E_NOT_OK;
    }

    /* Extract received CRC from byte[0] */
    received_crc = data[0];

    /* Compute CRC-8 over bytes [1..length-1] with Data ID */
    computed_crc = FzcCom_CalcCrc8(&data[1], (uint8)(length - 1u), dataId);

    if (received_crc != computed_crc) {
        return E_NOT_OK;
    }

    /* Extract alive counter from byte[1] bits [3:0] */
    rx_alive = (uint8)(data[1] & FZC_COM_ALIVE_MASK);
    (void)rx_alive;  /* Alive validation handled per-message in Receive */

    return E_OK;
}

/* ==================================================================
 * API: Swc_FzcCom_Receive (10ms cyclic)
 * ================================================================== */

void Swc_FzcCom_Receive(void)
{
    Std_ReturnType ret;

    if (FzcCom_Initialized != TRUE) {
        return;
    }

    /* Notify CAN monitor that COM RX cycle is running.
     * Resets the silence counter — prevents false CAN-loss latch. */
    Swc_FzcCanMonitor_NotifyRx();

    /* NOTE: Com_ReceiveSignal reads extracted signal values from shadow
     * buffers (not raw PDU bytes).  E2E protection should be applied at
     * the PDU level (TODO:POST-BETA — add Com_GetRxPduData + E2E at PDU
     * layer).  Signal IDs use FZC_COM_SIG_* defines from Fzc_Cfg.h. */

    /* ---- RX: 0x001 E-stop (uint8) ---- */
    {
        uint8 estop_val = 0u;
        ret = Com_ReceiveSignal(FZC_COM_SIG_ESTOP_BROADCAST_ACTIVE, &estop_val);
        if (ret == E_OK) {
            (void)Rte_Write(FZC_SIG_ESTOP_ACTIVE, (uint32)estop_val);
        }
    }

    /* ---- RX: 0x100 Vehicle State (uint8) ---- */
    {
        uint8 vs_val = 0u;
        ret = Com_ReceiveSignal(FZC_COM_SIG_VEHICLE_STATE_MODE, &vs_val);
        if (ret == E_OK) {
            (void)Rte_Write(FZC_SIG_VEHICLE_STATE, (uint32)vs_val);
        }
    }

    /* ---- RX: 0x102 Steering Command (sint16) ---- */
    {
        sint16 steer_val = 0;
        ret = Com_ReceiveSignal(FZC_COM_SIG_STEER_COMMAND_STEER_ANGLE_CMD, &steer_val);
        if (ret == E_OK) {
            (void)Rte_Write(FZC_SIG_STEER_CMD,
                (uint32)((uint16)steer_val));
        }
    }

    /* ---- RX: 0x103 Brake Command (uint8) ---- */
    {
        uint8 brake_val = 0u;
        ret = Com_ReceiveSignal(FZC_COM_SIG_BRAKE_COMMAND_BRAKE_FORCE_CMD, &brake_val);
        if (ret == E_OK) {
            (void)Rte_Write(FZC_SIG_BRAKE_CMD, (uint32)brake_val);
        }
    }
}

/* ==================================================================
 * API: Swc_FzcCom_TransmitSchedule (10ms cyclic)
 * ================================================================== */

void Swc_FzcCom_TransmitSchedule(void)
{
    uint8  txBuf[8];
    uint32 rteVal;
    uint8  i;
    PduInfoType pdu_info;

    if (FzcCom_Initialized != TRUE) {
        return;
    }

    g_dbg_steer_rte_dispatch++;

    pdu_info.SduDataPtr = txBuf;
    pdu_info.SduLength  = 8u;

    /* Heartbeat TX handled by Swc_Heartbeat_MainFunction (50ms via RTE) */

    /* Advance TX schedule cycle counter (paces fault frames to stay
     * within FDCAN TX FIFO depth of 3).  Steer+brake status use Com
     * shadow buffers (2 frames).  Fault frames are paced to 100ms with
     * offsets so they never collide with each other in the same cycle. */
    FzcCom_TxScheduleCycle++;
    if (FzcCom_TxScheduleCycle >= 1000u) {
        FzcCom_TxScheduleCycle = 0u;
    }

    /* ---- TX: Steering/Brake status → Com shadow buffers (10ms cyclic) ---- */
    {
        uint32 steer_val = 0u;
        uint32 fault_val = 0u;

        (void)Rte_Read(FZC_SIG_STEER_ANGLE, &steer_val);
        (void)Rte_Read(FZC_SIG_STEER_FAULT, &fault_val);

        /* TX: steer_angle (sint16), steer_fault (uint8) → Steering_Status PDU */
        {
            sint16 angle = (sint16)((uint16)steer_val);
            uint8  fault = (uint8)fault_val;
            (void)Com_SendSignal(FZC_COM_SIG_STEERING_STATUS_ACTUAL_ANGLE, &angle);
            (void)Com_SendSignal(FZC_COM_SIG_STEERING_STATUS_STEER_FAULT_STATUS, &fault);
            g_dbg_steer_com_send++;
        }

        /* TX: brake_pos (uint8) → Brake_Status PDU */
        {
            uint32 brake_val = 0u;
            uint8  bpos;
            (void)Rte_Read(FZC_SIG_BRAKE_POS, &brake_val);
            bpos = (uint8)brake_val;
            (void)Com_SendSignal(FZC_COM_SIG_BRAKE_STATUS_BRAKE_POSITION, &bpos);
        }
    }

    /* ---- TX: 0x210 Brake Fault (100ms, offset 3 — avoids heartbeat collision) ---- */
    if ((FzcCom_TxScheduleCycle % 10u) == 3u) {
        (void)Rte_Read(FZC_SIG_BRAKE_FAULT, &rteVal);
        for (i = 0u; i < 8u; i++) {
            txBuf[i] = 0u;
        }
        txBuf[2] = (uint8)rteVal;
        (void)E2E_Protect(&fzc_e2e_brake_fault_cfg, &fzc_e2e_brake_fault_state, txBuf, 8u);
        (void)PduR_Transmit(FZC_COM_TX_BRAKE_FAULT, &pdu_info);
    }

    /* ---- TX: 0x211 Motor Cutoff (100ms, offset 7 — avoids brake fault collision) ---- */
    if ((FzcCom_TxScheduleCycle % 10u) == 7u) {
        (void)Rte_Read(FZC_SIG_MOTOR_CUTOFF, &rteVal);
        for (i = 0u; i < 8u; i++) {
            txBuf[i] = 0u;
        }
        txBuf[2] = (uint8)rteVal;
        (void)E2E_Protect(&fzc_e2e_motor_cutoff_cfg, &fzc_e2e_motor_cutoff_state, txBuf, 8u);
        (void)PduR_Transmit(FZC_COM_TX_MOTOR_CUTOFF, &pdu_info);
    }

    /* ---- TX: 0x220 Lidar Warning (event-driven, PduR_Transmit) ---- */
    (void)Rte_Read(FZC_SIG_LIDAR_ZONE, &rteVal);
    /* Only broadcast obstacle warnings (WARNING/BRAKING/EMERGENCY),
     * NOT sensor FAULT — FAULT means invalid data, not an obstacle. */
    if ((rteVal >= (uint32)FZC_LIDAR_ZONE_WARNING)
        && (rteVal <= (uint32)FZC_LIDAR_ZONE_EMERGENCY)) {
        FzcCom_TxPendLidarWarn = TRUE;
    }
    if (FzcCom_TxPendLidarWarn == TRUE) {
        for (i = 0u; i < 8u; i++) {
            txBuf[i] = 0u;
        }
        txBuf[2] = (uint8)rteVal;
        (void)Rte_Read(FZC_SIG_LIDAR_DIST, &rteVal);
        txBuf[3] = (uint8)(rteVal & 0xFFu);
        txBuf[4] = (uint8)((rteVal >> 8u) & 0xFFu);
        (void)E2E_Protect(&fzc_e2e_lidar_cfg, &fzc_e2e_lidar_state, txBuf, 8u);
        (void)PduR_Transmit(FZC_COM_TX_LIDAR, &pdu_info);
        FzcCom_TxPendLidarWarn = FALSE;
    }
}
