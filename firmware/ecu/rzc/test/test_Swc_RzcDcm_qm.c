/**
 * @file    test_Swc_RzcDcm.c
 * @brief   Unit tests for Swc_RzcDcm -- UDS diagnostic services, DIDs F030-F036
 * @date    2026-02-24
 *
 * @verifies SWR-RZC-029
 *
 * Tests supported UDS services (0x10, 0x11, 0x22, 0x3E), DID reads
 * for motor current and battery voltage, and unsupported service NRC.
 *
 * Mocks: Rte_Read, Rte_Write
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6, ISO 14229 UDS
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
 * RZC signal IDs (from Rzc_Cfg.h)
 * ================================================================== */

#define RZC_SIG_TORQUE_CMD         16u
#define RZC_SIG_TORQUE_ECHO        17u
#define RZC_SIG_MOTOR_SPEED        18u
#define RZC_SIG_CURRENT_MA         22u
#define RZC_SIG_TEMP1_DC           24u
#define RZC_SIG_DERATING_PCT       26u
#define RZC_SIG_BATTERY_MV         28u
#define RZC_SIG_ENCODER_SPEED      30u
#define RZC_SIG_SIG_COUNT          40u

/* ==================================================================
 * Constants from Rzc_Cfg.h
 * ================================================================== */

#define RZC_CURRENT_ZEROCAL_CENTER 2048u

/* ==================================================================
 * UDS constants (from Swc_RzcDcm.h)
 * ================================================================== */

#define RZC_UDS_PHYS_REQ_ID     0x7E2u
#define RZC_UDS_FUNC_REQ_ID     0x7DFu
#define RZC_UDS_RESP_ID         0x7EAu

#define RZC_UDS_SID_READ_DID    0x22u
#define RZC_UDS_SID_DIAG_SESS   0x10u
#define RZC_UDS_SID_ECU_RESET   0x11u
#define RZC_UDS_SID_TESTER_PRESENT 0x3Eu

#define RZC_UDS_NRC_SERVICE_NOT_SUPPORTED     0x11u
#define RZC_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED 0x12u
#define RZC_UDS_NRC_REQUEST_OUT_OF_RANGE      0x31u

#define RZC_DID_MOTOR_CURRENT   0xF030u
#define RZC_DID_MOTOR_TEMP      0xF031u
#define RZC_DID_MOTOR_SPEED     0xF032u
#define RZC_DID_BATTERY_VOLTAGE 0xF033u
#define RZC_DID_TORQUE_ECHO     0xF034u
#define RZC_DID_DERATING        0xF035u
#define RZC_DID_ACS723_OFFSET   0xF036u
#define RZC_DID_COUNT           7u

/* ==================================================================
 * Swc_RzcDcm API declarations
 * ================================================================== */

extern void            Swc_RzcDcm_Init(void);
extern Std_ReturnType  Swc_RzcDcm_HandleRequest(const uint8 *reqData,
                                                  uint8 reqLen,
                                                  uint8 *respData,
                                                  uint8 *respLen);

/* ==================================================================
 * Mock: Rte_Read / Rte_Write
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  48u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) { return E_NOT_OK; }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
        return E_OK;
    }
    return E_NOT_OK;
}

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        mock_rte_signals[SignalId] = Data;
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    uint8 i;
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }
    Swc_RzcDcm_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-RZC-029: UDS Service Support
 * ================================================================== */

/** @verifies SWR-RZC-029 -- Supported services: 0x10, 0x11, 0x22, 0x3E respond positively */
void test_RzcDcm_supported_services(void)
{
    uint8 req[4];
    uint8 resp[16];
    uint8 respLen;

    /* DiagnosticSessionControl (0x10) */
    req[0] = RZC_UDS_SID_DIAG_SESS;
    req[1] = 0x01u;
    respLen = 0u;
    (void)Swc_RzcDcm_HandleRequest(req, 2u, resp, &respLen);
    TEST_ASSERT_EQUAL_UINT8(0x50u, resp[0]);  /* Positive response = SID + 0x40 */
    TEST_ASSERT_TRUE(respLen >= 2u);

    /* TesterPresent (0x3E) */
    req[0] = RZC_UDS_SID_TESTER_PRESENT;
    req[1] = 0x00u;
    respLen = 0u;
    (void)Swc_RzcDcm_HandleRequest(req, 2u, resp, &respLen);
    TEST_ASSERT_EQUAL_UINT8(0x7Eu, resp[0]);  /* 0x3E + 0x40 */
    TEST_ASSERT_TRUE(respLen >= 2u);

    /* ECUReset (0x11) */
    req[0] = RZC_UDS_SID_ECU_RESET;
    req[1] = 0x01u;
    respLen = 0u;
    (void)Swc_RzcDcm_HandleRequest(req, 2u, resp, &respLen);
    TEST_ASSERT_EQUAL_UINT8(0x51u, resp[0]);  /* 0x11 + 0x40 */
}

