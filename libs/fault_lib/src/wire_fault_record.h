/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (Taktflow fork)
 *
 * wire_fault_record.h — internal encoder for a framed postcard
 * WireFaultRecord. Not part of the public API. Exposed to the test
 * suite and fault_lib.c only.
 *
 * The encoder serialises a fault_lib_record_t into a caller-owned
 * buffer using the postcard_c primitives, then prepends a 4-byte
 * little-endian u32 length prefix. The total number of bytes written
 * is returned via *out_len.
 *
 * A tiny decoder is also exposed so the C unit tests can round-trip
 * their own output without linking against the Rust side. The
 * decoder is NOT used by the production library.
 */

#ifndef TAKTFLOW_LIBS_FAULT_LIB_WIRE_FAULT_RECORD_H
#define TAKTFLOW_LIBS_FAULT_LIB_WIRE_FAULT_RECORD_H

#include <stddef.h>
#include <stdint.h>

#include "fault_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIRE_FAULT_RECORD_OK        0
#define WIRE_FAULT_RECORD_INVAL    (-1)
#define WIRE_FAULT_RECORD_OVERFLOW (-2)
#define WIRE_FAULT_RECORD_TRUNC    (-3)

/*
 * Encode a framed WireFaultRecord into `buf`.
 *
 *   buf     - output buffer (caller-owned)
 *   cap     - capacity of buf
 *   record  - record to serialise; must be non-NULL
 *   out_len - on success, total bytes written (including 4-byte prefix)
 *
 * Returns WIRE_FAULT_RECORD_OK or a negative error code.
 */
int wire_fault_record_encode_frame(uint8_t *buf, size_t cap,
                                   const fault_lib_record_t *record,
                                   size_t *out_len);

/*
 * Decode a framed WireFaultRecord previously encoded by
 * wire_fault_record_encode_frame. Test-only. String fields are
 * copied into caller-owned buffers of fixed capacity.
 */
typedef struct wire_fault_record_decoded_s {
    char     component[FAULT_LIB_MAX_COMPONENT_LEN + 1U];
    size_t   component_len;
    uint32_t id;
    uint8_t  severity_raw;
    uint64_t timestamp_ms;
    bool     meta_present;
    char     meta_json[FAULT_LIB_MAX_META_JSON_LEN + 1U];
    size_t   meta_json_len;
} wire_fault_record_decoded_t;

int wire_fault_record_decode_frame(const uint8_t *buf, size_t len,
                                   wire_fault_record_decoded_t *out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TAKTFLOW_LIBS_FAULT_LIB_WIRE_FAULT_RECORD_H */
