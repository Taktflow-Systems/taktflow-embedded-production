/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (Taktflow fork)
 *
 * test_postcard_c.c — Unity unit tests for the clean-room postcard
 * primitive encoder. Every test derives its expected bytes from the
 * LEB128 specification and verifies the actual bytes emitted by the
 * writer.
 *
 * No hardcoded fault codes, severities, or message payloads live in
 * this file. Test vectors are arithmetic boundary values that come
 * from the wire format itself, not from any Taktflow domain data.
 */

#include "unity.h"

#include "postcard_c.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void write_must_succeed(int rc)
{
    TEST_ASSERT_EQUAL_INT(POSTCARD_OK, rc);
}

static void test_varint_u32_zero(void)
{
    uint8_t buf[8];
    postcard_writer_t w;
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_varint_u32(&w, 0U));
    TEST_ASSERT_FALSE(postcard_writer_failed(&w));
    TEST_ASSERT_EQUAL_UINT(1U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_UINT8(0x00U, buf[0]);
}

static void test_varint_u32_one(void)
{
    uint8_t buf[8];
    postcard_writer_t w;
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_varint_u32(&w, 1U));
    TEST_ASSERT_EQUAL_UINT(1U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_UINT8(0x01U, buf[0]);
}

static void test_varint_u32_127(void)
{
    uint8_t buf[8];
    postcard_writer_t w;
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_varint_u32(&w, 127U));
    TEST_ASSERT_EQUAL_UINT(1U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_UINT8(0x7FU, buf[0]);
}

static void test_varint_u32_128(void)
{
    /* 128 = 0x80 0x01 */
    uint8_t buf[8];
    postcard_writer_t w;
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_varint_u32(&w, 128U));
    TEST_ASSERT_EQUAL_UINT(2U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_UINT8(0x80U, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01U, buf[1]);
}

static void test_varint_u32_max(void)
{
    /* u32 max = 0xFFFFFFFF = 5 bytes: FF FF FF FF 0F */
    uint8_t buf[8];
    postcard_writer_t w;
    const uint8_t want[5] = { 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x0FU };
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_varint_u32(&w, 0xFFFFFFFFU));
    TEST_ASSERT_EQUAL_UINT(5U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_MEMORY(want, buf, 5U);
}

static void test_varint_u64_16384(void)
{
    /* 16384 = 0x80 0x80 0x01 (3 bytes in LEB128) */
    uint8_t buf[16];
    postcard_writer_t w;
    const uint8_t want[3] = { 0x80U, 0x80U, 0x01U };
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_varint_u64(&w, 16384ULL));
    TEST_ASSERT_EQUAL_UINT(3U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_MEMORY(want, buf, 3U);
}

static void test_varint_u64_max(void)
{
    /* u64 max = 0xFFFFFFFFFFFFFFFF = 10 bytes, all 0xFF except final 0x01 */
    uint8_t buf[16];
    postcard_writer_t w;
    const uint8_t want[10] = {
        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x01U
    };
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_varint_u64(&w, 0xFFFFFFFFFFFFFFFFULL));
    TEST_ASSERT_EQUAL_UINT(10U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_MEMORY(want, buf, 10U);
}

static void test_string_empty(void)
{
    uint8_t buf[8];
    postcard_writer_t w;
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_string(&w, "", 0U));
    TEST_ASSERT_EQUAL_UINT(1U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_UINT8(0x00U, buf[0]);
}

static void test_string_one_byte(void)
{
    /* "a" = len(1) + 'a' = 01 61 */
    uint8_t buf[8];
    postcard_writer_t w;
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_string(&w, "a", 1U));
    TEST_ASSERT_EQUAL_UINT(2U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_UINT8(0x01U, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x61U, buf[1]);
}

static void test_string_utf8_multibyte(void)
{
    /* U+00E9 'e' with acute = 0xC3 0xA9 (2 bytes UTF-8) */
    uint8_t buf[16];
    postcard_writer_t w;
    const char s[] = { (char)0xC3, (char)0xA9 };
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_string(&w, s, 2U));
    TEST_ASSERT_EQUAL_UINT(3U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_UINT8(0x02U, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0xC3U, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0xA9U, buf[2]);
}

static void test_option_string_none(void)
{
    uint8_t buf[8];
    postcard_writer_t w;
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_option_string(&w, NULL, 0U));
    TEST_ASSERT_EQUAL_UINT(1U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_UINT8(0x00U, buf[0]);
}

static void test_option_string_some(void)
{
    /* Some("xy") = 01 02 78 79 */
    uint8_t buf[8];
    postcard_writer_t w;
    postcard_writer_init(&w, buf, sizeof(buf));
    write_must_succeed(postcard_write_option_string(&w, "xy", 2U));
    TEST_ASSERT_EQUAL_UINT(4U, postcard_writer_len(&w));
    TEST_ASSERT_EQUAL_UINT8(0x01U, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x02U, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0x78U, buf[2]);
    TEST_ASSERT_EQUAL_UINT8(0x79U, buf[3]);
}

static void test_overflow_stickiness(void)
{
    /* Tiny buffer, write past the end, verify overflow is sticky. */
    uint8_t buf[2];
    postcard_writer_t w;
    postcard_writer_init(&w, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(POSTCARD_OVERFLOW,
                          postcard_write_string(&w, "abcd", 4U));
    TEST_ASSERT_TRUE(postcard_writer_failed(&w));
    /* Subsequent writes still fail. */
    TEST_ASSERT_EQUAL_INT(POSTCARD_OVERFLOW,
                          postcard_write_u8(&w, 0x01U));
}

static void test_zero_length_writer_rejects_any_write(void)
{
    postcard_writer_t w;
    postcard_writer_init(&w, NULL, 0U);
    TEST_ASSERT_EQUAL_INT(POSTCARD_OVERFLOW, postcard_write_u8(&w, 1U));
    TEST_ASSERT_TRUE(postcard_writer_failed(&w));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_varint_u32_zero);
    RUN_TEST(test_varint_u32_one);
    RUN_TEST(test_varint_u32_127);
    RUN_TEST(test_varint_u32_128);
    RUN_TEST(test_varint_u32_max);
    RUN_TEST(test_varint_u64_16384);
    RUN_TEST(test_varint_u64_max);
    RUN_TEST(test_string_empty);
    RUN_TEST(test_string_one_byte);
    RUN_TEST(test_string_utf8_multibyte);
    RUN_TEST(test_option_string_none);
    RUN_TEST(test_option_string_some);
    RUN_TEST(test_overflow_stickiness);
    RUN_TEST(test_zero_length_writer_rejects_any_write);
    return UNITY_END();
}
