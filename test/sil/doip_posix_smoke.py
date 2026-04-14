#!/usr/bin/env python3
"""Phase 1 DoIP POSIX smoke for a Docker-hosted virtual ECU."""

from __future__ import annotations

import argparse
import socket
import subprocess
import sys
import time
from pathlib import Path


DOIP_VERSION = 0x02
DOIP_VERSION_INV = 0xFD
PAYLOAD_VAM = 0x0004
PAYLOAD_ROUTING_REQ = 0x0005
PAYLOAD_ROUTING_RSP = 0x0006
PAYLOAD_ALIVE_REQ = 0x0007
PAYLOAD_ALIVE_RSP = 0x0008
PAYLOAD_DIAG_MSG = 0x8001
PAYLOAD_DIAG_ACK = 0x8002


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compose", default="docker/docker-compose.laptop.yml")
    parser.add_argument("--service", default="bcm")
    parser.add_argument("--host", default="127.0.0.5")
    parser.add_argument("--port", type=int, default=13400)
    parser.add_argument("--logical-address", type=parse_int, default=0x0005)
    parser.add_argument("--tester-address", type=parse_int, default=0x0E80)
    parser.add_argument("--expected-id", default="BCM1")
    parser.add_argument("--startup-timeout", type=float, default=45.0)
    parser.add_argument("--socket-timeout", type=float, default=3.0)
    parser.add_argument("--keep", action="store_true")
    return parser.parse_args()


def parse_int(value: str) -> int:
    return int(value, 0)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def compose_cmd(compose_file: str, *extra: str) -> list[str]:
    return ["docker", "compose", "-f", compose_file, *extra]


def run_compose(compose_file: str, *extra: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        compose_cmd(compose_file, *extra),
        cwd=repo_root(),
        check=check,
        capture_output=True,
        text=True,
    )


def build_frame(payload_type: int, payload: bytes) -> bytes:
    return (
        bytes([DOIP_VERSION, DOIP_VERSION_INV])
        + payload_type.to_bytes(2, "big")
        + len(payload).to_bytes(4, "big")
        + payload
    )


def parse_frame(raw: bytes) -> tuple[int, bytes]:
    if len(raw) < 8:
        raise RuntimeError(f"short DoIP frame: {len(raw)} bytes")
    if raw[0] != DOIP_VERSION or raw[1] != DOIP_VERSION_INV:
        raise RuntimeError(f"bad DoIP version bytes: {raw[0]:02X} {raw[1]:02X}")
    payload_type = int.from_bytes(raw[2:4], "big")
    payload_length = int.from_bytes(raw[4:8], "big")
    if len(raw) != 8 + payload_length:
        raise RuntimeError(
            f"frame length mismatch: header says {payload_length}, got {len(raw) - 8}"
        )
    return payload_type, raw[8:]


class DoipTcpClient:
    def __init__(self, host: str, port: int, timeout: float) -> None:
        self._sock = socket.create_connection((host, port), timeout=timeout)
        self._sock.settimeout(timeout)
        self._buffer = bytearray()

    def close(self) -> None:
        self._sock.close()

    def send_frame(self, payload_type: int, payload: bytes) -> None:
        self._sock.sendall(build_frame(payload_type, payload))

    def recv_frame(self) -> tuple[int, bytes]:
        while len(self._buffer) < 8:
            self._read_more()

        payload_length = int.from_bytes(self._buffer[4:8], "big")
        frame_length = 8 + payload_length

        while len(self._buffer) < frame_length:
            self._read_more()

        raw = bytes(self._buffer[:frame_length])
        del self._buffer[:frame_length]
        return parse_frame(raw)

    def _read_more(self) -> None:
        chunk = self._sock.recv(4096)
        if not chunk:
            raise RuntimeError("DoIP TCP socket closed before full frame arrived")
        self._buffer.extend(chunk)


