// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// DoIP TCP server. One-client-at-a-time is sufficient for MVP (CDA opens a
// single long-lived connection per ECU target); serve_many is left as a
// follow-up noted in the handoff.
//
// Phase 5 Line B D4: this module still calls the legacy `frame::*` and
// `message_types::*` APIs on purpose — the PR #9 ISO-TP FlowControl
// integration + DoipHandler trait live here, and the migration to
// `codec::DoipCodec` via Framed<TcpStream, DoipCodec> is tracked as a
// follow-up so it can get its own focused review. Until then the
// deprecation warnings are locally allow-listed; byte-compat with the
// new codec is proven by tests/doip_codec_byte_compat.rs.
#![allow(deprecated)]

use std::net::SocketAddr;
use std::sync::Arc;

use async_trait::async_trait;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};

use crate::frame::{FrameError, HEADER_LEN, Header, PayloadType, decode_header, encode_frame};
use crate::message_types::{
    ACTIVATION_OK, build_diagnostic_message, build_routing_activation_response,
    parse_diagnostic_message, parse_routing_activation_request,
};

/// Abstract handler the server calls for each decoded DoIP frame.
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

#[derive(Debug, thiserror::Error)]
pub enum ServerError {
    #[error("io: {0}")]
    Io(#[from] std::io::Error),
    #[error("frame: {0}")]
    Frame(#[from] FrameError),
    #[error("client closed")]
    ClientClosed,
}

/// Accept exactly one client connection, handle frames until the client
/// closes, then return. Used by tests; production calls [`serve`] which
/// loops.
///
/// # Errors
/// Propagates any I/O or frame-decode error.
pub async fn serve_one(listener: TcpListener, handler: SharedHandler) -> Result<(), ServerError> {
    let (stream, _peer) = listener.accept().await?;
    handle_client(stream, handler).await
}

/// Serve forever, accepting clients sequentially. Graceful shutdown is
/// driven by cancelling the enclosing tokio task.
///
/// # Errors
/// Propagates any I/O error from accept().
pub async fn serve(listener: TcpListener, handler: SharedHandler) -> Result<(), ServerError> {
    loop {
        let (stream, peer) = listener.accept().await?;
        tracing::info!(%peer, "doip client connected");
        let h = Arc::clone(&handler);
        if let Err(err) = handle_client(stream, h).await {
            tracing::warn!(?err, %peer, "doip client session ended with error");
        } else {
            tracing::info!(%peer, "doip client session ended");
        }
    }
}

async fn handle_client(mut stream: TcpStream, handler: SharedHandler) -> Result<(), ServerError> {
    let mut header_buf = [0u8; HEADER_LEN];
    loop {
        match stream.read_exact(&mut header_buf).await {
            Ok(_) => {}
            Err(err) if err.kind() == std::io::ErrorKind::UnexpectedEof => {
                return Ok(());
            }
            Err(err) => return Err(err.into()),
        }
        let header = decode_header(&header_buf)?;
        let mut payload = vec![0u8; header.payload_length as usize];
        if !payload.is_empty() {
            stream.read_exact(&mut payload).await?;
        }
        dispatch(&mut stream, header, &payload, Arc::clone(&handler)).await?;
    }
}

async fn dispatch(
    stream: &mut TcpStream,
    header: Header,
    payload: &[u8],
    handler: SharedHandler,
) -> Result<(), ServerError> {
    match header.payload_type {
        PayloadType::RoutingActivationRequest => {
            let req = parse_routing_activation_request(payload)
                .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e.to_string()))?;
            let ecu = handler.on_routing_activation(req.source_address).await;
            let body = match ecu {
                Some(ecu_addr) => {
                    build_routing_activation_response(req.source_address, ecu_addr, ACTIVATION_OK)
                }
                None => build_routing_activation_response(req.source_address, 0, 0x06),
            };
            let frame = encode_frame(PayloadType::RoutingActivationResponse, &body)?;
            stream.write_all(&frame).await?;
        }
        PayloadType::DiagnosticMessage => {
            let req = parse_diagnostic_message(payload)
                .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e.to_string()))?;
            if let Some(uds_rsp) = handler
                .on_diagnostic_message(req.source_address, req.target_address, req.uds)
                .await
            {
                // Send a DiagnosticAck per ISO 13400-2, then the response
                // diagnostic message. Many CDA clients accept the
                // response without the ack, but the spec requires it.
                let ack_body = {
                    let mut buf = Vec::with_capacity(5usize.saturating_add(req.uds.len()));
                    buf.extend_from_slice(&(req.source_address.to_be_bytes()));
                    buf.extend_from_slice(&(req.target_address.to_be_bytes()));
                    buf.push(0x00);
                    buf
                };
                let ack = encode_frame(PayloadType::DiagnosticAck, &ack_body)?;
                stream.write_all(&ack).await?;

                let rsp_payload =
                    build_diagnostic_message(req.target_address, req.source_address, &uds_rsp);
                let frame = encode_frame(PayloadType::DiagnosticMessage, &rsp_payload)?;
                stream.write_all(&frame).await?;
            }
        }
        PayloadType::AliveCheckRequest => {
            let frame = encode_frame(PayloadType::AliveCheckResponse, &[])?;
            stream.write_all(&frame).await?;
        }
        _ => {
            // Unsupported types are silently dropped for MVP. A future
            // commit can extend this to send a proper NACK.
            tracing::warn!(?header.payload_type, "unsupported DoIP payload type");
        }
    }
    Ok(())
}

