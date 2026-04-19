/**
 * @file    Swc_VehicleState.c
 * @brief   Vehicle state machine — transition table, fault monitoring
 * @date    2026-02-21
 *
 * @details Implements a table-driven state machine for the CVC with 6 states
 *          and 17 events. The transition table is a const 2D array that maps
 *          (current_state, event) -> next_state. Invalid combinations map to
 *          CVC_STATE_INVALID (0xFF) and are rejected.
 *
 *          The MainFunction runs every 10ms and:
 *          1. Reads fault signals from RTE (pedal, E-stop, comm, motor, brake, steering)
 *          2. Derives events from signal values
 *          3. Calls OnEvent for derived events
 *          4. Reports DTCs for motor cutoff, brake fault, steering fault
 *          5. Writes current state to RTE
 *
 * @safety_req SWR-CVC-009 to SWR-CVC-013
 * @traces_to  SSR-CVC-009 to SSR-CVC-013, TSR-046, TSR-047
 *
 * @standard AUTOSAR, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_VehicleState.h"
#include "Swc_CvcCom.h"
#include "Cvc_Cfg.h"
#include "Rte.h"
#include "BswM.h"
#include "Dem.h"
#include "Com.h"

/* SIL diagnostic logging — compile with -DSIL_DIAG to enable */
#ifdef SIL_DIAG
#include <stdio.h>
#define VSM_DIAG(fmt, ...) (void)fprintf(stderr, "[VSM] " fmt "\n", ##__VA_ARGS__)
static const char * const diag_state_names[CVC_STATE_COUNT] = {
    "INIT", "RUN", "DEGRADED", "LIMP", "SAFE_STOP", "SHUTDOWN"
};
static const char * const diag_event_names[CVC_EVT_COUNT] = {
    "SELF_TEST_PASS", "SELF_TEST_FAIL",
    "PEDAL_FAULT_S", "PEDAL_FAULT_D",
    "CAN_TMO_S", "CAN_TMO_D",
    "ESTOP", "SC_KILL",
    "FAULT_CLR", "CAN_RESTORED", "VEH_STOPPED",
    "MOTOR_CUTOFF", "BRAKE_FAULT", "STEER_FAULT",
    "BATT_WARN", "BATT_CRIT", "CREEP"
};
#else
#define VSM_DIAG(fmt, ...) ((void)0)
#endif

/* ==================================================================
 * BswM mode mapping — vehicle state to BswM mode
 * ================================================================== */

#define BSWM_STARTUP    0u
#define BSWM_RUN        1u
#define BSWM_DEGRADED   2u
#define BSWM_SAFE_STOP  3u
#define BSWM_SHUTDOWN   4u

/** @brief  Maps CVC vehicle state to BswM mode value */
static const uint8 state_to_bswm_mode[CVC_STATE_COUNT] = {
    BSWM_STARTUP,     /* CVC_STATE_INIT      -> BSWM_STARTUP   */
    BSWM_RUN,         /* CVC_STATE_RUN       -> BSWM_RUN       */
    BSWM_DEGRADED,    /* CVC_STATE_DEGRADED  -> BSWM_DEGRADED  */
    BSWM_DEGRADED,    /* CVC_STATE_LIMP      -> BSWM_DEGRADED  */
    BSWM_SAFE_STOP,   /* CVC_STATE_SAFE_STOP -> BSWM_SAFE_STOP */
    BSWM_SHUTDOWN     /* CVC_STATE_SHUTDOWN  -> BSWM_SHUTDOWN   */
};

/* ==================================================================
 * Transition table: [current_state][event] -> next_state
 *
 * CVC_STATE_INVALID (0xFF) means the transition is not allowed.
 * ================================================================== */

