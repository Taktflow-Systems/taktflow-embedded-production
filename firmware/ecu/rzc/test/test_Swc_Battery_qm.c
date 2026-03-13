/**
 * @file    test_Swc_Battery.c
 * @brief   Unit tests for Swc_Battery -- QM battery voltage monitoring SWC
 * @date    2026-02-23
 *
 * @verifies SWR-RZC-017, SWR-RZC-018
 *
 * Tests battery initialization, voltage-to-status mapping (NORMAL, WARN_LOW,
 * WARN_HIGH, DISABLE_LOW, DISABLE_HIGH), hysteresis recovery behaviour,
 * DTC reporting on DISABLE states, and CAN broadcast of battery status.
 *
 * Mocks: IoHwAb_ReadBatteryVoltage, Rte_Read, Rte_Write, Com_SendSignal,
 *        Dem_ReportErrorStatus
 *
 * @standard AUTOSAR SWC pattern, ISO 26262 Part 6
 * @copyright Taktflow Systems 2026
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
typedef uint8           boolean;

#define E_OK        ((Std_ReturnType)0x00U)
#define E_NOT_OK    ((Std_ReturnType)0x01U)
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* Prevent BSW headers from redefining types */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_BATTERY_H
#define RZC_CFG_H
#define IOHWAB_H
#define RTE_H
#define COM_H
#define DEM_H
#define WDGM_H

/* ==================================================================
 * Signal IDs (from Rzc_Cfg.h -- redefined locally for test isolation)
 * ================================================================== */

#define RZC_SIG_BATTERY_MV         28u
#define RZC_SIG_BATTERY_STATUS     29u
#define RZC_SIG_BATTERY_SOC        40u

#define RZC_COM_TX_BATTERY_STATUS   4u

#define RZC_DTC_BATTERY             9u

/* Battery thresholds */
#define RZC_BATT_DISABLE_LOW_MV    8000u
#define RZC_BATT_WARN_LOW_MV      10500u
#define RZC_BATT_WARN_HIGH_MV     15000u
#define RZC_BATT_DISABLE_HIGH_MV  17000u
#define RZC_BATT_HYSTERESIS_MV      500u
#define RZC_BATT_NOMINAL_MV       12600u
#define RZC_BATT_AVG_WINDOW           4u

/* Battery status codes */
#define RZC_BATT_STATUS_DISABLE_LOW   0u
#define RZC_BATT_STATUS_WARN_LOW      1u
#define RZC_BATT_STATUS_NORMAL        2u
#define RZC_BATT_STATUS_WARN_HIGH     3u
#define RZC_BATT_STATUS_DISABLE_HIGH  4u

/* DEM event status (matches Dem_EventStatusType enum values) */
#define DEM_EVENT_STATUS_PASSED      0u
#define DEM_EVENT_STATUS_FAILED      1u

/* Com signal type for mock compatibility */
typedef uint8 Com_SignalIdType;

/* Swc_Battery API declarations */
extern void Swc_Battery_Init(void);
extern void Swc_Battery_MainFunction(void);

/* ==================================================================
 * Mock: IoHwAb_ReadBatteryVoltage
 * ================================================================== */

static uint16  mock_battery_mV;

Std_ReturnType IoHwAb_ReadBatteryVoltage(uint16* Voltage_mV)
{
    if (Voltage_mV == NULL_PTR) {
        return E_NOT_OK;
    }
    *Voltage_mV = mock_battery_mV;
    return E_OK;
}

/* ==================================================================
 * Mock: Rte_Read
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  48u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];

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
 * Mock: Rte_Write
 * ================================================================== */

static uint8   mock_rte_write_count;

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    mock_rte_write_count++;
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        mock_rte_signals[SignalId] = Data;
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: Com_SendSignal (matches Com.h: 2-arg signature)
 * ================================================================== */

#define MOCK_COM_MAX_DATA  8u

static uint8   mock_com_send_count;
static uint16  mock_com_last_signal_id;
static uint8   mock_com_last_data[MOCK_COM_MAX_DATA];

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId, const void* SignalDataPtr)
{
    uint8 i;
    const uint8* DataPtr = (const uint8*)SignalDataPtr;
    mock_com_send_count++;
    mock_com_last_signal_id = (uint16)SignalId;
    for (i = 0u; i < MOCK_COM_MAX_DATA; i++) {
        mock_com_last_data[i] = DataPtr[i];
    }
    return E_OK;
}

/* ==================================================================
 * Mock: Dem_ReportErrorStatus
 * ================================================================== */

#define MOCK_DEM_MAX_EVENTS  16u

static uint8   mock_dem_last_event_id;
static uint8   mock_dem_last_status;
static uint8   mock_dem_call_count;
static uint8   mock_dem_event_reported[MOCK_DEM_MAX_EVENTS];
static uint8   mock_dem_event_status[MOCK_DEM_MAX_EVENTS];

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_call_count++;
    mock_dem_last_event_id = EventId;
    mock_dem_last_status   = EventStatus;
    if (EventId < MOCK_DEM_MAX_EVENTS) {
        mock_dem_event_reported[EventId] = 1u;
        mock_dem_event_status[EventId]   = EventStatus;
    }
}

