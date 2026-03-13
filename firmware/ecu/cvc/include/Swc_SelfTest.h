/**
 * @file    Swc_SelfTest.h
 * @brief   Startup self-test sequence SWC — 7 diagnostic checks
 * @date    2026-02-24
 *
 * @safety_req SWR-CVC-029
 * @traces_to  SSR-CVC-029, TSR-046
 *
 * @details  Runs the 7-step power-on self-test sequence:
 *           1. SPI sensor loopback
 *           2. CAN controller loopback
 *           3. NVM integrity (dual bank CRC)
 *           4. OLED I2C ACK
 *           5. MPU region verify
 *           6. Stack canary plant and check
 *           7. RAM pattern test
 *
 *           Returns aggregate pass/fail. Individual step failures
 *           are reported via Dem DTCs.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_SELFTEST_H
#define SWC_SELFTEST_H

#include "Std_Types.h"

/* ==================================================================
 * Result Codes
 * ================================================================== */

#define SELF_TEST_PASSED       1u
#define SELF_TEST_FAILED       0u

/* ==================================================================
 * Step Result Bitfield
 * ================================================================== */

#define SELFTEST_STEP_SPI          0x01u
#define SELFTEST_STEP_CAN          0x02u
#define SELFTEST_STEP_NVM          0x04u
#define SELFTEST_STEP_OLED         0x08u
#define SELFTEST_STEP_MPU          0x10u
#define SELFTEST_STEP_CANARY       0x20u
#define SELFTEST_STEP_RAM          0x40u
#define SELFTEST_ALL_PASSED        0x7Fu

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Execute the full startup self-test sequence
 * @return SELF_TEST_PASSED if all critical tests pass,
 *         SELF_TEST_FAILED if any critical test fails
 *
 * @note   OLED test failure is non-critical (QM) and does not
 *         cause overall failure; DTC is reported regardless.
 */
uint8 Swc_SelfTest_Startup(void);

/**
 * @brief  Get the step result bitmask from the last run
 * @return Bitmask of SELFTEST_STEP_xxx bits for passed steps
 */
uint8 Swc_SelfTest_GetResults(void);

#endif /* SWC_SELFTEST_H */
