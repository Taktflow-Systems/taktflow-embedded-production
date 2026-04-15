// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// proxy-core: routing table, DoIP <-> CAN ISO-TP translation, discovery.
// Pure logic, no I/O. See ADR-0004 and ADR-0010.

#![forbid(unsafe_code)]

pub mod discovery;
pub mod routing;
pub mod translator;

pub use discovery::{
    should_respond_to_broadcast, should_use_static, vehicle_announcement_payloads, DiscoveryMode,
};
pub use routing::{EcuRoute, RoutingTable, RoutingTableError};
pub use translator::{translate_request, TranslateError, TranslatedRequest};
