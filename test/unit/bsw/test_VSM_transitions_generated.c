/**
 * @file    test_VSM_transitions_generated.c
 * @brief   Exhaustive transition table tests for Vehicle State Machine
 * @date    2026-03-21
 *
 * @verifies SWR-CVC-009, SWR-CVC-010, SWR-CVC-011, SWR-CVC-012, SWR-CVC-013
 *
 * @details Tests EVERY cell in the 6x17 transition table (102 tests total).
 *          For valid transitions: verifies the new state matches the table.
 *          For invalid transitions: verifies state does NOT change.
 *
 *          Note: CVC_EVT_SELF_TEST_PASS in INIT is deferred (guarded by
 *          heartbeat check in MainFunction). OnEvent sets a pending flag
 *          and returns without changing state. This is tested explicitly.
 *
 *          Build pattern: include-the-source. Real AUTOSAR headers are
 *          included first for types/enums, then stubs are defined with
 *          matching signatures, then the .c source is included (header
 *          include guards prevent double-inclusion).
 */

/* ==================================================================
 * 1. Include real AUTOSAR headers — get all types, enums, declarations.
 *    Order matters: Platform_Types -> Std_Types -> ComStack_Types ->
 *    module headers.  Include guards prevent double-inclusion when
 *    Swc_VehicleState.c re-includes them.
 * ================================================================== */

#include "unity.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Dem.h"
#include "BswM.h"
#include "Com.h"
#include "Rte.h"

/* ==================================================================
 * 2. Pre-define CVC application constants that Swc_VehicleState.c needs.
 *    These are normally in Cvc_App.h but the .c file includes Cvc_Cfg.h
 *    (which uses #ifndef guards), so we define them here before the
 *    source is included.
 * ================================================================== */

/* Vehicle state events (from Cvc_App.h) */
#define CVC_EVT_SELF_TEST_PASS      0u
#define CVC_EVT_SELF_TEST_FAIL      1u
#define CVC_EVT_PEDAL_FAULT_SINGLE  2u
#define CVC_EVT_PEDAL_FAULT_DUAL    3u
#define CVC_EVT_CAN_TIMEOUT_SINGLE  4u
#define CVC_EVT_CAN_TIMEOUT_DUAL    5u
#define CVC_EVT_ESTOP               6u
#define CVC_EVT_SC_KILL             7u
#define CVC_EVT_FAULT_CLEARED       8u
#define CVC_EVT_CAN_RESTORED        9u
#define CVC_EVT_VEHICLE_STOPPED    10u
#define CVC_EVT_MOTOR_CUTOFF       11u
#define CVC_EVT_BRAKE_FAULT        12u
#define CVC_EVT_STEERING_FAULT     13u
#define CVC_EVT_BATTERY_WARN       14u
#define CVC_EVT_BATTERY_CRIT       15u
#define CVC_EVT_CREEP_FAULT        16u
#define CVC_EVT_COUNT              17u
#define CVC_STATE_INVALID          0xFFu

/* Fault latching (from Cvc_App.h) */
#define CVC_LATCH_IDX_ESTOP             0u
#define CVC_LATCH_IDX_SC_KILL           1u
#define CVC_LATCH_IDX_MOTOR_CUTOFF      2u
#define CVC_LATCH_IDX_BRAKE             3u
#define CVC_LATCH_IDX_STEERING          4u
#define CVC_LATCH_IDX_PEDAL_DUAL        5u
#define CVC_LATCH_IDX_CAN_DUAL          6u
#define CVC_LATCH_IDX_BATTERY_CRIT      7u
#define CVC_LATCH_IDX_CREEP             8u
#define CVC_LATCH_COUNT                 9u

/* Safe-stop / unlatch (from Cvc_App.h) */
#define CVC_SAFE_STOP_RECOVERY_CYCLES   200u
#define CVC_FAULT_UNLATCH_CYCLES        300u

/* Pedal fault enum (from Cvc_App.h) */
#define CVC_PEDAL_NO_FAULT      0u
#define CVC_PEDAL_PLAUSIBILITY  1u
#define CVC_PEDAL_STUCK         2u

