/**
 * @file    test_Swc_RzcSelfTest.c
 * @brief   Unit tests for Swc_RzcSelfTest -- 8 startup self-test checks
 * @date    2026-02-24
 *
 * @verifies SWR-RZC-025
 *
 * Tests all-pass scenario, individual failure scenarios for BTS7960,
 * ACS723, NTC, CAN loopback, stack canary, and RAM pattern.
 * Verifies motor is disabled and DTC reported on each failure.
 *
 * Mocks: Dem_ReportErrorStatus, Dio_WriteChannel, IoHwAb_SetMotorPWM,
 *        Rte_Read, Rte_Write, injectable HW test callbacks
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed char     sint8;
typedef signed short    sint16;
typedef signed int     sint32;
typedef uint8           Std_ReturnType;
typedef uint8           boolean;

#define E_OK        ((Std_ReturnType)0x00U)
#define E_NOT_OK    ((Std_ReturnType)0x01U)
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * Constants from Rzc_Cfg.h
 * ================================================================== */

#define RZC_SELF_TEST_PASS          1u
#define RZC_SELF_TEST_FAIL          0u
#define RZC_SELF_TEST_ITEMS         8u

#define RZC_MOTOR_R_EN_CHANNEL      5u
#define RZC_MOTOR_L_EN_CHANNEL      6u

#define RZC_DIR_STOP                2u

#define RZC_DTC_SELF_TEST_FAIL     7u
#define RZC_DTC_ZERO_CAL          11u
#define RZC_DTC_CAN_BUS_OFF        5u
#define RZC_DTC_ENCODER           10u

#define DEM_EVENT_STATUS_PASSED    0u
#define DEM_EVENT_STATUS_FAILED    1u

/* ==================================================================
 * Self-test bit positions (from Swc_RzcSelfTest.h)
 * ================================================================== */

#define RZC_ST_BIT_BTS7960      0x01u
#define RZC_ST_BIT_ACS723       0x02u
#define RZC_ST_BIT_NTC          0x04u
#define RZC_ST_BIT_ENCODER      0x08u
#define RZC_ST_BIT_CAN          0x10u
#define RZC_ST_BIT_MPU          0x20u
#define RZC_ST_BIT_CANARY       0x40u
#define RZC_ST_BIT_RAM          0x80u
#define RZC_ST_ALL_PASS         0xFFu

/* ==================================================================
 * Types (from Swc_RzcSelfTest.h)
 * ================================================================== */

typedef Std_ReturnType (*Swc_RzcSelfTest_HwTestFn)(void);

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
 * Swc_RzcSelfTest API declarations
 * ================================================================== */

extern void  Swc_RzcSelfTest_Init(const Swc_RzcSelfTest_CfgType *pCfg);
extern uint8 Swc_RzcSelfTest_Startup(void);
extern uint8 Swc_RzcSelfTest_GetResultMask(void);

/* ==================================================================
 * Mock: DEM
 * ================================================================== */

static uint8 mock_dem_call_count;
static uint8 mock_dem_last_event_id;
static uint8 mock_dem_last_status;

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_call_count++;
    mock_dem_last_event_id = EventId;
    mock_dem_last_status   = EventStatus;
}

/* ==================================================================
 * Mock: Dio_WriteChannel
 * ================================================================== */

#define MOCK_DIO_MAX_CHANNELS  16u
static uint8 mock_dio_state[MOCK_DIO_MAX_CHANNELS];
static uint8 mock_dio_call_count;

void Dio_WriteChannel(uint8 Channel, uint8 Level)
{
    mock_dio_call_count++;
    if (Channel < MOCK_DIO_MAX_CHANNELS) {
        mock_dio_state[Channel] = Level;
    }
}

/* ==================================================================
 * Mock: IoHwAb_SetMotorPWM
 * ================================================================== */

static uint8  mock_motor_pwm_call_count;
static uint8  mock_motor_pwm_last_dir;
static uint16 mock_motor_pwm_last_duty;