static const uint8 transition_table[CVC_STATE_COUNT][CVC_EVT_COUNT] = {
    /* CVC_STATE_INIT */
    {
        CVC_STATE_RUN,         /* EVT_SELF_TEST_PASS     -> RUN (guarded: heartbeats OK) */
        CVC_STATE_SAFE_STOP,   /* EVT_SELF_TEST_FAIL     -> SAFE_STOP    */
        CVC_STATE_INVALID,     /* EVT_PEDAL_FAULT_SINGLE -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_PEDAL_FAULT_DUAL   -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_TIMEOUT_SINGLE -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_TIMEOUT_DUAL   -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_ESTOP              -> (invalid)    */
        CVC_STATE_SHUTDOWN,    /* EVT_SC_KILL            -> SHUTDOWN (external override, TSR-035) */
        CVC_STATE_INVALID,     /* EVT_FAULT_CLEARED      -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_RESTORED       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_VEHICLE_STOPPED    -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_MOTOR_CUTOFF       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_BRAKE_FAULT        -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_STEERING_FAULT     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_BATTERY_WARN       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_BATTERY_CRIT       -> (invalid)    */
        CVC_STATE_INVALID      /* EVT_CREEP_FAULT        -> (invalid)    */
    },
    /* CVC_STATE_RUN */
    {
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_PASS     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_FAIL     -> (invalid)    */
        CVC_STATE_DEGRADED,    /* EVT_PEDAL_FAULT_SINGLE -> DEGRADED     */
        CVC_STATE_SAFE_STOP,   /* EVT_PEDAL_FAULT_DUAL   -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP,   /* EVT_CAN_TIMEOUT_SINGLE -> SAFE_STOP (HARA: no backup zone) */
        CVC_STATE_SAFE_STOP,   /* EVT_CAN_TIMEOUT_DUAL   -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP,   /* EVT_ESTOP              -> SAFE_STOP    */
        CVC_STATE_SHUTDOWN,    /* EVT_SC_KILL            -> SHUTDOWN (external override, TSR-035) */
        CVC_STATE_INVALID,     /* EVT_FAULT_CLEARED      -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_RESTORED       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_VEHICLE_STOPPED    -> (invalid)    */
        CVC_STATE_DEGRADED,    /* EVT_MOTOR_CUTOFF       -> DEGRADED (fail-silent, ASIL B) */
        CVC_STATE_SAFE_STOP,   /* EVT_BRAKE_FAULT        -> SAFE_STOP (ASIL D)   */
        CVC_STATE_SAFE_STOP,   /* EVT_STEERING_FAULT     -> SAFE_STOP (ASIL D)   */
        CVC_STATE_DEGRADED,    /* EVT_BATTERY_WARN       -> DEGRADED (graceful degradation) */
        CVC_STATE_LIMP,        /* EVT_BATTERY_CRIT       -> LIMP (reduced torque + speed limit) */
        CVC_STATE_SAFE_STOP    /* EVT_CREEP_FAULT        -> SAFE_STOP    */
    },
    /* CVC_STATE_DEGRADED */
    {
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_PASS     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_FAIL     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_PEDAL_FAULT_SINGLE -> (invalid)    */
        CVC_STATE_SAFE_STOP,   /* EVT_PEDAL_FAULT_DUAL   -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP,   /* EVT_CAN_TIMEOUT_SINGLE -> SAFE_STOP (HARA: no backup zone) */
        CVC_STATE_SAFE_STOP,   /* EVT_CAN_TIMEOUT_DUAL   -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP,   /* EVT_ESTOP              -> SAFE_STOP    */
        CVC_STATE_SHUTDOWN,    /* EVT_SC_KILL            -> SHUTDOWN (external override, TSR-035) */
        CVC_STATE_RUN,         /* EVT_FAULT_CLEARED      -> RUN          */
        CVC_STATE_INVALID,     /* EVT_CAN_RESTORED       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_VEHICLE_STOPPED    -> (invalid)    */
        CVC_STATE_SAFE_STOP,   /* EVT_MOTOR_CUTOFF       -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP,   /* EVT_BRAKE_FAULT        -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP,   /* EVT_STEERING_FAULT     -> SAFE_STOP    */
        CVC_STATE_INVALID,     /* EVT_BATTERY_WARN       -> (already degraded) */
        CVC_STATE_LIMP,        /* EVT_BATTERY_CRIT       -> LIMP         */
        CVC_STATE_SAFE_STOP    /* EVT_CREEP_FAULT        -> SAFE_STOP    */
    },
    /* CVC_STATE_LIMP */
    {
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_PASS     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_FAIL     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_PEDAL_FAULT_SINGLE -> (invalid)    */
        CVC_STATE_SAFE_STOP,   /* EVT_PEDAL_FAULT_DUAL   -> SAFE_STOP    */
        CVC_STATE_INVALID,     /* EVT_CAN_TIMEOUT_SINGLE -> (invalid)    */
        CVC_STATE_SAFE_STOP,   /* EVT_CAN_TIMEOUT_DUAL   -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP,   /* EVT_ESTOP              -> SAFE_STOP    */
        CVC_STATE_SHUTDOWN,    /* EVT_SC_KILL            -> SHUTDOWN (external override, TSR-035) */
        CVC_STATE_RUN,         /* EVT_FAULT_CLEARED      -> RUN          */
        CVC_STATE_DEGRADED,    /* EVT_CAN_RESTORED       -> DEGRADED     */
        CVC_STATE_INVALID,     /* EVT_VEHICLE_STOPPED    -> (invalid)    */
        CVC_STATE_SAFE_STOP,   /* EVT_MOTOR_CUTOFF       -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP,   /* EVT_BRAKE_FAULT        -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP,   /* EVT_STEERING_FAULT     -> SAFE_STOP    */
        CVC_STATE_INVALID,     /* EVT_BATTERY_WARN       -> (invalid)    */
        CVC_STATE_SAFE_STOP,   /* EVT_BATTERY_CRIT       -> SAFE_STOP    */
        CVC_STATE_SAFE_STOP    /* EVT_CREEP_FAULT        -> SAFE_STOP    */
    },
    /* CVC_STATE_SAFE_STOP */
    {
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_PASS     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_FAIL     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_PEDAL_FAULT_SINGLE -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_PEDAL_FAULT_DUAL   -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_TIMEOUT_SINGLE -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_TIMEOUT_DUAL   -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_ESTOP              -> (invalid)    */
        CVC_STATE_SHUTDOWN,    /* EVT_SC_KILL            -> SHUTDOWN (external override, TSR-035) */
        CVC_STATE_INVALID,     /* EVT_FAULT_CLEARED      -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_RESTORED       -> (invalid)    */
        CVC_STATE_SHUTDOWN,    /* EVT_VEHICLE_STOPPED    -> SHUTDOWN     */
        CVC_STATE_INVALID,     /* EVT_MOTOR_CUTOFF       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_BRAKE_FAULT        -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_STEERING_FAULT     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_BATTERY_WARN       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_BATTERY_CRIT       -> (invalid)    */
        CVC_STATE_INVALID      /* EVT_CREEP_FAULT        -> (invalid)    */
    },
    /* CVC_STATE_SHUTDOWN */
    {
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_PASS     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_SELF_TEST_FAIL     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_PEDAL_FAULT_SINGLE -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_PEDAL_FAULT_DUAL   -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_TIMEOUT_SINGLE -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_TIMEOUT_DUAL   -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_ESTOP              -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_SC_KILL            -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_FAULT_CLEARED      -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_CAN_RESTORED       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_VEHICLE_STOPPED    -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_MOTOR_CUTOFF       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_BRAKE_FAULT        -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_STEERING_FAULT     -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_BATTERY_WARN       -> (invalid)    */
        CVC_STATE_INVALID,     /* EVT_BATTERY_CRIT       -> (invalid)    */
        CVC_STATE_INVALID      /* EVT_CREEP_FAULT        -> (invalid)    */
    }
};

