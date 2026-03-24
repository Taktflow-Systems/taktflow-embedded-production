"""Waveshare USB-CAN adapter frame parsing and TX builder."""
import struct
import time

FRAME_SIZE = 20


def parse_fixed_frame(frame_bytes):
    """Parse a 20-byte fixed-length Waveshare frame.

    Returns dict with can_id, dlc, data, frame_type, frame_format,
    checksum_ok — or None if invalid.
    """
    if len(frame_bytes) != FRAME_SIZE:
        return None
    if frame_bytes[0] != 0xAA or frame_bytes[1] != 0x55:
        return None
    type_byte = frame_bytes[2]
    frame_type = frame_bytes[3]  # 0x01=std, 0x02=ext
    frame_format = frame_bytes[4]  # 0x01=data, 0x02=remote
    can_id = struct.unpack_from("<I", frame_bytes, 5)[0]
    if frame_type == 0x01:
        can_id &= 0x7FF
    else:
        can_id &= 0x1FFFFFFF
    dlc = frame_bytes[9]
    if dlc > 8:
        dlc = 8
    data = frame_bytes[10:10 + dlc]
    expected_cksum = sum(frame_bytes[2:19]) & 0xFF
    actual_cksum = frame_bytes[19]
    return {
        "can_id": can_id,
        "dlc": dlc,
        "data": bytes(data),
        "frame_type": "STD" if frame_type == 0x01 else "EXT",
        "frame_format": "DATA" if frame_format == 0x01 else "RTR",
        "checksum_ok": expected_cksum == actual_cksum,
    }


def parse_variable_frame(buf):
    """Parse a variable-length Waveshare frame from buffer.

    Returns (frame_dict, consumed_bytes) or (None, 1) on error.
    Returns (None, 0) if buffer too short (need more data).
    """
    if len(buf) < 2 or buf[0] != 0xAA:
        return None, 1
    type_byte = buf[1]
    if type_byte < 0xC0:
        return None, 1
    is_ext = bool(type_byte & 0x20)
    is_remote = bool(type_byte & 0x10)
    dlc = type_byte & 0x0F
    if dlc > 8:
        return None, 1
    id_len = 4 if is_ext else 2
    frame_len = 1 + 1 + id_len + dlc + 1
    if len(buf) < frame_len:
        return None, 0
    if is_ext:
        can_id = struct.unpack_from("<I", buf, 2)[0] & 0x1FFFFFFF
    else:
        can_id = struct.unpack_from("<H", buf, 2)[0] & 0x7FF
    data_start = 2 + id_len
    data = bytes(buf[data_start:data_start + dlc])
    tail = buf[data_start + dlc]
    if tail != 0x55:
        return None, 1
    return {
        "can_id": can_id,
        "dlc": dlc,
        "data": data,
        "frame_type": "EXT" if is_ext else "STD",
        "frame_format": "RTR" if is_remote else "DATA",
        "checksum_ok": True,
    }, frame_len


def detect_protocol(ser, timeout=2.0):
    """Auto-detect Waveshare protocol (fixed or variable).

    Returns ("fixed"|"variable"|"unknown", remaining_buf).
    """
    start = time.time()
    buf = bytearray()
    while time.time() - start < timeout:
        chunk = ser.read(max(1, ser.in_waiting))
        if chunk:
            buf.extend(chunk)
        if len(buf) >= 4:
            for i in range(len(buf) - 1):
                if buf[i] == 0xAA:
                    if buf[i + 1] == 0x55:
                        return "fixed", buf[i:]
                    elif buf[i + 1] >= 0xC0:
                        return "variable", buf[i:]
            buf = buf[-1:]
    return "unknown", buf


def build_tx_frame(can_id, data, extended=False):
    """Build a 20-byte TX frame for the Waveshare adapter.

    Args:
        can_id: CAN arbitration ID.
        data: Payload bytes (up to 8).
        extended: True for 29-bit extended ID, False for 11-bit standard.

    Returns:
        bytes: 20-byte Waveshare TX frame.
    """
    frame = bytearray(FRAME_SIZE)
    frame[0] = 0xAA
    frame[1] = 0x55
    frame[2] = 0x01  # send data command
    frame[3] = 0x02 if extended else 0x01  # frame type
    frame[4] = 0x01  # data frame
    struct.pack_into("<I", frame, 5, can_id)
    dlc = min(len(data), 8)
    frame[9] = dlc
    frame[10:10 + dlc] = data[:dlc]
    # Pad remaining data bytes with 0x00 (already zero from bytearray)
    frame[19] = sum(frame[2:19]) & 0xFF
    return bytes(frame)