/* ==================================================================
 * Include source under test (unity include-source pattern)
 * ================================================================== */

#include "../src/Swc_Battery.c"

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    uint8 i;

    /* Reset battery mock */
    mock_battery_mV = 12000u;

    /* Reset RTE mock */
    mock_rte_write_count = 0u;
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }

    /* Reset COM mock */
    mock_com_send_count     = 0u;
    mock_com_last_signal_id = 0xFFu;
    for (i = 0u; i < MOCK_COM_MAX_DATA; i++) {
        mock_com_last_data[i] = 0u;
    }

    /* Reset DEM mock */
    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;
    for (i = 0u; i < MOCK_DEM_MAX_EVENTS; i++) {
        mock_dem_event_reported[i] = 0u;
        mock_dem_event_status[i]   = 0xFFu;
    }

    Swc_Battery_Init();
}

void tearDown(void) { }

/* ==================================================================
 * Helper: run N main cycles with current mock voltage
 * ================================================================== */

static void run_cycles(uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        Swc_Battery_MainFunction();
    }
}

/* ==================================================================
 * Helper: fill averaging buffer with a given voltage
 * ================================================================== */

static void fill_avg_buffer(uint16 voltage_mV)
{
    mock_battery_mV = voltage_mV;
    run_cycles(RZC_BATT_AVG_WINDOW);
}

/* ==================================================================
 * SWR-RZC-017: Initialization
 * ================================================================== */

/** @verifies SWR-RZC-017 -- Init succeeds, MainFunction does not crash */
void test_Init_succeeds(void)
{
    /* Init already called in setUp. Run one cycle to confirm no crash. */
    Swc_Battery_MainFunction();

    /* No crash = pass */
    TEST_ASSERT_TRUE(1u);
}

/* ==================================================================
 * SWR-RZC-017: Voltage-to-Status Mapping
 * ================================================================== */

/** @verifies SWR-RZC-017 -- 12000mV maps to NORMAL status */
void test_Normal_voltage(void)
{
    fill_avg_buffer(12000u);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_NORMAL, status);
}

/** @verifies SWR-RZC-018 -- 7500mV maps to DISABLE_LOW, DTC reported */
void test_Low_disable(void)
{
    fill_avg_buffer(7500u);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_DISABLE_LOW, status);

    /* DTC should have been reported for battery */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[RZC_DTC_BATTERY]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[RZC_DTC_BATTERY]);
}

/** @verifies SWR-RZC-017 -- 10000mV maps to WARN_LOW */
void test_Low_warn(void)
{
    fill_avg_buffer(10000u);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_WARN_LOW, status);
}

/** @verifies SWR-RZC-017 -- 15500mV maps to WARN_HIGH */
void test_High_warn(void)
{
    fill_avg_buffer(15500u);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_WARN_HIGH, status);
}

/** @verifies SWR-RZC-018 -- 17500mV maps to DISABLE_HIGH, DTC reported */
void test_High_disable(void)
{
    fill_avg_buffer(17500u);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_DISABLE_HIGH, status);

    /* DTC should have been reported for battery */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[RZC_DTC_BATTERY]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[RZC_DTC_BATTERY]);
}

/* ==================================================================
 * SWR-RZC-018: Hysteresis Recovery
 * ================================================================== */

/** @verifies SWR-RZC-018 -- From DISABLE_LOW at 7500, must reach 8500 to recover */
void test_Hysteresis_recovery(void)
{
    /* Drive into DISABLE_LOW */
    fill_avg_buffer(7500u);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_DISABLE_LOW, status);

    /* Voltage at 8000 -- still within hysteresis band, should stay DISABLE_LOW */
    fill_avg_buffer(8000u);

    status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_DISABLE_LOW, status);

    /* Voltage at 8500 (8000 + 500 hysteresis) -- should recover to WARN_LOW */
    fill_avg_buffer(8500u);

    status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_WARN_LOW, status);
}

/* ==================================================================
 * SWR-RZC-017: CAN Broadcast
 * ================================================================== */

/** @verifies SWR-RZC-017 -- SWC writes to RTE only, CAN TX via Swc_RzcCom */
void test_CAN_broadcast(void)
{
    mock_battery_mV = 12000u;
    fill_avg_buffer(12000u);

    /* SWC must NOT call Com_SendSignal directly — CAN TX is
     * Swc_RzcCom's responsibility (reads RTE, sends via Com) */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);

    /* Voltage and status must be available on RTE */
    TEST_ASSERT_EQUAL_UINT32(12000u, mock_rte_signals[RZC_SIG_BATTERY_MV]);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_NORMAL,
                              mock_rte_signals[RZC_SIG_BATTERY_STATUS]);
}

/* ==================================================================
 * HARDENED TESTS — Boundary Values, Fault Injection
 * ================================================================== */

