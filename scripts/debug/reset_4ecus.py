#!/usr/bin/env python3
"""
Reset all 4 physical ECUs (CVC/FZC/RZC/SC) via debug UART COM ports,
then verify CAN activity on the USB-CAN adapter.

Default CAN checks:
  CVC heartbeat      0x010
  FZC heartbeat      0x011
  RZC heartbeat      0x012
  SC relay status    0x013

Usage:
  python scripts/reset_4ecus.py
  python scripts/reset_4ecus.py --can-port COM13 --verify-seconds 15
  python scripts/reset_4ecus.py --cvc COM3 --fzc COM7 --rzc COM8 --sc COM11
"""

import argparse
import struct
import sys
import threading
import time
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

import serial
import serial.tools.list_ports
from tools.bench.hardware_map import resolve_stlink_probe

FRAME_SIZE = 20

EXPECTED_IDS = {
    "cvc": 0x010,
    "fzc": 0x011,
    "rzc": 0x012,
    "sc": 0x013,
}


def hardware_map_port(logical_ecu: str):
    try:
        row = resolve_stlink_probe(logical_ecu)
    except Exception:
        return None
    port = row.get("com_port")
    if not port:
        return None
    return str(port)


def com_sort_key(port_name: str) -> int:
    try:
        return int(port_name.upper().replace("COM", ""))
    except ValueError:
        return 9999


def parse_fixed_frame(frame_bytes):
    if len(frame_bytes) != FRAME_SIZE:
        return None
    if frame_bytes[0] != 0xAA or frame_bytes[1] != 0x55:
        return None

    frame_type = frame_bytes[3]    # 0x01 std, 0x02 ext
    can_id = struct.unpack_from("<I", frame_bytes, 5)[0]
    can_id &= 0x7FF if frame_type == 0x01 else 0x1FFFFFFF
    dlc = min(frame_bytes[9], 8)
    data = bytes(frame_bytes[10:10 + dlc])
    expected_cksum = sum(frame_bytes[2:19]) & 0xFF
    checksum_ok = (expected_cksum == frame_bytes[19])

    return {"can_id": can_id, "data": data, "checksum_ok": checksum_ok}


def parse_variable_frame(buf):
    if len(buf) < 2 or buf[0] != 0xAA:
        return None, 1

    type_byte = buf[1]
    if type_byte < 0xC0:
        return None, 1

    is_ext = bool(type_byte & 0x20)
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

    tail = buf[2 + id_len + dlc]
    if tail != 0x55:
        return None, 1

    data = bytes(buf[2 + id_len:2 + id_len + dlc])
    return {"can_id": can_id, "data": data, "checksum_ok": True}, frame_len


def detect_protocol(ser, timeout=2.0):
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
                    if buf[i + 1] >= 0xC0:
                        return "variable", buf[i:]
            buf = buf[-1:]
    return "unknown", buf


def find_ports():
    ports = list(serial.tools.list_ports.comports())
    stlink = sorted(
        [p.device for p in ports if "STLink Virtual COM Port" in p.description],
        key=com_sort_key,
    )
    xds_app = sorted(
        [p.device for p in ports if "XDS110 Class Application/User UART" in p.description],
        key=com_sort_key,
    )
    ch340 = sorted(
        [p.device for p in ports if "CH340" in p.description.upper()],
        key=com_sort_key,
    )
    return stlink, xds_app, ch340, ports


def pulse_uart_reset(ecu_name: str, port: str):
    print(f"[reset] {ecu_name.upper():3s} on {port}: toggling DTR/RTS pulse")
    try:
        with serial.Serial(port=port, baudrate=1200, timeout=0.2) as ser:
            ser.dtr = False
            ser.rts = False
            time.sleep(0.10)
            ser.dtr = True
            ser.rts = True
            time.sleep(0.10)
            ser.send_break(duration=0.15)
            ser.dtr = False
            ser.rts = False
        print(f"[reset] {ecu_name.upper():3s}: pulse done")
        return True
    except Exception as exc:
        print(f"[reset] {ecu_name.upper():3s}: FAILED ({exc})")
        return False


def capture_can_ids(can_port: str, duration_s: float):
    id_counts = defaultdict(int)
    start = time.time()
    frame_count = 0
    error_count = 0

    with serial.Serial(can_port, 2000000, timeout=0.1) as ser:
        proto, initial = detect_protocol(ser)
        if proto == "unknown":
            raise RuntimeError("Could not detect Waveshare protocol on CAN adapter")
        buf = bytearray(initial)

        while time.time() - start < duration_s:
            chunk = ser.read(max(1, ser.in_waiting))
            if chunk:
                buf.extend(chunk)

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
                    if frame is None:
                        error_count += 1
                        continue
                    frame_count += 1
                    id_counts[frame["can_id"]] += 1
            else:
                while len(buf) >= 4:
                    if buf[0] != 0xAA:
                        buf.pop(0)
                        continue
                    frame, consumed = parse_variable_frame(buf)
                    if consumed == 0:
                        break
                    if frame is None:
                        error_count += 1
                        buf = buf[consumed:]
                        continue
                    frame_count += 1
                    id_counts[frame["can_id"]] += 1
                    buf = buf[consumed:]

    return id_counts, frame_count, error_count


