/**
 * @file    test_Swc_CanMonitor.c
 * @brief   Unit tests for Swc_CanMonitor — CAN bus loss detection and recovery
 * @date    2026-02-24
 *
 * @verifies SWR-CVC-024, SWR-CVC-025
 *
 * Tests: bus-off detection, 200ms silence detection, error warning 500ms,
 * recovery success, recovery 3-failure shutdown.
 *
 * Mocks: None required (pure logic, no BSW dependencies)
 */
#include "unity.h"

/* ==================================================================
 * Local type definitions (avoid BSW header mock conflicts)
 * ================================================================== */

typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int   uint32;
typedef uint8           Std_ReturnType;

#define E_OK        0u
#define E_NOT_OK    1u
#define TRUE        1u
#define FALSE       0u
#define NULL_PTR    ((void*)0)

/* ==================================================================
 * CAN Monitor Constants (mirrors header)
 * ================================================================== */

#define CANMON_SILENCE_TIMEOUT_MS       200u
#define CANMON_ERROR_WARN_TIMEOUT_MS    500u
#define CANMON_MAX_RECOVERY_ATTEMPTS      3u
#define CANMON_RECOVERY_WINDOW_MS     10000u

#define CANMON_STATUS_OK               0u
#define CANMON_STATUS_BUSOFF            1u
#define CANMON_STATUS_SILENCE           2u
#define CANMON_STATUS_ERROR_WARNING     3u
#define CANMON_STATUS_SAFE_STOP         4u
#define CANMON_STATUS_SHUTDOWN          5u

/* ==================================================================
 * Forward declarations (source included at bottom of file)
 * ================================================================== */

extern void            Swc_CanMonitor_Init(void);
extern uint8           Swc_CanMonitor_Check(uint8 isBusOff, uint32 rxMsgCount,
                                             uint8 errorWarning, uint32 currentTimeMs);
extern Std_ReturnType  Swc_CanMonitor_Recovery(uint32 currentTimeMs);
extern uint8           Swc_CanMonitor_GetStatus(void);

/* ==================================================================
 * Test Configuration
 * ================================================================== */

void setUp(void)
{
    Swc_CanMonitor_Init();
}

void tearDown(void) { }

/* ==================================================================
 * SWR-CVC-024: CAN Bus Loss Detection
 * ================================================================== */

/** @verifies SWR-CVC-024 — Bus-off detected triggers SAFE_STOP immediately */
void test_CanMonitor_busoff_detected(void)
{
    uint8 result;

    result = Swc_CanMonitor_Check(TRUE, 0u, FALSE, 1000u);

    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_SAFE_STOP, result);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_BUSOFF, Swc_CanMonitor_GetStatus());
}

/** @verifies SWR-CVC-024 — 200ms silence triggers SAFE_STOP */
void test_CanMonitor_silence_200ms_triggers_safe_stop(void)
{
    uint8 result;

    /* First call: rx count = 5 at t=0ms */
    result = Swc_CanMonitor_Check(FALSE, 5u, FALSE, 0u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, result);

    /* Same rx count at t=100ms — 100ms silence, not enough */
    result = Swc_CanMonitor_Check(FALSE, 5u, FALSE, 100u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, result);

    /* Same rx count at t=200ms — 200ms silence, triggers */
    result = Swc_CanMonitor_Check(FALSE, 5u, FALSE, 200u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_SAFE_STOP, result);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_SILENCE, Swc_CanMonitor_GetStatus());
}

/** @verifies SWR-CVC-024 — Error warning sustained 500ms triggers SAFE_STOP */
void test_CanMonitor_error_warning_sustained_500ms(void)
{
    uint8 result;
    uint32 t;

    /* Continuously receiving messages (silence timer ok) but error warning active */
    for (t = 0u; t < 490u; t += 10u)
    {
        result = Swc_CanMonitor_Check(FALSE, t + 1u, TRUE, t);
        TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, result);
    }

    /* At 500ms: error warning sustained long enough */
    result = Swc_CanMonitor_Check(FALSE, 51u, TRUE, 500u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_SAFE_STOP, result);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_ERROR_WARNING, Swc_CanMonitor_GetStatus());
}

/** @verifies SWR-CVC-024 — Error warning cleared before 500ms does NOT trigger */
void test_CanMonitor_error_warning_cleared_before_timeout(void)
{
    uint8 result;

    /* Error warning active at t=0 */
    result = Swc_CanMonitor_Check(FALSE, 1u, TRUE, 0u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, result);

    /* Error warning cleared at t=300ms */
    result = Swc_CanMonitor_Check(FALSE, 2u, FALSE, 300u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, result);

    /* More time passes without error — no trigger */
    result = Swc_CanMonitor_Check(FALSE, 3u, FALSE, 600u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, result);
}

