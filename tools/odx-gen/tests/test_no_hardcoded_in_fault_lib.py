"""Hardcoded-literal regression for libs/fault_lib/.

Same pattern as test_parser_dcm_cfg.test_no_hardcoded_data_leaks_
through_parser: every forbidden substring is a concrete fault code,
severity literal, or sample payload that MUST live only in
libs/fault_lib/testdata/ side files, never in a C header, C source,
or Unity test source under libs/fault_lib/.

The forbidden list is derived from wire_records.csv at test time —
the test itself reads the CSV and constructs the forbidden list
from it, so there is nothing to manually keep in sync.
"""

from __future__ import annotations

import csv
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
FAULT_LIB_ROOT = REPO_ROOT / "libs" / "fault_lib"
CSV_PATH = FAULT_LIB_ROOT / "testdata" / "wire_records.csv"

# C files that MUST be free of concrete test data. Test sources are
# excluded because they intentionally parse the CSV and load fault
# values out of it into local variables — but even the test code is
# forbidden to INLINE a literal string / id from the CSV.
SCANNED_C_GLOBS = [
    FAULT_LIB_ROOT / "include",
    FAULT_LIB_ROOT / "src",
    FAULT_LIB_ROOT / "tests",
    REPO_ROOT / "tests" / "interop" / "producer.c",
]


def _collect_forbidden_substrings() -> set[str]:
    """Return every CSV value that represents concrete test data."""
    forbidden: set[str] = set()
    with CSV_PATH.open("r", encoding="utf-8") as fp:
        reader = csv.DictReader(
            (line for line in fp if line.strip() and not line.lstrip().startswith("#"))
        )
        for row in reader:
            component = row["component"].strip()
            id_str = row["id"].strip()
            severity = row["severity_code"].strip()
            timestamp = row["timestamp_ms"].strip()
            meta = row["meta_json"].strip()
            # Component names and full meta payloads are the load-bearing
            # literals — forbid them as inline substrings.
            if len(component) >= 3:
                forbidden.add(component)
            if meta:
                # Forbid any meta payload longer than a few bytes so we
                # do not flag things like '{}' which might legitimately
                # appear in structural code.
                if len(meta) >= 6:
                    forbidden.add(meta)
            # Large integer ids that would be wildly unlikely to appear
            # by accident — only flag multi-digit, non-trivial values.
            if len(id_str) >= 3 and id_str not in {"100", "255"}:
                forbidden.add(id_str)
            # Timestamps that are distinctive enough to be caught.
            if len(timestamp) >= 10:
                forbidden.add(timestamp)
            # Severity literals are single digits and would false-positive
            # on common constants; skip.
            _ = severity
    return forbidden


def _iter_c_files() -> list[Path]:
    files: list[Path] = []
    for root in SCANNED_C_GLOBS:
        if root.is_file():
            files.append(root)
            continue
        if not root.exists():
            continue
        for p in root.rglob("*.c"):
            files.append(p)
        for p in root.rglob("*.h"):
            files.append(p)
    return files


def test_csv_has_rows() -> None:
    assert CSV_PATH.exists(), f"missing test vector CSV: {CSV_PATH}"
    forbidden = _collect_forbidden_substrings()
    assert forbidden, "CSV produced no forbidden substrings; regression is toothless"


def test_no_hardcoded_fault_data_in_fault_lib_sources() -> None:
    """Every C file under libs/fault_lib/{include,src,tests} and
    tests/interop/producer.c must be free of inline CSV values."""
    forbidden = _collect_forbidden_substrings()
    files = _iter_c_files()
    assert files, "no C files scanned; regression would silently pass"
    leaks: list[tuple[Path, str]] = []
    for path in files:
        text = path.read_text(encoding="utf-8", errors="ignore")
        for needle in forbidden:
            if needle in text:
                leaks.append((path, needle))
    assert not leaks, (
        "hardcoded fault data leaked into C sources:\n  "
        + "\n  ".join(f"{p}: {n!r}" for p, n in leaks)
    )
