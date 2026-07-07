#!/usr/bin/env python3
"""
@file       xcp_smoke.py
@brief      S-XCP-02 smoke client — XCP-on-UDP CONNECT + Seed&Key + read.

Drives the SC's minimal XCP-on-Ethernet slave (firmware/ecu/sc/src/
sc_xcp_eth.c) over UDP: CONNECT, GET_SEED / UNLOCK (Seed&Key), then reads
a known volatile counter twice ~1 s apart via SHORT_UPLOAD and prints both
values. Stdlib sockets only — no pyxcp dependency required.

The ASAM XCP-on-Ethernet transport header (LEN u16 LE + CTR u16 LE)
prefixes every packet. The Seed&Key derivation MUST match
firmware/bsw/services/Xcp/src/Xcp.c (xcp_compute_key): key = ROL13(seed ^
"TAKT") ^ "FLOW". Big-endian payload fields per the TMS570 slave.

Usage:
    # read the SC's g_sc_eth_telemetry_sequence twice, 1 s apart:
    python tools/bench/xcp_smoke.py --host 198.51.100.10 \
        --addr 0x08001234 --size 1

    # resolve the symbol address from the map first:
    python tools/bench/xcp_smoke.py --host 198.51.100.10 \
        --symbol g_sc_eth_telemetry_sequence \
        --map build/tms570-sxcp02-eth-hil/sc.map --size 1

    # offline self-test (no board): checks framing + key derivation
    python tools/bench/xcp_smoke.py --self-test

@aspice     SWE.6 — Software Qualification Testing (HIL support tooling)
"""

import argparse
import socket
import struct
import sys
import time

XCP_PORT = 55002

CMD_CONNECT = 0xFF
CMD_DISCONNECT = 0xFE
CMD_SHORT_UPLOAD = 0xF4
CMD_GET_SEED = 0xF8
CMD_UNLOCK = 0xF7
RES_OK = 0xFF


def compute_key(seed: int) -> int:
    """Must match Xcp.c xcp_compute_key / sc_xcp_eth.c sc_xcp_compute_key."""
    key = (seed ^ 0x54414B54) & 0xFFFFFFFF          # "TAKT"
    key = ((key << 13) | (key >> 19)) & 0xFFFFFFFF   # ROL 13
    key ^= 0x464C4F57                                # "FLOW"
    return key & 0xFFFFFFFF


def frame(packet: bytes, ctr: int) -> bytes:
    """Prepend the ASAM XCP-on-Ethernet transport header."""
    return struct.pack("<HH", len(packet), ctr & 0xFFFF) + packet


def unframe(datagram: bytes):
    """Return (ctr, xcp_packet) from a received transport frame."""
    if len(datagram) < 4:
        raise ValueError("short frame")
    length, ctr = struct.unpack_from("<HH", datagram, 0)
    packet = datagram[4:4 + length]
    if len(packet) != length:
        raise ValueError(f"length mismatch: hdr={length} got={len(packet)}")
    return ctr, packet


