/**
 * @file    Swc_Heartbeat.c
 * @brief   RZC heartbeat -- 50ms CAN TX, alive counter, ECU ID, fault bitmask
 * @date    2026-02-23
 *
 * @safety_req SWR-RZC-021, SWR-RZC-022
 * @traces_to  SSR-RZC-021, SSR-RZC-022, TSR-022
 *
 * @details  Implements the RZC heartbeat SWC:
 *           1. Transmits heartbeat on CAN 0x012 every 50ms
 *           2. Alive counter 0-15, wraps around
 *           3. Includes ECU ID (0x03), vehicle state, fault bitmask
 *           4. Suppresses TX during CAN bus-off condition
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Heartbeat.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Rte.h"
#include "PduR.h"
#include "Dem.h"
#include "E2E.h"

/* ==================================================================
 * Constants (derived from config — compile-time safe)
 * ================================================================== */

/** @brief Heartbeat period in RTE cycles: derived from RZC_HB_TX_PERIOD_MS / RZC_RTE_PERIOD_MS
 *  @safety_req SWR-RZC-021 */
#define HB_PERIOD_CYCLES  (RZC_HB_TX_PERIOD_MS / RZC_RTE_PERIOD_MS)

_Static_assert(RZC_HB_TX_PERIOD_MS % RZC_RTE_PERIOD_MS == 0u,
               "HB TX period must be exact multiple of RTE period");
_Static_assert(HB_PERIOD_CYCLES > 0u,
               "HB period cycles must be positive");

/** Heartbeat TX data layout (8 bytes) — E2E-protected PDU
 *  Bytes 0-1: E2E overhead (counter+dataid, CRC) — written by E2E_Protect
 *  Byte 2:    ECU_ID
 *  Byte 3:    [FaultStatus:4 | OperatingMode:4]
 */
#define HB_BYTE_ECU_ID       2u
#define HB_BYTE_STATE_FAULT  3u

/* ==================================================================
 * Module State
 * ================================================================== */

static uint8   Hb_Initialized;
static uint8   Hb_CycleCounter;
static uint8   Hb_AliveCounter;

/* E2E protection moved to Com_MainFunction_Tx (Phase 2) */

/* ==================================================================
 * API: Swc_Heartbeat_Init
 * ================================================================== */

void Swc_Heartbeat_Init(void)
{
    Hb_CycleCounter = 0u;
    Hb_AliveCounter = 0u;
    Hb_Initialized  = TRUE;
}

/* ==================================================================
 * API: Swc_Heartbeat_MainFunction (50ms cyclic)
 * ================================================================== */

void Swc_Heartbeat_MainFunction(void)
{
    uint32 vehicle_state;
    uint32 fault_mask;

    if (Hb_Initialized != TRUE) {
        return;
    }

    /* Increment cycle counter */
    Hb_CycleCounter++;

    if (Hb_CycleCounter < HB_PERIOD_CYCLES) {
        return;
    }

    /* Reset cycle counter */
    Hb_CycleCounter = 0u;

    /* Read current state from RTE */
    vehicle_state = RZC_STATE_INIT;
    (void)Rte_Read(RZC_SIG_VEHICLE_STATE, &vehicle_state);

    fault_mask = 0u;
    (void)Rte_Read(RZC_SIG_FAULT_MASK, &fault_mask);

    /* Suppress TX during bus-off (CAN fault set AND vehicle in SAFE_STOP) */
    if (((fault_mask & (uint32)RZC_FAULT_CAN) != 0u) &&
        (vehicle_state == (uint32)RZC_STATE_SAFE_STOP)) {
        return;
    }

    /* Heartbeat TX handled by Swc_RzcCom bridge via Com_SendSignal */

    /* Write alive counter to RTE for diagnostics */
    (void)Rte_Write(RZC_SIG_HEARTBEAT_ALIVE, (uint32)Hb_AliveCounter);

    /* Increment alive counter with wrap */
    Hb_AliveCounter++;
    if (Hb_AliveCounter > RZC_HB_ALIVE_MAX) {
        Hb_AliveCounter = 0u;
    }
}
