/**
 * @file    Swc_Heartbeat.h
 * @brief   Heartbeat TX/RX monitoring SWC
 * @date    2026-02-21
 *
 * @safety_req SWR-CVC-021, SWR-CVC-022
 * @traces_to  SSR-CVC-021, SSR-CVC-022, TSR-022, TSR-046
 *
 * @details  Transmits CVC heartbeat on CAN every 50ms with E2E protection
 *           and alive counter. Monitors FZC and RZC heartbeat reception
 *           with timeout detection (3 missed periods) and recovery.
 *           Reports comm status to RTE and DTCs to Dem.
 *           10ms cyclic execution.
 *
 * @standard AUTOSAR, ISO 26262 Part 6 (ASIL D)
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_HEARTBEAT_H
#define SWC_HEARTBEAT_H

#include "Std_Types.h"

/**
 * @brief  Initialise heartbeat SWC — reset timers, counters, comm status
 * @note   Must be called once before Swc_Heartbeat_MainFunction
 */
void Swc_Heartbeat_Init(void);

/**
 * @brief  Heartbeat cyclic main function — 10ms period
 * @note   Handles TX timing (50ms period) and RX timeout monitoring
 */
void Swc_Heartbeat_MainFunction(void);

/**
 * @brief  Called when a heartbeat CAN message is received from another ECU
 * @param  ecuId  Source ECU identifier (CVC_ECU_ID_FZC or CVC_ECU_ID_RZC)
 */
void Swc_Heartbeat_RxIndication(uint8 ecuId);

#endif /* SWC_HEARTBEAT_H */