/** @verifies SWR-RZC-029 -- ReadDataByIdentifier 0xF030 returns motor current */
void test_RzcDcm_read_did_motor_current(void)
{
    uint8 req[3];
    uint8 resp[16];
    uint8 respLen;
    uint16 value;

    /* Set motor current to 12500 mA in RTE */
    mock_rte_signals[RZC_SIG_CURRENT_MA] = 12500u;

    /* Request: SID=0x22, DID=0xF030 */
    req[0] = RZC_UDS_SID_READ_DID;
    req[1] = 0xF0u;
    req[2] = 0x30u;
    respLen = 0u;

    (void)Swc_RzcDcm_HandleRequest(req, 3u, resp, &respLen);

    /* Positive response: 0x62, 0xF0, 0x30, data... */
    TEST_ASSERT_EQUAL_UINT8(0x62u, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0xF0u, resp[1]);
    TEST_ASSERT_EQUAL_UINT8(0x30u, resp[2]);
    TEST_ASSERT_TRUE(respLen >= 5u);

    /* Decode 16-bit value (little-endian) */
    value = (uint16)resp[3] | ((uint16)resp[4] << 8u);
    TEST_ASSERT_EQUAL_UINT16(12500u, value);
}

/** @verifies SWR-RZC-029 -- ReadDataByIdentifier 0xF033 returns battery voltage */
void test_RzcDcm_read_did_battery_voltage(void)
{
    uint8 req[3];
    uint8 resp[16];
    uint8 respLen;
    uint16 value;

    /* Set battery voltage to 12600 mV in RTE */
    mock_rte_signals[RZC_SIG_BATTERY_MV] = 12600u;

    req[0] = RZC_UDS_SID_READ_DID;
    req[1] = 0xF0u;
    req[2] = 0x33u;
    respLen = 0u;

    (void)Swc_RzcDcm_HandleRequest(req, 3u, resp, &respLen);

    TEST_ASSERT_EQUAL_UINT8(0x62u, resp[0]);

    value = (uint16)resp[3] | ((uint16)resp[4] << 8u);
    TEST_ASSERT_EQUAL_UINT16(12600u, value);
}

/** @verifies SWR-RZC-029 -- Unsupported service returns NRC 0x11 */
void test_RzcDcm_unsupported_service_nrc(void)
{
    uint8 req[2];
    uint8 resp[16];
    uint8 respLen;

    /* Send unsupported SID 0x99 */
    req[0] = 0x99u;
    req[1] = 0x00u;
    respLen = 0u;

    (void)Swc_RzcDcm_HandleRequest(req, 2u, resp, &respLen);

    /* Negative response: 0x7F, SID, NRC */
    TEST_ASSERT_EQUAL_UINT8(0x7Fu, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(0x99u, resp[1]);
    TEST_ASSERT_EQUAL_UINT8(RZC_UDS_NRC_SERVICE_NOT_SUPPORTED, resp[2]);
    TEST_ASSERT_EQUAL_UINT8(3u, respLen);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-RZC-029: UDS Service Support */
    RUN_TEST(test_RzcDcm_supported_services);
    RUN_TEST(test_RzcDcm_read_did_motor_current);
    RUN_TEST(test_RzcDcm_read_did_battery_voltage);
    RUN_TEST(test_RzcDcm_unsupported_service_nrc);

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
#define SWC_RZC_DCM_H
#define RZC_CFG_H
#define RTE_H
#define WDGM_H
#define DEM_H
#define IOHWAB_H

#include "../src/Swc_RzcDcm.c"
