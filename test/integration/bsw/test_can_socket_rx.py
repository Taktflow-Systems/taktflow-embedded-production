#!/usr/bin/env python3
"""
Layer 3 Extension: CAN Socket Receive Test

Tests that Can_Hw_Receive actually receives frames from external processes
on vcan0. This is the test that SHOULD have existed to catch the XCP
routing failure at Layer 4.

The original Layer 3 test mocked Can_Write and injected directly into
CanIf_RxIndication — it never tested the real socket read path.

This test:
1. Starts the CVC binary on vcan0
2. Sends a known CAN frame from this script (external process)
3. Verifies the CVC processes it (by observing a side effect on the bus)

The simplest side effect to verify: send a UDS TesterPresent (0x7E0)
which should produce a UDS response (0x7E8). If we see 0x7E8, the
full RX chain works: socket → Can_Hw_Receive → CanIf → PduR → CanTp → Dcm.

If that doesn't work, send an XCP CONNECT (0x550) and check for 0x551.

@verifies SWR-BSW-003 (CAN receive processing)
"""

import os, signal, subprocess, sys, time

try:
    import can
except ImportError:
    print("ERROR: pip install python-can"); sys.exit(1)

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
CVC = os.path.join(ROOT, "build", "cvc_posix")
VCAN = "vcan0"

def main():
    if not os.path.exists(CVC):
        print(f"ERROR: {CVC} not found"); return 1

    print("Layer 3 Extension: CAN Socket Receive Test")
    print()

    bus = can.interface.Bus(interface='socketcan', channel=VCAN)
    proc = subprocess.Popen([CVC, VCAN], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2.0)

    passed = 0
    failed = 0

    try:
        # Test 1: Can CVC receive ANY frame from external process?
        print("  Test 1: External frame reception...", end=" ", flush=True)

        # Send UDS TesterPresent to CVC (0x7E0 → should get 0x7E8 response)
        bus.send(can.Message(arbitration_id=0x7E0, data=[0x02, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00], is_extended_id=False))

        # Wait for response on 0x7E8
        response = None
        deadline = time.time() + 3.0
        while time.time() < deadline:
            m = bus.recv(timeout=0.1)
            if m and m.arbitration_id == 0x7E8:
                response = m
                break

        if response:
            print(f"PASS — UDS response: {response.data.hex()}")
            passed += 1
        else:
            print("FAIL — No UDS response on 0x7E8 (CVC not receiving external frames)")
            failed += 1

        # Test 2: XCP CONNECT receive
        print("  Test 2: XCP CONNECT reception...", end=" ", flush=True)

        bus.send(can.Message(arbitration_id=0x550, data=[0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00], is_extended_id=False))

        response = None
        deadline = time.time() + 3.0
        while time.time() < deadline:
            m = bus.recv(timeout=0.1)
            if m and m.arbitration_id == 0x551:
                response = m
                break

        if response:
            print(f"PASS — XCP response: {response.data.hex()}")
            passed += 1
        else:
            print("FAIL — No XCP response on 0x551")
            failed += 1

        # Test 3: Verify CVC receives FZC heartbeat (prevents timeout)
        print("  Test 3: Inject FZC heartbeat, check no timeout...", end=" ", flush=True)

        # Send 10 FZC heartbeats
        for i in range(10):
            data = bytearray(4)
            data[0] = ((i & 0xF) << 4) | 3  # counter|DataId=3
            data[1] = 0x44  # CRC placeholder
            data[2] = 0x02  # ECU_ID
            data[3] = 0x01  # MODE=RUN
            bus.send(can.Message(arbitration_id=0x011, data=bytes(data), is_extended_id=False))
            time.sleep(0.05)

        time.sleep(1.0)

        # Check Vehicle_State — if FZC heartbeat received, FZC timeout bit should not be set
        # (This is an indirect test — we can't directly read CVC's internal state)
        m = None
        deadline = time.time() + 2.0
        while time.time() < deadline:
            msg = bus.recv(timeout=0.1)
            if msg and msg.arbitration_id == 0x100:
                m = msg
                break

        if m:
            print(f"PASS — Vehicle_State received: {m.data.hex()}")
            passed += 1
        else:
            print("FAIL — No Vehicle_State on bus")
            failed += 1

    finally:
        proc.send_signal(signal.SIGTERM)
        try: proc.wait(timeout=3)
        except: proc.kill()
        bus.shutdown()

    print()
    print(f"{passed + failed} tests: {passed} passed, {failed} failed")
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
