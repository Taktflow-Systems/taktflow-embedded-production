"""QNX TCP bridge → vcan0 relay for Docker.
Reads CAN frames from QNX Pi TCP bridge, writes to vcan0.
Also reads vcan0 and sends back to QNX (bidirectional).
"""
import os
import socket
import struct
import threading
import time
import can

QNX_HOST = os.environ.get("QNX_HOST", "192.0.2.10")
QNX_PORT = int(os.environ.get("QNX_PORT", "9877"))
VCAN = os.environ.get("CAN_INTERFACE", "vcan0")
TCP_FRAME = 21

def tcp_to_vcan(sock, bus):
    buf = bytearray()
    while True:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf.extend(chunk)
        except Exception:
            break
        while len(buf) >= TCP_FRAME:
            can_id = struct.unpack_from("<I", buf, 0)[0] & 0x1FFFFFFF
            dlc = buf[4]
            data = bytes(buf[5:13])[:dlc]
            buf = buf[TCP_FRAME:]
            try:
                bus.send(can.Message(arbitration_id=can_id, data=data, is_extended_id=False))
            except Exception:
                pass

def vcan_to_tcp(sock, bus):
    while True:
        try:
            m = bus.recv(timeout=0.1)
            if m is None:
                continue
            frame = struct.pack("<I", m.arbitration_id)
            frame += bytes([m.dlc])
            frame += bytes(m.data).ljust(8, b"\x00")
            frame += struct.pack("<Q", int(time.monotonic() * 1_000_000))
            sock.sendall(frame)
        except Exception:
            break

print(f"[relay] Connecting to QNX {QNX_HOST}:{QNX_PORT}...")
while True:
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((QNX_HOST, QNX_PORT))
        break
    except Exception as e:
        print(f"[relay] Connection failed: {e}, retrying in 3s...")
        time.sleep(3)

print(f"[relay] Connected. Bridging to {VCAN}...")
bus = can.interface.Bus(interface="socketcan", channel=VCAN)

t1 = threading.Thread(target=tcp_to_vcan, args=(sock, bus), daemon=True)
t2 = threading.Thread(target=vcan_to_tcp, args=(sock, bus), daemon=True)
t1.start()
t2.start()

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("[relay] Stopped")
    sock.close()
    bus.shutdown()
