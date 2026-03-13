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
#include "E2E.h"
#include "Com.h"
#include "Rte.h"
#include "Swc_VehicleState.h"
#include "PduR.h"

/* SIL diagnostic logging — compile with -DSIL_DIAG to enable */
#ifdef SIL_DIAG
#include <stdio.h>
#define CVCCOM_DIAG(fmt, ...) (void)fprintf(stderr, "[CVC_COM] " fmt "\n", ##__VA_ARGS__)
#else
#define CVCCOM_DIAG(fmt, ...) ((void)0)
#endif

#define CVC_SAFE_BRAKE_CMD   100u   /**< Max brake for safe-state TX */

/* ==================================================================
 * TX Message Schedule Table (SWR-CVC-017)
 * ================================================================== */

static const Swc_CvcCom_TxEntryType CvcCom_TxTable[5] = {
    { 0x001u,  10u, CVC_E2E_ESTOP_DATA_ID,     8u },  /* E-stop: 10ms      */
    { 0x010u,  50u, CVC_E2E_HEARTBEAT_DATA_ID,  8u },  /* Heartbeat: 50ms   */
    { 0x100u,  20u, CVC_E2E_VEHSTATE_DATA_ID,   8u },  /* Vehicle state:20ms*/
    { 0x101u,  10u, CVC_E2E_TORQUE_DATA_ID,     8u },  /* Torque req: 10ms  */
    { 0x102u,  20u, 0x07u,                       8u },  /* Steer cmd: 20ms   */
};

#define CVCCOM_TX_TABLE_SIZE  5u

/* ==================================================================
 * RX Message Routing Table (SWR-CVC-016)
 * ================================================================== */

static const Swc_CvcCom_RxEntryType CvcCom_RxTable[4] = {
    { 0x011u, 0x03u, 8u },  /* FZC heartbeat — FZC_E2E_HEARTBEAT_DATA_ID */
    { 0x012u, 0x04u, 8u },  /* RZC heartbeat — RZC_E2E_HEARTBEAT_DATA_ID */
    { 0x210u, 0x21u, 8u },  /* Brake fault — FZC_E2E_BRAKE_STATUS_DATA_ID */
    { 0x301u, 0x0Fu, 8u },  /* Motor current — RZC_E2E_MOTOR_CURRENT_DATA_ID */
};

#define CVCCOM_RX_TABLE_SIZE  4u

/* ==================================================================
 * Module State (all static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8   CvcCom_Initialized;

/* E2E TX state (per TX message) */
static E2E_StateType     CvcCom_TxE2eState[CVCCOM_TX_TABLE_SIZE];
static E2E_ConfigType    CvcCom_TxE2eConfig[CVCCOM_TX_TABLE_SIZE];

/* E2E RX state (per RX message) */
static E2E_StateType     CvcCom_RxE2eState[CVCCOM_RX_TABLE_SIZE];
static E2E_ConfigType    CvcCom_RxE2eConfig[CVCCOM_RX_TABLE_SIZE];

/* RX status tracking */
static Swc_CvcCom_RxStatusType CvcCom_RxStatus[CVCCOM_RX_TABLE_SIZE];

/* TX last-transmit timestamps */
static uint32  CvcCom_TxLastTimeMs[CVCCOM_TX_TABLE_SIZE];

/* ==================================================================
 * API: Swc_CvcCom_Init
 * ================================================================== */

