"""Serial reader thread for Waveshare USB-CAN adapter."""
import queue
import threading
import time
import serial
from .waveshare import parse_fixed_frame, parse_variable_frame, detect_protocol, FRAME_SIZE


class ReaderThread(threading.Thread):
    """Reads from serial port, parses Waveshare frames, puts dicts on out_queue."""

    def __init__(self, port, baud=2_000_000, out_queue=None):
        super().__init__(daemon=True)
        self.port = port
        self.baud = baud
        self.out_queue = out_queue or queue.Queue(maxsize=50_000)
        self.running = False
        self.ser = None
        self.error = None
        self.protocol = "unknown"

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=0.05)
            self.protocol, initial_buf = detect_protocol(self.ser, timeout=2.0)
            if self.protocol == "unknown":
                self.error = "Could not detect Waveshare protocol"
                self.ser.close()
                self.ser = None
                return False
            self._initial_buf = bytearray(initial_buf)
            self.running = True
            self.error = None
            return True
        except serial.SerialException as e:
            self.error = str(e)
            return False

    def run(self):
        if not self.ser:
            return
        buf = getattr(self, "_initial_buf", bytearray())
        t0 = time.monotonic()
        while self.running:
            try:
                waiting = self.ser.in_waiting
                chunk = self.ser.read(max(1, waiting))
            except Exception:
                break
            if chunk:
                buf.extend(chunk)
            now = time.monotonic()
            ts = now - t0
            if self.protocol == "fixed":
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
                        frame["timestamp"] = ts
                        try:
                            self.out_queue.put_nowait(frame)
                        except queue.Full:
                            pass
            else:  # variable
                while len(buf) >= 4:
                    if buf[0] != 0xAA:
                        buf.pop(0)
                        continue
                    frame, consumed = parse_variable_frame(buf)
                    if consumed == 0:
                        break
                    if frame:
                        frame["timestamp"] = ts
                        try:
                            self.out_queue.put_nowait(frame)
                        except queue.Full:
                            pass
                    buf = buf[consumed:]

    def stop(self):
        self.running = False
        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None

    def send_frame(self, tx_bytes):
        """Send raw bytes to the adapter."""
        if self.ser and self.ser.is_open:
            self.ser.write(tx_bytes)
