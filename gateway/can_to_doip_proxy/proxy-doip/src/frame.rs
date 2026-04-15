// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// DoIP generic header (ISO 13400-2) encode/decode.
//
// Wire layout:
//     byte 0: protocol_version       (0x02 for ISO 13400-2 2012)
//     byte 1: protocol_version_inv   (~protocol_version)
//     bytes 2..3: payload_type       (big endian u16)
//     bytes 4..7: payload_length     (big endian u32)
//     bytes 8..:  payload            (opaque)
//
// Byte-for-byte compatible with `firmware/platform/posix/src/DoIp_Posix.c`
// so that a DoIP client cannot tell whether it is talking to a POSIX vECU
// or to this proxy.

use core::fmt;

/// ISO 13400-2 protocol version for 2012 edition (the one POSIX vECU uses).
pub const PROTOCOL_VERSION: u8 = 0x02;
/// Bitwise inverse of the protocol version, always byte 1 of the header.
pub const PROTOCOL_VERSION_INV: u8 = 0xFD;
/// Fixed size of the DoIP generic header.
pub const HEADER_LEN: usize = 8;

/// Maximum payload we ever expect to assemble. Chosen to match the C-side
/// TCP RX buffer (512) plus headroom. Any real UDS message fits well
/// under 4 kB.
pub const MAX_PAYLOAD_LEN: usize = 4096;

#[repr(u16)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PayloadType {
    VehicleIdentificationRequest = 0x0001,
    VehicleIdentificationRequestEid = 0x0002,
    VehicleIdentificationRequestVin = 0x0003,
    VehicleAnnouncement = 0x0004,
    RoutingActivationRequest = 0x0005,
    RoutingActivationResponse = 0x0006,
    AliveCheckRequest = 0x0007,
    AliveCheckResponse = 0x0008,
    DiagnosticMessage = 0x8001,
    DiagnosticAck = 0x8002,
    DiagnosticNack = 0x8003,
}

impl PayloadType {
    #[must_use]
    pub fn from_u16(raw: u16) -> Option<Self> {
        match raw {
            0x0001 => Some(Self::VehicleIdentificationRequest),
            0x0002 => Some(Self::VehicleIdentificationRequestEid),
            0x0003 => Some(Self::VehicleIdentificationRequestVin),
            0x0004 => Some(Self::VehicleAnnouncement),
            0x0005 => Some(Self::RoutingActivationRequest),
            0x0006 => Some(Self::RoutingActivationResponse),
            0x0007 => Some(Self::AliveCheckRequest),
            0x0008 => Some(Self::AliveCheckResponse),
            0x8001 => Some(Self::DiagnosticMessage),
            0x8002 => Some(Self::DiagnosticAck),
            0x8003 => Some(Self::DiagnosticNack),
            _ => None,
        }
    }

    #[must_use]
    pub const fn as_u16(self) -> u16 {
        self as u16
    }
}

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum FrameError {
    #[error("short header: got {got} bytes, need at least {need}")]
    ShortHeader { got: usize, need: usize },
    #[error("bad protocol version: got 0x{got:02x}, expected 0x{expected:02x}")]
    BadProtocolVersion { got: u8, expected: u8 },
    #[error("bad protocol inverse: got 0x{got:02x}, expected 0x{expected:02x}")]
    BadProtocolInverse { got: u8, expected: u8 },
    #[error("unknown payload type 0x{raw:04x}")]
    UnknownPayloadType { raw: u16 },
    #[error("payload too large: {got} > {max}")]
    PayloadTooLarge { got: u32, max: usize },
    #[error("output buffer too small: need {need}, have {have}")]
    OutputBufferTooSmall { need: usize, have: usize },
}

/// Parsed DoIP generic header.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Header {
    pub payload_type: PayloadType,
    pub payload_length: u32,
}

impl fmt::Display for Header {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "DoIP {{ type: {:?}, len: {} }}",
            self.payload_type, self.payload_length
        )
    }
}

fn take(buf: &[u8], idx: usize) -> Result<u8, FrameError> {
    buf.get(idx)
        .copied()
        .ok_or(FrameError::ShortHeader { got: buf.len(), need: HEADER_LEN })
}

fn put(out: &mut [u8], idx: usize, value: u8) -> Result<(), FrameError> {
    let have = out.len();
    let slot = out
        .get_mut(idx)
        .ok_or(FrameError::OutputBufferTooSmall { need: HEADER_LEN, have })?;
    *slot = value;
    Ok(())
}

/// Decode a DoIP header from the first 8 bytes of `buf`.
///
/// # Errors
/// Returns [`FrameError`] on short buffer, bad version, or unknown payload.
pub fn decode_header(buf: &[u8]) -> Result<Header, FrameError> {
    if buf.len() < HEADER_LEN {
        return Err(FrameError::ShortHeader { got: buf.len(), need: HEADER_LEN });
    }
    let v = take(buf, 0)?;
    if v != PROTOCOL_VERSION {
        return Err(FrameError::BadProtocolVersion { got: v, expected: PROTOCOL_VERSION });
    }
    let vi = take(buf, 1)?;
    if vi != PROTOCOL_VERSION_INV {
        return Err(FrameError::BadProtocolInverse { got: vi, expected: PROTOCOL_VERSION_INV });
    }
    let raw = (u16::from(take(buf, 2)?) << 8) | u16::from(take(buf, 3)?);
    let payload_type =
        PayloadType::from_u16(raw).ok_or(FrameError::UnknownPayloadType { raw })?;
    let payload_length = (u32::from(take(buf, 4)?) << 24)
        | (u32::from(take(buf, 5)?) << 16)
        | (u32::from(take(buf, 6)?) << 8)
        | u32::from(take(buf, 7)?);
    if payload_length as usize > MAX_PAYLOAD_LEN {
        return Err(FrameError::PayloadTooLarge {
            got: payload_length,
            max: MAX_PAYLOAD_LEN,
        });
    }
    Ok(Header { payload_type, payload_length })
}

