/**
 * @file    test_Swc_Lights.c
 * @brief   Unit tests for Lights SWC — headlamp and tail light control
 * @date    2026-02-23
 *
 * @verifies SSR-BCM-003, SSR-BCM-004, SSR-BCM-005, SWR-BCM-003, SWR-BCM-004, SWR-BCM-005
 *
 * Tests light initialization (all off), automatic headlamp activation based on
 * vehicle speed and driving state, manual override, tail light following
 * headlamp state, and guard against uninitialized operation.
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
#define SWC_LIGHTS_H
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
#define BCM_SIG_LIGHT_HEADLAMP      19u
#define BCM_SIG_LIGHT_TAIL          20u

/* ARXML-generated signal IDs (fully-qualified names used by SWC source) */
#define BCM_SIG_MOTOR_STATUS_MOTOR_SPEED_RPM       119u
#define BCM_SIG_VEHICLE_STATE_VEHICLE_STATE        182u
#define BCM_SIG_BODY_CONTROL_CMD_HEADLIGHT_CMD      28u
#define BCM_SIG_LIGHT_STATUS_HEADLIGHT_ON           98u
#define BCM_SIG_LIGHT_STATUS_TAIL_LIGHT_ON          99u

/* Vehicle state values */
#define BCM_VSTATE_INIT             0u
#define BCM_VSTATE_READY            1u
#define BCM_VSTATE_DRIVING          2u
#define BCM_VSTATE_DEGRADED         3u
#define BCM_VSTATE_ESTOP            4u
#define BCM_VSTATE_FAULT            5u

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

#include "../src/Swc_Lights.c"

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

    Swc_Lights_Init();
}

void tearDown(void) { }

/* ====================================================================
 * SWR-BCM-003: Initialization — all outputs off
 * ==================================================================== */

/** @verifies SWR-BCM-003 */
void test_Lights_init_outputs_off(void)
{
    /* After init, run one cycle in READY state (parked), speed 0 */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;

    Swc_Lights_10ms();

    /* Headlamp and tail should be OFF */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/* ====================================================================
 * SWR-BCM-004: Auto headlamp ON when driving and speed > 0
 * ==================================================================== */

/** @verifies SWR-BCM-004 */
void test_Lights_auto_on_when_driving_and_speed_nonzero(void)
{
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 50u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;

    Swc_Lights_10ms();

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
}

/* ====================================================================
 * SWR-BCM-004: Lights OFF when stopped (speed 0)
 * ==================================================================== */

/** @verifies SWR-BCM-004 */
void test_Lights_off_when_stopped(void)
{
    /* First drive — turn lights on */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 50u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    Swc_Lights_10ms();
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);

    /* Now stop — speed 0, still DRIVING state */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED] = 0u;
    Swc_Lights_10ms();

    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
}

/* ====================================================================
 * SWR-BCM-005: Manual override — bit 0 of body_control_cmd forces ON
 * ==================================================================== */

/** @verifies SWR-BCM-005 */
void test_Lights_manual_override_on(void)
{
    /* Parked, speed 0, but manual override bit set */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0x01u;  /* Bit 0 = lights on */

    Swc_Lights_10ms();

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
}

/* ====================================================================
 * SWR-BCM-003: Tail lights follow headlamp state
 * ==================================================================== */

/** @verifies SWR-BCM-003 */
void test_Lights_tail_follows_headlamp(void)
{
    /* Drive — headlamp on, tail should follow */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 30u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;

    Swc_Lights_10ms();

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);

    /* Stop — headlamp off, tail should follow */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED] = 0u;
    Swc_Lights_10ms();

    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/* ====================================================================
 * SWR-BCM-004: Lights OFF when speed zero and parked (READY)
 * ==================================================================== */

