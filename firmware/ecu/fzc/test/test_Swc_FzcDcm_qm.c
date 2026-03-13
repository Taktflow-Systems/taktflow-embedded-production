/**
 * @file    test_Swc_FzcDcm.c
 * @brief   Unit tests for Swc_FzcDcm — UDS diagnostic service handler
 * @date    2026-02-24
 *
 * @verifies SWR-FZC-030
 *
 * Tests supported UDS services (8 services), ReadDataByIdentifier for
 * steering angle and lidar distance DIDs, unsupported service negative
 * response, and DID handling.
 *
 * Mocks: Rte_Read
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef signed short    sint16;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * FZC Config / UDS Constants
 * ================================================================== */

#define FZC_SIG_STEER_ANGLE        17u
#define FZC_SIG_BRAKE_POS          20u
#define FZC_SIG_LIDAR_DIST         22u
#define FZC_SIG_LIDAR_SIGNAL       23u
#define FZC_SIG_LIDAR_ZONE         24u
#define FZC_SIG_COUNT              36u

/* UDS SIDs */
#define FZC_UDS_SID_DIAG_SESSION      0x10u
#define FZC_UDS_SID_ECU_RESET         0x11u
#define FZC_UDS_SID_CLEAR_DTC         0x14u
#define FZC_UDS_SID_READ_DTC          0x19u
#define FZC_UDS_SID_READ_DID          0x22u
#define FZC_UDS_SID_COMM_CTRL         0x28u
#define FZC_UDS_SID_TESTER_PRESENT    0x3Eu
#define FZC_UDS_SID_CTRL_DTC_SETTING  0x85u

#define FZC_UDS_SUPPORTED_COUNT        8u

/* DIDs */
#define FZC_DID_VIN                 0xF190u
#define FZC_DID_HW_VERSION         0xF191u
#define FZC_DID_SW_VERSION         0xF195u
#define FZC_DID_STEERING_ANGLE     0xF020u
#define FZC_DID_BRAKE_POSITION     0xF021u
#define FZC_DID_LIDAR_DISTANCE     0xF022u
#define FZC_DID_LIDAR_SIGNAL       0xF023u
#define FZC_DID_LIDAR_ZONE         0xF024u

/* NRC */
#define FZC_UDS_NRC_SERVICE_NOT_SUPPORTED   0x11u
#define FZC_UDS_NRC_REQUEST_OUT_OF_RANGE    0x31u

/* UDS response markers */
#define UDS_POSITIVE_RESP_SID_OFFSET   0x40u
#define UDS_NEGATIVE_RESP_SID          0x7Fu

/* ==================================================================
 * Swc_FzcDcm API declarations
 * ================================================================== */

extern void            Swc_FzcDcm_Init(void);
extern Std_ReturnType  Swc_FzcDcm_ProcessRequest(
    const uint8* reqData, uint8 reqLen,
    uint8* respData, uint8* respLen);
extern uint8           Swc_FzcDcm_IsServiceSupported(uint8 sid);

/* ==================================================================
 * Mock: Rte_Read
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  64u

static uint32 mock_rte_signals[MOCK_RTE_MAX_SIGNALS];

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
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

    Swc_FzcDcm_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-FZC-030: Supported Services (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-030 — All 8 specified UDS services are supported */
void test_FzcDcm_supported_services(void)
{
    /* All 8 services should be supported */
    TEST_ASSERT_EQUAL_UINT8(TRUE, Swc_FzcDcm_IsServiceSupported(0x10u));
    TEST_ASSERT_EQUAL_UINT8(TRUE, Swc_FzcDcm_IsServiceSupported(0x11u));
    TEST_ASSERT_EQUAL_UINT8(TRUE, Swc_FzcDcm_IsServiceSupported(0x14u));
    TEST_ASSERT_EQUAL_UINT8(TRUE, Swc_FzcDcm_IsServiceSupported(0x19u));
    TEST_ASSERT_EQUAL_UINT8(TRUE, Swc_FzcDcm_IsServiceSupported(0x22u));
    TEST_ASSERT_EQUAL_UINT8(TRUE, Swc_FzcDcm_IsServiceSupported(0x28u));
    TEST_ASSERT_EQUAL_UINT8(TRUE, Swc_FzcDcm_IsServiceSupported(0x3Eu));
    TEST_ASSERT_EQUAL_UINT8(TRUE, Swc_FzcDcm_IsServiceSupported(0x85u));
}