/// Encode a DoIP header into the first 8 bytes of `out`.
///
/// # Errors
/// Returns [`FrameError::OutputBufferTooSmall`] if `out.len() < 8`.
pub fn encode_header(header: Header, out: &mut [u8]) -> Result<(), FrameError> {
    if out.len() < HEADER_LEN {
        return Err(FrameError::OutputBufferTooSmall { need: HEADER_LEN, have: out.len() });
    }
    let pt = header.payload_type.as_u16();
    let len = header.payload_length;
    put(out, 0, PROTOCOL_VERSION)?;
    put(out, 1, PROTOCOL_VERSION_INV)?;
    put(out, 2, ((pt >> 8) & 0xFF) as u8)?;
    put(out, 3, (pt & 0xFF) as u8)?;
    put(out, 4, ((len >> 24) & 0xFF) as u8)?;
    put(out, 5, ((len >> 16) & 0xFF) as u8)?;
    put(out, 6, ((len >> 8) & 0xFF) as u8)?;
    put(out, 7, (len & 0xFF) as u8)?;
    Ok(())
}

/// Encode a complete DoIP frame (header + payload) into a fresh `Vec<u8>`.
///
/// # Errors
/// Returns [`FrameError::PayloadTooLarge`] if the payload exceeds
/// [`MAX_PAYLOAD_LEN`].
pub fn encode_frame(
    payload_type: PayloadType,
    payload: &[u8],
) -> Result<Vec<u8>, FrameError> {
    if payload.len() > MAX_PAYLOAD_LEN {
        return Err(FrameError::PayloadTooLarge {
            got: payload.len() as u32,
            max: MAX_PAYLOAD_LEN,
        });
    }
    let total = HEADER_LEN
        .checked_add(payload.len())
        .ok_or(FrameError::PayloadTooLarge {
            got: payload.len() as u32,
            max: MAX_PAYLOAD_LEN,
        })?;
    let mut out = vec![0u8; total];
    encode_header(
        Header {
            payload_type,
            payload_length: payload.len() as u32,
        },
        &mut out,
    )?;
    let (_, tail) = out.split_at_mut(HEADER_LEN);
    tail.copy_from_slice(payload);
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

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
        let h = decode_header(&ROUTING_ACTIVATION_REQ_BYTES).expect("parse ok");
        assert_eq!(h.payload_type, PayloadType::RoutingActivationRequest);
        assert_eq!(h.payload_length, 7);
    }

    #[test]
    fn decode_diagnostic_message() {
        let h = decode_header(&DIAG_REQ_READ_VIN).expect("parse ok");
        assert_eq!(h.payload_type, PayloadType::DiagnosticMessage);
        assert_eq!(h.payload_length, 7);
    }

    #[test]
    fn encode_header_matches_byte_literal() {
        let mut out = [0u8; HEADER_LEN];
        encode_header(
            Header {
                payload_type: PayloadType::RoutingActivationRequest,
                payload_length: 7,
            },
            &mut out,
        )
        .unwrap();
        assert_eq!(out, ROUTING_ACTIVATION_REQ_BYTES[..HEADER_LEN]);
    }

    #[test]
    fn encode_frame_matches_diag_byte_literal() {
        let frame = encode_frame(
            PayloadType::DiagnosticMessage,
            &[0x0E, 0x00, 0x00, 0x01, 0x22, 0xF1, 0x90],
        )
        .unwrap();
        assert_eq!(frame.as_slice(), &DIAG_REQ_READ_VIN[..]);
    }

    #[test]
    fn encode_decode_roundtrip() {
        let frame = encode_frame(PayloadType::AliveCheckRequest, &[]).unwrap();
        let h = decode_header(&frame).unwrap();
        assert_eq!(h.payload_type, PayloadType::AliveCheckRequest);
        assert_eq!(h.payload_length, 0);
        assert_eq!(frame.len(), HEADER_LEN);
    }

    #[test]
    fn short_header_rejected() {
        let err = decode_header(&[0x02, 0xFD, 0x00, 0x05]).unwrap_err();
        assert!(matches!(err, FrameError::ShortHeader { .. }));
    }

    #[test]
    fn bad_protocol_version_rejected() {
        let mut buf = ROUTING_ACTIVATION_REQ_BYTES;
        buf[0] = 0x01;
        let err = decode_header(&buf).unwrap_err();
        assert!(matches!(err, FrameError::BadProtocolVersion { .. }));
    }

    #[test]
    fn bad_protocol_inverse_rejected() {
        let mut buf = ROUTING_ACTIVATION_REQ_BYTES;
        buf[1] = 0x00;
        let err = decode_header(&buf).unwrap_err();
        assert!(matches!(err, FrameError::BadProtocolInverse { .. }));
    }

    #[test]
    fn unknown_payload_type_rejected() {
        let buf: [u8; 8] = [0x02, 0xFD, 0xAB, 0xCD, 0x00, 0x00, 0x00, 0x00];
        let err = decode_header(&buf).unwrap_err();
        assert!(matches!(err, FrameError::UnknownPayloadType { raw: 0xABCD }));
    }

    #[test]
    fn oversized_payload_rejected() {
        let buf: [u8; 8] = [0x02, 0xFD, 0x80, 0x01, 0xFF, 0xFF, 0xFF, 0xFF];
        let err = decode_header(&buf).unwrap_err();
        assert!(matches!(err, FrameError::PayloadTooLarge { .. }));
    }
}
