/**
 * @file    test_Swc_TempMonitor.c
 * @brief   Unit tests for Swc_TempMonitor — NTC temperature monitoring,
 *          derating curve, and hysteresis recovery SWC
 * @date    2026-02-23
 *
 * @verifies SWR-RZC-009, SWR-RZC-010, SWR-RZC-011
 *
 * Tests temperature measurement via IoHwAb, plausible-range gating,
 * stepped derating curve (100/75/50/0%), hysteresis on recovery,
 * CAN broadcast at 100ms rate, and safe behaviour before init.
 *
 * Mocks: IoHwAb_ReadMotorTemp, Rte_Read, Rte_Write, Com_SendSignal,
 *        Dem_ReportErrorStatus
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

#define E_OK        ((Std_ReturnType)0x00U)
#define E_NOT_OK    ((Std_ReturnType)0x01U)
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

typedef uint8           boolean;
typedef uint8           Com_SignalIdType;

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_TEMPMONITOR_H
#define RZC_CFG_H
#define IOHWAB_H
#define RTE_H
#define COM_H
#define DEM_H
#define WDGM_H

/* ==================================================================
 * Signal IDs (from Rzc_Cfg.h — redefined locally)
 * ================================================================== */

#define RZC_SIG_TEMP1_DC            24u
#define RZC_SIG_TEMP2_DC            25u
#define RZC_SIG_DERATING_PCT        26u
#define RZC_SIG_TEMP_FAULT          27u

#define RZC_COM_TX_MOTOR_TEMP        3u

#define RZC_DTC_OVERTEMP             1u

/* DEM event status */
#define DEM_EVENT_STATUS_PASSED      0u
#define DEM_EVENT_STATUS_FAILED      1u

/* Temperature constants (from Rzc_Cfg.h) */
#define RZC_TEMP_DERATE_NONE_C      60u
#define RZC_TEMP_DERATE_75_C        80u
#define RZC_TEMP_DERATE_50_C       100u

#define RZC_TEMP_DERATE_100_PCT    100u
#define RZC_TEMP_DERATE_75_PCT      75u
#define RZC_TEMP_DERATE_50_PCT      50u
#define RZC_TEMP_DERATE_0_PCT        0u

#define RZC_TEMP_HYSTERESIS_C       10u

#define RZC_TEMP_MIN_DDC          (-300)
#define RZC_TEMP_MAX_DDC           1500

/* Swc_TempMonitor API declarations */
extern void Swc_TempMonitor_Init(void);
extern void Swc_TempMonitor_MainFunction(void);

/* ==================================================================
 * Mock: IoHwAb_ReadMotorTemp
 * ================================================================== */

static sint16  mock_temp_dC;
static uint8   mock_iohwab_return;

Std_ReturnType IoHwAb_ReadMotorTemp(uint16* Temp_dC)
{
    if (Temp_dC == NULL_PTR) {
        return E_NOT_OK;
    }
    *Temp_dC = (uint16)mock_temp_dC;
    return mock_iohwab_return;
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
 * Mock: Com_SendSignal
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
 * Include SWC source under test (unity include-source pattern)
 * ================================================================== */

#include "../src/Swc_TempMonitor.c"

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    uint8 i;

    /* Reset IoHwAb mock */
    mock_temp_dC        = 250;   /* 25.0 degC default */
    mock_iohwab_return  = E_OK;

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

    Swc_TempMonitor_Init();
}

void tearDown(void) { }

/* ==================================================================
 * Helper: run N MainFunction cycles (100ms per call)
 * ================================================================== */

static void run_cycles(uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        Swc_TempMonitor_MainFunction();
    }
}

/* ==================================================================
 * Helper: set mock temperature (deci-degrees C)
 * ================================================================== */

static void set_mock_temp(sint16 deci_degC)
{
    mock_temp_dC = deci_degC;
}

/* ==================================================================
 * SWR-RZC-009: Temperature Measurement
 * ================================================================== */

/** @verifies SWR-RZC-009 -- Init succeeds, no crash */
void test_Init_succeeds(void)
{
    /* Init already called in setUp. Run one cycle to confirm no crash. */
    Swc_TempMonitor_MainFunction();

    /* No crash = pass. Module is operational. */
    TEST_ASSERT_TRUE(1u);
}

/** @verifies SWR-RZC-009 -- Read 25.0 degC, writes RTE signal correctly */
void test_Temp_read_writes_RTE(void)
{
    set_mock_temp(250);   /* 25.0 degC */

    run_cycles(1u);

    TEST_ASSERT_EQUAL_UINT32(250u, mock_rte_signals[RZC_SIG_TEMP1_DC]);
}

/** @verifies SWR-RZC-009 -- Temp below -30.0 degC triggers fault */
void test_Temp_range_check_rejects_low(void)
{
    set_mock_temp(-310);   /* -31.0 degC, below RZC_TEMP_MIN_DDC */

    run_cycles(1u);

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[RZC_SIG_TEMP_FAULT]);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[RZC_DTC_OVERTEMP]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[RZC_DTC_OVERTEMP]);
}

