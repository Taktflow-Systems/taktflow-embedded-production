// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// Routing table: DoIP logical address -> CAN request/response ID pair.
// Tests-first skeleton; green commit adds the implementation.

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct EcuRoute {
    pub name: String,
    pub doip_logical_address: u16,
    pub can_request_id: u32,
    pub can_response_id: u32,
}

#[derive(Debug, Default, Clone)]
pub struct RoutingTable;

#[derive(Debug, thiserror::Error)]
pub enum RoutingTableError {
    #[error("placeholder")]
    Placeholder,
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
