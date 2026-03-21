/**
 * @file    Swc_Heartbeat.c
 * @brief   Heartbeat TX and RX timeout monitoring
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-021, SWR-CVC-022
 * @traces_to  SSR-CVC-021, SSR-CVC-022, TSR-022, TSR-046
 *
 * @details  TX: Sends CVC heartbeat on CAN every 50ms containing alive
 *           counter (4-bit, wraps at 15), ECU ID, and vehicle state.
 *           E2E protection applied before transmission.
 *
 *           RX: Monitors FZC and RZC heartbeat reception via E2E State
 *           Machine (sliding window evaluator). At each 50ms TX boundary,
 *           polls Com shadow buffers for alive counter changes, feeds
 *           E2E_STATUS_OK or E2E_STATUS_NO_NEW_DATA to per-ECU SM.
 *           SM status maps to comm status: VALID→OK, INIT/INVALID→TIMEOUT.
 *           DTC reported on transitions.
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (ASIL D)
 * @copyright Taktflow Systems 2026
 */
#include "Swc_Heartbeat.h"
#include "Cvc_Cfg.h"
#include "Com.h"
#include "PduR.h"
#include "E2E.h"
#include "E2E_Sm.h"
#include "Rte.h"
#include "Dem.h"
#include "WdgM.h"

/* SIL diagnostic logging — compile with -DSIL_DIAG to enable */
#ifdef SIL_DIAG
#include <stdio.h>
#define HB_DIAG(fmt, ...) (void)fprintf(stderr, "[HB] " fmt "\n", ##__VA_ARGS__)
#else
#define HB_DIAG(fmt, ...) ((void)0)
#endif

/* ====================================================================
 * Internal constants (derived from config — compile-time safe)
 * ==================================================================== */

/** @brief TX timer threshold: derived from CVC_HB_TX_PERIOD_MS / CVC_RTE_PERIOD_MS
 *  @safety_req SWR-CVC-021 */
#define HB_TX_CYCLES  (CVC_HB_TX_PERIOD_MS / CVC_RTE_PERIOD_MS)

_Static_assert(CVC_HB_TX_PERIOD_MS % CVC_RTE_PERIOD_MS == 0u,
               "HB TX period must be exact multiple of RTE period");
_Static_assert(HB_TX_CYCLES > 0u,
               "HB TX cycles must be positive");

/** @brief Heartbeat PDU length in bytes */
#define HB_PDU_LENGTH 8u

/* ====================================================================
 * Static module state
 * ==================================================================== */

static uint8   alive_counter;
static uint8   tx_timer;

static boolean fzc_rx_flag;
static boolean rzc_rx_flag;

static uint8   fzc_comm_status;
static uint8   rzc_comm_status;

/** Last seen alive counter values from Com shadow buffers (poll-based detection) */
static uint8   fzc_last_alive;
static uint8   rzc_last_alive;

static boolean initialized;

/* E2E protection moved to Com_MainFunction_Tx (Phase 2) */

/** @brief E2E SM per-ECU configurations (const, flash)
 *  @safety_req SWR-CVC-022 */
static const E2E_SmConfigType fzc_sm_config = {
    CVC_E2E_SM_FZC_WINDOW,
    CVC_E2E_SM_FZC_MIN_OK_INIT,
    CVC_E2E_SM_FZC_MAX_ERR_VALID,
    CVC_E2E_SM_FZC_MIN_OK_INV
};

static const E2E_SmConfigType rzc_sm_config = {
    CVC_E2E_SM_RZC_WINDOW,
    CVC_E2E_SM_RZC_MIN_OK_INIT,
    CVC_E2E_SM_RZC_MAX_ERR_VALID,
    CVC_E2E_SM_RZC_MIN_OK_INV
};

/** @brief E2E SM per-ECU runtime state */
static E2E_SmStateType fzc_sm_state;
static E2E_SmStateType rzc_sm_state;

/* ====================================================================
 * Public functions
 * ==================================================================== */

/**
 * @brief  Initialise all heartbeat state to safe defaults
 */
