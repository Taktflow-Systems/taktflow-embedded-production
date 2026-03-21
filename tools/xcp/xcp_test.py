#!/usr/bin/env python3
"""
XCP Verification Script — tests XCP slave on a live ECU.

Requires: pip install pyxcp python-can
Hardware: PCAN-USB adapter connected to CAN bus

Usage:
    python tools/xcp/xcp_test.py                    # Test CVC (default)
    python tools/xcp/xcp_test.py --ecu fzc          # Test FZC
    python tools/xcp/xcp_test.py --ecu rzc          # Test RZC
    python tools/xcp/xcp_test.py --addr 0x20000000  # Read specific address

@traces_to Phase 4: XCP measurement and calibration
"""

import argparse
import json
import struct
import sys

# XCP CAN ID pairs per ECU
ECU_CAN_IDS = {
    'cvc': (0x550, 0x551),
    'fzc': (0x552, 0x553),
    'rzc': (0x554, 0x555),
    'sc':  (0x556, 0x557),
}


def test_with_pyxcp(ecu: str, addr: int | None = None):
    """Test XCP connection using pyXCP library."""
    try:
        from pyxcp import Master
        from pyxcp.transport.can import Can
    except ImportError:
        print("pyXCP not installed. Install with: pip install pyxcp python-can")
        print("Falling back to raw CAN test...")
        return test_with_raw_can(ecu, addr)

    req_id, resp_id = ECU_CAN_IDS[ecu]

    config = {
        "TRANSPORT_LAYER": "CAN",
        "CAN": {
            "INTERFACE": "pcan",
            "CHANNEL": "PCAN_USBBUS1",
            "BITRATE": 500000,
            "RECEIVE_ID": hex(resp_id),
            "SEND_ID": hex(req_id),
        },
    }

    print(f"XCP Test — ECU: {ecu.upper()}")
    print(f"  Request CAN ID:  0x{req_id:03X}")
    print(f"  Response CAN ID: 0x{resp_id:03X}")
    print()

    with Master(config) as xcp:
        # 1. CONNECT
        print("[1] CONNECT...", end=" ")
        result = xcp.connect()
        print(f"OK — resource=0x{result.resource:02X}, maxCTO={result.maxCto}")

        # 2. GET_STATUS
        print("[2] GET_STATUS...", end=" ")
        status = xcp.getStatus()
        print(f"OK — session=0x{status.sessionStatus:02X}")

        # 3. SHORT_UPLOAD
        if addr:
            print(f"[3] SHORT_UPLOAD 0x{addr:08X} (4 bytes)...", end=" ")
            data = xcp.shortUpload(4, 0, addr)
            val = struct.unpack('<I', bytes(data))[0]
            print(f"OK — value = {val} (0x{val:08X})")

        # 4. DISCONNECT
        print("[4] DISCONNECT...", end=" ")
        xcp.disconnect()
        print("OK")

    print()
    print("ALL TESTS PASSED")
    return 0


def test_with_raw_can(ecu: str, addr: int | None = None):
    """Fallback: test XCP using raw python-can (no pyXCP)."""
    try:
        import can
    except ImportError:
        print("python-can not installed. Install with: pip install python-can")
        return 1

    req_id, resp_id = ECU_CAN_IDS[ecu]

    print(f"XCP Raw CAN Test — ECU: {ecu.upper()}")
    print(f"  Request CAN ID:  0x{req_id:03X}")
    print(f"  Response CAN ID: 0x{resp_id:03X}")
    print()

    bus = can.interface.Bus(interface='pcan', channel='PCAN_USBBUS1', bitrate=500000)

    try:
        # 1. CONNECT (0xFF 0x00)
        print("[1] CONNECT...", end=" ")
        msg = can.Message(arbitration_id=req_id, data=[0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00], is_extended_id=False)
        bus.send(msg)
        resp = bus.recv(timeout=2.0)
        if resp and resp.arbitration_id == resp_id and resp.data[0] == 0xFF:
            print(f"OK — resource=0x{resp.data[1]:02X}, maxCTO={resp.data[3]}")
        else:
            print(f"FAIL — response: {resp}")
            return 1

        # 2. GET_STATUS (0xFD)
        print("[2] GET_STATUS...", end=" ")
        msg = can.Message(arbitration_id=req_id, data=[0xFD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00], is_extended_id=False)
        bus.send(msg)
        resp = bus.recv(timeout=2.0)
        if resp and resp.data[0] == 0xFF:
            print(f"OK — session=0x{resp.data[1]:02X}")
        else:
            print(f"FAIL — response: {resp}")
            return 1

        # 3. SHORT_UPLOAD (0xF4)
        if addr:
            print(f"[3] SHORT_UPLOAD 0x{addr:08X} (4 bytes)...", end=" ")
            a = addr.to_bytes(4, 'big')
            msg = can.Message(arbitration_id=req_id,
                              data=[0xF4, 0x04, 0x00, 0x00, a[0], a[1], a[2], a[3]],
                              is_extended_id=False)
            bus.send(msg)
            resp = bus.recv(timeout=2.0)
            if resp and resp.data[0] == 0xFF:
                val = struct.unpack_from('<I', bytes(resp.data), 1)[0]
                print(f"OK — value = {val} (0x{val:08X})")
            else:
                print(f"FAIL — response: {resp}")
                return 1

        # 4. DISCONNECT (0xFE)
        print("[4] DISCONNECT...", end=" ")
        msg = can.Message(arbitration_id=req_id, data=[0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00], is_extended_id=False)
        bus.send(msg)
        resp = bus.recv(timeout=2.0)
        if resp and resp.data[0] == 0xFF:
            print("OK")
        else:
            print(f"FAIL — response: {resp}")
            return 1

    finally:
        bus.shutdown()

    print()
    print("ALL TESTS PASSED")
    return 0


def main():
    parser = argparse.ArgumentParser(description='XCP slave verification')
    parser.add_argument('--ecu', default='cvc', choices=['cvc', 'fzc', 'rzc', 'sc'],
                        help='Target ECU (default: cvc)')
    parser.add_argument('--addr', type=lambda x: int(x, 0), default=None,
                        help='Memory address to read (hex, e.g., 0x20000100)')
    args = parser.parse_args()

    return test_with_pyxcp(args.ecu, args.addr)


if __name__ == '__main__':
    sys.exit(main())
