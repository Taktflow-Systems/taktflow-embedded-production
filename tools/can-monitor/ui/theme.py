"""Taktflow purple dark theme for CAN Monitor."""

# Color palette
BG_DEEP = "#1e1b2e"
BG_PANEL = "#2d2640"
BG_INPUT = "#3d3455"
ACCENT = "#c4b5fd"
ACCENT_DIM = "#8b7dcf"
SELECTED = "#4c1d95"
TEXT = "#e5e7eb"
TEXT_DIM = "#9ca3af"
TEXT_BRIGHT = "#ffffff"
BORDER = "#4a4060"
SUCCESS = "#34d399"
WARNING = "#fbbf24"
ERROR = "#f87171"

# ECU row colors
ECU_COLORS = {
    "CVC": "#1a1840",
    "FZC": "#2a1530",
    "RZC": "#0f2518",
    "BCM": "#123026",
    "ICU": "#3a1d4e",
    "TCU": "#3a1f3f",
    "SC": "#2a2f3a",
    "?": BG_DEEP,
}

# ASIL badge colors
ASIL_COLORS = {
    "D": "#dc2626",
    "C": "#f97316",
    "B": "#facc15",
    "A": "#84cc16",
    "QM": "#6b7280",
}

FONT_DATA = "Consolas"
FONT_LABEL = "Segoe UI"
FONT_SIZE_UI = 14
FONT_SIZE_DATA = 13
FONT_SIZE_SMALL = 12
FONT_SIZE_STATUS = 13
FONT_SIZE_STAT_CARD = 28

