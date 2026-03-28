/**
 * @file    bcm_main.c
 * @brief   BCM main entry point — BSW init, SWC init, 10ms tick loop
 * @date    2026-02-23
 *
 * @safety_req SWR-BCM-001, SWR-BCM-012
 * @traces_to  SSR-BCM-001, SSR-BCM-012, TSR-046, TSR-047, TSR-048
 *
 * @details  Simulated ECU running in Docker on Linux with SocketCAN.
 *           Initializes all BSW modules and SWCs, then enters a main loop
 *           with 10ms tick via usleep. Rte_MainFunction dispatches all
 *           configured runnables at their configured periods.
 *
 *           Handles SIGINT/SIGTERM for graceful shutdown.
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (QM)
 * @copyright Taktflow Systems 2026
 */
#include "Std_Types.h"
#include "Sil_Time.h"
#include "Bcm_Cfg.h"

/* ==================================================================
 * BSW Module Headers
 * ================================================================== */

#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include "Dem.h"
#include "Rte.h"

/* ==================================================================
 * SWC Headers
 * ================================================================== */

#include "Swc_BcmMain.h"
#include "Swc_BcmCan.h"
#include "Swc_Lights.h"
#include "Swc_Indicators.h"
#include "Swc_DoorLock.h"

/* POSIX headers for simulated ECU */
#include <signal.h>
#include <unistd.h>

/* ==================================================================
 * RTE↔Com Bridge
 *
 * SWCs read/write RTE signals.  Com sends/receives CAN PDUs.
 * This bridge copies RTE output signals → Com TX shadow buffers
 * and Com RX shadow buffers → RTE input signals every 10ms.
 * ================================================================== */

/**
 * @brief  Bridge RTE outputs to Com TX + Com RX to RTE inputs
 *
 * @details  TX: Reads 6 RTE output signals (lights, indicators, door lock)
 *           and calls Com_SendSignal() so Com_MainFunction_Tx can transmit.
 *           RX: Reads 4 Com RX signals (vehicle state/speed, body cmd, e-stop)
 *           and writes them to RTE so SWCs can consume them.
 */
extern void Bcm_ComBridge_10ms(void);
void Bcm_ComBridge_10ms(void)
{
    /* ---- TX bridge: RTE outputs → Com TX ---- */
    {
        uint32 rte_val;
        uint8  sig_u8;

        /* Light_Status signals → CAN 0x400 */
        (void)Rte_Read(BCM_SIG_LIGHT_STATUS_HEADLIGHT_ON, &rte_val);
        sig_u8 = (uint8)rte_val;
        (void)Com_SendSignal(BCM_COM_SIG_LIGHT_STATUS_HEADLIGHT_ON, &sig_u8);

        (void)Rte_Read(BCM_SIG_LIGHT_STATUS_TAIL_LIGHT_ON, &rte_val);
        sig_u8 = (uint8)rte_val;
        (void)Com_SendSignal(BCM_COM_SIG_LIGHT_STATUS_TAIL_LIGHT_ON, &sig_u8);

        /* Indicator_State signals → CAN 0x401 */
        (void)Rte_Read(BCM_SIG_INDICATOR_STATE_LEFT_ON, &rte_val);
        sig_u8 = (uint8)rte_val;
        (void)Com_SendSignal(BCM_COM_SIG_INDICATOR_STATE_LEFT_ON, &sig_u8);

        (void)Rte_Read(BCM_SIG_INDICATOR_STATE_RIGHT_ON, &rte_val);
        sig_u8 = (uint8)rte_val;
        (void)Com_SendSignal(BCM_COM_SIG_INDICATOR_STATE_RIGHT_ON, &sig_u8);

        (void)Rte_Read(BCM_SIG_INDICATOR_STATE_HAZARD_ACTIVE, &rte_val);
        sig_u8 = (uint8)rte_val;
        (void)Com_SendSignal(BCM_COM_SIG_INDICATOR_STATE_HAZARD_ACTIVE, &sig_u8);

        /* Door_Lock_Status signals → CAN 0x402 */
        (void)Rte_Read(BCM_SIG_DOOR_LOCK_STATUS_FRONT_LEFT_LOCK, &rte_val);
        sig_u8 = (uint8)rte_val;
        (void)Com_SendSignal(BCM_COM_SIG_DOOR_LOCK_STATUS_FRONT_LEFT_LOCK, &sig_u8);
    }

    /* ---- RX bridge: Com RX → RTE inputs ---- */
    /* Com→RTE auto-binding handles Vehicle_State, Body_Control_Cmd,
     * EStop_Broadcast — no manual bridge needed (AUTOSAR layering). */
}

/* ==================================================================
 * External Configuration (defined in cfg/ files)
 * ================================================================== */

extern const Rte_ConfigType  bcm_rte_config;
extern const Com_ConfigType  bcm_com_config;

/* ==================================================================
 * Static Configuration Constants
 * ================================================================== */

/** CAN driver configuration — 500 kbps, controller 0 */
static const Can_ConfigType can_config = {
    .baudrate     = 500000u,
    .controllerId = 0u,
};

/** CanIf TX PDU routing: Com TX PDU -> CAN ID (array index == TxPduId) */
static const CanIf_TxPduConfigType canif_tx_config[] = {
    /* canId,  upperPduId,                    dlc, hth */
    { 0x016u, BCM_COM_TX_BCM_HEARTBEAT,       4u, 0u },  /* PDU 0: BCM heartbeat   */
    { 0x400u, BCM_COM_TX_LIGHT_STATUS,        8u, 0u },  /* PDU 1: Light status     */
    { 0x401u, BCM_COM_TX_INDICATOR_STATE,     8u, 0u },  /* PDU 2: Indicator state  */
    { 0x402u, BCM_COM_TX_DOOR_LOCK_STATUS,    8u, 0u },  /* PDU 3: Door lock state  */
};

