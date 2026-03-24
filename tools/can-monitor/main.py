"""Taktflow CAN Bus Monitor — CANoe-style desktop tool.

Usage:
    python -m tools.can-monitor.main [--port COM13] [--dbc path/to/file.dbc]

Or double-click 'CAN Monitor.bat' from the project root.
"""
import argparse
import sys
import os

def main():
    parser = argparse.ArgumentParser(description="Taktflow CAN Bus Monitor")
    parser.add_argument("--port", default="COM13", help="Serial port (default: COM13)")
    parser.add_argument("--dbc", default=None, help="DBC file path (default: gateway/taktflow_vehicle.dbc)")
    args = parser.parse_args()

    # Must import PyQt5 after argparse to avoid conflicts
    from PyQt5.QtWidgets import QApplication
    from PyQt5.QtGui import QFont, QColor, QPalette

    app = QApplication(sys.argv)
    app.setApplicationName("Taktflow CAN Monitor")
    app.setOrganizationName("Taktflow")
    app.setStyle("Fusion")

    # Dark palette — makes all native indicators (tree arrows, checkboxes,
    # combobox arrows, scrollbar buttons) visible on dark backgrounds
    palette = QPalette()
    palette.setColor(QPalette.Window, QColor("#1e1b2e"))
    palette.setColor(QPalette.WindowText, QColor("#e5e7eb"))
    palette.setColor(QPalette.Base, QColor("#1e1b2e"))
    palette.setColor(QPalette.AlternateBase, QColor("#2d2640"))
    palette.setColor(QPalette.Text, QColor("#e5e7eb"))
    palette.setColor(QPalette.Button, QColor("#2d2640"))
    palette.setColor(QPalette.ButtonText, QColor("#e5e7eb"))
    palette.setColor(QPalette.BrightText, QColor("#ffffff"))
    palette.setColor(QPalette.Highlight, QColor("#4c1d95"))
    palette.setColor(QPalette.HighlightedText, QColor("#c4b5fd"))
    palette.setColor(QPalette.ToolTipBase, QColor("#2d2640"))
    palette.setColor(QPalette.ToolTipText, QColor("#e5e7eb"))
    palette.setColor(QPalette.Link, QColor("#c4b5fd"))
    palette.setColor(QPalette.Disabled, QPalette.Text, QColor("#6b7280"))
    palette.setColor(QPalette.Disabled, QPalette.ButtonText, QColor("#6b7280"))
    palette.setColor(QPalette.Mid, QColor("#4a4060"))
    palette.setColor(QPalette.Dark, QColor("#1e1b2e"))
    palette.setColor(QPalette.Midlight, QColor("#3d3455"))
    palette.setColor(QPalette.Light, QColor("#9ca3af"))
    app.setPalette(palette)

    from .ui.app_window import AppWindow
    window = AppWindow(dbc_path=args.dbc)
    window.show()

    # Auto-select port from args
    for i in range(window._port_combo.count()):
        if args.port in window._port_combo.itemText(i):
            window._port_combo.setCurrentIndex(i)
            break

    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
