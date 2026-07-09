"""Tests for the public DBC corpus regression harness."""

from __future__ import annotations

from pathlib import Path

from tools.ci import run_dbc_corpus as corpus


def test_discovery_is_sorted_and_labels_sources(tmp_path):
    vendored = tmp_path / "vendored"
    opendbc = tmp_path / "opendbc"
    vendored.mkdir()
    (opendbc / "dbc").mkdir(parents=True)
    (vendored / "z.dbc").write_text("VERSION \"\"\n", encoding="utf-8")
    (vendored / "a.dbc").write_text("VERSION \"\"\n", encoding="utf-8")
    (opendbc / "dbc" / "car.dbc").write_text("VERSION \"\"\n", encoding="utf-8")

    inputs = corpus.discover_inputs(vendored, opendbc)

    assert [(item.source, item.name) for item in inputs] == [
        ("opendbc", "dbc/car.dbc"),
        ("vendored", "a.dbc"),
        ("vendored", "z.dbc"),
    ]


def test_parse_rejection_is_a_named_skip(tmp_path, monkeypatch):
    dbc = tmp_path / "overlap.dbc"
    dbc.write_text("VERSION \"\"\n", encoding="utf-8")
    item = corpus.CorpusInput(source="vendored", name=dbc.name, path=dbc)
    monkeypatch.setattr(
        corpus,
        "preflight_reason",
        lambda _path: "cantools rejected input: UnsupportedDatabaseFormatError",
    )

    result = corpus.run_input(item, "must-not-crash", tmp_path, Path("converter.py"), 5)

    assert result.status == "skipped-with-reason"
    assert result.reason == "cantools rejected input: UnsupportedDatabaseFormatError"


def test_successful_conversion_is_reported(tmp_path, monkeypatch):
    dbc = tmp_path / "valid.dbc"
    dbc.write_text("VERSION \"\"\n", encoding="utf-8")
    item = corpus.CorpusInput(source="vendored", name=dbc.name, path=dbc)
    monkeypatch.setattr(corpus, "preflight_reason", lambda _path: None)

    def fake_converter(_converter, _dbc, output_dir, _timeout):
        output_dir.mkdir(parents=True, exist_ok=True)
        (output_dir / "TaktflowSystem.arxml").write_text("<AUTOSAR/>", encoding="utf-8")
        return 0

    monkeypatch.setattr(corpus, "run_converter", fake_converter)

    result = corpus.run_input(item, "must-not-crash", tmp_path, Path("converter.py"), 5)

    assert result.status == "converted"
    assert result.reason == "converter completed"


def test_strict_mode_surfaces_validation_failure(tmp_path, monkeypatch):
    dbc = tmp_path / "valid.dbc"
    dbc.write_text("VERSION \"\"\n", encoding="utf-8")
    item = corpus.CorpusInput(source="vendored", name=dbc.name, path=dbc)
    monkeypatch.setattr(corpus, "preflight_reason", lambda _path: None)

    def fake_converter(_converter, _dbc, output_dir, _timeout):
        output_dir.mkdir(parents=True, exist_ok=True)
        (output_dir / "TaktflowSystem.arxml").write_text("<AUTOSAR/>", encoding="utf-8")
        return 0

    monkeypatch.setattr(corpus, "run_converter", fake_converter)
    monkeypatch.setattr(corpus, "strict_validation_reason", lambda _path: "strict load failed: AutosarDataError")

    result = corpus.run_input(item, "strict", tmp_path, Path("converter.py"), 5)

    assert result.status == "crashed"
    assert result.reason == "strict load failed: AutosarDataError"


def test_report_is_deterministic_and_path_free(tmp_path):
    report = tmp_path / "report.md"
    results = [
        corpus.CorpusResult("vendored", "b.dbc", "converted", "converter completed"),
        corpus.CorpusResult(
            "vendored",
            "a.dbc",
            "skipped-with-reason",
            "cantools rejected input: ParseError",
        ),
    ]

    corpus.write_report(results, report, "must-not-crash")
    first = report.read_text(encoding="utf-8")
    corpus.write_report(list(reversed(results)), report, "must-not-crash")
    second = report.read_text(encoding="utf-8")

    assert first == second
    assert str(tmp_path) not in first
    assert "| vendored | a.dbc | skipped-with-reason |" in first
    assert "Converted: 1; skipped: 1; crashed: 0." in first


def test_exit_code_fails_only_for_crashes():
    green = [
        corpus.CorpusResult("vendored", "a.dbc", "converted", "ok"),
        corpus.CorpusResult("vendored", "b.dbc", "skipped-with-reason", "unsupported"),
    ]
    red = green + [corpus.CorpusResult("vendored", "c.dbc", "crashed", "exit 1")]

    assert corpus.exit_code_for(green) == 0
    assert corpus.exit_code_for(red) == 1
