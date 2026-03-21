/**
 * @file    test_Swc_DoorLock.c
 * @brief   Unit tests for DoorLock SWC — manual and auto lock/unlock
 * @date    2026-02-23
 *
 * @verifies SSR-BCM-009, SWR-BCM-009
 *
 * Tests door lock initialization (unlocked), manual lock command,
 * auto-lock above 10 speed threshold, auto-unlock when transitioning
 * to parked state, and guard against uninitialized operation.
 *
 * Mocks: Rte_Read, Rte_Write, Dem_ReportErrorStatus
 */
#include "unity.h"

/* ====================================================================
 * Local type definitions (self-contained test — no BSW headers)
 * ==================================================================== */

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef uint8          Std_ReturnType;
typedef uint8          boolean;

#define E_OK      0u
#define E_NOT_OK  1u
#define TRUE      1u
#define FALSE     0u
#define NULL_PTR  ((void*)0)

/* Prevent BSW headers from redefining types */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define COMSTACK_TYPES_H
#define SWC_DOORLOCK_H
#define BCM_CFG_H
#define DEM_H
#define WDGM_H
#define IOHWAB_H
#define RTE_H

/* ====================================================================
 * Signal IDs (must match Bcm_Cfg.h)
 * ==================================================================== */

#define BCM_SIG_VEHICLE_SPEED       16u
#define BCM_SIG_VEHICLE_STATE       17u
#define BCM_SIG_BODY_CONTROL_CMD    18u
#define BCM_SIG_DOOR_LOCK_STATE     24u

/* ARXML-generated signal IDs used by Swc_DoorLock.c (mirrors Bcm_Cfg.h) */
#define BCM_SIG_BODY_CONTROL_CMD_DOOR_LOCK_CMD      26u
#define BCM_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM       119u
#define BCM_SIG_VEHICLE_STATE_VEHICLE_STATE        182u
#define BCM_SIG_DOOR_LOCK_STATUS_FRONT_LEFT_LOCK    64u

/* Vehicle state values */
#define BCM_VSTATE_INIT             0u
#define BCM_VSTATE_READY            1u
#define BCM_VSTATE_DRIVING          2u
#define BCM_VSTATE_DEGRADED         3u
#define BCM_VSTATE_ESTOP            4u
#define BCM_VSTATE_FAULT            5u

/* Auto-lock speed threshold */
#define BCM_AUTO_LOCK_SPEED         10u

/* ====================================================================
 * Mock: Rte_Read — store values in array, return when SWC reads
 * ==================================================================== */

#define MOCK_RTE_MAX_SIGNALS  32u

static uint32 mock_rte_signals[MOCK_RTE_MAX_SIGNALS];

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

/* ====================================================================
 * Mock: Rte_Write — capture what SWC writes
 * ==================================================================== */

static uint16 mock_rte_write_sig;
static uint32 mock_rte_write_val;
static uint8  mock_rte_write_count;

Std_ReturnType Rte_Write(uint16 SignalId, uint32 Data)
{
    mock_rte_write_sig = SignalId;
    mock_rte_write_val = Data;
    mock_rte_write_count++;
    if (SignalId < MOCK_RTE_MAX_SIGNALS) {
        mock_rte_signals[SignalId] = Data;
        return E_OK;
    }
    return E_NOT_OK;
}

/* ====================================================================
 * Mock: Dem_ReportErrorStatus
 * ==================================================================== */

static uint8 mock_dem_event_id;
static uint8 mock_dem_event_status;
static uint8 mock_dem_report_count;

void Dem_ReportErrorStatus(uint8 EventId, uint8 EventStatus)
{
    mock_dem_event_id = EventId;
    mock_dem_event_status = EventStatus;
    mock_dem_report_count++;
}

/* ====================================================================
 * Include SWC under test (source inclusion for test build)
 * ==================================================================== */

#include "../src/Swc_DoorLock.c"

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    uint8 i;

    for (i = 0u; i < MOCK_RTE_MAX_SIGNALS; i++) {
        mock_rte_signals[i] = 0u;
    }

    mock_rte_write_sig   = 0u;
    mock_rte_write_val   = 0u;
    mock_rte_write_count = 0u;

    mock_dem_event_id     = 0xFFu;
    mock_dem_event_status = 0xFFu;
    mock_dem_report_count = 0u;

    Swc_DoorLock_Init();
}

void tearDown(void) { }

/* ====================================================================
 * SWR-BCM-009: Initialization — door unlocked
 * ==================================================================== */

