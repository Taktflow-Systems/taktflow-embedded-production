#!/usr/bin/env python3
"""ARXML round-trip integrity validation using armodel + autosar-data."""

from __future__ import annotations

import argparse
from pathlib import Path

import armodel
import autosar_data as ad


def resolve_arxml_files(patterns: list[str]) -> list[Path]:
    files: list[Path] = []
    for pattern in patterns:
        if any(ch in pattern for ch in ("*", "?", "[")):
            files.extend(sorted(Path().glob(pattern)))
        else:
            files.append(Path(pattern))
    return [p for p in files if p.is_file()]


def snapshot_arxml(path: Path) -> tuple[int, int, int, set[str]]:
    model = ad.AutosarModel()
    model.load_file(str(path), strict=True)
    element_count = sum(1 for _ in model.elements_dfs)
    identifiable_paths = {p for p, _ in model.identifiable_elements}
    ref_errors = len(model.check_references())
    return element_count, len(identifiable_paths), ref_errors, identifiable_paths


def roundtrip_with_armodel(src: Path, dst: Path) -> None:
    doc = armodel.AUTOSAR()
    armodel.ARXMLParser().load(str(src), doc)
    armodel.ARXMLWriter().save(str(dst), doc)


def validate_roundtrip(path: Path) -> None:
    out_dir = Path("tmp/armodel_roundtrip")
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / path.name
    before = snapshot_arxml(path)
    roundtrip_with_armodel(path, out)
    after = snapshot_arxml(out)

    before_elements, before_ident_cnt, before_ref_err, before_paths = before
    after_elements, after_ident_cnt, after_ref_err, after_paths = after

    if before_ref_err != 0 or after_ref_err != 0:
        raise ValueError(
            f"Reference check failed for {path} (before={before_ref_err}, after={after_ref_err})."
        )
    if before_ident_cnt != after_ident_cnt:
        raise ValueError(
            f"Identifiable count mismatch for {path} (before={before_ident_cnt}, after={after_ident_cnt})."
        )
    if before_paths != after_paths:
        missing = sorted(before_paths - after_paths)
        extra = sorted(after_paths - before_paths)
        raise ValueError(
            f"Identifiable path set changed for {path} "
            f"(missing={len(missing)}, extra={len(extra)})."
        )
    if before_elements != after_elements:
        print(
            f"[WARN] Non-identifiable element count changed for {path} "
            f"(before={before_elements}, after={after_elements})."
        )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Validate ARXML round-trip integrity via armodel parse/write/re-parse."
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
        validate_roundtrip(path)
        print(f"[OK] Round-trip integrity passed: {path}")

    print(f"Round-trip validation passed for {len(arxml_files)} ARXML file(s).")


if __name__ == "__main__":
    main()