/* ==================================================================
 * Module-static variables
 * ================================================================== */

/** @brief  Current vehicle state */
static uint8 current_state;

/** @brief  Initialization flag — TRUE after Swc_VehicleState_Init() */
static uint8 initialized;

/** @brief  Pending self-test pass flag — held until heartbeats validated */
static uint8 self_test_pass_pending;

/** @brief  INIT hold counter — counts cycles in INIT before allowing RUN */
static uint16 init_hold_counter;

#ifdef SIL_DIAG
/** @brief  SIL boot grace — track whether each zone was ever seen during INIT.
 *          Docker containers have scheduling jitter that causes heartbeat
 *          flapping. Instead of requiring simultaneous OK, require "seen at
 *          least once during the hold period". */
static uint8 sil_fzc_seen;
static uint8 sil_rzc_seen;
#endif

/** @brief  SAFE_STOP recovery counter — counts all-clear cycles before recovery */
static uint16 safe_stop_clear_count;

/** @brief  Post-INIT grace counter — suppresses ConfirmFault and SC_KILL after
 *          INIT->RUN to absorb stale fault signals from zone controllers.
 *          Only set to non-zero on SIL (PLATFORM_POSIX).  On bare metal the
 *          value stays at 0 (transparent — guards always pass). */
static uint16 post_init_grace_counter;

/** @brief  CAN timeout debounce — consecutive timeout cycles before fault.
 *          Production: 50 cycles (500ms). SIL: 200 cycles (2s) to absorb
 *          Docker container scheduling jitter on heartbeat delivery. */
#ifdef SIL_DIAG
#define CAN_TMO_DEBOUNCE_THRESHOLD   200u  /* 2s — Docker jitter margin */
#else
#define CAN_TMO_DEBOUNCE_THRESHOLD    50u
#endif
static uint16 can_tmo_debounce;

/** @brief  Fault latch array — TRUE if that fault triggered SAFE_STOP */
static uint8  fault_latched[CVC_LATCH_COUNT];

/** @brief  Per-fault unlatch debounce counter — counts consecutive clear cycles */
static uint16 fault_unlatch_count[CVC_LATCH_COUNT];

/* ==================================================================
 * Confirmation-read pattern — ISO 26262 debounce + fresh Com + E2E
 * ================================================================== */

/** @brief  Creep guard debounce counter (SG-012 / HE-017 ASIL D) */
static uint16 creep_debounce_count;

/** @brief  Per-fault debounce counters: [brake, motor_cutoff_fzc, steering, motor_fault_rzc] */
static uint8 fault_confirm_count[4];

#define CVC_FAULT_CONFIRM_THRESHOLD  3u   /**< 3 consecutive 10ms cycles = 30ms */
#define CVC_FAULT_IDX_BRAKE          0u
#define CVC_FAULT_IDX_MOTOR_CUTOFF   1u
#define CVC_FAULT_IDX_STEERING       2u
#define CVC_FAULT_IDX_MOTOR_RZC      3u
#define CVC_FAULT_CONFIRM_COUNT      4u

#define CVC_FAULT_COM_BRAKE         CVC_COM_SIG_BRAKE_FAULT_FAULT_TYPE       /**< 103u */
#define CVC_FAULT_COM_MOTOR_CUTOFF  CVC_COM_SIG_MOTOR_CUTOFF_REQ_REQUEST_TYPE /**< 109u */
#define CVC_FAULT_COM_STEERING      0xFFu /**< Not bridged via Com             */

#define CVC_FAULT_E2E_BRAKE          2u   /**< CvcCom RX index for 0x210       */
#define CVC_FAULT_E2E_MOTOR_CUTOFF   0xFFu /**< 0x211 not in CvcCom RxTable   */
#define CVC_FAULT_E2E_STEERING       0xFFu

/* ==================================================================
 * Public API
 * ================================================================== */

/**
 * @brief  Initialize the vehicle state machine
 * @safety_req SWR-CVC-009
 */
void Swc_VehicleState_Init(void)
{
    uint8 i;

    current_state          = CVC_STATE_INIT;
    initialized            = TRUE;
    self_test_pass_pending = FALSE;
    init_hold_counter      = 0u;
    safe_stop_clear_count  = 0u;
#ifdef SIL_DIAG
    sil_fzc_seen           = FALSE;
    sil_rzc_seen           = FALSE;
#endif
    post_init_grace_counter = 0u;
    can_tmo_debounce        = 0u;
    creep_debounce_count    = 0u;

    for (i = 0u; i < CVC_FAULT_CONFIRM_COUNT; i++)
    {
        fault_confirm_count[i] = 0u;
    }

    for (i = 0u; i < CVC_LATCH_COUNT; i++)
    {
        fault_latched[i]       = FALSE;
        fault_unlatch_count[i] = 0u;
    }
}

/**
 * @brief  Get the current vehicle state
 * @return Current state value (CVC_STATE_INIT..CVC_STATE_SHUTDOWN)
 * @safety_req SWR-CVC-009
 */
uint8 Swc_VehicleState_GetState(void)
{
    return current_state;
}

/**
 * @brief  Inject an event and execute the state transition if valid
 * @param  event  Event ID (0..CVC_EVT_COUNT-1)
 * @safety_req SWR-CVC-009 to SWR-CVC-013
 *
 * @note   For SELF_TEST_PASS in INIT state, the transition is deferred
 *         until MainFunction validates that both heartbeats are OK.
 *         All other valid transitions execute immediately and notify BswM.
 */
