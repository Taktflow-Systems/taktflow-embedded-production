"""ECU health status bar widget."""
from PyQt5.QtWidgets import QWidget, QHBoxLayout, QLabel, QFrame
from PyQt5.QtCore import Qt
from . import theme

VSM_NAMES = {0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP", 4: "SAFE_STOP", 5: "SHUTDOWN"}

class EcuHealthBar(QWidget):
    """Horizontal bar showing 7-ECU health status."""

    ECU_ORDER = ("CVC", "FZC", "RZC", "SC", "BCM", "ICU", "TCU")

    # Heartbeat CAN IDs (where available in DBC/system design).
    # Heartbeat CAN IDs per taktflow_vehicle.dbc (BCM has no heartbeat message)
    HB_IDS = {
        "CVC": 0x010,
        "FZC": 0x011,
        "RZC": 0x012,
        "SC": 0x013,
        "BCM": None,
        "ICU": 0x014,
        "TCU": 0x015,
    }

    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)
        layout.setSpacing(16)

        self.setStyleSheet(f"background-color: {theme.BG_PANEL}; border-radius: 4px;")

        self._labels = {}
        for ecu in self.ECU_ORDER:
            frame = QFrame()
            frame.setStyleSheet(
                f"background-color: {theme.ECU_COLORS.get(ecu, theme.BG_DEEP)};"
                f"border-radius: 4px; padding: 4px 12px;"
            )
            fl = QHBoxLayout(frame)
            fl.setContentsMargins(8, 2, 8, 2)

            name_lbl = QLabel(f"{ecu}:")
            name_lbl.setStyleSheet(
                f"color: {theme.ACCENT}; font-family: {theme.FONT_DATA};"
                f"font-weight: bold; font-size: {theme.FONT_SIZE_UI}px;"
            )
            fl.addWidget(name_lbl)

            status_lbl = QLabel("--")
            status_lbl.setStyleSheet(
                f"color: {theme.TEXT_DIM}; font-family: {theme.FONT_DATA}; font-size: {theme.FONT_SIZE_UI}px;"
            )
            fl.addWidget(status_lbl)

            layout.addWidget(frame)
            self._labels[ecu] = status_lbl

        layout.addStretch()

    def update_from_store(self, messages_snapshot, elapsed):
        """Update ECU health from FrameStore messages snapshot."""
        # Build sender -> latest-seen timestamp fallback so nodes without explicit
        # heartbeat IDs still show health based on any observed traffic.
        sender_last_seen = {}
        for ms in messages_snapshot.values():
            frame = ms["latest"]
            sender = frame.sender
            if sender not in sender_last_seen or ms["last_seen"] > sender_last_seen[sender]:
                sender_last_seen[sender] = ms["last_seen"]

        for ecu, hb_id in self.HB_IDS.items():
            lbl = self._labels[ecu]
            if hb_id is not None and hb_id in messages_snapshot:
                ms = messages_snapshot[hb_id]
                frame = ms["latest"]
                age = elapsed - ms["last_seen"]

                # Decode heartbeat: byte[2]=OperatingMode (after E2E bytes 0-1)
                op_mode = 0
                if len(frame.data) >= 4:
                    # OperatingMode is at bit 24, 4 bits = byte 3 lower nibble
                    op_mode = frame.data[3] & 0x0F
                vsm_str = VSM_NAMES.get(op_mode, f"0x{op_mode:02X}")

                if age < 0.3:
                    status, color = "OK", theme.SUCCESS
                elif age < 1.0:
                    status, color = "STALE", theme.WARNING
                else:
                    status, color = "DEAD", theme.ERROR

                alive = (frame.data[0] >> 4) & 0x0F if len(frame.data) >= 1 else 0
                lbl.setText(f"{status}  vsm={vsm_str}  alive={alive}")
                lbl.setStyleSheet(
                    f"color: {color}; font-family: {theme.FONT_DATA}; font-size: {theme.FONT_SIZE_UI}px;"
                )
            elif ecu in sender_last_seen:
                age = elapsed - sender_last_seen[ecu]
                if age < 0.3:
                    status, color = "OK", theme.SUCCESS
                elif age < 1.0:
                    status, color = "STALE", theme.WARNING
                else:
                    status, color = "DEAD", theme.ERROR
                lbl.setText(f"{status}  last={age:.1f}s")
                lbl.setStyleSheet(
                    f"color: {color}; font-family: {theme.FONT_DATA}; font-size: {theme.FONT_SIZE_UI}px;"
                )
            else:
                lbl.setText("--")
                lbl.setStyleSheet(
                    f"color: {theme.TEXT_DIM}; font-family: {theme.FONT_DATA}; font-size: {theme.FONT_SIZE_UI}px;"
                )
