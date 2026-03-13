/**
 * @file    sc_relay.h
 * @brief   Kill relay GPIO control for Safety Controller
 * @date    2026-02-23
 *
 * @details Controls GIO_A0 for the kill relay (energize-to-run pattern).
 *          De-energize latch: once killed, only power cycle restores.
 *          Readback verification with 2-consecutive-mismatch threshold.
 *
 * @safety_req SWR-SC-010, SWR-SC-011, SWR-SC-012
 * @traces_to  SSR-SC-010, SSR-SC-011, SSR-SC-012
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_RELAY_H
#define SC_RELAY_H

#include "sc_types.h"

/**
 * @brief  Initialize relay — GIO_A0 LOW (safe state)
 */
void SC_Relay_Init(void);

/**
 * @brief  Energize relay — set GIO_A0 HIGH (only if not killed)
 */
void SC_Relay_Energize(void);

/**
 * @brief  De-energize relay — set GIO_A0 LOW and latch (permanent)
 */
void SC_Relay_DeEnergize(void);

/**
 * @brief  10ms cyclic — evaluate all de-energize trigger conditions
 *
 * Checks (in priority order): E-Stop command, heartbeat confirmed timeout,
 * plausibility fault, E2E persistent failure, self-test failure,
 * ESM lockstep error, CAN bus-off, bus silence, and GPIO readback mismatch.
 */
void SC_Relay_CheckTriggers(void);

/**
 * @brief  Check if relay has been permanently killed
 * @return TRUE if relay is latched in de-energized state
 */
boolean SC_Relay_IsKilled(void);

/**
 * @brief  Get the reason code for the most recent relay kill
 * @return Kill reason (SC_KILL_REASON_* from sc_cfg.h), 0 if not killed
 */
uint8 SC_Relay_GetKillReason(void);

#endif /* SC_RELAY_H */
