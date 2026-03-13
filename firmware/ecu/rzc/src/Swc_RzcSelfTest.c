/**
 * @file    Swc_RzcSelfTest.c
 * @brief   RZC startup self-test -- 8 checks for power-on validation
 * @date    2026-02-24
 *
 * @safety_req SWR-RZC-025
 * @traces_to  SSR-RZC-013
 *
 * @details  Implements the RZC startup self-test as a standalone SWC
 *           with injectable hardware callbacks for unit testability:
 *
 *           Item  Test                       Failure Action
 *           1     BTS7960 enable pin toggle  Motor disabled, DTC
 *           2     ACS723 baseline cal        Motor disabled, DTC
 *           3     NTC temperature range      Motor disabled, DTC
 *           4     Encoder connectivity       Motor disabled, DTC
 *           5     CAN loopback               Motor disabled, DTC
 *           6     MPU region verify          Motor disabled, DTC
 *           7     Stack canary plant         Motor disabled, DTC
 *           8     RAM pattern test           Motor disabled, DTC
 *
 *           Any single failure aborts the sequence and returns FAIL.
 *           The result bitmask records which items passed/failed.
 *
 *           All variables are static file-scope. No dynamic memory.
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */

#include "Swc_RzcSelfTest.h"
#include "Rzc_Cfg.h"

/* ==================================================================
 * BSW Includes
 * ================================================================== */

#include "Dem.h"
#include "Rte.h"
#include "IoHwAb.h"

/* ==================================================================
 * Module State (all static file-scope)
 * ================================================================== */

/** Hardware test callbacks (injected at init) */
static Swc_RzcSelfTest_CfgType  SelfTest_Cfg;

/** Module initialization flag */
static uint8  SelfTest_Initialized;

/** Result bitmask from last run (bit set = passed) */
static uint8  SelfTest_ResultMask;

/* ==================================================================
 * Private: Disable motor outputs (safe state)
 * ================================================================== */

static void SelfTest_DisableMotor(void)
{
    Dio_WriteChannel(RZC_MOTOR_R_EN_CHANNEL, 0u);
    Dio_WriteChannel(RZC_MOTOR_L_EN_CHANNEL, 0u);
    (void)IoHwAb_SetMotorPWM(RZC_DIR_STOP, 0u);
}

/* ==================================================================
 * API: Swc_RzcSelfTest_Init
 * ================================================================== */

void Swc_RzcSelfTest_Init(const Swc_RzcSelfTest_CfgType *pCfg)
{
    SelfTest_ResultMask = 0u;
    SelfTest_Initialized = FALSE;

    if (pCfg == NULL_PTR)
    {
        return;
    }

    SelfTest_Cfg.pfnBts7960 = pCfg->pfnBts7960;
    SelfTest_Cfg.pfnAcs723  = pCfg->pfnAcs723;
    SelfTest_Cfg.pfnNtc     = pCfg->pfnNtc;
    SelfTest_Cfg.pfnEncoder = pCfg->pfnEncoder;
    SelfTest_Cfg.pfnCan     = pCfg->pfnCan;
    SelfTest_Cfg.pfnMpu     = pCfg->pfnMpu;
    SelfTest_Cfg.pfnCanary  = pCfg->pfnCanary;
    SelfTest_Cfg.pfnRam     = pCfg->pfnRam;

    SelfTest_Initialized = TRUE;
}

/* ==================================================================
 * API: Swc_RzcSelfTest_Startup
 * ================================================================== */

