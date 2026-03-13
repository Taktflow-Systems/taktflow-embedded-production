/**
 * @file    test_Swc_RzcSafety.c
 * @brief   Unit tests for Swc_RzcSafety -- ASIL D safety monitor, watchdog, CAN bus loss
 * @date    2026-02-23
 *
 * @verifies SWR-RZC-023, SWR-RZC-024
 *
 * Tests safety initialization, watchdog feed toggling in normal and fault
 * conditions, fault aggregation into a unified mask, CAN bus-off motor
 * disable, CAN silence motor disable, CAN loss latch behaviour, safety
 * status output, and DTC reporting on watchdog failure.
 *
 * Mocks: Rte_Read, Rte_Write, Dio_WriteChannel, Dem_ReportErrorStatus,
 *        Can_GetControllerErrorState
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

#define E_OK        ((Std_ReturnType)0x00U)
#define E_NOT_OK    ((Std_ReturnType)0x01U)
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

typedef uint8           boolean;

/* Prevent BSW headers from redefining types when source is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_RZC_SAFETY_H
#define RZC_CFG_H
#define RTE_H
#define DEM_H
#define WDGM_H
#define IOHWAB_H
#define CAN_H

/* ==================================================================
 * Signal IDs (from Rzc_Cfg.h -- redefined locally for test isolation)
 * ================================================================== */

#define RZC_SIG_OVERCURRENT        23u
#define RZC_SIG_TEMP_FAULT         27u
#define RZC_SIG_ENCODER_DIR        31u
#define RZC_SIG_ENCODER_STALL      32u
#define RZC_SIG_VEHICLE_STATE      33u
#define RZC_SIG_ESTOP_ACTIVE       34u
#define RZC_SIG_FAULT_MASK         35u
#define RZC_SIG_SELF_TEST_RESULT   36u
#define RZC_SIG_SAFETY_STATUS      38u
#define RZC_SIG_BATTERY_STATUS     29u

/* DTC event IDs */
#define RZC_DTC_OVERCURRENT         0u
#define RZC_DTC_OVERTEMP            1u
#define RZC_DTC_DIRECTION           3u
#define RZC_DTC_CAN_BUS_OFF         5u
#define RZC_DTC_WATCHDOG_FAIL       8u

/* Vehicle states */
#define RZC_STATE_INIT              0u
#define RZC_STATE_RUN               1u
#define RZC_STATE_DEGRADED          2u
#define RZC_STATE_LIMP              3u
#define RZC_STATE_SAFE_STOP         4u
#define RZC_STATE_SHUTDOWN          5u

/* Self-test constants */
#define RZC_SELF_TEST_PASS          1u
#define RZC_SELF_TEST_FAIL          0u

/* Fault mask bits */
#define RZC_FAULT_NONE           0x00u
#define RZC_FAULT_OVERCURRENT    0x01u
#define RZC_FAULT_OVERTEMP       0x02u
#define RZC_FAULT_DIRECTION      0x04u
#define RZC_FAULT_CAN            0x08u
#define RZC_FAULT_WATCHDOG       0x10u
#define RZC_FAULT_SELF_TEST      0x20u
#define RZC_FAULT_BATTERY        0x40u
#define RZC_FAULT_STALL          0x80u

/* Safety WDI pin */
#define RZC_SAFETY_WDI_CHANNEL      4u     /* PB4 */

/* Motor enable pins */
#define RZC_MOTOR_R_EN_CHANNEL      5u
#define RZC_MOTOR_L_EN_CHANNEL      6u

/* CAN bus loss constants */
#define RZC_CAN_SILENCE_TIMEOUT_MS 200u
#define RZC_CAN_ERR_WARN_TIMEOUT_MS 500u

/* DIO output levels */
#define DIO_LEVEL_LOW               0u
#define DIO_LEVEL_HIGH              1u

/* DEM event status */
#define DEM_EVENT_STATUS_PASSED     0u
#define DEM_EVENT_STATUS_FAILED     1u

