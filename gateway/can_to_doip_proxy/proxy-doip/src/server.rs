// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// DoIP TCP server. Tests-first skeleton; green commit adds the impl.

use std::net::SocketAddr;
use std::sync::Arc;

use async_trait::async_trait;

/// Abstract handler the server calls for each decoded DoIP frame.
///
/// Keeping this a trait decouples the TCP plumbing from the CAN-side
/// translator so unit tests can drive the server with an in-memory mock
/// handler, no CAN bus required.
#[async_trait]
pub trait DoipHandler: Send + Sync + 'static {
    /// Handle a routing activation request. Return the ECU logical
    /// address to claim on success, or None to reject.
    async fn on_routing_activation(&self, source_address: u16) -> Option<u16>;

    /// Handle a diagnostic message. Return the UDS response bytes (just
    /// the UDS payload, no DoIP wrapping — the server wraps).
    async fn on_diagnostic_message(
        &self,
        source_address: u16,
        target_address: u16,
        uds: &[u8],
    ) -> Option<Vec<u8>>;
}

pub type SharedHandler = Arc<dyn DoipHandler>;

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::io::{AsyncReadExt, AsyncWriteExt};
    use tokio::net::TcpStream;

    struct MockHandler;

    #[async_trait]
    impl DoipHandler for MockHandler {
        async fn on_routing_activation(&self, _src: u16) -> Option<u16> {
            Some(0x0001)
        }
        async fn on_diagnostic_message(
            &self,
            _src: u16,
            _tgt: u16,
            uds: &[u8],
        ) -> Option<Vec<u8>> {
            // Echo the UDS bytes back as a positive response.
            let mut out = vec![0x40 | uds[0]];
            out.extend_from_slice(&uds[1..]);
            Some(out)
        }
    }

    #[tokio::test]
    async fn server_accepts_routing_activation_and_diag_roundtrip() {
        let handler: SharedHandler = Arc::new(MockHandler);
        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr: SocketAddr = listener.local_addr().unwrap();

        tokio::spawn(async move {
            super::serve_one(listener, handler).await.unwrap();
        });

        let mut client = TcpStream::connect(addr).await.unwrap();

        // Send routing activation request: header + 7-byte payload.
        let req = crate::frame::encode_frame(
            crate::frame::PayloadType::RoutingActivationRequest,
            &[0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
        )
        .unwrap();
        client.write_all(&req).await.unwrap();

        // Read response header (8) + routing activation response body (9).
        let mut rsp = [0u8; 17];
        client.read_exact(&mut rsp).await.unwrap();
        let h = crate::frame::decode_header(&rsp).unwrap();
        assert_eq!(h.payload_type, crate::frame::PayloadType::RoutingActivationResponse);
        assert_eq!(h.payload_length, 9);
        // Activation code at offset 8 + 4 = 12.
        assert_eq!(rsp[12], crate::message_types::ACTIVATION_OK);

        // Send diagnostic message: 0x22 0xF1 0x90 (Read VIN).
        let diag = crate::frame::encode_frame(
            crate::frame::PayloadType::DiagnosticMessage,
            &[0x0E, 0x00, 0x00, 0x01, 0x22, 0xF1, 0x90],
        )
        .unwrap();
        client.write_all(&diag).await.unwrap();

        // Response: diagnostic message payload = src(0x0001) + tgt(0x0E00) +
        // UDS bytes [0x62, 0xF1, 0x90]. Total payload 7, frame 15.
        let mut rsp = [0u8; 15];
        client.read_exact(&mut rsp).await.unwrap();
        let h = crate::frame::decode_header(&rsp).unwrap();
        assert_eq!(h.payload_type, crate::frame::PayloadType::DiagnosticMessage);
        assert_eq!(h.payload_length, 7);
        assert_eq!(&rsp[8..12], &[0x00, 0x01, 0x0E, 0x00]);
        assert_eq!(&rsp[12..15], &[0x62, 0xF1, 0x90]);
    }
}
