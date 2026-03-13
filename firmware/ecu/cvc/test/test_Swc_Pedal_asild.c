/**
 * @file    test_Swc_Pedal.c
 * @brief   Unit tests for Swc_Pedal — dual pedal sensor processing SWC
 * @date    2026-02-21
 *
 * @verifies SWR-CVC-001, SWR-CVC-002, SWR-CVC-003, SWR-CVC-004,
 *           SWR-CVC-005, SWR-CVC-006, SWR-CVC-007, SWR-CVC-008
 *
 * Tests pedal initialization, dual-sensor reading, plausibility checking,
 * stuck detection, fault latching, torque mapping with ramp limiting,
 * and vehicle-state-dependent mode limits.
 *
 * Mocks: IoHwAb_ReadPedalAngle, Rte_Write, Rte_Read, Dem_ReportErrorStatus
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
 * Signal IDs (from Cvc_Cfg.h)
 * ================================================================== */

#define CVC_SIG_PEDAL_RAW_1       16u
#define CVC_SIG_PEDAL_RAW_2       17u
#define CVC_SIG_PEDAL_POSITION    18u
#define CVC_SIG_PEDAL_FAULT       19u
#define CVC_SIG_VEHICLE_STATE     20u
#define CVC_SIG_TORQUE_REQUEST    21u

/* Com Signal IDs used by Swc_Pedal.c (mirrors Cvc_Cfg.h) */
#define CVC_COM_SIG_TORQUE_REQUEST_TORQUE_REQUEST    21u

/* Vehicle states */
#define CVC_STATE_INIT             0u
#define CVC_STATE_RUN              1u
#define CVC_STATE_DEGRADED         2u
#define CVC_STATE_LIMP             3u
#define CVC_STATE_SAFE_STOP        4u
#define CVC_STATE_SHUTDOWN         5u

/* Pedal faults */
#define CVC_PEDAL_NO_FAULT         0u
#define CVC_PEDAL_PLAUSIBILITY     1u
#define CVC_PEDAL_STUCK            2u
#define CVC_PEDAL_SENSOR1_FAIL     3u
#define CVC_PEDAL_SENSOR2_FAIL     4u

/* DTC event IDs */
#define CVC_DTC_PEDAL_PLAUSIBILITY   0u
#define CVC_DTC_PEDAL_SENSOR1_FAIL   1u
#define CVC_DTC_PEDAL_SENSOR2_FAIL   2u
#define CVC_DTC_PEDAL_STUCK          3u

/* Pedal thresholds */
#define CVC_PEDAL_PLAUS_THRESHOLD     819u
#define CVC_PEDAL_PLAUS_DEBOUNCE        2u
#define CVC_PEDAL_STUCK_THRESHOLD      10u
#define CVC_PEDAL_STUCK_CYCLES        100u
#define CVC_PEDAL_LATCH_CLEAR_CYCLES   50u
#define CVC_PEDAL_RAMP_LIMIT            5u
#define CVC_PEDAL_MAX_RUN            1000u   /* 100% torque in RUN mode */
#define CVC_PEDAL_MAX_DEGRADED        750u   /* 75% torque in DEGRADED */
#define CVC_PEDAL_MAX_LIMP            300u   /* 30% torque in LIMP */

/* DEM event status */
#define DEM_EVENT_STATUS_PASSED    0u
#define DEM_EVENT_STATUS_FAILED    1u

/* ==================================================================
 * Swc_Pedal Config Type (mirrors header)
 * ================================================================== */

typedef struct {
    uint16  plausThreshold;     /**< |S1-S2| threshold for plausibility     */
    uint8   plausDebounce;      /**< Cycles before plausibility fault       */
    uint16  stuckThreshold;     /**< Delta < this = stuck candidate         */
    uint16  stuckCycles;        /**< Consecutive stuck cycles before fault  */
    uint8   latchClearCycles;   /**< Fault-free cycles to clear latch       */
    uint16  rampLimit;          /**< Max torque increase per cycle           */
} Swc_Pedal_ConfigType;

/* Swc_Pedal API declarations */
extern void            Swc_Pedal_Init(const Swc_Pedal_ConfigType* ConfigPtr);
extern void            Swc_Pedal_MainFunction(void);
extern Std_ReturnType  Swc_Pedal_GetPosition(uint8* pos);

