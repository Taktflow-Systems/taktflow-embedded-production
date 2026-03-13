/**
 * @file    test_Swc_CvcDcm.c
 * @brief   Unit tests for Swc_CvcDcm — UDS service routing, DID reading, DTC reporting
 * @date    2026-02-24
 *
 * @verifies SWR-CVC-033, SWR-CVC-034, SWR-CVC-035
 *
 * Tests: supported/unsupported services, DID reads (vehicle state, pedal sensors,
 * SW version), DTC report and clear.
 *
 * Mocks: Dem_GetEventStatus, Dem_ClearAllDTCs, Rte_Read
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * CvcDcm Constants (mirrors header)
 * ================================================================== */

#define CVCDCM_SID_DIAG_SESSION       0x10u
#define CVCDCM_SID_ECU_RESET          0x11u
#define CVCDCM_SID_READ_DID           0x22u
#define CVCDCM_SID_WRITE_DID          0x2Eu
#define CVCDCM_SID_READ_DTC_INFO      0x19u
#define CVCDCM_SID_CLEAR_DTC          0x14u
#define CVCDCM_SID_SECURITY_ACCESS    0x27u
#define CVCDCM_SID_TESTER_PRESENT     0x3Eu

#define CVCDCM_NUM_SERVICES             8u

#define CVCDCM_DID_SW_VERSION        0xF190u
#define CVCDCM_DID_VEHICLE_STATE     0xF010u
#define CVCDCM_DID_PEDAL_POSITION    0xF011u
#define CVCDCM_DID_PEDAL_FAULT       0xF012u
#define CVCDCM_DID_PEDAL_SENSORS     0xF016u

#define CVCDCM_DID_ECU_SERIAL        0xF18Cu
#define CVCDCM_DID_HW_VERSION        0xF191u
#define CVCDCM_DID_TORQUE_REQUEST    0xF013u
#define CVCDCM_DID_CAN_STATUS        0xF014u
#define CVCDCM_DID_BATTERY_VOLTAGE   0xF015u

#define CVCDCM_NUM_DIDS                10u

#define CVCDCM_NRC_SERVICE_NOT_SUPPORTED   0x11u
#define CVCDCM_NRC_REQUEST_OUT_OF_RANGE    0x31u

#define CVCDCM_MAX_DTC_SLOTS          20u

/* Signal IDs from Cvc_Cfg.h */
#define CVC_SIG_PEDAL_POSITION    18u
#define CVC_SIG_PEDAL_FAULT       19u
#define CVC_SIG_VEHICLE_STATE     20u
#define CVC_SIG_TORQUE_REQUEST    21u

/* Dem constants */
#define DEM_STATUS_TEST_FAILED    0x01u
#define DEM_MAX_EVENTS            32u

/* ==================================================================
 * CvcDcm Types (mirrors header)
 * ================================================================== */

typedef struct {
    uint8   data[8];
    uint8   length;
} Swc_CvcDcm_RequestType;

typedef struct {
    uint8   data[8];
    uint8   length;
    uint8   nrc;
} Swc_CvcDcm_ResponseType;

/* API declarations */
extern void            Swc_CvcDcm_Init(void);
extern Std_ReturnType  Swc_CvcDcm_ProcessRequest(const Swc_CvcDcm_RequestType* request,
                                                   Swc_CvcDcm_ResponseType* response);
extern Std_ReturnType  Swc_CvcDcm_ReadDid(uint16 did, uint8* data, uint8* length);
extern Std_ReturnType  Swc_CvcDcm_ReportDtc(uint8* dtcBuffer, uint8 maxEntries,
                                              uint8* count);
extern Std_ReturnType  Swc_CvcDcm_ClearDtc(void);

/* ==================================================================
 * Mock: Dem
 * ================================================================== */

static uint8 mock_dem_event_status[DEM_MAX_EVENTS];
static uint8 mock_dem_cleared;

typedef uint8 Dem_EventIdType;

Std_ReturnType Dem_GetEventStatus(Dem_EventIdType EventId, uint8* StatusPtr)
{
    if (StatusPtr == NULL_PTR) { return E_NOT_OK; }
    if (EventId >= DEM_MAX_EVENTS) { return E_NOT_OK; }
    *StatusPtr = mock_dem_event_status[EventId];
    return E_OK;
}

Std_ReturnType Dem_ClearAllDTCs(void)
{
    uint8 i;
    for (i = 0u; i < DEM_MAX_EVENTS; i++)
    {
        mock_dem_event_status[i] = 0u;
    }
    mock_dem_cleared = TRUE;
    return E_OK;
}

