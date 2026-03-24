"""SocketCAN / Remote CAN reader for vcan and physical CAN interfaces.

Supports two modes:
1. Local SocketCAN (Linux only): reads directly from vcan0/can0 via python-can
2. Remote TCP bridge: connects to can_bridge.py running on a remote host

Frame dict format matches Waveshare reader:
    {"can_id": int, "dlc": int, "data": bytes, "frame_type": "data",
     "frame_format": "standard", "timestamp": float}
"""
import queue
import socket
import struct
import threading
import time

# python-can is optional (only available on Linux with SocketCAN)
try:
    import can as python_can
    HAS_PYTHON_CAN = True
except ImportError:
    HAS_PYTHON_CAN = False


class SocketCanReader(threading.Thread):
    """Reads CAN frames from a local SocketCAN interface (Linux only)."""

    def __init__(self, channel="vcan0", out_queue=None):
        super().__init__(daemon=True)
        self.channel = channel
        self.out_queue = out_queue or queue.Queue(maxsize=50_000)
        self.running = False
        self.bus = None
        self.error = None

    def connect(self):
        if not HAS_PYTHON_CAN:
            self.error = "python-can not installed (pip install python-can)"
            return False
        try:
            self.bus = python_can.interface.Bus(
                interface="socketcan", channel=self.channel
            )
            self.running = True
            self.error = None
            return True
        except Exception as e:
            self.error = str(e)
            return False

    def run(self):
        if not self.bus:
            return
        t0 = time.monotonic()
        while self.running:
            try:
                msg = self.bus.recv(timeout=0.05)
            except Exception:
                break
            if msg is None:
                continue
            frame = {
                "can_id": msg.arbitration_id,
                "dlc": msg.dlc,
                "data": bytes(msg.data),
                "frame_type": "data" if not msg.is_remote_frame else "remote",
                "frame_format": "extended" if msg.is_extended_id else "standard",
                "timestamp": time.monotonic() - t0,
            }
            try:
                self.out_queue.put_nowait(frame)
            except queue.Full:
                pass

    def stop(self):
        self.running = False
        if self.bus:
            try:
                self.bus.shutdown()
            except Exception:
                pass
            self.bus = None

    def write(self, can_id, data, extended=False):
        """Send a CAN frame (for TX panel support)."""
        if self.bus:
            msg = python_can.Message(
                arbitration_id=can_id,
                data=data,
                is_extended_id=extended,
            )
            try:
                self.bus.send(msg)
                return True
            except Exception:
                return False
        return False


# ---- TCP Bridge Protocol ----
# Each frame: [4B can_id LE][1B dlc][8B data][8B timestamp_us LE] = 21 bytes
TCP_FRAME_SIZE = 21


class RemoteCanReader(threading.Thread):
    """Reads CAN frames from a remote can_bridge.py over TCP.

    Use this when vECUs run on a different machine (e.g., Ubuntu laptop)
    and you want to monitor from Windows.

    Start the bridge on the remote host:
        python3 can_bridge.py --channel vcan0 --port 9876

    Then connect from this reader:
        reader = RemoteCanReader("192.168.0.158", 9876)
    """

    def __init__(self, host, port=9876, out_queue=None):
        super().__init__(daemon=True)
        self.host = host
        self.port = port
        self.out_queue = out_queue or queue.Queue(maxsize=50_000)
        self.running = False
        self.sock = None
        self.error = None

    def connect(self):
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(5.0)
            self.sock.connect((self.host, self.port))
            self.sock.settimeout(0.1)
            self.running = True
            self.error = None
            return True
        except Exception as e:
            self.error = str(e)
            if self.sock:
                self.sock.close()
                self.sock = None
            return False

    def run(self):
        if not self.sock:
            return
        buf = bytearray()
        t0 = time.monotonic()
        while self.running:
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                buf.extend(chunk)
            except socket.timeout:
                continue
            except Exception:
                break

            while len(buf) >= TCP_FRAME_SIZE:
                can_id = struct.unpack_from("<I", buf, 0)[0]
                dlc = buf[4]
                data = bytes(buf[5:13])
                ts_us = struct.unpack_from("<Q", buf, 13)[0]
                buf = buf[TCP_FRAME_SIZE:]

                frame = {
                    "can_id": can_id & 0x1FFFFFFF,
                    "dlc": dlc,
                    "data": data[:dlc],
                    "frame_type": "data",
                    "frame_format": "extended" if (can_id & 0x80000000) else "standard",
                    "timestamp": time.monotonic() - t0,  # Local clock for correct age display
                }
                try:
                    self.out_queue.put_nowait(frame)
                except queue.Full:
                    pass

    def stop(self):
        self.running = False
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None

    def write(self, can_id, data, extended=False):
        """Send a CAN frame via the TCP bridge (TX support)."""
        if self.sock:
            try:
                frame = struct.pack("<IB", can_id | (0x80000000 if extended else 0), len(data))
                frame += bytes(data).ljust(8, b'\x00')
                frame += struct.pack("<Q", int(time.monotonic() * 1_000_000))
                self.sock.sendall(frame)
                return True
            except Exception:
                return False
        return False