/* DTC IDs — will be re-defined by Cvc_App.h via Cvc_Cfg.h.
 * Only pre-define IDs that Cvc_App.h does NOT define. The rest
 * come from the generated header chain. */

/* ECU (from Cvc_App.h) */
#define CVC_ECU_ID_CVC  0x01u

/* Post-init grace (platform-neutral default for unit tests) */
#ifndef CVC_POST_INIT_GRACE_CYCLES
  #define CVC_POST_INIT_GRACE_CYCLES  300u
#endif

/* Creep guard (from Cvc_App.h) */
#define CVC_CREEP_SPEED_THRESH     50u
#define CVC_CREEP_TORQUE_THRESH    50u
#define CVC_CREEP_DEBOUNCE_TICKS   20u

/* ==================================================================
 * 3. Stub function implementations — signatures MUST match headers.
 *    Defined before #include of .c so they satisfy the linker; header
 *    include guards prevent the .c from re-declaring them.
 * ================================================================== */

/* ---- Mock: RTE ---- */

static uint32 mock_rte_signals[48];
static Std_ReturnType mock_rte_read_return;
static Std_ReturnType mock_rte_write_return;

static Rte_SignalIdType mock_rte_write_last_id;
static uint32 mock_rte_write_last_value;
static uint8  mock_rte_write_call_count;

Std_ReturnType Rte_Read(Rte_SignalIdType SignalId, uint32* DataPtr)
{
    if ((DataPtr == NULL_PTR) || (SignalId >= 48u)) { return E_NOT_OK; }
    *DataPtr = mock_rte_signals[SignalId];
    return mock_rte_read_return;
}

Std_ReturnType Rte_Write(Rte_SignalIdType SignalId, uint32 Data)
{
    if (SignalId >= 48u) { return E_NOT_OK; }
    mock_rte_write_last_id    = SignalId;
    mock_rte_write_last_value = Data;
    mock_rte_write_call_count++;
    return mock_rte_write_return;
}

/* ---- Mock: BswM ---- */

static uint8  mock_bswm_mode;
static uint8  mock_bswm_call_count;

Std_ReturnType BswM_RequestMode(BswM_RequesterIdType RequesterId,
                                 BswM_ModeType RequestedMode)
{
    (void)RequesterId;
    mock_bswm_mode = (uint8)RequestedMode;
    mock_bswm_call_count++;
    return E_OK;
}

/* ---- Mock: Dem ---- */

static Dem_EventIdType mock_dem_last_dtc;
static uint8           mock_dem_last_status;
static uint8           mock_dem_call_count;

void Dem_ReportErrorStatus(Dem_EventIdType EventId,
                            Dem_EventStatusType EventStatus)
{
    mock_dem_last_dtc    = EventId;
    mock_dem_last_status = (uint8)EventStatus;
    mock_dem_call_count++;
}

/* ---- Mock: Com ---- */

static uint32 mock_com_signals[256];

Std_ReturnType Com_ReceiveSignal(Com_SignalIdType SignalId, void* SignalDataPtr)
{
    uint32* DataPtr = (uint32*)SignalDataPtr;
    if ((DataPtr == NULL_PTR) || (SignalId >= 256u)) { return E_NOT_OK; }
    *DataPtr = mock_com_signals[SignalId];
    return E_OK;
}

Std_ReturnType Com_SendSignal(Com_SignalIdType SignalId,
                               const void* SignalDataPtr)
{
    (void)SignalId;
    (void)SignalDataPtr;
    return E_OK;
}

/* ==================================================================
 * 4. Stubs for other functions declared in headers but not needed.
 *    Prevents linker errors from Rte.h pulling in WdgM / IoHwAb.
 * ================================================================== */

/* WdgM stub — Rte.h includes WdgM.h which declares this */
Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEId)
{ (void)SEId; return E_OK; }

/* Heartbeat stub — Swc_VehicleState.c calls this on CAN_RESTORED */
void Swc_Heartbeat_ResetCommStatus(void) {}

/* ==================================================================
 * 5. Include the source under test.
 *    All headers it #includes are already satisfied by include guards.
 * ================================================================== */