/* ==================================================================
 * Mock: IoHwAb_ReadPedalAngle
 * ================================================================== */

static Std_ReturnType  mock_iohwab_result_s0;
static Std_ReturnType  mock_iohwab_result_s1;
static uint16          mock_iohwab_angle_s0;
static uint16          mock_iohwab_angle_s1;
static uint8           mock_iohwab_read_count;

Std_ReturnType IoHwAb_ReadPedalAngle(uint8 SensorId, uint16* Angle)
{
    mock_iohwab_read_count++;
    if (Angle == NULL_PTR) {
        return E_NOT_OK;
    }
    if (SensorId == 0u) {
        *Angle = mock_iohwab_angle_s0;
        return mock_iohwab_result_s0;
    }
    if (SensorId == 1u) {
        *Angle = mock_iohwab_angle_s1;
        return mock_iohwab_result_s1;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: Rte_Write
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  32u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];
static uint8   mock_rte_write_count;
static uint16  mock_rte_last_write_id;

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    mock_rte_write_count++;
    mock_rte_last_write_id = SignalId;
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        mock_rte_signals[SignalId] = Data;
        return E_OK;
    }
    return E_NOT_OK;
}

/* ==================================================================
 * Mock: Rte_Read
 * ================================================================== */

static uint32  mock_vehicle_state;

Std_ReturnType Rte_Read(uint16 SignalId, uint32* DataPtr)
{
    if (DataPtr == NULL_PTR) {
        return E_NOT_OK;
    }
    if (SignalId == CVC_SIG_VEHICLE_STATE) {
        *DataPtr = mock_vehicle_state;
        return E_OK;
    }
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        *DataPtr = mock_rte_signals[SignalId];
        return E_OK;
    }
    return E_NOT_OK;
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
 * Mock: Com_SendSignal — variable declarations (function at bottom)
 * ================================================================== */

static uint16  mock_com_last_signal_id;
static uint8   mock_com_send_count;

/* ==================================================================
 * Test Configuration
 * ================================================================== */

static Swc_Pedal_ConfigType test_config;

void setUp(void)
{
    uint8 i;

    /* Reset IoHwAb mock */
    mock_iohwab_result_s0 = E_OK;
    mock_iohwab_result_s1 = E_OK;
    mock_iohwab_angle_s0  = 0u;
    mock_iohwab_angle_s1  = 0u;
    mock_iohwab_read_count = 0u;

    /* Reset RTE mock */
    mock_rte_write_count   = 0u;
    mock_rte_last_write_id = 0u;
    mock_vehicle_state     = CVC_STATE_RUN;
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }

    /* Reset Com mock */
    mock_com_last_signal_id = 0u;
    mock_com_send_count     = 0u;

    /* Reset DEM mock */
    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;
    for (i = 0u; i < MOCK_DEM_MAX_EVENTS; i++) {
        mock_dem_event_reported[i] = 0u;
        mock_dem_event_status[i]   = 0xFFu;
    }

    /* Default config matching Cvc_Cfg.h thresholds */
    test_config.plausThreshold   = CVC_PEDAL_PLAUS_THRESHOLD;
    test_config.plausDebounce    = CVC_PEDAL_PLAUS_DEBOUNCE;
    test_config.stuckThreshold   = CVC_PEDAL_STUCK_THRESHOLD;
    test_config.stuckCycles      = CVC_PEDAL_STUCK_CYCLES;
    test_config.latchClearCycles = CVC_PEDAL_LATCH_CLEAR_CYCLES;
    test_config.rampLimit        = CVC_PEDAL_RAMP_LIMIT;

    Swc_Pedal_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * Helper: run N main cycles with given sensor values
 * ================================================================== */

static void run_cycles(uint16 s0, uint16 s1, uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        mock_iohwab_angle_s0 = s0;
        mock_iohwab_angle_s1 = s1;
        Swc_Pedal_MainFunction();
    }
}

/* ==================================================================
 * SWR-CVC-001: Initialization
 * ================================================================== */

/** @verifies SWR-CVC-001 — Init with valid config succeeds */
void test_Init_valid_config(void)
{
    /* Init already called in setUp. Verify module is operational
     * by running one cycle with valid sensor data. */
    mock_iohwab_angle_s0 = 8192u;
    mock_iohwab_angle_s1 = 8192u;
    Swc_Pedal_MainFunction();

    /* Position should be written via RTE */
    TEST_ASSERT_TRUE(mock_rte_write_count > 0u);
}