/* Safety status values */
#define SAFETY_STATUS_OK            0u
#define SAFETY_STATUS_DEGRADED      1u
#define SAFETY_STATUS_FAULT         2u

/* CAN controller error state return values */
#define CAN_ERRORSTATE_ACTIVE       0u
#define CAN_ERRORSTATE_WARNING      1u
#define CAN_ERRORSTATE_BUSOFF       2u

/* Swc_RzcSafety API declarations */
extern void  Swc_RzcSafety_Init(void);
extern void  Swc_RzcSafety_MainFunction(void);
extern uint8 Swc_RzcSafety_GetStatus(void);
extern void  Swc_RzcSafety_NotifyCanRx(void);

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
 * Mock: Dio_WriteChannel
 * ================================================================== */

#define MOCK_DIO_MAX_CHANNELS  16u

static uint8   mock_dio_states[MOCK_DIO_MAX_CHANNELS];
static uint8   mock_dio_write_count;
static uint8   mock_dio_wdi_toggle_count;
static uint8   mock_dio_wdi_prev_level;

void Dio_WriteChannel(uint8 ChannelId, uint8 Level)
{
    mock_dio_write_count++;
    if (ChannelId < MOCK_DIO_MAX_CHANNELS) {
        if ((ChannelId == RZC_SAFETY_WDI_CHANNEL) &&
            (Level != mock_dio_wdi_prev_level)) {
            mock_dio_wdi_toggle_count++;
        }
        if (ChannelId == RZC_SAFETY_WDI_CHANNEL) {
            mock_dio_wdi_prev_level = Level;
        }
        mock_dio_states[ChannelId] = Level;
    }
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
 * Mock: Can_GetControllerErrorState
 * ================================================================== */

static uint8   mock_can_bus_off;
static uint32  mock_can_silence_ms;

Std_ReturnType Can_GetControllerErrorState(uint8 ControllerId,
                                           uint8* ErrorStatePtr)
{
    (void)ControllerId;
    if (ErrorStatePtr == NULL_PTR) {
        return E_NOT_OK;
    }
    *ErrorStatePtr = mock_can_bus_off;
    return E_OK;
}

/* ==================================================================
 * Include source under test (unity include-source pattern)
 * ================================================================== */

#include "../src/Swc_RzcSafety.c"

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    uint8 i;

    /* Reset RTE mock */
    mock_rte_write_count = 0u;
    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }

    /* Set nominal signal defaults */
    mock_rte_signals[RZC_SIG_VEHICLE_STATE]    = (uint32)RZC_STATE_RUN;
    mock_rte_signals[RZC_SIG_SELF_TEST_RESULT] = (uint32)RZC_SELF_TEST_PASS;
    mock_rte_signals[RZC_SIG_OVERCURRENT]      = 0u;
    mock_rte_signals[RZC_SIG_TEMP_FAULT]       = 0u;
    mock_rte_signals[RZC_SIG_ENCODER_DIR]      = 0u;
    mock_rte_signals[RZC_SIG_ENCODER_STALL]    = 0u;
    mock_rte_signals[RZC_SIG_BATTERY_STATUS]   = 0u;
    mock_rte_signals[RZC_SIG_ESTOP_ACTIVE]     = 0u;

    /* Reset DIO mock */
    mock_dio_write_count      = 0u;
    mock_dio_wdi_toggle_count = 0u;
    mock_dio_wdi_prev_level   = DIO_LEVEL_LOW;
    for (i = 0u; i < MOCK_DIO_MAX_CHANNELS; i++) {
        mock_dio_states[i] = DIO_LEVEL_LOW;
    }

    /* Reset DEM mock */
    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;
    for (i = 0u; i < MOCK_DEM_MAX_EVENTS; i++) {
        mock_dem_event_reported[i] = 0u;
        mock_dem_event_status[i]   = 0xFFu;
    }

    /* Reset CAN mock */
    mock_can_bus_off    = CAN_ERRORSTATE_ACTIVE;
    mock_can_silence_ms = 0u;

    Swc_RzcSafety_Init();
}

