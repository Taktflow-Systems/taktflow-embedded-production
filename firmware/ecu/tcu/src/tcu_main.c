/**
 * @file    tcu_main.c
 * @brief   TCU main entry point -- BSW init, SWC init, 10ms main loop
 * @date    2026-02-23
 *
 * @safety_req SWR-TCU-001: TCU initialization and cyclic execution
 * @safety_req SWR-TCU-015: Graceful shutdown on SIGINT/SIGTERM
 * @traces_to  TSR-035, TSR-038, TSR-039, TSR-040, TSR-046, TSR-047
 *
 * @standard AUTOSAR BSW init sequence, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

#include "Std_Types.h"
#include "Sil_Time.h"
#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include "Dcm.h"
#include "Dem.h"
#include "CanTp.h"
#include "Rte.h"

#include "Swc_UdsServer.h"
#include "Swc_DtcStore.h"
#include "Swc_Obd2Pids.h"
#include "Tcu_Cfg.h"

/* ---- External Configuration ---- */

extern const Rte_ConfigType  tcu_rte_config;
extern const Com_ConfigType  tcu_com_config;
extern const Dcm_ConfigType  tcu_dcm_config;

/* ---- CanIf TX PDU Mapping ---- */

static const CanIf_TxPduConfigType canif_tx_config[] = {
    { 0x015u, TCU_COM_TX_HEARTBEAT,       4u, 0u },  /* PDU 0: TCU heartbeat */
    { 0x644u, TCU_COM_TX_UDS_RSP,         8u, 0u },  /* PDU 1: UDS response */
};

static const CanIf_RxPduConfigType canif_rx_config[] = {
    { 0x7DFu, TCU_COM_RX_UDS_FUNC,       8u, FALSE },
    { 0x604u, TCU_COM_RX_UDS_PHYS,       8u, FALSE },
    { 0x100u, TCU_COM_RX_VEHICLE_STATE,   8u, FALSE },
    { 0x301u, TCU_COM_RX_MOTOR_CURRENT,   8u, FALSE },
    { 0x302u, TCU_COM_RX_MOTOR_TEMP,      8u, FALSE },
    { 0x303u, TCU_COM_RX_BATTERY,         8u, FALSE },
    { 0x500u, TCU_COM_RX_DTC_BCAST,       8u, FALSE },
    { 0x010u, TCU_COM_RX_HB_CVC,          8u, FALSE },
    { 0x011u, TCU_COM_RX_HB_FZC,          8u, FALSE },
    { 0x012u, TCU_COM_RX_HB_RZC,          8u, FALSE },
};

static const CanIf_ConfigType canif_config = {
    .txPduConfig = canif_tx_config,
    .txPduCount  = (uint8)(sizeof(canif_tx_config) / sizeof(canif_tx_config[0])),
    .rxPduConfig = canif_rx_config,
    .rxPduCount  = (uint8)(sizeof(canif_rx_config) / sizeof(canif_rx_config[0])),
};


/** PduR routing: RX PDUs route to Com or Dcm */
static const PduR_RoutingTableType pdur_routing[] = {
    { TCU_COM_RX_UDS_FUNC,       PDUR_DEST_CANTP, TCU_COM_RX_UDS_FUNC      },
    { TCU_COM_RX_UDS_PHYS,       PDUR_DEST_CANTP, TCU_COM_RX_UDS_PHYS      },
    { TCU_COM_RX_VEHICLE_STATE,  PDUR_DEST_COM,   TCU_COM_RX_VEHICLE_STATE },
    { TCU_COM_RX_MOTOR_CURRENT,  PDUR_DEST_COM,   TCU_COM_RX_MOTOR_CURRENT },
    { TCU_COM_RX_MOTOR_TEMP,     PDUR_DEST_COM,   TCU_COM_RX_MOTOR_TEMP    },
    { TCU_COM_RX_BATTERY,        PDUR_DEST_COM,   TCU_COM_RX_BATTERY       },
    { TCU_COM_RX_DTC_BCAST,      PDUR_DEST_COM,   TCU_COM_RX_DTC_BCAST     },
    { TCU_COM_RX_HB_CVC,         PDUR_DEST_COM,   TCU_COM_RX_HB_CVC       },
    { TCU_COM_RX_HB_FZC,         PDUR_DEST_COM,   TCU_COM_RX_HB_FZC       },
    { TCU_COM_RX_HB_RZC,         PDUR_DEST_COM,   TCU_COM_RX_HB_RZC       },
};

