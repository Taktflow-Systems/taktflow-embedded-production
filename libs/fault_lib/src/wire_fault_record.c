/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (Taktflow fork)
 *
 * wire_fault_record.c — encode a fault_lib_record_t into a framed
 * postcard WireFaultRecord matching opensovd-core/
 * crates/fault-sink-unix/src/codec.rs byte-for-byte.
 *
 * Field order is frozen in postcard_c.md and must match the Rust
 * struct declaration order.
 */

#include "wire_fault_record.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fault_lib.h"
#include "postcard_c.h"

/* Reserved prefix length (4-byte little-endian u32). */
#define WIRE_FRAME_PREFIX_LEN ((size_t)4U)

static bool validate_record(const fault_lib_record_t *r)
{
    if (r == NULL) {
        return false;
    }
    if (r->component == NULL) {
        return false;
    }
    if (r->component_len == 0U) {
        return false;
    }
    if (r->component_len > FAULT_LIB_MAX_COMPONENT_LEN) {
        return false;
    }
    if ((r->meta.json == NULL) && (r->meta.len != 0U)) {
        return false;
    }
    if (r->meta.len > FAULT_LIB_MAX_META_JSON_LEN) {
        return false;
    }
    switch (r->severity) {
        case FAULT_LIB_SEVERITY_FATAL:
        case FAULT_LIB_SEVERITY_ERROR:
        case FAULT_LIB_SEVERITY_WARNING:
        case FAULT_LIB_SEVERITY_INFO:
            break;
        default:
            return false;
    }
    return true;
}

int wire_fault_record_encode_frame(uint8_t *buf, size_t cap,
                                   const fault_lib_record_t *record,
                                   size_t *out_len)
{
    postcard_writer_t w;
    size_t            payload_len;
    uint32_t          len_le;

    if ((buf == NULL) || (out_len == NULL)) {
        return WIRE_FAULT_RECORD_INVAL;
    }
    if (!validate_record(record)) {
        return WIRE_FAULT_RECORD_INVAL;
    }
    if (cap < WIRE_FRAME_PREFIX_LEN) {
        return WIRE_FAULT_RECORD_OVERFLOW;
    }

    /* Serialise the postcard payload after the 4-byte prefix slot. */
    postcard_writer_init(&w, &buf[WIRE_FRAME_PREFIX_LEN], cap - WIRE_FRAME_PREFIX_LEN);

    /* 1. component (postcard String) */
    (void)postcard_write_string(&w, record->component, record->component_len);
    /* 2. id (postcard varint u32) */
    (void)postcard_write_varint_u32(&w, record->id);
    /* 3. severity (postcard u8, explicit width) */
    (void)postcard_write_u8(&w, (uint8_t)record->severity);
    /* 4. timestamp_ms (postcard varint u64) */
    (void)postcard_write_varint_u64(&w, record->timestamp_ms);
    /* 5. meta_json (postcard Option<String>) */
    if ((record->meta.json != NULL) && (record->meta.len > 0U)) {
        (void)postcard_write_option_string(&w, record->meta.json, record->meta.len);
    } else {
        (void)postcard_write_option_string(&w, NULL, 0U);
    }

    if (postcard_writer_failed(&w)) {
        return WIRE_FAULT_RECORD_OVERFLOW;
    }
    payload_len = postcard_writer_len(&w);
    if (payload_len > FAULT_LIB_MAX_FRAME_LEN) {
        return WIRE_FAULT_RECORD_OVERFLOW;
    }

    /* Write the little-endian u32 length prefix. */
    len_le = (uint32_t)payload_len;
    buf[0] = (uint8_t)(len_le & 0xFFU);
    buf[1] = (uint8_t)((len_le >> 8) & 0xFFU);
    buf[2] = (uint8_t)((len_le >> 16) & 0xFFU);
    buf[3] = (uint8_t)((len_le >> 24) & 0xFFU);

    *out_len = WIRE_FRAME_PREFIX_LEN + payload_len;
    return WIRE_FAULT_RECORD_OK;
}

/* --- test-only decoder ------------------------------------------------- */

typedef struct reader_s {
    const uint8_t *buf;
    size_t         len;
    size_t         pos;
    bool           failed;
} reader_t;

static int reader_read_byte(reader_t *r, uint8_t *out)
{
    if (r->failed || (r->pos >= r->len)) {
        r->failed = true;
        return WIRE_FAULT_RECORD_TRUNC;
    }
    *out = r->buf[r->pos];
    r->pos++;
    return WIRE_FAULT_RECORD_OK;
}

static int reader_read_varint_u64(reader_t *r, uint64_t *out)
{
    uint64_t acc = 0U;
    uint32_t shift = 0U;
    uint8_t  byte;
    size_t   steps = 0U;
    while (steps < 10U) {
        int rc = reader_read_byte(r, &byte);
        if (rc != WIRE_FAULT_RECORD_OK) {
            return rc;
        }
        acc |= ((uint64_t)(byte & 0x7FU)) << shift;
        if ((byte & 0x80U) == 0U) {
            *out = acc;
            return WIRE_FAULT_RECORD_OK;
        }
        shift += 7U;
        steps++;
    }
    r->failed = true;
    return WIRE_FAULT_RECORD_INVAL;
}

