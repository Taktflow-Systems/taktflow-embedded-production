"""odx-gen CLI.

Examples:
    python -m odx_gen --list                       # discover ECUs and print
    python -m odx_gen cvc                          # build firmware/ecu/cvc/odx/cvc.pdx
    python -m odx_gen cvc --dry-run                # extract model and print as JSON
    python -m odx_gen --all                        # build PDX for every discovered ECU
    python -m odx_gen cvc -o out.pdx               # custom output path
    python -m odx_gen validate-dtc-catalog FILE    # validate a DTC catalog YAML
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def _default_output_path(repo_root: Path, ecu: str) -> Path:
    return repo_root / "firmware" / "ecu" / ecu / "odx" / f"{ecu}.pdx"


def _model_to_json(model) -> str:
    return json.dumps(model.to_dict(), indent=2, default=str)


def _cmd_validate_dtc_catalog(argv: list[str]) -> int:
    """Validate a DTC catalog YAML/JSON against the schema.

    Usage: python -m odx_gen validate-dtc-catalog <path>

    Exit codes:
        0  - file exists and validates
        2  - usage error (missing path, no such file, bad arg shape)
        3  - file fails schema validation
    """
    parser = argparse.ArgumentParser(
        prog="odx-gen validate-dtc-catalog",
        description=(
            "Validate a DTC catalog YAML/JSON against the odx-gen "
            "DTC catalog JSON Schema."
        ),
    )
    parser.add_argument(
        "path",
        type=Path,
        help="Path to a DTC catalog YAML or JSON file.",
    )
    try:
        args = parser.parse_args(argv)
    except SystemExit as exc:
        # argparse already wrote usage to stderr; just return code 2
        return int(exc.code) if isinstance(exc.code, int) else 2

    # Defer the import so this subcommand has no odxtools dependency.
    from .schemas.dtc_catalog import (
        DtcCatalogValidationError,
        load_dtc_catalog,
    )

    try:
        catalog = load_dtc_catalog(args.path)
    except FileNotFoundError as exc:
        print(f"error: DTC catalog not found: {exc}", file=sys.stderr)
        return 2
    except DtcCatalogValidationError as exc:
        print(f"error: invalid DTC catalog at {args.path}:", file=sys.stderr)
        for line in exc.errors or [str(exc)]:
            print(f"  - {line}", file=sys.stderr)
        return 3

    print(
        f"OK: {args.path} is a valid DTC catalog "
        f"(version={catalog.version!r}, source={catalog.source!r}, "
        f"{len(catalog.entries)} entries)"
    )
    return 0


def main(argv: list[str] | None = None) -> int:
    if argv is None:
        argv = sys.argv[1:]

    # Early dispatch: subcommand-style entries that should NOT require
    # odxtools or any of the firmware-extraction stack.
    if argv and argv[0] == "validate-dtc-catalog":
        return _cmd_validate_dtc_catalog(argv[1:])

    # Lazy import: keep the firmware-extraction stack out of the
    # import path for lightweight subcommands, and keep odxtools out
    # entirely for the --emit=mdd code path (which does not need PDX).
    from .discover import discover_ecus, find_repo_root
    from .extract import extract_ecu

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
        "--emit",
        choices=("pdx", "mdd"),
        default="pdx",
        help=(
            "Output format. 'pdx' (default) writes a PDX via odxtools. "
            "'mdd' writes a build/mdd/<ecu>.mdd FlatBuffers-compatible "
            "MDD file tracking the upstream Eclipse OpenSOVD CDA "
            "cda-database schema (Phase 5 Line B D5, STUB mode until "
            "flatc is wired in)."
        ),
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Output path (single ECU only). Default: "
             "firmware/ecu/<ecu>/odx/<ecu>.pdx (pdx mode) or "
             "build/mdd/<ecu>.mdd (mdd mode)",
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
                if args.emit == "mdd":
                    from .build_mdd import write_mdd
                    out = repo_root / "build" / "mdd" / f"{ecu}.mdd"
                    write_mdd(model, out, repo_root)
                    print(f"{ecu}: wrote {out} ({out.stat().st_size} bytes, MDD stub)")
                    continue
                from .build_pdx import write_pdx
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

    if args.emit == "mdd":
        from .build_mdd import write_mdd
        out = args.output or (repo_root / "build" / "mdd" / f"{args.ecu}.mdd")
        write_mdd(model, out, repo_root)
        print(f"wrote {out} ({out.stat().st_size} bytes, MDD stub)")
        if model.unresolved_todos:
            print(f"({len(model.unresolved_todos)} TODOs in model)")
        return 0

    from .build_pdx import write_pdx
    out = args.output or _default_output_path(repo_root, args.ecu)
    write_pdx(model, out)
    print(f"wrote {out} ({out.stat().st_size} bytes)")
    if model.unresolved_todos:
        print(f"({len(model.unresolved_todos)} TODOs in model)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