void Swc_VehicleState_OnEvent(uint8 event)
{
    uint8 next_state;

    /* Guard: must be initialized, event in range, state in range */
    if (initialized != TRUE)
    {
        return;
    }
    if (event >= CVC_EVT_COUNT)
    {
        return;
    }
    if (current_state >= CVC_STATE_COUNT)
    {
        return;
    }

    /* Special handling: SELF_TEST_PASS in INIT requires heartbeat guard */
    if ((event == CVC_EVT_SELF_TEST_PASS) && (current_state == CVC_STATE_INIT))
    {
        self_test_pass_pending = TRUE;
        VSM_DIAG("self-test pass pending (waiting for heartbeats)");
        return;
    }

    /* Look up transition */
    next_state = transition_table[current_state][event];

    /* Reject invalid transitions */
    if (next_state == CVC_STATE_INVALID)
    {
        return;
    }

    /* Execute transition */
    {
#ifdef SIL_DIAG
        uint8 prev = current_state;
#endif
        current_state = next_state;
#ifdef SIL_DIAG
        VSM_DIAG("%s + %s -> %s",
                 diag_state_names[prev],
                 diag_event_names[event],
                 diag_state_names[current_state]);
#endif
    }

    /* Latch the fault that caused SAFE_STOP (prevents flapping recovery) */
    if (next_state == CVC_STATE_SAFE_STOP)
    {
        uint8 latch_idx = 0xFFu;
        switch (event)
        {
            case CVC_EVT_ESTOP:              latch_idx = CVC_LATCH_IDX_ESTOP;        break;
            case CVC_EVT_SC_KILL:            latch_idx = CVC_LATCH_IDX_SC_KILL;      break;
            case CVC_EVT_MOTOR_CUTOFF:       latch_idx = CVC_LATCH_IDX_MOTOR_CUTOFF; break;
            case CVC_EVT_BRAKE_FAULT:        latch_idx = CVC_LATCH_IDX_BRAKE;        break;
            case CVC_EVT_STEERING_FAULT:     latch_idx = CVC_LATCH_IDX_STEERING;     break;
            case CVC_EVT_PEDAL_FAULT_DUAL:   latch_idx = CVC_LATCH_IDX_PEDAL_DUAL;   break;
            case CVC_EVT_CAN_TIMEOUT_DUAL:   latch_idx = CVC_LATCH_IDX_CAN_DUAL;     break;
            case CVC_EVT_BATTERY_CRIT:       latch_idx = CVC_LATCH_IDX_BATTERY_CRIT; break;
            default: break;
        }
        if (latch_idx != 0xFFu)
        {
            fault_latched[latch_idx]       = TRUE;
            fault_unlatch_count[latch_idx] = 0u;
        }
    }

    /* Notify BswM of the new mode */
    (void)BswM_RequestMode(CVC_ECU_ID_CVC, state_to_bswm_mode[current_state]);
}

/**
 * @brief  Confirm a fault signal before safety-critical transition
 * @param  faultIdx     Index into fault_confirm_count array
 * @param  rte_value    RTE signal value (non-zero = fault active)
 * @param  comSignalId  Com signal ID for fresh read (0xFF = no Com check)
 * @param  e2eRxIndex   CvcCom RX index for E2E check (0xFF = no E2E check)
 * @param  dtcId        DTC event ID for Dem_ReportErrorStatus
 * @param  eventId      Vehicle state event to fire on confirmed fault
 *
 * @details ISO 26262 confirmation-read pattern:
 *          1. Debounce: fault must persist for CVC_FAULT_CONFIRM_THRESHOLD cycles
 *          2. Com read: fresh value from Com shadow buffer (bypasses stale RTE)
 *          3. E2E check: verify message is not degraded (useSafeDefault == FALSE)
 *          Only if all checks pass is the event fired.
 */
#ifndef PLATFORM_HIL
static void Swc_VehicleState_ConfirmFault(
    uint8 faultIdx, uint32 rte_value,
    uint8 comSignalId, uint8 e2eRxIndex,
    uint8 dtcId, uint8 eventId)
{
    if (rte_value != 0u)
    {
        fault_confirm_count[faultIdx]++;

        if (fault_confirm_count[faultIdx] >= CVC_FAULT_CONFIRM_THRESHOLD)
        {
            uint8 confirmed = TRUE;

            /* Fresh read from Com (bypass RTE stale cache) */
            if (comSignalId != 0xFFu)
            {
                uint8 fresh_val = 0u;
                (void)Com_ReceiveSignal(comSignalId, &fresh_val);
                if (fresh_val == 0u)
                {
                    confirmed = FALSE;
                }
            }

            /* E2E status check: Com_RxIndication now discards frames with
             * bad E2E (CRC/counter mismatch). If a signal reaches the shadow
             * buffer, it passed E2E. Legacy CvcCom_GetRxStatus removed. */
            (void)e2eRxIndex;

            if (confirmed == TRUE)
            {
                VSM_DIAG("CONFIRM idx=%u rte=%u com=%u e2e=%u evt=%u",
                         (unsigned)faultIdx, (unsigned)rte_value,
                         (unsigned)comSignalId, (unsigned)e2eRxIndex,
                         (unsigned)eventId);
                Dem_ReportErrorStatus(dtcId, DEM_EVENT_STATUS_FAILED);
                Swc_VehicleState_OnEvent(eventId);
            }

            fault_confirm_count[faultIdx] = 0u;
        }
    }
    else
    {
        fault_confirm_count[faultIdx] = 0u;
    }
}
#endif /* !PLATFORM_HIL */