/* ==================================================================
 * SWR-CVC-002: Normal Sensor Reading
 * ================================================================== */

/** @verifies SWR-CVC-002 — Normal read: both sensors valid, position = (S1+S2)/2 scaled */
void test_Normal_read_position_average(void)
{
    /* Mid-range: 8192 / 16383 * 1000 = ~500 */
    mock_iohwab_angle_s0 = 8192u;
    mock_iohwab_angle_s1 = 8192u;
    mock_vehicle_state   = CVC_STATE_RUN;

    /* Run enough cycles for ramp to reach steady state */
    run_cycles(8192u, 8192u, 200u);

    uint32 position = mock_rte_signals[CVC_SIG_PEDAL_POSITION];
    /* (8192+8192)/2 = 8192 -> 8192*1000/16383 = ~499 or ~500 */
    TEST_ASSERT_TRUE(position >= 490u);
    TEST_ASSERT_TRUE(position <= 510u);
}

/** @verifies SWR-CVC-002 — Both sensors at 0 gives position = 0 */
void test_Both_sensors_zero_position_zero(void)
{
    run_cycles(0u, 0u, 10u);

    uint32 position = mock_rte_signals[CVC_SIG_PEDAL_POSITION];
    TEST_ASSERT_EQUAL_UINT32(0u, position);
}

/** @verifies SWR-CVC-002 — Both sensors at max (16383) gives position = 1000 */
void test_Both_sensors_max_position_1000(void)
{
    /* Must ramp up over many cycles due to ramp limit of 5/cycle */
    run_cycles(16383u, 16383u, 250u);

    uint32 position = mock_rte_signals[CVC_SIG_PEDAL_POSITION];
    TEST_ASSERT_EQUAL_UINT32(1000u, position);
}

/* ==================================================================
 * SWR-CVC-003: Plausibility Check
 * ================================================================== */

/** @verifies SWR-CVC-003 — Plausibility fail: |S1-S2| >= 819, debounce 2 cycles */
void test_Plausibility_fail_debounce_2_cycles(void)
{
    /* First cycle with big delta: no fault yet (debounce = 1) */
    mock_iohwab_angle_s0 = 0u;
    mock_iohwab_angle_s1 = 10000u;
    Swc_Pedal_MainFunction();

    uint32 fault1 = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_NO_FAULT, fault1);

    /* Second cycle: debounce counter reaches threshold -> fault */
    Swc_Pedal_MainFunction();

    uint32 fault2 = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_PLAUSIBILITY, fault2);
}

/** @verifies SWR-CVC-003 — Plausibility pass: delta back under threshold resets debounce */
void test_Plausibility_pass_resets_debounce(void)
{
    /* 1 cycle with big delta */
    mock_iohwab_angle_s0 = 0u;
    mock_iohwab_angle_s1 = 10000u;
    Swc_Pedal_MainFunction();

    /* Back to normal before debounce completes */
    mock_iohwab_angle_s0 = 8192u;
    mock_iohwab_angle_s1 = 8192u;
    Swc_Pedal_MainFunction();

    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_NO_FAULT, fault);

    /* Another big delta: should restart debounce from 0, not carry over */
    mock_iohwab_angle_s0 = 0u;
    mock_iohwab_angle_s1 = 10000u;
    Swc_Pedal_MainFunction();

    fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_NO_FAULT, fault);
}

/** @verifies SWR-CVC-003 — Sensor 1 at 0, sensor 2 at max triggers plausibility fault */
void test_Plausibility_s1_zero_s2_max(void)
{
    /* Delta = 16383 >> 819 threshold */
    run_cycles(0u, 16383u, 3u);

    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_PLAUSIBILITY, fault);
}

/** @verifies SWR-CVC-003 — DTC reported on plausibility fault */
void test_Plausibility_reports_DTC(void)
{
    run_cycles(0u, 10000u, 3u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[CVC_DTC_PEDAL_PLAUSIBILITY]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[CVC_DTC_PEDAL_PLAUSIBILITY]);
}

/* ==================================================================
 * SWR-CVC-004: Stuck Detection
 * ================================================================== */

