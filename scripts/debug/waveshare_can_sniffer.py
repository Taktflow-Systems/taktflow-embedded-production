#!/usr/bin/env python3
"""
Waveshare USB-CAN-A sniffer — decodes CAN frames from the fixed 20-byte
protocol over serial (2 Mbps, CH340).

Usage:
    python scripts/waveshare_can_sniffer.py [--port COM13] [--duration 10]
"""

import argparse
import serial
import struct
import time
import sys
from collections import defaultdict

# Known CAN IDs from firmware (for display)
CAN_ID_NAMES = {
    # CVC TX
    0x001: "CVC E-Stop",
    0x010: "CVC Heartbeat",
    0x100: "CVC Vehicle State",
    0x101: "CVC Torque Cmd",
    0x102: "CVC Steering Cmd",
    0x103: "CVC Brake Cmd",
    0x350: "CVC Body Ctrl",
    0x500: "DTC (shared)",
    0x7E8: "CVC UDS Response",
    # FZC TX
    0x011: "FZC Heartbeat",
    0x200: "FZC Steering Status",
    0x201: "FZC Brake Status",
    0x210: "FZC Brake Fault",
    0x211: "FZC Motor Cutoff",
    0x220: "FZC Lidar",
    # RZC TX
    0x012: "RZC Heartbeat",
    0x300: "RZC Motor Status",
    0x301: "RZC Motor Current",
    0x302: "RZC Motor Temp",
    0x303: "RZC Battery",
    # UDS
    0x7DF: "UDS Broadcast",
    0x7E0: "UDS Request",
}

FRAME_SIZE = 20


def parse_fixed_frame(frame_bytes):
    """Parse a fixed 20-byte Waveshare frame.

    Returns dict with keys: can_id, dlc, data, frame_type, frame_format,
    or None if invalid.
    """
    if len(frame_bytes) != FRAME_SIZE:
        return None
    if frame_bytes[0] != 0xAA or frame_bytes[1] != 0x55:
        return None

    type_byte = frame_bytes[2]  # 0x01 = data
    frame_type = frame_bytes[3]  # 0x01=std, 0x02=ext
    frame_format = frame_bytes[4]  # 0x01=data, 0x02=remote

    # CAN ID: bytes 5-8, little-endian
    can_id = struct.unpack_from("<I", frame_bytes, 5)[0]
    if frame_type == 0x01:  # standard: mask to 11 bits
        can_id &= 0x7FF
    else:  # extended: mask to 29 bits
        can_id &= 0x1FFFFFFF

    dlc = frame_bytes[9]
    if dlc > 8:
        dlc = 8

    data = frame_bytes[10:10 + dlc]

    # Verify checksum: sum of bytes[2:19] & 0xFF
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
    """Parse a variable-length Waveshare frame starting at buf[0].

    Returns (frame_dict, consumed_bytes) or (None, 1) to skip.
    """
    if len(buf) < 2 or buf[0] != 0xAA:
        return None, 1

    type_byte = buf[1]
    if type_byte < 0xC0:
        return None, 1  # not a valid type byte

    is_ext = bool(type_byte & 0x20)
    is_remote = bool(type_byte & 0x10)
    dlc = type_byte & 0x0F
    if dlc > 8:
        return None, 1

    id_len = 4 if is_ext else 2
    frame_len = 1 + 1 + id_len + dlc + 1  # header + type + id + data + tail

    if len(buf) < frame_len:
        return None, 0  # need more data

    if is_ext:
        can_id = struct.unpack_from("<I", buf, 2)[0] & 0x1FFFFFFF
    else:
        can_id = struct.unpack_from("<H", buf, 2)[0] & 0x7FF

    data_start = 2 + id_len
    data = bytes(buf[data_start:data_start + dlc])

    tail = buf[data_start + dlc]
    if tail != 0x55:
        return None, 1  # bad tail

    return {
        "can_id": can_id,
        "dlc": dlc,
        "data": data,
        "frame_type": "EXT" if is_ext else "STD",
        "frame_format": "RTR" if is_remote else "DATA",
        "checksum_ok": True,
    }, frame_len


def format_frame(frame, timestamp):
    """Format a parsed frame for display."""
    can_id = frame["can_id"]
    name = CAN_ID_NAMES.get(can_id, "???")
    data_hex = " ".join(f"{b:02X}" for b in frame["data"])
    cksum = "OK" if frame["checksum_ok"] else "BAD"

    return (
        f"[{timestamp:8.3f}s] "
        f"0x{can_id:03X} {name:<22s} "
        f"[{frame['dlc']}] {data_hex:<24s} "
        f"{frame['frame_type']} {cksum}"
    )


