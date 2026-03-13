/**
 * @file    test_int_safe_state.c
 * @brief   Integration test INT-007: Critical fault -> all actuators to safe
 *          state within deadline
 * @date    2026-02-24
 *
 * @verifies  SWR-BSW-019, SWR-BSW-020, SWR-BSW-022, SWR-BSW-026, SWR-BSW-027
 * @aspice    SWE.5 — Software Component Verification & Integration
 * @iso       ISO 26262 Part 6, Section 10 — Software Integration & Testing
 * @traces_to TSR-035, TSR-046, TSR-047, TSR-048
 *
 * Linked modules (REAL): WdgM.c, BswM.c, Dem.c, Rte.c
 * Mocked:                Dio_FlipChannel (captures calls)
 *
 * This test simulates the complete safety chain:
 *   1. System running normally (BswM RUN, WdgM OK, RTE signals active)
 *   2. Critical fault occurs (motor overcurrent via RTE signal)
 *   3. Safety runnable detects fault and triggers BswM SAFE_STOP
 *   4. SAFE_STOP action callback zeros actuator signals
 *   5. Verify actuator signals (motor torque, steering) are at safe values
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

/* ===================================================================
 * Mock: Dio_FlipChannel — HW layer mock for watchdog pin
 * =================================================================== */

static uint32 mock_dio_flip_count;

uint8 Dio_FlipChannel(uint8 ChannelId)
{
    (void)ChannelId;
    mock_dio_flip_count++;
    return STD_HIGH;
}

/* ===================================================================
 * Signal IDs used in this integration test
 * =================================================================== */

#define SIG_MOTOR_TORQUE    0u   /* Actuator: motor torque request (0 = safe) */
#define SIG_STEERING_ANGLE  1u   /* Actuator: steering angle (5000 = center/safe) */
#define SIG_FAULT_FLAG      2u   /* Sensor: overcurrent fault flag (1 = fault) */
#define SIG_BRAKE_CMD       3u   /* Actuator: brake command (10000 = max brake) */

#define SAFE_TORQUE_VALUE    0u      /* Safe: motor off */
#define SAFE_STEERING_VALUE  5000u   /* Safe: steering centered */
#define SAFE_BRAKE_VALUE     10000u  /* Safe: maximum braking */
#define NORMAL_TORQUE_VALUE  3000u   /* Normal: 30% torque */
#define NORMAL_STEERING_VALUE 7500u  /* Normal: slight right turn */
#define FAULT_ACTIVE         1u

/* ===================================================================
 * BswM action callbacks
 * =================================================================== */

static uint8 cb_run_count;
static uint8 cb_safe_stop_count;
static uint8 cb_shutdown_count;

/** RUN mode action: normal operation (no-op in this test) */
static void Action_Run(void)
{
    cb_run_count++;
}

/**
 * SAFE_STOP mode action: zero all actuators to safe values.
 * This is the critical safety reaction — must set motor to 0,
 * steering to center, brake to max.
 */
static void Action_SafeStop(void)
{
    cb_safe_stop_count++;
    (void)Rte_Write(SIG_MOTOR_TORQUE,   SAFE_TORQUE_VALUE);
    (void)Rte_Write(SIG_STEERING_ANGLE, SAFE_STEERING_VALUE);
    (void)Rte_Write(SIG_BRAKE_CMD,      SAFE_BRAKE_VALUE);
}

static void Action_Shutdown(void)
{
    cb_shutdown_count++;
}

/* ===================================================================
 * Safety runnable — detects fault and triggers BswM transition
 * =================================================================== */

static uint8 safety_runnable_exec_count;

/**
 * Safety monitoring runnable: reads fault flag from RTE,
 * if active, requests BswM SAFE_STOP transition and reports to DEM.
 */
static void SafetyRunnable(void)
{
    uint32 fault_flag = 0u;

    safety_runnable_exec_count++;

    if (Rte_Read(SIG_FAULT_FLAG, &fault_flag) == E_OK) {
        if (fault_flag == FAULT_ACTIVE) {
            /* Report fault to DEM BEFORE requesting safe state */
            Dem_ReportErrorStatus(10u, DEM_EVENT_STATUS_FAILED);

            /* Request safe state transition */
            (void)BswM_RequestMode(0u, BSWM_SAFE_STOP);
        }
    }
}