/** @verifies SWR-CVC-004 — Stuck detection: same value for 100 consecutive cycles */
void test_Stuck_detection_100_cycles(void)
{
    /* Same value (delta < 10) for 100 cycles */
    run_cycles(5000u, 5000u, 99u);

    uint32 fault99 = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_NO_FAULT, fault99);

    /* Cycle 100: stuck fault triggers */
    run_cycles(5000u, 5000u, 1u);

    uint32 fault100 = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_STUCK, fault100);
}

/** @verifies SWR-CVC-004 — DTC reported on stuck fault */
void test_Stuck_reports_DTC(void)
{
    run_cycles(5000u, 5000u, 101u);

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[CVC_DTC_PEDAL_STUCK]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[CVC_DTC_PEDAL_STUCK]);
}

/* ==================================================================
 * SWR-CVC-005: Sensor SPI Failure
 * ================================================================== */

/** @verifies SWR-CVC-005 — Sensor 1 SPI failure sets SENSOR1_FAIL */
void test_Sensor1_spi_failure(void)
{
    mock_iohwab_result_s0 = E_NOT_OK;
    mock_iohwab_angle_s1  = 5000u;
    Swc_Pedal_MainFunction();

    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_SENSOR1_FAIL, fault);
}

/** @verifies SWR-CVC-005 — Sensor 2 SPI failure sets SENSOR2_FAIL */
void test_Sensor2_spi_failure(void)
{
    mock_iohwab_angle_s0  = 5000u;
    mock_iohwab_result_s1 = E_NOT_OK;
    Swc_Pedal_MainFunction();

    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_SENSOR2_FAIL, fault);
}

/** @verifies SWR-CVC-005 — DTC reported on sensor 1 fail */
void test_Sensor1_fail_reports_DTC(void)
{
    mock_iohwab_result_s0 = E_NOT_OK;
    mock_iohwab_angle_s1  = 5000u;
    Swc_Pedal_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[CVC_DTC_PEDAL_SENSOR1_FAIL]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[CVC_DTC_PEDAL_SENSOR1_FAIL]);
}

/** @verifies SWR-CVC-005 — DTC reported on sensor 2 fail */
void test_Sensor2_fail_reports_DTC(void)
{
    mock_iohwab_angle_s0  = 5000u;
    mock_iohwab_result_s1 = E_NOT_OK;
    Swc_Pedal_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[CVC_DTC_PEDAL_SENSOR2_FAIL]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[CVC_DTC_PEDAL_SENSOR2_FAIL]);
}

/* ==================================================================
 * SWR-CVC-006: Zero-Torque Latch
 * ================================================================== */

/** @verifies SWR-CVC-006 — On any fault, torque output = 0 */
void test_Fault_forces_zero_torque(void)
{
    /* Build up some torque first */
    run_cycles(8192u, 8192u, 100u);
    uint32 torque_before = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_TRUE(torque_before > 0u);

    /* Now trigger a fault */
    mock_iohwab_result_s0 = E_NOT_OK;
    Swc_Pedal_MainFunction();

    uint32 torque_after = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque_after);
}

/** @verifies SWR-CVC-006 — Latch clear: 50 fault-free cycles required */
void test_Latch_clear_requires_50_cycles(void)
{
    /* Trigger sensor 1 fault */
    mock_iohwab_result_s0 = E_NOT_OK;
    Swc_Pedal_MainFunction();

    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_SENSOR1_FAIL, fault);

    /* Restore sensor and run 49 fault-free cycles — still latched */
    mock_iohwab_result_s0 = E_OK;
    mock_iohwab_angle_s0  = 5000u;
    mock_iohwab_angle_s1  = 5000u;
    run_cycles(5000u, 5000u, 49u);

    uint32 torque_49 = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque_49);

    /* 50th fault-free cycle: latch clears, torque may start ramping */
    run_cycles(5000u, 5000u, 1u);

    uint32 fault_cleared = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_NO_FAULT, fault_cleared);
}

/** @verifies SWR-CVC-006 — DTC cleared when fault-free */
void test_Fault_clears_DTC_when_fault_free(void)
{
    /* Trigger and confirm fault */
    mock_iohwab_result_s0 = E_NOT_OK;
    Swc_Pedal_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[CVC_DTC_PEDAL_SENSOR1_FAIL]);

    /* Restore sensor, run enough cycles to clear latch */
    mock_iohwab_result_s0 = E_OK;
    mock_iohwab_angle_s0  = 5000u;
    mock_iohwab_angle_s1  = 5000u;
    run_cycles(5000u, 5000u, 51u);

    /* DTC should now report PASSED */
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_PASSED,
                            mock_dem_event_status[CVC_DTC_PEDAL_SENSOR1_FAIL]);
}

