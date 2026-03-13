/**
 * @file    Swc_RzcCom.c
 * @brief   RZC CAN communication -- E2E protection, message RX/TX tables
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-019, SWR-RZC-020, SWR-RZC-026, SWR-RZC-027
 * @traces_to  SSR-RZC-019, SSR-RZC-020, SSR-RZC-026, SSR-RZC-027
 *
 * @details  Implements RZC CAN communication:
 *           1.  E2E transmit: CRC-8 polynomial 0x1D over payload bytes
 *               1..7 XOR'd with RZC-specific Data ID, alive counter in
 *               byte 1 bits [3:0], CRC in byte 0. 16-entry alive counter
 *               array indexed by PDU ID.
 *           2.  E2E receive: CRC verify + alive counter monotonic check.
 *               3 consecutive failures -> safe default = zero torque for
 *               torque command messages.
 *           3.  CAN RX table (10ms cyclic):
 *               0x001 E-stop broadcast -> set RZC_SIG_ESTOP_ACTIVE
 *               0x100 Vehicle_State + Torque -> write to RTE, 100ms timeout
 *           4.  CAN TX schedule (10ms cyclic):
 *               0x012 heartbeat every 50ms (5 cycles)
 *               0x301 motor status every 10ms (current/temp/speed/battery)
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_RzcCom.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"
#include "Com.h"
#include "PduR.h"
#include "Dem.h"
#include "E2E.h"
#include "Swc_RzcSafety.h"


/* ==================================================================
 * Constants
 * ================================================================== */

/** CRC-8 polynomial 0x1D (SAE J1850) */
#define RZCCOM_CRC8_POLY        0x1Du

/** Number of alive counter slots (one per PDU) */
#define RZCCOM_ALIVE_SLOTS      16u

/** E2E consecutive failure threshold for safe default */
#define RZCCOM_E2E_FAIL_LIMIT    3u

/** Torque command timeout in 10ms cycles: 100ms / 10ms = 10 */
#define RZCCOM_TORQUE_TIMEOUT   10u

/** Heartbeat TX period in 10ms cycles: 50ms / 10ms = 5 */
#define RZCCOM_HB_PERIOD         5u

/** Motor status TX period in 10ms cycles: 10ms / 10ms = 1 */
#define RZCCOM_MSTATUS_PERIOD    1u

/* ==================================================================
 * Module State (all static file-scope -- ASIL D: no dynamic memory)
 * ================================================================== */

/** Module initialization flag */
static uint8   RzcCom_Initialized;

/** TX alive counters: one 4-bit counter per PDU */
static uint8   RzcCom_TxAlive[RZCCOM_ALIVE_SLOTS];

/** RX alive counters: last received value per PDU */
static uint8   RzcCom_RxAlive[RZCCOM_ALIVE_SLOTS];

/** RX E2E consecutive failure count per PDU */
static uint8   RzcCom_RxFailCount[RZCCOM_ALIVE_SLOTS];

/** Torque command timeout counter (10ms cycles since last valid RX) */
static uint16  RzcCom_TorqueTimeout;

/** Heartbeat TX cycle counter */
static uint8   RzcCom_HbCycleCount;

/* ==================================================================
 * E2E RX Configuration — uses shared BSW E2E_Check (Profile P01)
 *
 * Must match CVC TX E2E config for interoperability.
 * Byte layout: [0]=counter:4|dataId:4, [1]=CRC-8, [2..7]=payload
 * ================================================================== */

/** E2E RX config: one entry per E2E-protected RX PDU (indexed by PDU ID) */
static E2E_ConfigType RzcCom_RxE2eConfig[3];

/** E2E RX state: alive counter tracking per PDU */
static E2E_StateType  RzcCom_RxE2eState[3];

/** TransmitSchedule cycle counter (for motor_temp and battery pacing) */
static uint16  RzcCom_TxScheduleCycle;

/* ==================================================================
 * E2E Configuration for TX data messages (shared BSW E2E module)
 * ================================================================== */

/** @brief E2E config for Motor Status (0x300) */
static const E2E_ConfigType rzc_e2e_motor_status_cfg = {
    RZC_E2E_MOTOR_STATUS_DATA_ID,   /* DataId = 0x0E */
    15u,                             /* MaxDeltaCounter */
    8u                               /* DataLength */
};
static E2E_StateType rzc_e2e_motor_status_state;

