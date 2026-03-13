/**
 * @file    test_Swc_FzcCanMonitor.c
 * @brief   Unit tests for Swc_FzcCanMonitor — CAN bus loss detection
 * @date    2026-02-24
 *
 * @verifies SWR-FZC-024
 *
 * Tests bus-off detection, silence detection (200ms), error warning
 * sustained, NO recovery (safe state latch until power cycle),
 * and safe state outputs (100% brake, center steering, continuous buzzer).
 *
 * Mocks: CanIf_GetControllerMode, Rte_Write, Dem_ReportErrorStatus
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

/* Prevent BSW headers from redefining types */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_FZC_CAN_MONITOR_H
#define FZC_CFG_H
#define RTE_H
#define CAN_H
#define DEM_H

/* ==================================================================
 * FZC Config Constants
 * ================================================================== */

#define FZC_SIG_STEER_CMD          16u
#define FZC_SIG_BRAKE_CMD          19u
#define FZC_SIG_BUZZER_PATTERN     28u
#define FZC_SIG_COUNT              36u

#define FZC_BUZZER_CONTINUOUS       4u
#define FZC_DTC_CAN_BUS_OFF        12u

#define FZC_CAN_OK                  0u
#define FZC_CAN_BUS_OFF             1u
#define FZC_CAN_SILENCE             2u
#define FZC_CAN_ERROR_WARNING       3u
#define FZC_CAN_SILENCE_CYCLES     20u
#define FZC_CAN_ERR_WARN_CYCLES    50u

/* Can controller state (mirrors Can_StateType from Can.h) */
typedef enum {
    CAN_CS_UNINIT  = 0u,
    CAN_CS_STOPPED = 1u,
    CAN_CS_STARTED = 2u,
    CAN_CS_SLEEP   = 3u
} Can_StateType;

/* CanIf controller modes (legacy aliases used in test setup helpers) */
#define CANIF_CS_STARTED            CAN_CS_STARTED
#define CANIF_CS_STOPPED            CAN_CS_STOPPED
#define CANIF_CS_ERROR_WARNING      2u

/* DEM event status */
#define DEM_EVENT_STATUS_PASSED     0u
#define DEM_EVENT_STATUS_FAILED     1u

/* ==================================================================
 * Swc_FzcCanMonitor API declarations
 * ================================================================== */

extern void  Swc_FzcCanMonitor_Init(void);
extern void  Swc_FzcCanMonitor_Check(void);
extern uint8 Swc_FzcCanMonitor_GetStatus(void);
extern void  Swc_FzcCanMonitor_NotifyRx(void);

/* ==================================================================
 * Mock: Can_GetControllerMode
 * ================================================================== */

static Can_StateType  mock_canif_mode;

Can_StateType Can_GetControllerMode(uint8 Controller)
{
    (void)Controller;
    return mock_canif_mode;
}

/* ==================================================================
 * Mock: Can_GetErrorCounters
 * ================================================================== */

static uint8 mock_can_tec;
static uint8 mock_can_rec;

Std_ReturnType Can_GetErrorCounters(uint8 Controller, uint8* tec, uint8* rec)
{
    (void)Controller;
    if ((tec == NULL_PTR) || (rec == NULL_PTR)) {
        return E_NOT_OK;
    }
    *tec = mock_can_tec;
    *rec = mock_can_rec;
    return E_OK;
}

/* ==================================================================
 * Mock: Rte_Write
 * ================================================================== */

#define MOCK_RTE_MAX_SIGNALS  64u

static uint32  mock_rte_signals[MOCK_RTE_MAX_SIGNALS];
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
 * Mock: Dem_ReportErrorStatus
 * ================================================================== */

static uint8 mock_dem_last_event_id;
static uint8 mock_dem_last_status;
static uint8 mock_dem_call_count;

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_call_count++;
    mock_dem_last_event_id = EventId;
    mock_dem_last_status   = EventStatus;
}

/* ==================================================================
 * Include SWC under test (source inclusion for test build)
 * ================================================================== */

