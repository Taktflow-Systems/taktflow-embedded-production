// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Taktflow Systems
//
// ISO-TP (ISO 15765-2) encode/decode. Pure logic: no CAN I/O. The
// Linux-only socket code in `socket.rs` drives this state machine.
//
// Tests-first skeleton; green commit adds the implementation.

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_frame_encode_short_payload() {
        // UDS 0x22 0xF1 0x90 -> single frame with PCI byte 0x03.
        // Data: 03 22 F1 90 00 00 00 00 (8 bytes, CAN DLC = 8)
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
        // 12-byte UDS -> first frame (6 bytes of data after 2 PCI bytes)
        // + 1 consecutive frame (6 bytes after 1 PCI byte).
        let payload: Vec<u8> = (0..12).collect();
        let frames = encode(&payload).unwrap();
        assert_eq!(frames.len(), 2);

        // First frame: PCI 0x1N NN where NNN is total length 0x00C = 12.
        assert_eq!(frames[0][0], 0x10);
        assert_eq!(frames[0][1], 0x0C);
        assert_eq!(&frames[0][2..8], &payload[0..6]);

        // Consecutive frame 1: PCI 0x21, then 6 payload bytes.
        assert_eq!(frames[1][0], 0x21);
        assert_eq!(&frames[1][1..7], &payload[6..12]);

        // Reassemble.
        let mut r = Reassembler::new();
        assert!(r.push(&frames[0]).unwrap().is_none());
        let out = r.push(&frames[1]).unwrap();
        assert_eq!(out, Some(payload));
    }

    #[test]
    fn consecutive_frame_sequence_number_wraps() {
        // 80-byte payload -> 1 first-frame (6 bytes) + 13 consecutive
        // frames (7 bytes each) but the SN wraps modulo 16.
        let payload: Vec<u8> = (0..80).map(|i| (i as u8)).collect();
        let frames = encode(&payload).unwrap();
        // Frame 0: first frame.
        assert_eq!(frames[0][0] & 0xF0, 0x10);
        // Frame 1: SN=1
        assert_eq!(frames[1][0], 0x21);
        // Frame 16: SN wraps to 0, stays in CF band.
        assert_eq!(frames[16][0], 0x20);
        // Round-trip.
        let mut r = Reassembler::new();
        for f in &frames {
            let _ = r.push(f).unwrap();
        }
        // Last push returns Some.
        // (already consumed above — repeat via fresh reassembler.)
        let mut r = Reassembler::new();
        let mut out = None;
        for f in &frames {
            out = r.push(f).unwrap();
        }
        assert_eq!(out, Some(payload));
    }

    #[test]
    fn rejects_over_4095_byte_payload() {
        // ISO-TP classic single address ceiling is 4095 (0xFFF) bytes.
        let payload = vec![0u8; 4096];
        assert!(encode(&payload).is_err());
    }

    #[test]
    fn rejects_out_of_order_consecutive_frame() {
        let payload: Vec<u8> = (0..12).collect();
        let frames = encode(&payload).unwrap();
        let mut r = Reassembler::new();
        let _ = r.push(&frames[0]).unwrap();
        // Tamper: SN becomes 5 instead of 1.
        let mut bad = frames[1];
        bad[0] = 0x25;
        assert!(r.push(&bad).is_err());
    }
}
