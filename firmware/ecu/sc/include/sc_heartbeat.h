/**
 * @file    sc_heartbeat.h
 * @brief   Per-ECU heartbeat monitoring for Safety Controller
 * @date    2026-02-23
 *
 * @details Monitors 3 independent heartbeat counters (CVC, FZC, RZC).
 *          Timeout at 150ms (15 ticks), confirmed at 200ms (20 ticks).
 *          Drives per-ECU fault LEDs on GIO_A1/A2/A3.
 *
 * @safety_req SWR-SC-004, SWR-SC-005, SWR-SC-006
 * @traces_to  SSR-SC-004, SSR-SC-005, SSR-SC-006
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_HEARTBEAT_H
#define SC_HEARTBEAT_H

#include "sc_types.h"

/**
 * @brief  Initialize heartbeat monitoring — reset all counters
 */
void SC_Heartbeat_Init(void);

/**
 * @brief  Notify that a valid heartbeat was received for an ECU
 *
 * Called by sc_can on valid heartbeat E2E reception.
 *
 * @param  ecuIndex  SC_ECU_CVC, SC_ECU_FZC, or SC_ECU_RZC
 */
void SC_Heartbeat_NotifyRx(uint8 ecuIndex);

/**
 * @brief  10ms cyclic heartbeat monitor — update counters and LEDs
 */
void SC_Heartbeat_Monitor(void);

/**
 * @brief  Check if a specific ECU has timed out
 *
 * @param  ecuIndex  SC_ECU_CVC, SC_ECU_FZC, or SC_ECU_RZC
 * @return TRUE if timeout detected (>= 150ms without heartbeat)
 */
boolean SC_Heartbeat_IsTimedOut(uint8 ecuIndex);

/**
 * @brief  Check if any ECU has a confirmed timeout (200ms total)
 * @return TRUE if at least one ECU timeout is confirmed
 */
boolean SC_Heartbeat_IsAnyConfirmed(void);

/**
 * @brief  Check if FZC brake fault flag is set
 * @return TRUE if FZC has reported a brake-related fault
 */
boolean SC_Heartbeat_IsFzcBrakeFault(void);

/**
 * @brief  Validate heartbeat payload content (SWR-SC-027, SWR-SC-028)
 *
 * Extracts OperatingMode and FaultStatus from heartbeat byte 3.
 * Increments stuck_degraded_cnt when mode is DEGRADED/LIMP (SWR-SC-027).
 * Increments fault_escalate_cnt when >=2 FaultStatus bits are set (SWR-SC-028).
 * Sets content_fault flag if either counter exceeds its threshold.
 * Call AFTER SC_Heartbeat_NotifyRx for the same ECU, with same validated payload.
 *
 * @param  ecuIndex  SC_ECU_CVC, SC_ECU_FZC, or SC_ECU_RZC
 * @param  payload   Validated 4-byte heartbeat payload (bytes 0-3)
 * @note   ASIL C per SWR-SC-027/028.
 */
void SC_Heartbeat_ValidateContent(uint8 ecuIndex, const uint8* payload);

/**
 * @brief  Check if content validation fault is active for an ECU
 *
 * @param  ecuIndex  SC_ECU_CVC, SC_ECU_FZC, or SC_ECU_RZC
 * @return TRUE if stuck_degraded or fault_escalate threshold exceeded
 */
boolean SC_Heartbeat_IsContentFault(uint8 ecuIndex);

#endif /* SC_HEARTBEAT_H */
