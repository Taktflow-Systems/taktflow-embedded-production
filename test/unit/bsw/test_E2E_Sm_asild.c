/**
 * @file    test_E2E_Sm_asild.c
 * @brief   Unit tests for E2E State Machine — sliding window evaluator
 * @date    2026-03-02
 *
 * @verifies SWR-BSW-026
 *
 * Tests all state transitions (INIT/VALID/INVALID), sliding window
 * behavior, boundary values, NULL handling, and configuration edge cases.
 */
#include "unity.h"

/* ====================================================================
 * Local type definitions (self-contained — no BSW headers)
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

/* Block real headers */
#define PLATFORM_TYPES_H
#define STD_TYPES_H
#define E2E_H

/* E2E types needed by E2E_Sm */
typedef enum {
    E2E_STATUS_OK           = 0u,
    E2E_STATUS_REPEATED     = 1u,
    E2E_STATUS_WRONG_SEQ    = 2u,
    E2E_STATUS_ERROR        = 3u,
    E2E_STATUS_NO_NEW_DATA  = 4u
} E2E_CheckStatusType;

/* ====================================================================
 * Include header under test (source linked by Makefile)
 * ==================================================================== */

#include "../services/E2E_Sm.h"

/* ====================================================================
 * Test configuration — typical heartbeat
 * ==================================================================== */

static const E2E_SmConfigType cfg_typical = {
    6u,  /* WindowSize */
    3u,  /* MinOkStateInit */
    3u,  /* MaxErrorStateValid */
    5u   /* MinOkStateInvalid */
};

static const E2E_SmConfigType cfg_fzc = {
    4u,  /* WindowSize */
    2u,  /* MinOkStateInit */
    1u,  /* MaxErrorStateValid */
    3u   /* MinOkStateInvalid */
};

static const E2E_SmConfigType cfg_rzc = {
    6u,  /* WindowSize */
    3u,  /* MinOkStateInit */
    2u,  /* MaxErrorStateValid */
    3u   /* MinOkStateInvalid */
};

static E2E_SmStateType state;

/* ====================================================================
 * setUp / tearDown
 * ==================================================================== */

void setUp(void)
{
    E2E_Sm_Init(&state);
}

void tearDown(void) { }

/* ====================================================================
 * Helper: feed N identical results
 * ==================================================================== */

static E2E_SmStatusType feed_n(const E2E_SmConfigType *cfg,
                                E2E_SmStateType *st,
                                E2E_CheckStatusType status,
                                uint8 n)
{
    E2E_SmStatusType result = st->Status;
    uint8 i;
    for (i = 0u; i < n; i++) {
        result = E2E_Sm_Check(cfg, st, status);
    }
    return result;
}

/* ====================================================================
 * INIT State Tests
 * ==================================================================== */

/** @verifies SWR-BSW-026 -- Init sets state to E2E_SM_INIT */
void test_SM_Init_state(void)
{
    TEST_ASSERT_EQUAL(E2E_SM_INIT, state.Status);
    TEST_ASSERT_EQUAL(0u, state.OkCount);
    TEST_ASSERT_EQUAL(0u, state.ErrorCount);
    TEST_ASSERT_EQUAL(0u, state.SampleCount);
}

/** @verifies SWR-BSW-026 -- Stays in INIT with only errors */
void test_SM_stays_INIT_with_errors(void)
{
    E2E_SmStatusType s = feed_n(&cfg_typical, &state, E2E_STATUS_NO_NEW_DATA, 10u);
    TEST_ASSERT_EQUAL(E2E_SM_INIT, s);
}

/** @verifies SWR-BSW-026 -- Stays in INIT with insufficient OKs */
void test_SM_stays_INIT_below_threshold(void)
{
    E2E_SmStatusType s;
    s = feed_n(&cfg_typical, &state, E2E_STATUS_OK, 2u);
    TEST_ASSERT_EQUAL(E2E_SM_INIT, s);
}