/** @verifies SWR-CVC-024 — Normal operation: messages arriving, no errors */
void test_CanMonitor_normal_operation_ok(void)
{
    uint8 result;

    result = Swc_CanMonitor_Check(FALSE, 1u, FALSE, 0u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, result);

    result = Swc_CanMonitor_Check(FALSE, 2u, FALSE, 50u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, result);

    result = Swc_CanMonitor_Check(FALSE, 3u, FALSE, 100u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, result);
}

/* ==================================================================
 * SWR-CVC-025: CAN Bus Recovery
 * ================================================================== */

/** @verifies SWR-CVC-025 — Recovery attempt succeeds (resets status to OK) */
void test_CanMonitor_recovery_success(void)
{
    Std_ReturnType ret;

    /* Trigger bus-off first */
    (void)Swc_CanMonitor_Check(TRUE, 0u, FALSE, 1000u);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_BUSOFF, Swc_CanMonitor_GetStatus());

    /* Attempt recovery */
    ret = Swc_CanMonitor_Recovery(1000u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_OK, Swc_CanMonitor_GetStatus());
}

/** @verifies SWR-CVC-025 — 3 failures within 10s triggers SHUTDOWN */
void test_CanMonitor_recovery_3_failures_shutdown(void)
{
    Std_ReturnType ret;

    /* Attempt 1 */
    (void)Swc_CanMonitor_Check(TRUE, 0u, FALSE, 1000u);
    ret = Swc_CanMonitor_Recovery(1000u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Attempt 2 */
    (void)Swc_CanMonitor_Check(TRUE, 0u, FALSE, 2000u);
    ret = Swc_CanMonitor_Recovery(2000u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Attempt 3 */
    (void)Swc_CanMonitor_Check(TRUE, 0u, FALSE, 3000u);
    ret = Swc_CanMonitor_Recovery(3000u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Attempt 4 — exceeds max, should fail */
    (void)Swc_CanMonitor_Check(TRUE, 0u, FALSE, 4000u);
    ret = Swc_CanMonitor_Recovery(4000u);
    TEST_ASSERT_EQUAL_UINT8(E_NOT_OK, ret);
    TEST_ASSERT_EQUAL_UINT8(CANMON_STATUS_SHUTDOWN, Swc_CanMonitor_GetStatus());
}

/** @verifies SWR-CVC-025 — Recovery counter resets after window expires */
void test_CanMonitor_recovery_window_resets(void)
{
    Std_ReturnType ret;

    /* 3 attempts in first window */
    (void)Swc_CanMonitor_Check(TRUE, 0u, FALSE, 0u);
    ret = Swc_CanMonitor_Recovery(0u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    (void)Swc_CanMonitor_Check(TRUE, 0u, FALSE, 1000u);
    ret = Swc_CanMonitor_Recovery(1000u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    (void)Swc_CanMonitor_Check(TRUE, 0u, FALSE, 2000u);
    ret = Swc_CanMonitor_Recovery(2000u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);

    /* Wait beyond 10s window, then try again — should succeed */
    (void)Swc_CanMonitor_Check(TRUE, 0u, FALSE, 15000u);
    ret = Swc_CanMonitor_Recovery(15000u);
    TEST_ASSERT_EQUAL_UINT8(E_OK, ret);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* SWR-CVC-024: CAN bus loss detection */
    RUN_TEST(test_CanMonitor_busoff_detected);
    RUN_TEST(test_CanMonitor_silence_200ms_triggers_safe_stop);
    RUN_TEST(test_CanMonitor_error_warning_sustained_500ms);
    RUN_TEST(test_CanMonitor_error_warning_cleared_before_timeout);
    RUN_TEST(test_CanMonitor_normal_operation_ok);

    /* SWR-CVC-025: CAN bus recovery */
    RUN_TEST(test_CanMonitor_recovery_success);
    RUN_TEST(test_CanMonitor_recovery_3_failures_shutdown);
    RUN_TEST(test_CanMonitor_recovery_window_resets);

    return UNITY_END();
}

/* ==================================================================
 * Source inclusion — link SWC under test directly into test binary
 * ================================================================== */

/* Prevent BSW headers from redefining types when Swc_CanMonitor.c is included */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define SWC_CANMONITOR_H

#include "../src/Swc_CanMonitor.c"
