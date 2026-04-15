#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
#
# Live smoke test against a physical STM32 ECU via the CAN-to-DoIP
# proxy running on the Pi. Requires:
#   - proxy listening on 127.0.0.1:13400, --can-interface can0
#   - physical ECU powered and attached to the can0 bus
#   - firmware has a working Dcm ReadDataByIdentifier 0xF190 handler
#
# If the ECU does not respond within 3 seconds this script exits with
# a warning — graceful degradation. The proxy itself has already been
# validated against vcan0 in the main smoke test.

import socket
import struct
import sys

def build_frame(pt, payload):
    return struct.pack(">BBHI", 0x02, 0xFD, pt, len(payload)) + payload

def parse_header(buf):
    return struct.unpack(">BBHI", buf[:8])

def main():
    s = socket.create_connection(("127.0.0.1", 13400), timeout=5.0)

    ra = build_frame(0x0005, b"\x0E\x00\x00\x00\x00\x00\x00")
    print(f"tx RA   {ra.hex()}")
    s.sendall(ra)
    rsp = s.recv(17)
    print(f"rx RA   {rsp.hex()}")

    diag = build_frame(0x8001, b"\x0E\x00\x00\x01\x22\xF1\x90")
    print(f"tx DIAG {diag.hex()}")
    s.sendall(diag)

    s.settimeout(4.0)
    try:
        first = s.recv(13)
        print(f"rx [1]  {first.hex()}")
        if len(first) >= 8:
            _, _, pt, plen = parse_header(first)
            if pt == 0x8002:
                print("  -> DoIP DiagnosticAck received")
        second = s.recv(512)
        print(f"rx [2]  {second.hex()}")
    except socket.timeout:
        print("  -> timeout waiting for ECU response (expected if no flashed F190 handler)")
        return 2

    print("DONE (bytes captured above)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
