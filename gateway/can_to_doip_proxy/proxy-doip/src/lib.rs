// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// proxy-doip: DoIP TCP/UDP framing and server. Wire format is
// bit-for-bit compatible with firmware/platform/posix/src/DoIp_Posix.c.

#![forbid(unsafe_code)]
#![cfg_attr(
    test,
    allow(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::arithmetic_side_effects
    )
)]

// Phase 5 Line B D4 — upstream CDA-pinned codec re-exports.
// Byte-compat with `frame` is proven by tests/doip_codec_byte_compat.rs.
pub mod codec;

/// Legacy hand-rolled DoIP framing, live in production since Phase 2
/// Line B. Byte-for-byte identical to `firmware/platform/posix/src/DoIp_Posix.c`.
/// Kept for the Phase 5+ migration window; new code should use
/// [`crate::codec`] instead.
#[deprecated(
    since = "0.1.0",
    note = "Phase 5 Line B D4: use `proxy_doip::codec` (upstream CDA doip-codec rev 0dba319)"
)]
pub mod frame;

/// Legacy hand-rolled DoIP payload parsers/builders. Kept for the
/// Phase 5+ migration window; new code should use [`crate::codec`] and
/// the upstream `doip_definitions::payload` types instead.
#[deprecated(
    since = "0.1.0",
    note = "Phase 5 Line B D4: use `proxy_doip::codec` (upstream CDA doip-definitions rev bdeab8c)"
)]
pub mod message_types;

pub mod server;
