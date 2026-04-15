// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// Phase 5 Line B D4 — upstream CDA-pinned doip-codec / doip-definitions
// re-exports. New code (and the eventual server.rs cutover) should reach
// for these types so the proxy stays aligned with upstream CDA's wire
// format definitions. Byte-compat with the legacy hand-rolled frame.rs
// encoder is proven by tests/doip_codec_byte_compat.rs.
//
// Migration policy:
//   * NEW code -> use `proxy_doip::codec::*`
//   * OLD code (server.rs + PR #9 ISO-TP FlowControl path) -> still
//     uses `proxy_doip::frame::*` and `proxy_doip::message_types::*`
//     until the full Framed<DoipCodec> cutover lands.
//
// Deprecation of frame.rs / message_types.rs happens in lib.rs via
// `#[deprecated]` doc annotations so that downstream crates compiling
// against proxy-doip get a clean warning trail pointing here.

pub use doip_codec::DoipCodec;
pub use doip_definitions::header::{DoipHeader, PayloadType, ProtocolVersion};
pub use doip_definitions::message::DoipMessage;
pub use doip_definitions::payload::{
    ActivationType, AliveCheckRequest, AliveCheckResponse,
    DiagnosticMessage as DoipDiagnosticMessage, DoipPayload,
    RoutingActivationRequest as DoipRoutingActivationRequest,
    RoutingActivationResponse as DoipRoutingActivationResponse,
};

/// Re-export tokio-util codec traits so downstream code can build a
/// `Framed<TcpStream, DoipCodec>` without pulling tokio-util directly.
pub use tokio_util::codec::{Decoder, Encoder, Framed};
