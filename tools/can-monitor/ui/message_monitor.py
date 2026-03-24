"""Message monitor tab — one row per CAN ID with decoded signals."""
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QTableWidget, QTableWidgetItem, QHeaderView,
    QHBoxLayout, QCheckBox
)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QColor
from . import theme

class MessageMonitorTab(QWidget):
    def __init__(self, store, decoder, parent=None):
        super().__init__(parent)
        self.store = store
        self.decoder = decoder
        self._hide_e2e = True
        self._row_map = {}  # can_id -> row index

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        # Controls
        ctrl = QHBoxLayout()
        self._e2e_check = QCheckBox("Show E2E signals")
        self._e2e_check.toggled.connect(self._on_e2e_toggle)
        ctrl.addWidget(self._e2e_check)
        ctrl.addStretch()
        layout.addLayout(ctrl)

        # Table
        self._table = QTableWidget()
        self._table.setColumnCount(7)
        self._table.setHorizontalHeaderLabels([
            "CAN ID", "Message", "Sender", "ASIL", "Signals", "Rate", "Age"
        ])
        self._table.horizontalHeader().setSectionResizeMode(4, QHeaderView.Stretch)
        self._table.setAlternatingRowColors(True)
        self._table.setEditTriggers(QTableWidget.NoEditTriggers)
        self._table.setSelectionBehavior(QTableWidget.SelectRows)
        self._table.setSortingEnabled(False)
        self._table.verticalHeader().setVisible(False)
        self._table.setColumnWidth(0, 88)
        self._table.setColumnWidth(1, 320)
        self._table.setColumnWidth(2, 70)
        self._table.setColumnWidth(3, 62)
        self._table.setColumnWidth(5, 96)
        self._table.setColumnWidth(6, 70)
        layout.addWidget(self._table)

    def _on_e2e_toggle(self, checked):
        self._hide_e2e = not checked

    def refresh(self, messages, elapsed):
        """Refresh table from message snapshot."""
        sorted_ids = sorted(messages.keys())

        # Add new rows if needed
        if len(sorted_ids) != self._table.rowCount():
            self._table.setRowCount(len(sorted_ids))
            self._row_map = {cid: i for i, cid in enumerate(sorted_ids)}

        for cid in sorted_ids:
            row = self._row_map.get(cid)
            if row is None:
                # Rebuild map
                self._row_map = {c: i for i, c in enumerate(sorted_ids)}
                self._table.setRowCount(len(sorted_ids))
                row = self._row_map.get(cid, 0)

            ms = messages[cid]
            frame = ms["latest"]
            age = elapsed - ms["last_seen"]

            # CAN ID
            id_item = QTableWidgetItem(f"0x{cid:03X}")
            id_item.setTextAlignment(Qt.AlignCenter)
            self._table.setItem(row, 0, id_item)

            # Message name + receivers
            msg_text = f"{frame.msg_name} -> {frame.receivers}"
            self._table.setItem(row, 1, QTableWidgetItem(msg_text))

            # Sender
            sender_item = QTableWidgetItem(frame.sender)
            sender_item.setTextAlignment(Qt.AlignCenter)
            self._table.setItem(row, 2, sender_item)

            # ASIL badge
            asil_item = QTableWidgetItem(frame.asil)
            asil_item.setTextAlignment(Qt.AlignCenter)
            asil_color = theme.ASIL_COLORS.get(frame.asil, theme.ASIL_COLORS["QM"])
            asil_item.setForeground(QColor(asil_color))
            self._table.setItem(row, 3, asil_item)

            # Signals
            sig_str = self.decoder.format_signals(frame.signals, self._hide_e2e)
            self._table.setItem(row, 4, QTableWidgetItem(sig_str))

            # Rate
            rate = ms["rate"]
            rate_str = f"{rate:.1f}/s"
            # Color: green if within expected cycle, yellow/red if off
            rate_item = QTableWidgetItem(rate_str)
            rate_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            self._table.setItem(row, 5, rate_item)

            # Age
            if age < 0.3:
                age_str = f"{age:.1f}s"
                age_color = theme.SUCCESS
            elif age < 1.0:
                age_str = f"{age:.1f}s"
                age_color = theme.WARNING
            else:
                age_str = f"{age:.0f}s"
                age_color = theme.ERROR
            age_item = QTableWidgetItem(age_str)
            age_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            age_item.setForeground(QColor(age_color))
            self._table.setItem(row, 6, age_item)

            # Row background by ECU
            ecu_bg = theme.ECU_COLORS.get(frame.sender, theme.BG_DEEP)
            for col in range(7):
                item = self._table.item(row, col)
                if item:
                    item.setBackground(QColor(ecu_bg))