#include "../../../firmware/ecu/cvc/src/Swc_VehicleState.c"

/* ==================================================================
 * Test helpers
 * ================================================================== */

static void reset_mocks(void)
{
    uint16 i;
    for (i = 0u; i < 48u; i++) { mock_rte_signals[i] = 0u; }
    for (i = 0u; i < 256u; i++) { mock_com_signals[i] = 0u; }
    mock_rte_read_return  = E_OK;
    mock_rte_write_return = E_OK;
    mock_rte_write_last_id    = 0u;
    mock_rte_write_last_value = 0u;
    mock_rte_write_call_count = 0u;
    mock_bswm_mode       = 0xFFu;
    mock_bswm_call_count = 0u;
    mock_dem_last_dtc    = 0u;
    mock_dem_last_status = 0u;
    mock_dem_call_count  = 0u;
}

/**
 * @brief Force the VSM into a specific state for testing.
 *
 * After Init, state is INIT. To reach other states we chain
 * valid transitions via OnEvent. This helper does that.
 */
static void force_state(uint8 target)
{
    Swc_VehicleState_Init();

    if (target == CVC_STATE_INIT) {
        return;
    }

    /* INIT -> RUN: must go through self-test pass.
     * OnEvent defers SELF_TEST_PASS, so we poke internal state directly. */
    current_state = CVC_STATE_RUN;

    if (target == CVC_STATE_RUN) {
        return;
    }

    if (target == CVC_STATE_DEGRADED) {
        Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_SINGLE);
        return;
    }

    if (target == CVC_STATE_LIMP) {
        Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_CRIT);
        return;
    }

    if (target == CVC_STATE_SAFE_STOP) {
        Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);
        return;
    }

    if (target == CVC_STATE_SHUTDOWN) {
        Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);   /* -> SAFE_STOP */
        Swc_VehicleState_OnEvent(CVC_EVT_SC_KILL);  /* -> SHUTDOWN  */
        return;
    }
}

/* ==================================================================
 * setUp / tearDown
 * ================================================================== */

void setUp(void)
{
    reset_mocks();
    Swc_VehicleState_Init();
}

void tearDown(void) {}

/* ==================================================================
 * Row 0: CVC_STATE_INIT  (17 events)
 * ================================================================== */

void test_INIT_SELF_TEST_PASS_deferred(void)
{
    /* SELF_TEST_PASS in INIT is deferred (pending flag). State stays INIT. */
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_PASS);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_SELF_TEST_FAIL_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_FAIL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_INIT_PEDAL_FAULT_SINGLE_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_PEDAL_FAULT_DUAL_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_CAN_TIMEOUT_SINGLE_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_CAN_TIMEOUT_DUAL_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_ESTOP_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_SC_KILL_to_SHUTDOWN(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_SC_KILL);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_INIT_FAULT_CLEARED_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_FAULT_CLEARED);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_CAN_RESTORED_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_RESTORED);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_VEHICLE_STOPPED_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_VEHICLE_STOPPED);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_MOTOR_CUTOFF_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_MOTOR_CUTOFF);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_BRAKE_FAULT_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_BRAKE_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_STEERING_FAULT_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_STEERING_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_BATTERY_WARN_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_WARN);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_BATTERY_CRIT_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_CRIT);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

void test_INIT_CREEP_FAULT_invalid(void)
{
    force_state(CVC_STATE_INIT);
    Swc_VehicleState_OnEvent(CVC_EVT_CREEP_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

/* ==================================================================
 * Row 1: CVC_STATE_RUN  (17 events)
 * ================================================================== */

void test_RUN_SELF_TEST_PASS_invalid(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_PASS);
    TEST_ASSERT_EQUAL(CVC_STATE_RUN, Swc_VehicleState_GetState());
}

void test_RUN_SELF_TEST_FAIL_invalid(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_FAIL);
    TEST_ASSERT_EQUAL(CVC_STATE_RUN, Swc_VehicleState_GetState());
}