#[allow(dead_code)]
fn listener_addr(listener: &TcpListener) -> SocketAddr {
    listener.local_addr().expect("listener has local addr")
}

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
        async fn on_diagnostic_message(&self, _src: u16, _tgt: u16, uds: &[u8]) -> Option<Vec<u8>> {
            let mut out = vec![0x40 | uds[0]];
            out.extend_from_slice(&uds[1..]);
            Some(out)
        }
    }

    #[tokio::test]
    async fn server_accepts_routing_activation_and_diag_roundtrip() {
        let handler: SharedHandler = Arc::new(MockHandler);
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();

        tokio::spawn(async move {
            let _ = serve_one(listener, handler).await;
        });

        let mut client = TcpStream::connect(addr).await.unwrap();

        // Routing activation request.
        let req = encode_frame(
            PayloadType::RoutingActivationRequest,
            &[0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
        )
        .unwrap();
        client.write_all(&req).await.unwrap();

        // Read response: header(8) + 9-byte routing activation response.
        let mut rsp = [0u8; 17];
        client.read_exact(&mut rsp).await.unwrap();
        let h = decode_header(&rsp).unwrap();
        assert_eq!(h.payload_type, PayloadType::RoutingActivationResponse);
        assert_eq!(h.payload_length, 9);
        assert_eq!(rsp[12], ACTIVATION_OK);

        // Diagnostic message: Read VIN.
        let diag = encode_frame(
            PayloadType::DiagnosticMessage,
            &[0x0E, 0x00, 0x00, 0x01, 0x22, 0xF1, 0x90],
        )
        .unwrap();
        client.write_all(&diag).await.unwrap();

        // Expect ack first.
        let mut ack = [0u8; 8 + 5];
        client.read_exact(&mut ack).await.unwrap();
        let ack_h = decode_header(&ack).unwrap();
        assert_eq!(ack_h.payload_type, PayloadType::DiagnosticAck);

        // Then the diag response: src(0x0001) + tgt(0x0E00) + [0x62, 0xF1, 0x90].
        let mut rsp = [0u8; 15];
        client.read_exact(&mut rsp).await.unwrap();
        let h = decode_header(&rsp).unwrap();
        assert_eq!(h.payload_type, PayloadType::DiagnosticMessage);
        assert_eq!(h.payload_length, 7);
        assert_eq!(&rsp[8..12], &[0x00, 0x01, 0x0E, 0x00]);
        assert_eq!(&rsp[12..15], &[0x62, 0xF1, 0x90]);
    }
}
