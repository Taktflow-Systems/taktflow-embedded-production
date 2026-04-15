// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// proxy-core: routing table, DoIP <-> CAN ISO-TP translation, discovery.
// Pure logic, no I/O. See ADR-0004.

#![forbid(unsafe_code)]

pub mod routing;
pub mod translator;
pub mod discovery;

pub use routing::{EcuRoute, RoutingTable, RoutingTableError};