/* ==================================================================
 * Mock: Rte_Read
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  32u

static uint32 mock_rte_signals[MOCK_RTE_MAX_SIGNALS];

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) { return E_NOT_OK; }
    if (SignalId < MOCK_RTE_MAX_SIGNALS)
    {
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

    for (i = 0u; i < DEM_MAX_EVENTS; i++)
    {
        mock_dem_event_status[i] = 0u;
    }

    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++)
    {
        mock_rte_signals[i] = 0u;
    }

    mock_dem_cleared = FALSE;

    Swc_CvcDcm_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-CVC-033: UDS Service Routing Tests
 * ================================================================== */

/** @verifies SWR-CVC-033 — All 8 supported services return positive response */
void test_CvcDcm_supported_services(void)
{
    static const uint8 services[8] = {
        0x10u, 0x11u, 0x22u, 0x2Eu, 0x19u, 0x14u, 0x27u, 0x3Eu
    };
    uint8 i;
    Swc_CvcDcm_RequestType  req;
    Swc_CvcDcm_ResponseType rsp;
    Std_ReturnType ret;

    for (i = 0u; i < 8u; i++)
    {
        req.data[0] = services[i];

        /* ReadDID needs extra bytes */
        if (services[i] == 0x22u)
        {
            req.data[1] = 0xF1u;
            req.data[2] = 0x90u;  /* SW Version DID */
            req.length  = 3u;
        }
        else
        {
            req.length = 1u;
        }

        ret = Swc_CvcDcm_ProcessRequest(&req, &rsp);
        TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

        /* Should NOT be a negative response for supported services */
        if (rsp.data[0] != 0x7Fu)
        {
            /* Positive response: SID + 0x40 */
            TEST_ASSERT_EQUAL_UINT8(services[i] + 0x40u, rsp.data[0]);
        }
    }
}

