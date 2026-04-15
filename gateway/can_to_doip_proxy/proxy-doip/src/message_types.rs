// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// DoIP message-type parsers / builders. Tests-first skeleton; green
// commit adds the implementation.

pub const ACTIVATION_OK: u8 = 0x10;

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