/** @verifies SWR-BSW-026 -- INIT → VALID at MinOkStateInit */
void test_SM_INIT_to_VALID(void)
{
    E2E_SmStatusType s;
    s = feed_n(&cfg_typical, &state, E2E_STATUS_OK, 3u);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

/** @verifies SWR-BSW-026 -- INIT → VALID with mixed OK/ERROR (enough OKs in window) */
void test_SM_INIT_to_VALID_mixed(void)
{
    E2E_SmStatusType s;
    /* Feed: OK, ERROR, OK, ERROR, OK → 3 OKs in window */
    (void)E2E_Sm_Check(&cfg_typical, &state, E2E_STATUS_OK);
    (void)E2E_Sm_Check(&cfg_typical, &state, E2E_STATUS_ERROR);
    (void)E2E_Sm_Check(&cfg_typical, &state, E2E_STATUS_OK);
    (void)E2E_Sm_Check(&cfg_typical, &state, E2E_STATUS_ERROR);
    s = E2E_Sm_Check(&cfg_typical, &state, E2E_STATUS_OK);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

/* ====================================================================
 * VALID State Tests
 * ==================================================================== */

/** @verifies SWR-BSW-026 -- Stays VALID with continuous OKs */
void test_SM_stays_VALID_with_OKs(void)
{
    feed_n(&cfg_typical, &state, E2E_STATUS_OK, 3u); /* INIT → VALID */
    E2E_SmStatusType s = feed_n(&cfg_typical, &state, E2E_STATUS_OK, 10u);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

/** @verifies SWR-BSW-026 -- Stays VALID at MaxErrorStateValid errors */
void test_SM_stays_VALID_at_max_errors(void)
{
    E2E_SmStatusType s;
    /* Fill window with OKs → VALID */
    feed_n(&cfg_typical, &state, E2E_STATUS_OK, 6u);
    /* Feed exactly MaxErrorStateValid (3) errors — window: [OK,OK,OK,ERR,ERR,ERR] */
    s = feed_n(&cfg_typical, &state, E2E_STATUS_NO_NEW_DATA, 3u);
    /* ErrorCount=3 == MaxErrorStateValid=3: stays VALID (transition at >3) */
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

/** @verifies SWR-BSW-026 -- VALID → INVALID when errors exceed threshold */
void test_SM_VALID_to_INVALID(void)
{
    E2E_SmStatusType s;
    /* Fill window with OKs → VALID */
    feed_n(&cfg_typical, &state, E2E_STATUS_OK, 6u);
    /* Feed 4 errors — window: [OK,OK,ERR,ERR,ERR,ERR] → ErrorCount=4 > 3 */
    s = feed_n(&cfg_typical, &state, E2E_STATUS_NO_NEW_DATA, 4u);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/** @verifies SWR-BSW-026 -- VALID → INVALID with REPEATED status (counts as error) */
void test_SM_REPEATED_counts_as_error(void)
{
    E2E_SmStatusType s;
    feed_n(&cfg_typical, &state, E2E_STATUS_OK, 6u);
    s = feed_n(&cfg_typical, &state, E2E_STATUS_REPEATED, 4u);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/** @verifies SWR-BSW-026 -- VALID → INVALID with WRONG_SEQ status */
void test_SM_WRONG_SEQ_counts_as_error(void)
{
    E2E_SmStatusType s;
    feed_n(&cfg_typical, &state, E2E_STATUS_OK, 6u);
    s = feed_n(&cfg_typical, &state, E2E_STATUS_WRONG_SEQ, 4u);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/* ====================================================================
 * INVALID State Tests
 * ==================================================================== */

/** @verifies SWR-BSW-026 -- Stays INVALID with insufficient OKs */
void test_SM_stays_INVALID_below_threshold(void)
{
    E2E_SmStatusType s;
    /* INIT → VALID → INVALID */
    feed_n(&cfg_typical, &state, E2E_STATUS_OK, 6u);
    feed_n(&cfg_typical, &state, E2E_STATUS_ERROR, 4u);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, state.Status);
    /* Feed 4 OKs — below MinOkStateInvalid (5) */
    s = feed_n(&cfg_typical, &state, E2E_STATUS_OK, 4u);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/** @verifies SWR-BSW-026 -- INVALID → VALID after MinOkStateInvalid OKs */
void test_SM_INVALID_to_VALID(void)
{
    E2E_SmStatusType s;
    /* INIT → VALID → INVALID */
    feed_n(&cfg_typical, &state, E2E_STATUS_OK, 6u);
    feed_n(&cfg_typical, &state, E2E_STATUS_ERROR, 4u);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, state.Status);
    /* Feed 5 OKs → MinOkStateInvalid met */
    s = feed_n(&cfg_typical, &state, E2E_STATUS_OK, 5u);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

/* ====================================================================
 * Sliding Window Tests
 * ==================================================================== */

/** @verifies SWR-BSW-026 -- Window evicts oldest entry correctly */
void test_SM_window_eviction(void)
{
    /* Fill window with 6 OKs → VALID */
    feed_n(&cfg_typical, &state, E2E_STATUS_OK, 6u);
    TEST_ASSERT_EQUAL(6u, state.OkCount);
    TEST_ASSERT_EQUAL(0u, state.ErrorCount);

    /* Feed 3 errors → evicts 3 old OKs: window now [OK,OK,OK,ERR,ERR,ERR] */
    feed_n(&cfg_typical, &state, E2E_STATUS_ERROR, 3u);
    TEST_ASSERT_EQUAL(3u, state.OkCount);
    TEST_ASSERT_EQUAL(3u, state.ErrorCount);
}

/** @verifies SWR-BSW-026 -- SampleCount saturates at WindowSize */
void test_SM_sample_count_saturates(void)
{
    feed_n(&cfg_typical, &state, E2E_STATUS_OK, 20u);
    TEST_ASSERT_EQUAL(6u, state.SampleCount);
}

/* ====================================================================
 * Per-ECU Configuration Tests (FTTI compliance)
 * ==================================================================== */

/** @verifies SWR-BSW-026 -- FZC: INIT → VALID after 2 OKs (100ms) */
void test_SM_FZC_INIT_to_VALID(void)
{
    E2E_SmStatusType s = feed_n(&cfg_fzc, &state, E2E_STATUS_OK, 2u);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

/** @verifies SWR-BSW-026 -- FZC: VALID → INVALID after 2 errors (100ms detection) */
void test_SM_FZC_timeout_at_2_errors(void)
{
    E2E_SmStatusType s;
    feed_n(&cfg_fzc, &state, E2E_STATUS_OK, 4u); /* fill window → VALID */
    s = feed_n(&cfg_fzc, &state, E2E_STATUS_NO_NEW_DATA, 2u);
    /* ErrorCount=2 > MaxErrValid=1 → INVALID */
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/** @verifies SWR-BSW-026 -- FZC: recovery after 3 OKs */
void test_SM_FZC_recovery(void)
{
    E2E_SmStatusType s;
    feed_n(&cfg_fzc, &state, E2E_STATUS_OK, 4u);
    feed_n(&cfg_fzc, &state, E2E_STATUS_NO_NEW_DATA, 2u); /* INVALID */
    s = feed_n(&cfg_fzc, &state, E2E_STATUS_OK, 3u);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

/** @verifies SWR-BSW-026 -- RZC: VALID → INVALID after 3 errors (150ms detection) */
void test_SM_RZC_timeout_at_3_errors(void)
{
    E2E_SmStatusType s;
    feed_n(&cfg_rzc, &state, E2E_STATUS_OK, 6u); /* fill window → VALID */
    s = feed_n(&cfg_rzc, &state, E2E_STATUS_NO_NEW_DATA, 3u);
    /* ErrorCount=3 > MaxErrValid=2 → INVALID */
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/** @verifies SWR-BSW-026 -- RZC: stays VALID at 2 errors */
void test_SM_RZC_stays_VALID_at_2_errors(void)
{
    E2E_SmStatusType s;
    feed_n(&cfg_rzc, &state, E2E_STATUS_OK, 6u);
    s = feed_n(&cfg_rzc, &state, E2E_STATUS_NO_NEW_DATA, 2u);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

/* ====================================================================
 * Defensive / NULL Tests
 * ==================================================================== */

/** @verifies SWR-BSW-026 -- NULL State pointer handled safely */
void test_SM_Init_NULL_state(void)
{
    /* Should not crash */
    E2E_Sm_Init(NULL_PTR);
}

/** @verifies SWR-BSW-026 -- NULL Config returns INVALID */
void test_SM_Check_NULL_config(void)
{
    E2E_SmStatusType s = E2E_Sm_Check(NULL_PTR, &state, E2E_STATUS_OK);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/** @verifies SWR-BSW-026 -- NULL State returns INVALID */
void test_SM_Check_NULL_state(void)
{
    E2E_SmStatusType s = E2E_Sm_Check(&cfg_typical, NULL_PTR, E2E_STATUS_OK);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/** @verifies SWR-BSW-026 -- WindowSize=0 returns INVALID */
void test_SM_Check_zero_window(void)
{
    E2E_SmConfigType bad_cfg = { 0u, 1u, 1u, 1u };
    E2E_SmStatusType s = E2E_Sm_Check(&bad_cfg, &state, E2E_STATUS_OK);
    TEST_ASSERT_EQUAL(E2E_SM_INVALID, s);
}

/** @verifies SWR-BSW-026 -- WindowSize > max clamped */
void test_SM_Check_oversized_window(void)
{
    E2E_SmConfigType big_cfg = { 255u, 1u, 1u, 1u };
    E2E_SmStatusType s;
    /* Should not crash — window clamped to E2E_SM_MAX_WINDOW */
    s = feed_n(&big_cfg, &state, E2E_STATUS_OK, 2u);
    TEST_ASSERT_EQUAL(E2E_SM_VALID, s);
}

/* ====================================================================
 * Test runner
 * ==================================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* INIT state */
    RUN_TEST(test_SM_Init_state);
    RUN_TEST(test_SM_stays_INIT_with_errors);
    RUN_TEST(test_SM_stays_INIT_below_threshold);
    RUN_TEST(test_SM_INIT_to_VALID);
    RUN_TEST(test_SM_INIT_to_VALID_mixed);

    /* VALID state */
    RUN_TEST(test_SM_stays_VALID_with_OKs);
    RUN_TEST(test_SM_stays_VALID_at_max_errors);
    RUN_TEST(test_SM_VALID_to_INVALID);
    RUN_TEST(test_SM_REPEATED_counts_as_error);
    RUN_TEST(test_SM_WRONG_SEQ_counts_as_error);

    /* INVALID state */
    RUN_TEST(test_SM_stays_INVALID_below_threshold);
    RUN_TEST(test_SM_INVALID_to_VALID);

    /* Sliding window */
    RUN_TEST(test_SM_window_eviction);
    RUN_TEST(test_SM_sample_count_saturates);

    /* Per-ECU FTTI */
    RUN_TEST(test_SM_FZC_INIT_to_VALID);
    RUN_TEST(test_SM_FZC_timeout_at_2_errors);
    RUN_TEST(test_SM_FZC_recovery);
    RUN_TEST(test_SM_RZC_timeout_at_3_errors);
    RUN_TEST(test_SM_RZC_stays_VALID_at_2_errors);

    /* Defensive */
    RUN_TEST(test_SM_Init_NULL_state);
    RUN_TEST(test_SM_Check_NULL_config);
    RUN_TEST(test_SM_Check_NULL_state);
    RUN_TEST(test_SM_Check_zero_window);
    RUN_TEST(test_SM_Check_oversized_window);

    return UNITY_END();
}