/** @brief E2E config for Motor Current (0x301) */
static const E2E_ConfigType rzc_e2e_motor_current_cfg = {
    RZC_E2E_MOTOR_CURRENT_DATA_ID,  /* DataId = 0x0F */
    15u,                             /* MaxDeltaCounter */
    8u                               /* DataLength */
};
static E2E_StateType rzc_e2e_motor_current_state;

/** @brief E2E config for Motor Temp (0x302) */
static const E2E_ConfigType rzc_e2e_motor_temp_cfg = {
    RZC_E2E_MOTOR_TEMP_DATA_ID,     /* DataId = 0x10 */
    15u,                             /* MaxDeltaCounter */
    8u                               /* DataLength */
};
static E2E_StateType rzc_e2e_motor_temp_state;

/** @brief E2E config for Battery Status (0x303) */
static const E2E_ConfigType rzc_e2e_battery_cfg = {
    RZC_E2E_BATTERY_DATA_ID,        /* DataId = 0x11 */
    15u,                             /* MaxDeltaCounter */
    8u                               /* DataLength */
};
static E2E_StateType rzc_e2e_battery_state;

/* ==================================================================
 * Private Helper: CRC-8 calculation (polynomial 0x1D)
 * ================================================================== */

/**
 * @brief  Compute CRC-8 over a byte array
 * @param  data   Pointer to data bytes
 * @param  length Number of bytes
 * @param  dataId Data ID XOR'd into CRC initial value
 * @return CRC-8 result
 */
static uint8 RzcCom_Crc8(const uint8 *data, uint8 length, uint8 dataId)
{
    uint8 crc;
    uint8 i;
    uint8 bit;

    crc = dataId;

    for (i = 0u; i < length; i++)
    {
        crc ^= data[i];

        for (bit = 0u; bit < 8u; bit++)
        {
            if ((crc & 0x80u) != 0u)
            {
                crc = (uint8)((uint8)(crc << 1u) ^ RZCCOM_CRC8_POLY);
            }
            else
            {
                crc = (uint8)(crc << 1u);
            }
        }
    }

    return crc;
}

/* ==================================================================
 * Private: RZC-specific Data ID lookup
 * ================================================================== */

/**
 * @brief  Get the E2E Data ID for a given PDU index
 * @param  pduId  PDU index
 * @return Data ID byte, or 0x00 if unknown
 */
static uint8 RzcCom_GetTxDataId(uint8 pduId)
{
    switch (pduId)
    {
        case RZC_COM_TX_HEARTBEAT:
            return RZC_E2E_HEARTBEAT_DATA_ID;
        case RZC_COM_TX_MOTOR_STATUS:
            return RZC_E2E_MOTOR_STATUS_DATA_ID;
        case RZC_COM_TX_MOTOR_CURRENT:
            return RZC_E2E_MOTOR_CURRENT_DATA_ID;
        case RZC_COM_TX_MOTOR_TEMP:
            return RZC_E2E_MOTOR_TEMP_DATA_ID;
        case RZC_COM_TX_BATTERY_STATUS:
            return RZC_E2E_BATTERY_DATA_ID;
        default:
            return 0x00u;
    }
}

static uint8 RzcCom_GetRxDataId(uint8 pduId)
{
    switch (pduId)
    {
        case RZC_COM_RX_ESTOP:
            return RZC_E2E_ESTOP_DATA_ID;
        case RZC_COM_RX_VEHICLE_TORQUE:
            return RZC_E2E_VEHSTATE_DATA_ID;
        default:
            return 0x00u;
    }
}

/* ==================================================================
 * API: Rzc_E2eRxCheck — CanIf E2E callback
 * ================================================================== */

/**
 * @brief  E2E RX validation callback for CanIf
 *
 * Only E2E-checks PDUs with configured Data IDs (0x001 E-stop, 0x100
 * Vehicle_State). Virtual sensors (0x401) bypass E2E.
 *
 * @param  pduId   Upper-layer PDU ID
 * @param  data    Pointer to received data
 * @param  length  Data length
 * @return E_OK to accept frame, E_NOT_OK to drop it
 *
 * @safety_req SWR-RZC-020
 */