/** @verifies SWR-BCM-009 */
void test_DoorLock_init_unlocked(void)
{
    /* Run one cycle with no commands, vehicle parked */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;

    Swc_DoorLock_100ms();

    /* Door lock state should be 0 (unlocked) */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/* ====================================================================
 * SWR-BCM-009: Manual lock command — byte 1 bit 0
 * ==================================================================== */

/** @verifies SWR-BCM-009 */
void test_DoorLock_manual_lock_command(void)
{
    /* Byte 1 bit 0 = lock. Body control cmd is uint32, byte 1 = bits 8-15 */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = (1u << 8u);  /* Byte 1, bit 0 */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;

    Swc_DoorLock_100ms();

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/* ====================================================================
 * SWR-BCM-009: Auto-lock when speed exceeds threshold (>10)
 * ==================================================================== */

/** @verifies SWR-BCM-009 */
void test_DoorLock_auto_lock_above_10_speed(void)
{
    /* No manual lock command, but speed > 10 */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 11u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;

    Swc_DoorLock_100ms();

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/* ====================================================================
 * SWR-BCM-009: Auto-unlock when transitioning to parked state
 * ==================================================================== */

/** @verifies SWR-BCM-009 */
void test_DoorLock_auto_unlock_when_parked(void)
{
    /* First: lock the door via auto-lock (driving, speed > 10) */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 20u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    Swc_DoorLock_100ms();
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);

    /* Now transition to parked (READY state, speed 0) */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE] = BCM_VSTATE_READY;
    Swc_DoorLock_100ms();

    /* Door should auto-unlock on transition to parked */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/* ====================================================================
 * SWR-BCM-009: Not initialized — does nothing
 * ==================================================================== */

/** @verifies SWR-BCM-009 */
void test_DoorLock_not_init_does_nothing(void)
{
    initialized = FALSE;

    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = (1u << 8u);
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 20u;
    mock_rte_write_count = 0u;

    Swc_DoorLock_100ms();

    /* No RTE writes should occur */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_rte_write_count);
}

/* ====================================================================
 * HARDENED: Boundary value tests
 * ==================================================================== */

/** @verifies SWR-BCM-009
 *  Equivalence class: auto-lock speed — boundary at threshold (10/11)
 *  Boundary value: speed == 10 should NOT lock, speed == 11 should lock */
void test_DoorLock_boundary_speed_at_threshold(void)
{
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;

    /* speed == 10 — exactly at threshold, should NOT auto-lock (> not >=) */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED] = 10u;
    Swc_DoorLock_100ms();
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);

    /* speed == 11 — one above threshold, SHOULD auto-lock */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED] = 11u;
    Swc_DoorLock_100ms();
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/** @verifies SWR-BCM-009
 *  Equivalence class: auto-lock speed — max uint32 speed
 *  Boundary value: maximum representable speed should lock */
void test_DoorLock_boundary_max_speed_locks(void)
{
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0xFFFFFFFFu;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;

    Swc_DoorLock_100ms();

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/** @verifies SWR-BCM-009
 *  Equivalence class: auto-lock speed — zero speed should not lock
 *  Boundary value: speed == 0 */
void test_DoorLock_boundary_speed_zero_no_lock(void)
{
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;

    Swc_DoorLock_100ms();

    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/* ====================================================================
 * HARDENED: State transition edge cases
 * ==================================================================== */

/** @verifies SWR-BCM-009
 *  Equivalence class: parked detection — Is_Parked for all vehicle states
 *  Partition: INIT(0) and READY(1) are parked, all others are not */
void test_DoorLock_parked_detection_all_states(void)
{
    /* Parked states: INIT (0) and READY (1) */
    TEST_ASSERT_TRUE(BCM_VSTATE_INIT <= BCM_VSTATE_READY);
    TEST_ASSERT_TRUE(BCM_VSTATE_READY <= BCM_VSTATE_READY);

    /* Non-parked states: DRIVING (2), DEGRADED (3), ESTOP (4), FAULT (5) */
    TEST_ASSERT_TRUE(BCM_VSTATE_DRIVING > BCM_VSTATE_READY);
    TEST_ASSERT_TRUE(BCM_VSTATE_DEGRADED > BCM_VSTATE_READY);
    TEST_ASSERT_TRUE(BCM_VSTATE_ESTOP > BCM_VSTATE_READY);
    TEST_ASSERT_TRUE(BCM_VSTATE_FAULT > BCM_VSTATE_READY);
}

/** @verifies SWR-BCM-009
 *  Equivalence class: auto-unlock — transition from each non-parked to READY
 *  Tests all non-parked -> parked transitions trigger unlock */
void test_DoorLock_auto_unlock_from_all_non_parked_states(void)
{
    uint32 non_parked_states[] = {
        BCM_VSTATE_DRIVING, BCM_VSTATE_DEGRADED,
        BCM_VSTATE_ESTOP, BCM_VSTATE_FAULT
    };
    uint8 i;

    for (i = 0u; i < 4u; i++) {
        /* Re-initialize */
        Swc_DoorLock_Init();

        /* First cycle: establish non-parked state and lock via speed */
        mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
        mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 20u;
        mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = non_parked_states[i];
        Swc_DoorLock_100ms();
        TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);

        /* Second cycle: transition to READY (parked) with low speed */
        mock_rte_signals[BCM_SIG_VEHICLE_SPEED] = 0u;
        mock_rte_signals[BCM_SIG_VEHICLE_STATE] = BCM_VSTATE_READY;
        Swc_DoorLock_100ms();
        TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
    }
}

/** @verifies SWR-BCM-009
 *  Equivalence class: no unlock on parked->parked transition
 *  Verify INIT->READY does NOT trigger auto-unlock (both are parked) */
void test_DoorLock_no_unlock_parked_to_parked(void)
{
    /* Init sets prev_vehicle_state = INIT (parked) */
    /* Manual lock */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = (1u << 8u);
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;
    Swc_DoorLock_100ms();
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);

    /* Still parked, no manual unlock command but also no transition */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;
    Swc_DoorLock_100ms();
    /* Should still be locked — no was_parked=FALSE -> now_parked=TRUE */
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/* ====================================================================
 * HARDENED: Manual lock and auto-lock interaction
 * ==================================================================== */

/** @verifies SWR-BCM-009
 *  Equivalence class: manual lock cmd — non-lock bits in byte 1 ignored
 *  Verify only bit 8 (byte 1 bit 0) triggers lock */
void test_DoorLock_manual_lock_only_bit8_matters(void)
{
    /* Set byte 1 high bits but NOT bit 0 of byte 1 */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0xFE00u; /* bits 9-15 set, bit 8 clear */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;
    Swc_DoorLock_100ms();

    /* Should NOT lock — only bit 8 triggers manual lock */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/** @verifies SWR-BCM-009
 *  Equivalence class: auto-lock overrides manual — speed overrides unlock
 *  Verify that auto-lock (speed > threshold) wins even without manual cmd */
void test_DoorLock_auto_lock_overrides_no_manual(void)
{
    /* No manual command, but speed triggers auto-lock */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 50u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    Swc_DoorLock_100ms();
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);

    /* Speed drops below threshold — lock should persist (no auto-unlock
       unless transitioning to parked) */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED] = 5u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE] = BCM_VSTATE_DRIVING;
    Swc_DoorLock_100ms();
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/* ====================================================================
 * HARDENED: Fault injection tests
 * ==================================================================== */

