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
#include "DoIp_Posix.h"

#include "Swc_DtcStore.h"
#include "Swc_Obd2Pids.h"
#include "Tcu_Cfg.h"

/* ---- External Configuration ---- */

extern const Rte_ConfigType  tcu_rte_config;
extern const Com_ConfigType  tcu_com_config;
extern const Dcm_ConfigType  tcu_dcm_config;

/* CanIf config — use GENERATED routing table from CanIf_Cfg_Tcu.c
 * DO NOT hand-write CAN ID routing here. All routing is generated from
 * DBC → ARXML → codegen. */
extern const CanIf_ConfigType tcu_canif_config;
#define canif_config tcu_canif_config

/* PduR config — use GENERATED routing table from PduR_Cfg_Tcu.c
 * DO NOT hand-write routing tables here. All routing is generated from
 * DBC → ARXML → codegen. The generated config routes UDS_Phys_Req_TCU
 * through CanTp for ISO-TP segmentation. */
extern const PduR_ConfigType tcu_pdur_config;

static const DoIp_Posix_ConfigType tcu_doip_config = {
    .LogicalAddress = 0x0004u,
    .Vin = { 'T', 'A', 'K', 'T', 'F', 'L', 'O', 'W', '0', '0', '0', '0', '0', '0', '0', '0', '1' },
    .Eid = { 'T', 'C', 'U', '0', '0', '1' },
    .Gid = { 'T', 'F', 'P', 'O', 'S', 'X' },
};

/** CanTp configuration — single channel for UDS diagnostics */
static const CanTp_ConfigType tcu_cantp_config = {
    .rxPduId      = 0u,                    /* CanTp RX channel ID               */
    .txPduId      = TCU_COM_TX_UDS_RSP,    /* TX frames → CanIf PDU (0x644)    */
    .fcTxPduId    = TCU_COM_TX_UDS_RSP,    /* FC frames → same CAN ID          */
    .upperRxPduId = 0u,                    /* Dcm RX PDU ID                     */
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
    PduR_Init(&tcu_pdur_config);
    CanTp_Init(&tcu_cantp_config);

    /* Services */
    Com_Init(&tcu_com_config);
    Dcm_Init(&tcu_dcm_config);
    Dem_Init(NULL_PTR);

    /* RTE */
    Rte_Init(&tcu_rte_config);

    if (DoIp_Posix_Init(&tcu_doip_config) != E_OK) {
        (void)printf("[TCU] DoIP init failed\n");
    }

    (void)printf("[TCU] BSW stack initialized\n");

    /* ---- SWC Initialization ---- */

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

        /* TCU heartbeat every 500ms — threshold derived from tick period */
        hb_tick++;
        if (hb_tick >= (500u / TCU_RTE_PERIOD_MS)) {
            hb_tick = 0u;
            Tcu_Heartbeat_500ms();
        }

        /* BSW CAN processing: transmit queued frames */
        Com_MainFunction_Tx();
        Can_MainFunction_Write();

        /* CanTp + Dcm for UDS processing */
        CanTp_MainFunction();
        Dcm_MainFunction();
        DoIp_Posix_MainFunction();

        Sil_Time_Sleep((uint32)TCU_RTE_PERIOD_MS * 1000u); /* tick from codegen */
    }

    /* ---- Shutdown ---- */

    (void)Can_SetControllerMode(0u, CAN_CS_STOPPED);
    DoIp_Posix_Deinit();
    (void)printf("[TCU] Shutdown complete\n");

    return 0;
}
