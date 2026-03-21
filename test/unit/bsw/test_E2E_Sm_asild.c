/**
 * @file    test_E2E_SM_asild.c
 * @brief   Unit tests for E2E Supervision State Machine
 * @date    2026-03-21
 *
 * @verifies SWR-BSW-023, SWR-BSW-024, TSR-022, TSR-023
 *
 * TDD: Tests define desired state machine behavior.
 */
#include "unity.h"
#include "E2E.h"

static const E2E_SMConfigType sm_config = {
    .WindowSizeValid   = 3u,
    .WindowSizeInvalid = 2u,
    .WindowSizeInit    = 1u,
};

static E2E_SMType sm;

void setUp(void)  { E2E_SMInit(&sm); }
void tearDown(void) {}

void test_SM_init_NODATA(void)
{
    TEST_ASSERT_EQUAL(E2E_SM_NODATA, sm.State);
}

void test_SM_first_OK_goes_INIT(void)
{
    /* First check: NODATA→INIT (any data received moves to INIT) */
    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_OK);
    TEST_ASSERT_EQUAL(E2E_SM_INIT, s);
}

void test_SM_second_OK_goes_VALID(void)
{
    /* WindowSizeInit=1: need 1 OK in INIT state to go VALID */
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_OK);  /* NODATA→INIT */
    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_OK);  /* INIT→VALID */
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

void test_SM_first_ERROR_goes_INIT(void)
{
    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    TEST_ASSERT_EQUAL(E2E_SM_INIT, s);
}

void test_SM_two_errors_go_INVALID(void)
{
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/** Helper: get SM to VALID state (NODATA→INIT→VALID) */
static void go_to_valid(void)
{
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_OK);  /* NODATA→INIT */
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_OK);  /* INIT→VALID */
}

void test_SM_one_error_in_VALID_tolerated(void)
{
    go_to_valid();
    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

void test_SM_two_errors_in_VALID_go_INVALID(void)
{
    go_to_valid();
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

void test_SM_error_OK_error_stays_VALID(void)
{
    go_to_valid();
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_OK);  /* resets err count */
    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

void test_SM_recovery_needs_3_OK(void)
{
    go_to_valid();
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);  /* INVALID */

    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_OK);
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_OK);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, sm.State);  /* 2 < 3 */

    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_OK);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);  /* 3 = 3 → recovered */
}

void test_SM_REPEATED_is_not_OK(void)
{
    go_to_valid();
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_ERROR);  /* INVALID */

    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_REPEATED);
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_REPEATED);
    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_REPEATED);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);  /* REPEATED ≠ OK */
}

void test_SM_WRONG_SEQ_is_error(void)
{
    go_to_valid();
    E2E_SMCheck(&sm_config, &sm, E2E_STATUS_WRONG_SEQ);
    E2E_SMStateType s = E2E_SMCheck(&sm_config, &sm, E2E_STATUS_WRONG_SEQ);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

void test_SM_null_returns_INVALID(void)
{
    E2E_SMStateType s = E2E_SMCheck(NULL_PTR, &sm, E2E_STATUS_OK);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_SM_init_NODATA);
    RUN_TEST(test_SM_first_OK_goes_INIT);
    RUN_TEST(test_SM_second_OK_goes_VALID);
    RUN_TEST(test_SM_first_ERROR_goes_INIT);
    RUN_TEST(test_SM_two_errors_go_INVALID);
    RUN_TEST(test_SM_one_error_in_VALID_tolerated);
    RUN_TEST(test_SM_two_errors_in_VALID_go_INVALID);
    RUN_TEST(test_SM_error_OK_error_stays_VALID);
    RUN_TEST(test_SM_recovery_needs_3_OK);
    RUN_TEST(test_SM_REPEATED_is_not_OK);
    RUN_TEST(test_SM_WRONG_SEQ_is_error);
    RUN_TEST(test_SM_null_returns_INVALID);
    return UNITY_END();
}