void Swc_Heartbeat_Init(void)
{
    alive_counter   = 0u;
    tx_timer        = 0u;

    fzc_rx_flag     = FALSE;
    rzc_rx_flag     = FALSE;

    fzc_comm_status = CVC_COMM_TIMEOUT;
    rzc_comm_status = CVC_COMM_TIMEOUT;

    fzc_last_alive  = 0u;   /* Match Com shadow buffer init (0) — no false positive */
    rzc_last_alive  = 0u;   /* Real detection starts when alive counter changes     */

    hb_e2e_state.Counter = 0u;

    E2E_Sm_Init(&fzc_sm_state);
    E2E_Sm_Init(&rzc_sm_state);

    initialized     = TRUE;
}

/**
 * @brief  10ms cyclic — heartbeat TX and RX timeout monitoring
 *
 * @details Execution flow:
 *   1. TX: count cycles, transmit at 50ms intervals
 *   2. RX poll: detect alive counter changes in Com shadow buffers
 *   3. E2E SM: feed OK/NO_NEW_DATA per ECU, evaluate window
 *   4. DTC: report on SM-driven comm status transitions
 *   5. Write comm status to RTE every cycle
 */
void Swc_Heartbeat_MainFunction(void)
{
    if (initialized == FALSE) {
        return;
    }

    /* --- 1. TX timing --------------------------------------------- */
    tx_timer++;

    if (tx_timer >= HB_TX_CYCLES) {
        uint8  pdu[HB_PDU_LENGTH] = {0u};
        uint32 vehicle_state = 0u;

        /* Read current vehicle state from RTE */
        (void)Rte_Read(CVC_SIG_VEHICLE_STATE, &vehicle_state);

        /* Heartbeat TX: Swc_CvcCom bridge reads vehicle state + sends via Com */

        /* WdgM checkpoint: SE 3 alive indication at TX boundary */
        (void)WdgM_CheckpointReached(3u);

        /* Increment alive counter with wrap */
        alive_counter++;
        if (alive_counter > CVC_HB_ALIVE_MAX) {
            alive_counter = 0u;
        }

        /* Reset TX timer */
        tx_timer = 0u;

        /* --- 2. RX monitoring (checked at TX boundary = 50ms) ----- */

        /* Poll Com shadow buffers for alive counter changes.
         * Com_RxIndication unpacks heartbeat PDUs into shadow buffers
         * but does not call Swc_Heartbeat_RxIndication, so we detect
         * heartbeat reception by checking if the alive counter changed. */
        {
            uint32 fzc_alive = 0u;
            uint32 rzc_alive = 0u;

            (void)Rte_Read(CVC_SIG_FZC_HEARTBEAT_E_2_E_ALIVE_COUNTER, &fzc_alive);
            (void)Rte_Read(CVC_SIG_RZC_HEARTBEAT_E_2_E_ALIVE_COUNTER, &rzc_alive);

            if (fzc_alive != fzc_last_alive) {
                fzc_rx_flag    = TRUE;
                fzc_last_alive = fzc_alive;
            }

            if (rzc_alive != rzc_last_alive) {
                rzc_rx_flag    = TRUE;
                rzc_last_alive = rzc_alive;
            }
        }

        /* --- 3. E2E SM evaluation ------------------------------------ */
        {
            E2E_CheckStatusType fzc_profile;
            E2E_CheckStatusType rzc_profile;
            E2E_SmStatusType    fzc_sm;
            E2E_SmStatusType    rzc_sm;
            uint8               fzc_new_comm;
            uint8               rzc_new_comm;

            /* Map rx flags to E2E profile status */
            fzc_profile = (fzc_rx_flag == TRUE) ? E2E_STATUS_OK
                                                 : E2E_STATUS_NO_NEW_DATA;
            rzc_profile = (rzc_rx_flag == TRUE) ? E2E_STATUS_OK
                                                 : E2E_STATUS_NO_NEW_DATA;
            fzc_rx_flag = FALSE;
            rzc_rx_flag = FALSE;

            /* Feed into per-ECU state machines */
            fzc_sm = E2E_Sm_Check(&fzc_sm_config, &fzc_sm_state, fzc_profile);
            rzc_sm = E2E_Sm_Check(&rzc_sm_config, &rzc_sm_state, rzc_profile);

            /* Map SM status → comm status */
            fzc_new_comm = (fzc_sm == E2E_SM_VALID) ? CVC_COMM_OK
                                                      : CVC_COMM_TIMEOUT;
            rzc_new_comm = (rzc_sm == E2E_SM_VALID) ? CVC_COMM_OK
                                                      : CVC_COMM_TIMEOUT;

            /* DTC on FZC transition */
            if (fzc_new_comm != fzc_comm_status) {
                if (fzc_new_comm == CVC_COMM_TIMEOUT) {
                    HB_DIAG("FZC: OK -> TIMEOUT");
                    Dem_ReportErrorStatus(CVC_DTC_CAN_FZC_TIMEOUT,
                                          DEM_EVENT_STATUS_FAILED);
                } else {
                    HB_DIAG("FZC: TIMEOUT -> OK (alive=%u)", (unsigned)fzc_last_alive);
                    Dem_ReportErrorStatus(CVC_DTC_CAN_FZC_TIMEOUT,
                                          DEM_EVENT_STATUS_PASSED);
                }
                fzc_comm_status = fzc_new_comm;
            }

            /* DTC on RZC transition */
            if (rzc_new_comm != rzc_comm_status) {
                if (rzc_new_comm == CVC_COMM_TIMEOUT) {
                    HB_DIAG("RZC: OK -> TIMEOUT");
                    Dem_ReportErrorStatus(CVC_DTC_CAN_RZC_TIMEOUT,
                                          DEM_EVENT_STATUS_FAILED);
                } else {
                    HB_DIAG("RZC: TIMEOUT -> OK (alive=%u)", (unsigned)rzc_last_alive);
                    Dem_ReportErrorStatus(CVC_DTC_CAN_RZC_TIMEOUT,
                                          DEM_EVENT_STATUS_PASSED);
                }
                rzc_comm_status = rzc_new_comm;
            }
        }
    }

    /* --- 5. Write comm status to RTE every cycle ------------------ */
    (void)Rte_Write(CVC_SIG_FZC_COMM_STATUS, (uint32)fzc_comm_status);
    (void)Rte_Write(CVC_SIG_RZC_COMM_STATUS, (uint32)rzc_comm_status);
}

