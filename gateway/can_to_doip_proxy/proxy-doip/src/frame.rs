// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// Tests-first skeleton — implementation added in the green commit that
// immediately follows.

#[cfg(test)]
mod tests {
    // Byte literals lifted from the C-side encoder in DoIp_Posix.c. These
    // are the fixed points that the Rust encoder must exactly reproduce for
    // the CDA to be unable to tell the proxy from a POSIX vECU.
    const ROUTING_ACTIVATION_REQ_BYTES: [u8; 15] = [
        0x02, 0xFD, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    ];

    const DIAG_REQ_READ_VIN: [u8; 15] = [
        0x02, 0xFD, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07, 0x0E, 0x00, 0x00, 0x01, 0x22, 0xF1, 0x90,
    ];

    #[test]
    fn decode_routing_activation_request() {
        let h = super::decode_header(&ROUTING_ACTIVATION_REQ_BYTES).expect("parse ok");
        assert_eq!(h.payload_type, super::PayloadType::RoutingActivationRequest);
        assert_eq!(h.payload_length, 7);
    }

    #[test]
    fn decode_diagnostic_message() {
        let h = super::decode_header(&DIAG_REQ_READ_VIN).expect("parse ok");
        assert_eq!(h.payload_type, super::PayloadType::DiagnosticMessage);
        assert_eq!(h.payload_length, 7);
    }

    #[test]
    fn encode_frame_matches_diag_byte_literal() {
        let frame = super::encode_frame(
            super::PayloadType::DiagnosticMessage,
            &[0x0E, 0x00, 0x00, 0x01, 0x22, 0xF1, 0x90],
        )
        .unwrap();
        assert_eq!(frame.as_slice(), &DIAG_REQ_READ_VIN[..]);
    }
}
