#!/usr/bin/env python3
"""
HIL test: measure actual CAN frame rates per message ID.

Reads from Waveshare USB-CAN on COM13 for 10 seconds,
counts frames per CAN ID, compares against DBC GenMsgCycleTime.

Usage:
    python test/hil/test_can_rates.py [--port COM13] [--duration 10]
"""

import struct
import sys
import time
import argparse

FRAME_SIZE = 20


def parse_waveshare_frame(frame_bytes):
    if len(frame_bytes) != FRAME_SIZE:
        return None
    if frame_bytes[0] != 0xAA or frame_bytes[1] != 0x55:
        return None
    frame_type = frame_bytes[3]
    can_id = struct.unpack_from("<I", frame_bytes, 5)[0]
    if frame_type == 0x01:
        can_id &= 0x7FF
    else:
        can_id &= 0x1FFFFFFF
    dlc = frame_bytes[9]
    if dlc > 8:
        dlc = 8
    data = bytes(frame_bytes[10:10 + dlc])
    return {"can_id": can_id, "dlc": dlc, "data": data}


# DBC expected rates (from GenMsgCycleTime)
EXPECTED_RATES = {
    0x001: ("EStop_Broadcast", 10, "CVC"),
    0x010: ("CVC_Heartbeat", 50, "CVC"),
    0x011: ("FZC_Heartbeat", 50, "FZC"),
    0x012: ("RZC_Heartbeat", 50, "RZC"),
    0x100: ("Vehicle_State", 10, "CVC"),
    0x101: ("Torque_Request", 10, "CVC"),
    0x102: ("Steer_Command", 10, "CVC"),
    0x103: ("Brake_Command", 10, "CVC"),
    0x200: ("Steering_Status", 20, "FZC"),
    0x201: ("Brake_Status", 20, "FZC"),
    0x210: ("Brake_Fault", 0, "FZC"),       # event
    0x211: ("Motor_Cutoff_Req", 10, "FZC"),
    0x220: ("Lidar_Distance", 10, "FZC"),
    0x300: ("Motor_Status", 20, "RZC"),
    0x301: ("Motor_Current", 10, "RZC"),
    0x302: ("Motor_Temperature", 100, "RZC"),
    0x303: ("Battery_Status", 1000, "RZC"),
}


def main():
    parser = argparse.ArgumentParser(description="HIL CAN rate test")
    parser.add_argument("--port", default="COM13", help="Waveshare serial port")
    parser.add_argument("--baud", type=int, default=2000000, help="Baud rate")
    parser.add_argument("--duration", type=float, default=10.0, help="Capture duration (s)")
    args = parser.parse_args()

    import serial
    print(f"Opening {args.port} at {args.baud} baud...")
    try:
        s = serial.Serial(args.port, args.baud, timeout=0.05)
    except Exception as e:
        print(f"ERROR: Cannot open {args.port}: {e}")
        print("Close the CAN Monitor app first (it locks the port).")
        sys.exit(1)

    buf = bytearray()
    counts = {}
    first_ts = {}
    last_ts = {}
    start = time.monotonic()

    print(f"Capturing for {args.duration}s...")
    while time.monotonic() - start < args.duration:
        chunk = s.read(500)
        if chunk:
            buf.extend(chunk)
        now = time.monotonic() - start

        while len(buf) >= FRAME_SIZE:
            idx = -1
            for i in range(len(buf) - 1):
                if buf[i] == 0xAA and buf[i + 1] == 0x55:
                    idx = i
                    break
            if idx < 0:
                buf = buf[-1:]
                break
            if idx > 0:
                buf = buf[idx:]
            if len(buf) < FRAME_SIZE:
                break
            frame = parse_waveshare_frame(buf[:FRAME_SIZE])
            buf = buf[FRAME_SIZE:]
            if frame:
                cid = frame["can_id"]
                counts[cid] = counts.get(cid, 0) + 1
                if cid not in first_ts:
                    first_ts[cid] = now
                last_ts[cid] = now

    s.close()
    elapsed = time.monotonic() - start
    total = sum(counts.values())

    print(f"\n{'='*80}")
    print(f"CAN Rate Test Results — {elapsed:.1f}s capture, {total} frames total")
    print(f"{'='*80}")
    print(f"{'ID':>6s}  {'Name':30s}  {'ECU':5s}  {'Count':>6s}  {'Rate':>7s}  {'Expected':>8s}  {'Result':>8s}")
    print(f"{'-'*6}  {'-'*30}  {'-'*5}  {'-'*6}  {'-'*7}  {'-'*8}  {'-'*8}")

    pass_count = 0
    fail_count = 0
    warn_count = 0

    for cid in sorted(counts.keys()):
        count = counts[cid]
        dt = last_ts[cid] - first_ts[cid]
        rate = (count - 1) / dt if dt > 0 and count > 1 else 0

        info = EXPECTED_RATES.get(cid, (f"Unknown_0x{cid:03X}", 0, "?"))
        name, cycle_ms, ecu = info

        if cycle_ms > 0:
            expected_rate = 1000.0 / cycle_ms
            tolerance = 0.3  # 30% tolerance for jitter
            low = expected_rate * (1 - tolerance)
            high = expected_rate * (1 + tolerance)
            if low <= rate <= high:
                result = "PASS"
                pass_count += 1
            else:
                result = "FAIL"
                fail_count += 1
            expected_str = f"{expected_rate:.0f}/s"
        else:
            expected_str = "event"
            result = "OK"
            warn_count += 1

        print(f"0x{cid:03X}  {name:30s}  {ecu:5s}  {count:6d}  {rate:6.1f}/s  {expected_str:>8s}  {result:>8s}")

    print(f"\n{'='*80}")
    print(f"Total: {total} frames, {total/elapsed:.0f}/s bus load")
    print(f"PASS: {pass_count}  FAIL: {fail_count}  EVENT: {warn_count}")
    if fail_count == 0:
        print("ALL CYCLIC MESSAGES WITHIN TOLERANCE")
    else:
        print(f"{fail_count} MESSAGE(S) OUTSIDE RATE TOLERANCE")
    print(f"{'='*80}")

    sys.exit(1 if fail_count > 0 else 0)


if __name__ == "__main__":
    main()
