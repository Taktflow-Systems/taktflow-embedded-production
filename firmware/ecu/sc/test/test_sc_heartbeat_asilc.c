/**
 * @file    test_sc_heartbeat.c
 * @brief   Unit tests for sc_heartbeat — per-ECU heartbeat monitoring
 * @date    2026-02-23
 *
 * @verifies SWR-SC-004, SWR-SC-005, SWR-SC-006
 *
 * Tests 3-ECU independent timeout counters, 100ms timeout detection,
 * 30ms confirmation window, heartbeat resume during confirmation,
 * and per-ECU fault LED control.
 *
 * Mocks: GIO registers.
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions
 * ================================================================== */

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int       uint32;
typedef signed short        sint16;
typedef signed int         sint32;
typedef uint8               boolean;
typedef uint8               Std_ReturnType;

#define TRUE                1u
#define FALSE               0u
#define E_OK                0u
#define E_NOT_OK            1u
#define NULL_PTR            ((void*)0)

/* ==================================================================
 * SC Configuration Constants
 * ================================================================== */

#define SC_ECU_CVC                  0u
#define SC_ECU_FZC                  1u
#define SC_ECU_RZC                  2u
#define SC_ECU_COUNT                3u

#define SC_HB_TIMEOUT_TICKS         10u
#define SC_HB_CONFIRM_TICKS         3u
#define SC_HB_RECOVERY_THRESHOLD    3u

#define SC_GIO_PORT_A               0u
#define SC_PIN_LED_CVC              1u
#define SC_PIN_LED_FZC              2u
#define SC_PIN_LED_RZC              3u

/* ==================================================================
 * Mock: GIO Register Access
 * ================================================================== */

static uint8 mock_gio_a_state[8];

void gioSetBit(uint8 port, uint8 pin, uint8 value)
{
    (void)port;
    if (pin < 8u) {
        mock_gio_a_state[pin] = value;
    }
}

uint8 gioGetBit(uint8 port, uint8 pin)
{
    (void)port;
    if (pin < 8u) {
        return mock_gio_a_state[pin];
    }
    return 0u;
}

/* ==================================================================
 * Include source under test
 * ================================================================== */

#include "../src/sc_heartbeat.c"

/* ==================================================================
 * Test setup / teardown
 * ================================================================== */

void setUp(void)
{
    uint8 i;
    uint16 g;
    for (i = 0u; i < 8u; i++) {
        mock_gio_a_state[i] = 0u;
    }
    SC_Heartbeat_Init();
    /* Advance past startup grace period so tests run against active monitoring */
    for (g = 0u; g < SC_HB_STARTUP_GRACE_TICKS; g++) {
        SC_Heartbeat_Monitor();
    }
}

void tearDown(void) { }

/* ==================================================================
 * SWR-SC-004: Individual Timeout Counters
 * ================================================================== */

/** @verifies SWR-SC-004 -- Init resets all counters to zero */
void test_HB_Init_resets_all(void)
{
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_FZC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_RZC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsAnyConfirmed());
}

/** @verifies SWR-SC-004 -- Counter increments each 10ms tick */
void test_HB_counter_increments(void)
{
    uint8 i;

    /* Run 9 ticks — just under timeout */
    for (i = 0u; i < (SC_HB_TIMEOUT_TICKS - 1u); i++) {
        SC_Heartbeat_Monitor();
    }

    /* Should not be timed out yet (10 ticks = threshold) */
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
}

/** @verifies SWR-SC-004 -- Counter resets on valid heartbeat reception */
void test_HB_counter_resets_on_rx(void)
{
    uint8 i;

    /* Run 7 ticks toward timeout */
    for (i = 0u; i < 7u; i++) {
        SC_Heartbeat_Monitor();
    }

    /* Receive heartbeat */
    SC_Heartbeat_NotifyRx(SC_ECU_CVC);

    /* Run 9 more ticks — still should not timeout because counter reset */
    for (i = 0u; i < (SC_HB_TIMEOUT_TICKS - 1u); i++) {
        SC_Heartbeat_Monitor();
    }

    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
}

/* ==================================================================
 * SWR-SC-005: 150ms Timeout Detection
 * ================================================================== */

/** @verifies SWR-SC-005 -- Timeout detected at exactly 10 ticks (100ms) */
void test_HB_timeout_at_10_ticks(void)
{
    uint8 i;
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }

    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_FZC));
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_RZC));
}

