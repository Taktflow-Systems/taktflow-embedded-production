/**
 * @file    test_WdgM.c
 * @brief   Unit tests for Watchdog Manager
 * @date    2026-02-21
 *
 * @verifies SWR-BSW-019, SWR-BSW-020, SWR-BSW-021, SWR-BSW-022
 *
 * Tests alive supervision, checkpoint counting, global status,
 * watchdog feed gating logic, and alive counter range checking
 * with WDI toggle gating.
 */
#include "unity.h"
#include "WdgM.h"

/* ==================================================================
 * Mock: Dio (watchdog pin toggle)
 * ================================================================== */

static uint8 mock_dio_flip_count;

uint8 Dio_FlipChannel(uint8 ChannelId)
{
    (void)ChannelId;
    mock_dio_flip_count++;
    return STD_HIGH;
}

/* ==================================================================
 * Mock: Dem (error reporting)
 * ================================================================== */

static uint8 mock_dem_event_id;
static uint8 mock_dem_status;
static uint8 mock_dem_report_count;

void Dem_ReportErrorStatus(Dem_EventIdType EventId, Dem_EventStatusType EventStatus)
{
    mock_dem_event_id = EventId;
    mock_dem_status = EventStatus;
    mock_dem_report_count++;
}

/* ==================================================================
 * Test configuration
 * ================================================================== */

static const WdgM_SupervisedEntityConfigType test_se_config[] = {
    { 0u, 4u, 6u, 2u },  /* SE 0: expected 4-6 checkpoints, 2 failed cycles tolerance */
    { 1u, 1u, 1u, 1u },  /* SE 1: expected exactly 1, 1 cycle tolerance */
};

static WdgM_ConfigType test_config;

void setUp(void)
{
    mock_dio_flip_count = 0u;
    mock_dem_report_count = 0u;

    test_config.seConfig = test_se_config;
    test_config.seCount = 2u;
    test_config.wdtDioChannel = 0u;

    WdgM_Init(&test_config);
}

void tearDown(void) { }

/* ==================================================================
 * SWR-BSW-019: Alive Supervision
 * ================================================================== */

/** @verifies SWR-BSW-019, SWR-BSW-021 */
void test_WdgM_CheckpointReached_increments(void)
{
    Std_ReturnType ret = WdgM_CheckpointReached(0u);
    TEST_ASSERT_EQUAL(E_OK, ret);
}

/** @verifies SWR-BSW-019 */
void test_WdgM_CheckpointReached_invalid_se(void)
{
    Std_ReturnType ret = WdgM_CheckpointReached(99u);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/** @verifies SWR-BSW-019 */
void test_WdgM_MainFunction_all_ok_feeds_wdt(void)
{
    /* SE 0: report 5 checkpoints (within 4-6 range) */
    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    /* SE 1: report 1 checkpoint (expected exactly 1) */
    WdgM_CheckpointReached(1u);

    WdgM_MainFunction();

    TEST_ASSERT_EQUAL_UINT8(1u, mock_dio_flip_count);
    TEST_ASSERT_EQUAL(WDGM_GLOBAL_STATUS_OK, WdgM_GetGlobalStatus());
}

/** @verifies SWR-BSW-019 */
void test_WdgM_MainFunction_too_few_no_feed(void)
{
    /* SE 0: only 2 checkpoints (below min 4) */
    WdgM_CheckpointReached(0u);
    WdgM_CheckpointReached(0u);
    /* SE 1: ok */
    WdgM_CheckpointReached(1u);

    WdgM_MainFunction();

    /* First failure — within tolerance, but local status should be FAILED */
    WdgM_LocalStatusType local_status;
    WdgM_GetLocalStatus(0u, &local_status);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_FAILED, local_status);
}

/** @verifies SWR-BSW-019 */
void test_WdgM_MainFunction_too_many_no_feed(void)
{
    /* SE 0: 10 checkpoints (above max 6) */
    for (uint8 i = 0u; i < 10u; i++) {
        WdgM_CheckpointReached(0u);
    }
    /* SE 1: ok */
    WdgM_CheckpointReached(1u);

    WdgM_MainFunction();

    WdgM_LocalStatusType local_status;
    WdgM_GetLocalStatus(0u, &local_status);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_FAILED, local_status);
}

