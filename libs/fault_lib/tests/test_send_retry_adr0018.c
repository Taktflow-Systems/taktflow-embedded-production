/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Taktflow Systems
 *
 * Phase 5 Line B D6 — unit test for the ADR-0018 bounded-retry send
 * path in fault_lib.c. We do NOT talk to a real socket here; the test
 * injects a mock `sender` function via fault_lib_send_with_retry that
 * returns EAGAIN a controllable number of times before reporting the
 * real bytes as sent. This proves:
 *
 *   1. EAGAIN within the retry budget -> the frame still lands.
 *   2. EAGAIN beyond the retry budget -> FAULT_LIB_ERR_WRITE (bounded).
 *   3. A clean send -> FAULT_LIB_OK, no retries consumed.
 *   4. A non-transient error -> FAULT_LIB_ERR_WRITE immediately,
 *      no retry budget wasted.
 *   5. EINTR -> infinite retry (budget not consumed) because EINTR
 *      is a signal delivery, not a transient backpressure event.
 *
 * Runs on Windows and Linux because fault_lib_send_with_retry is
 * declared unconditionally (the stubbed Windows transport only
 * skips the real send() wrapper, not the retry helper itself).
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fault_lib.h"
#include "unity.h"

/* Forward declaration mirrors the internal retry helper in fault_lib.c. */
typedef long (*fault_lib_send_fn)(int fd, const void *buf, size_t len, void *ctx);
extern int fault_lib_send_with_retry(int fd, const uint8_t *buf, size_t len,
                                      fault_lib_send_fn sender, void *ctx);

/* Test harness sender context. */
typedef struct {
    int    eagain_remaining;   /* how many times to return -1/EAGAIN */
    int    eintr_remaining;    /* how many times to return -1/EINTR */
    int    bail_after;         /* fail with -1/EIO after N successes (0 = never) */
    int    success_count;      /* successful calls so far */
    size_t total_sent;         /* bytes ack'd */
} mock_ctx_t;

static long mock_sender(int fd, const void *buf, size_t len, void *vctx)
{
    (void)fd;
    (void)buf;
    mock_ctx_t *ctx = (mock_ctx_t *)vctx;
    if (ctx->eintr_remaining > 0) {
        ctx->eintr_remaining--;
        errno = EINTR;
        return -1;
    }
    if (ctx->eagain_remaining > 0) {
        ctx->eagain_remaining--;
        errno = EAGAIN;
        return -1;
    }
    if ((ctx->bail_after > 0) && (ctx->success_count >= ctx->bail_after)) {
        errno = EIO;
        return -1;
    }
    ctx->success_count++;
    ctx->total_sent += len;
    return (long)len;
}

void setUp(void) {}
void tearDown(void) {}

static const uint8_t PAYLOAD[12] = {
    0xDE, 0xAD, 0xBE, 0xEF,
    0x12, 0x34, 0x56, 0x78,
    0x9A, 0xBC, 0xDE, 0xF0
};

void test_clean_send_no_retries(void)
{
    mock_ctx_t ctx = {0};
    int rc = fault_lib_send_with_retry(
        /*fd=*/0, PAYLOAD, sizeof(PAYLOAD), mock_sender, &ctx);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(sizeof(PAYLOAD), ctx.total_sent);
    TEST_ASSERT_EQUAL_INT(1, ctx.success_count);
}

void test_eagain_within_budget_retries_then_succeeds(void)
{
    mock_ctx_t ctx = {0};
    /* FAULT_LIB_SEND_RETRY_MAX is 3; 2 EAGAINs must be absorbed. */
    ctx.eagain_remaining = 2;
    int rc = fault_lib_send_with_retry(
        /*fd=*/0, PAYLOAD, sizeof(PAYLOAD), mock_sender, &ctx);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(sizeof(PAYLOAD), ctx.total_sent);
    TEST_ASSERT_EQUAL_INT(1, ctx.success_count);
}