/** @verifies SWR-SC-005 -- Only one ECU times out when others receive HB */
void test_HB_independent_ecus(void)
{
    uint8 i;

    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        /* FZC and RZC receive heartbeats, CVC does not */
        SC_Heartbeat_NotifyRx(SC_ECU_FZC);
        SC_Heartbeat_NotifyRx(SC_ECU_RZC);
        SC_Heartbeat_Monitor();
    }

    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_FZC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_RZC));
}

/** @verifies SWR-SC-005 -- Fault LED set HIGH on timeout */
void test_HB_fault_led_on_timeout(void)
{
    uint8 i;
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }

    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a_state[SC_PIN_LED_CVC]);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a_state[SC_PIN_LED_FZC]);
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a_state[SC_PIN_LED_RZC]);
}

/* ==================================================================
 * SWR-SC-006: 50ms Confirmation Window
 * ================================================================== */

/** @verifies SWR-SC-006 -- Timeout not confirmed until 130ms total */
void test_HB_confirmation_window(void)
{
    uint8 i;

    /* 10 ticks = timeout detected (100ms) */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsAnyConfirmed());

    /* 2 more ticks = still in confirmation window */
    for (i = 0u; i < (SC_HB_CONFIRM_TICKS - 1u); i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_FALSE(SC_Heartbeat_IsAnyConfirmed());

    /* 3rd tick = confirmed */
    SC_Heartbeat_Monitor();
    TEST_ASSERT_TRUE(SC_Heartbeat_IsAnyConfirmed());
}

/** @verifies SWR-SC-006 -- Heartbeat resume during confirmation cancels */
void test_HB_resume_cancels_confirmation(void)
{
    uint8 i;
    uint8 j;

    /* Enter timeout state */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));

    /* Heartbeat resumes for all ECUs — 3 consecutive HBs each (recovery debounce) */
    for (j = 0u; j < SC_HB_RECOVERY_THRESHOLD; j++) {
        SC_Heartbeat_NotifyRx(SC_ECU_CVC);
        SC_Heartbeat_NotifyRx(SC_ECU_FZC);
        SC_Heartbeat_NotifyRx(SC_ECU_RZC);
    }

    /* Continue monitoring — should not reach confirmed */
    for (i = 0u; i < SC_HB_CONFIRM_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }

    /* Not confirmed because heartbeats resumed (recovery completed) */
    TEST_ASSERT_FALSE(SC_Heartbeat_IsAnyConfirmed());
}

/** @verifies SWR-SC-006 -- Confirmation requires ALL to be timed out for any one confirmed */
void test_HB_partial_resume(void)
{
    uint8 i;
    uint8 j;

    /* All ECUs time out */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }

    /* Only CVC resumes — 3 consecutive HBs for recovery debounce */
    for (j = 0u; j < SC_HB_RECOVERY_THRESHOLD; j++) {
        SC_Heartbeat_NotifyRx(SC_ECU_CVC);
    }

    /* FZC and RZC continue through confirmation window */
    for (i = 0u; i < SC_HB_CONFIRM_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }

    /* FZC or RZC should be confirmed */
    TEST_ASSERT_TRUE(SC_Heartbeat_IsAnyConfirmed());
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
}

/** @verifies SWR-SC-004 -- Invalid ECU index handled safely */
void test_HB_invalid_ecu_index(void)
{
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_COUNT));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(0xFFu));
}

/** @verifies SWR-SC-004 -- NotifyRx with invalid index is safe no-op */
void test_HB_notify_invalid_index(void)
{
    /* Should not crash */
    SC_Heartbeat_NotifyRx(SC_ECU_COUNT);
    SC_Heartbeat_NotifyRx(0xFFu);

    TEST_ASSERT_FALSE(SC_Heartbeat_IsAnyConfirmed());
}

/* ==================================================================
 * HARDENED TESTS — Boundary Values, Fault Injection
 * ================================================================== */

/** @verifies SWR-SC-005
 *  Equivalence class: Boundary — timeout at exactly 9 ticks (one under) */
void test_hb_timeout_at_9_no_fault(void)
{
    uint8 i;
    for (i = 0u; i < (SC_HB_TIMEOUT_TICKS - 1u); i++) {
        SC_Heartbeat_Monitor();
    }

    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_FZC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_RZC));
}

/** @verifies SWR-SC-006
 *  Equivalence class: Boundary — confirmation at exactly 2 ticks (one under) */
