// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// proxy-can: SocketCAN + ISO-TP client. Linux-only for real I/O; stubbed
// on other hosts so the workspace still checks.

#![forbid(unsafe_code)]

pub mod socket;
pub mod isotp;