def main():
    parser = argparse.ArgumentParser(description="Reset 4 ECUs and verify CAN IDs")
    parser.add_argument("--cvc", help="CVC debug UART COM port (example: COM7)")
    parser.add_argument("--fzc", help="FZC debug UART COM port (example: COM3)")
    parser.add_argument("--rzc", help="RZC debug UART COM port (example: COM8)")
    parser.add_argument("--sc", help="SC debug UART COM port (example: COM11)")
    parser.add_argument("--can-port", help="Waveshare CAN adapter COM port (example: COM13)")
    parser.add_argument("--verify-seconds", type=float, default=12.0, help="CAN verification window")
    parser.add_argument("--inter-reset-ms", type=int, default=350, help="Delay between ECU reset pulses")
    args = parser.parse_args()

    stlink, xds_app, ch340, all_ports = find_ports()

    # Auto-map ports if not explicitly provided.
    cvc = args.cvc or hardware_map_port("cvc") or (stlink[0] if len(stlink) >= 1 else None)
    fzc = args.fzc or hardware_map_port("fzc") or (stlink[1] if len(stlink) >= 2 else None)
    rzc = args.rzc or hardware_map_port("rzc") or (stlink[2] if len(stlink) >= 3 else None)
    sc = args.sc or (xds_app[0] if len(xds_app) >= 1 else None)
    can_port = args.can_port or (ch340[0] if len(ch340) >= 1 else "COM13")

    print("Detected COM ports:")
    for p in sorted(all_ports, key=lambda x: com_sort_key(x.device)):
        print(f"  {p.device:5s}  {p.description}")

    print("\nResolved mapping:")
    print(f"  CVC -> {cvc}")
    print(f"  FZC -> {fzc}")
    print(f"  RZC -> {rzc}")
    print(f"  SC  -> {sc}")
    print(f"  CAN -> {can_port}")

    missing = [name for name, val in [("cvc", cvc), ("fzc", fzc), ("rzc", rzc), ("sc", sc)] if not val]
    if missing:
        print(f"\nERROR: Missing COM mapping for: {', '.join(missing)}")
        print("Pass explicit ports: --cvc COMx --fzc COMx --rzc COMx --sc COMx")
        return 1

    # Start CAN listener BEFORE sending reset pulses (listener-first pattern).
    # This prevents missing early heartbeat frames from fast-booting ECUs.
    print(f"\n[verify] opening CAN listener on {can_port} BEFORE reset...")
    capture_result = {}
    capture_error = [None]

    def _capture_thread():
        try:
            id_counts, frame_count, error_count = capture_can_ids(
                can_port, args.verify_seconds
            )
            capture_result["id_counts"] = id_counts
            capture_result["frame_count"] = frame_count
            capture_result["error_count"] = error_count
        except Exception as exc:
            capture_error[0] = exc

    listener = threading.Thread(target=_capture_thread, daemon=True)
    listener.start()
    time.sleep(0.3)  # Let serial port open and protocol detection finish

    ok = True
    for ecu_name, port in [("cvc", cvc), ("fzc", fzc), ("rzc", rzc), ("sc", sc)]:
        if not pulse_uart_reset(ecu_name, port):
            ok = False
        time.sleep(max(args.inter_reset_ms, 0) / 1000.0)

    print(f"[verify] listening on {can_port} for {args.verify_seconds:.1f}s")
    listener.join(timeout=args.verify_seconds + 5)

    if capture_error[0] is not None:
        print(f"[verify] FAILED to read CAN: {capture_error[0]}")
        return 1

    id_counts = capture_result.get("id_counts", {})
    frame_count = capture_result.get("frame_count", 0)
    error_count = capture_result.get("error_count", 0)

    print(f"[verify] frames={frame_count}, errors={error_count}")
    print("[verify] expected IDs:")

    all_seen = True
    for ecu_name in ("cvc", "fzc", "rzc", "sc"):
        can_id = EXPECTED_IDS[ecu_name]
        count = id_counts.get(can_id, 0)
        status = "OK" if count > 0 else "MISSING"
        if count == 0:
            all_seen = False
        print(f"  {ecu_name.upper():3s}  0x{can_id:03X}  count={count:4d}  {status}")

    print("\n[verify] top observed IDs:")
    for can_id, count in sorted(id_counts.items(), key=lambda kv: kv[1], reverse=True)[:12]:
        print(f"  0x{can_id:03X}  count={count}")

    if not ok:
        print("\nRESULT: reset pulse failed on one or more ECU ports.")
        return 2
    if not all_seen:
        print("\nRESULT: reset sent, but not all 4 ECU IDs appeared on CAN.")
        return 2

    print("\nRESULT: reset sent and all 4 ECU IDs observed on CAN.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
