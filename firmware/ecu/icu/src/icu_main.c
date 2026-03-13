/**
 * @file    icu_main.c
 * @brief   ICU main entry point — BSW init, ncurses init, 50ms tick loop
 * @date    2026-02-23
 *
 * @safety_req SWR-ICU-001, SWR-ICU-010
 * @traces_to  SSR-ICU-001, SSR-ICU-010, TSR-046, TSR-047
 *
 * @details  Initializes BSW stack (Can, CanIf, PduR, Com, Rte),
 *           ncurses terminal UI, SWC modules (Dashboard, DtcDisplay),
 *           then enters the main loop at 50ms tick (20 Hz).
 *
 *           ICU is a simulated ECU running in Docker on Linux.
 *           It subscribes to ALL CAN messages (listen-only consumer)
 *           and displays gauges, warnings, DTC list, and ECU health.
 *
 *           Headless mode: if no terminal is available (e.g. nohup on
 *           Raspberry Pi), ncurses is skipped — CAN processing continues.
 *
 *           Signal handler for SIGINT/SIGTERM ensures ncurses cleanup
 *           before exit.
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Std_Types.h"
#include "Sil_Time.h"
#include "Icu_Cfg.h"

/* ==================================================================
 * BSW Module Headers
 * ================================================================== */

#include "Can.h"
#include "CanIf.h"
#include "PduR.h"
#include "Com.h"
#include "Rte.h"

/* ==================================================================
 * SWC Headers
 * ================================================================== */

#include "Swc_Dashboard.h"
#include "Swc_DtcDisplay.h"

/* ==================================================================
 * POSIX Headers (Linux / Docker environment)
 * ================================================================== */

#ifndef PLATFORM_POSIX_TEST

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>

#endif /* PLATFORM_POSIX_TEST */

/* ==================================================================
 * External Configuration (defined in cfg/ files)
 * ================================================================== */

extern const Rte_ConfigType  icu_rte_config;
extern const Com_ConfigType  icu_com_config;

/* ==================================================================
 * Static Configuration Constants
 * ================================================================== */

/** CAN driver configuration — 500 kbps, controller 0 */
static const Can_ConfigType can_config = {
    .baudrate     = 500000u,
    .controllerId = 0u,
};

/** CanIf TX PDU routing: Com TX PDU -> CAN ID */
static const CanIf_TxPduConfigType canif_tx_config[] = {
    /* canId,  upperPduId,                dlc, hth */
    { 0x014u, ICU_COM_TX_HEARTBEAT,       8u, 0u },  /* ICU heartbeat */
};

/** CanIf RX PDU routing: CAN ID -> Com RX PDU */
static const CanIf_RxPduConfigType canif_rx_config[] = {
    /* canId,  upperPduId,               dlc, isExtended */
    { 0x001u, ICU_COM_RX_ESTOP,           8u, FALSE },  /* E-stop broadcast    */
    { 0x010u, ICU_COM_RX_HB_CVC,          8u, FALSE },  /* CVC heartbeat       */
    { 0x011u, ICU_COM_RX_HB_FZC,          8u, FALSE },  /* FZC heartbeat       */
    { 0x012u, ICU_COM_RX_HB_RZC,          8u, FALSE },  /* RZC heartbeat       */
    { 0x100u, ICU_COM_RX_VEHICLE_STATE,   8u, FALSE },  /* Vehicle state       */
    { 0x101u, ICU_COM_RX_TORQUE_REQ,      8u, FALSE },  /* Torque request      */
    { 0x301u, ICU_COM_RX_MOTOR_CURRENT,   8u, FALSE },  /* Motor current/RPM   */
    { 0x302u, ICU_COM_RX_MOTOR_TEMP,      8u, FALSE },  /* Motor temperature   */
    { 0x303u, ICU_COM_RX_BATTERY,         8u, FALSE },  /* Battery voltage     */
    { 0x400u, ICU_COM_RX_LIGHT_STATUS,    8u, FALSE },  /* Light status        */
    { 0x401u, ICU_COM_RX_INDICATOR,       8u, FALSE },  /* Turn indicator      */
    { 0x402u, ICU_COM_RX_DOOR_LOCK,       8u, FALSE },  /* Door lock           */
    { 0x500u, ICU_COM_RX_DTC_BCAST,       8u, FALSE },  /* DTC broadcast       */
};