static const PduR_ConfigType pdur_cfg = {
    .routingTable = pdur_routing,
    .routingCount = (uint8)(sizeof(pdur_routing) / sizeof(pdur_routing[0])),
};

/* ---- TCU Heartbeat ---- */

static uint8 tcu_hb_alive_counter = 0u;

/**
 * @brief  TCU heartbeat -- sends CAN 0x015 every 500ms
 */
extern void Tcu_Heartbeat_500ms(void);
void Tcu_Heartbeat_500ms(void)
{
    uint8 alive  = tcu_hb_alive_counter;
    uint8 ecu_id = 0x07u;  /* TCU = ECU 7 */
    (void)Com_SendSignal(3u, &alive);   /* signal 3 = heartbeat AliveCounter */
    (void)Com_SendSignal(4u, &ecu_id);  /* signal 4 = heartbeat ECU_ID */
    tcu_hb_alive_counter++;
}

/* ---- Shutdown Flag ---- */

static volatile boolean tcu_shutdown_requested = FALSE;

/**
 * @brief  Signal handler for SIGINT and SIGTERM
 */
static void tcu_signal_handler(int sig)
{
    (void)sig;
    tcu_shutdown_requested = TRUE;
}

/* ---- Main ---- */

int main(void)
{
    /* Install signal handlers for graceful shutdown */
    struct sigaction sa;
    sa.sa_handler = tcu_signal_handler;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGINT,  &sa, NULL_PTR);
    (void)sigaction(SIGTERM, &sa, NULL_PTR);

    (void)printf("[TCU] Telematics Control Unit starting...\n");

    /* ---- BSW Initialization (AUTOSAR order) ---- */

    /* MCAL */
    Can_ConfigType can_cfg = {
        .baudrate     = 500000u,
        .controllerId = 0u,
    };
    Can_Init(&can_cfg);
    (void)Can_SetControllerMode(0u, CAN_CS_STARTED);

    /* ECUAL */
    CanIf_Init(&canif_config);
    PduR_Init(&pdur_cfg);

    /* Services */
    Com_Init(&tcu_com_config);
    Dcm_Init(&tcu_dcm_config);
    Dem_Init(NULL_PTR);

    /* RTE */
    Rte_Init(&tcu_rte_config);

    (void)printf("[TCU] BSW stack initialized\n");

    /* ---- SWC Initialization ---- */

    Swc_UdsServer_Init();
    Swc_DtcStore_Init();
    Swc_Obd2Pids_Init();

    (void)printf("[TCU] SWC layer initialized\n");
    (void)printf("[TCU] Entering main loop (10ms tick)\n");

    /* ---- Main Loop: 10ms tick ---- */

    uint8 hb_tick = 0u;

    while (tcu_shutdown_requested == FALSE) {
        /* BSW CAN processing: receive frames from bus */
        Can_MainFunction_Read();
        Com_MainFunction_Rx();

        /* RTE scheduler dispatches all configured runnables */
        Rte_MainFunction();

        /* Periodic SWC functions */
        Swc_DtcStore_10ms();

        /* TCU heartbeat every 500ms (50 ticks) */
        hb_tick++;
        if (hb_tick >= 50u) {
            hb_tick = 0u;
            Tcu_Heartbeat_500ms();
        }

        /* BSW CAN processing: transmit queued frames */
        Com_MainFunction_Tx();

        /* CanTp + Dcm for UDS processing */
        CanTp_MainFunction();
        Dcm_MainFunction();

        Sil_Time_Sleep(10000u); /* 10ms virtual tick */
    }

    /* ---- Shutdown ---- */

    (void)Can_SetControllerMode(0u, CAN_CS_STOPPED);
    (void)printf("[TCU] Shutdown complete\n");

    return 0;
}
