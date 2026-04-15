// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// Routing table: DoIP logical address -> CAN request/response ID pair.
// Loaded from a TOML config (opensovd-proxy.toml) that the proxy reads
// at startup. CAN IDs come from the firmware Dcm_Cfg_<Ecu>.c headers,
// NOT from hardcoded guesses — the prompt's IMPORTANT rule. The example
// TOML in tests below mirrors the IDs found in the firmware CanIf_Cfg
// files today.

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct EcuRoute {
    pub name: String,
    pub doip_logical_address: u16,
    pub can_request_id: u32,
    pub can_response_id: u32,
}

#[derive(Debug, Clone, Deserialize)]
struct TomlRoot {
    #[serde(default)]
    ecu: Vec<EcuRoute>,
}

#[derive(Debug, Default, Clone)]
pub struct RoutingTable {
    entries: Vec<EcuRoute>,
}

#[derive(Debug, thiserror::Error)]
pub enum RoutingTableError {
    #[error("toml parse: {0}")]
    TomlParse(String),
    #[error("duplicate logical address 0x{addr:04x} in routing table")]
    DuplicateLogicalAddress { addr: u16 },
    #[error("can_id 0x{id:x} outside 11-bit standard range")]
    CanIdOutOfRange { id: u32 },
    #[error("empty routing table")]
    Empty,
}

impl RoutingTable {
    /// Parse from a TOML string.
    ///
    /// # Errors
    /// Returns [`RoutingTableError`] on parse failure, duplicate ECU,
    /// or out-of-range CAN IDs.
    pub fn from_toml_str(source: &str) -> Result<Self, RoutingTableError> {
        let root: TomlRoot =
            toml::from_str(source).map_err(|e| RoutingTableError::TomlParse(e.to_string()))?;
        let mut seen = std::collections::HashSet::new();
        for e in &root.ecu {
            if e.can_request_id > 0x7FF {
                return Err(RoutingTableError::CanIdOutOfRange { id: e.can_request_id });
            }
            if e.can_response_id > 0x7FF {
                return Err(RoutingTableError::CanIdOutOfRange { id: e.can_response_id });
            }
            if !seen.insert(e.doip_logical_address) {
                return Err(RoutingTableError::DuplicateLogicalAddress {
                    addr: e.doip_logical_address,
                });
            }
        }
        Ok(Self { entries: root.ecu })
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    #[must_use]
    pub fn by_logical_address(&self, addr: u16) -> Option<&EcuRoute> {
        self.entries.iter().find(|e| e.doip_logical_address == addr)
    }

    pub fn all(&self) -> impl Iterator<Item = &EcuRoute> + '_ {
        self.entries.iter()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const EXAMPLE_TOML: &str = r#"
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

[[ecu]]
name = "rzc"
doip_logical_address = 0x0003
can_request_id = 0x7E2
can_response_id = 0x7EA
"#;

    #[test]
    fn parses_three_ecus_from_example_toml() {
        let table = RoutingTable::from_toml_str(EXAMPLE_TOML).unwrap();
        assert_eq!(table.len(), 3);
        let cvc = table.by_logical_address(0x0001).unwrap();
        assert_eq!(cvc.name, "cvc");
        assert_eq!(cvc.can_request_id, 0x7E0);
        assert_eq!(cvc.can_response_id, 0x7E8);
    }

    #[test]
    fn lookup_by_unknown_logical_address_returns_none() {
        let table = RoutingTable::from_toml_str(EXAMPLE_TOML).unwrap();
        assert!(table.by_logical_address(0xABCD).is_none());
    }

    #[test]
    fn rejects_duplicate_logical_address() {
        let toml = r#"
[[ecu]]
name = "a"
doip_logical_address = 0x0001
can_request_id = 0x7E0
can_response_id = 0x7E8

[[ecu]]
name = "b"
doip_logical_address = 0x0001
can_request_id = 0x7E1
can_response_id = 0x7E9
"#;
        assert!(RoutingTable::from_toml_str(toml).is_err());
    }

    #[test]
    fn rejects_can_id_outside_11bit_range() {
        let toml = r#"
[[ecu]]
name = "bad"
doip_logical_address = 0x0001
can_request_id = 0x800
can_response_id = 0x7E8
"#;
        assert!(RoutingTable::from_toml_str(toml).is_err());
    }

    #[test]
    fn all_ecus_iterator_yields_entries() {
        let table = RoutingTable::from_toml_str(EXAMPLE_TOML).unwrap();
        let names: Vec<&str> = table.all().map(|e| e.name.as_str()).collect();
        assert_eq!(names, vec!["cvc", "fzc", "rzc"]);
    }
}