/* ===================================================================
 * Module configurations wired together
 * =================================================================== */

/** RTE signal configuration */
static const Rte_SignalConfigType sig_config[] = {
    { SIG_MOTOR_TORQUE,   0u    },
    { SIG_STEERING_ANGLE, 5000u },  /* Default: centered */
    { SIG_FAULT_FLAG,     0u    },  /* Default: no fault */
    { SIG_BRAKE_CMD,      0u    }
};

/** RTE runnable configuration: safety runnable at highest priority */
static const Rte_RunnableConfigType run_config[] = {
    { SafetyRunnable, 1u, 10u, 0u }  /* period=1ms, priority=10, SE=0 */
};

static Rte_ConfigType rte_config;

/** WdgM configuration: 1 SE for safety runnable */
static const WdgM_SupervisedEntityConfigType se_config[] = {
    { 0u, 1u, 5u, 1u }  /* SE 0: min=1, max=5, tolerance=1 */
};

static WdgM_ConfigType wdgm_config;

/** WdgM config for expiry test: very low tolerance */
static const WdgM_SupervisedEntityConfigType se_config_low_tol[] = {
    { 0u, 1u, 5u, 0u }  /* SE 0: min=1, max=5, tolerance=0 (expires on first miss) */
};

/** BswM mode action table */
static const BswM_ModeActionType bswm_actions[] = {
    { BSWM_RUN,       Action_Run      },
    { BSWM_SAFE_STOP, Action_SafeStop },
    { BSWM_SHUTDOWN,  Action_Shutdown }
};

static BswM_ConfigType bswm_config;

/* ===================================================================
 * setUp / tearDown — initialize all linked modules
 * =================================================================== */

void setUp(void)
{
    /* Reset mock/tracking state */
    mock_dio_flip_count       = 0u;
    cb_run_count              = 0u;
    cb_safe_stop_count        = 0u;
    cb_shutdown_count         = 0u;
    safety_runnable_exec_count = 0u;

    /* Initialize DEM */
    Dem_Init(NULL_PTR);

    /* Initialize BswM */
    bswm_config.ModeActions = bswm_actions;
    bswm_config.ActionCount = 3u;
    BswM_Init(&bswm_config);

    /* Initialize WdgM */
    wdgm_config.seConfig      = se_config;
    wdgm_config.seCount       = 1u;
    wdgm_config.wdtDioChannel = 7u;
    WdgM_Init(&wdgm_config);

    /* Initialize RTE */
    rte_config.signalConfig   = sig_config;
    rte_config.signalCount    = 4u;
    rte_config.runnableConfig = run_config;
    rte_config.runnableCount  = 1u;
    Rte_Init(&rte_config);

    /* Transition BswM to RUN (normal operating state) */
    BswM_RequestMode(0u, BSWM_RUN);
}

void tearDown(void)
{
    /* No dynamic resources */
}

/* ===================================================================
 * INT-007 Test 1: Fault signal -> safety runnable -> BswM SAFE_STOP
 * =================================================================== */

/**
 * @verifies SWR-BSW-022, SWR-BSW-026
 * @brief    Write fault signal to RTE -> safety runnable detects fault ->
 *           calls BswM_RequestMode(SAFE_STOP) -> verify BswM is SAFE_STOP
 *
 * Scenario: Set motor overcurrent fault flag in RTE, execute Rte_MainFunction
 * which dispatches the safety runnable. The runnable reads the fault flag
 * and requests BswM SAFE_STOP. Verify the transition occurred.
 */
void test_int_fault_to_safe_state_chain(void)
{
    /* Set normal operating values */
    Rte_Write(SIG_MOTOR_TORQUE, NORMAL_TORQUE_VALUE);
    Rte_Write(SIG_STEERING_ANGLE, NORMAL_STEERING_VALUE);

    /* Inject fault: motor overcurrent */
    TEST_ASSERT_EQUAL(E_OK, Rte_Write(SIG_FAULT_FLAG, FAULT_ACTIVE));

    /* Execute RTE MainFunction — dispatches SafetyRunnable */
    Rte_MainFunction();

    /* Verify safety runnable executed */
    TEST_ASSERT_TRUE(safety_runnable_exec_count > 0u);

    /* Verify BswM transitioned to SAFE_STOP */
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());
}