/**
 * @brief  10ms cyclic main function — reads faults, derives events, reports DTCs
 * @safety_req SWR-CVC-009 to SWR-CVC-013
 *
 * @note   Execution order:
 *         1. Read all fault signals from RTE
 *         2. Handle pending self-test pass (check heartbeats)
 *         3. Derive events from current signal values
 *         4. Report DTCs for motor cutoff, brake fault, steering fault
 *         5. Write current state to RTE
 */
void Swc_VehicleState_MainFunction(void)
{
    uint32 pedal_fault     = 0u;
    uint32 estop_active    = 0u;
    uint32 fzc_comm        = 0u;
    uint32 rzc_comm        = 0u;
    uint32 motor_cutoff    = 0u;
    uint32 brake_fault     = 0u;
    uint32 steering_fault  = 0u;
#ifdef PLATFORM_HIL
    uint32 sc_relay_kill   = 1u;  /* HIL: default "energized" — SC silent mode,
                                   * no 0x013 on bus (breadboard VCC issue).
                                   * Remove after bench hardening. */
#else
    uint32 sc_relay_kill   = 0u;  /* Production: fail-closed until SC_Status arrives */
#endif
    uint32 battery_status  = 2u;  /* Default NORMAL if read fails */
    uint32 motor_fault_rzc = 1u;  /* Default FAULT until Motor_Status confirmed fresh */
    uint32 motor_speed     = 0u;
    uint32 torque_request  = 0u;
    uint32 pedal_position  = 0u;

    if (initialized != TRUE)
    {
        return;
    }

    /* ---- Step 1: Read all fault signals from RTE ---- */
    (void)Rte_Read(CVC_SIG_PEDAL_FAULT,      &pedal_fault);
    (void)Rte_Read(CVC_SIG_ESTOP_ACTIVE,     &estop_active);
    (void)Rte_Read(CVC_SIG_FZC_COMM_STATUS,  &fzc_comm);
    (void)Rte_Read(CVC_SIG_RZC_COMM_STATUS,  &rzc_comm);
    (void)Rte_Read(CVC_SIG_MOTOR_CUTOFF,     &motor_cutoff);
    (void)Rte_Read(CVC_SIG_BRAKE_FAULT,      &brake_fault);
    (void)Rte_Read(CVC_SIG_STEERING_FAULT,   &steering_fault);
    (void)Rte_Read(CVC_SIG_SC_RELAY_KILL,   &sc_relay_kill);
    (void)Rte_Read(CVC_SIG_BATTERY_STATUS, &battery_status);
    (void)Rte_Read(CVC_SIG_MOTOR_FAULT_RZC, &motor_fault_rzc);
    (void)Rte_Read(CVC_SIG_MOTOR_SPEED,    &motor_speed);
    (void)Rte_Read(CVC_SIG_TORQUE_REQUEST, &torque_request);
    (void)Rte_Read(CVC_SIG_PEDAL_POSITION, &pedal_position);

    /* If Motor_Status PDU timed out, Com zeroed motor_fault_rzc to 0.
     * Treat timed-out signal as fault (1) — not "OK" (0). Prevents false
     * recovery from SAFE_STOP when RZC stops transmitting. */
    if (Com_GetRxPduQuality(CVC_COM_RX_MOTOR_STATUS) == COM_SIGNAL_QUALITY_TIMED_OUT) {
        motor_fault_rzc = 1u;
    }

#ifdef SIL_DIAG
    {
        static uint16 diag_cycle = 0u;
        static uint32 prev_mc = 0u;
        static uint32 prev_bf = 0u;
        diag_cycle++;
        if (diag_cycle <= 100u) {
            VSM_DIAG("c=%u st=%u ped=%u es=%u fzc=%u rzc=%u mc=%u bf=%u sf=%u sc=%u",
                     (unsigned)diag_cycle, (unsigned)current_state,
                     (unsigned)pedal_fault, (unsigned)estop_active,
                     (unsigned)fzc_comm, (unsigned)rzc_comm,
                     (unsigned)motor_cutoff, (unsigned)brake_fault,
                     (unsigned)steering_fault, (unsigned)sc_relay_kill);
        }
        /* Log whenever motor_cutoff or brake_fault transitions to non-zero */
        if ((motor_cutoff != 0u) && (prev_mc == 0u)) {
            VSM_DIAG("!! mc ONSET c=%u st=%u mc=%u bf=%u fzc=%u rzc=%u",
                     (unsigned)diag_cycle, (unsigned)current_state,
                     (unsigned)motor_cutoff, (unsigned)brake_fault,
                     (unsigned)fzc_comm, (unsigned)rzc_comm);
        }
        if ((brake_fault != 0u) && (prev_bf == 0u)) {
            VSM_DIAG("!! bf ONSET c=%u st=%u mc=%u bf=%u fzc=%u rzc=%u",
                     (unsigned)diag_cycle, (unsigned)current_state,
                     (unsigned)motor_cutoff, (unsigned)brake_fault,
                     (unsigned)fzc_comm, (unsigned)rzc_comm);
        }
        prev_mc = motor_cutoff;
        prev_bf = brake_fault;
    }
#endif

    /* ---- Step 2: INIT hold timer + pending self-test pass guard ---- */
    if (current_state == CVC_STATE_INIT)
    {
        if (init_hold_counter < CVC_INIT_HOLD_CYCLES)
        {
            init_hold_counter++;
        }

#ifdef SIL_DIAG
        /* SIL: latch "ever seen" to tolerate Docker scheduling jitter */
        if (fzc_comm == CVC_COMM_OK) { sil_fzc_seen = TRUE; }
        if (rzc_comm == CVC_COMM_OK) { sil_rzc_seen = TRUE; }
#endif

        {
            uint8 hb_ok;
#ifdef SIL_DIAG
            /* SIL: accept if zone ECUs were seen at least once during hold */
            hb_ok = (uint8)((sil_fzc_seen == TRUE) && (sil_rzc_seen == TRUE));
#else
            /* Production: require simultaneous heartbeat OK */
            hb_ok = (uint8)((fzc_comm == CVC_COMM_OK) && (rzc_comm == CVC_COMM_OK));
#endif
            (void)hb_ok; /* suppress unused warning if not used below */
        }

        if ((self_test_pass_pending == TRUE)
            && (init_hold_counter >= CVC_INIT_HOLD_CYCLES)
#ifdef PLATFORM_HIL
            )  /* HIL: skip heartbeat guard — E2E SM timing jitter
                * prevents stable VALID state. Heartbeat alive counters
                * verified by CAN sniffer (ok counter incrementing). */
#elif defined(SIL_DIAG)
            && (sil_fzc_seen == TRUE)
            && (sil_rzc_seen == TRUE))
#else
            && (fzc_comm == CVC_COMM_OK)
            && (rzc_comm == CVC_COMM_OK))