void test_hb_confirmation_at_2_not_confirmed(void)
{
    uint8 i;
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    for (i = 0u; i < (SC_HB_CONFIRM_TICKS - 1u); i++) {
        SC_Heartbeat_Monitor();
    }

    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsAnyConfirmed());
}

/** @verifies SWR-SC-005
 *  Equivalence class: Boundary — rapid notify/monitor interleaving */
void test_hb_rapid_notify_monitor(void)
{
    uint8 i;
    for (i = 0u; i < 100u; i++) {
        SC_Heartbeat_NotifyRx(SC_ECU_CVC);
        SC_Heartbeat_NotifyRx(SC_ECU_FZC);
        SC_Heartbeat_NotifyRx(SC_ECU_RZC);
        SC_Heartbeat_Monitor();
    }

    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_FZC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_RZC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsAnyConfirmed());
}

/** @verifies SWR-SC-006
 *  Equivalence class: Fault injection — single ECU timeout then resume
 *  after confirmation starts */
void test_hb_single_ecu_late_resume(void)
{
    uint8 i;
    uint8 j;

    /* All ECUs timeout */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }

    /* Start confirmation: 2 ticks in */
    SC_Heartbeat_Monitor();
    SC_Heartbeat_Monitor();

    /* RZC resumes — 3 consecutive HBs for recovery debounce */
    for (j = 0u; j < SC_HB_RECOVERY_THRESHOLD; j++) {
        SC_Heartbeat_NotifyRx(SC_ECU_RZC);
    }

    /* Complete confirmation window */
    for (i = 0u; i < (SC_HB_CONFIRM_TICKS - 2u); i++) {
        SC_Heartbeat_Monitor();
    }

    /* CVC and FZC should be confirmed */
    TEST_ASSERT_TRUE(SC_Heartbeat_IsAnyConfirmed());
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_RZC));
}

/** @verifies SWR-SC-005
 *  Equivalence class: Boundary — LED cleared when heartbeat resumes before confirmation */
void test_hb_led_cleared_on_resume(void)
{
    uint8 i;
    uint8 j;

    /* Trigger timeout to set LEDs */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_EQUAL_UINT8(1u, mock_gio_a_state[SC_PIN_LED_CVC]);

    /* Heartbeat resumes — 3 consecutive HBs for recovery debounce */
    for (j = 0u; j < SC_HB_RECOVERY_THRESHOLD; j++) {
        SC_Heartbeat_NotifyRx(SC_ECU_CVC);
    }

    /* LED should be cleared after recovery (gioSetBit called in NotifyRx) */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a_state[SC_PIN_LED_CVC]);
}

/* ==================================================================
 * PHASE 2: FTTI Budget Verification
 * ================================================================== */

/** @verifies SWR-SC-005
 *  Phase 2: Timeout detection at 100ms (10 ticks), within SG-008 FTTI */
void test_HB_timeout_within_ftti(void)
{
    uint8 i;
    /* Run exactly 10 ticks = 100ms */
    for (i = 0u; i < 10u; i++) {
        SC_Heartbeat_Monitor();
    }

    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
}

/** @verifies SWR-SC-006
 *  Phase 2: Confirmation at 3 ticks (30ms), total 130ms */
void test_HB_confirmation_at_3_ticks(void)
{
    uint8 i;

    /* 10 ticks = timeout */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsAnyConfirmed());

    /* 3 more ticks = confirmed */
    for (i = 0u; i < SC_HB_CONFIRM_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_TRUE(SC_Heartbeat_IsAnyConfirmed());
}

/* ==================================================================
 * PHASE 4: Recovery Debounce
 * ================================================================== */

/** @verifies SWR-SC-005
 *  Phase 4: Single HB does NOT cancel timeout (needs 3 consecutive) */
void test_HB_single_notify_does_not_cancel_timeout(void)
{
    uint8 i;

    /* Enter timeout state */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));

    /* Single heartbeat — insufficient for recovery */
    SC_Heartbeat_NotifyRx(SC_ECU_CVC);

    /* CVC should still be timed out */
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
}

/** @verifies SWR-SC-005
 *  Phase 4: 3 consecutive HBs clears timeout via recovery debounce */
void test_HB_recovery_after_3_consecutive_HBs(void)
{
    uint8 i;
    uint8 j;

    /* Enter timeout state */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));

    /* 3 consecutive HBs for CVC */
    for (j = 0u; j < SC_HB_RECOVERY_THRESHOLD; j++) {
        SC_Heartbeat_NotifyRx(SC_ECU_CVC);
    }

    /* CVC should now be recovered */
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    /* LED should be cleared */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_gio_a_state[SC_PIN_LED_CVC]);
}

