/**
 * @file    test_int_heartbeat_loss.c
 * @brief   Integration test: ECU heartbeat timeout -> degraded mode -> safe stop
 * @date    2026-02-24
 *
 * @verifies SWR-BSW-019, SWR-BSW-020, SWR-BSW-022, SWR-BSW-026
 * @aspice   SWE.5 — Software Component Verification & Integration
 * @iso      ISO 26262 Part 6, Section 10
 *
 * Simulates heartbeat monitoring: a monitor runnable reads the RTE heartbeat
 * counter each cycle. If the counter is stale for N cycles, the runnable
 * reports a DEM fault and requests BswM degraded/safe_stop mode transition.
 *
 * Linked modules (REAL): WdgM.c, BswM.c, Dem.c, Rte.c
 * Mocked: Dio_FlipChannel (called by WdgM for external watchdog toggle)
 *
 * @copyright Taktflow Systems 2026
 */
#include "unity.h"
#include "WdgM.h"
#include "BswM.h"
#include "Dem.h"
#include "Rte.h"
#include "ComStack_Types.h"

/* Stub: PduR_Transmit (Dem_MainFunction DTC broadcast dependency) */
Std_ReturnType PduR_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
    (void)TxPduId;
    (void)PduInfoPtr;
    return E_OK;
}

/* ====================================================================
 * Mock: Dio_FlipChannel — WdgM toggles external watchdog via DIO
 * ==================================================================== */

static uint32 mock_dio_flip_count;
static uint8  mock_dio_last_channel;

uint8 Dio_FlipChannel(uint8 ChannelId)
{
    mock_dio_last_channel = ChannelId;
    mock_dio_flip_count++;
    return STD_HIGH;
}

/* ====================================================================
 * Constants for heartbeat monitoring
 * ==================================================================== */

#define HB_SIGNAL_FZC        RTE_SIG_HEARTBEAT  /* Signal ID 9 for FZC heartbeat */
#define HB_SIGNAL_RZC        10u                /* Extended signal ID for RZC heartbeat */
#define HB_TIMEOUT_CYCLES    5u                 /* Stale for 5 cycles = timeout */
#define DEM_EVENT_HB_FZC     20u                /* DEM event for FZC heartbeat loss */
#define DEM_EVENT_HB_RZC     21u                /* DEM event for RZC heartbeat loss */
#define BSWM_REQUESTER_HB    3u                 /* BswM requester ID for heartbeat */

#define MOTOR_ENABLE_SIGNAL  RTE_SIG_MOTOR_STATUS  /* Signal ID 4 */

/* ====================================================================
 * Monitor Runnable: Heartbeat Monitor (defined in test, simulates SWC)
 *
 * Reads heartbeat counter from RTE. If counter has not changed for
 * HB_TIMEOUT_CYCLES consecutive calls, reports DEM event and requests
 * BswM DEGRADED. If both FZC and RZC heartbeats are lost, requests
 * BswM SAFE_STOP.
 * ==================================================================== */

static uint32 hb_fzc_last_value;
static uint8  hb_fzc_stale_count;
static uint32 hb_rzc_last_value;
static uint8  hb_rzc_stale_count;
static boolean hb_fzc_lost;
static boolean hb_rzc_lost;

static void HeartbeatMonitor_Init(void)
{
    hb_fzc_last_value  = 0u;
    hb_fzc_stale_count = 0u;
    hb_rzc_last_value  = 0u;
    hb_rzc_stale_count = 0u;
    hb_fzc_lost        = FALSE;
    hb_rzc_lost        = FALSE;
}

static void HeartbeatMonitor_Runnable(void)
{
    uint32 fzc_counter = 0u;
    uint32 rzc_counter = 0u;

    /* Read FZC heartbeat counter from RTE */
    if (Rte_Read(HB_SIGNAL_FZC, &fzc_counter) == E_OK) {
        if (fzc_counter == hb_fzc_last_value) {
            hb_fzc_stale_count++;
        } else {
            hb_fzc_stale_count = 0u;
            hb_fzc_lost = FALSE;
        }
        hb_fzc_last_value = fzc_counter;
    }

    /* Read RZC heartbeat counter from RTE */
    if (Rte_Read(HB_SIGNAL_RZC, &rzc_counter) == E_OK) {
        if (rzc_counter == hb_rzc_last_value) {
            hb_rzc_stale_count++;
        } else {
            hb_rzc_stale_count = 0u;
            hb_rzc_lost = FALSE;
        }
        hb_rzc_last_value = rzc_counter;
    }

    /* FZC heartbeat timeout -> DEGRADED */
    if ((hb_fzc_stale_count >= HB_TIMEOUT_CYCLES) && (hb_fzc_lost == FALSE)) {
        hb_fzc_lost = TRUE;
        Dem_ReportErrorStatus(DEM_EVENT_HB_FZC, DEM_EVENT_STATUS_FAILED);
        (void)BswM_RequestMode(BSWM_REQUESTER_HB, BSWM_DEGRADED);
    }

    /* RZC heartbeat timeout -> DEGRADED */
    if ((hb_rzc_stale_count >= HB_TIMEOUT_CYCLES) && (hb_rzc_lost == FALSE)) {
        hb_rzc_lost = TRUE;
        Dem_ReportErrorStatus(DEM_EVENT_HB_RZC, DEM_EVENT_STATUS_FAILED);
        (void)BswM_RequestMode(BSWM_REQUESTER_HB, BSWM_DEGRADED);
    }

    /* Both heartbeats lost -> SAFE_STOP */
    if ((hb_fzc_lost == TRUE) && (hb_rzc_lost == TRUE)) {
        (void)BswM_RequestMode(BSWM_REQUESTER_HB, BSWM_SAFE_STOP);
    }
}

