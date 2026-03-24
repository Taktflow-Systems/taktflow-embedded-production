"""Signal graph tab — pyqtgraph strip charts with signal selector and cursor probe."""
import bisect
import math
import os
import tempfile

from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QSplitter, QTreeWidget,
    QTreeWidgetItem, QPushButton, QComboBox, QLabel, QSizePolicy
)
from PyQt5.QtCore import Qt, QSize
from PyQt5.QtGui import QPixmap, QPainter, QColor, QPen, QPolygon
from PyQt5.QtCore import QPoint
import pyqtgraph as pg
from . import theme


def _generate_arrow_images():
    """Generate expand/collapse arrow pixmaps for dark theme, return (closed_path, open_path)."""
    arrow_dir = os.path.join(tempfile.gettempdir(), "taktflow_can_monitor")
    os.makedirs(arrow_dir, exist_ok=True)

    closed_path = os.path.join(arrow_dir, "arrow_closed.png")
    open_path = os.path.join(arrow_dir, "arrow_open.png")

    color = QColor(theme.TEXT_DIM)
    size = 12

    # Right-pointing triangle (collapsed)
    pm = QPixmap(size, size)
    pm.fill(Qt.transparent)
    p = QPainter(pm)
    p.setRenderHint(QPainter.Antialiasing)
    p.setPen(Qt.NoPen)
    p.setBrush(color)
    p.drawPolygon(QPolygon([
        QPoint(3, 2), QPoint(10, 6), QPoint(3, 10)
    ]))
    p.end()
    pm.save(closed_path)

    # Down-pointing triangle (expanded)
    pm2 = QPixmap(size, size)
    pm2.fill(Qt.transparent)
    p2 = QPainter(pm2)
    p2.setRenderHint(QPainter.Antialiasing)
    p2.setPen(Qt.NoPen)
    p2.setBrush(color)
    p2.drawPolygon(QPolygon([
        QPoint(2, 3), QPoint(10, 3), QPoint(6, 10)
    ]))
    p2.end()
    pm2.save(open_path)

    return closed_path, open_path


_ARROW_CLOSED, _ARROW_OPEN = _generate_arrow_images()

# Configure pyqtgraph
pg.setConfigOptions(
    background=theme.BG_DEEP,
    foreground=theme.TEXT,
    antialias=True,
)

PLOT_COLORS = [
    "#c4b5fd", "#f472b6", "#34d399", "#fbbf24",
    "#60a5fa", "#fb923c", "#a78bfa", "#f87171",
]

CURSOR_COLOR = "#ffffff"
CURSOR_WIDTH = 1


