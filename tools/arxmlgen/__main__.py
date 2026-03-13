"""
arxmlgen CLI entry point.

Usage:
    python -m tools.arxmlgen --config project.yaml
    python -m tools.arxmlgen --config project.yaml --dry-run
    python -m tools.arxmlgen --config project.yaml --ecu cvc --generator com
"""

from __future__ import annotations

import argparse
import os
import sys

from . import __version__
from .config import ConfigError, load_config
from .engine import TemplateEngine
from .generators import available_generators, get_generator
from .reader import ArxmlReadError, ArxmlReader


def main():
    parser = argparse.ArgumentParser(
        prog="arxmlgen",
        description="Project-agnostic AUTOSAR C code generator",
    )
    parser.add_argument(
        "--config", required=True, help="Path to project.yaml"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse and validate without writing files",
    )
    parser.add_argument(
        "--ecu",
        action="append",
        dest="ecus",
        help="Generate for specific ECU(s) only (repeatable)",
    )
    parser.add_argument(
        "--generator",
        action="append",
        dest="generators",
        help="Run specific generator(s) only (repeatable)",
    )
    parser.add_argument(
        "--output-dir",
        help="Override output.base_dir from config",
    )
    parser.add_argument(
        "--e2e-source",
        choices=("dbc", "sidecar"),
        help="E2E data ID source: 'dbc' (from DBC E2E_DataID attribute) "
             "or 'sidecar' (from ecu_sidecar.yaml pdu_e2e_map). "
             "Overrides input.e2e_source in project.yaml.",
    )
    parser.add_argument(
        "--verbose", action="store_true", help="Detailed output"
    )
    parser.add_argument(
        "--quiet", action="store_true", help="Errors and summary only"
    )
    parser.add_argument(
        "--version", action="version", version=f"arxmlgen {__version__}"
    )

    args = parser.parse_args()

    # Banner
    if not args.quiet:
        print(f"arxmlgen v{__version__}")

    # Load config
    try:
        config = load_config(args.config)
    except ConfigError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    if not args.quiet:
        print(f"Project: {config.name} v{config.version}")

    # Override output dir
    if args.output_dir:
        config.output.base_dir = args.output_dir

    # Override E2E source
    e2e_source_arg = getattr(args, "e2e_source", None)
    if e2e_source_arg:
        config.e2e_source = e2e_source_arg

    # Read ARXML
    if not args.quiet:
        print(f"\nReading ARXML...")
    try:
        reader = ArxmlReader(config)
        model = reader.read()
    except ArxmlReadError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    # Print model summary
    if not args.quiet:
        print(f"\n{'=' * 60}")
        print(f"Model Summary")
        print(f"{'=' * 60}")
        print(f"  Project:          {model.name}")
        print(f"  ECUs:             {len(model.ecus)}")
        print(f"  Platform types:   {len(model.platform_types)}")
        print(f"  S/R interfaces:   {len(model.sr_interfaces)}")
        print()

        for ecu_name, ecu in sorted(model.ecus.items()):
            print(f"  {ecu.prefix} ({ecu_name}):")
            print(f"    TX PDUs:        {len(ecu.tx_pdus)}")
            print(f"    RX PDUs:        {len(ecu.rx_pdus)}")
            print(f"    Signals:        {len(ecu.all_signals)}")
            print(f"    SWCs:           {len(ecu.swcs)}")
            runnables = sum(len(s.runnables) for s in ecu.swcs)
            ports = sum(len(s.ports) for s in ecu.swcs)
            print(f"    Runnables:      {runnables}")
            print(f"    Ports:          {ports}")
            print(f"    RTE signals:    {len(ecu.rte_signal_map)}")
            if ecu.dtc_events:
                print(f"    DTCs:           {len(ecu.dtc_events)}")
            if ecu.enums:
                print(f"    Enums:          {len(ecu.enums)}")
            if ecu.thresholds:
                print(f"    Thresholds:     {len(ecu.thresholds)}")
            print()

        print(f"  Totals:")
        print(f"    Signals:        {model.total_signals}")
        print(f"    PDUs:           {model.total_pdus}")
        print(f"    SWCs:           {model.total_swcs}")
        print(f"    Runnables:      {model.total_runnables}")
        print(f"{'=' * 60}")

    if args.dry_run:
        print("\nDry run complete — no files written.")
        sys.exit(0)

    # Filter ECUs if specified
    target_ecus = args.ecus or list(model.ecus.keys())
    target_ecus = [e.lower() for e in target_ecus]

    # Filter generators if specified
    registered = available_generators()
    target_gens = args.generators or list(config.generators.keys())

    # Initialize template engine
    engine = TemplateEngine(config)

    # Run generators
    total_written = 0
    total_skipped = 0
    total_created = 0

    for ecu_name in target_ecus:
        if ecu_name not in model.ecus:
            if not args.quiet:
                print(f"\n  WARNING: ECU '{ecu_name}' not in model — skipped")
            continue

        ecu = model.ecus[ecu_name]
        ecu_cfg = config.ecus.get(ecu_name)

        if not args.quiet:
            print(f"\nGenerating for {ecu.prefix} ({ecu_name})")

        for gen_name in target_gens:
            gen_config = config.generators.get(gen_name)
            if not gen_config or not gen_config.enabled:
                continue

            # Check ECU include_in filter
            if ecu_cfg and ecu_cfg.include_in and gen_name not in ecu_cfg.include_in:
                continue

            generator = get_generator(gen_name)
            if not generator:
                if args.verbose:
                    print(f"  SKIP    {gen_name} (not implemented yet)")
                continue

            results = generator.generate(ecu, gen_config, engine)
            for r in results:
                if r.action == "WRITE":
                    total_written += 1
                elif r.action == "CREATE":
                    total_created += 1
                elif r.action == "SKIP":
                    total_skipped += 1

                if not args.quiet:
                    rel = os.path.relpath(r.path, config.project_root)
                    print(f"  {r.action:6s}  {rel}")

    # Summary
    if not args.quiet:
        total = total_written + total_created
        print(f"\nDone. {total} files written, {total_skipped} skipped.")


if __name__ == "__main__":
    main()