void IoHwAb_SetMotorPWM(uint8 Direction, uint16 DutyCycle)
{
    mock_motor_pwm_call_count++;
    mock_motor_pwm_last_dir  = Direction;
    mock_motor_pwm_last_duty = DutyCycle;
}

/* ==================================================================
 * Mock: Rte (not used by self-test, but required for link)
 * ================================================================== */

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    (void)SignalId; (void)DataPtr;
    return E_OK;
}

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    (void)SignalId; (void)Data;
    return E_OK;
}

/* ==================================================================
 * Injectable HW Test Callbacks
 * ================================================================== */

static Std_ReturnType mock_hw_pass(void)  __attribute__((unused));
static Std_ReturnType mock_hw_pass(void)  { return E_OK; }
static Std_ReturnType mock_hw_fail(void)  __attribute__((unused));
static Std_ReturnType mock_hw_fail(void)  { return E_NOT_OK; }

/* Track which callback was actually called */
static uint8 mock_bts_called;
static uint8 mock_acs_called;
static uint8 mock_ntc_called;
static uint8 mock_enc_called;
static uint8 mock_can_called;
static uint8 mock_mpu_called;
static uint8 mock_canary_called;
static uint8 mock_ram_called;

static Std_ReturnType mock_bts_pass(void)     { mock_bts_called++;    return E_OK; }
static Std_ReturnType mock_acs_pass(void)     { mock_acs_called++;    return E_OK; }
static Std_ReturnType mock_ntc_pass(void)     { mock_ntc_called++;    return E_OK; }
static Std_ReturnType mock_enc_pass(void)     { mock_enc_called++;    return E_OK; }
static Std_ReturnType mock_can_pass(void)     { mock_can_called++;    return E_OK; }
static Std_ReturnType mock_mpu_pass(void)     { mock_mpu_called++;    return E_OK; }
static Std_ReturnType mock_canary_pass(void)  { mock_canary_called++; return E_OK; }
static Std_ReturnType mock_ram_pass(void)     { mock_ram_called++;    return E_OK; }

static Std_ReturnType mock_bts_fail(void)     { mock_bts_called++;    return E_NOT_OK; }
static Std_ReturnType mock_acs_fail(void)     { mock_acs_called++;    return E_NOT_OK; }
static Std_ReturnType mock_ntc_fail(void)     { mock_ntc_called++;    return E_NOT_OK; }
static Std_ReturnType mock_can_fail(void)     { mock_can_called++;    return E_NOT_OK; }
static Std_ReturnType mock_canary_fail(void)  { mock_canary_called++; return E_NOT_OK; }
static Std_ReturnType mock_ram_fail(void)     { mock_ram_called++;    return E_NOT_OK; }

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static Swc_RzcSelfTest_CfgType all_pass_cfg;

void setUp(void)
{
    uint8 i;

    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;

    mock_dio_call_count = 0u;
    for (i = 0u; i < MOCK_DIO_MAX_CHANNELS; i++) {
        mock_dio_state[i] = 0xFFu;
    }

    mock_motor_pwm_call_count = 0u;
    mock_motor_pwm_last_dir   = 0xFFu;
    mock_motor_pwm_last_duty  = 0xFFFFu;

    mock_bts_called    = 0u;
    mock_acs_called    = 0u;
    mock_ntc_called    = 0u;
    mock_enc_called    = 0u;
    mock_can_called    = 0u;
    mock_mpu_called    = 0u;
    mock_canary_called = 0u;
    mock_ram_called    = 0u;

    /* Default: all pass */
    all_pass_cfg.pfnBts7960 = mock_bts_pass;
    all_pass_cfg.pfnAcs723  = mock_acs_pass;
    all_pass_cfg.pfnNtc     = mock_ntc_pass;
    all_pass_cfg.pfnEncoder = mock_enc_pass;
    all_pass_cfg.pfnCan     = mock_can_pass;
    all_pass_cfg.pfnMpu     = mock_mpu_pass;
    all_pass_cfg.pfnCanary  = mock_canary_pass;
    all_pass_cfg.pfnRam     = mock_ram_pass;

    Swc_RzcSelfTest_Init(&all_pass_cfg);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-RZC-025: Startup Self-Test
 * ================================================================== */

/** @verifies SWR-RZC-025 -- All 8 tests pass: returns PASS, mask = 0xFF */
void test_RzcSelfTest_all_pass(void)
{
    uint8 result;
    uint8 mask;

    result = Swc_RzcSelfTest_Startup();
    mask   = Swc_RzcSelfTest_GetResultMask();

    TEST_ASSERT_EQUAL_UINT8(RZC_SELF_TEST_PASS, result);
    TEST_ASSERT_EQUAL_UINT8(RZC_ST_ALL_PASS, mask);

    /* All callbacks should have been called exactly once */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_bts_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_acs_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_ntc_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_enc_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_mpu_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_canary_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_ram_called);

    /* No DEM error reported */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dem_call_count);
}