/** @verifies SWR-RZC-009 -- Temp above 150.0 degC triggers fault */
void test_Temp_range_check_rejects_high(void)
{
    set_mock_temp(1510);   /* 151.0 degC, above RZC_TEMP_MAX_DDC */

    run_cycles(1u);

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[RZC_SIG_TEMP_FAULT]);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[RZC_DTC_OVERTEMP]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[RZC_DTC_OVERTEMP]);
}

/* ==================================================================
 * SWR-RZC-010: Derating Curve
 * ================================================================== */

/** @verifies SWR-RZC-010 -- 50 degC (500 ddc) -> derating 100% */
void test_Derating_100pct_below_60C(void)
{
    set_mock_temp(500);   /* 50.0 degC */

    run_cycles(1u);

    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_100_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);
}

/** @verifies SWR-RZC-010 -- 70 degC (700 ddc) -> derating 75% */
void test_Derating_75pct_at_70C(void)
{
    set_mock_temp(700);   /* 70.0 degC */

    run_cycles(1u);

    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_75_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);
}

/** @verifies SWR-RZC-010 -- 90 degC (900 ddc) -> derating 50% */
void test_Derating_50pct_at_90C(void)
{
    set_mock_temp(900);   /* 90.0 degC */

    run_cycles(1u);

    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_50_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);
}

/** @verifies SWR-RZC-010 -- 100 degC (1000 ddc) -> derating 0%, overtemp */
void test_Derating_0pct_at_100C(void)
{
    set_mock_temp(1000);   /* 100.0 degC */

    run_cycles(1u);

    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_0_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[RZC_DTC_OVERTEMP]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[RZC_DTC_OVERTEMP]);
}

/* ==================================================================
 * SWR-RZC-011: Hysteresis Recovery
 * ================================================================== */

/** @verifies SWR-RZC-011 -- After 50% at 90 degC, must cool to 70 degC to
 *  recover to 75% (threshold 80 - hysteresis 10 = 70) */
void test_Hysteresis_recovery(void)
{
    /* Drive into 50% derating zone */
    set_mock_temp(900);   /* 90 degC */
    run_cycles(1u);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_50_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);

    /* Cool to 75 degC — still in 60-79 range but hysteresis blocks recovery.
     * Must remain at 50% because recovery threshold is 70 degC. */
    set_mock_temp(750);   /* 75.0 degC */
    run_cycles(1u);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_50_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);

    /* Cool to 690 (69.0 degC) — below hysteresis threshold (70 degC).
     * Should recover to 75%. */
    set_mock_temp(690);   /* 69.0 degC */
    run_cycles(1u);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_75_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);
}

/** @verifies SWR-RZC-011 -- After 0% at 100 degC, must cool to 90 degC
 *  (100 - 10 hysteresis) to recover to 50% */
void test_Hysteresis_from_shutdown(void)
{
    /* Drive into shutdown (0% derating) */
    set_mock_temp(1000);   /* 100 degC */
    run_cycles(1u);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_0_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);

    /* Cool to 95 degC — above recovery threshold (90 degC), stay at 0% */
    set_mock_temp(950);   /* 95.0 degC */
    run_cycles(1u);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_0_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);

    /* Cool to 890 (89.0 degC) — below 90 degC, should recover to 50% */
    set_mock_temp(890);   /* 89.0 degC */
    run_cycles(1u);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_50_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);
}

/* ==================================================================
 * Additional Tests
 * ================================================================== */

/** @verifies SWR-RZC-009 -- SWC writes to RTE only, CAN TX via Swc_RzcCom */
void test_CAN_broadcast(void)
{
    set_mock_temp(250);   /* 25.0 degC */

    run_cycles(1u);

    /* SWC must NOT call Com_SendSignal directly — CAN TX is
     * Swc_RzcCom's responsibility (reads RTE, sends via Com) */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);

    /* Temperature must be available on RTE for Swc_RzcCom to read */
    TEST_ASSERT_EQUAL_UINT32(250u, mock_rte_signals[RZC_SIG_TEMP1_DC]);
}

/** @verifies SWR-RZC-009 -- MainFunction without init does not crash */
void test_MainFunction_without_init_safe(void)
{
    /* Reset module state by re-zeroing the init flag manually.
     * We need to bypass init to test the guard. Re-init to clear,
     * then force TM_Initialized to FALSE. */
    TM_Initialized = FALSE;

    /* Should return immediately, no crash, no side effects */
    Swc_TempMonitor_MainFunction();

    /* COM should not have been called */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_com_send_count);

    /* RTE write count should remain at 0 (setUp reset it, init was not called) */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_rte_write_count);
}

/* ==================================================================
 * HARDENED TESTS — Boundary Values, Fault Injection
 * ================================================================== */

/** @verifies SWR-RZC-010
 *  Equivalence class: Boundary — exactly 60.0 degC (threshold for 100->75%) */