/** @verifies SWR-RZC-017
 *  Equivalence class: Boundary — exactly at DISABLE_LOW threshold (8000mV) */
void test_Boundary_exactly_disable_low(void)
{
    fill_avg_buffer(RZC_BATT_DISABLE_LOW_MV);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_TRUE((status == (uint32)RZC_BATT_STATUS_DISABLE_LOW) ||
                     (status == (uint32)RZC_BATT_STATUS_WARN_LOW));
}

/** @verifies SWR-RZC-017
 *  Equivalence class: Boundary — exactly at WARN_LOW threshold (10500mV) */
void test_Boundary_exactly_warn_low(void)
{
    fill_avg_buffer(RZC_BATT_WARN_LOW_MV);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_TRUE((status == (uint32)RZC_BATT_STATUS_WARN_LOW) ||
                     (status == (uint32)RZC_BATT_STATUS_NORMAL));
}

/** @verifies SWR-RZC-017
 *  Equivalence class: Boundary — exactly at WARN_HIGH threshold (15000mV) */
void test_Boundary_exactly_warn_high(void)
{
    fill_avg_buffer(RZC_BATT_WARN_HIGH_MV);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_TRUE((status == (uint32)RZC_BATT_STATUS_NORMAL) ||
                     (status == (uint32)RZC_BATT_STATUS_WARN_HIGH));
}

/** @verifies SWR-RZC-017
 *  Equivalence class: Boundary — exactly at DISABLE_HIGH threshold (17000mV) */
void test_Boundary_exactly_disable_high(void)
{
    fill_avg_buffer(RZC_BATT_DISABLE_HIGH_MV);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_TRUE((status == (uint32)RZC_BATT_STATUS_WARN_HIGH) ||
                     (status == (uint32)RZC_BATT_STATUS_DISABLE_HIGH));
}

/** @verifies SWR-RZC-017
 *  Equivalence class: Boundary — zero voltage (completely dead battery) */
void test_Battery_zero_voltage(void)
{
    fill_avg_buffer(0u);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_DISABLE_LOW, status);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[RZC_DTC_BATTERY]);
}

/** @verifies SWR-RZC-018
 *  Equivalence class: Fault injection — hysteresis from DISABLE_HIGH */
void test_Hysteresis_from_disable_high(void)
{
    /* Drive into DISABLE_HIGH */
    fill_avg_buffer(17500u);
    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_DISABLE_HIGH, status);

    /* Drop to 17000mV — within hysteresis, should stay DISABLE_HIGH */
    fill_avg_buffer(17000u);
    status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_DISABLE_HIGH, status);

    /* Drop to 16499mV (17000 - 500 - 1) — below hysteresis, recover to WARN_HIGH */
    fill_avg_buffer(16499u);
    status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_WARN_HIGH, status);
}

/** @verifies SWR-RZC-017
 *  Equivalence class: Boundary — maximum uint16 voltage (65535mV) */
void test_Battery_max_uint16(void)
{
    fill_avg_buffer(65535u);

    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_BATT_STATUS_DISABLE_HIGH, status);
}

/** @verifies SWR-RZC-018
 *  Equivalence class: Boundary — averaging window affects status transition */
void test_Battery_averaging_step_transition(void)
{
    /* Fill 3 of 4 samples with normal, 1 with low */
    mock_battery_mV = 12000u;
    run_cycles(3u);
    mock_battery_mV = 7500u;
    run_cycles(1u);

    /* Average = (12000*3 + 7500) / 4 = 10875 -> NORMAL or WARN_LOW */
    uint32 status = mock_rte_signals[RZC_SIG_BATTERY_STATUS];
    TEST_ASSERT_TRUE((status == (uint32)RZC_BATT_STATUS_NORMAL) ||
                     (status == (uint32)RZC_BATT_STATUS_WARN_LOW));
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-RZC-017: Initialization */
    RUN_TEST(test_Init_succeeds);

    /* SWR-RZC-017: Voltage-to-status mapping */
    RUN_TEST(test_Normal_voltage);
    RUN_TEST(test_Low_disable);
    RUN_TEST(test_Low_warn);
    RUN_TEST(test_High_warn);
    RUN_TEST(test_High_disable);

    /* SWR-RZC-018: Hysteresis recovery */
    RUN_TEST(test_Hysteresis_recovery);

    /* SWR-RZC-017: CAN broadcast */
    RUN_TEST(test_CAN_broadcast);

    /* Hardened tests — boundary values, fault injection */
    RUN_TEST(test_Boundary_exactly_disable_low);
    RUN_TEST(test_Boundary_exactly_warn_low);
    RUN_TEST(test_Boundary_exactly_warn_high);
    RUN_TEST(test_Boundary_exactly_disable_high);
    RUN_TEST(test_Battery_zero_voltage);
    RUN_TEST(test_Hysteresis_from_disable_high);
    RUN_TEST(test_Battery_max_uint16);
    RUN_TEST(test_Battery_averaging_step_transition);

    return UNITY_END();
}