/* ==================================================================
 * SWR-CVC-007: Torque Mapping and Ramp Limit
 * ================================================================== */

/** @verifies SWR-CVC-007 — Torque mapping: known input gives expected output */
void test_Torque_mapping_mid_range(void)
{
    /* Run at mid-range for enough cycles to reach steady state */
    run_cycles(8192u, 8192u, 250u);

    uint32 torque = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    /* At ~500 position, torque should be around 500 (linear mapping) */
    TEST_ASSERT_TRUE(torque >= 450u);
    TEST_ASSERT_TRUE(torque <= 550u);
}

/** @verifies SWR-CVC-007 — Ramp limit: torque increase capped at 5 units/cycle */
void test_Ramp_limit_caps_torque_increase(void)
{
    /* First cycle from 0: torque should increase by at most ramp limit */
    mock_iohwab_angle_s0 = 16383u;
    mock_iohwab_angle_s1 = 16383u;
    mock_vehicle_state   = CVC_STATE_RUN;
    Swc_Pedal_MainFunction();

    uint32 torque_1 = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_TRUE(torque_1 <= CVC_PEDAL_RAMP_LIMIT);

    /* Second cycle: should increase by at most ramp limit again */
    Swc_Pedal_MainFunction();

    uint32 torque_2 = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_TRUE(torque_2 <= (2u * CVC_PEDAL_RAMP_LIMIT));
    TEST_ASSERT_TRUE((torque_2 - torque_1) <= CVC_PEDAL_RAMP_LIMIT);
}

/* ==================================================================
 * SWR-CVC-008: Mode Limits
 * ================================================================== */

/** @verifies SWR-CVC-008 — Mode limit RUN: max 1000 (100%) */
void test_Mode_limit_run_max_1000(void)
{
    mock_vehicle_state = CVC_STATE_RUN;
    run_cycles(16383u, 16383u, 250u);

    uint32 torque = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_TRUE(torque <= 1000u);
    /* Should reach close to 1000 at full pedal in RUN mode */
    TEST_ASSERT_TRUE(torque >= 950u);
}

/** @verifies SWR-CVC-008 — Mode limit DEGRADED: max 750 (75%) */
void test_Mode_limit_degraded_max_750(void)
{
    mock_vehicle_state = CVC_STATE_DEGRADED;
    run_cycles(16383u, 16383u, 250u);

    uint32 torque = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_TRUE(torque <= 750u);
    /* Should reach close to 750 */
    TEST_ASSERT_TRUE(torque >= 700u);
}

/** @verifies SWR-CVC-008 — Mode limit LIMP: max 300 (30%) */
void test_Mode_limit_limp_max_300(void)
{
    mock_vehicle_state = CVC_STATE_LIMP;
    run_cycles(16383u, 16383u, 250u);

    uint32 torque = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_TRUE(torque <= 300u);
    /* Should reach close to 300 */
    TEST_ASSERT_TRUE(torque >= 250u);
}

/** @verifies SWR-CVC-008 — Torque zero in SAFE_STOP state */
void test_Torque_zero_in_safe_stop(void)
{
    /* Build up torque first */
    mock_vehicle_state = CVC_STATE_RUN;
    run_cycles(8192u, 8192u, 100u);

    /* Switch to SAFE_STOP */
    mock_vehicle_state = CVC_STATE_SAFE_STOP;
    Swc_Pedal_MainFunction();

    uint32 torque = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque);
}

/* ==================================================================
 * SWR-CVC-002: RTE Signal Writes
 * ================================================================== */

/** @verifies SWR-CVC-002 — Pedal position signal written each cycle */
void test_RTE_write_pedal_position_each_cycle(void)
{
    mock_iohwab_angle_s0 = 4000u;
    mock_iohwab_angle_s1 = 4000u;

    mock_rte_write_count = 0u;
    Swc_Pedal_MainFunction();

    /* Should have written at least: position, fault, torque signals */
    TEST_ASSERT_TRUE(mock_rte_write_count >= 2u);

    /* Verify position signal was written */
    uint32 position = mock_rte_signals[CVC_SIG_PEDAL_POSITION];
    TEST_ASSERT_TRUE(position > 0u);
}