uint8 Swc_RzcSelfTest_Startup(void)
{
    uint8 mask;

    mask = 0u;

    if (SelfTest_Initialized != TRUE)
    {
        return RZC_SELF_TEST_FAIL;
    }

    /* Item 1: BTS7960 enable pin toggle */
    if ((SelfTest_Cfg.pfnBts7960 != NULL_PTR) &&
        (SelfTest_Cfg.pfnBts7960() == E_OK))
    {
        mask |= RZC_ST_BIT_BTS7960;
    }
    else
    {
        SelfTest_DisableMotor();
        Dem_ReportErrorStatus(RZC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        SelfTest_ResultMask = mask;
        return RZC_SELF_TEST_FAIL;
    }

    /* Item 2: ACS723 baseline calibration */
    if ((SelfTest_Cfg.pfnAcs723 != NULL_PTR) &&
        (SelfTest_Cfg.pfnAcs723() == E_OK))
    {
        mask |= RZC_ST_BIT_ACS723;
    }
    else
    {
        SelfTest_DisableMotor();
        Dem_ReportErrorStatus(RZC_DTC_ZERO_CAL, DEM_EVENT_STATUS_FAILED);
        SelfTest_ResultMask = mask;
        return RZC_SELF_TEST_FAIL;
    }

    /* Item 3: NTC temperature range check */
    if ((SelfTest_Cfg.pfnNtc != NULL_PTR) &&
        (SelfTest_Cfg.pfnNtc() == E_OK))
    {
        mask |= RZC_ST_BIT_NTC;
    }
    else
    {
        SelfTest_DisableMotor();
        Dem_ReportErrorStatus(RZC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        SelfTest_ResultMask = mask;
        return RZC_SELF_TEST_FAIL;
    }

    /* Item 4: Encoder connectivity */
    if ((SelfTest_Cfg.pfnEncoder != NULL_PTR) &&
        (SelfTest_Cfg.pfnEncoder() == E_OK))
    {
        mask |= RZC_ST_BIT_ENCODER;
    }
    else
    {
        SelfTest_DisableMotor();
        Dem_ReportErrorStatus(RZC_DTC_ENCODER, DEM_EVENT_STATUS_FAILED);
        SelfTest_ResultMask = mask;
        return RZC_SELF_TEST_FAIL;
    }

    /* Item 5: CAN loopback */
    if ((SelfTest_Cfg.pfnCan != NULL_PTR) &&
        (SelfTest_Cfg.pfnCan() == E_OK))
    {
        mask |= RZC_ST_BIT_CAN;
    }
    else
    {
        SelfTest_DisableMotor();
        Dem_ReportErrorStatus(RZC_DTC_CAN_BUS_OFF, DEM_EVENT_STATUS_FAILED);
        SelfTest_ResultMask = mask;
        return RZC_SELF_TEST_FAIL;
    }

    /* Item 6: MPU region verify */
    if ((SelfTest_Cfg.pfnMpu != NULL_PTR) &&
        (SelfTest_Cfg.pfnMpu() == E_OK))
    {
        mask |= RZC_ST_BIT_MPU;
    }
    else
    {
        SelfTest_DisableMotor();
        Dem_ReportErrorStatus(RZC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        SelfTest_ResultMask = mask;
        return RZC_SELF_TEST_FAIL;
    }

    /* Item 7: Stack canary plant */
    if ((SelfTest_Cfg.pfnCanary != NULL_PTR) &&
        (SelfTest_Cfg.pfnCanary() == E_OK))
    {
        mask |= RZC_ST_BIT_CANARY;
    }
    else
    {
        SelfTest_DisableMotor();
        Dem_ReportErrorStatus(RZC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        SelfTest_ResultMask = mask;
        return RZC_SELF_TEST_FAIL;
    }

    /* Item 8: RAM pattern test */
    if ((SelfTest_Cfg.pfnRam != NULL_PTR) &&
        (SelfTest_Cfg.pfnRam() == E_OK))
    {
        mask |= RZC_ST_BIT_RAM;
    }
    else
    {
        SelfTest_DisableMotor();
        Dem_ReportErrorStatus(RZC_DTC_SELF_TEST_FAIL, DEM_EVENT_STATUS_FAILED);
        SelfTest_ResultMask = mask;
        return RZC_SELF_TEST_FAIL;
    }

    /* All 8 items passed */
    SelfTest_ResultMask = mask;
    return RZC_SELF_TEST_PASS;
}

/* ==================================================================
 * API: Swc_RzcSelfTest_GetResultMask
 * ================================================================== */

uint8 Swc_RzcSelfTest_GetResultMask(void)
{
    return SelfTest_ResultMask;
}