Std_ReturnType Rzc_E2eRxCheck(uint8 pduId, const uint8* data, uint8 length)
{
    E2E_CheckStatusType e2eResult;

    switch (pduId)
    {
        case RZC_COM_RX_ESTOP:
        case RZC_COM_RX_VEHICLE_TORQUE:
            e2eResult = E2E_Check(
                &RzcCom_RxE2eConfig[pduId],
                &RzcCom_RxE2eState[pduId],
                data, (uint16)length);
            if (e2eResult == E2E_STATUS_ERROR)
            {
                RzcCom_RxFailCount[pduId]++;
                return E_NOT_OK;
            }
            /* OK, REPEATED, WRONG_SEQ — all acceptable, clear fail count */
            RzcCom_RxFailCount[pduId] = 0u;
            return E_OK;

        default:
            return E_OK;  /* No E2E for this PDU */
    }
}

/* ==================================================================
 * API: Swc_RzcCom_Init
 * ================================================================== */

void Swc_RzcCom_Init(void)
{
    uint8 i;

    for (i = 0u; i < RZCCOM_ALIVE_SLOTS; i++)
    {
        RzcCom_TxAlive[i]     = 0u;
        RzcCom_RxAlive[i]     = 0u;
        RzcCom_RxFailCount[i] = 0u;
    }

    RzcCom_TorqueTimeout   = 0u;
    RzcCom_HbCycleCount    = 0u;
    RzcCom_TxScheduleCycle = 0u;

    /* Initialize shared BSW E2E states for TX data messages */
    rzc_e2e_motor_status_state.Counter  = 0u;
    rzc_e2e_motor_current_state.Counter = 0u;
    rzc_e2e_motor_temp_state.Counter    = 0u;
    rzc_e2e_battery_state.Counter       = 0u;

    /* Initialize shared BSW E2E config and state for RX messages.
     * DataId/MaxDeltaCounter must match CVC TX E2E configuration. */
    RzcCom_RxE2eConfig[RZC_COM_RX_ESTOP].DataId          = RZC_E2E_ESTOP_DATA_ID;
    RzcCom_RxE2eConfig[RZC_COM_RX_ESTOP].MaxDeltaCounter  = 3u;
    RzcCom_RxE2eConfig[RZC_COM_RX_ESTOP].DataLength        = 8u;
    RzcCom_RxE2eState[RZC_COM_RX_ESTOP].Counter            = 0u;

    RzcCom_RxE2eConfig[RZC_COM_RX_VEHICLE_TORQUE].DataId          = RZC_E2E_VEHSTATE_DATA_ID;
    RzcCom_RxE2eConfig[RZC_COM_RX_VEHICLE_TORQUE].MaxDeltaCounter  = 3u;
    RzcCom_RxE2eConfig[RZC_COM_RX_VEHICLE_TORQUE].DataLength        = 8u;
    RzcCom_RxE2eState[RZC_COM_RX_VEHICLE_TORQUE].Counter            = 0u;

    /* VIRT_SENSORS (index 2) not E2E-protected — zero-init for safety */
    RzcCom_RxE2eConfig[RZC_COM_RX_VIRT_SENSORS].DataId          = 0u;
    RzcCom_RxE2eConfig[RZC_COM_RX_VIRT_SENSORS].MaxDeltaCounter  = 0u;
    RzcCom_RxE2eConfig[RZC_COM_RX_VIRT_SENSORS].DataLength        = 0u;
    RzcCom_RxE2eState[RZC_COM_RX_VIRT_SENSORS].Counter            = 0u;

    RzcCom_Initialized     = TRUE;
}

/* ==================================================================
 * API: Swc_RzcCom_E2eProtect (TX)
 * ================================================================== */