def get_stylesheet():
    return f"""
    QMainWindow, QWidget {{
        background-color: {BG_DEEP};
        color: {TEXT};
        font-family: "{FONT_LABEL}";
        font-size: {FONT_SIZE_UI}px;
    }}
    QTabWidget::pane {{
        border: 1px solid {BORDER};
        background-color: {BG_DEEP};
    }}
    QTabBar::tab {{
        background-color: {BG_PANEL};
        color: {TEXT_DIM};
        padding: 8px 20px;
        border: 1px solid {BORDER};
        border-bottom: none;
        margin-right: 2px;
        font-family: "{FONT_LABEL}";
        font-size: {FONT_SIZE_UI}px;
        font-weight: bold;
    }}
    QTabBar::tab:selected {{
        background-color: {BG_DEEP};
        color: {ACCENT};
        border-bottom: 2px solid {ACCENT};
    }}
    QTabBar::tab:hover {{
        color: {TEXT_BRIGHT};
    }}
    QToolBar {{
        background-color: {BG_PANEL};
        border-bottom: 1px solid {BORDER};
        padding: 4px;
        spacing: 8px;
    }}
    QStatusBar {{
        background-color: {BG_PANEL};
        color: {TEXT_DIM};
        border-top: 1px solid {BORDER};
        font-family: "{FONT_DATA}";
        font-size: {FONT_SIZE_STATUS}px;
    }}
    QTableWidget {{
        background-color: {BG_DEEP};
        alternate-background-color: {BG_PANEL};
        color: {TEXT};
        gridline-color: {BORDER};
        border: none;
        font-family: "{FONT_DATA}";
        font-size: {FONT_SIZE_DATA}px;
        selection-background-color: {SELECTED};
    }}
    QTableWidget::item {{
        padding: 4px 8px;
    }}
    QHeaderView::section {{
        background-color: {BG_PANEL};
        color: {ACCENT};
        padding: 6px 10px;
        border: 1px solid {BORDER};
        font-family: "{FONT_DATA}";
        font-size: {FONT_SIZE_DATA}px;
        font-weight: bold;
    }}
    QPushButton {{
        background-color: {BG_PANEL};
        color: {TEXT};
        border: 1px solid {BORDER};
        padding: 8px 18px;
        border-radius: 4px;
        font-family: "{FONT_LABEL}";
    }}
    QPushButton:hover {{
        background-color: {SELECTED};
        color: {TEXT_BRIGHT};
    }}
    QPushButton:pressed {{
        background-color: {ACCENT_DIM};
    }}
    QPushButton:checked {{
        background-color: {SELECTED};
        color: {ACCENT};
        border: 1px solid {ACCENT};
    }}
    QComboBox {{
        background-color: {BG_INPUT};
        color: {TEXT};
        border: 1px solid {BORDER};
        padding: 6px 10px;
        border-radius: 4px;
        font-family: "{FONT_DATA}";
        font-size: {FONT_SIZE_DATA}px;
    }}
    QComboBox::drop-down {{
        border-left: 1px solid {BORDER};
        width: 20px;
    }}
    QComboBox QAbstractItemView {{
        background-color: {BG_PANEL};
        color: {TEXT};
        selection-background-color: {SELECTED};
    }}
    QLineEdit {{
        background-color: {BG_INPUT};
        color: {TEXT};
        border: 1px solid {BORDER};
        padding: 6px 10px;
        border-radius: 4px;
        font-family: "{FONT_DATA}";
        font-size: {FONT_SIZE_DATA}px;
    }}
    QLabel {{
        color: {TEXT};
    }}
    QTreeWidget {{
        background-color: {BG_DEEP};
        color: {TEXT};
        border: none;
        font-family: "{FONT_DATA}";
        font-size: {FONT_SIZE_DATA}px;
    }}
    QTreeWidget::item:selected {{
        background-color: {SELECTED};
    }}
    QTreeWidget::item:hover {{
        background-color: {BG_PANEL};
    }}
    QTreeWidget::branch {{
        background-color: transparent;
    }}
    QTreeWidget::indicator {{
        width: 14px;
        height: 14px;
        border: 1px solid {BORDER};
        border-radius: 2px;
        background-color: {BG_INPUT};
    }}
    QTreeWidget::indicator:checked {{
        background-color: {ACCENT};
        border-color: {ACCENT};
    }}
    QTreeWidget::indicator:hover {{
        border-color: {ACCENT_DIM};
    }}
    QScrollBar:vertical {{
        background-color: {BG_DEEP};
        width: 12px;
    }}
    QScrollBar::handle:vertical {{
        background-color: {BORDER};
        border-radius: 5px;
        min-height: 20px;
    }}
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
        height: 0px;
    }}
    QScrollBar:horizontal {{
        background-color: {BG_DEEP};
        height: 12px;
    }}
    QScrollBar::handle:horizontal {{
        background-color: {BORDER};
        border-radius: 5px;
        min-width: 20px;
    }}
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
        width: 0px;
    }}
    QCheckBox {{
        color: {TEXT};
        spacing: 8px;
        font-size: {FONT_SIZE_DATA}px;
    }}
    QCheckBox::indicator {{
        width: 14px;
        height: 14px;
        border: 1px solid {BORDER};
        border-radius: 2px;
        background-color: {BG_INPUT};
    }}
    QCheckBox::indicator:checked {{
        background-color: {ACCENT};
        border-color: {ACCENT};
    }}
    QCheckBox::indicator:hover {{
        border-color: {ACCENT_DIM};
    }}
    QGroupBox {{
        color: {ACCENT};
        border: 1px solid {BORDER};
        border-radius: 4px;
        margin-top: 8px;
        padding-top: 12px;
        font-weight: bold;
        font-size: {FONT_SIZE_DATA}px;
    }}
    QGroupBox::title {{
        subcontrol-origin: margin;
        padding: 0 6px;
    }}
    QSplitter::handle {{
        background-color: {BORDER};
    }}
    """

def asil_badge_style(asil):
    color = ASIL_COLORS.get(asil, ASIL_COLORS["QM"])
    return f"color: {color}; font-weight: bold; font-family: {FONT_DATA};"
