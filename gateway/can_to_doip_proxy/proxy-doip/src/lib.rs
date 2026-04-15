// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// proxy-doip: DoIP TCP/UDP framing and server. Wire format is
// bit-for-bit compatible with firmware/platform/posix/src/DoIp_Posix.c.

#![forbid(unsafe_code)]

pub mod frame;
pub mod message_types;
pub mod server;