void test_RUN_PEDAL_FAULT_SINGLE_to_DEGRADED(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_RUN_PEDAL_FAULT_DUAL_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_RUN_CAN_TIMEOUT_SINGLE_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_RUN_CAN_TIMEOUT_DUAL_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_RUN_ESTOP_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_RUN_SC_KILL_to_SHUTDOWN(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_SC_KILL);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_RUN_FAULT_CLEARED_invalid(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_FAULT_CLEARED);
    TEST_ASSERT_EQUAL(CVC_STATE_RUN, Swc_VehicleState_GetState());
}

void test_RUN_CAN_RESTORED_invalid(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_RESTORED);
    TEST_ASSERT_EQUAL(CVC_STATE_RUN, Swc_VehicleState_GetState());
}

void test_RUN_VEHICLE_STOPPED_invalid(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_VEHICLE_STOPPED);
    TEST_ASSERT_EQUAL(CVC_STATE_RUN, Swc_VehicleState_GetState());
}

void test_RUN_MOTOR_CUTOFF_to_DEGRADED(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_MOTOR_CUTOFF);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_RUN_BRAKE_FAULT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_BRAKE_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_RUN_STEERING_FAULT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_STEERING_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_RUN_BATTERY_WARN_to_DEGRADED(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_WARN);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_RUN_BATTERY_CRIT_to_LIMP(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_CRIT);
    TEST_ASSERT_EQUAL(CVC_STATE_LIMP, Swc_VehicleState_GetState());
}

void test_RUN_CREEP_FAULT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_CREEP_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

/* ==================================================================
 * Row 2: CVC_STATE_DEGRADED  (17 events)
 * ================================================================== */

void test_DEGRADED_SELF_TEST_PASS_invalid(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_PASS);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_DEGRADED_SELF_TEST_FAIL_invalid(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_FAIL);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_DEGRADED_PEDAL_FAULT_SINGLE_invalid(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_DEGRADED_PEDAL_FAULT_DUAL_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_DEGRADED_CAN_TIMEOUT_SINGLE_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_DEGRADED_CAN_TIMEOUT_DUAL_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_DEGRADED_ESTOP_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_DEGRADED_SC_KILL_to_SHUTDOWN(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_SC_KILL);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_DEGRADED_FAULT_CLEARED_to_RUN(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_FAULT_CLEARED);
    TEST_ASSERT_EQUAL(CVC_STATE_RUN, Swc_VehicleState_GetState());
}

void test_DEGRADED_CAN_RESTORED_invalid(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_RESTORED);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_DEGRADED_VEHICLE_STOPPED_invalid(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_VEHICLE_STOPPED);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_DEGRADED_MOTOR_CUTOFF_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_MOTOR_CUTOFF);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_DEGRADED_BRAKE_FAULT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_BRAKE_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_DEGRADED_STEERING_FAULT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_STEERING_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_DEGRADED_BATTERY_WARN_invalid(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_WARN);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_DEGRADED_BATTERY_CRIT_to_LIMP(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_CRIT);
    TEST_ASSERT_EQUAL(CVC_STATE_LIMP, Swc_VehicleState_GetState());
}

void test_DEGRADED_CREEP_FAULT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_DEGRADED);
    Swc_VehicleState_OnEvent(CVC_EVT_CREEP_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

/* ==================================================================
 * Row 3: CVC_STATE_LIMP  (17 events)
 * ================================================================== */

void test_LIMP_SELF_TEST_PASS_invalid(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_PASS);
    TEST_ASSERT_EQUAL(CVC_STATE_LIMP, Swc_VehicleState_GetState());
}

void test_LIMP_SELF_TEST_FAIL_invalid(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_FAIL);
    TEST_ASSERT_EQUAL(CVC_STATE_LIMP, Swc_VehicleState_GetState());
}

void test_LIMP_PEDAL_FAULT_SINGLE_invalid(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_LIMP, Swc_VehicleState_GetState());
}

void test_LIMP_PEDAL_FAULT_DUAL_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_LIMP_CAN_TIMEOUT_SINGLE_invalid(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_LIMP, Swc_VehicleState_GetState());
}