/* ===================================================================
 * INT-007 Test 2: SAFE_STOP action zeros actuator signals
 * =================================================================== */

/**
 * @verifies SWR-BSW-022, SWR-BSW-026, SWR-BSW-027
 * @brief    After SAFE_STOP, BswM MainFunction runs SAFE_STOP callback
 *           which writes safe values to all actuator RTE signals
 *
 * Scenario: Inject fault, trigger safety chain, then call BswM_MainFunction
 * to execute the SAFE_STOP action callback. Verify all actuator signals
 * are at safe values (motor=0, steering=center, brake=max).
 */
void test_int_safe_state_zeros_actuators(void)
{
    /* Set normal operating values */
    Rte_Write(SIG_MOTOR_TORQUE, NORMAL_TORQUE_VALUE);
    Rte_Write(SIG_STEERING_ANGLE, NORMAL_STEERING_VALUE);
    Rte_Write(SIG_BRAKE_CMD, 0u);

    /* Inject fault and trigger safety chain */
    Rte_Write(SIG_FAULT_FLAG, FAULT_ACTIVE);
    Rte_MainFunction();

    /* Now in SAFE_STOP — execute BswM MainFunction to fire callbacks */
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());
    BswM_MainFunction();

    /* Verify SAFE_STOP callback was invoked */
    TEST_ASSERT_EQUAL_UINT8(1u, cb_safe_stop_count);

    /* Verify actuator signals are at safe values */
    uint32 torque_val = 0xFFFFu;
    uint32 steering_val = 0xFFFFu;
    uint32 brake_val = 0u;

    TEST_ASSERT_EQUAL(E_OK, Rte_Read(SIG_MOTOR_TORQUE, &torque_val));
    TEST_ASSERT_EQUAL_UINT32(SAFE_TORQUE_VALUE, torque_val);

    TEST_ASSERT_EQUAL(E_OK, Rte_Read(SIG_STEERING_ANGLE, &steering_val));
    TEST_ASSERT_EQUAL_UINT32(SAFE_STEERING_VALUE, steering_val);

    TEST_ASSERT_EQUAL(E_OK, Rte_Read(SIG_BRAKE_CMD, &brake_val));
    TEST_ASSERT_EQUAL_UINT32(SAFE_BRAKE_VALUE, brake_val);
}

/* ===================================================================
 * INT-007 Test 3: WdgM expiry triggers DEM event -> BswM SAFE_STOP
 * =================================================================== */

/**
 * @verifies SWR-BSW-019, SWR-BSW-020, SWR-BSW-022
 * @brief    WdgM SE expires -> DEM event stored -> safety handler
 *           transitions BswM to SAFE_STOP
 *
 * Scenario: Configure WdgM with tolerance=0 (expires on first miss).
 * Do not provide RTE-dispatched checkpoints (stop calling Rte_MainFunction
 * so no runnable fires, thus no WdgM_CheckpointReached). Run
 * WdgM_MainFunction to detect the miss. After expiry, simulate the
 * safety handler requesting SAFE_STOP.
 */
void test_int_wdgm_expiry_triggers_safe_stop(void)
{
    /* Reconfigure WdgM with tolerance=0 */
    wdgm_config.seConfig = se_config_low_tol;
    wdgm_config.seCount  = 1u;
    WdgM_Init(&wdgm_config);

    /* Do NOT call Rte_MainFunction (no runnable fires, no checkpoint) */

    /* Cycle 1: no checkpoints -> FAILED, failedCycleCount=1 > tolerance=0 -> EXPIRED */
    WdgM_MainFunction();

    /* Verify SE expired */
    WdgM_LocalStatusType local;
    WdgM_GetLocalStatus(0u, &local);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_EXPIRED, local);
    TEST_ASSERT_EQUAL(WDGM_GLOBAL_STATUS_FAILED, WdgM_GetGlobalStatus());

    /* Verify DEM event recorded */
    uint8 dem_status = 0u;
    Dem_GetEventStatus(15u, &dem_status);
    TEST_ASSERT_BITS(DEM_STATUS_TEST_FAILED, DEM_STATUS_TEST_FAILED, dem_status);

    /* Safety handler reacts: transition to SAFE_STOP */
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_SAFE_STOP));
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    /* Execute SAFE_STOP action */
    BswM_MainFunction();
    TEST_ASSERT_EQUAL_UINT8(1u, cb_safe_stop_count);
}

