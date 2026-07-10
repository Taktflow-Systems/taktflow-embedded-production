#!/usr/bin/env python3
"""Strict ARXML validation using autosar-data."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from arxml_validation import (  # noqa: E402
    StrictArxmlValidationError,
    validate_arxml_strict,
)
from pipeline_diagnostics import portable_path  # noqa: E402


def resolve_arxml_files(patterns: list[str]) -> list[Path]:
    files: list[Path] = []
    for pattern in patterns:
        if any(ch in pattern for ch in ("*", "?", "[")):
            files.extend(sorted(Path().glob(pattern)))
        else:
            files.append(Path(pattern))
    return [p for p in files if p.is_file()]


def validate_strict(path: Path) -> tuple[int, int]:
    result = validate_arxml_strict(path)
    return result.warning_count, result.identifiable_count


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Strictly validate ARXML files using autosar-data."
    )
    parser.add_argument(
        "arxml",
        nargs="+",
        help="ARXML paths or globs (example: arxml/*.arxml)",
    )
    args = parser.parse_args()

    arxml_files = resolve_arxml_files(args.arxml)
    if not arxml_files:
        raise FileNotFoundError("No ARXML files matched input arguments.")

    try:
        for path in arxml_files:
            warnings, identifiable = validate_strict(path)
            print(
                f"[OK] {portable_path(path)} "
                f"(warnings={warnings}, identifiable={identifiable})"
            )
    except StrictArxmlValidationError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    print(f"Strict validation passed for {len(arxml_files)} ARXML file(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