#include "../src/Swc_FzcCanMonitor.c"

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    uint8 i;

    mock_canif_mode        = CAN_CS_STARTED;
    mock_can_tec           = 0u;
    mock_can_rec           = 0u;
    mock_rte_write_count   = 0u;
    mock_dem_call_count    = 0u;
    mock_dem_last_event_id = 0xFFu;
    mock_dem_last_status   = 0xFFu;

    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }

    Swc_FzcCanMonitor_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-FZC-024: Bus-Off Detection (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-024 — Bus-off triggers auto-brake 100%, center steering, buzzer */
void test_FzcCanMonitor_busoff_triggers_autobrake(void)
{
    /* Simulate bus-off condition */
    mock_canif_mode = CAN_CS_STOPPED;

    Swc_FzcCanMonitor_Check();

    /* assert: brake = 100% */
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_CMD]);

    /* assert: steering = center (0) */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_STEER_CMD]);

    /* assert: buzzer = continuous */
    TEST_ASSERT_EQUAL_UINT32(FZC_BUZZER_CONTINUOUS, mock_rte_signals[FZC_SIG_BUZZER_PATTERN]);

    /* assert: status = bus-off */
    TEST_ASSERT_EQUAL_UINT8(FZC_CAN_BUS_OFF, Swc_FzcCanMonitor_GetStatus());

    /* assert: DTC reported */
    TEST_ASSERT_EQUAL_UINT8(FZC_DTC_CAN_BUS_OFF, mock_dem_last_event_id);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED, mock_dem_last_status);
}

/* ==================================================================
 * SWR-FZC-024: Silence Detection (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-024 — 200ms silence (20 cycles no RX) triggers safe state */
void test_FzcCanMonitor_silence_200ms_triggers_safe(void)
{
    uint16 i;

    /* Run 19 cycles without any RX notification: should be OK */
    for (i = 0u; i < 19u; i++) {
        Swc_FzcCanMonitor_Check();
    }
    TEST_ASSERT_EQUAL_UINT8(FZC_CAN_OK, Swc_FzcCanMonitor_GetStatus());

    /* 20th cycle: silence threshold reached */
    Swc_FzcCanMonitor_Check();
    TEST_ASSERT_EQUAL_UINT8(FZC_CAN_SILENCE, Swc_FzcCanMonitor_GetStatus());

    /* assert: safe state applied */
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_CMD]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_STEER_CMD]);
    TEST_ASSERT_EQUAL_UINT32(FZC_BUZZER_CONTINUOUS, mock_rte_signals[FZC_SIG_BUZZER_PATTERN]);
}

/* ==================================================================
 * SWR-FZC-024: No Recovery (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-024 — After safe latch, no recovery even if CAN returns */
void test_FzcCanMonitor_no_recovery_stays_safe(void)
{
    uint16 i;

    /* Trigger bus-off */
    mock_canif_mode = CAN_CS_STOPPED;
    Swc_FzcCanMonitor_Check();
    TEST_ASSERT_EQUAL_UINT8(FZC_CAN_BUS_OFF, Swc_FzcCanMonitor_GetStatus());

    /* Restore CAN to normal */
    mock_canif_mode = CAN_CS_STARTED;

    /* Notify RX (bus is alive again) */
    Swc_FzcCanMonitor_NotifyRx();

    /* Run many cycles: safe state MUST persist (no recovery) */
    for (i = 0u; i < 100u; i++) {
        Swc_FzcCanMonitor_Check();
    }

    /* assert: still in safe state */
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_CMD]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_STEER_CMD]);
    TEST_ASSERT_EQUAL_UINT32(FZC_BUZZER_CONTINUOUS, mock_rte_signals[FZC_SIG_BUZZER_PATTERN]);
}

/* ==================================================================
 * SWR-FZC-024: Error Warning Sustained (1 test)
 * ================================================================== */

/** @verifies SWR-FZC-024 — Sustained error warning for 50 cycles triggers safe state */
void test_FzcCanMonitor_error_warning_sustained(void)
{
    uint16 i;

    /* Inject error counters at/above the error warning threshold (96) */
    mock_can_tec = 96u;

    /* Notify RX to prevent silence timeout during this test */
    /* Run 49 cycles: error warning but not yet sustained threshold */
    for (i = 0u; i < 49u; i++) {
        Swc_FzcCanMonitor_NotifyRx();
        Swc_FzcCanMonitor_Check();
    }
    TEST_ASSERT_EQUAL_UINT8(FZC_CAN_OK, Swc_FzcCanMonitor_GetStatus());

    /* 50th cycle: sustained threshold reached */
    Swc_FzcCanMonitor_NotifyRx();
    Swc_FzcCanMonitor_Check();
    TEST_ASSERT_EQUAL_UINT8(FZC_CAN_ERROR_WARNING, Swc_FzcCanMonitor_GetStatus());

    /* assert: safe state applied */
    TEST_ASSERT_EQUAL_UINT32(100u, mock_rte_signals[FZC_SIG_BRAKE_CMD]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[FZC_SIG_STEER_CMD]);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-FZC-024: CAN Bus Loss Detection */
    RUN_TEST(test_FzcCanMonitor_busoff_triggers_autobrake);
    RUN_TEST(test_FzcCanMonitor_silence_200ms_triggers_safe);
    RUN_TEST(test_FzcCanMonitor_no_recovery_stays_safe);
    RUN_TEST(test_FzcCanMonitor_error_warning_sustained);

    return UNITY_END();
}
