"""Bus statistics tab."""
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTableWidget, QTableWidgetItem,
    QHeaderView, QLabel, QGroupBox, QGridLayout
)
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QColor
from . import theme

class BusStatsTab(QWidget):
    def __init__(self, store, parent=None):
        super().__init__(parent)
        self.store = store

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 8, 8, 8)

        # Summary cards
        summary = QHBoxLayout()

        self._total_label = self._make_stat_card("Total Frames", "0")
        self._fps_label = self._make_stat_card("Frame Rate", "0 fps")
        self._errors_label = self._make_stat_card("Checksum Err", "0")
        self._rejected_dlc_label = self._make_stat_card("DLC Rejected", "0")
        self._rejected_range_label = self._make_stat_card("Range Rejected", "0")
        self._rejected_unknown_label = self._make_stat_card("Unknown ID", "0")
        self._bus_load_label = self._make_stat_card("Bus Load", "0%")
        self._elapsed_label = self._make_stat_card("Elapsed", "0s")

        for card in (self._total_label, self._fps_label, self._errors_label,
                     self._rejected_dlc_label, self._rejected_range_label,
                     self._rejected_unknown_label,
                     self._bus_load_label, self._elapsed_label):
            summary.addWidget(card[2])  # card = (name_lbl, value_lbl, group)
        layout.addLayout(summary)

        # Per-message rate table
        self._table = QTableWidget()
        self._table.setColumnCount(6)
        self._table.setHorizontalHeaderLabels([
            "CAN ID", "Message", "Count", "Rate (Hz)", "Expected (ms)", "ASIL"
        ])
        self._table.horizontalHeader().setSectionResizeMode(1, QHeaderView.Stretch)
        self._table.setAlternatingRowColors(True)
        self._table.setEditTriggers(QTableWidget.NoEditTriggers)
        self._table.verticalHeader().setVisible(False)
        self._table.setSortingEnabled(True)
        layout.addWidget(self._table)

    def _make_stat_card(self, name, default_value):
        group = QGroupBox(name)
        gl = QVBoxLayout(group)
        value_lbl = QLabel(default_value)
        value_lbl.setStyleSheet(
            f"color: {theme.ACCENT}; font-family: {theme.FONT_DATA};"
            f"font-size: {theme.FONT_SIZE_STAT_CARD}px; font-weight: bold;"
        )
        value_lbl.setAlignment(Qt.AlignCenter)
        gl.addWidget(value_lbl)
        name_lbl = QLabel(name)
        return name_lbl, value_lbl, group

    def refresh(self, messages, stats):
        # Summary
        self._total_label[1].setText(f"{stats['total']:,}")
        self._fps_label[1].setText(f"{stats['fps']:.0f} fps")
        self._errors_label[1].setText(f"{stats['errors']:,}")
        self._rejected_dlc_label[1].setText(f"{stats.get('rejected_dlc', 0):,}")
        self._rejected_range_label[1].setText(f"{stats.get('rejected_range', 0):,}")
        self._rejected_unknown_label[1].setText(f"{stats.get('rejected_unknown', 0):,}")
        self._elapsed_label[1].setText(f"{stats['elapsed']:.0f}s")

        # Bus load estimate (assuming 500kbps, avg 8 byte frames = 130 bits each)
        # load = (frames/sec * 130 bits) / 500000 * 100%
        bus_load = (stats['fps'] * 130) / 500_000 * 100
        self._bus_load_label[1].setText(f"{bus_load:.1f}%")

        # Per-message table
        sorted_ids = sorted(messages.keys())
        self._table.setSortingEnabled(False)
        self._table.setRowCount(len(sorted_ids))

        for row, cid in enumerate(sorted_ids):
            ms = messages[cid]
            frame = ms["latest"]

            self._table.setItem(row, 0, QTableWidgetItem(f"0x{cid:03X}"))
            self._table.setItem(row, 1, QTableWidgetItem(frame.msg_name))

            count_item = QTableWidgetItem()
            count_item.setData(Qt.EditRole, ms["count"])
            self._table.setItem(row, 2, count_item)

            rate_item = QTableWidgetItem(f"{ms['rate']:.1f}")
            rate_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            self._table.setItem(row, 3, rate_item)

            cycle_item = QTableWidgetItem(f"{frame.cycle_ms}" if frame.cycle_ms else "event")
            cycle_item.setTextAlignment(Qt.AlignCenter)
            self._table.setItem(row, 4, cycle_item)

            asil_item = QTableWidgetItem(frame.asil)
            asil_item.setTextAlignment(Qt.AlignCenter)
            asil_color = theme.ASIL_COLORS.get(frame.asil, theme.ASIL_COLORS["QM"])
            asil_item.setForeground(QColor(asil_color))
            self._table.setItem(row, 5, asil_item)

        self._table.setSortingEnabled(True)