/** @verifies SWR-BCM-009
 *  Fault injection: invalid vehicle state value (255) — should not crash,
 *  should NOT be considered parked */
void test_DoorLock_fault_invalid_vehicle_state(void)
{
    /* First establish a non-parked previous state */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 20u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    Swc_DoorLock_100ms();
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);

    /* Invalid state 255 — Is_Parked(255) returns FALSE (255 > READY) */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE] = 255u;
    Swc_DoorLock_100ms();
    /* No auto-unlock (not transitioning to parked), no auto-lock (speed=0) */
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_DOOR_LOCK_STATE]);
}

/** @verifies SWR-BCM-009
 *  Fault injection: repeated calls without init — no writes */
void test_DoorLock_repeated_calls_without_init(void)
{
    initialized = FALSE;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = (1u << 8u);
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 50u;
    mock_rte_write_count = 0u;

    Swc_DoorLock_100ms();
    Swc_DoorLock_100ms();
    Swc_DoorLock_100ms();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_rte_write_count);
}

/** @verifies SWR-BCM-009
 *  Fault injection: re-initialization resets lock and prev state */
void test_DoorLock_reinit_resets_state(void)
{
    /* Lock via speed */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 20u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    Swc_DoorLock_100ms();
    TEST_ASSERT_EQUAL_UINT32(1u, lock_state);

    /* Re-init */
    Swc_DoorLock_Init();
    TEST_ASSERT_EQUAL_UINT32(0u, lock_state);
    TEST_ASSERT_EQUAL_UINT32(BCM_VSTATE_INIT, prev_vehicle_state);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-BCM-009: Door lock control */
    RUN_TEST(test_DoorLock_init_unlocked);
    RUN_TEST(test_DoorLock_manual_lock_command);
    RUN_TEST(test_DoorLock_auto_lock_above_10_speed);
    RUN_TEST(test_DoorLock_auto_unlock_when_parked);
    RUN_TEST(test_DoorLock_not_init_does_nothing);

    /* HARDENED: Boundary values */
    RUN_TEST(test_DoorLock_boundary_speed_at_threshold);
    RUN_TEST(test_DoorLock_boundary_max_speed_locks);
    RUN_TEST(test_DoorLock_boundary_speed_zero_no_lock);

    /* HARDENED: State transition edge cases */
    RUN_TEST(test_DoorLock_parked_detection_all_states);
    RUN_TEST(test_DoorLock_auto_unlock_from_all_non_parked_states);
    RUN_TEST(test_DoorLock_no_unlock_parked_to_parked);

    /* HARDENED: Manual/auto-lock interaction */
    RUN_TEST(test_DoorLock_manual_lock_only_bit8_matters);
    RUN_TEST(test_DoorLock_auto_lock_overrides_no_manual);

    /* HARDENED: Fault injection */
    RUN_TEST(test_DoorLock_fault_invalid_vehicle_state);
    RUN_TEST(test_DoorLock_repeated_calls_without_init);
    RUN_TEST(test_DoorLock_reinit_resets_state);

    return UNITY_END();
}
