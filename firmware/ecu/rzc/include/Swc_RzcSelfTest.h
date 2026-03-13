/**
 * @file    Swc_RzcSelfTest.h
 * @brief   RZC startup self-test -- 8 checks for power-on validation
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-025
 * @traces_to  SSR-RZC-013
 *
 * @details  Wraps the RZC power-on self-test sequence into a
 *           standalone SWC with testable API:
 *           1. BTS7960 enable pin toggle
 *           2. ACS723 baseline calibration
 *           3. NTC temperature range check
 *           4. Encoder connectivity
 *           5. CAN loopback
 *           6. MPU region verify
 *           7. Stack canary plant
 *           8. RAM pattern test
 *
 *           Returns a bitmask of pass/fail per item and an overall
 *           result. Motor is disabled on any failure.
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#ifndef SWC_RZC_SELF_TEST_H
#define SWC_RZC_SELF_TEST_H

#include "Std_Types.h"

/* ==================================================================
 * Constants
 * ================================================================== */

/** Self-test item bit positions in result bitmask */
#define RZC_ST_BIT_BTS7960      0x01u
#define RZC_ST_BIT_ACS723       0x02u
#define RZC_ST_BIT_NTC          0x04u
#define RZC_ST_BIT_ENCODER      0x08u
#define RZC_ST_BIT_CAN          0x10u
#define RZC_ST_BIT_MPU          0x20u
#define RZC_ST_BIT_CANARY       0x40u
#define RZC_ST_BIT_RAM          0x80u

/** All 8 items passed */
#define RZC_ST_ALL_PASS         0xFFu

/* ==================================================================
 * Types
 * ================================================================== */

/** Hardware test callback type (injected for testability) */
typedef Std_ReturnType (*Swc_RzcSelfTest_HwTestFn)(void);

/** Self-test configuration: one callback per item */
typedef struct {
    Swc_RzcSelfTest_HwTestFn  pfnBts7960;
    Swc_RzcSelfTest_HwTestFn  pfnAcs723;
    Swc_RzcSelfTest_HwTestFn  pfnNtc;
    Swc_RzcSelfTest_HwTestFn  pfnEncoder;
    Swc_RzcSelfTest_HwTestFn  pfnCan;
    Swc_RzcSelfTest_HwTestFn  pfnMpu;
    Swc_RzcSelfTest_HwTestFn  pfnCanary;
    Swc_RzcSelfTest_HwTestFn  pfnRam;
} Swc_RzcSelfTest_CfgType;

/* ==================================================================
 * API Functions
 * ================================================================== */

/**
 * @brief  Initialize the self-test module with HW test callbacks
 * @param  pCfg  Pointer to configuration with 8 HW test callbacks
 */
void Swc_RzcSelfTest_Init(const Swc_RzcSelfTest_CfgType *pCfg);

/**
 * @brief  Run the full 8-item startup self-test sequence
 * @return RZC_SELF_TEST_PASS (1) if all pass, RZC_SELF_TEST_FAIL (0)
 *         if any test fails. Motor outputs are disabled on failure.
 *
 * @safety_req SWR-RZC-025
 */
uint8 Swc_RzcSelfTest_Startup(void);

/**
 * @brief  Get the detailed pass/fail bitmask from the last run
 * @return Bitmask where each bit corresponds to one test item.
 *         Bit set = passed, bit clear = failed or not run.
 */
uint8 Swc_RzcSelfTest_GetResultMask(void);

#endif /* SWC_RZC_SELF_TEST_H */
