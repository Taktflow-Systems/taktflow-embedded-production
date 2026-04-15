/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 The Contributors to Eclipse OpenSOVD (Taktflow fork)
 *
 * fault_lib.h — C-side Fault Library shim public API.
 *
 * This header is the C-side mirror of the Rust fault-lib API documented
 * in ADR-0002. It lets embedded firmware emit fault records over the
 * Unix-socket wire protocol defined in ADR-0017 so that a Rust
 * fault-sink-unix consumer on the Pi gateway (see opensovd-core
 * crates/fault-sink-unix) can decode them byte-for-byte.
 *
 * Field order and types below MUST match the WireFaultRecord shadow
 * struct in opensovd-core/crates/fault-sink-unix/src/codec.rs. Any
 * drift breaks the interop test under tests/interop/ and fails the
 * phase gate.
 *
 * The header is self-contained: only <stdint.h>, <stddef.h>,
 * <stdbool.h>. It compiles cleanly under -std=c99 -Wall -Wextra
 * -Werror -pedantic. No hardcoded fault codes, severity values, or
 * message payloads live in this file — test data is loaded from
 * side files under libs/fault_lib/testdata/.
 */

#ifndef TAKTFLOW_LIBS_FAULT_LIB_FAULT_LIB_H
#define TAKTFLOW_LIBS_FAULT_LIB_FAULT_LIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Wire-level limits shared with the Rust side. FAULT_LIB_MAX_FRAME_LEN
 * mirrors MAX_FRAME_LEN in fault-sink-unix/src/codec.rs. Changing it
 * here without changing it there breaks interop.
 */
#define FAULT_LIB_MAX_FRAME_LEN          ((size_t)(64U * 1024U))
#define FAULT_LIB_MAX_COMPONENT_LEN      ((size_t)64U)
#define FAULT_LIB_MAX_META_JSON_LEN      ((size_t)(16U * 1024U))

/*
 * Severity codes — numeric values frozen by codec.rs:
 *   Fatal   = 1
 *   Error   = 2
 *   Warning = 3
 *   Info    = 4
 *
 * The enum names below do NOT carry numeric defaults chosen by the C
 * side: the values are assigned explicitly so the enum cannot drift
 * from the Rust match arm.
 */
typedef enum fault_lib_severity_e {
    FAULT_LIB_SEVERITY_FATAL   = 1,
    FAULT_LIB_SEVERITY_ERROR   = 2,
    FAULT_LIB_SEVERITY_WARNING = 3,
    FAULT_LIB_SEVERITY_INFO    = 4
} fault_lib_severity_t;

/*
 * fault_lib_fault_code_t — stable 32-bit fault id that maps 1:1 to
 * FaultId(u32) on the Rust side. Upstream FaultId is a tuple-struct
 * around u32; we mirror the primitive directly.
 */
typedef uint32_t fault_lib_fault_code_t;

/*
 * fault_lib_timestamp_t — milliseconds since epoch, u64.
 *
 * NOTE: the Rust WireFaultRecord field is named `timestamp_ms`. The
 * API name historically used "microseconds" in older drafts; this
 * header tracks the wire protocol, not the historical API draft.
 */
typedef uint64_t fault_lib_timestamp_t;

/*
 * fault_lib_meta_t — opaque JSON text + length. The library does NOT
 * parse or validate the JSON. It treats the bytes as an opaque
 * length-prefixed blob that the Rust side will re-parse via
 * serde_json. If `json` is NULL or `len` is 0, the record is
 * serialized as Option::None on the wire.
 */
typedef struct fault_lib_meta_s {
    const char *json;
    size_t      len;
} fault_lib_meta_t;

/*
 * fault_lib_record_t — mirrors WireFaultRecord in fault-sink-unix.
 *
 * Field order here is documentation only — the wire order is fixed
 * in libs/fault_lib/src/wire_fault_record.c. Do not reorder these
 * fields without updating the encoder and the interop test.
 *
 *   component   <-> WireFaultRecord::component (String)
 *   id          <-> WireFaultRecord::id        (u32)
 *   severity    <-> WireFaultRecord::severity  (u8)
 *   timestamp_ms<-> WireFaultRecord::timestamp_ms (u64)
 *   meta        <-> WireFaultRecord::meta_json (Option<String>)
 */
typedef struct fault_lib_record_s {
    const char            *component;
    size_t                 component_len;
    fault_lib_fault_code_t id;
    fault_lib_severity_t   severity;
    fault_lib_timestamp_t  timestamp_ms;
    fault_lib_meta_t       meta;
} fault_lib_record_t;

/* Error codes returned by the public functions. All non-zero. */
#define FAULT_LIB_OK                 0
#define FAULT_LIB_ERR_INVAL         (-1)
#define FAULT_LIB_ERR_NOT_CONNECTED (-2)
#define FAULT_LIB_ERR_SOCKET        (-3)
#define FAULT_LIB_ERR_CONNECT       (-4)
#define FAULT_LIB_ERR_ENCODE        (-5)
#define FAULT_LIB_ERR_WRITE         (-6)
#define FAULT_LIB_ERR_OVERSIZE      (-7)

/*
 * fault_lib_init — connect to the Unix socket at `socket_path`.
 *
 * On POSIX hosts this opens AF_UNIX SOCK_STREAM and connects. On
 * Windows the AF_UNIX Winsock path is used; if unavailable at build
 * time, init returns FAULT_LIB_ERR_SOCKET.
 *
 * Returns 0 on success, a negative FAULT_LIB_ERR_* code on failure.
 * Calling fault_lib_init twice without an intervening shutdown
 * returns FAULT_LIB_ERR_INVAL.
 */
int fault_lib_init(const char *socket_path);

/*
 * fault_lib_report — serialize `record` as a framed postcard
 * WireFaultRecord and send it across the socket.
 *
 * Returns 0 on success, a negative FAULT_LIB_ERR_* code on failure.
 * A NULL record, NULL component, invalid severity, or oversized
 * component / meta returns FAULT_LIB_ERR_INVAL.
 */
int fault_lib_report(const fault_lib_record_t *record);

/*
 * fault_lib_shutdown — close the socket and flush any pending state.
 * Safe to call even if fault_lib_init was never called; in that case
 * it returns 0 as a no-op.
 */
int fault_lib_shutdown(void);

/*
 * fault_lib_version — return a build-time version string used by the
 * wire-compat regression test to cross-check against the Rust side.
 * Must never return NULL.
 */
const char *fault_lib_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TAKTFLOW_LIBS_FAULT_LIB_FAULT_LIB_H */
