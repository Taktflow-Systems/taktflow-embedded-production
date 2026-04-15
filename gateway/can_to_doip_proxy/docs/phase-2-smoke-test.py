#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
#
# Phase 2 Line B smoke test. Runs on the Pi.
#
# 1. Spawns a mock ECU thread that listens on vcan0 for CAN ID 0x7E0 and
#    responds on 0x7E8 with a canned "ReadDataByIdentifier 0xF190" VIN.
# 2. Opens a TCP client to 127.0.0.1:13400 and sends:
#      - DoIP routing activation (payload type 0x0005)
#      - DoIP diagnostic message (payload type 0x8001) containing UDS
#        0x22 0xF1 0x90 (Read VIN)
# 3. Prints the hex of every byte sent and received so the wire protocol
#    verification is reproducible.
# 4. Asserts the expected round-trip.

import socket
import struct
import sys
import threading
import time


def build_frame(payload_type: int, payload: bytes) -> bytes:
    return struct.pack(">BBHI", 0x02, 0xFD, payload_type, len(payload)) + payload


def parse_header(buf: bytes):
    v, vi, pt, plen = struct.unpack(">BBHI", buf[:8])
    return v, vi, pt, plen


def isotp_encode_single(payload: bytes) -> bytes:
    # Single frame 0x0N, len <= 7.
    assert len(payload) <= 7
    frame = bytes([len(payload)]) + payload + b"\x00" * (7 - len(payload))
    return frame


def isotp_encode(payload: bytes) -> list:
    if len(payload) <= 7:
        return [isotp_encode_single(payload)]
    frames = []
    total = len(payload)
    first = bytes([0x10 | ((total >> 8) & 0x0F), total & 0xFF]) + payload[:6]
    frames.append(first)
    sn = 1
    cursor = 6
    while cursor < len(payload):
        chunk = payload[cursor : cursor + 7]
        cf = bytes([0x20 | (sn & 0x0F)]) + chunk + b"\x00" * (7 - len(chunk))
        frames.append(cf)
        cursor += 7
        sn = (sn + 1) & 0x0F
    return frames


def mock_ecu_worker():
    """Listen on vcan0 for 0x7E0, respond on 0x7E8 with a VIN."""
    import ctypes

    try:
        sock = socket.socket(
            socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW
        )
    except AttributeError:
        print("[mock-ecu] AF_CAN not available on this Python", flush=True)
        return
    sock.bind(("vcan0",))
    sock.settimeout(10.0)
    print("[mock-ecu] listening on vcan0 for 0x7E0", flush=True)
    # Classic CAN frame layout: can_id (u32 LE) + can_dlc (u8) + pad(3) + data(8)
    fmt = "=IB3x8s"
    # Read up to 3 frames (enough for single- or first+consecutive).
    reassembled = b""
    first_frame_total = None
    for _ in range(5):
        try:
            raw = sock.recv(16)
        except socket.timeout:
            print("[mock-ecu] timeout waiting for request", flush=True)
            sock.close()
            return
        can_id, dlc, data = struct.unpack(fmt, raw)
        print(
            f"[mock-ecu] rx can_id=0x{can_id & 0x1FFFFFFF:x} dlc={dlc} data={data.hex()}",
            flush=True,
        )
        if (can_id & 0x1FFFFFFF) != 0x7E0:
            continue
        pci_hi = data[0] & 0xF0
        if pci_hi == 0x00:
            length = data[0] & 0x0F
            reassembled = data[1 : 1 + length]
            break
        if pci_hi == 0x10:
            total = ((data[0] & 0x0F) << 8) | data[1]
            first_frame_total = total
            reassembled = data[2:8]
            continue
        if pci_hi == 0x20:
            reassembled += data[1:8]
            if first_frame_total is not None and len(reassembled) >= first_frame_total:
                reassembled = reassembled[:first_frame_total]
                break
    print(f"[mock-ecu] reassembled UDS: {reassembled.hex()}", flush=True)
    if reassembled[:3] != b"\x22\xF1\x90":
        print(
            f"[mock-ecu] unexpected UDS request; not responding: {reassembled.hex()}",
            flush=True,
        )
        sock.close()
        return
    response = b"\x62\xF1\x90" + b"TAKTFLOW_CVC_0001"  # 3 + 17 = 20 bytes
    frames = isotp_encode(response)
    for f in frames:
        pkt = struct.pack(fmt, 0x7E8, 8, f)
        sock.send(pkt)
        print(f"[mock-ecu] tx 0x7E8 {f.hex()}", flush=True)
    sock.close()


def main():
    host = "127.0.0.1"
    port = 13400
    t = threading.Thread(target=mock_ecu_worker, daemon=True)
    t.start()
    time.sleep(0.2)

    print(f"[cda-sim] connecting to {host}:{port}", flush=True)
    s = socket.create_connection((host, port), timeout=5.0)

    # Routing activation request: source 0x0E00, activation type 0x00.
    ra_payload = b"\x0E\x00\x00" + b"\x00\x00\x00\x00"
    ra_frame = build_frame(0x0005, ra_payload)
    print(f"[cda-sim] tx RA:   {ra_frame.hex()}", flush=True)
    s.sendall(ra_frame)

    # Expect RA response: header(8) + 9-byte body.
    buf = s.recv(17)
    print(f"[cda-sim] rx RA:   {buf.hex()}", flush=True)
    v, vi, pt, plen = parse_header(buf)
    assert v == 0x02 and vi == 0xFD, f"bad protocol version {v:02x}/{vi:02x}"
    assert pt == 0x0006, f"expected RA response 0x0006, got 0x{pt:04x}"
    assert plen == 9
    assert buf[8 + 4] == 0x10, "activation code != ACTIVATION_OK"

    # Diagnostic message: source 0x0E00, target 0x0001, UDS 0x22 0xF1 0x90.
    diag_payload = b"\x0E\x00\x00\x01" + b"\x22\xF1\x90"
    diag_frame = build_frame(0x8001, diag_payload)
    print(f"[cda-sim] tx DIAG: {diag_frame.hex()}", flush=True)
    s.sendall(diag_frame)

    # Read ack first.
    ack = s.recv(8 + 5)
    print(f"[cda-sim] rx ACK:  {ack.hex()}", flush=True)
    _, _, pt, _ = parse_header(ack)
    assert pt == 0x8002, f"expected DiagnosticAck 0x8002, got 0x{pt:04x}"

    # Read diag response.
    hdr = s.recv(8)
    _, _, pt, plen = parse_header(hdr)
    assert pt == 0x8001, f"expected DiagnosticMessage 0x8001, got 0x{pt:04x}"
    body = b""
    while len(body) < plen:
        chunk = s.recv(plen - len(body))
        if not chunk:
            break
        body += chunk
    full = hdr + body
    print(f"[cda-sim] rx DIAG: {full.hex()}", flush=True)
    # Body: src(0x0001) + tgt(0x0E00) + UDS response.
    assert body[:4] == b"\x00\x01\x0E\x00", f"bad src/tgt in response: {body[:4].hex()}"
    assert body[4:7] == b"\x62\xF1\x90", f"bad UDS sid+did: {body[4:7].hex()}"
    vin = body[7:]
    print(f"[cda-sim] VIN bytes: {vin!r}", flush=True)
    assert vin == b"TAKTFLOW_CVC_0001", f"VIN mismatch: {vin!r}"

    s.close()
    print("[cda-sim] SUCCESS: full round-trip CAN <-> DoIP via proxy", flush=True)


if __name__ == "__main__":
    main()
