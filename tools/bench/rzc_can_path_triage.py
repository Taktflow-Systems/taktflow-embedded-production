# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B bench helper: print the current RZC CAN-path triage sheet.

This keeps the live probe assignment, expected CAN IDs, physical checks,
and replay success gates in one executable place instead of relying on
scattered handoff notes.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[2]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.bench.hardware_map import DEFAULT_HARDWARE_MAP, resolve_stlink_probe

EXPECTED_IDS = {
    "heartbeat": "0x012",
    "request": "0x7E2",
    "response": "0x7EA",
}

KNOWN_SYMPTOMS = [
    "RZC firmware boots on COM8 and reports healthy local FDCAN init.",
    "Pi can0 still sees no 0x012 heartbeat and no 0x7EA UDS replies.",
    "CVC still reports h12=0 / RZC=TIMEOUT.",
    "Hot-plug SWD reads show g_can_rx_count=0, so the node is not receiving the shared bus either.",
]

CHECKS = [
    {
        "name": "transceiver",
        "details": "Confirm a TJA1051T/3 or equivalent CAN transceiver is physically populated on the RZC node.",
    },
    {
        "name": "power",
        "details": "Measure transceiver VCC and GND. Expect a stable 3.3 V rail and a valid ground reference.",
    },
    {
        "name": "standby",
        "details": "Confirm STB -> GND so the transceiver is not held in standby.",
    },
    {
        "name": "mcu_pins",
        "details": "Confirm TXD -> PA12 (FDCAN1_TX) and RXD -> PA11 (FDCAN1_RX), not swapped.",
    },
    {
        "name": "bus_continuity",
        "details": "Confirm CAN_H, CAN_L, and CAN_GND continuity from the transceiver into the shared trunk.",
    },
    {
        "name": "termination",
        "details": "With bench power off, measure about 60 ohm between CAN_H and CAN_L.",
    },
    {
        "name": "idle_bias",
        "details": "With bench power on and idle, expect both CAN_H and CAN_L near 2.5 V.",
    },
]

SUCCESS_GATES = [
    "Pi can0 capture sees heartbeat 0x012 from RZC.",
    "CVC status starts incrementing h12 instead of reporting RZC=TIMEOUT.",
    "Tester Present on 0x7E2 returns a positive 0x7EA response.",
    "22 F191 returns a positive 0x7EA response.",
    "22 F190 completes a full FF/FC/CF exchange and reassembles the VIN.",
]

REFERENCES = [
    "docs/guides/usb-can-adapter-setup.md",
    "docs/reference/lessons-learned/infrastructure/PROCESS-can-bus-bring-up.md",
]


def _load_rzc_assignment(hardware_map_path: Path) -> dict[str, Any]:
    return resolve_stlink_probe("rzc", path=hardware_map_path)


def _build_payload(hardware_map_path: Path) -> dict[str, Any]:
    row = _load_rzc_assignment(hardware_map_path)
    return {
        "logical_ecu": "rzc",
        "hardware_map": str(hardware_map_path),
        "stlink_serial": row.get("stlink_serial", ""),
        "com_port": row.get("com_port", ""),
        "target_mcu": row.get("target_mcu", ""),
        "flashed_image": row.get("flashed_image", ""),
        "notes": row.get("notes", ""),
        "expected_ids": EXPECTED_IDS,
        "known_symptoms": KNOWN_SYMPTOMS,
        "checks": CHECKS,
        "success_gates": SUCCESS_GATES,
        "references": REFERENCES,
    }


def _render_text(payload: dict[str, Any]) -> str:
    lines = [
        "RZC CAN-Path Triage",
        "===================",
        f"Hardware map: {payload['hardware_map']}",
        f"ST-LINK serial: {payload['stlink_serial']}",
        f"UART COM port: {payload['com_port']}",
        f"Target MCU: {payload['target_mcu']}",
        f"Flashed image: {payload['flashed_image']}",
        "",
        "Expected CAN IDs:",
        f"  heartbeat: {payload['expected_ids']['heartbeat']}",
        f"  request:   {payload['expected_ids']['request']}",
        f"  response:  {payload['expected_ids']['response']}",
        "",
        "Known current symptoms:",
    ]
    lines.extend(f"  - {item}" for item in payload["known_symptoms"])
    lines.append("")
    lines.append("Physical checklist:")
    lines.extend(f"  - {item['details']}" for item in payload["checks"])
    lines.append("")
    lines.append("Replay success gates after the physical fix:")
    lines.extend(f"  - {item}" for item in payload["success_gates"])
    lines.append("")
    lines.append("References:")
    lines.extend(f"  - {item}" for item in payload["references"])
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Print the current RZC CAN-path triage worksheet from the checked-in "
            "hardware map."
        )
    )
    parser.add_argument(
        "--hardware-map",
        default=str(DEFAULT_HARDWARE_MAP),
        help="Path to tools/bench/hardware-map.toml",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="Output format",
    )
    args = parser.parse_args(argv)

    payload = _build_payload(Path(args.hardware_map))
    if args.format == "json":
        print(json.dumps(payload, indent=2, sort_keys=True))
    else:
        print(_render_text(payload))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