static const CanIf_ConfigType canif_config = {
    .txPduConfig = canif_tx_config,
    .txPduCount  = (uint8)(sizeof(canif_tx_config) / sizeof(canif_tx_config[0])),
    .rxPduConfig = canif_rx_config,
    .rxPduCount  = (uint8)(sizeof(canif_rx_config) / sizeof(canif_rx_config[0])),
    .e2eRxCheck  = NULL_PTR,
};

/** PduR routing: all RX PDUs route to Com (no DCM for ICU) */
static const PduR_RoutingTableType pdur_routing[] = {
    { ICU_COM_RX_ESTOP,          PDUR_DEST_COM, ICU_COM_RX_ESTOP         },
    { ICU_COM_RX_HB_CVC,         PDUR_DEST_COM, ICU_COM_RX_HB_CVC        },
    { ICU_COM_RX_HB_FZC,         PDUR_DEST_COM, ICU_COM_RX_HB_FZC        },
    { ICU_COM_RX_HB_RZC,         PDUR_DEST_COM, ICU_COM_RX_HB_RZC        },
    { ICU_COM_RX_VEHICLE_STATE,  PDUR_DEST_COM, ICU_COM_RX_VEHICLE_STATE },
    { ICU_COM_RX_TORQUE_REQ,     PDUR_DEST_COM, ICU_COM_RX_TORQUE_REQ    },
    { ICU_COM_RX_MOTOR_CURRENT,  PDUR_DEST_COM, ICU_COM_RX_MOTOR_CURRENT },
    { ICU_COM_RX_MOTOR_TEMP,     PDUR_DEST_COM, ICU_COM_RX_MOTOR_TEMP    },
    { ICU_COM_RX_BATTERY,        PDUR_DEST_COM, ICU_COM_RX_BATTERY       },
    { ICU_COM_RX_LIGHT_STATUS,   PDUR_DEST_COM, ICU_COM_RX_LIGHT_STATUS  },
    { ICU_COM_RX_INDICATOR,      PDUR_DEST_COM, ICU_COM_RX_INDICATOR     },
    { ICU_COM_RX_DOOR_LOCK,      PDUR_DEST_COM, ICU_COM_RX_DOOR_LOCK     },
    { ICU_COM_RX_DTC_BCAST,      PDUR_DEST_COM, ICU_COM_RX_DTC_BCAST     },
};

static const PduR_ConfigType pdur_config = {
    .routingTable = pdur_routing,
    .routingCount = (uint8)(sizeof(pdur_routing) / sizeof(pdur_routing[0])),
};

/* ==================================================================
 * ICU Heartbeat — transmits CAN 0x014 every 500ms
 * ================================================================== */

/** Alive counter for ICU heartbeat (wraps 0-255) */
static uint8 icu_hb_alive_counter = 0u;

/**
 * @brief  ICU heartbeat transmit — called every 500ms by RTE scheduler
 * @note   Packs alive counter (signal 19) and ECU ID (signal 20) into
 *         TX PDU 0 (CAN 0x014) via Com_SendSignal.
 */
extern void Icu_Heartbeat_500ms(void);
void Icu_Heartbeat_500ms(void)
{
    uint8 alive = icu_hb_alive_counter;
    uint8 ecu_id = 0x04u;  /* ICU = ECU 4 */

    (void)Com_SendSignal(3u, &alive);    /* signal 3 = heartbeat AliveCounter */
    (void)Com_SendSignal(4u, &ecu_id);   /* signal 4 = heartbeat ECU_ID */

    icu_hb_alive_counter++;
}