def detect_protocol(ser, timeout=2.0):
    """Read initial bytes to detect fixed (AA 55) vs variable (AA Cx) protocol."""
    start = time.time()
    buf = bytearray()
    while time.time() - start < timeout:
        chunk = ser.read(max(1, ser.in_waiting))
        if chunk:
            buf.extend(chunk)
        if len(buf) >= 4:
            # Scan for first AA
            for i in range(len(buf) - 1):
                if buf[i] == 0xAA:
                    if buf[i + 1] == 0x55:
                        return "fixed", buf[i:]
                    elif buf[i + 1] >= 0xC0:
                        return "variable", buf[i:]
            # Keep only last byte (might be start of frame)
            buf = buf[-1:]
    return "unknown", buf


def run_sniffer(port, duration, quiet=False):
    """Main sniffer loop."""
    print(f"Opening {port} at 2000000 baud...")
    try:
        ser = serial.Serial(port, 2000000, timeout=0.1)
    except serial.SerialException as e:
        print(f"ERROR: Cannot open {port}: {e}")
        print("Make sure the Waveshare tool is closed!")
        sys.exit(1)

    print("Detecting protocol mode...")
    proto, initial_buf = detect_protocol(ser)
    print(f"Protocol: {proto}")

    if proto == "unknown":
        print("Could not detect protocol. Is the adapter sending data?")
        ser.close()
        sys.exit(1)

    print(f"\nCapturing for {duration}s... Press Ctrl+C to stop.\n")
    print(
        f"{'Time':>10s}  {'CAN ID':>7s}  {'Name':<22s}  "
        f"{'DLC':>3s}  {'Data':<24s}  {'Type':>4s}"
    )
    print("-" * 82)

    stats = defaultdict(int)
    frame_count = 0
    error_count = 0
    t0 = time.time()
    buf = bytearray(initial_buf)

    try:
        while True:
            elapsed = time.time() - t0
            if elapsed > duration:
                break

            chunk = ser.read(max(1, ser.in_waiting))
            if chunk:
                buf.extend(chunk)

            if proto == "fixed":
                # Process all complete 20-byte frames
                while len(buf) >= FRAME_SIZE:
                    # Find AA 55 header
                    idx = -1
                    for i in range(len(buf) - 1):
                        if buf[i] == 0xAA and buf[i + 1] == 0x55:
                            idx = i
                            break
                    if idx < 0:
                        buf = buf[-1:]  # keep last byte
                        break
                    if idx > 0:
                        buf = buf[idx:]  # skip garbage before header
                    if len(buf) < FRAME_SIZE:
                        break

                    frame = parse_fixed_frame(buf[:FRAME_SIZE])
                    buf = buf[FRAME_SIZE:]

                    if frame:
                        frame_count += 1
                        stats[frame["can_id"]] += 1
                        if not quiet:
                            ts = time.time() - t0
                            print(format_frame(frame, ts))
                    else:
                        error_count += 1

            else:  # variable
                while len(buf) >= 4:
                    if buf[0] != 0xAA:
                        buf.pop(0)
                        continue
                    frame, consumed = parse_variable_frame(buf)
                    if consumed == 0:
                        break  # need more data
                    if frame:
                        frame_count += 1
                        stats[frame["can_id"]] += 1
                        if not quiet:
                            ts = time.time() - t0
                            print(format_frame(frame, ts))
                    else:
                        error_count += 1
                    buf = buf[consumed:]

    except KeyboardInterrupt:
        print("\n\nStopped by user.")

    ser.close()
    elapsed = time.time() - t0

    # Summary
    print(f"\n{'='*82}")
    print(f"Captured {frame_count} frames in {elapsed:.1f}s "
          f"({frame_count/max(elapsed,0.001):.0f} frames/sec), "
          f"{error_count} errors")
    print(f"\n{'CAN ID':>7s}  {'Name':<22s}  {'Count':>6s}  {'Rate':>8s}")
    print("-" * 50)
    for can_id in sorted(stats.keys()):
        name = CAN_ID_NAMES.get(can_id, "???")
        count = stats[can_id]
        rate = count / max(elapsed, 0.001)
        print(f"0x{can_id:03X}    {name:<22s}  {count:6d}  {rate:7.1f}/s")