Std_ReturnType Swc_RzcCom_E2eProtect(uint8 pduId, uint8 *data, uint8 length)
{
    uint8 dataId;
    uint8 alive;
    uint8 crc;

    if ((data == NULL_PTR) || (length < 2u))
    {
        return E_NOT_OK;
    }

    if (pduId >= RZCCOM_ALIVE_SLOTS)
    {
        return E_NOT_OK;
    }

    dataId = RzcCom_GetTxDataId(pduId);
    alive  = RzcCom_TxAlive[pduId];

    /* Write alive counter into byte 1 low nibble */
    data[1] = (uint8)((data[1] & 0xF0u) | (alive & 0x0Fu));

    /* Compute CRC over bytes 1..length-1 */
    crc = RzcCom_Crc8(&data[1], (uint8)(length - 1u), dataId);

    /* Write CRC into byte 0 */
    data[0] = crc;

    /* Increment alive counter with wrap at 15 */
    alive++;
    if (alive > 15u)
    {
        alive = 0u;
    }
    RzcCom_TxAlive[pduId] = alive;

    return E_OK;
}

/* ==================================================================
 * API: Swc_RzcCom_E2eCheck (RX)
 * ================================================================== */

Std_ReturnType Swc_RzcCom_E2eCheck(uint8 pduId, const uint8 *data, uint8 length)
{
    uint8 dataId;
    uint8 rx_crc;
    uint8 calc_crc;
    uint8 rx_alive;
    uint8 expected_alive;

    if ((data == NULL_PTR) || (length < 2u))
    {
        return E_NOT_OK;
    }

    if (pduId >= RZCCOM_ALIVE_SLOTS)
    {
        return E_NOT_OK;
    }

    dataId = RzcCom_GetTxDataId(pduId);
    if (dataId == 0x00u)
    {
        dataId = RzcCom_GetRxDataId(pduId);
    }

    /* Extract received CRC from byte 0 */
    rx_crc = data[0];

    /* Compute expected CRC over bytes 1..length-1 */
    calc_crc = RzcCom_Crc8(&data[1], (uint8)(length - 1u), dataId);

    if (rx_crc != calc_crc)
    {
        RzcCom_RxFailCount[pduId]++;
        return E_NOT_OK;
    }

    /* Extract received alive counter from byte 1 low nibble */
    rx_alive = (uint8)(data[1] & 0x0Fu);

    /* Check alive counter is monotonically increasing */
    expected_alive = RzcCom_RxAlive[pduId];
    expected_alive++;
    if (expected_alive > 15u)
    {
        expected_alive = 0u;
    }

    if (rx_alive != expected_alive)
    {
        RzcCom_RxFailCount[pduId]++;
        RzcCom_RxAlive[pduId] = rx_alive;  /* Re-sync */
        return E_NOT_OK;
    }

    /* Valid: update alive tracker and clear failure count */
    RzcCom_RxAlive[pduId]     = rx_alive;
    RzcCom_RxFailCount[pduId] = 0u;

    return E_OK;
}

/* ==================================================================
 * API: Swc_RzcCom_Receive (10ms cyclic)
 * ================================================================== */

void Swc_RzcCom_Receive(void)
{
    uint32 estop_raw;
    uint32 vehicle_raw;
    uint32 torque_raw;
    uint8  new_torque_received;

    if (RzcCom_Initialized != TRUE)
    {
        return;
    }

    /* Notify safety module that COM RX cycle is running.
     * Resets the CAN silence counter — prevents false CAN-loss latch. */
    Swc_RzcSafety_NotifyCanRx();

    /* --- 0x001 E-stop broadcast --- */
    estop_raw = 0u;
    (void)Rte_Read(RZC_SIG_ESTOP_ACTIVE, &estop_raw);

    if (estop_raw != 0u)
    {
        /* E-stop active: signal motor disable via RTE */
        (void)Rte_Write(RZC_SIG_ESTOP_ACTIVE, 1u);
    }

    /* --- 0x100 Vehicle_State + Torque --- */
    /* Check if E2E for torque PDU has exceeded failure threshold */
    if (RzcCom_RxFailCount[RZC_COM_RX_VEHICLE_TORQUE] >= RZCCOM_E2E_FAIL_LIMIT)
    {
        /* 3 consecutive E2E failures: safe default = zero torque */
        (void)Rte_Write(RZC_SIG_TORQUE_CMD, 0u);
        Dem_ReportErrorStatus(RZC_DTC_CAN_BUS_OFF, DEM_EVENT_STATUS_FAILED);
        return;
    }

    /* Read vehicle state and torque from RTE (set by lower BSW/Com) */
    vehicle_raw = 0u;
    (void)Rte_Read(RZC_SIG_VEHICLE_STATE, &vehicle_raw);

    torque_raw = 0u;
    new_torque_received = FALSE;
    (void)Rte_Read(RZC_SIG_TORQUE_CMD, &torque_raw);

    /* Detect new torque command (simplified: any non-zero read) */
    if (torque_raw != 0u)
    {
        new_torque_received = TRUE;
    }

    /* Torque command timeout: 100ms with no new command */
    if (new_torque_received == TRUE)
    {
        RzcCom_TorqueTimeout = 0u;
    }
    else
    {
        if (RzcCom_TorqueTimeout < 0xFFFFu)
        {
            RzcCom_TorqueTimeout++;
        }

        if (RzcCom_TorqueTimeout >= RZCCOM_TORQUE_TIMEOUT)
        {
            /* Timeout: force zero torque */
            (void)Rte_Write(RZC_SIG_TORQUE_CMD, 0u);
        }
    }
}