/* ====================================================================
 * Test Configuration
 * ==================================================================== */

/* RTE signals: heartbeat FZC, heartbeat RZC, motor status + standard signals */
static const Rte_SignalConfigType rte_signals[] = {
    { RTE_SIG_TORQUE_REQUEST,  0u },
    { RTE_SIG_STEERING_ANGLE,  0u },
    { RTE_SIG_VEHICLE_SPEED,   0u },
    { RTE_SIG_BRAKE_PRESSURE,  0u },
    { RTE_SIG_MOTOR_STATUS,    1u },  /* Motor enable: starts as 1 (enabled) */
    { RTE_SIG_BATTERY_VOLTAGE, 0u },
    { RTE_SIG_BATTERY_CURRENT, 0u },
    { RTE_SIG_MOTOR_TEMP,      0u },
    { RTE_SIG_ESTOP_STATUS,    0u },
    { RTE_SIG_HEARTBEAT,       0u },  /* FZC heartbeat counter */
    { HB_SIGNAL_RZC,           0u }   /* RZC heartbeat counter */
};

#define RTE_SIGNAL_COUNT  11u

/* RTE does not use runnables in this test (we call the monitor manually) */
static Rte_ConfigType rte_cfg;

/* WdgM supervised entities (minimal config for integration) */
static const WdgM_SupervisedEntityConfigType wdgm_se[] = {
    { 0u, 0u, 10u, 3u }  /* SE 0: relaxed limits (not testing WdgM here) */
};

static WdgM_ConfigType wdgm_cfg;

/* BswM: no mode actions needed (we just check mode transitions) */
static BswM_ConfigType bswm_cfg;

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_dio_flip_count = 0u;
    mock_dio_last_channel = 0xFFu;

    HeartbeatMonitor_Init();

    /* Initialize DEM */
    Dem_Init(NULL_PTR);

    /* Initialize WdgM (needed because Rte.c calls WdgM_CheckpointReached) */
    wdgm_cfg.seConfig = wdgm_se;
    wdgm_cfg.seCount  = 1u;
    wdgm_cfg.wdtDioChannel = 5u;
    WdgM_Init(&wdgm_cfg);

    /* Initialize BswM in STARTUP, then transition to RUN */
    bswm_cfg.ModeActions = NULL_PTR;
    bswm_cfg.ActionCount = 0u;
    BswM_Init(&bswm_cfg);
    (void)BswM_RequestMode(0u, BSWM_RUN);

    /* Initialize RTE with heartbeat signals */
    rte_cfg.signalConfig   = rte_signals;
    rte_cfg.signalCount    = RTE_SIGNAL_COUNT;
    rte_cfg.runnableConfig = NULL_PTR;
    rte_cfg.runnableCount  = 0u;
    Rte_Init(&rte_cfg);
}

void tearDown(void) { }

/* ====================================================================
 * INT-008-01: Heartbeat present — system stays in RUN
 * ==================================================================== */

/** @verifies SWR-BSW-022, SWR-BSW-026 */
void test_int_heartbeat_present_system_ok(void)
{
    uint8 cycle;

    for (cycle = 0u; cycle < 20u; cycle++) {
        /* Simulate heartbeat: increment counter each cycle */
        (void)Rte_Write(HB_SIGNAL_FZC, (uint32)(cycle + 1u));
        (void)Rte_Write(HB_SIGNAL_RZC, (uint32)(cycle + 1u));

        HeartbeatMonitor_Runnable();
    }

    /* System must remain in RUN mode */
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());

    /* No DEM events should be reported */
    {
        uint8 status = 0u;
        (void)Dem_GetEventStatus(DEM_EVENT_HB_FZC, &status);
        TEST_ASSERT_EQUAL_HEX8(0x00u, status);
    }
}

/* ====================================================================
 * INT-008-02: Stop updating heartbeat -> BswM transitions to DEGRADED
 * ==================================================================== */