void Swc_CvcCom_Init(void)
{
    uint8 i;

    /* Initialize TX E2E state and config */
    for (i = 0u; i < CVCCOM_TX_TABLE_SIZE; i++)
    {
        CvcCom_TxE2eState[i].Counter     = 0u;
        CvcCom_TxE2eConfig[i].DataId     = CvcCom_TxTable[i].dataId;
        CvcCom_TxE2eConfig[i].MaxDeltaCounter = 3u;
        CvcCom_TxE2eConfig[i].DataLength = CvcCom_TxTable[i].dlc;
        CvcCom_TxLastTimeMs[i]           = 0u;
    }

    /* Initialize RX E2E state and config */
    for (i = 0u; i < CVCCOM_RX_TABLE_SIZE; i++)
    {
        CvcCom_RxE2eState[i].Counter     = 0u;
        CvcCom_RxE2eConfig[i].DataId     = CvcCom_RxTable[i].dataId;
        CvcCom_RxE2eConfig[i].MaxDeltaCounter = 3u;
        CvcCom_RxE2eConfig[i].DataLength = CvcCom_RxTable[i].dlc;

        CvcCom_RxStatus[i].failCount     = 0u;
        CvcCom_RxStatus[i].useSafeDefault = FALSE;
    }

    CvcCom_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_CvcCom_E2eProtect
 * ================================================================== */

/**
 * @safety_req SWR-CVC-014
 */
Std_ReturnType Swc_CvcCom_E2eProtect(uint8 txIndex, uint8* payload,
                                      uint8 length)
{
    if (CvcCom_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (txIndex >= CVCCOM_TX_TABLE_SIZE)
    {
        return E_NOT_OK;
    }

    if (payload == NULL_PTR)
    {
        return E_NOT_OK;
    }

    return E2E_Protect(&CvcCom_TxE2eConfig[txIndex],
                        &CvcCom_TxE2eState[txIndex],
                        payload,
                        (uint16)length);
}

/* ==================================================================
 * API: Swc_CvcCom_E2eCheck
 * ================================================================== */

/**
 * @safety_req SWR-CVC-015
 */
Std_ReturnType Swc_CvcCom_E2eCheck(uint8 rxIndex, const uint8* payload,
                                    uint8 length)
{
    E2E_CheckStatusType e2eResult;

    if (CvcCom_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (rxIndex >= CVCCOM_RX_TABLE_SIZE)
    {
        return E_NOT_OK;
    }

    if (payload == NULL_PTR)
    {
        return E_NOT_OK;
    }

    e2eResult = E2E_Check(&CvcCom_RxE2eConfig[rxIndex],
                           &CvcCom_RxE2eState[rxIndex],
                           payload,
                           (uint16)length);

    if (e2eResult == E2E_STATUS_OK)
    {
        /* Valid message — reset failure counter */
        CvcCom_RxStatus[rxIndex].failCount     = 0u;
        CvcCom_RxStatus[rxIndex].useSafeDefault = FALSE;
        return E_OK;
    }

    /* E2E check failed — increment failure counter */
    CvcCom_RxStatus[rxIndex].failCount++;

    if (CvcCom_RxStatus[rxIndex].failCount >= CVCCOM_E2E_FAIL_THRESHOLD)
    {
        /* 3 consecutive failures — use safe default values */
        CvcCom_RxStatus[rxIndex].useSafeDefault = TRUE;
    }

    return E_NOT_OK;
}

/* ==================================================================
 * API: Swc_CvcCom_Receive
 * ================================================================== */

/**
 * @safety_req SWR-CVC-016
 */
Std_ReturnType Swc_CvcCom_Receive(uint16 canId, const uint8* payload,
                                   uint8 length)
{
    uint8 i;

    if (CvcCom_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (payload == NULL_PTR)
    {
        return E_NOT_OK;
    }

    /* Look up CAN ID in routing table */
    for (i = 0u; i < CVCCOM_RX_TABLE_SIZE; i++)
    {
        if (CvcCom_RxTable[i].canId == canId)
        {
            /* Found: apply E2E check */
            return Swc_CvcCom_E2eCheck(i, payload, length);
        }
    }

    /* Unknown CAN ID — not in routing table */
    return E_NOT_OK;
}

/* ==================================================================
 * API: Swc_CvcCom_TransmitSchedule
 * ================================================================== */

/**
 * @safety_req SWR-CVC-017
 */
void Swc_CvcCom_TransmitSchedule(uint32 currentTimeMs)
{
    uint8  i;
    uint32 elapsed;

    if (CvcCom_Initialized != TRUE)
    {
        return;
    }

    for (i = 0u; i < CVCCOM_TX_TABLE_SIZE; i++)
    {
        elapsed = currentTimeMs - CvcCom_TxLastTimeMs[i];

        if (elapsed >= (uint32)CvcCom_TxTable[i].periodMs)
        {
            CvcCom_TxLastTimeMs[i] = currentTimeMs;

            /* Transmit is handled by the caller after E2E protect.
             * Mark the slot as due for transmission. */
        }
    }

    /* ---- TX: 0x100 Vehicle State (E2E protected, cyclic) ---- */
    {
        uint8  txBuf[8];
        uint8  j;
        uint32 faultSig;
        uint8  faultMask = 0u;
        PduInfoType pdu_info;

        pdu_info.SduDataPtr = txBuf;
        pdu_info.SduLength  = 8u;

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

        /* E2E protect: CRC-8 + alive counter (TX index 2 = 0x100) */
        (void)Swc_CvcCom_E2eProtect(2u, txBuf, 8u);

        /* Transmit via PduR -> CanIf -> CAN 0x100 */
        (void)PduR_Transmit(CVC_COM_TX_VEHICLE_STATE, &pdu_info);
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
    (void)Com_ReceiveSignal(CVC_COM_SIG_SC_STATUS_RELAY_STATE, &sc_relay_byte3);
    /* Com extracts the 1-bit RelayState signal as 0 or 1.
     * 1=energized (OK), 0=de-energized (killed).
     * On PDU timeout Com zeros shadow → 0 → kill. */
    {
        (void)Rte_Write(CVC_SIG_SC_RELAY_KILL, (uint32)sc_relay_byte3);
    }
    (void)Com_ReceiveSignal(CVC_COM_SIG_BATTERY_STATUS_BATTERY_STATUS, &battery_status_val);
    (void)Com_ReceiveSignal(CVC_COM_SIG_STEERING_STATUS_STEER_FAULT_STATUS, &steering_fault_val);
    (void)Com_ReceiveSignal(CVC_COM_SIG_MOTOR_STATUS_MOTOR_FAULT_STATUS, &motor_fault_rzc_val);

    /* Bridge to RTE for VehicleState to consume.
     * Heartbeat comm status is owned exclusively by Swc_Heartbeat.c
     * — do NOT write CVC_SIG_FZC/RZC_COMM_STATUS here. */
    (void)Rte_Write(CVC_SIG_BRAKE_FAULT,    (uint32)brake_fault_val);
    (void)Rte_Write(CVC_SIG_MOTOR_CUTOFF,   (uint32)motor_cutoff_val);
    /* SC_RELAY_KILL already written above after bit extraction */
    (void)Rte_Write(CVC_SIG_BATTERY_STATUS, (uint32)battery_status_val);
    (void)Rte_Write(CVC_SIG_STEERING_FAULT, (uint32)steering_fault_val);
    (void)Rte_Write(CVC_SIG_MOTOR_FAULT_RZC, (uint32)motor_fault_rzc_val);

    /* SIL E-Stop injection: fault-inject API sends CAN 0x001 with E-Stop
     * active flag at byte 2.  CVC normally reads E-Stop via GPIO (DIO ch 5).
     * In SIL, CvcCom_Hw_InjectEstop writes to IoHwAb injection buffer.
     * On target, this is a no-op (GPIO reads real hardware). */
    {
        extern void CvcCom_Hw_InjectEstop(uint8 Level);
        uint8 estop_inject_val = 0u;
        /* TODO:SCALE — signal ID 19u is a dev-repo leftover; no dedicated
         * RX EStop_Inject Com signal exists in production codegen yet.
         * Harmless on target (CvcCom_Hw_InjectEstop is a no-op). */
        (void)Com_ReceiveSignal(19u, &estop_inject_val);
        CvcCom_Hw_InjectEstop((estop_inject_val != 0u) ? 1u : 0u);
    }
}

/* ==================================================================
 * API: Swc_CvcCom_GetRxStatus
 * ================================================================== */

Std_ReturnType Swc_CvcCom_GetRxStatus(uint8 rxIndex,
                                       Swc_CvcCom_RxStatusType* status)
{
    if (CvcCom_Initialized != TRUE)
    {
        return E_NOT_OK;
    }

    if (rxIndex >= CVCCOM_RX_TABLE_SIZE)
    {
        return E_NOT_OK;
    }

    if (status == NULL_PTR)
    {
        return E_NOT_OK;
    }

    *status = CvcCom_RxStatus[rxIndex];
    return E_OK;
}
