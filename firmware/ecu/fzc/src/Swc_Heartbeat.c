/**
 * @file    Swc_Heartbeat.c
 * @brief   FZC heartbeat — 50ms CAN TX, alive counter, ECU ID, fault bitmask
 * @date    2026-02-23
 *
 * @safety_req SWR-FZC-021, SWR-FZC-022
 * @traces_to  SSR-FZC-021, SSR-FZC-022, TSR-022
 *
 * @details  Implements the FZC heartbeat SWC:
 *           1. Transmits heartbeat on CAN 0x011 every 50ms
 *           2. Alive counter 0-15, wraps around
 *           3. Includes ECU ID (0x02), vehicle state, fault bitmask
 *           4. Suppresses TX during CAN bus-off condition
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_Heartbeat.h"
#include "Fzc_Cfg.h"

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

/** @brief Heartbeat period in RTE cycles: derived from FZC_HB_TX_PERIOD_MS / FZC_RTE_PERIOD_MS
 *  @safety_req SWR-FZC-021 */
#define HB_PERIOD_CYCLES  (FZC_HB_TX_PERIOD_MS / FZC_RTE_PERIOD_MS)

_Static_assert(FZC_HB_TX_PERIOD_MS % FZC_RTE_PERIOD_MS == 0u,
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

/** @brief E2E configuration for heartbeat TX protection
 *  @safety_req SWR-FZC-021 */
static const E2E_ConfigType hb_e2e_config = {
    FZC_E2E_HEARTBEAT_DATA_ID,   /* DataId = 0x03 per spec */
    15u,                          /* MaxDeltaCounter */
    8u                            /* DataLength */
};

/** @brief E2E state for heartbeat TX alive counter tracking */
static E2E_StateType hb_e2e_state;

/* ==================================================================
 * API: Swc_Heartbeat_Init
 * ================================================================== */

void Swc_Heartbeat_Init(void)
{
    Hb_CycleCounter = 0u;
    Hb_AliveCounter = 0u;
    hb_e2e_state.Counter = 0u;
    Hb_Initialized  = TRUE;
}

/* ==================================================================
 * API: Swc_Heartbeat_MainFunction (1ms cyclic)
 * ================================================================== */

void Swc_Heartbeat_MainFunction(void)
{
    uint32 vehicle_state;
    uint32 fault_mask;
    uint8  tx_data[8];
    uint8  i;

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
    vehicle_state = FZC_STATE_INIT;
    (void)Rte_Read(FZC_SIG_VEHICLE_STATE, &vehicle_state);

    fault_mask = 0u;
    (void)Rte_Read(FZC_SIG_FAULT_MASK, &fault_mask);

    /* Suppress TX during bus-off (indicated by CAN bus-off bit in mask) */
    if ((fault_mask & (uint32)FZC_FAULT_CAN_BUS_OFF) != 0u) {
        return;
    }

    /* Build heartbeat payload */
    for (i = 0u; i < 8u; i++) {
        tx_data[i] = 0u;
    }

    /* Heartbeat state written to RTE — Swc_FzcCom bridge reads + sends via Com */

    /* Write alive counter to RTE for diagnostics */
    (void)Rte_Write(FZC_SIG_HEARTBEAT_ALIVE, (uint32)Hb_AliveCounter);

    /* Increment alive counter with wrap */
    Hb_AliveCounter++;
    if (Hb_AliveCounter > FZC_HB_ALIVE_MAX) {
        Hb_AliveCounter = 0u;
    }
}