/* ==================================================================
 * Signal Handler — clean up ncurses on SIGINT/SIGTERM
 * ================================================================== */

#ifndef PLATFORM_POSIX_TEST

static volatile sig_atomic_t running = 1;

/** Tracks whether ncurses was successfully initialized (0 = headless) */
static int ncurses_active = 0;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

#endif /* PLATFORM_POSIX_TEST */

/* ==================================================================
 * Main Entry Point
 * ================================================================== */

/**
 * @brief  ICU main function — init BSW, ncurses, SWCs, then 50ms loop
 *
 * @safety_req SWR-ICU-001, SWR-ICU-010
 * @traces_to  SSR-ICU-001, SSR-ICU-010, TSR-046, TSR-047
 */
#ifndef PLATFORM_POSIX_TEST

int main(void)
{
    /* ---- Step 1: Install signal handlers ---- */
    {
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sa.sa_flags   = 0;
        sigemptyset(&sa.sa_mask);
        (void)sigaction(SIGINT,  &sa, NULL_PTR);
        (void)sigaction(SIGTERM, &sa, NULL_PTR);
    }

    /* ---- Step 2: BSW module initialization (order matters) ---- */
    Can_Init(&can_config);
    CanIf_Init(&canif_config);
    PduR_Init(&pdur_config);
    Com_Init(&icu_com_config);
    Rte_Init(&icu_rte_config);

    /* ---- Step 3: ncurses terminal UI initialization ---- */
    /* Graceful fallback: if no terminal is available (headless Pi, nohup),
     * skip ncurses and run CAN processing only.  Rendering is TODO stubs
     * anyway, so nothing is lost in headless mode. */
    {
        const char *term = getenv("TERM");
        if ((term != NULL) && (term[0] != '\0')) {
            WINDOW *win = initscr();
            if (win != NULL) {
                ncurses_active = 1;
                (void)cbreak();
                (void)noecho();
                (void)start_color();
                (void)curs_set(0);

                /* Initialize color pairs */
                (void)init_pair(ICU_COLOR_GREEN,  COLOR_GREEN,  COLOR_BLACK);
                (void)init_pair(ICU_COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
                (void)init_pair(ICU_COLOR_ORANGE, COLOR_RED,    COLOR_YELLOW);
                (void)init_pair(ICU_COLOR_RED,    COLOR_RED,    COLOR_BLACK);
                (void)init_pair(ICU_COLOR_WHITE,  COLOR_WHITE,  COLOR_BLACK);
            }
        }
    }

    if (ncurses_active == 0) {
        (void)fprintf(stderr, "[ICU] No terminal — running headless (CAN-only)\n");
    }

    /* ---- Step 4: SWC initialization ---- */
    Swc_Dashboard_Init();
    Swc_DtcDisplay_Init();

    /* ---- Step 5: Start CAN controller ---- */
    (void)Can_SetControllerMode(0u, CAN_CS_STARTED);

    /* ---- Step 6: Main loop — 50ms tick (20 Hz) ---- */
    uint8 hb_tick = 0u;

    while (running != 0) {
        /* Sleep 50ms (50000 microseconds) */
        Sil_Time_Sleep(50000u); /* 50ms virtual tick */

        /* BSW CAN processing: receive frames from bus */
        Can_MainFunction_Read();
        Com_MainFunction_Rx();

        /* Drive RTE scheduler (fires runnables based on period) */
        Rte_MainFunction();

        /* ICU heartbeat every 500ms (10 ticks × 50ms) */
        hb_tick++;
        if (hb_tick >= 10u) {
            hb_tick = 0u;
            Icu_Heartbeat_500ms();
        }

        /* BSW CAN processing: transmit queued frames */
        Com_MainFunction_Tx();

        /* Check for bus-off condition */
        Can_MainFunction_BusOff();
    }

    /* ---- Step 7: Clean shutdown ---- */
    if (ncurses_active != 0) {
        (void)endwin();
    }

    return 0;
}

#endif /* PLATFORM_POSIX_TEST */