def run_live(port):
    """Live dashboard mode — shows latest value per CAN ID, updates in-place."""
    import os

    print(f"Opening {port} at 2000000 baud...")
    try:
        ser = serial.Serial(port, 2000000, timeout=0.1)
    except serial.SerialException as e:
        print(f"ERROR: Cannot open {port}: {e}")
        print("Make sure the Waveshare tool is closed!")
        sys.exit(1)

    print("Detecting protocol mode...")
    proto, initial_buf = detect_protocol(ser)
    if proto == "unknown":
        print("Could not detect protocol. Is the adapter sending data?")
        ser.close()
        sys.exit(1)

    # State: latest frame per CAN ID
    latest = {}  # can_id -> {frame, timestamp, count}
    t0 = time.time()
    buf = bytearray(initial_buf)
    total_frames = 0
    last_redraw = 0

    try:
        while True:
            chunk = ser.read(max(1, ser.in_waiting))
            if chunk:
                buf.extend(chunk)

            now = time.time()
            elapsed = now - t0

            if proto == "fixed":
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
                    frame = parse_fixed_frame(buf[:FRAME_SIZE])
                    buf = buf[FRAME_SIZE:]
                    if frame:
                        total_frames += 1
                        cid = frame["can_id"]
                        if cid not in latest:
                            latest[cid] = {"count": 0}
                        latest[cid]["frame"] = frame
                        latest[cid]["timestamp"] = elapsed
                        latest[cid]["count"] += 1
            else:
                while len(buf) >= 4:
                    if buf[0] != 0xAA:
                        buf.pop(0)
                        continue
                    frame, consumed = parse_variable_frame(buf)
                    if consumed == 0:
                        break
                    if frame:
                        total_frames += 1
                        cid = frame["can_id"]
                        if cid not in latest:
                            latest[cid] = {"count": 0}
                        latest[cid]["frame"] = frame
                        latest[cid]["timestamp"] = elapsed
                        latest[cid]["count"] += 1
                    buf = buf[consumed:]

            # Redraw at ~4 Hz
            if now - last_redraw < 0.25:
                continue
            last_redraw = now

            # Clear screen and draw table
            os.system("cls" if os.name == "nt" else "clear")
            fps = total_frames / max(elapsed, 0.001)
            print(f"  TAKTFLOW CAN BUS MONITOR  |  {elapsed:.0f}s  |  "
                  f"{total_frames} frames  |  {fps:.0f} fps  |  "
                  f"Ctrl+C to stop")
            print()
            print(f"  {'CAN ID':>7s}  {'Name':<22s}  "
                  f"{'Data':<24s}  {'Count':>7s}  {'Rate':>7s}  "
                  f"{'Age':>5s}")
            print(f"  {'-'*7}  {'-'*22}  {'-'*24}  {'-'*7}  {'-'*7}  "
                  f"{'-'*5}")

            for cid in sorted(latest.keys()):
                entry = latest[cid]
                frame = entry["frame"]
                name = CAN_ID_NAMES.get(cid, "???")
                data_hex = " ".join(f"{b:02X}" for b in frame["data"])
                count = entry["count"]
                rate = count / max(elapsed, 0.001)
                age = elapsed - entry["timestamp"]
                age_str = f"{age:.1f}s" if age < 10 else f"{age:.0f}s"

                # Color-code by ECU (using ANSI where supported)
                if cid <= 0x0FF:
                    ecu_tag = "CVC"
                elif cid <= 0x1FF:
                    ecu_tag = "FZC"
                elif cid <= 0x2FF:
                    ecu_tag = "RZC"
                else:
                    ecu_tag = "   "

                print(f"  0x{cid:03X}    {name:<22s}  "
                      f"{data_hex:<24s}  {count:7d}  {rate:6.1f}/s  "
                      f"{age_str:>5s}")

            # ECU health summary
            print()
            for ecu, hb_id in [("CVC", 0x010), ("FZC", 0x011),
                                ("RZC", 0x012)]:
                if hb_id in latest:
                    age = elapsed - latest[hb_id]["timestamp"]
                    status = "OK" if age < 0.2 else "STALE" if age < 1.0 else "DEAD"
                    hb_data = latest[hb_id]["frame"]["data"]
                    alive = hb_data[0] if len(hb_data) > 0 else 0
                    vsm = hb_data[1] if len(hb_data) > 1 else 0
                    vsm_names = {0: "INIT", 1: "INIT", 2: "RUN",
                                 3: "SAFE_STOP", 4: "FAULT"}
                    vsm_str = vsm_names.get(vsm, f"0x{vsm:02X}")
                    print(f"  {ecu}: {status:>5s}  "
                          f"alive={alive}  vsm={vsm_str}")
                else:
                    print(f"  {ecu}: --  (no heartbeat)")

    except KeyboardInterrupt:
        pass

    ser.close()
    print("\n\nStopped.")


def main():
    parser = argparse.ArgumentParser(
        description="Waveshare USB-CAN-A CAN sniffer"
    )
    parser.add_argument("--port", default="COM13",
                        help="Serial port (default: COM13)")
    parser.add_argument("--duration", type=float, default=10.0,
                        help="Capture duration in seconds (default: 10)")
    parser.add_argument("--quiet", action="store_true",
                        help="Only show summary, no per-frame output")
    parser.add_argument("--live", action="store_true",
                        help="Live dashboard mode (updates in-place)")
    args = parser.parse_args()

    if args.live:
        run_live(args.port)
    else:
        run_sniffer(args.port, args.duration, args.quiet)


if __name__ == "__main__":
    main()