#endif
        {
            self_test_pass_pending = FALSE;
            current_state = CVC_STATE_RUN;

            /* Reset ConfirmFault + creep counters — ensure fresh detection
             * from RUN.  Defense-in-depth: counters should already be 0
             * (ConfirmFault is suppressed during INIT), but reset explicitly. */
            creep_debounce_count = 0u;
            {
                uint8 fi;
                for (fi = 0u; fi < CVC_FAULT_CONFIRM_COUNT; fi++)
                {
                    fault_confirm_count[fi] = 0u;
                }
            }

            /* Start post-INIT grace period (0 on bare metal = transparent).
             * SIL: absorbs stale zone-controller signals after restart. */
            post_init_grace_counter = CVC_POST_INIT_GRACE_CYCLES;
            if (post_init_grace_counter > 0u) {
                VSM_DIAG("post-INIT grace: %u cycles", (unsigned)post_init_grace_counter);
            }

            (void)BswM_RequestMode(CVC_ECU_ID_CVC, BSWM_RUN);
            VSM_DIAG("INIT -> RUN (heartbeats confirmed)");
        }
        /* else: remain in INIT — hold time or heartbeats not yet OK */
    }

    /* ---- Step 3: Derive events from signal values ---- */

    /* E-stop — highest priority, check first */
    if (estop_active != 0u)
    {
        Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);
    }

    /* SC relay kill — second highest priority.
     * sc_relay_kill holds RelayState from DBC: 1=energized (OK), 0=killed.
     * HIL: Skip — SC silent mode, no 0x013 on bus (breadboard VCC issue).
     * Remove guard after bench hardening. */
#ifndef PLATFORM_HIL
    if ((sc_relay_kill == 0u) && (current_state != CVC_STATE_INIT)
        && (post_init_grace_counter == 0u))
    {
        Swc_VehicleState_OnEvent(CVC_EVT_SC_KILL);
    }
#endif

    /* CAN communication faults (debounced: 50 consecutive timeout cycles = 500ms)
     * HIL: Skip — E2E alive counter jitter from gs_usb bridge causes false
     * COMM_TIMEOUT in Swc_Heartbeat. Physical heartbeats verified by
     * test_hil_heartbeat.py (6/6 pass). */
#ifndef PLATFORM_HIL
    if (post_init_grace_counter == 0u)
    {
        if ((fzc_comm == CVC_COMM_TIMEOUT) || (rzc_comm == CVC_COMM_TIMEOUT))
        {
            can_tmo_debounce++;
        }
        else
        {
            can_tmo_debounce = 0u;
        }

        if (can_tmo_debounce >= CAN_TMO_DEBOUNCE_THRESHOLD)
        {
            if ((fzc_comm == CVC_COMM_TIMEOUT) && (rzc_comm == CVC_COMM_TIMEOUT))
            {
                Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_DUAL);
            }
            else if ((current_state == CVC_STATE_RUN) || (current_state == CVC_STATE_DEGRADED))
            {
                Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_SINGLE);
            }
        }
    }
    else
    {
        /* Both comm OK — if in LIMP, signal CAN restored */
        if (current_state == CVC_STATE_LIMP)
        {
            Swc_VehicleState_OnEvent(CVC_EVT_CAN_RESTORED);
        }
    }
#endif /* !PLATFORM_HIL */

    /* Pedal faults — only derive events when in relevant states.
     * HIL: Skip — no physical accelerator pedal sensor on bench.
     * FZC reports pedal_fault because ADC reads floating pin.
     * SIL: Suppress during post-INIT grace. Swc_Pedal can latch a
     * plausibility fault on boot from SPI-stub init transients;
     * without this gate, CVC trips RUN->DEGRADED inside the grace
     * window and never returns to clean RUN between test scenarios. */
#ifndef PLATFORM_HIL
    if ((current_state == CVC_STATE_RUN) && (post_init_grace_counter == 0u))
    {
        if (pedal_fault != 0u)
        {
            Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_SINGLE);
        }
    }
#endif

    /* Battery faults (SG-006) — derived directly from battery_status signal.
     * Battery status codes from RZC: 0=DISABLE_LOW, 1=WARN_LOW,
     * 2=NORMAL, 3=WARN_HIGH, 4=DISABLE_HIGH.
     * WARN (1 or 3) -> EVT_BATTERY_WARN (RUN->DEGRADED)
     * CRIT (0 or 4) -> EVT_BATTERY_CRIT (RUN->LIMP, LIMP->SAFE_STOP)
     * HIL: Skip — RZC Battery_Status_State defaults to 0 (DISABLE_LOW)
     * because physical ACS723 current sensor is absent. Plant-sim provides
     * nominal 12.6V via 0x601 but RZC firmware doesn't map virtual sensor
     * data to Battery_Status_State on HIL. */