void test_eagain_beyond_budget_bounded_fails(void)
{
    mock_ctx_t ctx = {0};
    /* 10 consecutive EAGAINs overshoot the 3-retry budget. */
    ctx.eagain_remaining = 10;
    int rc = fault_lib_send_with_retry(
        /*fd=*/0, PAYLOAD, sizeof(PAYLOAD), mock_sender, &ctx);
    TEST_ASSERT_EQUAL_INT(-6 /* FAULT_LIB_ERR_WRITE */, rc);
    /* ctx.eagain_remaining still > 0 because we gave up */
    TEST_ASSERT_GREATER_THAN_INT(0, ctx.eagain_remaining);
    /* no real send completed */
    TEST_ASSERT_EQUAL_UINT(0, ctx.total_sent);
}

void test_non_transient_error_fails_immediately(void)
{
    mock_ctx_t ctx = {0};
    ctx.bail_after = 0; /* never succeed */
    /* Make the first call report EIO directly (not EAGAIN/EINTR). */
    /* We implement this by returning -1/EIO via bail_after=-1 trick:
     * set bail_after so (success_count >= bail_after) is true up
     * front. success_count starts at 0, so bail_after=0 falls
     * through; use bail_after trick differently — set -1 means
     * "always bail". mock_sender checks `bail_after > 0`. So
     * instead mark ctx with a dedicated "force_eio_immediately". */
    (void)ctx;
    /* Use a local sender that always returns EIO. */
    long (*immediate_eio)(int, const void *, size_t, void *) =
        NULL;
    immediate_eio = immediate_eio; /* silence unused warning */

    /* We inline the sender here instead of a new function to keep the
     * test file tidy. */
    struct local_ctx { int called; } lctx = {0};
    /* A scoped sender is not possible in C99; use a separate fn. */
    extern long eio_sender(int fd, const void *buf, size_t len, void *vctx);
    int rc = fault_lib_send_with_retry(
        /*fd=*/0, PAYLOAD, sizeof(PAYLOAD), eio_sender, &lctx);
    TEST_ASSERT_EQUAL_INT(-6 /* FAULT_LIB_ERR_WRITE */, rc);
    TEST_ASSERT_EQUAL_INT(1, lctx.called); /* exactly one try, no retry */
}

long eio_sender(int fd, const void *buf, size_t len, void *vctx)
{
    (void)fd;
    (void)buf;
    (void)len;
    struct local_ctx { int called; } *l = (struct local_ctx *)vctx;
    l->called++;
    errno = EIO;
    return -1;
}

void test_eintr_does_not_burn_retry_budget(void)
{
    mock_ctx_t ctx = {0};
    ctx.eintr_remaining  = 20;   /* many interrupts */
    ctx.eagain_remaining = 0;
    int rc = fault_lib_send_with_retry(
        /*fd=*/0, PAYLOAD, sizeof(PAYLOAD), mock_sender, &ctx);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT(sizeof(PAYLOAD), ctx.total_sent);
    TEST_ASSERT_EQUAL_INT(1, ctx.success_count);
    TEST_ASSERT_EQUAL_INT(0, ctx.eintr_remaining);
}

void test_null_sender_rejected(void)
{
    int rc = fault_lib_send_with_retry(0, PAYLOAD, sizeof(PAYLOAD), NULL, NULL);
    TEST_ASSERT_EQUAL_INT(-1 /* FAULT_LIB_ERR_INVAL */, rc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_clean_send_no_retries);
    RUN_TEST(test_eagain_within_budget_retries_then_succeeds);
    RUN_TEST(test_eagain_beyond_budget_bounded_fails);
    RUN_TEST(test_non_transient_error_fails_immediately);
    RUN_TEST(test_eintr_does_not_burn_retry_budget);
    RUN_TEST(test_null_sender_rejected);
    return UNITY_END();
}