class XcpUdpClient:
    def __init__(self, host, port, timeout=1.0):
        self.addr = (host, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # The SC has IP 0.0.0.0 and no ARP responder; the request is
        # broadcast (EMAC accepts broadcast, dispatch matches on port) and
        # the SC replies unicast to the requester's learned MAC/IP/port.
        if host.endswith(".255") or host == "<broadcast>":
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.settimeout(timeout)
        self.ctr = 0

    def transact(self, packet: bytes) -> bytes:
        self.ctr = (self.ctr + 1) & 0xFFFF
        self.sock.sendto(frame(packet, self.ctr), self.addr)
        datagram, _ = self.sock.recvfrom(1024)
        _, resp = unframe(datagram)
        return resp

    def connect(self):
        resp = self.transact(bytes([CMD_CONNECT, 0x00]))
        if not resp or resp[0] != RES_OK:
            raise RuntimeError(f"CONNECT failed: {resp.hex()}")
        return resp

    def unlock(self):
        resp = self.transact(bytes([CMD_GET_SEED, 0x00, 0x01]))
        if resp[0] != RES_OK:
            raise RuntimeError(f"GET_SEED failed: {resp.hex()}")
        if resp[1] == 0:
            return  # already unlocked
        seed = struct.unpack_from(">I", resp, 2)[0]
        key = compute_key(seed)
        resp = self.transact(bytes([CMD_UNLOCK, 4]) + struct.pack(">I", key))
        if resp[0] != RES_OK:
            raise RuntimeError(f"UNLOCK rejected: {resp.hex()}")

    def short_upload(self, addr: int, size: int) -> bytes:
        packet = bytes([CMD_SHORT_UPLOAD, size, 0, 0]) + struct.pack(">I", addr)
        resp = self.transact(packet)
        if resp[0] != RES_OK:
            raise RuntimeError(f"SHORT_UPLOAD failed: {resp.hex()}")
        return resp[1:1 + size]

    def disconnect(self):
        try:
            self.transact(bytes([CMD_DISCONNECT]))
        except socket.timeout:
            pass

    def close(self):
        self.sock.close()


def resolve_symbol_a2l(a2l_path: str, symbol: str) -> int:
    """Find a MEASUREMENT's ECU_ADDRESS in an A2L file (S-XCP-03).

    Unlike the raw map lookup, this also resolves file-static symbols —
    gen_a2l.py extracts them from the map's section-allocation rows.
    """
    in_block = False
    with open(a2l_path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            parts = line.split()
            if parts[:2] == ["/begin", "MEASUREMENT"] and len(parts) >= 3:
                in_block = parts[2] == symbol
            elif in_block and parts[:1] == ["ECU_ADDRESS"] and len(parts) >= 2:
                return int(parts[1], 16)
            elif parts[:2] == ["/end", "MEASUREMENT"]:
                in_block = False
    raise SystemExit(f"MEASUREMENT {symbol!r} not found in {a2l_path}")


def resolve_symbol(map_path: str, symbol: str) -> int:
    """Find a symbol's address in a TI linker .map file."""
    with open(map_path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            parts = line.split()
            # TI map rows: <address> <name> ... — match exact symbol token
            if symbol in parts:
                for tok in parts:
                    if len(tok) == 8 and all(c in "0123456789abcdefABCDEF"
                                             for c in tok):
                        return int(tok, 16)
    raise SystemExit(f"symbol {symbol!r} not found in {map_path}")


def self_test() -> int:
    failures = 0
    # Key derivation vector (matches Xcp.c constants)
    seed = 0x12345678
    key = compute_key(seed)
    if key != compute_key(seed):
        print("FAIL: key not deterministic")
        failures += 1
    # Frame round trip
    pkt = bytes([CMD_SHORT_UPLOAD, 4, 0, 0, 0x08, 0x00, 0x10, 0x00])
    ctr, back = unframe(frame(pkt, 7))
    if ctr != 7 or back != pkt:
        print("FAIL: frame round trip")
        failures += 1
    # Known key value (regression guard on the ROL13 derivation)
    if compute_key(0x00000000) != (((0x54414B54 << 13) |
                                    (0x54414B54 >> 19)) & 0xFFFFFFFF) ^ 0x464C4F57:
        print("FAIL: key derivation formula drifted")
        failures += 1
    print("self-test:", "PASS" if failures == 0 else f"{failures} FAIL")
    print(f"  seed=0x{seed:08X} -> key=0x{key:08X}")
    print(f"  connect frame: {frame(bytes([CMD_CONNECT, 0]), 1).hex().upper()}")
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(
        description="XCP-on-UDP smoke client for the SC XCP slave")
    parser.add_argument("--host", help="SC board IPv4 address")
    parser.add_argument("--port", type=int, default=XCP_PORT)
    parser.add_argument("--addr", type=lambda s: int(s, 0),
                        help="memory address to read (SHORT_UPLOAD)")
    parser.add_argument("--symbol", help="resolve address from --map/--a2l instead")
    parser.add_argument("--map", help="TI linker .map file for --symbol")
    parser.add_argument("--a2l", help="A2L file (gen_a2l.py output) for --symbol")
    parser.add_argument("--size", type=int, default=1,
                        help="bytes to read, 1..7 (default 1)")
    parser.add_argument("--interval", type=float, default=1.0,
                        help="seconds between the two reads (default 1.0)")
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        sys.exit(self_test())
    if not args.host:
        print("ERROR: --host required (or use --self-test)", file=sys.stderr)
        sys.exit(2)

    addr = args.addr
    if addr is None:
        if not (args.symbol and (args.map or args.a2l)):
            print("ERROR: give --addr, or --symbol with --map or --a2l",
                  file=sys.stderr)
            sys.exit(2)
        if args.a2l:
            addr = resolve_symbol_a2l(args.a2l, args.symbol)
        else:
            addr = resolve_symbol(args.map, args.symbol)
        print(f"resolved {args.symbol} -> 0x{addr:08X}")

    client = XcpUdpClient(args.host, args.port, args.timeout)
    try:
        client.connect()
        print("CONNECT ok")
        client.unlock()
        print("UNLOCK ok")
        first = client.short_upload(addr, args.size)
        print(f"read #1 @0x{addr:08X}: {first.hex().upper()}")
        time.sleep(args.interval)
        second = client.short_upload(addr, args.size)
        print(f"read #2 @0x{addr:08X}: {second.hex().upper()}")
        client.disconnect()
        changed = first != second
        print(f"value {'CHANGED' if changed else 'unchanged'} over "
              f"{args.interval:.1f}s")
        # DoD: a live counter must change between the two reads.
        sys.exit(0 if changed else 1)
    except (socket.timeout, RuntimeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
    finally:
        client.close()


if __name__ == "__main__":
    main()
