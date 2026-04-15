// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// DoIP payload parsers / builders for the message types the proxy cares
// about: routing activation request/response, diagnostic message.
// Byte layout pinned by the tests below to the C-side DoIp_Posix.c
// reference so the CDA cannot tell the proxy from a POSIX vECU.

/// Routing activation OK status code, matches `DOIP_POSIX_ACTIVATION_OK`.
pub const ACTIVATION_OK: u8 = 0x10;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RoutingActivationRequest {
    pub source_address: u16,
    pub activation_type: u8,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DiagnosticMessage<'a> {
    pub source_address: u16,
    pub target_address: u16,
    pub uds: &'a [u8],
}

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum MessageError {
    #[error("short message: got {got}, need at least {need}")]
    Short { got: usize, need: usize },
}

fn be16(b0: u8, b1: u8) -> u16 {
    (u16::from(b0) << 8) | u16::from(b1)
}

/// Parse a routing activation request payload (7 bytes after the 8-byte
/// DoIP header): source(2) + activation_type(1) + reserved(4).
///
/// # Errors
/// Returns [`MessageError::Short`] if the buffer is under 7 bytes.
pub fn parse_routing_activation_request(
    payload: &[u8],
) -> Result<RoutingActivationRequest, MessageError> {
    if payload.len() < 7 {
        return Err(MessageError::Short { got: payload.len(), need: 7 });
    }
    let b0 = *payload.first().ok_or(MessageError::Short { got: 0, need: 7 })?;
    let b1 = *payload.get(1).ok_or(MessageError::Short { got: 1, need: 7 })?;
    let t = *payload.get(2).ok_or(MessageError::Short { got: 2, need: 7 })?;
    Ok(RoutingActivationRequest {
        source_address: be16(b0, b1),
        activation_type: t,
    })
}

/// Build a routing activation response payload: tester(2) + ecu(2) +
/// code(1) + reserved(4).
#[must_use]
pub fn build_routing_activation_response(
    tester_address: u16,
    ecu_address: u16,
    code: u8,
) -> [u8; 9] {
    let mut out = [0u8; 9];
    out[0] = ((tester_address >> 8) & 0xFF) as u8;
    out[1] = (tester_address & 0xFF) as u8;
    out[2] = ((ecu_address >> 8) & 0xFF) as u8;
    out[3] = (ecu_address & 0xFF) as u8;
    out[4] = code;
    out
}

/// Parse a diagnostic-message payload: source(2) + target(2) + UDS(N).
///
/// # Errors
/// Returns [`MessageError::Short`] if less than 5 bytes.
pub fn parse_diagnostic_message(payload: &[u8]) -> Result<DiagnosticMessage<'_>, MessageError> {
    if payload.len() < 5 {
        return Err(MessageError::Short { got: payload.len(), need: 5 });
    }
    let b0 = *payload.first().ok_or(MessageError::Short { got: 0, need: 5 })?;
    let b1 = *payload.get(1).ok_or(MessageError::Short { got: 1, need: 5 })?;
    let b2 = *payload.get(2).ok_or(MessageError::Short { got: 2, need: 5 })?;
    let b3 = *payload.get(3).ok_or(MessageError::Short { got: 3, need: 5 })?;
    let (_, rest) = payload.split_at(4);
    Ok(DiagnosticMessage {
        source_address: be16(b0, b1),
        target_address: be16(b2, b3),
        uds: rest,
    })
}

/// Build a diagnostic-message payload.
#[must_use]
pub fn build_diagnostic_message(
    source_address: u16,
    target_address: u16,
    uds: &[u8],
) -> Vec<u8> {
    let mut out = Vec::with_capacity(4_usize.saturating_add(uds.len()));
    out.push(((source_address >> 8) & 0xFF) as u8);
    out.push((source_address & 0xFF) as u8);
    out.push(((target_address >> 8) & 0xFF) as u8);
    out.push((target_address & 0xFF) as u8);
    out.extend_from_slice(uds);
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    const ROUTING_ACTIVATION_REQ: [u8; 7] = [0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00];
    const ROUTING_ACTIVATION_RSP: [u8; 9] =
        [0x0E, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00];
    const DIAG_MSG_PAYLOAD: [u8; 7] = [0x0E, 0x00, 0x00, 0x01, 0x22, 0xF1, 0x90];

    #[test]
    fn parse_routing_activation_request_payload() {
        let msg = parse_routing_activation_request(&ROUTING_ACTIVATION_REQ).unwrap();
        assert_eq!(msg.source_address, 0x0E00);
        assert_eq!(msg.activation_type, 0x00);
    }

    #[test]
    fn build_routing_activation_response_matches_byte_literal() {
        let bytes = build_routing_activation_response(0x0E00, 0x0001, ACTIVATION_OK);
        assert_eq!(bytes, ROUTING_ACTIVATION_RSP);
    }

    #[test]
    fn parse_diagnostic_message_payload() {
        let msg = parse_diagnostic_message(&DIAG_MSG_PAYLOAD).unwrap();
        assert_eq!(msg.source_address, 0x0E00);
        assert_eq!(msg.target_address, 0x0001);
        assert_eq!(msg.uds, &[0x22, 0xF1, 0x90]);
    }

    #[test]
    fn build_diagnostic_message_roundtrip() {
        let built = build_diagnostic_message(0x0001, 0x0E00, &[0x62, 0xF1, 0x90]);
        assert_eq!(&built[0..2], &[0x00, 0x01]);
        assert_eq!(&built[2..4], &[0x0E, 0x00]);
        assert_eq!(&built[4..], &[0x62, 0xF1, 0x90]);
    }

    #[test]
    fn short_routing_request_rejected() {
        assert!(parse_routing_activation_request(&[0x0E, 0x00]).is_err());
    }

    #[test]
    fn short_diagnostic_message_rejected() {
        assert!(parse_diagnostic_message(&[0x0E, 0x00, 0x00, 0x01]).is_err());
    }
}