/** @verifies SWR-SC-005
 *  Phase 4: Recovery counter resets when Monitor detects re-timeout */
void test_HB_recovery_reset_on_continued_timeout(void)
{
    uint8 i;

    /* Enter timeout state */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));

    /* 2 HBs (not enough) */
    SC_Heartbeat_NotifyRx(SC_ECU_CVC);
    SC_Heartbeat_NotifyRx(SC_ECU_CVC);

    /* Run enough Monitor ticks to re-enter timeout (counter resets recovery) */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }

    /* 2 more HBs — should NOT recover because counter was reset */
    SC_Heartbeat_NotifyRx(SC_ECU_CVC);
    SC_Heartbeat_NotifyRx(SC_ECU_CVC);

    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
}

/* ==================================================================
 * PHASE 6: Comprehensive Test Coverage
 * ================================================================== */

/** @verifies SWR-SC-004
 *  Phase 6: Startup grace period prevents false timeout during ECU boot */
void test_HB_startup_grace_no_timeout(void)
{
    uint16 g;

    /* Re-init to get a fresh startup grace period */
    SC_Heartbeat_Init();

    /* Run just under grace period without any heartbeats */
    for (g = 0u; g < (SC_HB_STARTUP_GRACE_TICKS - 1u); g++) {
        SC_Heartbeat_Monitor();
    }

    /* No ECU should be timed out — grace period active */
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_FZC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_RZC));
}

/** @verifies SWR-SC-004
 *  Phase 6: Double init resets all counters and flags safely */
void test_HB_double_init_resets_all(void)
{
    uint8 i;

    /* Force a timeout */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_TRUE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));

    /* Double init */
    SC_Heartbeat_Init();

    /* Advance past grace period again */
    {
        uint16 g;
        for (g = 0u; g < SC_HB_STARTUP_GRACE_TICKS; g++) {
            SC_Heartbeat_Monitor();
        }
    }

    /* After fresh init + grace, no timeout (counters reset) */
    TEST_ASSERT_FALSE(SC_Heartbeat_IsTimedOut(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsAnyConfirmed());
}

/** @verifies SWR-SC-006
 *  Phase 6: Confirmed timeout is latched — NotifyRx does NOT clear it */
