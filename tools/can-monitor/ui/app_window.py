"""Main application window for Taktflow CAN Monitor."""
import queue
import threading
import time

from PyQt5.QtWidgets import (
    QMainWindow, QTabWidget, QToolBar, QComboBox, QPushButton,
    QLabel, QStatusBar, QVBoxLayout, QWidget, QFileDialog, QHBoxLayout,
    QAction
)
from PyQt5.QtCore import QTimer, pyqtSignal, QObject
import serial.tools.list_ports

from ..core.bus_reader import ReaderThread
from ..core.socketcan_reader import SocketCanReader, RemoteCanReader, HAS_PYTHON_CAN
from ..core.decoder import CanDecoder
from ..core.frame_store import FrameStore, ParsedFrame
from ..core.recorder import Recorder
from . import theme
from .ecu_health import EcuHealthBar
from .message_monitor import MessageMonitorTab
from .trace_view import TraceViewTab
from .signal_graph import SignalGraphTab
from .bus_stats import BusStatsTab
from .tx_panel import TxPanelTab


class FrameSignal(QObject):
    """Signal bridge between dispatch thread and Qt main thread."""
    new_data = pyqtSignal()


class AppWindow(QMainWindow):
    def __init__(self, dbc_path=None):
        super().__init__()
        self.setWindowTitle("Taktflow CAN Bus Monitor")
        self.setMinimumSize(1100, 700)

        # Core
        self.decoder = CanDecoder(dbc_path)
        self.store = FrameStore()
        self.reader = None
        self.recorder = Recorder()
        self._frame_signal = FrameSignal()
        self._frame_signal.new_data.connect(self._on_new_data)
        self._dispatch_thread = None
        self._last_header_refresh = 0.0
        self._header_refresh_interval = 0.5

        # Apply theme
        self.setStyleSheet(theme.get_stylesheet())

        self._build_toolbar()
        self._build_ui()
        self._build_statusbar()

        # Refresh timer (60 Hz)
        self._refresh_timer = QTimer()
        self._refresh_timer.timeout.connect(self._refresh)
        self._refresh_timer.start(16)  # ~60 fps

        # Restore geometry
        from PyQt5.QtCore import QSettings
        settings = QSettings("Taktflow", "CANMonitor")
        geom = settings.value("geometry")
        if geom:
            self.restoreGeometry(geom)

    def _build_toolbar(self):
        toolbar = QToolBar("Main")
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        # Port selector
        toolbar.addWidget(QLabel("  Port: "))
        self._port_combo = QComboBox()
        self._port_combo.setMinimumWidth(120)
        self._refresh_ports()
        toolbar.addWidget(self._port_combo)

        # Refresh ports button
        refresh_btn = QPushButton("Scan")
        refresh_btn.clicked.connect(self._refresh_ports)
        toolbar.addWidget(refresh_btn)

        toolbar.addSeparator()

        # Connect / Disconnect
        self._connect_btn = QPushButton("Connect")
        self._connect_btn.setCheckable(True)
        self._connect_btn.clicked.connect(self._toggle_connection)
        toolbar.addWidget(self._connect_btn)

        toolbar.addSeparator()

        # Record
        self._record_btn = QPushButton("Record")
        self._record_btn.setCheckable(True)
        self._record_btn.clicked.connect(self._toggle_record)
        toolbar.addWidget(self._record_btn)

        toolbar.addSeparator()

        # DBC label
        toolbar.addWidget(QLabel("  DBC: "))
        self._dbc_label = QLabel(f"taktflow_vehicle.dbc ({len(self.decoder.db.messages)} msgs)")
        self._dbc_label.setStyleSheet(f"color: {theme.ACCENT}; font-family: {theme.FONT_DATA};")
        toolbar.addWidget(self._dbc_label)

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(4)

        # ECU health bar
        self._health_bar = EcuHealthBar()
        layout.addWidget(self._health_bar)

        # Tabs
        self._tabs = QTabWidget()
        layout.addWidget(self._tabs)

        self._msg_tab = MessageMonitorTab(self.store, self.decoder)
        self._trace_tab = TraceViewTab(self.store, self.decoder)
        self._graph_tab = SignalGraphTab(self.store, self.decoder)
        self._stats_tab = BusStatsTab(self.store)
        self._tx_tab = TxPanelTab(self)

        self._tabs.addTab(self._msg_tab, "Messages")
        self._tabs.addTab(self._trace_tab, "Trace")
        self._tabs.addTab(self._graph_tab, "Signals")
        self._tabs.addTab(self._stats_tab, "Stats")
        self._tabs.addTab(self._tx_tab, "TX Panel")

    def _build_statusbar(self):
        self._statusbar = QStatusBar()
        self.setStatusBar(self._statusbar)
        self._status_conn = QLabel("Disconnected")
        self._status_frames = QLabel("0 frames")
        self._status_fps = QLabel("0 fps")
        self._status_errors = QLabel("0 errors")
        for w in (self._status_conn, self._status_frames, self._status_fps, self._status_errors):
            self._statusbar.addPermanentWidget(w)

    def _refresh_ports(self):
        self._port_combo.clear()

        # ---- Remote TCP bridges (always available, any OS) ----
        self._port_combo.addItem(
            "🟣 QNX Pi 4 SIL (192.168.137.184:9877)",
            ("remote", "192.168.137.184", 9877)
        )
        self._port_combo.addItem(
            "🌐 Linux vcan0 (laptop 192.168.0.158:9876)",
            ("remote", "192.168.0.158", 9876)
        )

        # ---- Local SocketCAN interfaces (Linux only) ----
        if HAS_PYTHON_CAN:
            import subprocess
            try:
                out = subprocess.check_output(
                    ["ip", "-o", "link", "show", "type", "vcan"],
                    stderr=subprocess.DEVNULL, text=True, timeout=2
                )
                for line in out.strip().splitlines():
                    iface = line.split(":")[1].strip().split("@")[0]
                    self._port_combo.addItem(
                        f"🔌 {iface} (SocketCAN local)",
                        ("socketcan", iface)
                    )
            except Exception:
                pass
            # Also check physical CAN interfaces
            try:
                out = subprocess.check_output(
                    ["ip", "-o", "link", "show", "type", "can"],
                    stderr=subprocess.DEVNULL, text=True, timeout=2
                )
                for line in out.strip().splitlines():
                    iface = line.split(":")[1].strip().split("@")[0]
                    self._port_combo.addItem(
                        f"🔌 {iface} (SocketCAN local)",
                        ("socketcan", iface)
                    )
            except Exception:
                pass

        # ---- Serial ports (Waveshare USB-CAN) ----
        ports = serial.tools.list_ports.comports()
        for p in sorted(ports, key=lambda x: x.device):
            self._port_combo.addItem(
                f"📡 {p.device} - {p.description}",
                ("serial", p.device)
            )

        # Select COM13 if available, otherwise first entry
        for i in range(self._port_combo.count()):
            if "COM13" in self._port_combo.itemText(i):
                self._port_combo.setCurrentIndex(i)
                return
        if self._port_combo.count() > 0:
            self._port_combo.setCurrentIndex(0)

    def _toggle_connection(self, checked):
        if checked:
            port_data = self._port_combo.currentData()
            if not port_data:
                self._connect_btn.setChecked(False)
                return

            # Create reader based on port type
            if isinstance(port_data, tuple):
                port_type = port_data[0]
                if port_type == "remote":
                    host, tcp_port = port_data[1], port_data[2]
                    self.reader = RemoteCanReader(host, tcp_port)
                    conn_label = f"{host}:{tcp_port} (TCP bridge)"
                elif port_type == "socketcan":
                    channel = port_data[1]
                    self.reader = SocketCanReader(channel)
                    conn_label = f"{channel} (SocketCAN)"
                else:
                    self.reader = ReaderThread(port_data[1])
                    conn_label = f"{port_data[1]} @ 2Mbps"
            else:
                self.reader = ReaderThread(port_data)
                conn_label = f"{port_data} @ 2Mbps"

            if not self.reader.connect():
                self._status_conn.setText(f"Error: {self.reader.error}")
                self._status_conn.setStyleSheet(f"color: {theme.ERROR};")
                self._connect_btn.setChecked(False)
                self.reader = None
                return
            self.store = FrameStore()
            self._msg_tab.store = self.store
            self._trace_tab.store = self.store
            self._graph_tab.store = self.store
            self._stats_tab.store = self.store
            self.reader.start()
            self._start_dispatch()
            self._connect_btn.setText("Disconnect")
            self._status_conn.setText(conn_label)
            self._status_conn.setStyleSheet(f"color: {theme.SUCCESS};")
        else:
            self._stop_connection()

    def _stop_connection(self):
        if self._dispatch_thread:
            self._dispatch_running = False
            self._dispatch_thread.join(timeout=2)
            self._dispatch_thread = None
        if self.reader:
            self.reader.stop()
            self.reader = None
        self._connect_btn.setText("Connect")
        self._connect_btn.setChecked(False)
        self._status_conn.setText("Disconnected")
        self._status_conn.setStyleSheet(f"color: {theme.TEXT_DIM};")

    def _start_dispatch(self):
        self._dispatch_running = True
        self._dispatch_thread = threading.Thread(target=self._dispatch_loop, daemon=True)
        self._dispatch_thread.start()

    def _dispatch_loop(self):
        """Read from reader queue, decode, push to store."""
        while self._dispatch_running and self.reader:
            batch = []
            try:
                # Drain up to 500 frames per batch
                for _ in range(500):
                    raw = self.reader.out_queue.get_nowait()
                    batch.append(raw)
            except queue.Empty:
                pass

            if not batch:
                time.sleep(0.002)
                continue

            for raw in batch:
                # Reject bad checksums (fixed protocol only — variable has no checksum)
                if not raw.get("checksum_ok", True):
                    self.store.error_frames += 1
                    continue

                # DLC cross-check: reject frames whose DLC is shorter than DBC expects.
                # Catches Waveshare variable-protocol mis-sync (no checksum).
                # Allow DLC >= expected — CAN padding to 8 bytes is normal.
                msg_def = self.decoder._msg_cache.get(raw["can_id"])
                if msg_def and raw["dlc"] < msg_def.length:
                    self.store.rejected_dlc += 1
                    continue

                # Decode with choices for display, without for plotting
                decoded = self.decoder.decode_raw(
                    raw["can_id"], raw["data"], decode_choices=True
                )
                decoded_raw = self.decoder.decode_raw(
                    raw["can_id"], raw["data"], decode_choices=False
                )
                # Strict DBC-only monitor mode:
                # drop any frame that is not decodable by DBC, or decodes to no signals.
                if not decoded or not decoded.get("signals"):
                    self.store.rejected_unknown += 1
                    continue
                # Signal range validation: reject frames with out-of-range values.
                # Catches Waveshare mis-sync where data bytes are garbage (e.g. ECU_ID=170=0xAA).
                if not self.decoder.signals_in_range(raw["can_id"], decoded["signals"]):
                    self.store.rejected_range += 1
                    continue
                frame = ParsedFrame(
                    timestamp=raw["timestamp"],
                    can_id=raw["can_id"],
                    dlc=raw["dlc"],
                    data=raw["data"],
                    checksum_ok=raw.get("checksum_ok", True),
                )
                frame.msg_name = decoded["msg_name"]
                frame.signals = decoded["signals"]
                frame.raw_signals = decoded_raw["signals"] if decoded_raw else {}
                frame.sender = decoded["sender"]
                frame.receivers = decoded.get("receivers", "-")
                frame.asil = decoded["asil"]
                frame.cycle_ms = decoded["cycle_ms"]

                self.store.add_frame(frame)

                if self.recorder.recording:
                    self.recorder.write_frame(frame)

            # Signal Qt thread
            self._frame_signal.new_data.emit()

    def _on_new_data(self):
        """Called in Qt main thread when new data is available."""
        pass  # Refresh happens on timer

    def _refresh(self):
        """Periodic refresh at 60Hz -- only update visible tab."""
        stats = self.store.get_stats()
        msgs = self.store.get_messages_snapshot()

        # Header/status updates are intentionally throttled to reduce jitter/noise.
        now = time.monotonic()
        if now - self._last_header_refresh >= self._header_refresh_interval:
            self._status_frames.setText(f"{stats['total']:,} frames")
            self._status_fps.setText(f"{stats['fps']:.0f} fps")
            self._status_errors.setText(f"{stats['errors']} errors")
            self._health_bar.update_from_store(msgs, stats["elapsed"])
            self._last_header_refresh = now

        # Active tab only
        idx = self._tabs.currentIndex()
        if idx == 0:
            self._msg_tab.refresh(msgs, stats["elapsed"])
        elif idx == 1:
            self._trace_tab.refresh()
        elif idx == 2:
            self._graph_tab.refresh()
        elif idx == 3:
            self._stats_tab.refresh(msgs, stats)
        # TX panel doesn't need periodic refresh

    def _toggle_record(self, checked):
        if checked:
            filename = self.recorder.start()
            self._record_btn.setText(f"Recording...")
            self._record_btn.setStyleSheet(f"color: {theme.ERROR};")
            self._statusbar.showMessage(f"Recording to {filename}", 3000)
        else:
            filename = self.recorder.stop()
            self._record_btn.setText("Record")
            self._record_btn.setStyleSheet("")
            if filename:
                self._statusbar.showMessage(f"Saved: {filename}", 5000)

    def send_tx(self, tx_bytes):
        """Send a TX frame via the reader's serial connection."""
        if self.reader:
            self.reader.send_frame(tx_bytes)

    def closeEvent(self, event):
        self._stop_connection()
        if self.recorder.recording:
            self.recorder.stop()
        # Save geometry
        from PyQt5.QtCore import QSettings
        settings = QSettings("Taktflow", "CANMonitor")
        settings.setValue("geometry", self.saveGeometry())
        super().closeEvent(event)
