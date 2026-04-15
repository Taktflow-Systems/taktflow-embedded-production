// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// ISO-TP (ISO 15765-2) encode/decode for classic CAN 2.0B 8-byte frames.
//
// Frame layout (single-address, no extended addressing):
//   Single frame:      [0x0N, d0, d1, ..., dN-1]        where N <= 7
//   First frame:       [0x1N, NN, d0, d1, d2, d3, d4, d5]
//                      where NNNN is 12-bit total length (>= 8)
//   Consecutive frame: [0x2N, d0..d6]                    where N is 4-bit SN
//   Flow control:      [0x30, BlockSize, STmin, ...]     handled by caller
//
// The PDU cap in single-address mode is 4095 bytes (0xFFF).

/// Maximum single-address ISO-TP payload (ISO 15765-2).
pub const MAX_PDU_LEN: usize = 4095;

/// Fixed CAN data length for classic CAN frames.
pub const CAN_DLC: usize = 8;

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum IsoTpError {
    #[error("payload too large: {got} > {max}")]
    PayloadTooLarge { got: usize, max: usize },
    #[error("short frame: got {got} bytes")]
    ShortFrame { got: usize },
    #[error("unexpected consecutive frame outside session")]
    UnexpectedConsecutiveFrame,
    #[error("out-of-order sequence: expected {expected}, got {got}")]
    BadSequence { expected: u8, got: u8 },
    #[error("reassembly overflow")]
    ReassemblyOverflow,
    #[error("unknown PCI type 0x{pci:02x}")]
    UnknownPci { pci: u8 },
}

/// Encode a UDS payload into one or more classic-CAN ISO-TP frames, each
/// 8 bytes long. Single frame if payload ≤ 7 bytes, else first frame +
/// consecutive frames.
///
/// # Errors
/// Returns [`IsoTpError::PayloadTooLarge`] if the payload exceeds
/// [`MAX_PDU_LEN`].
pub fn encode(payload: &[u8]) -> Result<Vec<[u8; CAN_DLC]>, IsoTpError> {
    if payload.len() > MAX_PDU_LEN {
        return Err(IsoTpError::PayloadTooLarge {
            got: payload.len(),
            max: MAX_PDU_LEN,
        });
    }

    // Single frame.
    if payload.len() <= 7 {
        let mut frame = [0u8; CAN_DLC];
        frame[0] = payload.len() as u8; // PCI = 0x0N
        let (_, rest) = frame.split_at_mut(1);
        let (head, _) = rest.split_at_mut(payload.len());
        head.copy_from_slice(payload);
        return Ok(vec![frame]);
    }

    // First frame + consecutive frames.
    let total = payload.len() as u16;
    let mut out: Vec<[u8; CAN_DLC]> = Vec::new();

    let mut first = [0u8; CAN_DLC];
    first[0] = 0x10 | (((total >> 8) & 0x0F) as u8);
    first[1] = (total & 0xFF) as u8;
    // 6 data bytes in the first frame.
    let (ff_head, rest) = payload.split_at(6);
    let (_, ff_data) = first.split_at_mut(2);
    ff_data.copy_from_slice(ff_head);
    out.push(first);

    let mut sn: u8 = 1;
    let mut cursor = rest;
    while !cursor.is_empty() {
        let take = core::cmp::min(cursor.len(), 7);
        let mut cf = [0u8; CAN_DLC];
        cf[0] = 0x20 | (sn & 0x0F);
        let (chunk, tail) = cursor.split_at(take);
        let (_, cf_data) = cf.split_at_mut(1);
        let (data_head, _) = cf_data.split_at_mut(take);
        data_head.copy_from_slice(chunk);
        out.push(cf);
        cursor = tail;
        sn = sn.wrapping_add(1) & 0x0F;
    }

    Ok(out)
}

/// Incremental ISO-TP reassembler. Feed frames with [`Reassembler::push`];
/// when the message is complete, that call returns `Ok(Some(bytes))`.
#[derive(Debug, Default)]
pub struct Reassembler {
    expected_total: Option<usize>,
    next_sn: u8,
    buf: Vec<u8>,
}

impl Reassembler {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    /// Push one CAN frame (8 bytes) into the reassembler.
    ///
    /// # Errors
    /// Returns an [`IsoTpError`] on short frame, unknown PCI type,
    /// out-of-order SN, or buffer overflow.
    pub fn push(&mut self, frame: &[u8]) -> Result<Option<Vec<u8>>, IsoTpError> {
        if frame.len() < CAN_DLC {
            return Err(IsoTpError::ShortFrame { got: frame.len() });
        }
        let pci = *frame.first().ok_or(IsoTpError::ShortFrame { got: 0 })?;
        match pci & 0xF0 {
            0x00 => self.handle_single(frame, pci),
            0x10 => self.handle_first(frame, pci),
            0x20 => self.handle_consecutive(frame, pci),
            other => Err(IsoTpError::UnknownPci { pci: other }),
        }
    }

