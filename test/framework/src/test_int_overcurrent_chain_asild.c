/**
 * @file    test_int_overcurrent_chain.c
 * @brief   Integration test: Current sensor -> threshold -> motor shutdown -> DEM
 * @date    2026-02-24
 *
 * @verifies SWR-BSW-018, SWR-BSW-022, SWR-BSW-026, SWR-BSW-027
 * @aspice   SWE.5 — Software Component Verification & Integration
 * @iso      ISO 26262 Part 6, Section 10
 *
 * Tests the overcurrent safety chain: an overcurrent monitor runnable reads
 * the motor current signal from RTE. If the value exceeds the threshold,
 * it sets motor enable to 0 (shutdown), reports a DEM event, and requests
 * BswM transition to SAFE_STOP.
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

uint8 Dio_FlipChannel(uint8 ChannelId)
{
    (void)ChannelId;
    mock_dio_flip_count++;
    return STD_HIGH;
}

/* ====================================================================
 * Overcurrent Monitor Constants
 * ==================================================================== */

#define OVERCURRENT_THRESHOLD_MA  15000u   /* 15A overcurrent threshold */
#define DEM_EVENT_OVERCURRENT     22u      /* DEM event for overcurrent */
#define BSWM_REQUESTER_OC         4u       /* BswM requester for overcurrent */

/* RTE signal IDs used by the monitor */
#define SIG_MOTOR_CURRENT         RTE_SIG_BATTERY_CURRENT  /* Signal ID 6 */
#define SIG_MOTOR_ENABLE          RTE_SIG_MOTOR_STATUS     /* Signal ID 4 */

/* ====================================================================
 * Monitor Runnable: Overcurrent Monitor (defined in test, simulates SWC)
 *
 * Reads motor current from RTE. If above threshold:
 *   1. Set motor enable signal to 0 (shutdown)
 *   2. Report DEM event
 *   3. Request BswM SAFE_STOP
 * ==================================================================== */

static boolean oc_fault_latched;

static void OvercurrentMonitor_Init(void)
{
    oc_fault_latched = FALSE;
}

static void OvercurrentMonitor_Runnable(void)
{
    uint32 current_ma = 0u;

    if (Rte_Read(SIG_MOTOR_CURRENT, &current_ma) != E_OK) {
        return;
    }

    if (current_ma > (uint32)OVERCURRENT_THRESHOLD_MA) {
        /* Immediate motor shutdown */
        (void)Rte_Write(SIG_MOTOR_ENABLE, 0u);

        /* Report DEM event (each call increments debounce counter) */
        Dem_ReportErrorStatus(DEM_EVENT_OVERCURRENT, DEM_EVENT_STATUS_FAILED);

        /* Request safe stop (only attempt once to avoid spamming) */
        if (oc_fault_latched == FALSE) {
            oc_fault_latched = TRUE;
            (void)BswM_RequestMode(BSWM_REQUESTER_OC, BSWM_SAFE_STOP);
        }
    }
}

/* ====================================================================
 * Test Configuration
 * ==================================================================== */

/* RTE signals */
static const Rte_SignalConfigType rte_signals[] = {
    { RTE_SIG_TORQUE_REQUEST,  0u },
    { RTE_SIG_STEERING_ANGLE,  0u },
    { RTE_SIG_VEHICLE_SPEED,   0u },
    { RTE_SIG_BRAKE_PRESSURE,  0u },
    { RTE_SIG_MOTOR_STATUS,    1u },     /* Motor enable: starts enabled */
    { RTE_SIG_BATTERY_VOLTAGE, 0u },
    { RTE_SIG_BATTERY_CURRENT, 0u },     /* Motor current in mA */
    { RTE_SIG_MOTOR_TEMP,      0u },
    { RTE_SIG_ESTOP_STATUS,    0u },
    { RTE_SIG_HEARTBEAT,       0u }
};

#define RTE_SIGNAL_COUNT  10u

static Rte_ConfigType rte_cfg;

/* WdgM config (minimal, not testing WdgM directly) */
static const WdgM_SupervisedEntityConfigType wdgm_se[] = {
    { 0u, 0u, 10u, 3u }
};

static WdgM_ConfigType wdgm_cfg;

/* BswM config (no actions, just mode tracking) */
static BswM_ConfigType bswm_cfg;

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    mock_dio_flip_count = 0u;

    OvercurrentMonitor_Init();

    Dem_Init(NULL_PTR);

    wdgm_cfg.seConfig = wdgm_se;
    wdgm_cfg.seCount  = 1u;
    wdgm_cfg.wdtDioChannel = 5u;
    WdgM_Init(&wdgm_cfg);

    bswm_cfg.ModeActions = NULL_PTR;
    bswm_cfg.ActionCount = 0u;
    BswM_Init(&bswm_cfg);
    (void)BswM_RequestMode(0u, BSWM_RUN);

    rte_cfg.signalConfig   = rte_signals;
    rte_cfg.signalCount    = RTE_SIGNAL_COUNT;
    rte_cfg.runnableConfig = NULL_PTR;
    rte_cfg.runnableCount  = 0u;
    Rte_Init(&rte_cfg);
}

void tearDown(void) { }

/* ====================================================================
 * INT-010-01: Overcurrent detected -> motor enable goes to 0
 * ==================================================================== */

