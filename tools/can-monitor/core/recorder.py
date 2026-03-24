"""CSV recording and replay for CAN frames."""
import csv
import os
import time
from .frame_store import ParsedFrame


class Recorder:
    def __init__(self):
        self.recording = False
        self._file = None
        self._writer = None
        self._filename = None

    def start(self, filename=None):
        if filename is None:
            ts = time.strftime("%Y%m%d_%H%M%S")
            filename = f"can_trace_{ts}.csv"
        self._filename = filename
        self._file = open(filename, "w", newline="")
        self._writer = csv.writer(self._file)
        self._writer.writerow([
            "Timestamp", "CAN_ID", "DLC", "Data_Hex",
            "Message", "Sender", "ASIL", "Signals"
        ])
        self.recording = True
        return filename

    def write_frame(self, frame: ParsedFrame):
        if not self.recording or not self._writer:
            return
        data_hex = " ".join(f"{b:02X}" for b in frame.data)
        sig_str = "; ".join(f"{k}={v}" for k, v in frame.signals.items())
        self._writer.writerow([
            f"{frame.timestamp:.6f}",
            f"0x{frame.can_id:03X}",
            frame.dlc,
            data_hex,
            frame.msg_name,
            frame.sender,
            frame.asil,
            sig_str,
        ])

    def stop(self):
        self.recording = False
        if self._file:
            self._file.close()
            self._file = None
            self._writer = None
        return self._filename


def export_trace(frames, filename):
    """Export a list of ParsedFrame to CSV."""
    with open(filename, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "Timestamp", "CAN_ID", "DLC", "Data_Hex",
            "Message", "Sender", "ASIL", "Signals"
        ])
        for frame in frames:
            data_hex = " ".join(f"{b:02X}" for b in frame.data)
            sig_str = "; ".join(f"{k}={v}" for k, v in frame.signals.items())
            writer.writerow([
                f"{frame.timestamp:.6f}",
                f"0x{frame.can_id:03X}",
                frame.dlc,
                data_hex,
                frame.msg_name,
                frame.sender,
                frame.asil,
                sig_str,
            ])


def load_replay(filename):
    """Load a CSV trace file and return list of ParsedFrame."""
    frames = []
    with open(filename, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            ts = float(row["Timestamp"])
            can_id = int(row["CAN_ID"], 16)
            dlc = int(row["DLC"])
            data = bytes(int(x, 16) for x in row["Data_Hex"].split())
            frame = ParsedFrame(
                timestamp=ts, can_id=can_id, dlc=dlc, data=data,
                msg_name=row.get("Message", ""),
                sender=row.get("Sender", ""),
                asil=row.get("ASIL", "QM"),
            )
            # Parse signals back
            sig_str = row.get("Signals", "")
            if sig_str:
                for part in sig_str.split("; "):
                    if "=" in part:
                        k, v = part.split("=", 1)
                        try:
                            frame.signals[k] = float(v)
                        except ValueError:
                            frame.signals[k] = v
            frames.append(frame)
    return frames