/* ==================================================================
 * SWR-FZC-030: Read DID — Steering Angle (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-030 — ReadDataByIdentifier (0x22) returns steering angle DID */
void test_FzcDcm_read_did_steering_angle(void)
{
    uint8 req[3];
    uint8 resp[64];
    uint8 respLen;
    Std_ReturnType ret;

    /* Set steering angle to 25 degrees in RTE */
    mock_rte_signals[FZC_SIG_STEER_ANGLE] = 25u;

    /* Build request: SID=0x22, DID=0xF020 (big-endian) */
    req[0] = FZC_UDS_SID_READ_DID;
    req[1] = 0xF0u;
    req[2] = 0x20u;

    ret = Swc_FzcDcm_ProcessRequest(req, 3u, resp, &respLen);

    /* assert: success */
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* assert: positive response (SID + 0x40) */
    TEST_ASSERT_EQUAL_UINT8((uint8)(FZC_UDS_SID_READ_DID + UDS_POSITIVE_RESP_SID_OFFSET), resp[0]);

    /* assert: DID echoed back */
    TEST_ASSERT_EQUAL_UINT8(0xF0u, resp[1]);
    TEST_ASSERT_EQUAL_UINT8(0x20u, resp[2]);

    /* assert: steering angle value = 25 (low byte) */
    TEST_ASSERT_EQUAL_UINT8(25u, resp[3]);
}

/* ==================================================================
 * SWR-FZC-030: Read DID — Lidar Distance (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-030 — ReadDataByIdentifier returns lidar distance DID */
void test_FzcDcm_read_did_lidar_distance(void)
{
    uint8 req[3];
    uint8 resp[64];
    uint8 respLen;
    Std_ReturnType ret;

    /* Set lidar distance to 150 cm in RTE */
    mock_rte_signals[FZC_SIG_LIDAR_DIST] = 150u;

    /* Build request: SID=0x22, DID=0xF022 */
    req[0] = FZC_UDS_SID_READ_DID;
    req[1] = 0xF0u;
    req[2] = 0x22u;

    ret = Swc_FzcDcm_ProcessRequest(req, 3u, resp, &respLen);

    /* assert: positive response */
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8((uint8)(FZC_UDS_SID_READ_DID + UDS_POSITIVE_RESP_SID_OFFSET), resp[0]);

    /* assert: lidar distance = 150 (little-endian: low=150, high=0) */
    TEST_ASSERT_EQUAL_UINT8(150u, resp[3]);
    TEST_ASSERT_EQUAL_UINT8(0u, resp[4]);
}

/* ==================================================================
 * SWR-FZC-030: Unsupported Service NRC (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-030 — Unsupported service returns NRC 0x11 */
void test_FzcDcm_unsupported_service_nrc(void)
{
    uint8 req[1];
    uint8 resp[64];
    uint8 respLen;
    Std_ReturnType ret;

    /* SID 0x27 (SecurityAccess) is NOT in the supported list */
    req[0] = 0x27u;

    ret = Swc_FzcDcm_ProcessRequest(req, 1u, resp, &respLen);

    /* assert: function returns OK (request was processed, just negatively) */
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* assert: negative response */
    TEST_ASSERT_EQUAL_UINT8(UDS_NEGATIVE_RESP_SID, resp[0]);

    /* assert: original SID echoed */
    TEST_ASSERT_EQUAL_UINT8(0x27u, resp[1]);

    /* assert: NRC = serviceNotSupported (0x11) */
    TEST_ASSERT_EQUAL_UINT8(FZC_UDS_NRC_SERVICE_NOT_SUPPORTED, resp[2]);

    /* assert: response length = 3 */
    TEST_ASSERT_EQUAL_UINT8(3u, respLen);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-FZC-030: UDS Service Support */
    RUN_TEST(test_FzcDcm_supported_services);
    RUN_TEST(test_FzcDcm_read_did_steering_angle);
    RUN_TEST(test_FzcDcm_read_did_lidar_distance);
    RUN_TEST(test_FzcDcm_unsupported_service_nrc);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_FZC_DCM_H
#define FZC_CFG_H
#define RTE_H

#include "../src/Swc_FzcDcm.c"
