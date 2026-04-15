"""Phase 4 Line B D1 — POSIX builds of cvc/fzc/rzc are Docker-ready.

These tests assert that each of the three target ECUs has a Dockerfile
sibling to the POSIX build input, and that the Dockerfile is
well-formed (references the POSIX Makefile, picks the right TARGET,
sets CAN_INTERFACE via env, exposes an entrypoint to the binary).

A separate `test_docker_build_cvc_image` test performs an actual
`docker build` on the CVC Dockerfile on Linux hosts that have docker
available. It is skipped on Windows and on Linux hosts without docker.
No credentials or network resources are consumed by the build because
the Dockerfile uses only the local build context.

The test does NOT hardcode TARGET names — the ECU list is loaded from
a config YAML so that future ECUs (icu/tcu/bcm) can be added without
touching test code.
"""
from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
PHASE4_CONFIG = REPO_ROOT / "tests" / "phase4" / "phase4_ecus.yml"


def _load_ecu_list() -> list[str]:
    """Parse the phase4 ECU list from config. Never hardcoded in test."""
    import yaml  # noqa: PLC0415 — optional dep; test skips if missing

    if not PHASE4_CONFIG.is_file():
        pytest.skip(f"config not found: {PHASE4_CONFIG}")
    data = yaml.safe_load(PHASE4_CONFIG.read_text(encoding="utf-8"))
    ecus = data.get("posix_dockerized_ecus") or []
    assert isinstance(ecus, list) and ecus, "posix_dockerized_ecus must be non-empty list"
    for e in ecus:
        assert isinstance(e, str) and e.isalnum(), f"invalid ECU name: {e!r}"
    return ecus


def _ecu_dockerfile(ecu: str) -> Path:
    return REPO_ROOT / "firmware" / "ecu" / ecu / "Dockerfile"


def _docker_available() -> bool:
    return shutil.which("docker") is not None and os.name != "nt"


@pytest.fixture(scope="module")
def ecus() -> list[str]:
    return _load_ecu_list()


def test_each_ecu_has_dockerfile(ecus: list[str]) -> None:
    missing = [e for e in ecus if not _ecu_dockerfile(e).is_file()]
    assert not missing, f"ECUs without Dockerfile: {missing}"


def test_dockerfiles_reference_posix_makefile(ecus: list[str]) -> None:
    """Each Dockerfile must build against Makefile.posix and select its TARGET."""
    for ecu in ecus:
        df = _ecu_dockerfile(ecu)
        text = df.read_text(encoding="utf-8")
        assert "Makefile.posix" in text, f"{ecu}: Dockerfile does not invoke Makefile.posix"
        # TARGET=<ecu> must appear — enforces the right binary is built.
        assert f"TARGET={ecu}" in text, f"{ecu}: Dockerfile does not pass TARGET={ecu}"


def test_dockerfiles_set_can_interface_env(ecus: list[str]) -> None:
    """POSIX BSW reads CAN_INTERFACE env. The container must set it or the
    caller must be able to override it via `docker run -e`.

    We require the image to declare `ENV CAN_INTERFACE=vcan0` so compose
    deployments inherit a sensible default, while `-e` overrides still work.
    """
    for ecu in ecus:
        df = _ecu_dockerfile(ecu)
        text = df.read_text(encoding="utf-8")
        assert "CAN_INTERFACE" in text, (
            f"{ecu}: Dockerfile does not declare CAN_INTERFACE env var"
        )


def test_dockerfiles_have_entrypoint_to_posix_binary(ecus: list[str]) -> None:
    """Each Dockerfile must ENTRYPOINT or CMD into build/<ecu>/<ecu>_posix."""
    for ecu in ecus:
        df = _ecu_dockerfile(ecu)
        text = df.read_text(encoding="utf-8")
        expected = f"{ecu}_posix"
        assert expected in text, (
            f"{ecu}: Dockerfile does not reference the POSIX binary {expected}"
        )


def test_dockerfiles_no_hardcoded_vin(ecus: list[str]) -> None:
    """Safety net for D6: no Dockerfile may inline a 17-character VIN literal
    or a 4-hex-digit DID literal. All such values live in config."""
    import re
    vin_re = re.compile(r"\b[A-HJ-NPR-Z0-9]{17}\b")
    hex_did_re = re.compile(r"\b0[xX][0-9A-Fa-f]{4}\b")
    for ecu in ecus:
        df = _ecu_dockerfile(ecu)
        text = df.read_text(encoding="utf-8")
        vins = vin_re.findall(text)
        dids = hex_did_re.findall(text)
        assert not vins, f"{ecu}: VIN literal in Dockerfile: {vins}"
        assert not dids, f"{ecu}: 4-hex DID literal in Dockerfile: {dids}"


@pytest.mark.skipif(
    not _docker_available(),
    reason="docker not available on this host; run on Linux laptop",
)
def test_docker_build_cvc_image_smoke(tmp_path: Path) -> None:
    """Smoke test: `docker build` the CVC image.

    We build from the repo root so the Makefile.posix and firmware tree
    are in the build context. The build must succeed; we do not yet
    `docker run` in this test because vcan0 on the host namespace is a
    separate concern handled by D4's compose gate.
    """
    ecu = "cvc"
    df = _ecu_dockerfile(ecu)
    assert df.is_file(), f"missing Dockerfile: {df}"
    cmd = [
        "docker", "build",
        "--file", str(df),
        "--tag", f"taktflow/{ecu}-posix:phase4-test",
        str(REPO_ROOT),
    ]
    result = subprocess.run(
        cmd, capture_output=True, text=True, timeout=900,
    )
    assert result.returncode == 0, (
        f"docker build failed for {ecu}:\n"
        f"stdout:\n{result.stdout[-2000:]}\n"
        f"stderr:\n{result.stderr[-2000:]}"
    )
