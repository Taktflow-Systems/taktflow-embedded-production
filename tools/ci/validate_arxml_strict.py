#!/usr/bin/env python3
"""Strict ARXML validation using autosar-data."""

from __future__ import annotations

import argparse
from pathlib import Path

import autosar_data as ad


def resolve_arxml_files(patterns: list[str]) -> list[Path]:
    files: list[Path] = []
    for pattern in patterns:
        if any(ch in pattern for ch in ("*", "?", "[")):
            files.extend(sorted(Path().glob(pattern)))
        else:
            files.append(Path(pattern))
    return [p for p in files if p.is_file()]


def validate_strict(path: Path) -> tuple[int, int]:
    model = ad.AutosarModel()
    _, warnings = model.load_file(str(path), strict=True)
    ref_errors = model.check_references()
    if ref_errors:
        raise ValueError(
            f"Reference validation failed for {path}: {len(ref_errors)} invalid references."
        )
    return len(warnings), len(list(model.identifiable_elements))


def main() -> None:
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

    for path in arxml_files:
        warnings, identifiable = validate_strict(path)
        print(f"[OK] {path} (warnings={warnings}, identifiable={identifiable})")

    print(f"Strict validation passed for {len(arxml_files)} ARXML file(s).")


if __name__ == "__main__":
    main()
