"""TX panel — send CAN frames and UDS requests."""
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QLabel,
    QLineEdit, QPushButton, QComboBox, QGridLayout, QTextEdit
)
from PyQt5.QtCore import Qt
from ..core.waveshare import build_tx_frame
from ..core.tx_engine import (
    build_default_session, build_extended_session, build_ecu_reset,
    build_read_dtc, build_clear_dtc, build_tester_present
)
from . import theme

class TxPanelTab(QWidget):
    def __init__(self, app_window, parent=None):
        super().__init__(parent)
        self._app = app_window

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)
        layout.setSpacing(12)

        # Raw frame builder
        raw_group = QGroupBox("Raw Frame Builder")
        rg = QGridLayout(raw_group)

        rg.addWidget(QLabel("CAN ID (hex):"), 0, 0)
        self._raw_id = QLineEdit("100")
        self._raw_id.setMaximumWidth(100)
        rg.addWidget(self._raw_id, 0, 1)

        rg.addWidget(QLabel("Data (hex):"), 0, 2)
        self._raw_data = QLineEdit("00 00 00 00 00 00 00 00")
        self._raw_data.setMinimumWidth(250)
        rg.addWidget(self._raw_data, 0, 3)

        self._raw_send = QPushButton("Send")
        self._raw_send.clicked.connect(self._send_raw)
        rg.addWidget(self._raw_send, 0, 4)

        layout.addWidget(raw_group)

        # UDS quick buttons
        uds_group = QGroupBox("UDS Diagnostics")
        ug = QGridLayout(uds_group)

        ug.addWidget(QLabel("Target ECU:"), 0, 0)
        self._uds_ecu = QComboBox()
        self._uds_ecu.addItems(["CVC", "FZC", "RZC", "SC", "BCM", "ICU", "TCU", "Broadcast"])
        ug.addWidget(self._uds_ecu, 0, 1)

        uds_buttons = [
            ("Default Session (0x10 01)", self._uds_default_session),
            ("Extended Session (0x10 03)", self._uds_extended_session),
            ("ECU Reset (0x11 01)", self._uds_ecu_reset),
            ("Read DTC (0x19 02)", self._uds_read_dtc),
            ("Clear DTC (0x14 FF)", self._uds_clear_dtc),
            ("Tester Present (0x3E 00)", self._uds_tester_present),
        ]

        for i, (label, handler) in enumerate(uds_buttons):
            btn = QPushButton(label)
            btn.clicked.connect(handler)
            ug.addWidget(btn, 1 + i // 3, i % 3)

        layout.addWidget(uds_group)

        # TX log
        log_group = QGroupBox("TX Log")
        lg = QVBoxLayout(log_group)
        self._tx_log = QTextEdit()
        self._tx_log.setReadOnly(True)
        self._tx_log.setMaximumHeight(200)
        self._tx_log.setStyleSheet(
            f"font-family: {theme.FONT_DATA}; font-size: {theme.FONT_SIZE_DATA}px;"
            f"background-color: {theme.BG_DEEP}; color: {theme.TEXT};"
        )
        lg.addWidget(self._tx_log)
        layout.addWidget(log_group)

        layout.addStretch()

    def _log(self, msg):
        self._tx_log.append(msg)

    def _send_raw(self):
        try:
            can_id = int(self._raw_id.text(), 16)
            data_str = self._raw_data.text().strip()
            data = bytes(int(x, 16) for x in data_str.split())
            tx = build_tx_frame(can_id, data)
            self._app.send_tx(tx)
            data_hex = " ".join(f"{b:02X}" for b in data)
            self._log(f"TX: 0x{can_id:03X} [{len(data)}] {data_hex}")
        except Exception as e:
            self._log(f"Error: {e}")

    def _send_uds(self, builder_func):
        ecu = self._uds_ecu.currentText()
        can_id, tx = builder_func(ecu)
        self._app.send_tx(tx)
        self._log(f"TX UDS: 0x{can_id:03X} → {ecu}")

    def _uds_default_session(self):
        self._send_uds(build_default_session)

    def _uds_extended_session(self):
        self._send_uds(build_extended_session)

    def _uds_ecu_reset(self):
        self._send_uds(build_ecu_reset)

    def _uds_read_dtc(self):
        self._send_uds(build_read_dtc)

    def _uds_clear_dtc(self):
        self._send_uds(build_clear_dtc)

    def _uds_tester_present(self):
        self._send_uds(build_tester_present)