class SignalGraphTab(QWidget):
    def __init__(self, store, decoder, parent=None):
        super().__init__(parent)
        self.store = store
        self.decoder = decoder
        self._plots = {}       # signal_key -> PlotEntry
        self._frozen = False
        self._cursor_x = None
        self._color_map = {}   # signal_key -> assigned color (persists across rebuilds)
        self._rebuilding = False  # guard against re-entrant itemChanged

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 0)
        layout.setSpacing(2)

        # Controls bar
        ctrl = QHBoxLayout()
        ctrl.addWidget(QLabel("Time Window:"))
        self._window_combo = QComboBox()
        self._window_combo.addItems(["5s", "10s", "30s", "60s"])
        self._window_combo.setCurrentIndex(1)
        ctrl.addWidget(self._window_combo)

        self._freeze_btn = QPushButton("Freeze")
        self._freeze_btn.setCheckable(True)
        self._freeze_btn.toggled.connect(lambda c: setattr(self, '_frozen', c))
        ctrl.addWidget(self._freeze_btn)

        self._clear_btn = QPushButton("Clear All")
        self._clear_btn.clicked.connect(self._clear_all)
        ctrl.addWidget(self._clear_btn)

        self._reset_y_btn = QPushButton("Reset Y")
        self._reset_y_btn.setToolTip("Reset Y-axis auto-range on all plots")
        self._reset_y_btn.clicked.connect(self._reset_y_range)
        ctrl.addWidget(self._reset_y_btn)

        ctrl.addStretch()

        self._cursor_label = QLabel("")
        self._cursor_label.setStyleSheet(
            f"color: {theme.ACCENT}; font-family: {theme.FONT_DATA};"
            f"font-size: {theme.FONT_SIZE_UI}px; padding: 0 10px;"
        )
        ctrl.addWidget(self._cursor_label)

        layout.addLayout(ctrl)

        # Splitter fills ALL remaining space
        splitter = QSplitter(Qt.Horizontal)
        splitter.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        layout.addWidget(splitter, stretch=1)

        # Left: signal tree with checkboxes
        tree_container = QWidget()
        tree_layout = QVBoxLayout(tree_container)
        tree_layout.setContentsMargins(0, 0, 0, 0)
        tree_layout.setSpacing(2)

        tree_hint = QLabel("Check signals to show on chart:")
        tree_hint.setStyleSheet(
            f"color: {theme.TEXT_DIM}; font-size: {theme.FONT_SIZE_SMALL}px; padding: 3px;"
        )
        tree_hint.setFixedHeight(24)
        tree_layout.addWidget(tree_hint)

        self._tree = QTreeWidget()
        self._tree.setHeaderLabels(["Signal", "Unit"])
        self._tree.setColumnWidth(0, 200)
        # Apply visible arrow images for dark theme
        closed = _ARROW_CLOSED.replace("\\", "/")
        opened = _ARROW_OPEN.replace("\\", "/")
        self._tree.setStyleSheet(f"""
            QTreeWidget::branch:has-children:closed {{
                image: url({closed});
            }}
            QTreeWidget::branch:has-children:open {{
                image: url({opened});
            }}
        """)
        self._build_signal_tree()
        self._tree.itemChanged.connect(self._on_item_changed)
        tree_layout.addWidget(self._tree, stretch=1)

        splitter.addWidget(tree_container)

        # Right: plot area
        self._plot_widget = pg.GraphicsLayoutWidget()
        self._plot_widget.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        splitter.addWidget(self._plot_widget)

        splitter.setSizes([280, 800])
        splitter.setStretchFactor(0, 0)
        splitter.setStretchFactor(1, 1)

    def _build_signal_tree(self):
        tree_data = self.decoder.get_signal_tree()

        # Group messages by sender (ECU)
        ecu_groups = {}
        for msg_name, info in tree_data.items():
            sender = info["sender"]
            if sender not in ecu_groups:
                ecu_groups[sender] = []
            ecu_groups[sender].append((msg_name, info))

        # Sort ECUs alphabetically, messages by CAN ID within each ECU
        for ecu_name in sorted(ecu_groups.keys()):
            ecu_item = QTreeWidgetItem([ecu_name, ""])
            ecu_item.setFlags(ecu_item.flags() & ~Qt.ItemIsUserCheckable)
            ecu_item.setExpanded(True)

            msgs = sorted(ecu_groups[ecu_name], key=lambda x: x[1]["msg_id"])
            for msg_name, info in msgs:
                msg_item = QTreeWidgetItem([
                    f"{msg_name} (0x{info['msg_id']:03X})",
                    ""
                ])
                msg_item.setFlags(msg_item.flags() & ~Qt.ItemIsUserCheckable)
                for sig_name, unit, sig_min, sig_max in info["signals"]:
                    sig_item = QTreeWidgetItem([sig_name, unit or ""])
                    sig_item.setData(0, Qt.UserRole, f"{msg_name}.{sig_name}")
                    sig_item.setFlags(sig_item.flags() | Qt.ItemIsUserCheckable)
                    sig_item.setCheckState(0, Qt.Unchecked)
                    msg_item.addChild(sig_item)
                ecu_item.addChild(msg_item)
                msg_item.setExpanded(False)

            self._tree.addTopLevelItem(ecu_item)

    def _on_item_changed(self, item, column):
        """Handle checkbox toggle — rebuild plots for checked signals."""
        if self._rebuilding:
            return
        key = item.data(0, Qt.UserRole)
        if key is None:
            return  # message-level item, ignore

        checked = item.checkState(0) == Qt.Checked
        if checked and key not in self._plots:
            self._add_plot(key)
        elif not checked and key in self._plots:
            self._remove_plot(key)

    def _get_color(self, key):
        """Get a stable color for a signal key."""
        if key not in self._color_map:
            self._color_map[key] = PLOT_COLORS[len(self._color_map) % len(PLOT_COLORS)]
        return self._color_map[key]

    def _add_plot(self, key):
        color = self._get_color(key)

        plot = self._plot_widget.addPlot(title=key)
        plot.showGrid(x=True, y=True, alpha=0.3)
        plot.setMouseEnabled(x=False, y=True)  # lock X (time window), zoom Y only
        plot.setLabel('bottom', 'Time', 's')
        parts = key.split('.')
        sig_label = parts[1] if len(parts) == 2 else key
        plot.setLabel('left', sig_label)

        pen = pg.mkPen(color=color, width=2)
        curve = plot.plot(pen=pen)

        # Cursor vertical line
        cursor_line = pg.InfiniteLine(
            angle=90, movable=False,
            pen=pg.mkPen(color=CURSOR_COLOR, width=CURSOR_WIDTH, style=Qt.DashLine)
        )
        cursor_line.setVisible(False)
        plot.addItem(cursor_line)

        # Cursor dot
        cursor_dot = pg.ScatterPlotItem(
            size=10, pen=pg.mkPen(CURSOR_COLOR, width=1.5),
            brush=pg.mkBrush(color)
        )
        cursor_dot.setVisible(False)
        plot.addItem(cursor_dot)

        # Cursor value text
        cursor_text = pg.TextItem(
            color=color, anchor=(0, 1),
            fill=pg.mkBrush(theme.BG_PANEL + "CC")
        )
        cursor_text.setFont(pg.QtGui.QFont(theme.FONT_DATA, theme.FONT_SIZE_DATA, pg.QtGui.QFont.Bold))
        cursor_text.setVisible(False)
        plot.addItem(cursor_text)

        entry = PlotEntry(
            key=key, color=color, plot=plot, curve=curve,
            cursor_line=cursor_line, cursor_dot=cursor_dot,
            cursor_text=cursor_text,
            times=[], values=[],
        )
        self._plots[key] = entry
        self._plot_widget.nextRow()

        # Connect click
        plot.scene().sigMouseClicked.connect(self._on_plot_clicked)

    def _remove_plot(self, key):
        """Remove one signal's plot and rebuild the layout with remaining plots."""
        if key in self._plots:
            del self._plots[key]
        self._rebuild_plots()

    def _rebuild_plots(self):
        """Clear and re-add all checked signal plots (preserves order)."""
        self._rebuilding = True
        old_cursor = self._cursor_x

        # Disconnect old scene signals to avoid duplicates
        self._plot_widget.clear()

        # Collect current keys in order
        keys = list(self._plots.keys())
        self._plots.clear()

        for key in keys:
            self._add_plot(key)

        self._cursor_x = old_cursor
        self._rebuilding = False

    def _clear_all(self):
        """Uncheck all signals and clear plots."""
        self._rebuilding = True
        self._plot_widget.clear()
        self._plots.clear()
        self._cursor_x = None
        self._cursor_label.setText("")

        # Uncheck all tree items (ECU → Message → Signal)
        for i in range(self._tree.topLevelItemCount()):
            ecu_item = self._tree.topLevelItem(i)
            for j in range(ecu_item.childCount()):
                msg_item = ecu_item.child(j)
                for k in range(msg_item.childCount()):
                    sig_item = msg_item.child(k)
                    sig_item.setCheckState(0, Qt.Unchecked)

        self._rebuilding = False

    def _reset_y_range(self):
        """Re-enable Y auto-range on all plots."""
        for entry in self._plots.values():
            entry.plot.enableAutoRange(axis='y')

    def _on_plot_clicked(self, event):
        """Place cursor on all plots at the clicked x position."""
        pos = event.scenePos()
        for entry in self._plots.values():
            vb = entry.plot.vb
            if vb.sceneBoundingRect().contains(pos):
                mouse_point = vb.mapSceneToView(pos)
                self._cursor_x = mouse_point.x()
                self._update_cursors()
                return

    def _update_cursors(self):
        """Update cursor line + value readout on all plots."""
        if self._cursor_x is None:
            return

        readouts = []

        for key, entry in self._plots.items():
            entry.cursor_line.setPos(self._cursor_x)
            entry.cursor_line.setVisible(True)

            if not entry.times:
                entry.cursor_dot.setVisible(False)
                entry.cursor_text.setVisible(False)
                continue

            # Nearest sample to cursor position
            idx = bisect.bisect_left(entry.times, self._cursor_x)
            if idx >= len(entry.times):
                idx = len(entry.times) - 1
            elif idx > 0:
                if abs(entry.times[idx - 1] - self._cursor_x) < abs(entry.times[idx] - self._cursor_x):
                    idx = idx - 1

            t = entry.times[idx]
            v = entry.values[idx]

            entry.cursor_dot.setData([t], [v])
            entry.cursor_dot.setVisible(True)

            sig_label = key.split('.')[-1]
            entry.cursor_text.setText(f" {v:.2f} ")
            entry.cursor_text.setPos(t, v)
            entry.cursor_text.setVisible(True)

            readouts.append(f"{sig_label}={v:.2f}")

        if readouts:
            self._cursor_label.setText(
                f"t={self._cursor_x:.3f}s  |  " + "  |  ".join(readouts)
            )

    def _get_window_seconds(self):
        text = self._window_combo.currentText()
        return int(text.replace("s", ""))

    def refresh(self):
        if self._frozen or not self._plots:
            return

        window = self._get_window_seconds()

        for key, entry in self._plots.items():
            data = self.store.get_signal_data(key)
            if not data:
                continue

            all_times = [d[0] for d in data]
            all_values = [d[1] for d in data]

            if all_times:
                latest = all_times[-1]
                start = latest - window

                filtered_t = []
                filtered_v = []
                for t, v in zip(all_times, all_values):
                    if t >= start and not math.isnan(v):
                        filtered_t.append(t)
                        filtered_v.append(v)

                entry.times = filtered_t
                entry.values = filtered_v

                if filtered_t:
                    entry.curve.setData(filtered_t, filtered_v)
                    entry.plot.setXRange(start, latest, padding=0)

        if self._cursor_x is not None:
            self._update_cursors()


class PlotEntry:
    """Data holder for one signal's plot widgets."""
    __slots__ = (
        "key", "color", "plot", "curve",
        "cursor_line", "cursor_dot", "cursor_text",
        "times", "values",
    )

    def __init__(self, key, color, plot, curve,
                 cursor_line, cursor_dot, cursor_text,
                 times, values):
        self.key = key
        self.color = color
        self.plot = plot
        self.curve = curve
        self.cursor_line = cursor_line
        self.cursor_dot = cursor_dot
        self.cursor_text = cursor_text
        self.times = times
        self.values = values
