"""Byte-level interop test — Phase 3 Line B D6 gate.

This test is the meaningful Phase 3 Line B gate. It builds the C
producer bridge against libfaultlib, runs it against
libs/fault_lib/testdata/wire_records.csv, then runs the Rust
sink_consumer example (documented cross-line exception per the
Phase 3 Line B prompt D6) against the same CSV, and asserts that
every ``ROW <idx> <hex>`` line matches byte-for-byte.

The test deliberately avoids standing up a live Unix socket: byte
equality at the encoder level is a strictly stronger guarantee than
end-to-end socket round-trip (a socket gate could pass even if the
decoder is forgiving). If a future phase needs a live-socket gate,
extend sink_consumer.rs with a --serve mode.

No hardcoded fault codes, severities, or payloads live in this file.
Every test vector comes from the CSV.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
CSV_PATH = REPO_ROOT / "libs" / "fault_lib" / "testdata" / "wire_records.csv"
OPENSOVD_ROOT = REPO_ROOT.parent / "eclipse-opensovd" / "opensovd-core"


def _have_cargo() -> bool:
    return shutil.which("cargo") is not None


def _have_cc() -> bool:
    return (shutil.which("gcc") is not None) or (shutil.which("cc") is not None)


def _normalize(lines: str) -> list[str]:
    return [line.strip() for line in lines.replace("\r", "").splitlines() if line.strip()]


@pytest.fixture(scope="module")
def c_producer_path(tmp_path_factory: pytest.TempPathFactory) -> Path:
    """Compile tests/interop/producer.c against libfaultlib sources."""
    if not _have_cc():
        pytest.skip("no C compiler available on PATH")
    out_dir = REPO_ROOT / "build" / "posix" / "tests" / "interop"
    out_dir.mkdir(parents=True, exist_ok=True)
    binary = out_dir / "producer"
    srcs = [
        REPO_ROOT / "tests" / "interop" / "producer.c",
        REPO_ROOT / "libs" / "fault_lib" / "src" / "postcard_c.c",
        REPO_ROOT / "libs" / "fault_lib" / "src" / "wire_fault_record.c",
        REPO_ROOT / "libs" / "fault_lib" / "src" / "fault_lib.c",
    ]
    cmd = [
        "gcc",
        "-Wall", "-Wextra", "-Werror", "-pedantic", "-std=c99",
        "-D_DEFAULT_SOURCE",
        "-I" + str(REPO_ROOT / "libs" / "fault_lib" / "include"),
        "-I" + str(REPO_ROOT / "libs" / "fault_lib" / "src"),
        *[str(s) for s in srcs],
        "-o", str(binary),
    ]
    subprocess.run(cmd, check=True, cwd=str(REPO_ROOT))
    # MinGW adds .exe
    if not binary.exists() and binary.with_suffix(".exe").exists():
        return binary.with_suffix(".exe")
    return binary


@pytest.fixture(scope="module")
def rust_sink_consumer() -> None:
    """Ensure the cross-line Rust example builds. Returns None; the
    cargo invocation is repeated at run time because cargo is
    incremental and fast."""
    if not _have_cargo():
        pytest.skip("cargo unavailable")
    if not OPENSOVD_ROOT.exists():
        pytest.skip(f"opensovd-core not found at {OPENSOVD_ROOT}")
    subprocess.run(
        ["cargo", "build", "--example", "sink_consumer", "-p", "fault-sink-unix"],
        check=True,
        cwd=str(OPENSOVD_ROOT),
    )


def _run_c_producer(binary: Path, csv: Path) -> list[str]:
    result = subprocess.run(
        [str(binary), "--dump-frames", str(csv)],
        capture_output=True,
        text=True,
        check=True,
        cwd=str(REPO_ROOT),
    )
    return _normalize(result.stdout)


def _run_rust_producer(csv: Path) -> list[str]:
    result = subprocess.run(
        [
            "cargo", "run",
            "--example", "sink_consumer",
            "-p", "fault-sink-unix",
            "--quiet", "--",
            "--dump-frames", str(csv),
        ],
        capture_output=True,
        text=True,
        check=True,
        cwd=str(OPENSOVD_ROOT),
    )
    return _normalize(result.stdout)


def test_csv_present() -> None:
    assert CSV_PATH.exists(), f"test vector CSV missing: {CSV_PATH}"


def test_byte_level_interop_rust_vs_c(
    c_producer_path: Path,
    rust_sink_consumer: None,
) -> None:
    """Every row in wire_records.csv must encode identically on both
    the C `wire_fault_record_encode_frame` and the Rust
    `fault_sink_unix::codec::encode_frame` paths. This is the Phase 3
    Line B gate."""
    rust_lines = _run_rust_producer(CSV_PATH)
    c_lines = _run_c_producer(c_producer_path, CSV_PATH)

    assert len(rust_lines) >= 1, "Rust dumper produced no rows"
    assert len(c_lines) == len(rust_lines), (
        f"row count mismatch: rust={len(rust_lines)} c={len(c_lines)}"
    )
    for idx, (rust, c) in enumerate(zip(rust_lines, c_lines, strict=True)):
        assert rust == c, (
            f"row {idx} diverges:\n"
            f"  rust: {rust}\n"
            f"  c   : {c}\n"
        )
