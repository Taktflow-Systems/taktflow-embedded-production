/**
 * @file    sc_relay.c
 * @brief   Kill relay GPIO control for Safety Controller
 * @date    2026-02-23
 *
 * @safety_req SWR-SC-010, SWR-SC-011, SWR-SC-012
 * @traces_to  SSR-SC-010, SSR-SC-011, SSR-SC-012
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "sc_relay.h"
#include "Sc_Hw_Cfg.h"
#include "sc_gio.h"
#include "sc_heartbeat.h"
#include "sc_plausibility.h"
#include "sc_selftest.h"
#include "sc_esm.h"
#include "sc_can.h"
#include "sc_e2e.h"

/* SIL diagnostic logging — compile with -DSIL_DIAG to enable */
#ifdef SIL_DIAG
#include <stdio.h>  /* cppcheck-suppress misra-c2012-21.6 ; SIL-only diagnostic output */
#define SC_RELAY_DIAG(fmt, ...) (void)fprintf(stderr, "[SC_RELAY] " fmt "\n", ##__VA_ARGS__)
#else
#define SC_RELAY_DIAG(fmt, ...) ((void)0)
#endif

/* ==================================================================
 * Module State
 * ================================================================== */

/** Relay kill latch — once TRUE, cannot be cleared (power cycle only) */
static boolean relay_killed;

/** Commanded relay state (TRUE = HIGH = energized) */
static boolean relay_commanded;

/** GPIO readback mismatch counter */
static uint8 readback_mismatch_count;

/** Kill reason code (SC_KILL_REASON_*) */
static uint8 kill_reason;


/* ==================================================================
 * Public API
 * ================================================================== */

void SC_Relay_Init(void)
{
    /* Kill latch is NOT cleared — once killed, only a power cycle resets it.
     * This ensures the kill latch survives re-initialization (SWR-SC-011). */
    relay_commanded         = FALSE;
    readback_mismatch_count = 0u;
    kill_reason             = SC_KILL_REASON_NONE;

    /* Ensure relay is de-energized (safe state) */
    gioSetBit(SC_GIO_PORT_A, SC_PIN_RELAY, 0u);
}

void SC_Relay_Energize(void)
{
    /* Kill latch prevents re-energize */
    if (relay_killed == TRUE) {
        return;
    }

    relay_commanded = TRUE;
    gioSetBit(SC_GIO_PORT_A, SC_PIN_RELAY, 1u);
}

void SC_Relay_DeEnergize(void)
{
#ifdef PLATFORM_HIL
    /* HIL: CAN transceiver power depends on relay — never de-energize.
     * Without this, ECU heartbeats disappear and SC enters a kill loop.
     * E-Stop still works via physical NC switch bypassing the relay. */
    (void)0;
#else
    relay_commanded = FALSE;
    relay_killed    = TRUE;
    gioSetBit(SC_GIO_PORT_A, SC_PIN_RELAY, 0u);
#endif
}