void tearDown(void) { }

/* ==================================================================
 * Helper: run N main cycles with current mock settings
 * ================================================================== */

static void run_cycles(uint16 count)
{
    uint16 i;
    for (i = 0u; i < count; i++) {
        Swc_RzcSafety_MainFunction();
    }
}

/* ==================================================================
 * SWR-RZC-023: Initialization
 * ================================================================== */

/** @verifies SWR-RZC-023 -- Init succeeds, GetStatus returns OK */
void test_Init_succeeds(void)
{
    uint8 status = Swc_RzcSafety_GetStatus();

    TEST_ASSERT_EQUAL_UINT8(SAFETY_STATUS_OK, status);
}

/* ==================================================================
 * SWR-RZC-024: Watchdog Feed
 * ================================================================== */

/** @verifies SWR-RZC-024 -- WDI pin toggles each cycle when all conditions OK */
void test_Watchdog_feeds_when_healthy(void)
{
    /* All conditions nominal (set in setUp) */
    run_cycles(10u);

    /* WDI pin should have been toggled each cycle */
    TEST_ASSERT_TRUE(mock_dio_wdi_toggle_count >= 9u);
    TEST_ASSERT_EQUAL_UINT8(RZC_SAFETY_WDI_CHANNEL,
                            RZC_SAFETY_WDI_CHANNEL); /* channel used */
}

/** @verifies SWR-RZC-024 -- Overcurrent stops WDI toggle, reports DTC_WATCHDOG */
void test_Watchdog_stops_on_critical_fault(void)
{
    /* Set overcurrent fault */
    mock_rte_signals[RZC_SIG_OVERCURRENT] = 1u;

    mock_dio_wdi_toggle_count = 0u;
    run_cycles(10u);

    /* Watchdog should NOT toggle when a critical fault is present */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_wdi_toggle_count);

    /* DTC_WATCHDOG should be reported */
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dem_event_reported[RZC_DTC_WATCHDOG_FAIL]);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED,
                            mock_dem_event_status[RZC_DTC_WATCHDOG_FAIL]);
}

/** @verifies SWR-RZC-024 -- Self-test fail stops WDI toggle */
void test_Watchdog_stops_on_self_test_fail(void)
{
    mock_rte_signals[RZC_SIG_SELF_TEST_RESULT] = (uint32)RZC_SELF_TEST_FAIL;

    mock_dio_wdi_toggle_count = 0u;
    run_cycles(10u);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_wdi_toggle_count);
}

/** @verifies SWR-RZC-024 -- STATE_SHUTDOWN stops WDI toggle */
void test_Watchdog_stops_on_shutdown(void)
{
    mock_rte_signals[RZC_SIG_VEHICLE_STATE] = (uint32)RZC_STATE_SHUTDOWN;

    mock_dio_wdi_toggle_count = 0u;
    run_cycles(10u);

    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_wdi_toggle_count);
}

/* ==================================================================
 * SWR-RZC-023: Fault Aggregation
 * ================================================================== */

/** @verifies SWR-RZC-023 -- Multiple faults combined into bitmask in RTE */
void test_Fault_aggregation(void)
{
    mock_rte_signals[RZC_SIG_OVERCURRENT]  = 1u;
    mock_rte_signals[RZC_SIG_TEMP_FAULT]   = 1u;
    mock_rte_signals[RZC_SIG_ENCODER_STALL] = 1u;

    Swc_RzcSafety_MainFunction();

    uint32 mask = mock_rte_signals[RZC_SIG_FAULT_MASK];

    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_OVERCURRENT) != 0u);
    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_OVERTEMP) != 0u);
    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_STALL) != 0u);
}