/** @verifies SWR-CVC-002 — Pedal fault signal written each cycle */
void test_RTE_write_pedal_fault_each_cycle(void)
{
    mock_iohwab_angle_s0 = 4000u;
    mock_iohwab_angle_s1 = 4000u;

    Swc_Pedal_MainFunction();

    /* Fault signal should be NO_FAULT when sensors are OK */
    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_NO_FAULT, fault);
}

/* ==================================================================
 * HARDENED TESTS — Boundary Value, NULL Pointer, Fault Injection
 * ================================================================== */

/* ------------------------------------------------------------------
 * SWR-CVC-001: Initialization — NULL pointer and defensive checks
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-001
 *  Equivalence class: INVALID — NULL config pointer
 *  Boundary: NULL_PTR (invalid partition) */
void test_Init_null_config_rejects(void)
{
    /* Re-init with NULL — module should enter failed state */
    Swc_Pedal_Init(NULL_PTR);

    /* MainFunction should be a no-op (no RTE writes) */
    mock_rte_write_count = 0u;
    mock_iohwab_angle_s0 = 8192u;
    mock_iohwab_angle_s1 = 8192u;
    Swc_Pedal_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_rte_write_count);
}

/** @verifies SWR-CVC-001
 *  Equivalence class: INVALID — GetPosition before init
 *  Boundary: uninitialized module */
void test_GetPosition_null_pointer_rejects(void)
{
    Std_ReturnType ret;

    ret = Swc_Pedal_GetPosition(NULL_PTR);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
}

/** @verifies SWR-CVC-001
 *  Equivalence class: VALID — GetPosition returns scaled percentage */
void test_GetPosition_returns_scaled_percentage(void)
{
    uint8 pos = 0xFFu;
    Std_ReturnType ret;

    /* Run at mid-range to build up position */
    run_cycles(8192u, 8192u, 200u);

    ret = Swc_Pedal_GetPosition(&pos);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    /* 8192/16383 * 1000 / 10 ~ 49-50 percent */
    TEST_ASSERT_TRUE(pos >= 45u);
    TEST_ASSERT_TRUE(pos <= 55u);
}

/* ------------------------------------------------------------------
 * SWR-CVC-003: Plausibility — boundary value analysis
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-003
 *  Equivalence class: VALID — delta exactly at threshold (819), boundary
 *  Boundary: plausThreshold = 819, test with delta = 819 (at boundary) */
void test_Plausibility_boundary_exact_threshold(void)
{
    /* delta = 819 exactly — should trigger debounce increment */
    mock_iohwab_angle_s0 = 0u;
    mock_iohwab_angle_s1 = 819u;
    Swc_Pedal_MainFunction();

    /* First cycle: debounce = 1, no fault yet */
    uint32 fault1 = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_NO_FAULT, fault1);

    /* Second cycle: debounce = 2, fault triggers */
    Swc_Pedal_MainFunction();
    uint32 fault2 = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_PLAUSIBILITY, fault2);
}

/** @verifies SWR-CVC-003
 *  Equivalence class: VALID — delta one below threshold (818), no fault
 *  Boundary: plausThreshold - 1 = 818 */
void test_Plausibility_boundary_one_below_threshold(void)
{
    /* delta = 818 — just under threshold, should NOT fault */
    run_cycles(0u, 818u, 10u);

    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_NO_FAULT, fault);
}

/* ------------------------------------------------------------------
 * SWR-CVC-005: Fault injection — ADC stuck at 0x0000 / 0xFFFF
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-005
 *  Equivalence class: FAULT — sensor 1 returns saturated 0xFFFF (invalid for 14-bit)
 *  Fault injection: ADC returns max 16-bit value */
void test_Fault_injection_sensor1_adc_0xFFFF(void)
{
    /* 0xFFFF exceeds 14-bit max but IoHwAb returns E_OK — module should
     * still detect via plausibility if S2 differs significantly,
     * or clamp position correctly.
     * With S2 at 0, delta = 0xFFFF >> 819, fault after debounce. */
    mock_iohwab_angle_s0 = 0xFFFFu;
    mock_iohwab_angle_s1 = 0u;
    Swc_Pedal_MainFunction();
    Swc_Pedal_MainFunction();

    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_PLAUSIBILITY, fault);
}