static int reader_read_varint_u32(reader_t *r, uint32_t *out)
{
    uint64_t tmp = 0U;
    int rc = reader_read_varint_u64(r, &tmp);
    if (rc != WIRE_FAULT_RECORD_OK) {
        return rc;
    }
    if (tmp > (uint64_t)0xFFFFFFFFU) {
        return WIRE_FAULT_RECORD_INVAL;
    }
    *out = (uint32_t)tmp;
    return WIRE_FAULT_RECORD_OK;
}

static int reader_read_bytes(reader_t *r, size_t n, char *dst, size_t dst_cap)
{
    if (r->failed || ((r->len - r->pos) < n)) {
        r->failed = true;
        return WIRE_FAULT_RECORD_TRUNC;
    }
    if (n > dst_cap) {
        return WIRE_FAULT_RECORD_OVERFLOW;
    }
    if (n > 0U) {
        (void)memcpy(dst, &r->buf[r->pos], n);
    }
    r->pos += n;
    return WIRE_FAULT_RECORD_OK;
}

int wire_fault_record_decode_frame(const uint8_t *buf, size_t len,
                                   wire_fault_record_decoded_t *out)
{
    reader_t r;
    uint32_t comp_len;
    uint32_t frame_len;
    uint64_t ts;
    uint8_t  sev;
    uint8_t  opt_tag;
    int      rc;

    if ((buf == NULL) || (out == NULL) || (len < WIRE_FRAME_PREFIX_LEN)) {
        return WIRE_FAULT_RECORD_INVAL;
    }
    (void)memset(out, 0, sizeof(*out));

    /* Parse and verify the length prefix. */
    frame_len = (uint32_t)buf[0]
              | ((uint32_t)buf[1] << 8)
              | ((uint32_t)buf[2] << 16)
              | ((uint32_t)buf[3] << 24);
    if ((size_t)frame_len != (len - WIRE_FRAME_PREFIX_LEN)) {
        return WIRE_FAULT_RECORD_INVAL;
    }

    r.buf = &buf[WIRE_FRAME_PREFIX_LEN];
    r.len = frame_len;
    r.pos = 0U;
    r.failed = false;

    /* 1. component string */
    rc = reader_read_varint_u32(&r, &comp_len);
    if (rc != WIRE_FAULT_RECORD_OK) {
        return rc;
    }
    if (comp_len > FAULT_LIB_MAX_COMPONENT_LEN) {
        return WIRE_FAULT_RECORD_INVAL;
    }
    rc = reader_read_bytes(&r, comp_len, out->component, sizeof(out->component));
    if (rc != WIRE_FAULT_RECORD_OK) {
        return rc;
    }
    out->component[comp_len] = '\0';
    out->component_len = comp_len;

    /* 2. id */
    rc = reader_read_varint_u32(&r, &out->id);
    if (rc != WIRE_FAULT_RECORD_OK) {
        return rc;
    }
    /* 3. severity */
    rc = reader_read_byte(&r, &sev);
    if (rc != WIRE_FAULT_RECORD_OK) {
        return rc;
    }
    out->severity_raw = sev;
    /* 4. timestamp_ms */
    rc = reader_read_varint_u64(&r, &ts);
    if (rc != WIRE_FAULT_RECORD_OK) {
        return rc;
    }
    out->timestamp_ms = ts;
    /* 5. meta_json Option<String> */
    rc = reader_read_byte(&r, &opt_tag);
    if (rc != WIRE_FAULT_RECORD_OK) {
        return rc;
    }
    if (opt_tag == 0U) {
        out->meta_present = false;
        out->meta_json_len = 0U;
    } else if (opt_tag == 1U) {
        uint32_t meta_len;
        rc = reader_read_varint_u32(&r, &meta_len);
        if (rc != WIRE_FAULT_RECORD_OK) {
            return rc;
        }
        if (meta_len > FAULT_LIB_MAX_META_JSON_LEN) {
            return WIRE_FAULT_RECORD_INVAL;
        }
        rc = reader_read_bytes(&r, meta_len, out->meta_json, sizeof(out->meta_json));
        if (rc != WIRE_FAULT_RECORD_OK) {
            return rc;
        }
        out->meta_json[meta_len] = '\0';
        out->meta_json_len = meta_len;
        out->meta_present = true;
    } else {
        return WIRE_FAULT_RECORD_INVAL;
    }

    /* Must have consumed every byte. */
    if (r.pos != r.len) {
        return WIRE_FAULT_RECORD_INVAL;
    }
    return WIRE_FAULT_RECORD_OK;
}