void test_HB_confirmed_timeout_latches(void)
{
    uint8 i;
    uint8 j;

    /* Timeout + confirmation */
    for (i = 0u; i < SC_HB_TIMEOUT_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    for (i = 0u; i < SC_HB_CONFIRM_TICKS; i++) {
        SC_Heartbeat_Monitor();
    }
    TEST_ASSERT_TRUE(SC_Heartbeat_IsAnyConfirmed());

    /* Try to recover with heartbeats — should be ignored (latched) */
    for (j = 0u; j < (SC_HB_RECOVERY_THRESHOLD * 3u); j++) {
        SC_Heartbeat_NotifyRx(SC_ECU_CVC);
        SC_Heartbeat_NotifyRx(SC_ECU_FZC);
        SC_Heartbeat_NotifyRx(SC_ECU_RZC);
    }

    /* Still confirmed — latched */
    TEST_ASSERT_TRUE(SC_Heartbeat_IsAnyConfirmed());
}

/* ==================================================================
 * Phase 7: Content Validation (SWR-SC-027, SWR-SC-028)
 * ================================================================== */

/** @id TC-SC-047 @verifies SWR-SC-027
 *  Equivalence class: Threshold boundary — stuck_degraded_cnt reaching
 *  SC_HB_STUCK_DEGRADED_MAX triggers content fault; one below does not. */
void test_HB_content_fault_stuck_degraded_threshold(void)
{
    uint8 i;
    uint8 payload[4];

    SC_Heartbeat_Init();

    /* payload[3] bits[3:0]=2 → DEGRADED mode; FaultStatus=0 (no fault bits) */
    payload[0] = 0u;
    payload[1] = 0u;
    payload[2] = 0u;
    payload[3] = 0x02u;  /* mode=DEGRADED, FaultStatus=0 */

    /* SC_HB_STUCK_DEGRADED_MAX - 1 calls → fault NOT yet latched */
    for (i = 0u; i < (SC_HB_STUCK_DEGRADED_MAX - 1u); i++) {
        SC_Heartbeat_ValidateContent(SC_ECU_CVC, payload);
    }
    TEST_ASSERT_FALSE(SC_Heartbeat_IsContentFault(SC_ECU_CVC));

    /* One more call → fault latches */
    SC_Heartbeat_ValidateContent(SC_ECU_CVC, payload);
    TEST_ASSERT_TRUE(SC_Heartbeat_IsContentFault(SC_ECU_CVC));

    /* Other ECUs must remain clean */
    TEST_ASSERT_FALSE(SC_Heartbeat_IsContentFault(SC_ECU_FZC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsContentFault(SC_ECU_RZC));
}

/** @id TC-SC-048 @verifies SWR-SC-028
 *  Equivalence class: Threshold boundary — fault_escalate_cnt reaching
 *  SC_HB_FAULT_ESCALATE_MAX triggers content fault; one below does not. */
void test_HB_content_fault_escalate_threshold(void)
{
    uint8 i;
    uint8 payload[4];

    SC_Heartbeat_Init();

    /* payload[3] bits[7:4]=0x3 → 2 FaultStatus bits set; mode=0 (NORMAL) */
    payload[0] = 0u;
    payload[1] = 0u;
    payload[2] = 0u;
    payload[3] = 0x30u;  /* FaultStatus=0x3 (2 bits), mode=NORMAL */

    /* SC_HB_FAULT_ESCALATE_MAX - 1 calls → fault NOT yet latched */
    for (i = 0u; i < (SC_HB_FAULT_ESCALATE_MAX - 1u); i++) {
        SC_Heartbeat_ValidateContent(SC_ECU_FZC, payload);
    }
    TEST_ASSERT_FALSE(SC_Heartbeat_IsContentFault(SC_ECU_FZC));

    /* One more call → fault latches */
    SC_Heartbeat_ValidateContent(SC_ECU_FZC, payload);
    TEST_ASSERT_TRUE(SC_Heartbeat_IsContentFault(SC_ECU_FZC));

    /* Other ECUs must remain clean */
    TEST_ASSERT_FALSE(SC_Heartbeat_IsContentFault(SC_ECU_CVC));
    TEST_ASSERT_FALSE(SC_Heartbeat_IsContentFault(SC_ECU_RZC));
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-SC-004: Individual counters */
    RUN_TEST(test_HB_Init_resets_all);
    RUN_TEST(test_HB_counter_increments);
    RUN_TEST(test_HB_counter_resets_on_rx);

    /* SWR-SC-005: Timeout detection */
    RUN_TEST(test_HB_timeout_at_10_ticks);
    RUN_TEST(test_HB_independent_ecus);
    RUN_TEST(test_HB_fault_led_on_timeout);

    /* SWR-SC-006: Confirmation window */
    RUN_TEST(test_HB_confirmation_window);
    RUN_TEST(test_HB_resume_cancels_confirmation);
    RUN_TEST(test_HB_partial_resume);

    /* Boundary / safety */
    RUN_TEST(test_HB_invalid_ecu_index);
    RUN_TEST(test_HB_notify_invalid_index);

    /* Hardened tests -- boundary values, fault injection */
    RUN_TEST(test_hb_timeout_at_9_no_fault);
    RUN_TEST(test_hb_confirmation_at_2_not_confirmed);
    RUN_TEST(test_hb_rapid_notify_monitor);
    RUN_TEST(test_hb_single_ecu_late_resume);
    RUN_TEST(test_hb_led_cleared_on_resume);

    /* Phase 2: FTTI Budget */
    RUN_TEST(test_HB_timeout_within_ftti);
    RUN_TEST(test_HB_confirmation_at_3_ticks);

    /* Phase 4: Recovery Debounce */
    RUN_TEST(test_HB_single_notify_does_not_cancel_timeout);
    RUN_TEST(test_HB_recovery_after_3_consecutive_HBs);
    RUN_TEST(test_HB_recovery_reset_on_continued_timeout);

    /* Phase 6: Comprehensive Coverage */
    RUN_TEST(test_HB_startup_grace_no_timeout);
    RUN_TEST(test_HB_double_init_resets_all);
    RUN_TEST(test_HB_confirmed_timeout_latches);

    /* Phase 7: Content Validation (SWR-SC-027, SWR-SC-028) */
    RUN_TEST(test_HB_content_fault_stuck_degraded_threshold);  /* TC-SC-047 */
    RUN_TEST(test_HB_content_fault_escalate_threshold);        /* TC-SC-048 */

    return UNITY_END();
}