void test_LIMP_CAN_TIMEOUT_DUAL_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_LIMP_ESTOP_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_LIMP_SC_KILL_to_SHUTDOWN(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_SC_KILL);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_LIMP_FAULT_CLEARED_to_RUN(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_FAULT_CLEARED);
    TEST_ASSERT_EQUAL(CVC_STATE_RUN, Swc_VehicleState_GetState());
}

void test_LIMP_CAN_RESTORED_to_DEGRADED(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_RESTORED);
    TEST_ASSERT_EQUAL(CVC_STATE_DEGRADED, Swc_VehicleState_GetState());
}

void test_LIMP_VEHICLE_STOPPED_invalid(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_VEHICLE_STOPPED);
    TEST_ASSERT_EQUAL(CVC_STATE_LIMP, Swc_VehicleState_GetState());
}

void test_LIMP_MOTOR_CUTOFF_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_MOTOR_CUTOFF);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_LIMP_BRAKE_FAULT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_BRAKE_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_LIMP_STEERING_FAULT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_STEERING_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_LIMP_BATTERY_WARN_invalid(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_WARN);
    TEST_ASSERT_EQUAL(CVC_STATE_LIMP, Swc_VehicleState_GetState());
}

void test_LIMP_BATTERY_CRIT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_CRIT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_LIMP_CREEP_FAULT_to_SAFE_STOP(void)
{
    force_state(CVC_STATE_LIMP);
    Swc_VehicleState_OnEvent(CVC_EVT_CREEP_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

/* ==================================================================
 * Row 4: CVC_STATE_SAFE_STOP  (17 events)
 * ================================================================== */

void test_SAFE_STOP_SELF_TEST_PASS_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_PASS);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_SELF_TEST_FAIL_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_FAIL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_PEDAL_FAULT_SINGLE_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_PEDAL_FAULT_DUAL_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_CAN_TIMEOUT_SINGLE_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_CAN_TIMEOUT_DUAL_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_ESTOP_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_SC_KILL_to_SHUTDOWN(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_SC_KILL);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_FAULT_CLEARED_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_FAULT_CLEARED);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_CAN_RESTORED_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_RESTORED);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_VEHICLE_STOPPED_to_SHUTDOWN(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_VEHICLE_STOPPED);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_MOTOR_CUTOFF_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_MOTOR_CUTOFF);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_BRAKE_FAULT_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_BRAKE_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_STEERING_FAULT_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_STEERING_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_BATTERY_WARN_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_WARN);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_BATTERY_CRIT_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_CRIT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

void test_SAFE_STOP_CREEP_FAULT_invalid(void)
{
    force_state(CVC_STATE_SAFE_STOP);
    Swc_VehicleState_OnEvent(CVC_EVT_CREEP_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
}

/* ==================================================================
 * Row 5: CVC_STATE_SHUTDOWN  (17 events — ALL invalid, terminal state)
 * ================================================================== */

void test_SHUTDOWN_SELF_TEST_PASS_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_PASS);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_SELF_TEST_FAIL_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_FAIL);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_PEDAL_FAULT_SINGLE_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_PEDAL_FAULT_DUAL_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_CAN_TIMEOUT_SINGLE_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_CAN_TIMEOUT_DUAL_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_TIMEOUT_DUAL);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_ESTOP_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_SC_KILL_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_SC_KILL);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_FAULT_CLEARED_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_FAULT_CLEARED);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_CAN_RESTORED_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_CAN_RESTORED);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_VEHICLE_STOPPED_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_VEHICLE_STOPPED);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_MOTOR_CUTOFF_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_MOTOR_CUTOFF);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_BRAKE_FAULT_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_BRAKE_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_STEERING_FAULT_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_STEERING_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_BATTERY_WARN_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_WARN);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_BATTERY_CRIT_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_BATTERY_CRIT);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

void test_SHUTDOWN_CREEP_FAULT_invalid(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_OnEvent(CVC_EVT_CREEP_FAULT);
    TEST_ASSERT_EQUAL(CVC_STATE_SHUTDOWN, Swc_VehicleState_GetState());
}