/** @verifies SWR-BSW-022, SWR-BSW-026 */
void test_int_heartbeat_timeout_triggers_degraded(void)
{
    uint8 cycle;

    /* First, provide some heartbeats */
    for (cycle = 0u; cycle < 3u; cycle++) {
        (void)Rte_Write(HB_SIGNAL_FZC, (uint32)(cycle + 1u));
        (void)Rte_Write(HB_SIGNAL_RZC, (uint32)(cycle + 1u));
        HeartbeatMonitor_Runnable();
    }

    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());

    /* Stop updating FZC heartbeat — counter stays at 3 */
    for (cycle = 0u; cycle < HB_TIMEOUT_CYCLES + 1u; cycle++) {
        /* RZC keeps beating, FZC does not */
        (void)Rte_Write(HB_SIGNAL_RZC, (uint32)(4u + cycle));
        HeartbeatMonitor_Runnable();
    }

    /* System should be in DEGRADED */
    TEST_ASSERT_EQUAL(BSWM_DEGRADED, BswM_GetCurrentMode());
}

/* ====================================================================
 * INT-008-03: On heartbeat timeout, verify DEM event is reported
 * ==================================================================== */

/** @verifies SWR-BSW-018, SWR-BSW-022 */
void test_int_heartbeat_timeout_dem_event(void)
{
    uint8 cycle;
    uint8 status;

    /* Provide initial heartbeats */
    (void)Rte_Write(HB_SIGNAL_FZC, 1u);
    HeartbeatMonitor_Runnable();

    /* Stop updating — run for timeout period */
    for (cycle = 0u; cycle < HB_TIMEOUT_CYCLES + 1u; cycle++) {
        HeartbeatMonitor_Runnable();
    }

    /* DEM event for FZC heartbeat loss should be reported (testFailed bit set) */
    status = 0u;
    (void)Dem_GetEventStatus(DEM_EVENT_HB_FZC, &status);
    TEST_ASSERT_BITS(DEM_STATUS_TEST_FAILED, DEM_STATUS_TEST_FAILED, status);
}

/* ====================================================================
 * INT-008-04: Heartbeat returns after DEGRADED entry — BswM is
 *             forward-only, so system stays in DEGRADED
 * ==================================================================== */

/** @verifies SWR-BSW-022 */
void test_int_heartbeat_recovery_from_degraded(void)
{
    uint8 cycle;

    /* Establish heartbeats */
    (void)Rte_Write(HB_SIGNAL_FZC, 1u);
    (void)Rte_Write(HB_SIGNAL_RZC, 1u);
    HeartbeatMonitor_Runnable();

    /* Timeout FZC heartbeat */
    for (cycle = 0u; cycle < HB_TIMEOUT_CYCLES + 1u; cycle++) {
        (void)Rte_Write(HB_SIGNAL_RZC, (uint32)(2u + cycle));
        HeartbeatMonitor_Runnable();
    }

    TEST_ASSERT_EQUAL(BSWM_DEGRADED, BswM_GetCurrentMode());

    /* "Recover" heartbeat — start updating FZC again */
    for (cycle = 0u; cycle < 10u; cycle++) {
        (void)Rte_Write(HB_SIGNAL_FZC, (uint32)(100u + cycle));
        (void)Rte_Write(HB_SIGNAL_RZC, (uint32)(100u + cycle));
        HeartbeatMonitor_Runnable();
    }

    /* BswM is forward-only: RUN cannot be re-entered from DEGRADED.
     * System MUST stay in DEGRADED even though heartbeat recovered. */
    TEST_ASSERT_EQUAL(BSWM_DEGRADED, BswM_GetCurrentMode());
}

/* ====================================================================
 * INT-008-05: Both FZC and RZC heartbeats lost -> SAFE_STOP
 * ==================================================================== */

/** @verifies SWR-BSW-022, SWR-BSW-018 */
void test_int_dual_heartbeat_loss_safe_stop(void)
{
    uint8 cycle;

    /* Establish heartbeats */
    (void)Rte_Write(HB_SIGNAL_FZC, 1u);
    (void)Rte_Write(HB_SIGNAL_RZC, 1u);
    HeartbeatMonitor_Runnable();

    /* Stop BOTH heartbeat counters */
    for (cycle = 0u; cycle < HB_TIMEOUT_CYCLES + 2u; cycle++) {
        HeartbeatMonitor_Runnable();
    }

    /* System must be in SAFE_STOP (both heartbeats lost) */
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    /* Both DEM events should have testFailed bit */
    {
        uint8 status_fzc = 0u;
        uint8 status_rzc = 0u;
        (void)Dem_GetEventStatus(DEM_EVENT_HB_FZC, &status_fzc);
        (void)Dem_GetEventStatus(DEM_EVENT_HB_RZC, &status_rzc);
        TEST_ASSERT_BITS(DEM_STATUS_TEST_FAILED, DEM_STATUS_TEST_FAILED, status_fzc);
        TEST_ASSERT_BITS(DEM_STATUS_TEST_FAILED, DEM_STATUS_TEST_FAILED, status_rzc);
    }
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_int_heartbeat_present_system_ok);
    RUN_TEST(test_int_heartbeat_timeout_triggers_degraded);
    RUN_TEST(test_int_heartbeat_timeout_dem_event);
    RUN_TEST(test_int_heartbeat_recovery_from_degraded);
    RUN_TEST(test_int_dual_heartbeat_loss_safe_stop);

    return UNITY_END();
}