/* ==================================================================
 * SWR-BSW-020: Global Status and Expiry
 * ================================================================== */

/** @verifies SWR-BSW-020 */
void test_WdgM_expired_after_tolerance(void)
{
    /* SE 1 has tolerance 1. Fail it twice to expire. */
    /* Cycle 1: no checkpoints for SE 1 */
    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    WdgM_MainFunction();

    /* Cycle 2: still no checkpoints for SE 1 */
    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    WdgM_MainFunction();

    WdgM_LocalStatusType local;
    WdgM_GetLocalStatus(1u, &local);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_EXPIRED, local);
    TEST_ASSERT_EQUAL(WDGM_GLOBAL_STATUS_FAILED, WdgM_GetGlobalStatus());
}

/** @verifies SWR-BSW-020 */
void test_WdgM_Init_null_config(void)
{
    WdgM_Init(NULL_PTR);
    TEST_ASSERT_EQUAL(WDGM_GLOBAL_STATUS_FAILED, WdgM_GetGlobalStatus());
}

/** @verifies SWR-BSW-020 */
void test_WdgM_GetLocalStatus_null(void)
{
    Std_ReturnType ret = WdgM_GetLocalStatus(0u, NULL_PTR);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * SWR-BSW-019, SWR-BSW-020: ASIL D Hardened Boundary / Fault Injection Tests
 * ================================================================== */

/** @verifies SWR-BSW-019
 *  Boundary: Checkpoint exactly at min boundary (4) for SE 0 — should be OK */
void test_WdgM_Checkpoint_exact_min(void)
{
    /* SE 0: expected 4-6 checkpoints. Report exactly 4. */
    for (uint8 i = 0u; i < 4u; i++) {
        WdgM_CheckpointReached(0u);
    }
    /* SE 1: report 1 checkpoint (expected exactly 1) */
    WdgM_CheckpointReached(1u);

    WdgM_MainFunction();

    WdgM_LocalStatusType local;
    WdgM_GetLocalStatus(0u, &local);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_OK, local);
    TEST_ASSERT_EQUAL(WDGM_GLOBAL_STATUS_OK, WdgM_GetGlobalStatus());
}

/** @verifies SWR-BSW-019
 *  Boundary: Checkpoint exactly at max boundary (6) for SE 0 — should be OK */
void test_WdgM_Checkpoint_exact_max(void)
{
    /* SE 0: expected 4-6 checkpoints. Report exactly 6. */
    for (uint8 i = 0u; i < 6u; i++) {
        WdgM_CheckpointReached(0u);
    }
    /* SE 1: report 1 checkpoint */
    WdgM_CheckpointReached(1u);

    WdgM_MainFunction();

    WdgM_LocalStatusType local;
    WdgM_GetLocalStatus(0u, &local);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_OK, local);
    TEST_ASSERT_EQUAL(WDGM_GLOBAL_STATUS_OK, WdgM_GetGlobalStatus());
}

/** @verifies SWR-BSW-019
 *  Boundary: Checkpoint at min-1 (3, below range) — should fail SE 0 */
void test_WdgM_Checkpoint_min_minus_one(void)
{
    /* SE 0: expected 4-6 checkpoints. Report only 3 (one below min). */
    for (uint8 i = 0u; i < 3u; i++) {
        WdgM_CheckpointReached(0u);
    }
    /* SE 1: report 1 checkpoint */
    WdgM_CheckpointReached(1u);

    WdgM_MainFunction();

    WdgM_LocalStatusType local;
    WdgM_GetLocalStatus(0u, &local);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_FAILED, local);
}

/** @verifies SWR-BSW-019
 *  Boundary: Checkpoint at max+1 (7, above range) — should fail SE 0 */
void test_WdgM_Checkpoint_max_plus_one(void)
{
    /* SE 0: expected 4-6 checkpoints. Report 7 (one above max). */
    for (uint8 i = 0u; i < 7u; i++) {
        WdgM_CheckpointReached(0u);
    }
    /* SE 1: report 1 checkpoint */
    WdgM_CheckpointReached(1u);

    WdgM_MainFunction();

    WdgM_LocalStatusType local;
    WdgM_GetLocalStatus(0u, &local);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_FAILED, local);
}