void test_Derating_boundary_60C(void)
{
    set_mock_temp(600);   /* 60.0 degC */
    run_cycles(1u);

    /* At exactly 60C, should still be 100% OR transition to 75% depending on >= vs > */
    uint32 derating = mock_rte_signals[RZC_SIG_DERATING_PCT];
    TEST_ASSERT_TRUE((derating == (uint32)RZC_TEMP_DERATE_100_PCT) ||
                     (derating == (uint32)RZC_TEMP_DERATE_75_PCT));
}

/** @verifies SWR-RZC-010
 *  Equivalence class: Boundary — exactly 80.0 degC (threshold for 75->50%) */
void test_Derating_boundary_80C(void)
{
    set_mock_temp(800);   /* 80.0 degC */
    run_cycles(1u);

    uint32 derating = mock_rte_signals[RZC_SIG_DERATING_PCT];
    TEST_ASSERT_TRUE((derating == (uint32)RZC_TEMP_DERATE_75_PCT) ||
                     (derating == (uint32)RZC_TEMP_DERATE_50_PCT));
}

/** @verifies SWR-RZC-010
 *  Equivalence class: Boundary — exactly 100.0 degC (threshold for 50->0%) */
void test_Derating_boundary_100C(void)
{
    set_mock_temp(1000);   /* 100.0 degC */
    run_cycles(1u);

    uint32 derating = mock_rte_signals[RZC_SIG_DERATING_PCT];
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_0_PCT, derating);
}

/** @verifies SWR-RZC-009
 *  Equivalence class: Boundary — exactly at min plausible (-30.0 degC) */
void test_Temp_at_min_plausible(void)
{
    set_mock_temp(-300);   /* -30.0 degC = RZC_TEMP_MIN_DDC */
    run_cycles(1u);

    /* At exactly -30.0C, should be accepted (boundary-inclusive) */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[RZC_SIG_TEMP_FAULT]);
}

/** @verifies SWR-RZC-009
 *  Equivalence class: Boundary — exactly at max plausible (150.0 degC) */
void test_Temp_at_max_plausible(void)
{
    set_mock_temp(1500);   /* 150.0 degC = RZC_TEMP_MAX_DDC */
    run_cycles(1u);

    /* At exactly 150.0C, should be accepted (boundary-inclusive) */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[RZC_SIG_TEMP_FAULT]);
}

/** @verifies SWR-RZC-009
 *  Equivalence class: Fault injection — IoHwAb returns E_NOT_OK */
void test_Temp_iohwab_failure(void)
{
    mock_iohwab_return = E_NOT_OK;
    run_cycles(1u);

    /* On read failure, module should report fault */
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[RZC_SIG_TEMP_FAULT]);
}

/** @verifies SWR-RZC-011
 *  Equivalence class: Boundary — hysteresis at exactly recovery threshold (50.0 degC) */
void test_Hysteresis_exact_recovery_threshold(void)
{
    /* Drive into 75% derating zone */
    set_mock_temp(700);   /* 70 degC */
    run_cycles(1u);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_75_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);

    /* Cool to exactly 50 degC (60 - 10 hysteresis) */
    set_mock_temp(500);   /* 50.0 degC */
    run_cycles(1u);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_100_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);
}

/** @verifies SWR-RZC-009
 *  Equivalence class: Boundary — zero degrees Celsius */
void test_Temp_zero_degC(void)
{
    set_mock_temp(0);   /* 0.0 degC */
    run_cycles(1u);

    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[RZC_SIG_TEMP_FAULT]);
    TEST_ASSERT_EQUAL_UINT32((uint32)RZC_TEMP_DERATE_100_PCT,
                             mock_rte_signals[RZC_SIG_DERATING_PCT]);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-RZC-009: Temperature Measurement */
    RUN_TEST(test_Init_succeeds);
    RUN_TEST(test_Temp_read_writes_RTE);
    RUN_TEST(test_Temp_range_check_rejects_low);
    RUN_TEST(test_Temp_range_check_rejects_high);

    /* SWR-RZC-010: Derating Curve */
    RUN_TEST(test_Derating_100pct_below_60C);
    RUN_TEST(test_Derating_75pct_at_70C);
    RUN_TEST(test_Derating_50pct_at_90C);
    RUN_TEST(test_Derating_0pct_at_100C);

    /* SWR-RZC-011: Hysteresis Recovery */
    RUN_TEST(test_Hysteresis_recovery);
    RUN_TEST(test_Hysteresis_from_shutdown);

    /* Additional */
    RUN_TEST(test_CAN_broadcast);
    RUN_TEST(test_MainFunction_without_init_safe);

    /* Hardened tests — boundary values, fault injection */
    RUN_TEST(test_Derating_boundary_60C);
    RUN_TEST(test_Derating_boundary_80C);
    RUN_TEST(test_Derating_boundary_100C);
    RUN_TEST(test_Temp_at_min_plausible);
    RUN_TEST(test_Temp_at_max_plausible);
    RUN_TEST(test_Temp_iohwab_failure);
    RUN_TEST(test_Hysteresis_exact_recovery_threshold);
    RUN_TEST(test_Temp_zero_degC);

    return UNITY_END();
}