/** @verifies SWR-RZC-025 -- BTS7960 fail: motor disabled, DTC reported */
void test_RzcSelfTest_bts7960_fail_motor_disabled(void)
{
    Swc_RzcSelfTest_CfgType cfg;
    uint8 result;

    cfg = all_pass_cfg;
    cfg.pfnBts7960 = mock_bts_fail;
    Swc_RzcSelfTest_Init(&cfg);

    result = Swc_RzcSelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(RZC_SELF_TEST_FAIL, result);

    /* Motor should be disabled: R_EN and L_EN set LOW */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_state[RZC_MOTOR_R_EN_CHANNEL]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_state[RZC_MOTOR_L_EN_CHANNEL]);

    /* DTC should be reported */
    TEST_ASSERT_TRUE(mock_dem_call_count > 0u);
    TEST_ASSERT_EQUAL_UINT8(RZC_DTC_SELF_TEST_FAIL, mock_dem_last_event_id);

    /* Subsequent tests should NOT have been called */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_acs_called);
}

/** @verifies SWR-RZC-025 -- ACS723 fail: motor disabled, DTC reported */
void test_RzcSelfTest_acs723_fail_motor_disabled(void)
{
    Swc_RzcSelfTest_CfgType cfg;
    uint8 result;

    cfg = all_pass_cfg;
    cfg.pfnAcs723 = mock_acs_fail;
    Swc_RzcSelfTest_Init(&cfg);

    result = Swc_RzcSelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(RZC_SELF_TEST_FAIL, result);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_state[RZC_MOTOR_R_EN_CHANNEL]);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_state[RZC_MOTOR_L_EN_CHANNEL]);

    /* ACS723 failure should report zero-cal DTC */
    TEST_ASSERT_EQUAL_UINT8(RZC_DTC_ZERO_CAL, mock_dem_last_event_id);

    /* BTS7960 should have passed, NTC should not have been called */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_bts_called);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_ntc_called);
}

/** @verifies SWR-RZC-025 -- NTC out of range: motor disabled, DTC reported */
void test_RzcSelfTest_ntc_out_of_range(void)
{
    Swc_RzcSelfTest_CfgType cfg;
    uint8 result;
    uint8 mask;

    cfg = all_pass_cfg;
    cfg.pfnNtc = mock_ntc_fail;
    Swc_RzcSelfTest_Init(&cfg);

    result = Swc_RzcSelfTest_Startup();
    mask   = Swc_RzcSelfTest_GetResultMask();

    TEST_ASSERT_EQUAL_UINT8(RZC_SELF_TEST_FAIL, result);

    /* BTS7960 and ACS723 should have passed */
    TEST_ASSERT_TRUE((mask & RZC_ST_BIT_BTS7960) != 0u);
    TEST_ASSERT_TRUE((mask & RZC_ST_BIT_ACS723) != 0u);

    /* NTC should have failed (bit clear) */
    TEST_ASSERT_TRUE((mask & RZC_ST_BIT_NTC) == 0u);

    TEST_ASSERT_EQUAL_UINT8(RZC_DTC_SELF_TEST_FAIL, mock_dem_last_event_id);
}

