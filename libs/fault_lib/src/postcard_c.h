/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (Taktflow fork)
 *
 * postcard_c.h — minimal clean-room postcard encoder, subset required
 * by WireFaultRecord (see ADR-0017, postcard 1.1.3).
 *
 * This is NOT a full postcard implementation. It covers the subset
 * necessary to emit the WireFaultRecord fields:
 *
 *   - varint(u32)  — LEB128 unsigned, up to 5 bytes
 *   - varint(u64)  — LEB128 unsigned, up to 10 bytes
 *   - u8           — single raw byte
 *   - bool         — single raw byte, 0x00 false, 0x01 true
 *   - string       — varint(len) + raw utf-8 bytes
 *   - option(T)    — 0x00 for None, 0x01 for Some followed by T
 *
 * Postcard 1.x encodes every integer as an LEB128 varint, little-endian
 * 7-bit groups, the MSB of every byte set except the final byte. We
 * do NOT handle signed zig-zag because WireFaultRecord has no signed
 * integer fields.
 *
 * Every function writes into a caller-supplied buffer and returns the
 * number of bytes written, or a negative value on overflow. Buffers
 * are stepped via a postcard_writer_t cursor so callers can chain
 * calls without tracking offsets manually.
 *
 * The covered subset is documented in postcard_c.md alongside this
 * header. If a future WireFaultRecord field needs a primitive that
 * isn't covered, extend this file AND update postcard_c.md in the
 * same commit.
 */

#ifndef TAKTFLOW_LIBS_FAULT_LIB_POSTCARD_C_H
#define TAKTFLOW_LIBS_FAULT_LIB_POSTCARD_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * postcard_writer_t — append-only cursor into a caller-owned buffer.
 *
 *   buf      — start of the buffer
 *   cap      — total capacity in bytes
 *   len      — bytes written so far; always <= cap unless overflow
 *   overflow — sticky flag; once set, every subsequent write is a
 *              no-op and the writer stays in the failed state.
 */
typedef struct postcard_writer_s {
    uint8_t *buf;
    size_t   cap;
    size_t   len;
    bool     overflow;
} postcard_writer_t;

/* Return codes. */
#define POSTCARD_OK        0
#define POSTCARD_OVERFLOW (-1)
#define POSTCARD_INVAL    (-2)

/*
 * Initialise the writer around `buf` with capacity `cap`. Neither
 * `buf` nor `cap` may be zero if data is to be written; a zero-sized
 * writer is legal but every write will overflow.
 */
void postcard_writer_init(postcard_writer_t *w, uint8_t *buf, size_t cap);

/*
 * Total bytes written so far. Undefined if the writer overflowed
 * (the overflow flag must be checked separately).
 */
size_t postcard_writer_len(const postcard_writer_t *w);

/*
 * True iff any prior write overflowed the buffer.
 */
bool postcard_writer_failed(const postcard_writer_t *w);

/*
 * Append a single raw u8. Used by severity and option discriminants.
 */
int postcard_write_u8(postcard_writer_t *w, uint8_t v);

/*
 * Append a single boolean (0x00 or 0x01).
 */
int postcard_write_bool(postcard_writer_t *w, bool v);

/*
 * Append an LEB128 unsigned varint. postcard 1.x uses this encoding
 * for every unsigned integer type regardless of width.
 */
int postcard_write_varint_u32(postcard_writer_t *w, uint32_t v);
int postcard_write_varint_u64(postcard_writer_t *w, uint64_t v);

/*
 * Append a postcard string: varint(len) followed by `len` raw bytes.
 * `bytes` may be NULL only if `len` is 0.
 */
int postcard_write_string(postcard_writer_t *w, const char *bytes, size_t len);

/*
 * Append a postcard Option<String> discriminant + body. If `bytes`
 * is NULL, writes a None (0x00). Otherwise writes Some (0x01) and
 * the string body.
 */
int postcard_write_option_string(postcard_writer_t *w,
                                 const char *bytes, size_t len);

/*
 * Maximum bytes an LEB128 varint can occupy for each width.
 */
#define POSTCARD_MAX_VARINT_U32 ((size_t)5U)
#define POSTCARD_MAX_VARINT_U64 ((size_t)10U)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TAKTFLOW_LIBS_FAULT_LIB_POSTCARD_C_H */
