"""Mapping from DTC codes to SAP QM defect catalog entries.

Each DTC code from an ECU diagnostic alert maps to a specific SAP defect code,
human-readable text, and default priority level.
"""

from __future__ import annotations

from typing import Optional

# DTC hex code -> SAP defect catalog entry
DTC_TO_DEFECT: dict[str, dict[str, str]] = {
    "0xE301": {
        "code": "D-MOT-001",
        "text": "Motor overcurrent detected",
        "priority": "1",
    },
    "0xE302": {
        "code": "D-MOT-002",
        "text": "Motor overtemperature",
        "priority": "2",
    },
    "0xE303": {
        "code": "D-MOT-003",
        "text": "Motor stall fault",
        "priority": "1",
    },
    "0xD001": {
        "code": "D-STR-001",
        "text": "Steering plausibility fault",
        "priority": "1",
    },
    "0xE202": {
        "code": "D-BRK-001",
        "text": "Brake actuator fault",
        "priority": "1",
    },
    "0xE101": {
        "code": "D-CAN-001",
        "text": "CAN bus communication loss",
        "priority": "1",
    },
    "0xE102": {
        "code": "D-HB-001",
        "text": "ECU heartbeat timeout",
        "priority": "2",
    },
    "0xE401": {
        "code": "D-BAT-001",
        "text": "Battery undervoltage",
        "priority": "2",
    },
    "0xE501": {
        "code": "D-LID-001",
        "text": "LiDAR sensor fault",
        "priority": "3",
    },
    "0xE601": {
        "code": "D-ML-001",
        "text": "Anomaly detection alert",
        "priority": "2",
    },
}

# ECU node ID -> Plant code mapping
ECU_PLANTS: dict[int, str] = {
    1: "CVC",
    2: "FZC",
    3: "RZC",
    4: "SC",
    5: "BCM",
    6: "ICU",
    7: "TCU",
}


def map_dtc_to_defect(dtc_code: str) -> Optional[dict[str, str]]:
    """Look up a DTC code and return the corresponding SAP defect entry.

    Args:
        dtc_code: Hex DTC code string, e.g. '0xE301' or '0x00E301'.
                  Leading zero-padded formats are normalized before lookup.

    Returns:
        Dict with 'code', 'text', and 'priority' keys, or None if
        the DTC code is not found in the catalog.
    """
    # Normalize: strip "0x" prefix, remove leading zeros, re-add "0x"
    # so both "0xE301" and "0x00E301" match catalog key "0xE301"
    stripped = dtc_code.upper().removeprefix("0X").lstrip("0") or "0"
    normalized = f"0x{stripped}"
    return DTC_TO_DEFECT.get(normalized)