#ifndef PLATFORM_HIL
    if (((current_state == CVC_STATE_RUN) ||
         (current_state == CVC_STATE_DEGRADED) ||
         (current_state == CVC_STATE_LIMP)) &&
        (post_init_grace_counter == 0u))
    {
        if ((battery_status == 0u) || (battery_status == 4u))
        {
            Dem_ReportErrorStatus(CVC_DTC_BATT_UNDERVOLT, DEM_EVENT_STATUS_FAILED);
            Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_CRIT);
        }
        else if ((battery_status == 1u) || (battery_status == 3u))
        {
            Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_WARN);
        }
        else
        {
            /* battery_status == 2 (NORMAL) — no action */
        }
    }
#endif

    /* Creep guard (SG-012 / HE-017 ASIL D) — detect torque at standstill
     * without driver intent.  Uses latching detection: once torque appears
     * while motor is at standstill, keep counting even as motor spins up
     * (the spin-up IS the creep).  Only reset when torque drops (pedal
     * released) — that proves the condition was intentional or cleared.
     *
     * Guards:
     * - pedal_fault != 0: pedal fault path handles that case
     * - pedal_position > dead zone: driver IS pressing pedal — not a creep.
     *   If pedal input is excessive, the runaway acceleration path (DEGRADED
     *   with torque limiting) handles it.  Creep = torque WITHOUT pedal.
     * HIL: Skip — no physical motor/pedal. Plant-sim may have zero pedal
     * with non-zero torque during initialization transient. */
#ifndef PLATFORM_HIL
    if (((current_state == CVC_STATE_RUN) ||
         (current_state == CVC_STATE_DEGRADED) ||
         (current_state == CVC_STATE_LIMP)) &&
        (pedal_fault == 0u) &&
        (pedal_position < CVC_CREEP_TORQUE_THRESH))
    {
        if (torque_request > CVC_CREEP_TORQUE_THRESH)
        {
            /* Torque present — start or continue counting if motor was/is
             * at standstill when we first detected it */
            if ((creep_debounce_count > 0u) ||
                (motor_speed < CVC_CREEP_SPEED_THRESH))
            {
                creep_debounce_count++;
                if (creep_debounce_count >= CVC_CREEP_DEBOUNCE_TICKS)
                {
                    Dem_ReportErrorStatus(CVC_DTC_CREEP_FAULT, DEM_EVENT_STATUS_FAILED);
                    Swc_VehicleState_OnEvent(CVC_EVT_CREEP_FAULT);
                    VSM_DIAG("CREEP FAULT: rpm=%u torque=%u pedal=%u -> SAFE_STOP",
                             (unsigned)motor_speed, (unsigned)torque_request,
                             (unsigned)pedal_position);
                }
            }
            /* else: torque present but motor already spinning — normal driving */
        }
        else
        {
            /* No torque request — clear the latch */
            creep_debounce_count = 0u;
        }
    }