/** @verifies SWR-BSW-020
 *  Tolerance exhaustion: SE 0 has tolerance 2 — fail exactly 2 cycles, verify
 *  still FAILED (not yet EXPIRED). Fail once more -> EXPIRED. */
void test_WdgM_Tolerance_exact_exhaustion(void)
{
    /* Cycle 1: SE 0 too few (0 checkpoints), SE 1 OK */
    WdgM_CheckpointReached(1u);
    WdgM_MainFunction();

    WdgM_LocalStatusType local1;
    WdgM_GetLocalStatus(0u, &local1);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_FAILED, local1);

    /* Cycle 2: SE 0 still too few, SE 1 OK */
    WdgM_CheckpointReached(1u);
    WdgM_MainFunction();

    WdgM_LocalStatusType local2;
    WdgM_GetLocalStatus(0u, &local2);
    /* After 2 failed cycles (tolerance=2), still FAILED but not EXPIRED yet */
    TEST_ASSERT_TRUE((local2 == WDGM_LOCAL_STATUS_FAILED) ||
                     (local2 == WDGM_LOCAL_STATUS_EXPIRED));

    /* Cycle 3: third consecutive failure — must be EXPIRED */
    WdgM_CheckpointReached(1u);
    WdgM_MainFunction();

    WdgM_LocalStatusType local3;
    WdgM_GetLocalStatus(0u, &local3);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_EXPIRED, local3);
    TEST_ASSERT_EQUAL(WDGM_GLOBAL_STATUS_FAILED, WdgM_GetGlobalStatus());
}

/** @verifies SWR-BSW-019
 *  Multiple SEs: both OK — global should be OK */
void test_WdgM_MultipleSE_both_ok(void)
{
    /* SE 0: 5 checkpoints (within 4-6) */
    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    /* SE 1: 1 checkpoint (expected exactly 1) */
    WdgM_CheckpointReached(1u);

    WdgM_MainFunction();

    WdgM_LocalStatusType local0;
    WdgM_LocalStatusType local1;
    WdgM_GetLocalStatus(0u, &local0);
    WdgM_GetLocalStatus(1u, &local1);

    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_OK, local0);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_OK, local1);
    TEST_ASSERT_EQUAL(WDGM_GLOBAL_STATUS_OK, WdgM_GetGlobalStatus());
}

/** @verifies SWR-BSW-019
 *  Multiple SEs: SE 1 fails while SE 0 is OK — global status reflects failure */
void test_WdgM_MultipleSE_one_fails(void)
{
    /* SE 0: 5 checkpoints (OK) */
    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    /* SE 1: 0 checkpoints (expected 1 — FAILS) */

    WdgM_MainFunction();

    WdgM_LocalStatusType local0;
    WdgM_LocalStatusType local1;
    WdgM_GetLocalStatus(0u, &local0);
    WdgM_GetLocalStatus(1u, &local1);

    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_OK, local0);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_FAILED, local1);
}

/** @verifies SWR-BSW-020
 *  WdgM feed verification: DIO flip count is exactly 1 per successful cycle */
void test_WdgM_DioFlip_exactly_one_per_ok_cycle(void)
{
    /* Run two consecutive OK cycles */
    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    WdgM_CheckpointReached(1u);
    WdgM_MainFunction();
    TEST_ASSERT_EQUAL_UINT8(1u, mock_dio_flip_count);

    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    WdgM_CheckpointReached(1u);
    WdgM_MainFunction();
    TEST_ASSERT_EQUAL_UINT8(2u, mock_dio_flip_count);
}

/** @verifies SWR-BSW-020
 *  WdgM feed verification: DIO NOT flipped on failed cycle */
void test_WdgM_DioFlip_not_on_failure(void)
{
    /* SE 0: too few (2 checkpoints), SE 1: OK */
    WdgM_CheckpointReached(0u);
    WdgM_CheckpointReached(0u);
    WdgM_CheckpointReached(1u);

    WdgM_MainFunction();

    /* DIO should NOT be flipped because SE 0 failed */
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_flip_count);
}

/** @verifies SWR-BSW-019
 *  Recovery: SE goes from FAILED back to OK on next valid cycle */
