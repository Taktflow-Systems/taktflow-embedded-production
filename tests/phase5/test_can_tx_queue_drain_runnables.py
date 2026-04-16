# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B follow-up — queued CAN TX drain must be scheduled on the
STM32-style ECUs.

Why this exists:
    `Can_Write()` falls back to a software TX queue when the hardware TX
    mailbox/FIFO is temporarily busy. That queue is only drained by
    `Can_MainFunction_Write()`.

    On 2026-04-15 the FZC bench showed a silent UDS F190 multi-frame
    failure: SingleFrame responses worked, but the larger VIN response
    sometimes emitted no FirstFrame on the wire. A missing queue-drain
    runnable is a credible cause for that class of symptom because the
    initial frame can be accepted into the software queue and then never
    flushed.

    CVC, FZC, and RZC all use generated RTE runnable tables instead of
    explicit main-loop calls, so we guard those tables directly here.
"""
from __future__ import annotations

import re
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]

RTE_CFGS = {
    "cvc": REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "Rte_Cfg_Cvc.c",
    "fzc": REPO_ROOT / "firmware" / "ecu" / "fzc" / "cfg" / "Rte_Cfg_Fzc.c",
    "rzc": REPO_ROOT / "firmware" / "ecu" / "rzc" / "cfg" / "Rte_Cfg_Rzc.c",
}


def _load_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _parse_runnable_rows(text: str) -> list[dict[str, int | str]]:
    match = re.search(
        r"runnable_config\[\].*?=\s*\{(.*?)\};",
        text,
        re.DOTALL,
    )
    assert match, "Runnable config table not found"

    rows = []
    for entry in re.findall(r"\{([^}]+)\}", match.group(1)):
        fields = [field.strip() for field in entry.strip().split(",")]
        rows.append(
            {
                "name": fields[0],
                "period_ms": int(fields[1].rstrip("u").strip()),
                "priority": int(fields[2].rstrip("u").strip()),
            }
        )
    return rows


def test_rte_configs_declare_can_mainfunction_write() -> None:
    for ecu, path in RTE_CFGS.items():
        text = _load_text(path)
        assert "extern void Can_MainFunction_Write(void);" in text, (
            f"{ecu} RTE config is missing the Can_MainFunction_Write extern. "
            "Without it, the generated runnable table cannot drain queued "
            "CAN frames after mailbox pressure."
        )


def test_rte_runnable_tables_schedule_can_mainfunction_write() -> None:
    for ecu, path in RTE_CFGS.items():
        text = _load_text(path)
        rows = _parse_runnable_rows(text)
        by_name = {row["name"]: row for row in rows}

        assert "Can_MainFunction_Write" in by_name, (
            f"{ecu} runnable table does not schedule Can_MainFunction_Write. "
            "Queued CAN frames can stall forever if Can_Write() had to fall "
            "back to the software TX queue."
        )
        assert by_name["Can_MainFunction_Write"]["period_ms"] == 1, (
            f"{ecu} does not schedule Can_MainFunction_Write at the 1 ms tick. "
            "Queued CAN frames can stall forever if the software TX queue is "
            "only drained at a slower rate."
        )


def test_write_runnable_is_ordered_before_busoff_in_tables() -> None:
    for ecu, path in RTE_CFGS.items():
        text = _load_text(path)
        rows = _parse_runnable_rows(text)
        positions = {row["name"]: idx for idx, row in enumerate(rows)}
        priorities = {row["name"]: row["priority"] for row in rows}

        assert "Can_MainFunction_Write" in priorities, (
            f"{ecu} write runnable missing from runnable table"
        )
        assert "Can_MainFunction_BusOff" in priorities, (
            f"{ecu} bus-off runnable missing from runnable table"
        )
        assert priorities["Can_MainFunction_Write"] >= priorities["Can_MainFunction_BusOff"], (
            f"{ecu} schedules Can_MainFunction_Write below "
            "Can_MainFunction_BusOff. The queue drain must not lose priority "
            "against bus-off polling."
        )
        if priorities["Can_MainFunction_Write"] == priorities["Can_MainFunction_BusOff"]:
            assert positions["Can_MainFunction_Write"] < positions["Can_MainFunction_BusOff"], (
                f"{ecu} places Can_MainFunction_BusOff ahead of the equal-priority "
                "Can_MainFunction_Write runnable. Keep the queue drain first so "
                "queued frames get a chance to flush on shared ticks."
            )