/** CanIf RX PDU routing: CAN ID -> Com RX PDU */
static const CanIf_RxPduConfigType canif_rx_config[] = {
    /* canId,  upperPduId,              dlc, isExtended */
    { 0x100u, BCM_COM_RX_VEHICLE_STATE,  8u, FALSE },  /* Vehicle state from CVC */
    { 0x301u, BCM_COM_RX_MOTOR_CURRENT,  8u, FALSE },  /* Speed from RZC         */
    { 0x350u, BCM_COM_RX_BODY_CONTROL_CMD,       8u, FALSE },  /* Body cmd from CVC      */
};

static const CanIf_ConfigType canif_config = {
    .txPduConfig = canif_tx_config,
    .txPduCount  = (uint8)(sizeof(canif_tx_config) / sizeof(canif_tx_config[0])),
    .rxPduConfig = canif_rx_config,
    .rxPduCount  = (uint8)(sizeof(canif_rx_config) / sizeof(canif_rx_config[0])),
    .e2eRxCheck  = NULL_PTR,
};

/** PduR RX routing: CanIf RX PDU ID → Com */
static const PduR_RoutingTableType bcm_pdur_routing[] = {
    { BCM_COM_RX_VEHICLE_STATE,  PDUR_DEST_COM, BCM_COM_RX_VEHICLE_STATE },
    { BCM_COM_RX_MOTOR_CURRENT,  PDUR_DEST_COM, BCM_COM_RX_MOTOR_CURRENT },
    { BCM_COM_RX_BODY_CONTROL_CMD,       PDUR_DEST_COM, BCM_COM_RX_BODY_CONTROL_CMD      },
};

static const PduR_ConfigType bcm_pdur_config = {
    .routingTable = bcm_pdur_routing,
    .routingCount = (uint8)(sizeof(bcm_pdur_routing) / sizeof(bcm_pdur_routing[0])),
};

/* ==================================================================
 * Graceful Shutdown
 * ================================================================== */

/* ==================================================================
 * BCM Heartbeat (500ms period, CAN 0x016)
 * ================================================================== */

static uint8 bcm_hb_alive_counter;

/**
 * @brief  Transmit BCM heartbeat via Com
 * @note   Called every 500ms from main loop (50 ticks × 10ms)
 */
static void Bcm_Heartbeat_500ms(void)
{
    uint8 alive  = bcm_hb_alive_counter;
    uint8 ecu_id = 0x06u;  /* BCM = ECU 6 */
    (void)Com_SendSignal(BCM_COM_SIG_BCM_HEARTBEAT_ALIVE_COUNTER, &alive);
    (void)Com_SendSignal(BCM_COM_SIG_BCM_HEARTBEAT_ECU_ID, &ecu_id);
    bcm_hb_alive_counter++;
}

static volatile uint8 shutdown_requested;

static void Bcm_SignalHandler(int sig)
{
    (void)sig;
    shutdown_requested = 1u;
}

/* ==================================================================
 * Main Entry Point
 * ================================================================== */

/**
 * @brief  BCM main function — init, main loop, graceful shutdown
 *
 * @safety_req SWR-BCM-001, SWR-BCM-012
 * @traces_to  SSR-BCM-001, SSR-BCM-012, TSR-046, TSR-047, TSR-048
 */
int main(void)
{
    shutdown_requested = 0u;

    /* ---- Step 1: Register signal handlers for graceful shutdown ---- */
    (void)signal(SIGINT, Bcm_SignalHandler);
    (void)signal(SIGTERM, Bcm_SignalHandler);

    /* ---- Step 2: BSW module initialization (order matters) ---- */
    Can_Init(&can_config);
    CanIf_Init(&canif_config);
    PduR_Init(&bcm_pdur_config);
    Com_Init(&bcm_com_config);
    Dem_Init(NULL_PTR);
    Rte_Init(&bcm_rte_config);

    /* ---- Step 3: SWC initialization ---- */
    (void)BCM_CAN_Init();
    Swc_Lights_Init();
    Swc_Indicators_Init();
    Swc_DoorLock_Init();

    /* ---- Step 4: Start CAN controller ---- */
    (void)Can_SetControllerMode(0u, CAN_CS_STARTED);

    /* ---- Step 5: Main loop — 10ms tick via usleep ---- */
    bcm_hb_alive_counter = 0u;
    {
    uint8 hb_tick = 0u;
    while (shutdown_requested == 0u) {
        /* Sleep 10ms (10000 microseconds) */
        Sil_Time_Sleep((uint32)BCM_RTE_PERIOD_MS * 1000u); /* tick from codegen */

        /* BSW CAN processing: receive frames from bus */
        Can_MainFunction_Read();
        Com_MainFunction_Rx();

        /* Bridge: Com RX → RTE inputs, RTE outputs → Com TX */
        Bcm_ComBridge_10ms();

        /* RTE scheduler dispatches all configured runnables */
        Rte_MainFunction();

        /* Bridge again after RTE: push SWC outputs to Com */
        Bcm_ComBridge_10ms();

        /* Heartbeat every 500ms — threshold derived from tick period */
        hb_tick++;
        if (hb_tick >= (500u / BCM_RTE_PERIOD_MS)) {
            hb_tick = 0u;
            Bcm_Heartbeat_500ms();
        }

        /* BSW CAN processing: transmit queued frames */
        Com_MainFunction_Tx();
        Can_MainFunction_Write();
    }
    }

    /* ---- Step 6: Graceful shutdown ---- */
    (void)Can_SetControllerMode(0u, CAN_CS_STOPPED);

    return 0;
}
