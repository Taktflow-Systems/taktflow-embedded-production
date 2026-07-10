"""Contracts for deterministic DBC conversion-assumptions reports."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import pytest

TOOLS_DIR = Path(__file__).resolve().parents[2]
REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(TOOLS_DIR))

from conversion_assumptions import AssumptionsCollector  # noqa: E402
from pipeline_diagnostics import DiagnosticSeverity, PipelineDiagnostic  # noqa: E402
from tools.ci import run_dbc_corpus as corpus  # noqa: E402


CONVERTER = REPO_ROOT / "tools" / "arxml" / "dbc2arxml.py"
FIXTURES = Path(__file__).with_name("fixtures") / "assumptions"
GOLDEN = Path(__file__).with_name("golden") / "assumptions" / "minimal_defaults.assumptions.md"


def _run_converter(dbc: Path, output: Path, model: Path | None = None):
    command = [sys.executable, str(CONVERTER), str(dbc), str(output)]
    if model is not None:
        command.append(str(model))
    return subprocess.run(
        command,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )


def _report_path(output: Path) -> Path:
    return output / "TaktflowSystem.arxml.assumptions.md"


def test_renderer_is_sorted_deterministic_and_scrubs_host_paths(tmp_path):
    collector = AssumptionsCollector(tmp_path / "inputs" / "vehicle.dbc")
    collector.record_convention("Zeta", "message", False, 0)
    collector.record_convention("Alpha", "message", True, 2)
    collector.record_default("message:Zulu", "Cycle", 0, "missing value")
    collector.record_default("message:Alpha", "Cycle", 10, "fallback")
    collector.record_diagnostics(
        [
            PipelineDiagnostic(
                "DBC004",
                DiagnosticSeverity.WARNING,
                "attribute 'GenUnused'",
                "not interpreted",
                path=str(tmp_path / "inputs" / "vehicle.dbc"),
                line=12,
                column=4,
            )
        ]
    )

    first = collector.render()
    second = collector.render()

    assert first == second
    assert str(tmp_path) not in first
    assert first.index("`Alpha`") < first.index("`Zeta`")
    assert first.index("`message:Alpha`") < first.index("`message:Zulu`")
    assert "vehicle.dbc:12:4" in first


def test_atomic_failure_preserves_existing_report(tmp_path, monkeypatch):
    report = tmp_path / "output.arxml.assumptions.md"
    report.write_bytes(b"known-good\n")
    collector = AssumptionsCollector("input.dbc")

    def fail_replace(_source, _target):
        raise OSError("simulated replacement failure")

    monkeypatch.setattr("conversion_assumptions.os.replace", fail_replace)

    with pytest.raises(OSError, match="simulated replacement failure"):
        collector.write_atomic(report)

    assert report.read_bytes() == b"known-good\n"
    assert not list(tmp_path.glob(".output.arxml.assumptions.md.*.tmp"))


def test_end_to_end_report_matches_golden_and_is_byte_identical(tmp_path):
    dbc = FIXTURES / "minimal_defaults.dbc"
    first_output = tmp_path / "first"
    second_output = tmp_path / "second"

    first = _run_converter(dbc, first_output)
    second = _run_converter(dbc, second_output)

    assert first.returncode == 0, first.stdout
    assert second.returncode == 0, second.stdout
    first_bytes = _report_path(first_output).read_bytes()
    second_bytes = _report_path(second_output).read_bytes()
    assert first_bytes == second_bytes
    assert first_bytes == GOLDEN.read_bytes()


def test_explicit_attributes_change_report_and_timing_decision(tmp_path):
    defaults_output = tmp_path / "defaults"
    explicit_output = tmp_path / "explicit"

    defaults = _run_converter(FIXTURES / "minimal_defaults.dbc", defaults_output)
    explicit = _run_converter(FIXTURES / "minimal_explicit.dbc", explicit_output)

    assert defaults.returncode == 0, defaults.stdout
    assert explicit.returncode == 0, explicit.stdout
    defaults_report = _report_path(defaults_output).read_text(encoding="utf-8")
    explicit_report = _report_path(explicit_output).read_text(encoding="utf-8")
    assert defaults_report != explicit_report
    assert "No timing specification emitted" in defaults_report
    assert "Cyclic timing emitted every 20 ms" in explicit_report
    assert "`GenMsgCycleTime`" in defaults_report
    assert "`GenMsgSendType`" in explicit_report


def test_sidecar_values_are_object_addressed_overrides(tmp_path):
    output = tmp_path / "with-sidecar"
    completed = _run_converter(
        FIXTURES / "minimal_explicit.dbc",
        output,
        FIXTURES / "minimal_sidecar.json",
    )

    assert completed.returncode == 0, completed.stdout
    report = _report_path(output).read_text(encoding="utf-8")
    assert "`ECU:ECU_A/runnable:Swc_Com_Main`" in report
    assert "`period_ms`" in report
    assert "`20`" in report
    assert "sidecar supplies runnable timing" in report
    assert "minimal_sidecar.json" in report


def test_failed_conversion_does_not_create_or_replace_report(tmp_path):
    output = tmp_path / "failed"
    output.mkdir()
    report = _report_path(output)
    report.write_bytes(b"known-good\n")
    malformed = tmp_path / "malformed.dbc"
    malformed.write_text("not a DBC", encoding="utf-8")

    completed = _run_converter(malformed, output)

    assert completed.returncode != 0
    assert report.read_bytes() == b"known-good\n"
    assert not list(output.glob(".TaktflowSystem.arxml.assumptions.md.*.tmp"))


def test_corpus_classifies_missing_assumptions_report_as_crash(
    tmp_path, monkeypatch
):
    dbc = tmp_path / "valid.dbc"
    dbc.write_text("VERSION \"\"\n", encoding="utf-8")
    item = corpus.CorpusInput("synthetic", dbc.name, dbc)
    monkeypatch.setattr(corpus, "preflight_reason", lambda _path: None)
    monkeypatch.setattr(
        corpus,
        "run_converter",
        lambda *_args: corpus.ConverterRun(0, "", False),
    )

    result = corpus.run_input(
        item, "must-not-crash", tmp_path, Path("converter.py"), 5
    )

    assert result.status == "crashed"
    assert result.reason == "converter emitted no assumptions report"
