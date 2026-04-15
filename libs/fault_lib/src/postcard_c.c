/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (Taktflow fork)
 *
 * postcard_c.c — clean-room LEB128 varint + string + option encoders.
 *
 * Only the subset used by WireFaultRecord (ADR-0017) is implemented.
 * No stdio, no heap allocation, no longjmp.
 */

#include "postcard_c.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int postcard_write_raw(postcard_writer_t *w, const uint8_t *src, size_t n)
{
    if (w == NULL) {
        return POSTCARD_INVAL;
    }
    if (w->overflow) {
        return POSTCARD_OVERFLOW;
    }
    if (n > (w->cap - w->len)) {
        w->overflow = true;
        return POSTCARD_OVERFLOW;
    }
    if ((n > 0U) && (src != NULL)) {
        (void)memcpy(&w->buf[w->len], src, n);
    }
    w->len += n;
    return POSTCARD_OK;
}

void postcard_writer_init(postcard_writer_t *w, uint8_t *buf, size_t cap)
{
    if (w == NULL) {
        return;
    }
    w->buf = buf;
    w->cap = (buf == NULL) ? 0U : cap;
    w->len = 0U;
    w->overflow = false;
}

size_t postcard_writer_len(const postcard_writer_t *w)
{
    return (w == NULL) ? 0U : w->len;
}

bool postcard_writer_failed(const postcard_writer_t *w)
{
    return (w == NULL) ? true : w->overflow;
}

int postcard_write_u8(postcard_writer_t *w, uint8_t v)
{
    return postcard_write_raw(w, &v, 1U);
}

int postcard_write_bool(postcard_writer_t *w, bool v)
{
    uint8_t byte = v ? (uint8_t)1U : (uint8_t)0U;
    return postcard_write_raw(w, &byte, 1U);
}

/*
 * LEB128 unsigned varint, postcard 1.x style:
 *
 *   while v >= 0x80:
 *       emit (v & 0x7F) | 0x80
 *       v >>= 7
 *   emit v
 *
 * Maximum length: ceil(32/7) = 5 bytes for u32, ceil(64/7) = 10 for u64.
 */
int postcard_write_varint_u32(postcard_writer_t *w, uint32_t v)
{
    uint8_t tmp[POSTCARD_MAX_VARINT_U32];
    size_t  i = 0U;
    uint32_t cur = v;
    while (cur >= 0x80U) {
        tmp[i] = (uint8_t)((cur & 0x7FU) | 0x80U);
        cur >>= 7;
        i++;
    }
    tmp[i] = (uint8_t)cur;
    i++;
    return postcard_write_raw(w, tmp, i);
}

int postcard_write_varint_u64(postcard_writer_t *w, uint64_t v)
{
    uint8_t tmp[POSTCARD_MAX_VARINT_U64];
    size_t  i = 0U;
    uint64_t cur = v;
    while (cur >= 0x80U) {
        tmp[i] = (uint8_t)((cur & 0x7FU) | 0x80U);
        cur >>= 7;
        i++;
    }
    tmp[i] = (uint8_t)cur;
    i++;
    return postcard_write_raw(w, tmp, i);
}

int postcard_write_string(postcard_writer_t *w, const char *bytes, size_t len)
{
    int rc;
    if ((bytes == NULL) && (len > 0U)) {
        return POSTCARD_INVAL;
    }
    /* postcard strings are length-prefixed utf-8. The length is a
     * varint over `usize`, which postcard 1.x serialises as the
     * same LEB128 unsigned encoding regardless of host width. Our
     * FAULT_LIB_MAX_META_JSON_LEN never exceeds a u32 range, so
     * varint_u32 is safe.
     */
    if (len > (size_t)0xFFFFFFFFU) {
        return POSTCARD_INVAL;
    }
    rc = postcard_write_varint_u32(w, (uint32_t)len);
    if (rc != POSTCARD_OK) {
        return rc;
    }
    return postcard_write_raw(w, (const uint8_t *)bytes, len);
}

int postcard_write_option_string(postcard_writer_t *w,
                                 const char *bytes, size_t len)
{
    int rc;
    if (bytes == NULL) {
        return postcard_write_u8(w, (uint8_t)0U);
    }
    rc = postcard_write_u8(w, (uint8_t)1U);
    if (rc != POSTCARD_OK) {
        return rc;
    }
    return postcard_write_string(w, bytes, len);
}
