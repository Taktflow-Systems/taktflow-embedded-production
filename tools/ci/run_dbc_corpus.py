#!/usr/bin/env python3
"""Run dbc2arxml against the licensed public DBC regression corpus.

``--must-not-crash`` is the compatibility gate: inputs rejected by cantools are
reported as skipped with a typed reason, while converter failures are fatal.
``--strict`` additionally strict-loads each emitted ARXML with autosar-data.
An opendbc checkout is included only when explicitly supplied with
``--opendbc``; no network access occurs inside this harness.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

import autosar_data as ad
import cantools


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CORPUS = REPO_ROOT / "test" / "framework" / "corpus" / "dbc"
DEFAULT_REPORT = REPO_ROOT / "test" / "reports" / "dbc_corpus_report.md"
DEFAULT_CONVERTER = REPO_ROOT / "tools" / "arxml" / "dbc2arxml.py"


@dataclass(frozen=True)
class CorpusInput:
    """One DBC input with a portable display name."""

    source: str
    name: str
    path: Path


@dataclass(frozen=True)
class CorpusResult:
    """Stable per-file outcome used by stdout and Markdown reporting."""

    source: str
    name: str
    status: str
    reason: str


def discover_inputs(corpus_dir: Path, opendbc_dir: Path | None = None) -> list[CorpusInput]:
    """Discover vendored and explicitly enabled opendbc files deterministically."""
    inputs = [
        CorpusInput("vendored", path.relative_to(corpus_dir).as_posix(), path)
        for path in corpus_dir.rglob("*.dbc")
        if path.is_file()
    ]

    if opendbc_dir is not None:
        candidates = (
            opendbc_dir / "opendbc" / "dbc",
            opendbc_dir / "dbc",
            opendbc_dir,
        )
        search_root = next((path for path in candidates if path.is_dir()), opendbc_dir)
        inputs.extend(
            CorpusInput("opendbc", path.relative_to(opendbc_dir).as_posix(), path)
            for path in search_root.rglob("*.dbc")
            if path.is_file()
        )

    return sorted(inputs, key=lambda item: (item.source, item.name))


def preflight_reason(path: Path) -> str | None:
    """Return a typed skip reason when cantools cannot parse an input."""
    try:
        cantools.database.load_file(str(path))
    except Exception as exc:  # The library wraps format-specific parse errors.
        return f"cantools rejected input: {type(exc).__name__}"
    return None


def run_converter(converter: Path, dbc: Path, output_dir: Path, timeout: int) -> int:
    """Run the production converter and return its process exit status."""
    completed = subprocess.run(
        [sys.executable, str(converter), str(dbc), str(output_dir)],
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=timeout,
        check=False,
    )
    return completed.returncode


def strict_validation_reason(path: Path) -> str | None:
    """Return a portable failure reason, or None for strict-valid ARXML."""
    try:
        model = ad.AutosarModel()
        _, warnings = model.load_file(str(path), strict=True)
        if warnings:
            return f"strict load warnings: {len(warnings)}"
        reference_errors = model.check_references()
        if reference_errors:
            return f"reference validation failed: {len(reference_errors)}"
    except Exception as exc:
        return f"strict load failed: {type(exc).__name__}"
    return None


def run_input(
    item: CorpusInput,
    mode: str,
    work_root: Path,
    converter: Path,
    timeout: int,
) -> CorpusResult:
    """Convert and optionally strict-validate one corpus item."""
    skip_reason = preflight_reason(item.path)
    if skip_reason is not None:
        return CorpusResult(item.source, item.name, "skipped-with-reason", skip_reason)

    label = f"{item.source}__{item.name}".replace("/", "__").replace("\\", "__")
    output_dir = work_root / label

    try:
        returncode = run_converter(converter, item.path, output_dir, timeout)
    except subprocess.TimeoutExpired:
        return CorpusResult(
            item.source,
            item.name,
            "crashed",
            f"converter timed out after {timeout}s",
        )
    except Exception as exc:
        return CorpusResult(
            item.source,
            item.name,
            "crashed",
            f"converter launch failed: {type(exc).__name__}",
        )

    if returncode != 0:
        return CorpusResult(
            item.source,
            item.name,
            "crashed",
            f"converter exited {returncode}",
        )

    arxml_path = output_dir / "TaktflowSystem.arxml"
    if not arxml_path.is_file():
        return CorpusResult(item.source, item.name, "crashed", "converter emitted no ARXML")

    if mode == "strict":
        validation_reason = strict_validation_reason(arxml_path)
        if validation_reason is not None:
            return CorpusResult(item.source, item.name, "crashed", validation_reason)
        reason = "strict validation passed"
    else:
        reason = "converter completed"

    return CorpusResult(item.source, item.name, "converted", reason)


def _escape_cell(value: str) -> str:
    return value.replace("|", "\\|").replace("\r", " ").replace("\n", " ")


def report_lines(results: list[CorpusResult], mode: str) -> list[str]:
    """Build deterministic Markdown with no host paths or timestamps."""
    ordered = sorted(results, key=lambda result: (result.source, result.name))
    converted = sum(result.status == "converted" for result in ordered)
    skipped = sum(result.status == "skipped-with-reason" for result in ordered)
    crashed = sum(result.status == "crashed" for result in ordered)
    lines = [
        "# DBC Corpus Report",
        "",
        f"Mode: `{mode}`",
        "",
        "| Source | File | Result | Reason |",
        "|---|---|---|---|",
    ]
    lines.extend(
        "| {source} | {name} | {status} | {reason} |".format(
            source=_escape_cell(result.source),
            name=_escape_cell(result.name),
            status=_escape_cell(result.status),
            reason=_escape_cell(result.reason),
        )
        for result in ordered
    )
    lines.extend(
        [
            "",
            f"Converted: {converted}; skipped: {skipped}; crashed: {crashed}.",
            "",
        ]
    )
    return lines


def write_report(results: list[CorpusResult], path: Path, mode: str) -> None:
    """Write the deterministic corpus report with LF line endings."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as report:
        report.write("\n".join(report_lines(results, mode)))


def exit_code_for(results: list[CorpusResult]) -> int:
    """A skip is evidence; only a crash fails the requested mode."""
    return 1 if any(result.status == "crashed" for result in results) else 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run DBC-to-ARXML corpus regressions")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--must-not-crash", dest="mode", action="store_const", const="must-not-crash")
    mode.add_argument("--strict", dest="mode", action="store_const", const="strict")
    parser.add_argument("--corpus", type=Path, default=DEFAULT_CORPUS)
    parser.add_argument("--opendbc", type=Path, help="Optional local opendbc checkout")
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    parser.add_argument("--converter", type=Path, default=DEFAULT_CONVERTER)
    parser.add_argument("--timeout", type=int, default=30, help="Per-file timeout in seconds")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if not args.corpus.is_dir():
        parser.error(f"corpus directory not found: {args.corpus}")
    if args.opendbc is not None and not args.opendbc.is_dir():
        parser.error(f"opendbc checkout not found: {args.opendbc}")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")

    inputs = discover_inputs(args.corpus, args.opendbc)
    if not inputs:
        parser.error("no DBC files discovered")

    with tempfile.TemporaryDirectory(prefix="dbc-corpus-") as temp_dir:
        work_root = Path(temp_dir)
        results = [
            run_input(item, args.mode, work_root, args.converter, args.timeout)
            for item in inputs
        ]

    write_report(results, args.report, args.mode)
    print("\n".join(report_lines(results, args.mode)))
    print(f"Report: {args.report}")
    return exit_code_for(results)


if __name__ == "__main__":
    raise SystemExit(main())