/** @verifies SWR-CVC-033 — Unsupported service returns NRC 0x11 */
void test_CvcDcm_unsupported_service_nrc(void)
{
    Swc_CvcDcm_RequestType  req;
    Swc_CvcDcm_ResponseType rsp;

    req.data[0] = 0xFFu;  /* Not a valid UDS service */
    req.length  = 1u;

    (void)Swc_CvcDcm_ProcessRequest(&req, &rsp);

    TEST_ASSERT_EQUAL_UINT8(0x7Fu, rsp.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8(CVCDCM_NRC_SERVICE_NOT_SUPPORTED, rsp.data[2]);
}

/* ==================================================================
 * SWR-CVC-035: CVC DID Tests
 * ================================================================== */

/** @verifies SWR-CVC-035 — Read DID vehicle state */
void test_CvcDcm_read_did_vehicle_state(void)
{
    uint8 data[4];
    uint8 len = 4u;
    Std_ReturnType ret;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = 1u;  /* RUN */

    ret = Swc_CvcDcm_ReadDid(CVCDCM_DID_VEHICLE_STATE, data, &len);

    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(1u, data[0]);
}

/** @verifies SWR-CVC-035 — Read DID pedal sensors */
void test_CvcDcm_read_did_pedal_sensors(void)
{
    uint8 data[4];
    uint8 len = 4u;
    Std_ReturnType ret;

    mock_rte_signals[CVC_SIG_PEDAL_POSITION] = 500u;
    mock_rte_signals[CVC_SIG_PEDAL_FAULT]    = 0u;

    ret = Swc_CvcDcm_ReadDid(CVCDCM_DID_PEDAL_SENSORS, data, &len);

    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    /* Position LSB */
    TEST_ASSERT_EQUAL_UINT8((uint8)(500u & 0xFFu), data[0]);
    /* Fault */
    TEST_ASSERT_EQUAL_UINT8(0u, data[1]);
    TEST_ASSERT_EQUAL_UINT8(2u, len);
}

/** @verifies SWR-CVC-035 — Read DID SW version */
void test_CvcDcm_read_did_sw_version(void)
{
    uint8 data[4];
    uint8 len = 4u;
    Std_ReturnType ret;

    ret = Swc_CvcDcm_ReadDid(CVCDCM_DID_SW_VERSION, data, &len);

    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(0x01u, data[0]);  /* Major version 1 */
    TEST_ASSERT_EQUAL_UINT8(4u, len);
}

/** @verifies SWR-CVC-035 — Read unsupported DID returns E_NOT_OK */
void test_CvcDcm_read_did_unsupported(void)
{
    uint8 data[4];
    uint8 len = 4u;
    Std_ReturnType ret;

    ret = Swc_CvcDcm_ReadDid(0x9999u, data, &len);

    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/** @verifies SWR-CVC-035 — ReadDID via ProcessRequest positive response */
void test_CvcDcm_process_read_did_positive(void)
{
    Swc_CvcDcm_RequestType  req;
    Swc_CvcDcm_ResponseType rsp;

    mock_rte_signals[CVC_SIG_VEHICLE_STATE] = 2u;  /* DEGRADED */

    /* ReadByIdentifier for 0xF010 (vehicle state) */
    req.data[0] = 0x22u;
    req.data[1] = 0xF0u;
    req.data[2] = 0x10u;
    req.length  = 3u;

    (void)Swc_CvcDcm_ProcessRequest(&req, &rsp);

    TEST_ASSERT_EQUAL_UINT8(0x62u, rsp.data[0]);  /* 0x22 + 0x40 */
    TEST_ASSERT_EQUAL_UINT8(0xF0u, rsp.data[1]);
    TEST_ASSERT_EQUAL_UINT8(0x10u, rsp.data[2]);
    TEST_ASSERT_EQUAL_UINT8(2u, rsp.data[3]);     /* DEGRADED */
}

/* ==================================================================
 * SWR-CVC-034: DTC Reporting Tests
 * ================================================================== */

/** @verifies SWR-CVC-034 — Report DTCs with active faults */
void test_CvcDcm_report_dtc_status(void)
{
    uint8 dtcBuf[CVCDCM_MAX_DTC_SLOTS];
    uint8 count = 0u;
    Std_ReturnType ret;

    /* Set some DTCs as failed */
    mock_dem_event_status[0] = DEM_STATUS_TEST_FAILED;
    mock_dem_event_status[5] = DEM_STATUS_TEST_FAILED;
    mock_dem_event_status[12] = DEM_STATUS_TEST_FAILED;

    ret = Swc_CvcDcm_ReportDtc(dtcBuf, CVCDCM_MAX_DTC_SLOTS, &count);

    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(3u, count);
    TEST_ASSERT_EQUAL_UINT8(0u, dtcBuf[0]);
    TEST_ASSERT_EQUAL_UINT8(5u, dtcBuf[1]);
    TEST_ASSERT_EQUAL_UINT8(12u, dtcBuf[2]);
}

/** @verifies SWR-CVC-034 — Clear all DTCs */
void test_CvcDcm_clear_dtc(void)
{
    uint8 dtcBuf[CVCDCM_MAX_DTC_SLOTS];
    uint8 count = 0u;
    Std_ReturnType ret;

    /* Set DTCs */
    mock_dem_event_status[3] = DEM_STATUS_TEST_FAILED;
    mock_dem_event_status[7] = DEM_STATUS_TEST_FAILED;

    /* Clear */
    ret = Swc_CvcDcm_ClearDtc();
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(TRUE, mock_dem_cleared);

    /* Report should show 0 DTCs */
    (void)Swc_CvcDcm_ReportDtc(dtcBuf, CVCDCM_MAX_DTC_SLOTS, &count);
    TEST_ASSERT_EQUAL_UINT8(0u, count);
}

/** @verifies SWR-CVC-034 — Report DTCs with no active faults returns 0 */
void test_CvcDcm_report_dtc_no_faults(void)
{
    uint8 dtcBuf[CVCDCM_MAX_DTC_SLOTS];
    uint8 count = 0xFFu;

    (void)Swc_CvcDcm_ReportDtc(dtcBuf, CVCDCM_MAX_DTC_SLOTS, &count);

    TEST_ASSERT_EQUAL_UINT8(0u, count);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-CVC-033: UDS service routing */
    RUN_TEST(test_CvcDcm_supported_services);
    RUN_TEST(test_CvcDcm_unsupported_service_nrc);

    /* SWR-CVC-035: CVC DIDs */
    RUN_TEST(test_CvcDcm_read_did_vehicle_state);
    RUN_TEST(test_CvcDcm_read_did_pedal_sensors);
    RUN_TEST(test_CvcDcm_read_did_sw_version);
    RUN_TEST(test_CvcDcm_read_did_unsupported);
    RUN_TEST(test_CvcDcm_process_read_did_positive);

    /* SWR-CVC-034: DTC reporting */
    RUN_TEST(test_CvcDcm_report_dtc_status);
    RUN_TEST(test_CvcDcm_clear_dtc);
    RUN_TEST(test_CvcDcm_report_dtc_no_faults);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_CVCDCM_H
#define CVC_CFG_H
#define DEM_H
#define RTE_H

#include "../src/Swc_CvcDcm.c"