/* ==================================================================
 * Defensive / Edge case tests
 * ================================================================== */

void test_OnEvent_before_init_ignored(void)
{
    /* Reset initialized flag by re-declaring (include pattern) */
    initialized = FALSE;
    Swc_VehicleState_OnEvent(CVC_EVT_SELF_TEST_FAIL);
    /* State should not change — function returns early */
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, current_state);
}

void test_OnEvent_out_of_range_event_ignored(void)
{
    force_state(CVC_STATE_RUN);
    Swc_VehicleState_OnEvent(CVC_EVT_COUNT);      /* one past last */
    TEST_ASSERT_EQUAL(CVC_STATE_RUN, Swc_VehicleState_GetState());
    Swc_VehicleState_OnEvent(0xFFu);               /* way out of range */
    TEST_ASSERT_EQUAL(CVC_STATE_RUN, Swc_VehicleState_GetState());
}

void test_BswM_notified_on_valid_transition(void)
{
    force_state(CVC_STATE_RUN);
    mock_bswm_call_count = 0u;
    Swc_VehicleState_OnEvent(CVC_EVT_ESTOP);
    TEST_ASSERT_EQUAL(CVC_STATE_SAFE_STOP, Swc_VehicleState_GetState());
    TEST_ASSERT_EQUAL(BSWM_SAFE_STOP, mock_bswm_mode);
    TEST_ASSERT_TRUE(mock_bswm_call_count > 0u);
}

void test_BswM_not_notified_on_invalid_transition(void)
{
    force_state(CVC_STATE_INIT);
    mock_bswm_call_count = 0u;
    Swc_VehicleState_OnEvent(CVC_EVT_PEDAL_FAULT_SINGLE);
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
    TEST_ASSERT_EQUAL(0u, mock_bswm_call_count);
}

void test_Init_resets_to_INIT_state(void)
{
    force_state(CVC_STATE_SHUTDOWN);
    Swc_VehicleState_Init();
    TEST_ASSERT_EQUAL(CVC_STATE_INIT, Swc_VehicleState_GetState());
}

