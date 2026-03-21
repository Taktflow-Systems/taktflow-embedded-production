#!/usr/bin/env python3
"""Windows relay: QNX Pi (192.168.137.184) <-> Laptop vcan0 (192.168.0.158)

Bridges two TCP CAN bridges:
- QNX Pi: 192.168.137.184:9877 (UDP multicast CAN -> TCP)
- Laptop: 192.168.0.158:9876 (TCP -> vcan0, bidirectional)

Run on Windows (which has access to both subnets).
QNX frames appear on laptop vcan0 -> Linux ECUs (BCM/TCU/ICU) see them.
"""
import socket
import threading
import sys
import time

QNX_HOST = "192.168.137.184"
QNX_PORT = 9877
LAPTOP_HOST = "192.168.0.158"
LAPTOP_PORT = 9876  # Laptop's taktflow-can-bridge service


def relay(src, dst, name):
    """Forward TCP data from src to dst."""
    try:
        while True:
            data = src.recv(4096)
            if not data:
                break
            dst.sendall(data)
    except Exception as e:
        print(f"[relay] {name} stopped: {e}")


def main():
    print(f"[relay] Connecting QNX {QNX_HOST}:{QNX_PORT}...")
    qnx = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    qnx.settimeout(5)
    qnx.connect((QNX_HOST, QNX_PORT))
    print(f"[relay] QNX connected")

    print(f"[relay] Connecting Laptop {LAPTOP_HOST}:{LAPTOP_PORT}...")
    laptop = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    laptop.settimeout(5)
    laptop.connect((LAPTOP_HOST, LAPTOP_PORT))
    print(f"[relay] Laptop connected")

    print(f"[relay] Bridging QNX <-> Laptop (Ctrl+C to stop)")

    t1 = threading.Thread(target=relay, args=(qnx, laptop, "QNX->Laptop"), daemon=True)
    t2 = threading.Thread(target=relay, args=(laptop, qnx, "Laptop->QNX"), daemon=True)
    t1.start()
    t2.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n[relay] Stopped")
        qnx.close()
        laptop.close()


if __name__ == "__main__":
    main()