/* ==================================================================
 * API: Swc_RzcCom_TransmitSchedule (10ms cyclic)
 * ================================================================== */

void Swc_RzcCom_TransmitSchedule(void)
{
    if (RzcCom_Initialized != TRUE)
    {
        return;
    }

    /* Increment TX schedule cycle counter (wraps at 1000) */
    RzcCom_TxScheduleCycle++;
    if (RzcCom_TxScheduleCycle >= 1000u)
    {
        RzcCom_TxScheduleCycle = 0u;
    }

    /* RZC is the sole authority for sensor-derived CAN messages (0x300-0x303).
     * In SIL, the RzcSensorFeeder injects plant-sim physics values into
     * MCAL ADC stubs, so SWC sensor reads return correct values.
     * On real hardware, real ADC/encoder drivers provide the data.
     *
     * IMPORTANT: FDCAN TX FIFO has only 3 slots. Messages are paced across
     * cycles to avoid FIFO overflow. Max 2 frames per cycle from this SWC,
     * leaving 1 slot for heartbeat (0x012) from Swc_Heartbeat. */
    {
        uint32 torque_echo;
        uint32 speed_rpm;
        uint32 motor_dir;
        uint32 motor_enable;
        uint32 motor_fault;
        uint8  pdu[8];
        uint8  i;
        PduInfoType pdu_info;

        pdu_info.SduDataPtr = pdu;
        pdu_info.SduLength  = 8u;

        /* --- 0x300 Motor Status: every cycle (10ms) --- */
        torque_echo  = 0u;
        speed_rpm    = 0u;
        motor_dir    = 0u;
        motor_enable = 0u;
        motor_fault  = 0u;
        (void)Rte_Read(RZC_SIG_TORQUE_ECHO, &torque_echo);
        (void)Rte_Read(RZC_SIG_ENCODER_SPEED, &speed_rpm);
        (void)Rte_Read(RZC_SIG_MOTOR_DIR, &motor_dir);
        (void)Rte_Read(RZC_SIG_MOTOR_ENABLE, &motor_enable);
        (void)Rte_Read(RZC_SIG_MOTOR_FAULT, &motor_fault);

        for (i = 0u; i < 8u; i++) { pdu[i] = 0u; }
        pdu[2] = (uint8)torque_echo;
        pdu[3] = (uint8)(speed_rpm & 0xFFu);
        pdu[4] = (uint8)((speed_rpm >> 8u) & 0xFFu);
        pdu[5] = (uint8)motor_dir;
        pdu[6] = (uint8)motor_enable;
        pdu[7] = (uint8)motor_fault;
        (void)E2E_Protect(&rzc_e2e_motor_status_cfg, &rzc_e2e_motor_status_state, pdu, 8u);
        (void)PduR_Transmit(RZC_COM_TX_MOTOR_STATUS, &pdu_info);

        /* --- 0x301 Motor Current: every cycle (10ms) ---
         * DBC layout (little-endian bit numbering):
         *   [0-1]  E2E (DataID + AliveCounter + CRC8)
         *   [2-3]  Current_mA        16|16  (uint16 LE)
         *   [4]    CurrentDirection   32|1   (bit 0)
         *          MotorEnable        33|1   (bit 1)
         *          OvercurrentFlag    34|1   (bit 2)
         *          TorqueEcho low     35|5   (bits 3-7)
         *   [5]    TorqueEcho high    —      (bits 0-2)
         */
        {
            uint32 current_ma  = 0u;
            uint32 overcurrent = 0u;
            uint8  dir_bit;
            uint8  torque_val;

            (void)Rte_Read(RZC_SIG_CURRENT_MA, &current_ma);
            (void)Rte_Read(RZC_SIG_OVERCURRENT, &overcurrent);

            dir_bit   = (motor_dir == 2u) ? 1u : 0u;
            torque_val = (uint8)(torque_echo & 0xFFu);

            for (i = 0u; i < 8u; i++) { pdu[i] = 0u; }
            pdu[2] = (uint8)(current_ma & 0xFFu);
            pdu[3] = (uint8)((current_ma >> 8u) & 0xFFu);
            pdu[4] = dir_bit
                   | (uint8)((motor_enable != 0u) ? 2u : 0u)
                   | (uint8)((overcurrent != 0u) ? 4u : 0u)
                   | (uint8)((torque_val & 0x1Fu) << 3u);
            pdu[5] = (uint8)((torque_val >> 5u) & 0x07u);
            (void)E2E_Protect(&rzc_e2e_motor_current_cfg, &rzc_e2e_motor_current_state, pdu, 8u);
            (void)PduR_Transmit(RZC_COM_TX_MOTOR_CURRENT, &pdu_info);
        }

        /* --- 0x302 Motor Temp: every 10 cycles (100ms) ---
         * Offset by 3 cycles to avoid collision with heartbeat (every 5th). */
        if ((RzcCom_TxScheduleCycle % 10u) == 3u)
        {
            uint32 temp1_ddc    = 0u;
            uint32 temp2_ddc    = 0u;
            uint32 derating_pct = 0u;
            (void)Rte_Read(RZC_SIG_TEMP1_DC, &temp1_ddc);
            (void)Rte_Read(RZC_SIG_TEMP2_DC, &temp2_ddc);
            (void)Rte_Read(RZC_SIG_DERATING_PCT, &derating_pct);

            for (i = 0u; i < 8u; i++) { pdu[i] = 0u; }
            pdu[2] = (uint8)(temp1_ddc & 0xFFu);
            pdu[3] = (uint8)((temp1_ddc >> 8u) & 0xFFu);
            pdu[4] = (uint8)(temp2_ddc & 0xFFu);
            pdu[5] = (uint8)((temp2_ddc >> 8u) & 0xFFu);
            pdu[6] = (uint8)derating_pct;
            (void)E2E_Protect(&rzc_e2e_motor_temp_cfg, &rzc_e2e_motor_temp_state, pdu, 8u);
            (void)PduR_Transmit(RZC_COM_TX_MOTOR_TEMP, &pdu_info);
        }

        /* --- 0x303 Battery Status: every 20 cycles (200ms) ---
         * Offset by 7 cycles to avoid collision with motor_temp and heartbeat. */
        if ((RzcCom_TxScheduleCycle % 20u) == 7u)
        {
            uint32 battery_mv     = 0u;
            uint32 battery_status = 0u;
            uint32 battery_soc    = 0u;
            (void)Rte_Read(RZC_SIG_BATTERY_MV, &battery_mv);
            (void)Rte_Read(RZC_SIG_BATTERY_STATUS, &battery_status);
            (void)Rte_Read(RZC_SIG_BATTERY_SOC, &battery_soc);

            for (i = 0u; i < 8u; i++) { pdu[i] = 0u; }
            pdu[2] = (uint8)(battery_mv & 0xFFu);
            pdu[3] = (uint8)((battery_mv >> 8u) & 0xFFu);
            pdu[4] = (uint8)battery_status;
            pdu[5] = (uint8)battery_soc;
            (void)E2E_Protect(&rzc_e2e_battery_cfg, &rzc_e2e_battery_state, pdu, 8u);
            (void)PduR_Transmit(RZC_COM_TX_BATTERY_STATUS, &pdu_info);
        }
    }

    /* Heartbeat TX handled by Swc_Heartbeat_MainFunction (50ms via RTE) */
}
