// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// Phase 5 Line B D4 — byte-for-byte compatibility gate between the
// hand-rolled proxy-doip encoder (frame.rs / message_types.rs, live in
// production as of Phase 4 Line B) and the pinned `theswiftfox/doip-codec`
// + `theswiftfox/doip-definitions` upstream forks that upstream CDA tracks.
//
// Gate contract: for every fixture byte string the proxy emits today, the
// doip-codec encoder must produce the same bytes. If they diverge the D4
// migration is rejected on byte-compat grounds and must be reverted per
// the Phase 5 Line B subset prompt.
//
// These tests live in an integration-test file (not #[cfg(test)] inside
// src/) so they land red without touching the existing 17 in-crate tests.

#![allow(
    clippy::unwrap_used,
    clippy::indexing_slicing,
    clippy::arithmetic_side_effects,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::uninlined_format_args,
    clippy::needless_pass_by_value,
    clippy::doc_markdown,
    deprecated // this file exists to gate deprecated vs new encoder parity
)]

use doip_codec::DoipCodec;
use doip_definitions::header::{DoipHeader, PayloadType as CodecPayloadType, ProtocolVersion};
use doip_definitions::message::DoipMessage;
use doip_definitions::payload::{
    ActivationType, AliveCheckRequest, DiagnosticMessage as CodecDiagMessage, DoipPayload,
    RoutingActivationRequest as CodecRoutingActivationRequest,
};
use tokio_util::bytes::BytesMut;
use tokio_util::codec::Encoder;

use proxy_doip::frame::{encode_frame, PayloadType};

/// Routing activation request that the hand-rolled encoder emits today,
/// lifted byte-for-byte from DoIp_Posix.c (the C reference encoder).
const ROUTING_ACTIVATION_REQ_BYTES: [u8; 15] = [
    0x02, 0xFD, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
];

/// Diagnostic message carrying UDS 22F190 (Read VIN).
const DIAG_REQ_READ_VIN: [u8; 15] = [
    0x02, 0xFD, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07, 0x0E, 0x00, 0x00, 0x01, 0x22, 0xF1, 0x90,
];

/// Bare alive-check request (header only, zero payload).
const ALIVE_CHECK_REQ_BYTES: [u8; 8] = [0x02, 0xFD, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00];

fn encode_via_doip_codec(msg: DoipMessage) -> Vec<u8> {
    let mut codec = DoipCodec {};
    let mut buf = BytesMut::new();
    codec.encode(msg, &mut buf).expect("doip-codec encode");
    buf.to_vec()
}

fn make_header(payload_type: CodecPayloadType, payload_length: u32) -> DoipHeader {
    DoipHeader {
        protocol_version: ProtocolVersion::Iso13400_2012,
        inverse_protocol_version: 0xFD,
        payload_type,
        payload_length,
    }
}

#[test]
fn byte_compat_routing_activation_request() {
    // Hand-rolled encoder — the bytes the proxy emits today.
    let old_bytes =
        encode_frame(PayloadType::RoutingActivationRequest, &[
            0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        ])
        .unwrap();
    assert_eq!(old_bytes, ROUTING_ACTIVATION_REQ_BYTES);

    // doip-codec path — what theswiftfox/doip-codec rev 0dba319 emits for
    // the same logical frame.
    let payload = CodecRoutingActivationRequest {
        source_address: [0x0E, 0x00],
        activation_type: ActivationType::Default,
        buffer: [0x00, 0x00, 0x00, 0x00],
    };
    let msg = DoipMessage {
        header: make_header(CodecPayloadType::RoutingActivationRequest, 7),
        payload: DoipPayload::RoutingActivationRequest(payload),
    };
    let new_bytes = encode_via_doip_codec(msg);

    assert_eq!(
        new_bytes, old_bytes,
        "D4 byte-compat FAILED for RoutingActivationRequest:\n  old={:02X?}\n  new={:02X?}",
        old_bytes, new_bytes
    );
}

#[test]
fn byte_compat_diagnostic_message_read_vin() {
    let old_bytes = encode_frame(
        PayloadType::DiagnosticMessage,
        &[0x0E, 0x00, 0x00, 0x01, 0x22, 0xF1, 0x90],
    )
    .unwrap();
    assert_eq!(old_bytes, DIAG_REQ_READ_VIN);

    let payload = CodecDiagMessage {
        source_address: [0x0E, 0x00],
        target_address: [0x00, 0x01],
        message: vec![0x22, 0xF1, 0x90],
    };
    let msg = DoipMessage {
        header: make_header(CodecPayloadType::DiagnosticMessage, 7),
        payload: DoipPayload::DiagnosticMessage(payload),
    };
    let new_bytes = encode_via_doip_codec(msg);

    assert_eq!(
        new_bytes, old_bytes,
        "D4 byte-compat FAILED for DiagnosticMessage (UDS 22F190):\n  old={:02X?}\n  new={:02X?}",
        old_bytes, new_bytes
    );
}

#[test]
fn byte_compat_alive_check_request_empty_payload() {
    let old_bytes = encode_frame(PayloadType::AliveCheckRequest, &[]).unwrap();
    assert_eq!(old_bytes, ALIVE_CHECK_REQ_BYTES);

    let msg = DoipMessage {
        header: make_header(CodecPayloadType::AliveCheckRequest, 0),
        payload: DoipPayload::AliveCheckRequest(AliveCheckRequest {}),
    };
    let new_bytes = encode_via_doip_codec(msg);

    assert_eq!(
        new_bytes, old_bytes,
        "D4 byte-compat FAILED for AliveCheckRequest:\n  old={:02X?}\n  new={:02X?}",
        old_bytes, new_bytes
    );
}