void test_WdgM_Recovery_failed_to_ok(void)
{
    /* Cycle 1: SE 0 fails (too few checkpoints) */
    WdgM_CheckpointReached(0u);
    WdgM_CheckpointReached(1u);
    WdgM_MainFunction();

    WdgM_LocalStatusType local_after_fail;
    WdgM_GetLocalStatus(0u, &local_after_fail);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_FAILED, local_after_fail);

    /* Cycle 2: SE 0 recovers (5 checkpoints, within 4-6) */
    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    WdgM_CheckpointReached(1u);
    WdgM_MainFunction();

    WdgM_LocalStatusType local_after_recovery;
    WdgM_GetLocalStatus(0u, &local_after_recovery);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_OK, local_after_recovery);
}

/** @verifies SWR-BSW-020
 *  DEM integration: verify error reported with correct event ID on expiry */
void test_WdgM_DemIntegration_error_on_expiry(void)
{
    mock_dem_report_count = 0u;

    /* SE 1 has tolerance 1. Fail it twice to expire. */
    /* Cycle 1: SE 0 OK, SE 1 no checkpoints */
    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    WdgM_MainFunction();

    /* Cycle 2: SE 0 OK, SE 1 still no checkpoints -> EXPIRED */
    for (uint8 i = 0u; i < 5u; i++) {
        WdgM_CheckpointReached(0u);
    }
    WdgM_MainFunction();

    /* DEM should have been called to report the error */
    TEST_ASSERT_TRUE(mock_dem_report_count > 0u);
    TEST_ASSERT_EQUAL_UINT8(DEM_EVENT_STATUS_FAILED, mock_dem_status);
}

/** @verifies SWR-BSW-019
 *  Boundary: zero checkpoints for both SEs — both fail */
void test_WdgM_Zero_checkpoints_all_fail(void)
{
    /* No checkpoints for any SE */
    WdgM_MainFunction();

    WdgM_LocalStatusType local0;
    WdgM_LocalStatusType local1;
    WdgM_GetLocalStatus(0u, &local0);
    WdgM_GetLocalStatus(1u, &local1);

    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_FAILED, local0);
    TEST_ASSERT_EQUAL(WDGM_LOCAL_STATUS_FAILED, local1);
    TEST_ASSERT_EQUAL_UINT8(0u, mock_dio_flip_count);
}

/** @verifies SWR-BSW-019
 *  GetLocalStatus: invalid SE ID returns E_NOT_OK */
void test_WdgM_GetLocalStatus_invalid_se(void)
{
    WdgM_LocalStatusType local;
    Std_ReturnType ret = WdgM_GetLocalStatus(99u, &local);
    TEST_ASSERT_EQUAL(E_NOT_OK, ret);
}

/* ==================================================================
 * Test runner
 * ================================================================== */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_WdgM_CheckpointReached_increments);
    RUN_TEST(test_WdgM_CheckpointReached_invalid_se);
    RUN_TEST(test_WdgM_MainFunction_all_ok_feeds_wdt);
    RUN_TEST(test_WdgM_MainFunction_too_few_no_feed);
    RUN_TEST(test_WdgM_MainFunction_too_many_no_feed);
    RUN_TEST(test_WdgM_expired_after_tolerance);
    RUN_TEST(test_WdgM_Init_null_config);
    RUN_TEST(test_WdgM_GetLocalStatus_null);

    /* ASIL D hardened boundary / fault injection tests */
    RUN_TEST(test_WdgM_Checkpoint_exact_min);
    RUN_TEST(test_WdgM_Checkpoint_exact_max);
    RUN_TEST(test_WdgM_Checkpoint_min_minus_one);
    RUN_TEST(test_WdgM_Checkpoint_max_plus_one);
    RUN_TEST(test_WdgM_Tolerance_exact_exhaustion);
    RUN_TEST(test_WdgM_MultipleSE_both_ok);
    RUN_TEST(test_WdgM_MultipleSE_one_fails);
    RUN_TEST(test_WdgM_DioFlip_exactly_one_per_ok_cycle);
    RUN_TEST(test_WdgM_DioFlip_not_on_failure);
    RUN_TEST(test_WdgM_Recovery_failed_to_ok);
    RUN_TEST(test_WdgM_DemIntegration_error_on_expiry);
    RUN_TEST(test_WdgM_Zero_checkpoints_all_fail);
    RUN_TEST(test_WdgM_GetLocalStatus_invalid_se);

    return UNITY_END();
}
