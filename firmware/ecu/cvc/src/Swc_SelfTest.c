/**
 * @file    Swc_SelfTest.c
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
 *           4. OLED I2C ACK (non-critical)
 *           5. MPU region verify
 *           6. Stack canary plant and check
 *           7. RAM pattern test
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_SelfTest.h"
#include "Cvc_Cfg.h"
#include "Dem.h"

/* ==================================================================
 * Hardware test externs (platform-specific implementation)
 * ================================================================== */

extern Std_ReturnType SelfTest_Hw_SpiLoopback(void);
extern Std_ReturnType SelfTest_Hw_CanLoopback(void);
extern Std_ReturnType SelfTest_Hw_NvmCheck(void);
extern Std_ReturnType SelfTest_Hw_OledAck(void);
extern Std_ReturnType SelfTest_Hw_MpuVerify(void);
extern Std_ReturnType SelfTest_Hw_CanaryCheck(void);
extern Std_ReturnType SelfTest_Hw_RamPattern(void);

/* ==================================================================
 * Module State (all static file-scope — ASIL D: no dynamic memory)
 * ================================================================== */

static uint8  SelfTest_StepResults;

/* ==================================================================
 * API: Swc_SelfTest_Startup
 * ================================================================== */

/**
 * @safety_req SWR-CVC-029
 */
uint8 Swc_SelfTest_Startup(void)
{
    uint8 result;

    SelfTest_StepResults = 0u;
    result = SELF_TEST_PASSED;

    /* ---- Step 1: SPI sensor loopback ---- */
    if (SelfTest_Hw_SpiLoopback() == E_OK)
    {
        SelfTest_StepResults |= SELFTEST_STEP_SPI;
    }
    else
    {
        Dem_ReportErrorStatus(CVC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        result = SELF_TEST_FAILED;
        return result;
    }

    /* ---- Step 2: CAN controller loopback ---- */
    if (SelfTest_Hw_CanLoopback() == E_OK)
    {
        SelfTest_StepResults |= SELFTEST_STEP_CAN;
    }
    else
    {
        Dem_ReportErrorStatus(CVC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        result = SELF_TEST_FAILED;
        return result;
    }

    /* ---- Step 3: NVM integrity ---- */
    if (SelfTest_Hw_NvmCheck() == E_OK)
    {
        SelfTest_StepResults |= SELFTEST_STEP_NVM;
    }
    else
    {
        /* NVM failure: load defaults and report DTC, but continue */
        Dem_ReportErrorStatus(CVC_DTC_NVM_CRC_FAIL, DEM_EVENT_STATUS_FAILED);
        /* Both banks corrupt is critical */
        result = SELF_TEST_FAILED;
        return result;
    }

    /* ---- Step 4: OLED I2C ACK (non-critical QM) ---- */
    if (SelfTest_Hw_OledAck() == E_OK)
    {
        SelfTest_StepResults |= SELFTEST_STEP_OLED;
    }
    else
    {
        Dem_ReportErrorStatus(CVC_DTC_DISPLAY_COMM, DEM_EVENT_STATUS_FAILED);
        /* Non-critical: do NOT fail self-test */
    }

    /* ---- Step 5: MPU region verify ---- */
    if (SelfTest_Hw_MpuVerify() == E_OK)
    {
        SelfTest_StepResults |= SELFTEST_STEP_MPU;
    }
    else
    {
        Dem_ReportErrorStatus(CVC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        result = SELF_TEST_FAILED;
        return result;
    }

    /* ---- Step 6: Stack canary check ---- */
    if (SelfTest_Hw_CanaryCheck() == E_OK)
    {
        SelfTest_StepResults |= SELFTEST_STEP_CANARY;
    }
    else
    {
        Dem_ReportErrorStatus(CVC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        result = SELF_TEST_FAILED;
        return result;
    }

    /* ---- Step 7: RAM pattern test ---- */
    if (SelfTest_Hw_RamPattern() == E_OK)
    {
        SelfTest_StepResults |= SELFTEST_STEP_RAM;
    }
    else
    {
        Dem_ReportErrorStatus(CVC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        result = SELF_TEST_FAILED;
        return result;
    }

    return result;
}

/* ==================================================================
 * API: Swc_SelfTest_GetResults
 * ================================================================== */

uint8 Swc_SelfTest_GetResults(void)
{
    return SelfTest_StepResults;
}