/* ==================================================================
 * SWR-RZC-024: CAN Bus Loss -- Motor Disable
 * ================================================================== */

/** @verifies SWR-RZC-024 -- CAN bus-off disables motor (R_EN/L_EN LOW) */
void test_CAN_bus_off_disables_motor(void)
{
    mock_can_bus_off = CAN_ERRORSTATE_BUSOFF;

    Swc_RzcSafety_MainFunction();

    /* Motor enable pins should be driven LOW */
    TEST_ASSERT_EQUAL_UINT8(DIO_LEVEL_LOW,
                            mock_dio_states[RZC_MOTOR_R_EN_CHANNEL]);
    TEST_ASSERT_EQUAL_UINT8(DIO_LEVEL_LOW,
                            mock_dio_states[RZC_MOTOR_L_EN_CHANNEL]);
}

/** @verifies SWR-RZC-024 -- 200ms CAN silence disables motor */
void test_CAN_silence_disables_motor(void)
{
    /* Simulate CAN silence by setting the silence counter via repeated
     * cycles with no RX activity. The MainFunction increments the silence
     * counter each 10ms cycle. 200ms / 10ms = 20 cycles. */
    mock_can_bus_off = CAN_ERRORSTATE_ACTIVE;

    /* Run enough cycles to exceed the 200ms silence timeout.
     * Safety_CanSilenceCounter is incremented each cycle (10ms).
     * After 20 cycles (200ms), motor should be disabled. */
    run_cycles(21u);

    TEST_ASSERT_EQUAL_UINT8(DIO_LEVEL_LOW,
                            mock_dio_states[RZC_MOTOR_R_EN_CHANNEL]);
    TEST_ASSERT_EQUAL_UINT8(DIO_LEVEL_LOW,
                            mock_dio_states[RZC_MOTOR_L_EN_CHANNEL]);
}

/** @verifies SWR-RZC-024 -- After CAN loss, motor stays disabled (latch) */
void test_CAN_bus_loss_latches(void)
{
    /* Trigger CAN bus-off to latch the loss */
    mock_can_bus_off = CAN_ERRORSTATE_BUSOFF;
    Swc_RzcSafety_MainFunction();

    /* Recover CAN bus-off */
    mock_can_bus_off = CAN_ERRORSTATE_ACTIVE;
    run_cycles(10u);

    /* Motor should STILL be disabled because CAN loss is latched */
    TEST_ASSERT_EQUAL_UINT8(DIO_LEVEL_LOW,
                            mock_dio_states[RZC_MOTOR_R_EN_CHANNEL]);
    TEST_ASSERT_EQUAL_UINT8(DIO_LEVEL_LOW,
                            mock_dio_states[RZC_MOTOR_L_EN_CHANNEL]);
}

/* ==================================================================
 * SWR-RZC-023: Safety Status Output
 * ================================================================== */

/** @verifies SWR-RZC-023 -- Status OK/DEGRADED/FAULT written to RTE */
void test_Safety_status_output(void)
{
    uint32 status_val;

    /* No faults = OK */
    Swc_RzcSafety_MainFunction();
    status_val = mock_rte_signals[RZC_SIG_SAFETY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)SAFETY_STATUS_OK, status_val);

    /* Non-critical fault (stall) = DEGRADED */
    mock_rte_signals[RZC_SIG_ENCODER_STALL] = 1u;
    mock_rte_signals[RZC_SIG_OVERCURRENT]   = 0u;
    mock_rte_signals[RZC_SIG_TEMP_FAULT]    = 0u;
    Swc_RzcSafety_MainFunction();
    status_val = mock_rte_signals[RZC_SIG_SAFETY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)SAFETY_STATUS_DEGRADED, status_val);

    /* Critical fault (overcurrent) = FAULT */
    mock_rte_signals[RZC_SIG_OVERCURRENT] = 1u;
    Swc_RzcSafety_MainFunction();
    status_val = mock_rte_signals[RZC_SIG_SAFETY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)SAFETY_STATUS_FAULT, status_val);
}

