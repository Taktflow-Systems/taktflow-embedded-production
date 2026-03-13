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

/** @brief E2E configuration for heartbeat TX protection
 *  @safety_req SWR-RZC-021 */
static const E2E_ConfigType hb_e2e_config = {
    RZC_E2E_HEARTBEAT_DATA_ID,   /* DataId = 0x04 per spec */
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
 * API: Swc_Heartbeat_MainFunction (50ms cyclic)
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
    vehicle_state = RZC_STATE_INIT;
    (void)Rte_Read(RZC_SIG_VEHICLE_STATE, &vehicle_state);

    fault_mask = 0u;
    (void)Rte_Read(RZC_SIG_FAULT_MASK, &fault_mask);

    /* Suppress TX during bus-off (CAN fault set AND vehicle in SAFE_STOP) */
    if (((fault_mask & (uint32)RZC_FAULT_CAN) != 0u) &&
        (vehicle_state == (uint32)RZC_STATE_SAFE_STOP)) {
        return;
    }

    /* Build heartbeat payload */
    for (i = 0u; i < 8u; i++) {
        tx_data[i] = 0u;
    }

    /* Bytes 0-1 reserved for E2E_Protect (counter+dataid, CRC) */
    tx_data[HB_BYTE_ECU_ID]      = RZC_ECU_ID;
    tx_data[HB_BYTE_STATE_FAULT] = (uint8)(((fault_mask & 0x0Fu) << 4u)
                                         | (vehicle_state & 0x0Fu));

    /* E2E protect then send via PduR → CanIf → CAN 0x012 */
    (void)E2E_Protect(&hb_e2e_config, &hb_e2e_state, tx_data, 8u);
    {
        PduInfoType pdu_info;
        pdu_info.SduDataPtr = tx_data;
        pdu_info.SduLength  = 8u;
        (void)PduR_Transmit(RZC_COM_TX_HEARTBEAT, &pdu_info);
    }

    /* Write alive counter to RTE for diagnostics */
    (void)Rte_Write(RZC_SIG_HEARTBEAT_ALIVE, (uint32)Hb_AliveCounter);

    /* Increment alive counter with wrap */
    Hb_AliveCounter++;
    if (Hb_AliveCounter > RZC_HB_ALIVE_MAX) {
        Hb_AliveCounter = 0u;
    }
}
