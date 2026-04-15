// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// Pure translator: given a routing table and an inbound DoIP diagnostic
// message, figure out where it needs to go on the CAN side. Separated
// from the I/O layer so it is trivially unit-testable.
//
// Tests-first skeleton; green commit adds the implementation.

use crate::routing::{EcuRoute, RoutingTable};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TranslatedRequest {
    pub can_request_id: u32,
    pub can_response_id: u32,
    pub uds_payload: Vec<u8>,
}

#[derive(Debug, thiserror::Error)]
pub enum TranslateError {
    #[error("unknown target logical address 0x{addr:04x}")]
    UnknownTarget { addr: u16 },
    #[error("empty UDS payload (no SID byte)")]
    EmptyPayload,
}

/// Translate an inbound DoIP diagnostic message (source, target, UDS) to
/// a CAN-side [`TranslatedRequest`]. Read-only; does no I/O.
///
/// # Errors
/// Returns [`TranslateError::UnknownTarget`] if the routing table has no
/// entry for the target logical address, or [`TranslateError::EmptyPayload`]
/// on empty UDS.
pub fn translate_request(
    table: &RoutingTable,
    _source_address: u16,
    target_address: u16,
    uds: &[u8],
) -> Result<TranslatedRequest, TranslateError> {
    if uds.is_empty() {
        return Err(TranslateError::EmptyPayload);
    }
    let route: &EcuRoute =
        table
            .by_logical_address(target_address)
            .ok_or(TranslateError::UnknownTarget {
                addr: target_address,
            })?;
    Ok(TranslatedRequest {
        can_request_id: route.can_request_id,
        can_response_id: route.can_response_id,
        uds_payload: uds.to_vec(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    const CONFIG: &str = r#"
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

    fn table() -> RoutingTable {
        RoutingTable::from_toml_str(CONFIG).unwrap()
    }

    #[test]
    fn translates_cvc_read_vin_to_can_7e0() {
        let req = translate_request(&table(), 0x0E00, 0x0001, &[0x22, 0xF1, 0x90]).unwrap();
        assert_eq!(req.can_request_id, 0x7E0);
        assert_eq!(req.can_response_id, 0x7E8);
        assert_eq!(req.uds_payload, vec![0x22, 0xF1, 0x90]);
    }

    #[test]
    fn translates_fzc_clear_dtcs_to_can_7e1() {
        let req = translate_request(&table(), 0x0E00, 0x0002, &[0x14, 0xFF, 0xFF, 0xFF]).unwrap();
        assert_eq!(req.can_request_id, 0x7E1);
        assert_eq!(req.can_response_id, 0x7E9);
    }

    #[test]
    fn unknown_target_rejected() {
        let err = translate_request(&table(), 0x0E00, 0x00FF, &[0x22, 0xF1, 0x90]).unwrap_err();
        assert!(matches!(
            err,
            TranslateError::UnknownTarget { addr: 0x00FF }
        ));
    }

    #[test]
    fn empty_payload_rejected() {
        let err = translate_request(&table(), 0x0E00, 0x0001, &[]).unwrap_err();
        assert!(matches!(err, TranslateError::EmptyPayload));
    }
}
