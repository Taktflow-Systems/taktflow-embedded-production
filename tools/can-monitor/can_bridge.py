#!/usr/bin/env python3
"""CAN-to-TCP bridge — run on Linux host with SocketCAN, connect from Windows.

Usage:
    python3 can_bridge.py --channel vcan0 --port 9876

Protocol: each frame = 21 bytes:
    [4B can_id LE][1B dlc][8B data (zero-padded)][8B timestamp_us LE]

Supports multiple concurrent clients. Ctrl+C to stop.
"""
import argparse
import socket
import struct
import threading
import time

import can


def serve_client(conn, addr, bus_channel, stop_event):
    """Stream CAN frames to a single TCP client."""
    print(f"[bridge] Client connected: {addr}")
    bus = can.interface.Bus(interface="socketcan", channel=bus_channel)
    try:
        while not stop_event.is_set():
            msg = bus.recv(timeout=0.05)
            if msg is None:
                continue
            can_id = msg.arbitration_id
            if msg.is_extended_id:
                can_id |= 0x80000000
            frame = struct.pack("<IB", can_id, msg.dlc)
            frame += bytes(msg.data).ljust(8, b'\x00')
            frame += struct.pack("<Q", int(msg.timestamp * 1_000_000))
            try:
                conn.sendall(frame)
            except BrokenPipeError:
                break
            except Exception:
                break
    finally:
        bus.shutdown()
        conn.close()
        print(f"[bridge] Client disconnected: {addr}")


def main():
    parser = argparse.ArgumentParser(description="CAN-to-TCP bridge")
    parser.add_argument("--channel", default="vcan0", help="SocketCAN channel")
    parser.add_argument("--port", type=int, default=9876, help="TCP listen port")
    parser.add_argument("--bind", default="0.0.0.0", help="Bind address")
    args = parser.parse_args()

    stop_event = threading.Event()
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((args.bind, args.port))
    server.listen(5)
    server.settimeout(1.0)

    print(f"[bridge] Listening on {args.bind}:{args.port}, channel={args.channel}")
    print(f"[bridge] Connect from Windows: RemoteCanReader('{args.bind}', {args.port})")

    threads = []
    try:
        while True:
            try:
                conn, addr = server.accept()
                t = threading.Thread(
                    target=serve_client,
                    args=(conn, addr, args.channel, stop_event),
                    daemon=True,
                )
                t.start()
                threads.append(t)
            except socket.timeout:
                continue
    except KeyboardInterrupt:
        print("\n[bridge] Shutting down...")
        stop_event.set()
        for t in threads:
            t.join(timeout=2)
        server.close()


if __name__ == "__main__":
    main()
