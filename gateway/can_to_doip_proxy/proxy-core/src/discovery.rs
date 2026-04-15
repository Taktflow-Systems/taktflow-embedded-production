// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// Discovery mode per ADR-0010: support broadcast, static, or both. The
// proxy responds to UDP vehicle identification requests on behalf of
// every ECU in the routing table (per ADR-0004 §responsibility).
//
// This module is pure logic: given a mode and a routing table, it
// produces the list of VAM (Vehicle Announcement Message) payloads that
// the UDP responder should emit. The UDP I/O lives in proxy-main.

use serde::{Deserialize, Serialize};

use crate::routing::RoutingTable;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum DiscoveryMode {
    Broadcast,
    Static,
    Both,
}

impl Default for DiscoveryMode {
    fn default() -> Self {
        Self::Both
    }
}

/// Should the proxy answer incoming UDP vehicle-id requests?
#[must_use]
pub fn should_respond_to_broadcast(mode: DiscoveryMode) -> bool {
    matches!(mode, DiscoveryMode::Broadcast | DiscoveryMode::Both)
}

/// Should static entries be pre-loaded into the table?
#[must_use]
pub fn should_use_static(mode: DiscoveryMode) -> bool {
    matches!(mode, DiscoveryMode::Static | DiscoveryMode::Both)
}

/// Build one VAM per ECU in the table. Each VAM announces the ECU's
/// logical address as a separate "vehicle" from the CDA's point of view,
/// which is how the proxy multiplexes several physical ECUs behind one
/// Pi IP address per ADR-0004.
#[must_use]
pub fn vehicle_announcement_payloads(table: &RoutingTable) -> Vec<Vec<u8>> {
    table
        .all()
        .map(|e| {
            // VAM payload (minimal): 17-byte VIN + 2-byte logical addr +
            // 6-byte EID + 6-byte GID + 1-byte action-required. Total 32
            // bytes. We stub VIN/EID/GID to ASCII "TAKTFLOW" + name so
            // the CDA can distinguish the entries, and set action = 0
            // (no further action required).
            let mut payload = vec![0u8; 32];
            let vin = format!("TAKTFLOW{:<9}", e.name);
            let vin_bytes = vin.as_bytes();
            let take = core::cmp::min(vin_bytes.len(), 17);
            let (vin_slot, rest) = payload.split_at_mut(17);
            let (head, _) = vin_slot.split_at_mut(take);
            head.copy_from_slice(&vin_bytes[..take]);
            let (addr_slot, rest) = rest.split_at_mut(2);
            addr_slot[0] = ((e.doip_logical_address >> 8) & 0xFF) as u8;
            addr_slot[1] = (e.doip_logical_address & 0xFF) as u8;
            // EID (6) + GID (6) left as zeros; action (1) = 0.
            let _ = rest;
            payload
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    const CFG: &str = r#"
[[ecu]]
name = "cvc"
doip_logical_address = 0x0001
can_request_id = 0x7E0
can_response_id = 0x7E8

[[ecu]]
name = "fzc"
doip_logical_address = 0x0002
can_request_id = 0x7E1
can_response_id = 0x7E9
"#;

    #[test]
    fn broadcast_mode_responds() {
        assert!(should_respond_to_broadcast(DiscoveryMode::Broadcast));
        assert!(should_respond_to_broadcast(DiscoveryMode::Both));
        assert!(!should_respond_to_broadcast(DiscoveryMode::Static));
    }

    #[test]
    fn static_mode_loads() {
        assert!(should_use_static(DiscoveryMode::Static));
        assert!(should_use_static(DiscoveryMode::Both));
        assert!(!should_use_static(DiscoveryMode::Broadcast));
    }

    #[test]
    fn default_mode_is_both() {
        assert_eq!(DiscoveryMode::default(), DiscoveryMode::Both);
    }

    #[test]
    fn vams_one_per_ecu_with_logical_address_encoded() {
        let table = RoutingTable::from_toml_str(CFG).unwrap();
        let vams = vehicle_announcement_payloads(&table);
        assert_eq!(vams.len(), 2);
        // ECU 0x0001 logical addr at bytes 17..19.
        assert_eq!(vams[0][17], 0x00);
        assert_eq!(vams[0][18], 0x01);
        assert_eq!(vams[1][17], 0x00);
        assert_eq!(vams[1][18], 0x02);
        // All 32 bytes as spec.
        assert_eq!(vams[0].len(), 32);
    }
}