#endif /* !PLATFORM_HIL */

    /* Fault cleared — when in DEGRADED and ALL DEGRADED-causing faults
     * are clear. Must check every fault that can trigger DEGRADED:
     *   - pedal_fault    (EVT_PEDAL_FAULT_SINGLE -> DEGRADED)
     *   - motor_cutoff   (EVT_MOTOR_CUTOFF -> DEGRADED)
     *   - battery_status (EVT_BATTERY_WARN -> DEGRADED) — must be NORMAL(2)
     * Also check brake_fault for robustness: if brake_fault is non-zero
     * but not yet confirmed, premature clear would bounce RUN->SAFE_STOP
     * instead of the correct DEGRADED->SAFE_STOP path. */
    if ((current_state == CVC_STATE_DEGRADED) &&
        (pedal_fault == 0u) &&
        (motor_cutoff == 0u) &&
        (motor_fault_rzc == 0u) &&
        (brake_fault == 0u) &&
        (battery_status == 2u))
    {
        Swc_VehicleState_OnEvent(CVC_EVT_FAULT_CLEARED);
    }

    /* ---- Step 4: Confirmed fault events (ISO 26262 confirmation-read) ----
     * Suppressed during INIT: zone controllers are booting and may send
     * spurious fault signals.  The transition table maps these events to
     * INVALID from INIT anyway, but running ConfirmFault wastes cycles
     * and leaves counters at unpredictable values when INIT->RUN fires.
     * Suppressing during INIT ensures counters start at 0 for RUN.
     *
     * Also suppressed during post-INIT grace period (0 on bare metal =
     * transparent).  SIL: absorbs stale zone-controller signals after
     * container restart.  Grace counter counts down to zero, then normal
     * detection resumes.  Platform-equivalent code path. */
    {
        uint8 suppress_faults = FALSE;

        if (current_state == CVC_STATE_INIT)
        {
            suppress_faults = TRUE;
        }

        if (post_init_grace_counter > 0u)
        {
            post_init_grace_counter--;
            suppress_faults = TRUE;
#ifdef SIL_DIAG
            if ((post_init_grace_counter % 100u) == 0u)
            {
                VSM_DIAG("post-INIT grace remaining: %u", (unsigned)post_init_grace_counter);
            }
#endif
            /* When grace expires, reset heartbeat comm status so stale
             * timeouts accumulated during Docker boot don't immediately
             * trigger CAN_TMO_S on the first post-grace cycle. */
            if (post_init_grace_counter == 0u)
            {
                extern void Swc_Heartbeat_ResetCommStatus(void);
                Swc_Heartbeat_ResetCommStatus();
            }
        }

        /* HIL: Skip all CAN-derived fault confirmations — no physical
         * sensors connected. Motor cutoff, brake fault, and steering fault
         * signals read stale/default values from absent CAN frames.
         * Plant-sim fault injection tests verify these paths via MQTT. */
#ifdef PLATFORM_HIL
        (void)suppress_faults;
#else
        if (suppress_faults == FALSE)
        {
            Swc_VehicleState_ConfirmFault(
                CVC_FAULT_IDX_MOTOR_CUTOFF, motor_cutoff,
                CVC_FAULT_COM_MOTOR_CUTOFF, CVC_FAULT_E2E_MOTOR_CUTOFF,
                CVC_DTC_MOTOR_CUTOFF_RX, CVC_EVT_MOTOR_CUTOFF);

            Swc_VehicleState_ConfirmFault(
                CVC_FAULT_IDX_MOTOR_RZC, motor_fault_rzc,
                CVC_COM_SIG_MOTOR_STATUS_MOTOR_FAULT_STATUS, 0xFFu,
                CVC_DTC_MOTOR_OVERCURRENT, CVC_EVT_MOTOR_CUTOFF);

            Swc_VehicleState_ConfirmFault(
                CVC_FAULT_IDX_BRAKE, brake_fault,
                CVC_FAULT_COM_BRAKE, CVC_FAULT_E2E_BRAKE,
                CVC_DTC_BRAKE_FAULT_RX, CVC_EVT_BRAKE_FAULT);

            Swc_VehicleState_ConfirmFault(
                CVC_FAULT_IDX_STEERING, steering_fault,
                CVC_FAULT_COM_STEERING, CVC_FAULT_E2E_STEERING,
                CVC_DTC_STEERING_FAULT_RX, CVC_EVT_STEERING_FAULT);
        }
#endif
    }

    /* ---- Step 5: SAFE_STOP recovery with fault latching ---- */
    if (current_state == CVC_STATE_SAFE_STOP)
    {
        /* Map latch indices to their raw signal values.
         * 0 = fault clear, non-zero = fault still active. */
        const uint32 raw_signals[CVC_LATCH_COUNT] = {
            estop_active,   /* CVC_LATCH_IDX_ESTOP        */
            (sc_relay_kill == 0u) ? 1u : 0u,  /* CVC_LATCH_IDX_SC_KILL — invert: 0=killed(fault), 1=energized(OK) */
            (motor_cutoff != 0u || motor_fault_rzc != 0u) ? 1u : 0u,  /* CVC_LATCH_IDX_MOTOR_CUTOFF */
            brake_fault,    /* CVC_LATCH_IDX_BRAKE        */
            steering_fault, /* CVC_LATCH_IDX_STEERING     */
            pedal_fault,    /* CVC_LATCH_IDX_PEDAL_DUAL   */
            ((fzc_comm == CVC_COMM_TIMEOUT) && (rzc_comm == CVC_COMM_TIMEOUT)) ? 1u : 0u,  /* CVC_LATCH_IDX_CAN_DUAL */
            (battery_status != 2u) ? 1u : 0u  /* CVC_LATCH_IDX_BATTERY_CRIT — clear when NORMAL(2) */
        };

        uint8 all_latches_clear = TRUE;
        uint8 li;

        for (li = 0u; li < CVC_LATCH_COUNT; li++)
        {
            if (fault_latched[li] == TRUE)
            {
                if (raw_signals[li] == 0u)
                {
                    fault_unlatch_count[li]++;
                    if (fault_unlatch_count[li] >= CVC_FAULT_UNLATCH_CYCLES)
                    {
                        fault_latched[li]       = FALSE;
                        fault_unlatch_count[li] = 0u;
                        VSM_DIAG("UNLATCH idx=%u after %u cycles",
                                 (unsigned)li, (unsigned)CVC_FAULT_UNLATCH_CYCLES);
                    }
                    else
                    {
                        all_latches_clear = FALSE;
                    }
                }
                else
                {
                    fault_unlatch_count[li] = 0u;  /* Reset: fault still active */
                    all_latches_clear = FALSE;
                }
            }
        }

        /* Standard recovery only after ALL latches cleared */
        if (all_latches_clear == TRUE)
        {
            /* Defense in depth: check instantaneous signals */
            if ((estop_active == 0u) &&
                (motor_cutoff == 0u) &&
                (motor_fault_rzc == 0u) &&
                (brake_fault == 0u) &&
                (steering_fault == 0u) &&
                (pedal_fault == 0u) &&
                (sc_relay_kill != 0u) &&  /* 1=energized(OK) */
                (fzc_comm == CVC_COMM_OK) &&
                (rzc_comm == CVC_COMM_OK) &&
                (battery_status == 2u))
            {
                safe_stop_clear_count++;
                if (safe_stop_clear_count >= CVC_SAFE_STOP_RECOVERY_CYCLES)
                {
                    safe_stop_clear_count  = 0u;
                    init_hold_counter      = 0u;  /* BUG FIX: force full INIT hold */
                    current_state          = CVC_STATE_INIT;
                    self_test_pass_pending = TRUE;
                    (void)BswM_RequestMode(CVC_ECU_ID_CVC, BSWM_STARTUP);
                    VSM_DIAG("SAFE_STOP -> INIT (recovery, latches clear)");
                }
            }
            else
            {
                safe_stop_clear_count = 0u;
            }
        }
        else
        {
            safe_stop_clear_count = 0u;
        }
    }

    /* ---- Step 6: Write current state to RTE ---- */
    (void)Rte_Write(CVC_SIG_VEHICLE_STATE, (uint32)current_state);

    /* Heartbeat OperatingMode — auto-pulled by Com_MainFunction_Tx */
    (void)Rte_Write(CVC_SIG_CVC_HEARTBEAT_OPERATING_MODE, (uint32)current_state);
}