def wait_for_port(host: str, port: int, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((host, port), timeout=1.0):
                return
        except OSError:
            time.sleep(0.5)
    raise TimeoutError(f"timed out waiting for {host}:{port}")


def verify_udp_vehicle_ident(host: str, port: int, logical_address: int, timeout: float) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_sock:
        udp_sock.settimeout(timeout)
        udp_sock.sendto(build_frame(0x0001, b""), (host, port))
        raw, _ = udp_sock.recvfrom(512)

    payload_type, payload = parse_frame(raw)
    if payload_type != PAYLOAD_VAM:
        raise RuntimeError(f"expected VAM 0x0004, got 0x{payload_type:04X}")
    if len(payload) != 32:
        raise RuntimeError(f"unexpected VAM payload length: {len(payload)}")

    actual_address = int.from_bytes(payload[17:19], "big")
    if actual_address != logical_address:
        raise RuntimeError(
            f"unexpected logical address in VAM: 0x{actual_address:04X}"
        )


def verify_routing_activation(
    client: DoipTcpClient,
    tester_address: int,
    logical_address: int,
) -> None:
    payload = tester_address.to_bytes(2, "big") + bytes([0x00, 0x00, 0x00, 0x00, 0x00])
    client.send_frame(PAYLOAD_ROUTING_REQ, payload)
    payload_type, response = client.recv_frame()

    if payload_type != PAYLOAD_ROUTING_RSP:
        raise RuntimeError(f"expected routing response, got 0x{payload_type:04X}")
    if len(response) != 9:
        raise RuntimeError(f"unexpected routing response length: {len(response)}")
    if int.from_bytes(response[0:2], "big") != tester_address:
        raise RuntimeError("routing response echoed the wrong tester address")
    if int.from_bytes(response[2:4], "big") != logical_address:
        raise RuntimeError("routing response carried the wrong ECU address")
    if response[4] != 0x10:
        raise RuntimeError(f"routing activation failed with code 0x{response[4]:02X}")


def verify_alive_check(client: DoipTcpClient, logical_address: int) -> None:
    client.send_frame(PAYLOAD_ALIVE_REQ, b"")
    payload_type, response = client.recv_frame()
    if payload_type != PAYLOAD_ALIVE_RSP:
        raise RuntimeError(f"expected alive response, got 0x{payload_type:04X}")
    if len(response) != 2:
        raise RuntimeError(f"unexpected alive response length: {len(response)}")
    if int.from_bytes(response[0:2], "big") != logical_address:
        raise RuntimeError("alive response carried the wrong ECU address")


def verify_read_did(
    client: DoipTcpClient,
    tester_address: int,
    logical_address: int,
    expected_id: str,
) -> None:
    uds_request = bytes([0x22, 0xF1, 0x90])
    diag_payload = (
        tester_address.to_bytes(2, "big")
        + logical_address.to_bytes(2, "big")
        + uds_request
    )

    client.send_frame(PAYLOAD_DIAG_MSG, diag_payload)

    ack_type, ack_payload = client.recv_frame()
    if ack_type != PAYLOAD_DIAG_ACK:
        raise RuntimeError(f"expected diagnostic ACK, got 0x{ack_type:04X}")
    if int.from_bytes(ack_payload[0:2], "big") != logical_address:
        raise RuntimeError("diagnostic ACK carried the wrong ECU address")
    if int.from_bytes(ack_payload[2:4], "big") != tester_address:
        raise RuntimeError("diagnostic ACK carried the wrong tester address")
    if ack_payload[4] != 0x00:
        raise RuntimeError(f"diagnostic ACK code was 0x{ack_payload[4]:02X}")
    if ack_payload[5:] != uds_request:
        raise RuntimeError("diagnostic ACK did not echo the original UDS payload")

    rsp_type, rsp_payload = client.recv_frame()
    if rsp_type != PAYLOAD_DIAG_MSG:
        raise RuntimeError(f"expected diagnostic response, got 0x{rsp_type:04X}")
    if int.from_bytes(rsp_payload[0:2], "big") != logical_address:
        raise RuntimeError("diagnostic response carried the wrong ECU address")
    if int.from_bytes(rsp_payload[2:4], "big") != tester_address:
        raise RuntimeError("diagnostic response carried the wrong tester address")

    expected_uds = bytes([0x62, 0xF1, 0x90]) + expected_id.encode("ascii")
    if rsp_payload[4:] != expected_uds:
        raise RuntimeError(
            f"unexpected UDS response: {rsp_payload[4:].hex()} != {expected_uds.hex()}"
        )


def main() -> int:
    args = parse_args()
    started_service = False

    try:
        run_compose(args.compose, "up", "-d", "--build", args.service)
        started_service = True
        wait_for_port(args.host, args.port, args.startup_timeout)
        verify_udp_vehicle_ident(
            args.host, args.port, args.logical_address, args.socket_timeout
        )

        client = DoipTcpClient(args.host, args.port, args.socket_timeout)
        try:
            verify_routing_activation(
                client, args.tester_address, args.logical_address
            )
            verify_alive_check(client, args.logical_address)
            verify_read_did(
                client,
                args.tester_address,
                args.logical_address,
                args.expected_id,
            )
        finally:
            client.close()

        print("DOIP_SMOKE_PASS")
        print(
            f"endpoint={args.host}:{args.port} logical_address=0x{args.logical_address:04X}"
        )
        return 0
    except Exception as exc:  # pylint: disable=broad-except
        print(f"DOIP_SMOKE_FAIL: {exc}", file=sys.stderr)
        if started_service:
            logs = run_compose(args.compose, "logs", "--no-color", args.service, check=False)
            if logs.stdout:
                print(logs.stdout, file=sys.stderr)
            if logs.stderr:
                print(logs.stderr, file=sys.stderr)
        return 1
    finally:
        if started_service and not args.keep:
            run_compose(args.compose, "stop", args.service, check=False)


if __name__ == "__main__":
    sys.exit(main())