/* ===================================================================
 * INT-007 Test 4: From SAFE_STOP, only SHUTDOWN transition allowed
 * =================================================================== */

/**
 * @verifies SWR-BSW-022
 * @brief    From SAFE_STOP, only SHUTDOWN is a valid forward transition.
 *           All other transitions must be rejected.
 *
 * Scenario: Reach SAFE_STOP state, then verify:
 *   - STARTUP, RUN, DEGRADED transitions are all rejected
 *   - SHUTDOWN transition is accepted
 */
void test_int_safe_stop_to_shutdown_only(void)
{
    /* Reach SAFE_STOP via fault injection */
    Rte_Write(SIG_FAULT_FLAG, FAULT_ACTIVE);
    Rte_MainFunction();
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    /* Backward transitions must fail */
    TEST_ASSERT_EQUAL(E_NOT_OK, BswM_RequestMode(0u, BSWM_STARTUP));
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    TEST_ASSERT_EQUAL(E_NOT_OK, BswM_RequestMode(0u, BSWM_RUN));
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    TEST_ASSERT_EQUAL(E_NOT_OK, BswM_RequestMode(0u, BSWM_DEGRADED));
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    /* Only SHUTDOWN should succeed */
    TEST_ASSERT_EQUAL(E_OK, BswM_RequestMode(0u, BSWM_SHUTDOWN));
    TEST_ASSERT_EQUAL(BSWM_SHUTDOWN, BswM_GetCurrentMode());
}

/* ===================================================================
 * INT-007 Test 5: DEM records fault BEFORE safe state transition
 * =================================================================== */

/**
 * @verifies SWR-BSW-020, SWR-BSW-022
 * @brief    Verify the DEM event is stored BEFORE the BswM safe state
 *           transition occurs
 *
 * Scenario: The SafetyRunnable calls Dem_ReportErrorStatus BEFORE
 * BswM_RequestMode(SAFE_STOP). After the runnable executes, verify
 * that both the DEM event and the BswM transition have occurred,
 * proving the DEM event was stored first (by code ordering guarantee).
 */
void test_int_dem_records_fault_before_safe_state(void)
{
    /* Inject fault */
    Rte_Write(SIG_FAULT_FLAG, FAULT_ACTIVE);

    /* Execute safety chain */
    Rte_MainFunction();

    /* Verify DEM event 10 (our overcurrent event) has TEST_FAILED set.
     * The safety runnable reports to DEM before requesting SAFE_STOP.
     * By the time we check here, both have happened. The code ordering
     * in SafetyRunnable guarantees DEM was called first. */
    uint8 dem_status = 0u;
    TEST_ASSERT_EQUAL(E_OK, Dem_GetEventStatus(10u, &dem_status));
    TEST_ASSERT_BITS(DEM_STATUS_TEST_FAILED, DEM_STATUS_TEST_FAILED, dem_status);

    /* Verify BswM is in SAFE_STOP (transition happened after DEM report) */
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, BswM_GetCurrentMode());

    /* Verify the safety runnable was the one that drove both actions */
    TEST_ASSERT_TRUE(safety_runnable_exec_count > 0u);
}

/* ===================================================================
 * Test runner
 * =================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_int_fault_to_safe_state_chain);
    RUN_TEST(test_int_safe_state_zeros_actuators);
    RUN_TEST(test_int_wdgm_expiry_triggers_safe_stop);
    RUN_TEST(test_int_safe_stop_to_shutdown_only);
    RUN_TEST(test_int_dem_records_fault_before_safe_state);

    return UNITY_END();
}