/**
 * @brief  RX indication — called by Com layer when heartbeat received
 * @param  ecuId  Source ECU ID (CVC_ECU_ID_FZC or CVC_ECU_ID_RZC)
 */
void Swc_Heartbeat_RxIndication(uint8 ecuId)
{
    if (ecuId == CVC_ECU_ID_FZC) {
        fzc_rx_flag = TRUE;
    } else if (ecuId == CVC_ECU_ID_RZC) {
        rzc_rx_flag = TRUE;
    } else {
        /* Unknown ECU ID — ignore */
    }
}

/**
 * @brief  Reset comm status to OK — called when post-INIT grace expires
 *
 * Clears stale TIMEOUT status accumulated during Docker boot transient
 * so the first post-grace cycle doesn't immediately trigger CAN_TMO_S.
 */
void Swc_Heartbeat_ResetCommStatus(void)
{
    fzc_comm_status = CVC_COMM_OK;
    rzc_comm_status = CVC_COMM_OK;
    /* Force SM to VALID — skip the MIN_OK_INIT window requirement.
     * The grace period already absorbed the boot transient; heartbeats
     * are now arriving normally.  Without this, SM starts in INIT and
     * needs 2 OKs (100ms) before VALID — but the CAN timeout check
     * runs on the very next 10ms cycle and sees INIT → TIMEOUT. */
    E2E_Sm_Init(&fzc_sm_state);
    E2E_Sm_Init(&rzc_sm_state);
    fzc_sm_state.Status = E2E_SM_VALID;
    rzc_sm_state.Status = E2E_SM_VALID;
    (void)Rte_Write(CVC_SIG_FZC_COMM_STATUS, (uint32)CVC_COMM_OK);
    (void)Rte_Write(CVC_SIG_RZC_COMM_STATUS, (uint32)CVC_COMM_OK);
}