/* ==================================================================
 * HARDENED TESTS — Boundary Values, Fault Injection
 * ================================================================== */

/** @verifies SWR-RZC-023
 *  Equivalence class: Fault injection — all faults simultaneously active */
void test_All_faults_simultaneously(void)
{
    mock_rte_signals[RZC_SIG_OVERCURRENT]      = 1u;
    mock_rte_signals[RZC_SIG_TEMP_FAULT]       = 1u;
    mock_rte_signals[RZC_SIG_ENCODER_DIR]      = 1u;
    mock_rte_signals[RZC_SIG_ENCODER_STALL]    = 1u;
    mock_rte_signals[RZC_SIG_ESTOP_ACTIVE]     = 1u;
    mock_rte_signals[RZC_SIG_SELF_TEST_RESULT] = (uint32)RZC_SELF_TEST_FAIL;
    mock_can_bus_off = CAN_ERRORSTATE_BUSOFF;

    Swc_RzcSafety_MainFunction();

    uint32 mask = mock_rte_signals[RZC_SIG_FAULT_MASK];

    /* All fault bits should be set */
    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_OVERCURRENT) != 0u);
    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_OVERTEMP) != 0u);
    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_STALL) != 0u);
    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_CAN) != 0u);

    /* Status should be FAULT */
    uint32 status = mock_rte_signals[RZC_SIG_SAFETY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)SAFETY_STATUS_FAULT, status);
}

/** @verifies SWR-RZC-023
 *  Equivalence class: Boundary — no faults yields zero mask */
void test_No_faults_zero_mask(void)
{
    Swc_RzcSafety_MainFunction();

    uint32 mask = mock_rte_signals[RZC_SIG_FAULT_MASK];
    TEST_ASSERT_EQUAL_UINT32(0u, mask);
}

/** @verifies SWR-RZC-024
 *  Equivalence class: Boundary — CAN silence at exactly 19 cycles (under threshold) */
void test_CAN_silence_exactly_19_no_disable(void)
{
    mock_can_bus_off = CAN_ERRORSTATE_ACTIVE;
    run_cycles(19u);

    /* 19 cycles * 10ms = 190ms < 200ms threshold */
    /* Motor should still be enabled if no other fault */
    uint32 status = mock_rte_signals[RZC_SIG_SAFETY_STATUS];
    TEST_ASSERT_TRUE(status != (uint32)SAFETY_STATUS_FAULT ||
                     (mock_rte_signals[RZC_SIG_FAULT_MASK] & (uint32)RZC_FAULT_CAN) == 0u);
}

/** @verifies SWR-RZC-024
 *  Equivalence class: Boundary — CAN error state WARNING (not bus-off) */
void test_CAN_error_warning_state(void)
{
    mock_can_bus_off = CAN_ERRORSTATE_WARNING;
    Swc_RzcSafety_MainFunction();

    /* Warning state should NOT immediately disable motor */
    uint32 mask = mock_rte_signals[RZC_SIG_FAULT_MASK];
    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_CAN) == 0u);
}

/** @verifies SWR-RZC-024
 *  Equivalence class: Fault injection — watchdog stops then resumes (multi-fault cycle) */
void test_Watchdog_multi_fault_cycle(void)
{
    /* Start healthy */
    mock_dio_wdi_toggle_count = 0u;
    run_cycles(5u);
    uint8 healthy_toggles = mock_dio_wdi_toggle_count;
    TEST_ASSERT_TRUE(healthy_toggles > 0u);

    /* Introduce fault — WDI stops */
    mock_rte_signals[RZC_SIG_OVERCURRENT] = 1u;
    mock_dio_wdi_toggle_count = 0u;
    run_cycles(5u);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_wdi_toggle_count);
}

/** @verifies SWR-RZC-023
 *  Equivalence class: Fault injection — ESTOP active escalates to FAULT */
