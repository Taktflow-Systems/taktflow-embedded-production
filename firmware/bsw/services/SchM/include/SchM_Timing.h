/**
 * @file    SchM_Timing.h
 * @brief   WCET measurement — runtime execution time capture
 * @date    2026-03-21
 *
 * @details Lightweight instrumentation for measuring worst-case execution
 *          time (WCET) of BSW and SWC functions. Uses hardware cycle counter
 *          (DWT on Cortex-M, RTICOMP on TMS570, clock() on POSIX).
 *
 *          Usage:
 *            SchM_TimingStart(TIMING_ID_COM_MAIN_TX);
 *            Com_MainFunction_Tx();
 *            SchM_TimingStop(TIMING_ID_COM_MAIN_TX);
 *
 *          Results accessible via g_timing_max_us[] for XCP/UDS readout.
 *
 * @safety_req SWR-BSW-028 (timing verification)
 * @traces_to  ISO 26262 Part 6, Section 9 (Resource Usage)
 * @copyright Taktflow Systems 2026
 */
#ifndef SCHM_TIMING_H
#define SCHM_TIMING_H

#include "Std_Types.h"

/* ---- Timing Point IDs ---- */

#define TIMING_ID_COM_MAIN_TX       0u
#define TIMING_ID_COM_MAIN_RX       1u
#define TIMING_ID_COM_RX_INDICATION 2u
#define TIMING_ID_E2E_PROTECT       3u
#define TIMING_ID_E2E_CHECK         4u
#define TIMING_ID_SWC_HEARTBEAT     5u
#define TIMING_ID_SWC_BRAKE         6u
#define TIMING_ID_SWC_MOTOR         7u
#define TIMING_ID_SWC_PEDAL         8u
#define TIMING_ID_SWC_VEHICLE_STATE 9u
#define TIMING_ID_SWC_SAFETY       10u
#define TIMING_ID_DEM_MAIN         11u
#define TIMING_ID_XCP_RX           12u
#define TIMING_ID_CANSM_MAIN       13u
#define TIMING_ID_FIM_MAIN         14u
#define TIMING_MAX_IDS             16u

/* ---- Results (readable via XCP/UDS) ---- */

/** Maximum execution time in microseconds per timing point */
extern volatile uint32 g_timing_max_us[TIMING_MAX_IDS];

/** Last execution time in microseconds */
extern volatile uint32 g_timing_last_us[TIMING_MAX_IDS];

/** Call count per timing point */
extern volatile uint32 g_timing_count[TIMING_MAX_IDS];

/* ---- API ---- */

/**
 * @brief  Initialize timing measurement (reset all counters, configure HW timer)
 */
void SchM_TimingInit(void);

/**
 * @brief  Start timing measurement for a given point
 * @param  TimingId  Timing point ID (TIMING_ID_*)
 */
void SchM_TimingStart(uint8 TimingId);

/**
 * @brief  Stop timing measurement and update max/last
 * @param  TimingId  Timing point ID (TIMING_ID_*)
 */
void SchM_TimingStop(uint8 TimingId);

/**
 * @brief  Reset all timing statistics (max, last, count)
 */
void SchM_TimingReset(void);

#endif /* SCHM_TIMING_H */