/** @verifies SWR-BCM-004 */
void test_Lights_off_when_speed_zero_parked(void)
{
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;

    Swc_Lights_10ms();

    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/* ====================================================================
 * SWR-BCM-004: Lights ON at minimum driving speed (speed == 1)
 * ==================================================================== */

/** @verifies SWR-BCM-004 */
void test_Lights_on_driving_speed_1(void)
{
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 1u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;

    Swc_Lights_10ms();

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/* ====================================================================
 * SWR-BCM-003: Not initialized — does nothing
 * ==================================================================== */

/** @verifies SWR-BCM-003 */
void test_Lights_not_init_does_nothing(void)
{
    /* Reset the initialized flag by directly manipulating the static var.
     * Since we include the .c file, we have access. Set initialized = FALSE. */
    initialized = FALSE;

    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 50u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    mock_rte_write_count = 0u;

    Swc_Lights_10ms();

    /* No RTE writes should occur */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_rte_write_count);
}

/* ====================================================================
 * HARDENED TESTS — Boundary, NULL, Fault Injection, Equivalence Classes
 * ==================================================================== */

/** @verifies SWR-BCM-004
 *  Equivalence class: speed boundary — speed == 0 is OFF, speed == 1 is ON
 *  Boundary value: speed at exact zero boundary while DRIVING */
void test_Lights_boundary_speed_zero_driving(void)
{
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;

    Swc_Lights_10ms();

    /* Speed == 0 even in DRIVING state => headlamp OFF */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/** @verifies SWR-BCM-004
 *  Equivalence class: speed boundary — max uint32 speed value
 *  Boundary value: maximum representable speed */
void test_Lights_boundary_max_speed(void)
{
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0xFFFFFFFFu;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;

    Swc_Lights_10ms();

    /* Very high speed while driving => headlamp ON */
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/** @verifies SWR-BCM-004
 *  Equivalence class: vehicle state — invalid states not DRIVING
 *  Partition: INIT, READY, DEGRADED, ESTOP, FAULT all keep lights OFF */
void test_Lights_off_for_all_non_driving_states(void)
{
    uint32 non_driving_states[] = {
        BCM_VSTATE_INIT, BCM_VSTATE_READY,
        BCM_VSTATE_DEGRADED, BCM_VSTATE_ESTOP, BCM_VSTATE_FAULT
    };
    uint8 i;

    for (i = 0u; i < 5u; i++) {
        mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 100u;
        mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = non_driving_states[i];
        mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;

        Swc_Lights_10ms();

        TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    }
}

/** @verifies SWR-BCM-004
 *  Equivalence class: vehicle state — invalid state value beyond enum range
 *  Fault injection: out-of-range vehicle state value */
void test_Lights_fault_invalid_vehicle_state(void)
{
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 50u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = 255u;  /* Invalid state */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;

    Swc_Lights_10ms();

    /* Invalid state != DRIVING, so headlamp should be OFF */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/** @verifies SWR-BCM-005
 *  Equivalence class: manual override — override combined with auto-ON
 *  Tests override + auto conditions both true simultaneously */
void test_Lights_manual_override_plus_auto_on(void)
{
    /* Both auto-on condition (DRIVING + speed > 0) AND manual override */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 50u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0x01u;

    Swc_Lights_10ms();

    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/** @verifies SWR-BCM-005
 *  Equivalence class: body_cmd — non-override bits should not activate lights
 *  Tests that only bit 0 of body_cmd activates manual override */
void test_Lights_body_cmd_non_override_bits_ignored(void)
{
    /* Set all bits EXCEPT bit 0 */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0xFEu;  /* All bits except bit 0 */

    Swc_Lights_10ms();

    /* Only bit 0 is manual override — lights should remain OFF */
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/** @verifies SWR-BCM-005
 *  Equivalence class: manual override — toggle on then off
 *  Verifies override deactivation restores auto behavior */
void test_Lights_manual_override_toggle_off(void)
{
    /* Override ON while parked */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0x01u;
    Swc_Lights_10ms();
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);

    /* Override OFF — should revert to auto (parked + speed 0 = OFF) */
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0x00u;
    Swc_Lights_10ms();
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/** @verifies SWR-BCM-003
 *  Fault injection: call 10ms repeatedly without init — no crash, no writes */
void test_Lights_repeated_calls_without_init(void)
{
    initialized = FALSE;
    mock_rte_write_count = 0u;

    /* Multiple calls should be safe */
    Swc_Lights_10ms();
    Swc_Lights_10ms();
    Swc_Lights_10ms();

    TEST_ASSERT_EQUAL_UINT8(0u, mock_rte_write_count);
}

/** @verifies SWR-BCM-003
 *  Fault injection: re-initialization after normal operation resets outputs */
void test_Lights_reinit_resets_state(void)
{
    /* First, drive with lights on */
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 50u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_DRIVING;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    Swc_Lights_10ms();
    TEST_ASSERT_EQUAL_UINT32(1u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);

    /* Re-initialize — parked state, should go off */
    Swc_Lights_Init();
    mock_rte_signals[BCM_SIG_VEHICLE_SPEED]    = 0u;
    mock_rte_signals[BCM_SIG_VEHICLE_STATE]    = BCM_VSTATE_READY;
    mock_rte_signals[BCM_SIG_BODY_CONTROL_CMD] = 0u;
    Swc_Lights_10ms();
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_HEADLAMP]);
    TEST_ASSERT_EQUAL_UINT32(0u, mock_rte_signals[BCM_SIG_LIGHT_TAIL]);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-BCM-003: Initialization and tail follows headlamp */
    RUN_TEST(test_Lights_init_outputs_off);
    RUN_TEST(test_Lights_tail_follows_headlamp);
    RUN_TEST(test_Lights_not_init_does_nothing);

    /* SWR-BCM-004: Auto headlamp logic */
    RUN_TEST(test_Lights_auto_on_when_driving_and_speed_nonzero);
    RUN_TEST(test_Lights_off_when_stopped);
    RUN_TEST(test_Lights_off_when_speed_zero_parked);
    RUN_TEST(test_Lights_on_driving_speed_1);

    /* SWR-BCM-005: Manual override */
    RUN_TEST(test_Lights_manual_override_on);

    /* HARDENED: Boundary value tests */
    RUN_TEST(test_Lights_boundary_speed_zero_driving);
    RUN_TEST(test_Lights_boundary_max_speed);

    /* HARDENED: Equivalence class / partition tests */
    RUN_TEST(test_Lights_off_for_all_non_driving_states);
    RUN_TEST(test_Lights_manual_override_plus_auto_on);
    RUN_TEST(test_Lights_body_cmd_non_override_bits_ignored);
    RUN_TEST(test_Lights_manual_override_toggle_off);

    /* HARDENED: Fault injection tests */
    RUN_TEST(test_Lights_fault_invalid_vehicle_state);
    RUN_TEST(test_Lights_repeated_calls_without_init);
    RUN_TEST(test_Lights_reinit_resets_state);

    return UNITY_END();
}
