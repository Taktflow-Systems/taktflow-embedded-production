#!/usr/bin/env python3
"""Scoped trace extraction to avoid large-context AI runs.

Extracts bidirectional traceability links:
  stk-sys: STK→SYS from both system-requirements.md "Traces up" fields
           AND the Section 18 forward matrix table.
  sys-swr: SYS→SWR from both system-requirements.md "Traces down" fields
           AND SWR-*.md "Traces up" fields.
"""
import argparse
import csv
import re
from pathlib import Path


def num_suffix(req_id: str) -> int:
    m = re.search(r"-([0-9]+)$", req_id)
    return int(m.group(1)) if m else -1


def in_range(req_id: str, from_id: str | None, to_id: str | None) -> bool:
    n = num_suffix(req_id)
    if from_id and n < num_suffix(from_id):
        return False
    if to_id and n > num_suffix(to_id):
        return False
    return True


def _extract_stk_sys_reverse(text: str) -> set[tuple[str, str]]:
    """Parse SYS sections for 'Traces up: STK-NNN' (reverse direction)."""
    rows: set[tuple[str, str]] = set()
    cur_sys = ""
    for line in text.splitlines():
        m = re.match(r"^###\s+(SYS-[0-9]+):", line)
        if m:
            cur_sys = m.group(1)
            continue
        if "- **Traces up**:" in line and cur_sys:
            for stk in re.findall(r"(STK-[0-9]+)", line):
                rows.add((stk, cur_sys))
    return rows


def _extract_stk_sys_forward(text: str) -> set[tuple[str, str]]:
    """Parse Section 18 forward matrix table '| STK-NNN | SYS-NNN, ... |'."""
    rows: set[tuple[str, str]] = set()
    in_matrix = False
    for line in text.splitlines():
        if re.match(r"^##\s+18\.\s+Traceability Matrix", line):
            in_matrix = True
            continue
        if in_matrix and re.match(r"^###?\s+18\.[1-9]", line):
            in_matrix = False
            continue
        if in_matrix and re.match(r"^##\s+(?!18\.)", line):
            in_matrix = False
            continue
        if in_matrix:
            m = re.match(r"^\|\s*(STK-[0-9]+)\s*\|(.+)\|", line)
            if m:
                stk = m.group(1)
                sys_cell = m.group(2)
                for sys_id in re.findall(r"(SYS-[0-9]+)", sys_cell):
                    rows.add((stk, sys_id))
    return rows


def extract_stk_sys(text: str, direction: str = "both") -> list[tuple[str, str]]:
    rows: set[tuple[str, str]] = set()
    if direction in ("reverse", "both"):
        rows |= _extract_stk_sys_reverse(text)
    if direction in ("forward", "both"):
        rows |= _extract_stk_sys_forward(text)
    return sorted(rows, key=lambda x: (num_suffix(x[0]), num_suffix(x[1])))


def _extract_sys_swr_forward(text: str) -> set[tuple[str, str]]:
    """Parse system-requirements.md 'Traces down: SWR-NNN' (forward direction)."""
    rows: set[tuple[str, str]] = set()
    cur_sys = ""
    for line in text.splitlines():
        m = re.match(r"^###\s+(SYS-[0-9]+):", line)
        if m:
            cur_sys = m.group(1)
            continue
        if "- **Traces down**:" in line and cur_sys:
            for swr in re.findall(r"(SWR-[A-Z]+-[0-9]+)", line):
                rows.add((cur_sys, swr))
    return rows


def _extract_sys_swr_reverse(swr_dir: Path) -> set[tuple[str, str]]:
    """Parse SWR-*.md files for 'Traces up: SYS-NNN' (reverse direction)."""
    rows: set[tuple[str, str]] = set()
    if not swr_dir.is_dir():
        return rows
    for swr_file in sorted(swr_dir.glob("SWR-*.md")):
        swr_text = swr_file.read_text(encoding="utf-8")
        cur_swr = ""
        for line in swr_text.splitlines():
            m = re.match(r"^###\s+(SWR-[A-Z]+-[0-9]+):", line)
            if m:
                cur_swr = m.group(1)
                continue
            if "- **Traces up**:" in line and cur_swr:
                for sys_id in re.findall(r"(SYS-[0-9]+)", line):
                    rows.add((sys_id, cur_swr))
    return rows


def extract_sys_swr(
    sysreq_text: str, swr_dir: Path, direction: str = "both"
) -> list[tuple[str, str]]:
    rows: set[tuple[str, str]] = set()
    if direction in ("forward", "both"):
        rows |= _extract_sys_swr_forward(sysreq_text)
    if direction in ("reverse", "both"):
        rows |= _extract_sys_swr_reverse(swr_dir)
    return sorted(rows, key=lambda x: (num_suffix(x[0]), x[1]))


def write_csv(rows: list[tuple[str, str]], header: tuple[str, str], out_path: Path | None) -> None:
    if out_path:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        f = out_path.open("w", newline="", encoding="utf-8")
    else:
        import sys

        f = sys.stdout
    with f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Scoped trace extraction to avoid large-context AI runs."
    )
    parser.add_argument("--layer", required=True, choices=["stk-sys", "sys-swr"])
    parser.add_argument("--from", dest="from_id")
    parser.add_argument("--to", dest="to_id")
    parser.add_argument("--out")
    parser.add_argument(
        "--direction",
        choices=["forward", "reverse", "both"],
        default="both",
        help="forward=parent doc only, reverse=child docs only, both=merge (default: both)",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    sysreq = root / "docs" / "aspice" / "system" / "system-requirements.md"
    if not sysreq.exists():
        raise SystemExit(f"Missing file: {sysreq}")

    text = sysreq.read_text(encoding="utf-8")

    if args.layer == "stk-sys":
        rows = [
            r
            for r in extract_stk_sys(text, args.direction)
            if in_range(r[0], args.from_id, args.to_id)
        ]
        header = ("STK_ID", "SYS_ID")
    else:
        swr_dir = root / "docs" / "aspice" / "software" / "sw-requirements"
        rows = [
            r
            for r in extract_sys_swr(text, swr_dir, args.direction)
            if in_range(r[0], args.from_id, args.to_id)
        ]
        header = ("SYS_ID", "SWR_ID")

    out_path = (root / args.out) if args.out else None
    write_csv(rows, header, out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