/* ==================================================================
 * Unity test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* Row 0: INIT (17 tests) */
    RUN_TEST(test_INIT_SELF_TEST_PASS_deferred);
    RUN_TEST(test_INIT_SELF_TEST_FAIL_to_SAFE_STOP);
    RUN_TEST(test_INIT_PEDAL_FAULT_SINGLE_invalid);
    RUN_TEST(test_INIT_PEDAL_FAULT_DUAL_invalid);
    RUN_TEST(test_INIT_CAN_TIMEOUT_SINGLE_invalid);
    RUN_TEST(test_INIT_CAN_TIMEOUT_DUAL_invalid);
    RUN_TEST(test_INIT_ESTOP_invalid);
    RUN_TEST(test_INIT_SC_KILL_to_SHUTDOWN);
    RUN_TEST(test_INIT_FAULT_CLEARED_invalid);
    RUN_TEST(test_INIT_CAN_RESTORED_invalid);
    RUN_TEST(test_INIT_VEHICLE_STOPPED_invalid);
    RUN_TEST(test_INIT_MOTOR_CUTOFF_invalid);
    RUN_TEST(test_INIT_BRAKE_FAULT_invalid);
    RUN_TEST(test_INIT_STEERING_FAULT_invalid);
    RUN_TEST(test_INIT_BATTERY_WARN_invalid);
    RUN_TEST(test_INIT_BATTERY_CRIT_invalid);
    RUN_TEST(test_INIT_CREEP_FAULT_invalid);

    /* Row 1: RUN (17 tests) */
    RUN_TEST(test_RUN_SELF_TEST_PASS_invalid);
    RUN_TEST(test_RUN_SELF_TEST_FAIL_invalid);
    RUN_TEST(test_RUN_PEDAL_FAULT_SINGLE_to_DEGRADED);
    RUN_TEST(test_RUN_PEDAL_FAULT_DUAL_to_SAFE_STOP);
    RUN_TEST(test_RUN_CAN_TIMEOUT_SINGLE_to_SAFE_STOP);
    RUN_TEST(test_RUN_CAN_TIMEOUT_DUAL_to_SAFE_STOP);
    RUN_TEST(test_RUN_ESTOP_to_SAFE_STOP);
    RUN_TEST(test_RUN_SC_KILL_to_SHUTDOWN);
    RUN_TEST(test_RUN_FAULT_CLEARED_invalid);
    RUN_TEST(test_RUN_CAN_RESTORED_invalid);
    RUN_TEST(test_RUN_VEHICLE_STOPPED_invalid);
    RUN_TEST(test_RUN_MOTOR_CUTOFF_to_DEGRADED);
    RUN_TEST(test_RUN_BRAKE_FAULT_to_SAFE_STOP);
    RUN_TEST(test_RUN_STEERING_FAULT_to_SAFE_STOP);
    RUN_TEST(test_RUN_BATTERY_WARN_to_DEGRADED);
    RUN_TEST(test_RUN_BATTERY_CRIT_to_LIMP);
    RUN_TEST(test_RUN_CREEP_FAULT_to_SAFE_STOP);

    /* Row 2: DEGRADED (17 tests) */
    RUN_TEST(test_DEGRADED_SELF_TEST_PASS_invalid);
    RUN_TEST(test_DEGRADED_SELF_TEST_FAIL_invalid);
    RUN_TEST(test_DEGRADED_PEDAL_FAULT_SINGLE_invalid);
    RUN_TEST(test_DEGRADED_PEDAL_FAULT_DUAL_to_SAFE_STOP);
    RUN_TEST(test_DEGRADED_CAN_TIMEOUT_SINGLE_to_SAFE_STOP);
    RUN_TEST(test_DEGRADED_CAN_TIMEOUT_DUAL_to_SAFE_STOP);
    RUN_TEST(test_DEGRADED_ESTOP_to_SAFE_STOP);
    RUN_TEST(test_DEGRADED_SC_KILL_to_SHUTDOWN);
    RUN_TEST(test_DEGRADED_FAULT_CLEARED_to_RUN);
    RUN_TEST(test_DEGRADED_CAN_RESTORED_invalid);
    RUN_TEST(test_DEGRADED_VEHICLE_STOPPED_invalid);
    RUN_TEST(test_DEGRADED_MOTOR_CUTOFF_to_SAFE_STOP);
    RUN_TEST(test_DEGRADED_BRAKE_FAULT_to_SAFE_STOP);
    RUN_TEST(test_DEGRADED_STEERING_FAULT_to_SAFE_STOP);
    RUN_TEST(test_DEGRADED_BATTERY_WARN_invalid);
    RUN_TEST(test_DEGRADED_BATTERY_CRIT_to_LIMP);
    RUN_TEST(test_DEGRADED_CREEP_FAULT_to_SAFE_STOP);

    /* Row 3: LIMP (17 tests) */
    RUN_TEST(test_LIMP_SELF_TEST_PASS_invalid);
    RUN_TEST(test_LIMP_SELF_TEST_FAIL_invalid);
    RUN_TEST(test_LIMP_PEDAL_FAULT_SINGLE_invalid);
    RUN_TEST(test_LIMP_PEDAL_FAULT_DUAL_to_SAFE_STOP);
    RUN_TEST(test_LIMP_CAN_TIMEOUT_SINGLE_invalid);
    RUN_TEST(test_LIMP_CAN_TIMEOUT_DUAL_to_SAFE_STOP);
    RUN_TEST(test_LIMP_ESTOP_to_SAFE_STOP);
    RUN_TEST(test_LIMP_SC_KILL_to_SHUTDOWN);
    RUN_TEST(test_LIMP_FAULT_CLEARED_to_RUN);
    RUN_TEST(test_LIMP_CAN_RESTORED_to_DEGRADED);
    RUN_TEST(test_LIMP_VEHICLE_STOPPED_invalid);
    RUN_TEST(test_LIMP_MOTOR_CUTOFF_to_SAFE_STOP);
    RUN_TEST(test_LIMP_BRAKE_FAULT_to_SAFE_STOP);
    RUN_TEST(test_LIMP_STEERING_FAULT_to_SAFE_STOP);
    RUN_TEST(test_LIMP_BATTERY_WARN_invalid);
    RUN_TEST(test_LIMP_BATTERY_CRIT_to_SAFE_STOP);
    RUN_TEST(test_LIMP_CREEP_FAULT_to_SAFE_STOP);

    /* Row 4: SAFE_STOP (17 tests) */
    RUN_TEST(test_SAFE_STOP_SELF_TEST_PASS_invalid);
    RUN_TEST(test_SAFE_STOP_SELF_TEST_FAIL_invalid);
    RUN_TEST(test_SAFE_STOP_PEDAL_FAULT_SINGLE_invalid);
    RUN_TEST(test_SAFE_STOP_PEDAL_FAULT_DUAL_invalid);
    RUN_TEST(test_SAFE_STOP_CAN_TIMEOUT_SINGLE_invalid);
    RUN_TEST(test_SAFE_STOP_CAN_TIMEOUT_DUAL_invalid);
    RUN_TEST(test_SAFE_STOP_ESTOP_invalid);
    RUN_TEST(test_SAFE_STOP_SC_KILL_to_SHUTDOWN);
    RUN_TEST(test_SAFE_STOP_FAULT_CLEARED_invalid);
    RUN_TEST(test_SAFE_STOP_CAN_RESTORED_invalid);
    RUN_TEST(test_SAFE_STOP_VEHICLE_STOPPED_to_SHUTDOWN);
    RUN_TEST(test_SAFE_STOP_MOTOR_CUTOFF_invalid);
    RUN_TEST(test_SAFE_STOP_BRAKE_FAULT_invalid);
    RUN_TEST(test_SAFE_STOP_STEERING_FAULT_invalid);
    RUN_TEST(test_SAFE_STOP_BATTERY_WARN_invalid);
    RUN_TEST(test_SAFE_STOP_BATTERY_CRIT_invalid);
    RUN_TEST(test_SAFE_STOP_CREEP_FAULT_invalid);

    /* Row 5: SHUTDOWN (17 tests) */
    RUN_TEST(test_SHUTDOWN_SELF_TEST_PASS_invalid);
    RUN_TEST(test_SHUTDOWN_SELF_TEST_FAIL_invalid);
    RUN_TEST(test_SHUTDOWN_PEDAL_FAULT_SINGLE_invalid);
    RUN_TEST(test_SHUTDOWN_PEDAL_FAULT_DUAL_invalid);
    RUN_TEST(test_SHUTDOWN_CAN_TIMEOUT_SINGLE_invalid);
    RUN_TEST(test_SHUTDOWN_CAN_TIMEOUT_DUAL_invalid);
    RUN_TEST(test_SHUTDOWN_ESTOP_invalid);
    RUN_TEST(test_SHUTDOWN_SC_KILL_invalid);
    RUN_TEST(test_SHUTDOWN_FAULT_CLEARED_invalid);
    RUN_TEST(test_SHUTDOWN_CAN_RESTORED_invalid);
    RUN_TEST(test_SHUTDOWN_VEHICLE_STOPPED_invalid);
    RUN_TEST(test_SHUTDOWN_MOTOR_CUTOFF_invalid);
    RUN_TEST(test_SHUTDOWN_BRAKE_FAULT_invalid);
    RUN_TEST(test_SHUTDOWN_STEERING_FAULT_invalid);
    RUN_TEST(test_SHUTDOWN_BATTERY_WARN_invalid);
    RUN_TEST(test_SHUTDOWN_BATTERY_CRIT_invalid);
    RUN_TEST(test_SHUTDOWN_CREEP_FAULT_invalid);

    /* Defensive / edge cases (5 tests) */
    RUN_TEST(test_OnEvent_before_init_ignored);
    RUN_TEST(test_OnEvent_out_of_range_event_ignored);
    RUN_TEST(test_BswM_notified_on_valid_transition);
    RUN_TEST(test_BswM_not_notified_on_invalid_transition);
    RUN_TEST(test_Init_resets_to_INIT_state);

    return UNITY_END();
}