/** @verifies SWR-BSW-026, SWR-BSW-027 */
void test_int_overcurrent_detected_motor_shutdown(void)
{
    uint32 motor_enable = 99u;

    /* Set motor current above threshold */
    (void)Rte_Write(SIG_MOTOR_CURRENT, (uint32)(OVERCURRENT_THRESHOLD_MA + 1u));

    /* Verify motor enable is still 1 before monitor runs */
    (void)Rte_Read(SIG_MOTOR_ENABLE, &motor_enable);
    TEST_ASSERT_EQUAL_UINT32(1u, motor_enable);

    /* Run overcurrent monitor */
    OvercurrentMonitor_Runnable();

    /* Motor enable must be set to 0 */
    (void)Rte_Read(SIG_MOTOR_ENABLE, &motor_enable);
    TEST_ASSERT_EQUAL_UINT32(0u, motor_enable);
}

/* ====================================================================
 * INT-010-02: After overcurrent, verify DEM event stored with CONFIRMED
 * ==================================================================== */

/** @verifies SWR-BSW-018 */
void test_int_overcurrent_dem_event_stored(void)
{
    uint8 status;
    uint8 i;

    /* Set overcurrent */
    (void)Rte_Write(SIG_MOTOR_CURRENT, (uint32)(OVERCURRENT_THRESHOLD_MA + 500u));

    /* Run monitor enough times for DEM debounce to reach CONFIRMED
     * DEM_DEBOUNCE_FAIL_THRESHOLD = 3 */
    for (i = 0u; i < (uint8)DEM_DEBOUNCE_FAIL_THRESHOLD; i++) {
        OvercurrentMonitor_Runnable();
    }

    /* Check DEM event status: CONFIRMED should be set */
    status = 0u;
    (void)Dem_GetEventStatus(DEM_EVENT_OVERCURRENT, &status);
    TEST_ASSERT_BITS(DEM_STATUS_CONFIRMED_DTC, DEM_STATUS_CONFIRMED_DTC, status);
    TEST_ASSERT_BITS(DEM_STATUS_TEST_FAILED, DEM_STATUS_TEST_FAILED, status);
}

/* ====================================================================
 * INT-010-03: Overcurrent -> motor shutdown -> BswM SAFE_STOP
 * ==================================================================== */

/** @verifies SWR-BSW-022 */
void test_int_overcurrent_triggers_safe_stop(void)
{
    /* Set overcurrent */
    (void)Rte_Write(SIG_MOTOR_CURRENT, (uint32)(OVERCURRENT_THRESHOLD_MA + 1000u));

    /* Run monitor */
    OvercurrentMonitor_Runnable();

    /* BswM should be in SAFE_STOP (RUN -> SAFE_STOP is allowed) */
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    /* Motor enable should be 0 */
    {
        uint32 motor_enable = 99u;
        (void)Rte_Read(SIG_MOTOR_ENABLE, &motor_enable);
        TEST_ASSERT_EQUAL_UINT32(0u, motor_enable);
    }
}

/* ====================================================================
 * INT-010-04: Normal current -> no shutdown, no DEM event
 * ==================================================================== */

/** @verifies SWR-BSW-026 */
void test_int_normal_current_no_action(void)
{
    uint8  status;
    uint32 motor_enable;

    /* Set current below threshold */
    (void)Rte_Write(SIG_MOTOR_CURRENT, (uint32)(OVERCURRENT_THRESHOLD_MA - 1000u));

    /* Run monitor multiple times */
    OvercurrentMonitor_Runnable();
    OvercurrentMonitor_Runnable();
    OvercurrentMonitor_Runnable();

    /* Motor enable should still be 1 (initial value) */
    motor_enable = 99u;
    (void)Rte_Read(SIG_MOTOR_ENABLE, &motor_enable);
    TEST_ASSERT_EQUAL_UINT32(1u, motor_enable);

    /* No DEM event should be reported */
    status = 0u;
    (void)Dem_GetEventStatus(DEM_EVENT_OVERCURRENT, &status);
    TEST_ASSERT_EQUAL_HEX8(0x00u, status);

    /* BswM should still be in RUN */
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());
}

/* ====================================================================
 * INT-010-05: Threshold boundary testing
 *             - At exactly threshold: no trigger
 *             - At threshold + 1: triggers
 * ==================================================================== */

/** @verifies SWR-BSW-026, SWR-BSW-027 */
void test_int_overcurrent_threshold_boundary(void)
{
    uint32 motor_enable;

    /* --- At exactly threshold: must NOT trigger --- */
    (void)Rte_Write(SIG_MOTOR_CURRENT, (uint32)OVERCURRENT_THRESHOLD_MA);
    OvercurrentMonitor_Runnable();

    motor_enable = 99u;
    (void)Rte_Read(SIG_MOTOR_ENABLE, &motor_enable);
    TEST_ASSERT_EQUAL_UINT32(1u, motor_enable);
    TEST_ASSERT_EQUAL(BSWM_RUN, BswM_GetCurrentMode());

    /* --- At threshold + 1: MUST trigger --- */
    (void)Rte_Write(SIG_MOTOR_CURRENT, (uint32)(OVERCURRENT_THRESHOLD_MA + 1u));
    OvercurrentMonitor_Runnable();

    motor_enable = 99u;
    (void)Rte_Read(SIG_MOTOR_ENABLE, &motor_enable);
    TEST_ASSERT_EQUAL_UINT32(0u, motor_enable);
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_int_overcurrent_detected_motor_shutdown);
    RUN_TEST(test_int_overcurrent_dem_event_stored);
    RUN_TEST(test_int_overcurrent_triggers_safe_stop);
    RUN_TEST(test_int_normal_current_no_action);
    RUN_TEST(test_int_overcurrent_threshold_boundary);

    return UNITY_END();
}