    fn handle_single(&mut self, frame: &[u8], pci: u8) -> Result<Option<Vec<u8>>, IsoTpError> {
        let len = (pci & 0x0F) as usize;
        if len > 7 || frame.len() < 1usize.saturating_add(len) {
            return Err(IsoTpError::ShortFrame { got: frame.len() });
        }
        let (_, tail) = frame.split_at(1);
        let (data, _) = tail.split_at(len);
        // Reset any prior state.
        self.expected_total = None;
        self.next_sn = 0;
        self.buf.clear();
        Ok(Some(data.to_vec()))
    }

    fn handle_first(&mut self, frame: &[u8], pci: u8) -> Result<Option<Vec<u8>>, IsoTpError> {
        let len_hi = (pci & 0x0F) as u16;
        let len_lo = u16::from(*frame.get(1).ok_or(IsoTpError::ShortFrame { got: 1 })?);
        let total = ((len_hi << 8) | len_lo) as usize;
        if total > MAX_PDU_LEN {
            return Err(IsoTpError::PayloadTooLarge {
                got: total,
                max: MAX_PDU_LEN,
            });
        }
        self.expected_total = Some(total);
        self.next_sn = 1;
        self.buf.clear();
        self.buf.reserve(total);
        let (_, data) = frame.split_at(2);
        let take = core::cmp::min(data.len(), 6);
        let (head, _) = data.split_at(take);
        self.buf.extend_from_slice(head);
        Ok(None)
    }

    fn handle_consecutive(&mut self, frame: &[u8], pci: u8) -> Result<Option<Vec<u8>>, IsoTpError> {
        let total = self
            .expected_total
            .ok_or(IsoTpError::UnexpectedConsecutiveFrame)?;
        let sn = pci & 0x0F;
        if sn != self.next_sn {
            return Err(IsoTpError::BadSequence {
                expected: self.next_sn,
                got: sn,
            });
        }
        self.next_sn = self.next_sn.wrapping_add(1) & 0x0F;
        let (_, data) = frame.split_at(1);
        let remaining = total.saturating_sub(self.buf.len());
        let take = core::cmp::min(core::cmp::min(data.len(), 7), remaining);
        let (head, _) = data.split_at(take);
        self.buf.extend_from_slice(head);
        if self.buf.len() >= total {
            let out = core::mem::take(&mut self.buf);
            self.expected_total = None;
            self.next_sn = 0;
            return Ok(Some(out));
        }
        Ok(None)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_frame_encode_short_payload() {
        let frames = encode(&[0x22, 0xF1, 0x90]).unwrap();
        assert_eq!(frames.len(), 1);
        let frame = &frames[0];
        assert_eq!(frame[0], 0x03);
        assert_eq!(&frame[1..4], &[0x22, 0xF1, 0x90]);
    }

    #[test]
    fn single_frame_decode_short_payload() {
        let frame = [0x03u8, 0x22, 0xF1, 0x90, 0x00, 0x00, 0x00, 0x00];
        let mut r = Reassembler::new();
        let out = r.push(&frame).unwrap();
        assert_eq!(out, Some(vec![0x22, 0xF1, 0x90]));
    }

    #[test]
    fn multi_frame_encode_decode_12_bytes() {
        let payload: Vec<u8> = (0..12).collect();
        let frames = encode(&payload).unwrap();
        assert_eq!(frames.len(), 2);

        assert_eq!(frames[0][0], 0x10);
        assert_eq!(frames[0][1], 0x0C);
        assert_eq!(&frames[0][2..8], &payload[0..6]);

        assert_eq!(frames[1][0], 0x21);
        assert_eq!(&frames[1][1..7], &payload[6..12]);

        let mut r = Reassembler::new();
        assert!(r.push(&frames[0]).unwrap().is_none());
        let out = r.push(&frames[1]).unwrap();
        assert_eq!(out, Some(payload));
    }

    #[test]
    fn consecutive_frame_sequence_number_wraps() {
        // Need enough frames so SN wraps: 1 first (6 bytes) + N CFs (7 bytes each).
        // Need at least 16 CFs => total payload >= 6 + 16*7 = 118 bytes.
        let payload: Vec<u8> = (0..140).map(|i| i as u8).collect();
        let frames = encode(&payload).unwrap();
        assert_eq!(frames[0][0] & 0xF0, 0x10);
        assert_eq!(frames[1][0], 0x21);
        // Frame at index 16 is the 16th CF (SN should have wrapped to 0).
        assert_eq!(frames[16][0], 0x20);

        let mut r = Reassembler::new();
        let mut out = None;
        for f in &frames {
            out = r.push(f).unwrap();
        }
        assert_eq!(out, Some(payload));
    }

    #[test]
    fn rejects_over_4095_byte_payload() {
        let payload = vec![0u8; 4096];
        assert!(encode(&payload).is_err());
    }

    #[test]
    fn rejects_out_of_order_consecutive_frame() {
        let payload: Vec<u8> = (0..12).collect();
        let frames = encode(&payload).unwrap();
        let mut r = Reassembler::new();
        let _ = r.push(&frames[0]).unwrap();
        let mut bad = frames[1];
        bad[0] = 0x25;
        assert!(r.push(&bad).is_err());
    }
}
