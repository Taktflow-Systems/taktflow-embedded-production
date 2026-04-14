"""odx-gen CLI.

Examples:
    python -m odx_gen --list             # discover ECUs and print
    python -m odx_gen cvc                 # build firmware/ecu/cvc/odx/cvc.pdx
    python -m odx_gen cvc --dry-run       # extract model and print as JSON
    python -m odx_gen --all               # build PDX for every discovered ECU
    python -m odx_gen cvc -o out.pdx      # custom output path
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .build_pdx import write_pdx
from .discover import discover_ecus, find_repo_root
from .extract import extract_ecu


def _default_output_path(repo_root: Path, ecu: str) -> Path:
    return repo_root / "firmware" / "ecu" / ecu / "odx" / f"{ecu}.pdx"


def _model_to_json(model) -> str:
    return json.dumps(model.to_dict(), indent=2, default=str)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="odx-gen",
        description="Generate ODX PDX files from Taktflow firmware C sources",
    )
    parser.add_argument(
        "ecu",
        nargs="?",
        help="ECU short name (e.g. cvc). Required unless --all or --list.",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List discovered ECUs and exit.",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Generate PDX files for every discovered ECU.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the parsed EcuDiagnosticModel as JSON; do not write a PDX.",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Output PDX path (single ECU only). Default: "
             "firmware/ecu/<ecu>/odx/<ecu>.pdx",
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=None,
        help="Path to the firmware repository root. Auto-detected if omitted.",
    )

    args = parser.parse_args(argv)

    try:
        repo_root = args.repo_root.resolve() if args.repo_root else find_repo_root()
    except FileNotFoundError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    ecus = discover_ecus(repo_root)

    if args.list:
        if not ecus:
            print("(no ECUs discovered)")
        else:
            for name in ecus:
                print(name)
        return 0

    if args.all:
        if not ecus:
            print("error: no ECUs discovered", file=sys.stderr)
            return 2
        if args.output is not None:
            print("error: --output cannot be used with --all", file=sys.stderr)
            return 2
        rc = 0
        for ecu in ecus:
            try:
                model = extract_ecu(repo_root, ecu)
                if args.dry_run:
                    print(f"=== {ecu} ===")
                    print(_model_to_json(model))
                    continue
                out = _default_output_path(repo_root, ecu)
                write_pdx(model, out)
                print(f"{ecu}: wrote {out} ({out.stat().st_size} bytes)")
            except Exception as e:
                print(f"{ecu}: ERROR {e}", file=sys.stderr)
                rc = 1
        return rc

    if args.ecu is None:
        parser.print_usage(sys.stderr)
        print(
            "error: ECU name is required (or use --list / --all)",
            file=sys.stderr,
        )
        return 2

    if args.ecu not in ecus:
        print(
            f"error: ECU '{args.ecu}' not in discovered list: {ecus}",
            file=sys.stderr,
        )
        return 2

    try:
        model = extract_ecu(repo_root, args.ecu)
    except FileNotFoundError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    if args.dry_run:
        print(_model_to_json(model))
        return 0

    out = args.output or _default_output_path(repo_root, args.ecu)
    write_pdx(model, out)
    print(f"wrote {out} ({out.stat().st_size} bytes)")
    if model.unresolved_todos:
        print(f"({len(model.unresolved_todos)} TODOs in model)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
