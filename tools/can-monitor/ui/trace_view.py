"""Trace view tab — scrolling CAN frame log with filters and adjustable update rate."""
import time

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTableWidget, QTableWidgetItem,
    QHeaderView, QLineEdit, QComboBox, QPushButton, QCheckBox, QLabel,
    QFileDialog
)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QColor
from ..core.frame_store import ParsedFrame
from ..core.recorder import export_trace
from . import theme

# Update interval options (label, seconds)
UPDATE_RATES = [
    ("50ms",  0.05),
    ("100ms", 0.1),
    ("250ms", 0.25),
    ("500ms", 0.5),
    ("1s",    1.0),
    ("2s",    2.0),
    ("5s",    5.0),
]


class TraceViewTab(QWidget):
    def __init__(self, store, decoder, parent=None):
        super().__init__(parent)
        self.store = store
        self.decoder = decoder
        self._paused = False
        self._last_count = 0
        self._hide_e2e = True
        self._last_refresh = 0.0
        self._update_interval = 0.5  # default 500ms

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        # Filter bar
        fbar = QHBoxLayout()

        fbar.addWidget(QLabel("Filter ID:"))
        self._id_filter = QLineEdit()
        self._id_filter.setPlaceholderText("e.g. 0x100,0x200")
        self._id_filter.setMaximumWidth(200)
        fbar.addWidget(self._id_filter)

        fbar.addWidget(QLabel("ECU:"))
        self._ecu_filter = QComboBox()
        self._ecu_filter.addItems(["All", "CVC", "FZC", "RZC", "SC", "BCM", "ICU", "TCU"])
        fbar.addWidget(self._ecu_filter)

        self._e2e_check = QCheckBox("Show E2E")
        self._e2e_check.toggled.connect(lambda c: setattr(self, '_hide_e2e', not c))
        fbar.addWidget(self._e2e_check)

        fbar.addSeparator() if hasattr(fbar, 'addSeparator') else None

        fbar.addWidget(QLabel("  Update:"))
        self._rate_combo = QComboBox()
        for label, _ in UPDATE_RATES:
            self._rate_combo.addItem(label)
        self._rate_combo.setCurrentIndex(3)  # 500ms default
        self._rate_combo.currentIndexChanged.connect(self._on_rate_changed)
        fbar.addWidget(self._rate_combo)

        fbar.addStretch()

        self._pause_btn = QPushButton("Pause")
        self._pause_btn.setCheckable(True)
        self._pause_btn.toggled.connect(lambda c: setattr(self, '_paused', c))
        fbar.addWidget(self._pause_btn)

        self._export_btn = QPushButton("Export CSV")
        self._export_btn.clicked.connect(self._export)
        fbar.addWidget(self._export_btn)

        self._clear_btn = QPushButton("Clear")
        self._clear_btn.clicked.connect(self._clear)
        fbar.addWidget(self._clear_btn)

        layout.addLayout(fbar)

        # Table
        self._table = QTableWidget()
        self._table.setColumnCount(6)
        self._table.setHorizontalHeaderLabels([
            "Time", "CAN ID", "DLC", "Data", "Message", "Signals"
        ])
        self._table.horizontalHeader().setSectionResizeMode(5, QHeaderView.Stretch)
        self._table.setAlternatingRowColors(True)
        self._table.setEditTriggers(QTableWidget.NoEditTriggers)
        self._table.verticalHeader().setVisible(False)
        self._table.setColumnWidth(0, 110)
        self._table.setColumnWidth(1, 88)
        self._table.setColumnWidth(2, 52)
        self._table.setColumnWidth(3, 230)
        self._table.setColumnWidth(4, 300)
        layout.addWidget(self._table)

    def _on_rate_changed(self, index):
        if 0 <= index < len(UPDATE_RATES):
            self._update_interval = UPDATE_RATES[index][1]

    def _parse_id_filter(self):
        """Parse ID filter text into a set of CAN IDs."""
        text = self._id_filter.text().strip()
        if not text:
            return None
        ids = set()
        for part in text.replace(",", " ").split():
            part = part.strip()
            try:
                ids.add(int(part, 16) if part.startswith("0x") else int(part))
            except ValueError:
                pass
        return ids if ids else None

    def refresh(self):
        if self._paused:
            return

        # Throttle: only update at the chosen interval
        now = time.monotonic()
        if now - self._last_refresh < self._update_interval:
            return
        self._last_refresh = now

        frames = self.store.get_trace_snapshot(last_n=1000)
        id_filter = self._parse_id_filter()
        ecu_filter = self._ecu_filter.currentText()

        # Apply filters
        filtered = []
        for f in frames:
            if id_filter and f.can_id not in id_filter:
                continue
            if ecu_filter != "All" and f.sender != ecu_filter:
                continue
            filtered.append(f)

        # Only show last 500
        filtered = filtered[-500:]

        if len(filtered) == self._last_count:
            return
        self._last_count = len(filtered)

        self._table.setRowCount(len(filtered))
        for row, frame in enumerate(filtered):
            # Time
            self._table.setItem(row, 0, QTableWidgetItem(f"{frame.timestamp:.3f}"))
            # CAN ID
            id_item = QTableWidgetItem(f"0x{frame.can_id:03X}")
            id_item.setTextAlignment(Qt.AlignCenter)
            self._table.setItem(row, 1, id_item)
            # DLC
            self._table.setItem(row, 2, QTableWidgetItem(str(frame.dlc)))
            # Data hex
            data_hex = " ".join(f"{b:02X}" for b in frame.data)
            self._table.setItem(row, 3, QTableWidgetItem(data_hex))
            # Message name + receivers
            self._table.setItem(row, 4, QTableWidgetItem(f"{frame.msg_name} -> {frame.receivers}"))
            # Signals
            sig_str = self.decoder.format_signals(frame.signals, self._hide_e2e)
            self._table.setItem(row, 5, QTableWidgetItem(sig_str))

            # Row color
            ecu_bg = theme.ECU_COLORS.get(frame.sender, theme.BG_DEEP)
            for col in range(6):
                item = self._table.item(row, col)
                if item:
                    item.setBackground(QColor(ecu_bg))

        # Auto-scroll to bottom
        self._table.scrollToBottom()

    def _export(self):
        frames = self.store.get_trace_snapshot(last_n=10000)
        if not frames:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Export Trace", "can_trace.csv", "CSV Files (*.csv)"
        )
        if path:
            export_trace(frames, path)

    def _clear(self):
        self._table.setRowCount(0)
        self._last_count = 0