/** @verifies SWR-RZC-025 -- CAN loopback fail: motor disabled, DTC reported */
void test_RzcSelfTest_can_loopback_fail(void)
{
    Swc_RzcSelfTest_CfgType cfg;
    uint8 result;

    cfg = all_pass_cfg;
    cfg.pfnCan = mock_can_fail;
    Swc_RzcSelfTest_Init(&cfg);

    result = Swc_RzcSelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(RZC_SELF_TEST_FAIL, result);
    TEST_ASSERT_EQUAL_UINT8(RZC_DTC_CAN_BUS_OFF, mock_dem_last_event_id);

    /* Items 1-4 should have been called; items 5-8 should not */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_bts_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_acs_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_ntc_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_enc_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_can_called);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_mpu_called);
}

/** @verifies SWR-RZC-025 -- Stack canary fail: motor disabled, DTC reported */
void test_RzcSelfTest_canary_fail(void)
{
    Swc_RzcSelfTest_CfgType cfg;
    uint8 result;

    cfg = all_pass_cfg;
    cfg.pfnCanary = mock_canary_fail;
    Swc_RzcSelfTest_Init(&cfg);

    result = Swc_RzcSelfTest_Startup();

    TEST_ASSERT_EQUAL_UINT8(RZC_SELF_TEST_FAIL, result);
    TEST_ASSERT_EQUAL_UINT8(RZC_DTC_SELF_TEST_FAIL, mock_dem_last_event_id);

    /* Items 1-6 passed, item 7 failed, item 8 not called */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_mpu_called);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_canary_called);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_ram_called);
}

/** @verifies SWR-RZC-025 -- RAM pattern fail: motor disabled, DTC reported */
void test_RzcSelfTest_ram_fail(void)
{
    Swc_RzcSelfTest_CfgType cfg;
    uint8 result;
    uint8 mask;

    cfg = all_pass_cfg;
    cfg.pfnRam = mock_ram_fail;
    Swc_RzcSelfTest_Init(&cfg);

    result = Swc_RzcSelfTest_Startup();
    mask   = Swc_RzcSelfTest_GetResultMask();

    TEST_ASSERT_EQUAL_UINT8(RZC_SELF_TEST_FAIL, result);

    /* Items 1-7 should have passed (bits set), item 8 failed (bit clear) */
    TEST_ASSERT_EQUAL_UINT8(0x7Fu, mask);  /* 0111 1111 = items 1-7 passed */
    TEST_ASSERT_TRUE((mask & RZC_ST_BIT_RAM) == 0u);

    TEST_ASSERT_EQUAL_UINT8(RZC_DTC_SELF_TEST_FAIL, mock_dem_last_event_id);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-RZC-025: Startup Self-Test */
    RUN_TEST(test_RzcSelfTest_all_pass);
    RUN_TEST(test_RzcSelfTest_bts7960_fail_motor_disabled);
    RUN_TEST(test_RzcSelfTest_acs723_fail_motor_disabled);
    RUN_TEST(test_RzcSelfTest_ntc_out_of_range);
    RUN_TEST(test_RzcSelfTest_can_loopback_fail);
    RUN_TEST(test_RzcSelfTest_canary_fail);
    RUN_TEST(test_RzcSelfTest_ram_fail);

    return UNITY_END();
}

/* ==================================================================
 * Include implementation under test (source inclusion pattern)
 *
 * Pre-define BSW header guards so that the real BSW headers are NOT
 * pulled in -- the test already provides its own mock declarations.
 * ================================================================== */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_RZC_SELF_TEST_H
#define RZC_CFG_H
#define DEM_H
#define RTE_H
#define IOHWAB_H
#define WDGM_H

#include "../src/Swc_RzcSelfTest.c"
