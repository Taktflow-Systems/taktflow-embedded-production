// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// Stub; filled in under T4.

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct EcuRoute {
    pub name: String,
    pub doip_logical_address: u16,
    pub can_request_id: u32,
    pub can_response_id: u32,
}

#[derive(Debug, Default)]
pub struct RoutingTable;

#[derive(Debug, thiserror::Error)]
pub enum RoutingTableError {
    #[error("placeholder")]
    Placeholder,
}