/** @verifies SWR-CVC-005
 *  Equivalence class: FAULT — both sensors return 0x0000
 *  Fault injection: ADC returns stuck-at 0x0000 */
void test_Fault_injection_both_sensors_adc_0x0000(void)
{
    /* Both at 0x0000 — stuck detection should fire after 100 cycles */
    run_cycles(0u, 0u, 101u);

    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_STUCK, fault);
}

/** @verifies SWR-CVC-005
 *  Equivalence class: FAULT — both sensors fail simultaneously
 *  Fault injection: dual SPI failure */
void test_Fault_injection_both_sensors_spi_fail(void)
{
    mock_iohwab_result_s0 = E_NOT_OK;
    mock_iohwab_result_s1 = E_NOT_OK;
    Swc_Pedal_MainFunction();

    /* Sensor 1 checked first — should report SENSOR1_FAIL */
    uint32 fault = mock_rte_signals[CVC_SIG_PEDAL_FAULT];
    TEST_ASSERT_EQUAL_UINT32(CVC_PEDAL_SENSOR1_FAIL, fault);

    /* Torque must be zero */
    uint32 torque = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque);
}

/* ------------------------------------------------------------------
 * SWR-CVC-006: Latch — fault re-trigger during latch clear window
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-006
 *  Equivalence class: INVALID — fault during latch clear resets counter
 *  Fault injection: intermittent fault resets latch counter */
void test_Latch_reset_on_fault_during_clear_window(void)
{
    /* Trigger initial fault */
    mock_iohwab_result_s0 = E_NOT_OK;
    Swc_Pedal_MainFunction();

    /* Restore sensor, run 40 fault-free cycles (< 50 threshold) */
    mock_iohwab_result_s0 = E_OK;
    mock_iohwab_angle_s0  = 5000u;
    mock_iohwab_angle_s1  = 5000u;
    run_cycles(5000u, 5000u, 40u);

    /* Torque should still be 0 (latch active) */
    uint32 torque_40 = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque_40);

    /* Re-trigger fault — latch counter resets */
    mock_iohwab_result_s0 = E_NOT_OK;
    Swc_Pedal_MainFunction();

    /* Restore and run another 49 cycles — should still be latched */
    mock_iohwab_result_s0 = E_OK;
    run_cycles(5000u, 5000u, 49u);

    uint32 torque_after = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque_after);
}

/* ------------------------------------------------------------------
 * SWR-CVC-007: Torque ramp — boundary: ramp allows immediate decrease
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-007
 *  Equivalence class: VALID — torque decrease is immediate (no ramp limit)
 *  Boundary: safety rule — decreasing torque shall not be limited */
void test_Ramp_immediate_decrease_allowed(void)
{
    /* Build up torque */
    mock_vehicle_state = CVC_STATE_RUN;
    run_cycles(16383u, 16383u, 250u);

    uint32 torque_high = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_TRUE(torque_high >= 950u);

    /* Suddenly release pedal to 0 — torque should drop immediately */
    run_cycles(0u, 0u, 1u);

    uint32 torque_low = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque_low);
}

/* ------------------------------------------------------------------
 * SWR-CVC-008: Mode limit — SHUTDOWN and INIT force zero torque
 * ------------------------------------------------------------------ */

/** @verifies SWR-CVC-008
 *  Equivalence class: VALID — SHUTDOWN state forces zero torque
 *  Boundary: mode_limit = 0 for SHUTDOWN */
void test_Torque_zero_in_shutdown(void)
{
    mock_vehicle_state = CVC_STATE_SHUTDOWN;
    run_cycles(16383u, 16383u, 100u);

    uint32 torque = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque);
}

/** @verifies SWR-CVC-008
 *  Equivalence class: VALID — INIT state forces zero torque
 *  Boundary: mode_limit = 0 for INIT */
void test_Torque_zero_in_init(void)
{
    mock_vehicle_state = CVC_STATE_INIT;
    run_cycles(16383u, 16383u, 100u);

    uint32 torque = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque);
}

/** @verifies SWR-CVC-008
 *  Equivalence class: INVALID — unknown vehicle state defaults to zero torque
 *  Boundary: state value outside enum range */
