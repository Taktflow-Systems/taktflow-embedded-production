#!/usr/bin/env python3
"""SC Ethernet telemetry UDP receiver.

Decodes the S-UDP-01 ``SCET`` payload using only the Python standard library.
The tool intentionally omits sender addresses from logs and CSV output so
local bench network details do not end up in artifacts.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import socket
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


MAGIC = b"SCET"
VERSION = 1
HEADER_LEN = 12
STATUS_LEN = 4
TIMESTAMP_SOURCE_MAIN_LOOP = 1
DEFAULT_PORT = 55001
DEFAULT_DURATION_S = 60.0


class DecodeError(ValueError):
    """Raised when a UDP datagram is not a valid SCET payload."""


@dataclass
class DecodedTelemetry:
    host_time_iso: str
    host_time_s: float
    sequence_mod16: int
    timestamp_source: int
    status_period_ms: int
    status_byte0: int
    status_crc8: int
    status_byte2: int
    status_byte3: int
    alive_counter_mod16: int
    status_data_id: int
    sc_mode: int
    fault_flags: int
    ecu_health: int
    fault_reason: int
    relay_energized: int
    delta_mod16: Optional[int]
    missed_periods: Optional[int]
    crc_ok: bool


def crc8_j1850(data: bytes) -> int:
    crc = 0xFF
    for value in data:
        crc ^= value
        for _ in range(8):
            if (crc & 0x80) != 0:
                crc = ((crc << 1) ^ 0x1D) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc ^ 0xFF


def u16be(data: bytes) -> int:
    return (data[0] << 8) | data[1]


def decode_payload(data: bytes,
                   host_time_s: float,
                   last_sequence: Optional[int]) -> DecodedTelemetry:
    if len(data) != 16:
        raise DecodeError(f"length={len(data)}")
    if data[0:4] != MAGIC:
        raise DecodeError("bad_magic")
    if data[4] != VERSION:
        raise DecodeError(f"unsupported_version={data[4]}")
    if data[5] != HEADER_LEN:
        raise DecodeError(f"bad_header_len={data[5]}")
    if u16be(data[6:8]) != STATUS_LEN:
        raise DecodeError(f"bad_status_len={u16be(data[6:8])}")

    sequence = data[8] & 0x0F
    timestamp_source = data[9]
    status_period_ms = u16be(data[10:12])
    status0 = data[12]
    status_crc = data[13]
    status2 = data[14]
    status3 = data[15]

    alive = (status0 >> 4) & 0x0F
    data_id = status0 & 0x0F
    if alive != sequence:
        raise DecodeError(f"sequence_alive_mismatch={sequence}:{alive}")

    expected_crc = crc8_j1850(bytes((status2, status3, data_id)))
    crc_ok = expected_crc == status_crc
    if not crc_ok:
        raise DecodeError(f"bad_crc=0x{status_crc:02X}/0x{expected_crc:02X}")

    delta = None
    missed = None
    if last_sequence is not None:
        delta = (sequence - last_sequence) & 0x0F
        missed = 0 if delta in (0, 1) else delta - 1

    host_dt = dt.datetime.fromtimestamp(host_time_s, tz=dt.timezone.utc)
    return DecodedTelemetry(
        host_time_iso=host_dt.isoformat(timespec="milliseconds"),
        host_time_s=host_time_s,
        sequence_mod16=sequence,
        timestamp_source=timestamp_source,
        status_period_ms=status_period_ms,
        status_byte0=status0,
        status_crc8=status_crc,
        status_byte2=status2,
        status_byte3=status3,
        alive_counter_mod16=alive,
        status_data_id=data_id,
        sc_mode=status2 & 0x0F,
        fault_flags=(status2 >> 4) & 0x0F,
        ecu_health=status3 & 0x07,
        fault_reason=(status3 >> 3) & 0x0F,
        relay_energized=(status3 >> 7) & 0x01,
        delta_mod16=delta,
        missed_periods=missed,
        crc_ok=crc_ok,
    )


def telemetry_row(decoded: DecodedTelemetry) -> dict[str, object]:
    return {
        "host_time_iso": decoded.host_time_iso,
        "host_time_s": f"{decoded.host_time_s:.6f}",
        "sequence_mod16": decoded.sequence_mod16,
        "timestamp_source": decoded.timestamp_source,
        "status_period_ms": decoded.status_period_ms,
        "status_byte0": f"0x{decoded.status_byte0:02X}",
        "status_crc8": f"0x{decoded.status_crc8:02X}",
        "status_byte2": f"0x{decoded.status_byte2:02X}",
        "status_byte3": f"0x{decoded.status_byte3:02X}",
        "alive_counter_mod16": decoded.alive_counter_mod16,
        "status_data_id": decoded.status_data_id,
        "sc_mode": decoded.sc_mode,
        "fault_flags": decoded.fault_flags,
        "ecu_health": decoded.ecu_health,
        "fault_reason": decoded.fault_reason,
        "relay_energized": decoded.relay_energized,
        "delta_mod16": "" if decoded.delta_mod16 is None else decoded.delta_mod16,
        "missed_periods": "" if decoded.missed_periods is None else decoded.missed_periods,
        "crc_ok": int(decoded.crc_ok),
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Decode SC Ethernet SCET UDP telemetry into CSV."
    )
    parser.add_argument("--bind", default="0.0.0.0",
                        help="local address to bind, default all interfaces")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                        help=f"UDP port to listen on, default {DEFAULT_PORT}")
    parser.add_argument("--duration", type=float, default=DEFAULT_DURATION_S,
                        help=f"capture duration in seconds, default {DEFAULT_DURATION_S:g}")
    parser.add_argument("--csv", default="tmp/sc_eth_telemetry_capture.csv",
                        help="CSV output path, default tmp/sc_eth_telemetry_capture.csv")
    parser.add_argument("--socket-timeout", type=float, default=1.0,
                        help="socket timeout in seconds")
    parser.add_argument("--print-every", type=float, default=1.0,
                        help="live summary print period in seconds")
    parser.add_argument("--fail-on-gap", action="store_true",
                        help="return non-zero when sequence gaps are observed")
    parser.add_argument("--expect-rate-hz", type=float, default=None,
                        help="optional expected receive rate")
    parser.add_argument("--rate-tolerance", type=float, default=0.01,
                        help="fractional rate tolerance when --expect-rate-hz is set")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    csv_path = Path(args.csv)
    csv_path.parent.mkdir(parents=True, exist_ok=True)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(args.socket_timeout)
    sock.bind((args.bind, args.port))

    fieldnames = list(telemetry_row(DecodedTelemetry(
        host_time_iso="",
        host_time_s=0.0,
        sequence_mod16=0,
        timestamp_source=0,
        status_period_ms=0,
        status_byte0=0,
        status_crc8=0,
        status_byte2=0,
        status_byte3=0,
        alive_counter_mod16=0,
        status_data_id=0,
        sc_mode=0,
        fault_flags=0,
        ecu_health=0,
        fault_reason=0,
        relay_energized=0,
        delta_mod16=None,
        missed_periods=None,
        crc_ok=True,
    )).keys())

    start = time.time()
    last_print = start
    first_valid: Optional[float] = None
    last_valid: Optional[float] = None
    last_sequence: Optional[int] = None
    valid_count = 0
    invalid_count = 0
    gap_count = 0
    missed_total = 0

    print(
        f"listening port={args.port} duration_s={args.duration:g} csv={csv_path}",
        flush=True,
    )

    with csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()

        while (time.time() - start) < args.duration:
            try:
                data, _addr = sock.recvfrom(2048)
            except socket.timeout:
                continue

            now = time.time()
            try:
                decoded = decode_payload(data, now, last_sequence)
            except DecodeError as exc:
                invalid_count += 1
                if invalid_count <= 5:
                    print(f"invalid={invalid_count} reason={exc}", flush=True)
                continue

            if first_valid is None:
                first_valid = now
            last_valid = now
            valid_count += 1
            if decoded.missed_periods is not None and decoded.missed_periods > 0:
                gap_count += 1
                missed_total += decoded.missed_periods
            last_sequence = decoded.sequence_mod16
            writer.writerow(telemetry_row(decoded))

            if (now - last_print) >= args.print_every:
                active = (now - first_valid) if first_valid is not None else 0.0
                rate = ((valid_count - 1) / active) if active > 0.0 else 0.0
                print(
                    "live "
                    f"valid={valid_count} rate_hz={rate:.3f} "
                    f"gaps={gap_count} missed={missed_total} "
                    f"invalid={invalid_count} seq={decoded.sequence_mod16}",
                    flush=True,
                )
                last_print = now

    active_s = 0.0
    if first_valid is not None and last_valid is not None:
        active_s = max(0.0, last_valid - first_valid)
    rate_hz = ((valid_count - 1) / active_s) if active_s > 0.0 else 0.0

    print(
        "summary "
        f"valid={valid_count} invalid={invalid_count} gaps={gap_count} "
        f"missed={missed_total} active_s={active_s:.3f} rate_hz={rate_hz:.3f} "
        f"csv={csv_path}",
        flush=True,
    )

    if valid_count == 0:
        return 2
    if args.fail_on_gap and gap_count != 0:
        return 3
    if args.expect_rate_hz is not None:
        lower = args.expect_rate_hz * (1.0 - args.rate_tolerance)
        upper = args.expect_rate_hz * (1.0 + args.rate_tolerance)
        if not (lower <= rate_hz <= upper):
            return 4

    return 0


if __name__ == "__main__":
    sys.exit(main())