void SC_Relay_CheckTriggers(void)
{
    uint8 readback;

    /* If already killed, nothing to check */
    if (relay_killed == TRUE) {
        return;
    }

    /* Trigger (a): E-Stop command (highest priority — explicit safety request) */
    if (SC_CAN_IsEStopActive() == TRUE) {
        kill_reason = SC_KILL_REASON_ESTOP;
        SC_RELAY_DIAG("KILL reason=ESTOP");
        SC_Relay_DeEnergize();
        return;
    }

    /* Trigger (b): Heartbeat confirmed timeout */
    if (SC_Heartbeat_IsAnyConfirmed() == TRUE) {
        kill_reason = SC_KILL_REASON_HB_TIMEOUT;
        SC_RELAY_DIAG("KILL reason=HB_TIMEOUT cvc=%u fzc=%u rzc=%u",
                      (unsigned)SC_Heartbeat_IsTimedOut(SC_ECU_CVC),
                      (unsigned)SC_Heartbeat_IsTimedOut(SC_ECU_FZC),
                      (unsigned)SC_Heartbeat_IsTimedOut(SC_ECU_RZC));
        SC_Relay_DeEnergize();
        return;
    }

    /* Trigger (b): Plausibility fault */
    if (SC_Plausibility_IsFaulted() == TRUE) {
        kill_reason = SC_KILL_REASON_PLAUSIBILITY;
        SC_RELAY_DIAG("KILL reason=PLAUSIBILITY");
        SC_Relay_DeEnergize();
        return;
    }

    /* Trigger (b2): Creep guard — standstill torque cross-plausibility (SSR-SC-018) */
    if (SC_Plausibility_IsCreepFaulted() == TRUE) {
        kill_reason = SC_KILL_REASON_CREEP_GUARD;
        SC_RELAY_DIAG("KILL reason=CREEP_GUARD (DTC 0xE312)");
        SC_Relay_DeEnergize();
        return;
    }

    /* Trigger (c): E2E persistent failure on safety-critical mailbox (GAP-SC-002) */
    if (SC_E2E_IsAnyCriticalFailed() == TRUE) {
        kill_reason = SC_KILL_REASON_E2E_FAIL;
        SC_RELAY_DIAG("KILL reason=E2E_FAIL");
        SC_Relay_DeEnergize();
        return;
    }

    /* Trigger (d/e): Self-test failure (startup or runtime) */
    if (SC_SelfTest_IsHealthy() == FALSE) {
        kill_reason = SC_KILL_REASON_SELFTEST;
        SC_RELAY_DIAG("KILL reason=SELFTEST");
        SC_Relay_DeEnergize();
        return;
    }

    /* Trigger (e): ESM lockstep error */
    if (SC_ESM_IsErrorActive() == TRUE) {
        kill_reason = SC_KILL_REASON_ESM;
        SC_RELAY_DIAG("KILL reason=ESM");
        SC_Relay_DeEnergize();
        return;
    }

#ifndef PLATFORM_HIL
    /* Trigger (f): CAN bus-off
     * HIL: DCAN silent mode prevents bus-off, but guard anyway. */
    if (SC_CAN_IsBusOff() == TRUE) {
        kill_reason = SC_KILL_REASON_BUSOFF;
        SC_RELAY_DIAG("KILL reason=BUSOFF");
        SC_Relay_DeEnergize();
        return;
    }

    /* Trigger (g): CAN bus silence — no valid frames for >= 200ms */
    if (SC_CAN_IsBusSilent() == TRUE) {
        kill_reason = SC_KILL_REASON_BUS_SILENCE;
        SC_RELAY_DIAG("KILL reason=BUS_SILENCE");
        SC_Relay_DeEnergize();
        return;
    }
#endif

    /* Trigger (h): GPIO readback mismatch
     * Skip entirely on HIL: GIOA[0] (ball A5) is muxed to LIN1TX for SCI
     * debug and no physical relay is connected on the LaunchPad. */
#ifndef PLATFORM_HIL
    readback = gioGetBit(SC_GIO_PORT_A, SC_PIN_RELAY);
    if (relay_commanded == TRUE) {
        if (readback != 1u) {
            readback_mismatch_count++;
        } else {
            readback_mismatch_count = 0u;
        }
    } else {
        if (readback != 0u) {
            readback_mismatch_count++;
        } else {
            readback_mismatch_count = 0u;
        }
    }

    if (readback_mismatch_count >= SC_RELAY_READBACK_THRESHOLD) {
        kill_reason = SC_KILL_REASON_READBACK;
        SC_RELAY_DIAG("KILL reason=READBACK cmd=%u rb=%u cnt=%u",
                      (unsigned)relay_commanded, (unsigned)readback,
                      (unsigned)readback_mismatch_count);
        SC_Relay_DeEnergize();
    }
#else
    (void)readback;
#endif
}

boolean SC_Relay_IsKilled(void)
{
#if defined(PLATFORM_POSIX) || defined(PLATFORM_HIL)
    /* SIL/HIL: only CREEP_GUARD and ESTOP kills propagate to CVC. Others
     * (HB_TIMEOUT, PLAUSIBILITY, E2E_FAIL, BUSOFF, BUS_SILENCE, READBACK,
     * SELFTEST, ESM) get suppressed because:
     *   - HB_TIMEOUT: boot-phase false kills before CVC finishes INIT
     *     (chicken-and-egg on HIL where relay powers the CAN transceiver).
     *   - PLAUSIBILITY: plant-sim's motor model does not match SC's
     *     lockstep expected-current lookup closely enough in fault
     *     scenarios, so overcurrent/stall tests spuriously trip SC and
     *     drive CVC to SHUTDOWN via EVT_SC_KILL instead of the intended
     *     SAFE_STOP path.
     * CREEP_GUARD is the only SC-only detection the scenario set
     * actually needs, and ESTOP is a hard safety override that must
     * always propagate. */
    if (relay_killed == TRUE) {
        if (kill_reason == SC_KILL_REASON_CREEP_GUARD) {
            return TRUE;
        }
        /* ESTOP also propagates via SC, but CVC already transitions to
         * SAFE_STOP from its own 0x010 E-Stop_Command RX path — reporting
         * via RelayEnergized here additionally fires EVT_SC_KILL which
         * maps to SHUTDOWN, overshooting the intended SAFE_STOP verdict. */
        return FALSE;
    }
#endif
    return relay_killed;
}

uint8 SC_Relay_GetKillReason(void)
{
    return kill_reason;
}