void test_Mode_limit_unknown_state_defaults_zero(void)
{
    mock_vehicle_state = 0xFFu; /* Invalid state */
    run_cycles(16383u, 16383u, 100u);

    uint32 torque = mock_rte_signals[CVC_SIG_TORQUE_REQUEST];
    TEST_ASSERT_EQUAL_UINT32(0u, torque);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-CVC-001: Initialization */
    RUN_TEST(test_Init_valid_config);

    /* SWR-CVC-002: Normal sensor reading */
    RUN_TEST(test_Normal_read_position_average);
    RUN_TEST(test_Both_sensors_zero_position_zero);
    RUN_TEST(test_Both_sensors_max_position_1000);
    RUN_TEST(test_RTE_write_pedal_position_each_cycle);
    RUN_TEST(test_RTE_write_pedal_fault_each_cycle);

    /* SWR-CVC-003: Plausibility check */
    RUN_TEST(test_Plausibility_fail_debounce_2_cycles);
    RUN_TEST(test_Plausibility_pass_resets_debounce);
    RUN_TEST(test_Plausibility_s1_zero_s2_max);
    RUN_TEST(test_Plausibility_reports_DTC);

    /* SWR-CVC-004: Stuck detection */
    RUN_TEST(test_Stuck_detection_100_cycles);
    RUN_TEST(test_Stuck_reports_DTC);

    /* SWR-CVC-005: Sensor SPI failure */
    RUN_TEST(test_Sensor1_spi_failure);
    RUN_TEST(test_Sensor2_spi_failure);
    RUN_TEST(test_Sensor1_fail_reports_DTC);
    RUN_TEST(test_Sensor2_fail_reports_DTC);

    /* SWR-CVC-006: Zero-torque latch */
    RUN_TEST(test_Fault_forces_zero_torque);
    RUN_TEST(test_Latch_clear_requires_50_cycles);
    RUN_TEST(test_Fault_clears_DTC_when_fault_free);

    /* SWR-CVC-007: Torque mapping and ramp limit */
    RUN_TEST(test_Torque_mapping_mid_range);
    RUN_TEST(test_Ramp_limit_caps_torque_increase);

    /* SWR-CVC-008: Mode limits */
    RUN_TEST(test_Mode_limit_run_max_1000);
    RUN_TEST(test_Mode_limit_degraded_max_750);
    RUN_TEST(test_Mode_limit_limp_max_300);
    RUN_TEST(test_Torque_zero_in_safe_stop);

    /* --- HARDENED TESTS --- */

    /* SWR-CVC-001: NULL pointer and defensive checks */
    RUN_TEST(test_Init_null_config_rejects);
    RUN_TEST(test_GetPosition_null_pointer_rejects);
    RUN_TEST(test_GetPosition_returns_scaled_percentage);

    /* SWR-CVC-003: Plausibility boundary values */
    RUN_TEST(test_Plausibility_boundary_exact_threshold);
    RUN_TEST(test_Plausibility_boundary_one_below_threshold);

    /* SWR-CVC-005: Fault injection — ADC stuck-at and dual failure */
    RUN_TEST(test_Fault_injection_sensor1_adc_0xFFFF);
    RUN_TEST(test_Fault_injection_both_sensors_adc_0x0000);
    RUN_TEST(test_Fault_injection_both_sensors_spi_fail);

    /* SWR-CVC-006: Latch — intermittent fault resets counter */
    RUN_TEST(test_Latch_reset_on_fault_during_clear_window);

    /* SWR-CVC-007: Ramp — immediate decrease */
    RUN_TEST(test_Ramp_immediate_decrease_allowed);

    /* SWR-CVC-008: Mode limits — SHUTDOWN, INIT, unknown state */
    RUN_TEST(test_Torque_zero_in_shutdown);
    RUN_TEST(test_Torque_zero_in_init);
    RUN_TEST(test_Mode_limit_unknown_state_defaults_zero);

    return UNITY_END();
}

/* ==================================================================
 * Mock: Com_SendSignal (Swc_Pedal publishes torque to Com)
 * ================================================================== */

Std_ReturnType Com_SendSignal(uint16 SignalId, const void* SignalDataPtr)
{
    mock_com_last_signal_id = SignalId;
    mock_com_send_count++;
    (void)SignalDataPtr;
    return E_OK;
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_PEDAL_H
#define CVC_CFG_H
#define IOHWAB_H
#define RTE_H
#define DEM_H
#define COM_H

#include "../src/Swc_Pedal.c"