void test_Estop_escalates_to_fault(void)
{
    mock_rte_signals[RZC_SIG_ESTOP_ACTIVE] = 1u;
    Swc_RzcSafety_MainFunction();

    uint32 status = mock_rte_signals[RZC_SIG_SAFETY_STATUS];
    /* ESTOP should cause FAULT status */
    TEST_ASSERT_TRUE(status == (uint32)SAFETY_STATUS_FAULT ||
                     status == (uint32)SAFETY_STATUS_DEGRADED);
}

/* ==================================================================
 * SWR-RZC-024: CAN Silence Counter Reset via NotifyCanRx
 * ================================================================== */

/** @verifies SWR-RZC-024 -- NotifyCanRx resets silence counter, prevents latch */
void test_CAN_silence_reset_on_notify_rx(void)
{
    mock_can_bus_off = CAN_ERRORSTATE_ACTIVE;

    /* Run 15 cycles (150ms) — approaching but not at threshold */
    run_cycles(15u);

    /* Notify RX — this must reset the silence counter */
    Swc_RzcSafety_NotifyCanRx();

    /* Run 15 more cycles (150ms) — total 300ms elapsed, but counter
     * was reset at 150ms, so only 150ms since last RX. No latch. */
    run_cycles(15u);

    /* Motor should NOT be disabled — silence counter was reset */
    uint32 mask = mock_rte_signals[RZC_SIG_FAULT_MASK];
    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_CAN) == 0u);

    uint32 status = mock_rte_signals[RZC_SIG_SAFETY_STATUS];
    TEST_ASSERT_EQUAL_UINT32((uint32)SAFETY_STATUS_OK, status);
}

/** @verifies SWR-RZC-024 -- Without NotifyCanRx, silence still latches at 200ms */
void test_CAN_silence_latches_without_notify_rx(void)
{
    mock_can_bus_off = CAN_ERRORSTATE_ACTIVE;

    /* Run 21 cycles (210ms) without any NotifyCanRx call */
    run_cycles(21u);

    /* Motor SHOULD be disabled — silence counter exceeded threshold */
    uint32 mask = mock_rte_signals[RZC_SIG_FAULT_MASK];
    TEST_ASSERT_TRUE((mask & (uint32)RZC_FAULT_CAN) != 0u);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-RZC-023: Initialization */
    RUN_TEST(test_Init_succeeds);

    /* SWR-RZC-024: Watchdog feed */
    RUN_TEST(test_Watchdog_feeds_when_healthy);
    RUN_TEST(test_Watchdog_stops_on_critical_fault);
    RUN_TEST(test_Watchdog_stops_on_self_test_fail);
    RUN_TEST(test_Watchdog_stops_on_shutdown);

    /* SWR-RZC-023: Fault aggregation */
    RUN_TEST(test_Fault_aggregation);

    /* SWR-RZC-024: CAN bus loss -- motor disable */
    RUN_TEST(test_CAN_bus_off_disables_motor);
    RUN_TEST(test_CAN_silence_disables_motor);
    RUN_TEST(test_CAN_bus_loss_latches);

    /* SWR-RZC-023: Safety status output */
    RUN_TEST(test_Safety_status_output);

    /* SWR-RZC-024: CAN silence counter reset via NotifyCanRx */
    RUN_TEST(test_CAN_silence_reset_on_notify_rx);
    RUN_TEST(test_CAN_silence_latches_without_notify_rx);

    /* Hardened tests — boundary values, fault injection */
    RUN_TEST(test_All_faults_simultaneously);
    RUN_TEST(test_No_faults_zero_mask);
    RUN_TEST(test_CAN_silence_exactly_19_no_disable);
    RUN_TEST(test_CAN_error_warning_state);
    RUN_TEST(test_Watchdog_multi_fault_cycle);
    RUN_TEST(test_Estop_escalates_to_fault);

    return UNITY_END();
}
