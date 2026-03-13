/**
 * @file    sc_selftest.h
 * @brief   Startup and runtime self-test for Safety Controller
 * @date    2026-02-23
 *
 * @details 7-step startup BIST (lockstep, RAM, flash CRC, DCAN loopback,
 *          GPIO readback, lamp test, watchdog test). Runtime periodic test
 *          (flash CRC incremental, RAM pattern, DCAN error, GPIO readback).
 *          Stack canary check every main loop iteration.
 *
 * @safety_req SWR-SC-016, SWR-SC-017, SWR-SC-018, SWR-SC-019,
 *             SWR-SC-020, SWR-SC-021
 * @traces_to  SSR-SC-010, SSR-SC-011, SSR-SC-016, SSR-SC-017
 * @note    Safety level: ASIL D
 * @standard ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SC_SELFTEST_H
#define SC_SELFTEST_H

#include "sc_types.h"

/**
 * @brief  Initialize self-test module — plant canary, write RAM pattern
 */
void SC_SelfTest_Init(void);

/**
 * @brief  Execute 7-step startup self-test
 *
 * Steps: (1) lockstep BIST, (2) RAM PBIST, (3) flash CRC-32,
 *        (4) DCAN loopback, (5) GPIO readback, (6) lamp test,
 *        (7) watchdog test.
 *
 * @return 0 on success, step number (1-7) on failure
 */
uint8 SC_SelfTest_Startup(void);

/**
 * @brief  10ms cyclic runtime self-test — 1 step per call, spread over 60s
 */
void SC_SelfTest_Runtime(void);

/**
 * @brief  Check stack canary integrity
 * @return TRUE if canary value matches expected
 */
boolean SC_SelfTest_StackCanaryOk(void);

/**
 * @brief  Check if all self-tests are passing
 * @return TRUE if startup passed and runtime checks are healthy
 */
boolean SC_SelfTest_IsHealthy(void);

#endif /* SC_SELFTEST_H */
