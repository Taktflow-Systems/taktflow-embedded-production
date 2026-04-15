"""Phase 4 Line B D4 — docker-compose skeleton for POSIX ECU fleet.

Tests live in two tiers:

1. Structural (run everywhere): assert the compose file exists, is
   well-formed YAML, declares exactly the services listed in
   tests/phase4/phase4_ecus.yml, uses host networking (so containers
   share vcan0 with the host kernel), and refers to each ECU's
   Dockerfile via the `build.dockerfile` key.

2. docker-compose config validation (Linux only, docker required):
   `docker compose -f <file> config` must exit 0.

The compose file's vcan bridge setup is intentionally out-of-compose
(host network mode + pre-existing vcan0). The alternative (init
container running `modprobe vcan`) requires a privileged container
and was rejected because the Linux laptop already has vcan0 up; see
the D4 commit message for the rationale.
"""
from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

import pytest
import yaml

REPO_ROOT = Path(__file__).resolve().parents[2]
COMPOSE_FILE = REPO_ROOT / "deploy" / "docker" / "compose-posix-ecus.yml"
PHASE4_CONFIG = REPO_ROOT / "tests" / "phase4" / "phase4_ecus.yml"


@pytest.fixture(scope="module")
def ecus() -> list[str]:
    data = yaml.safe_load(PHASE4_CONFIG.read_text(encoding="utf-8"))
    return list(data["posix_dockerized_ecus"])


@pytest.fixture(scope="module")
def compose() -> dict:
    assert COMPOSE_FILE.is_file(), f"compose file missing: {COMPOSE_FILE}"
    return yaml.safe_load(COMPOSE_FILE.read_text(encoding="utf-8"))


def test_compose_file_exists(compose: dict) -> None:
    assert isinstance(compose, dict)
    assert "services" in compose, "compose file has no services key"


def test_compose_services_match_ecu_list(compose: dict, ecus: list[str]) -> None:
    svc_names = set(compose["services"].keys())
    expected = set(ecus)
    assert svc_names == expected, (
        f"compose services {svc_names} do not match ECU list {expected}"
    )


def test_compose_services_use_host_network(compose: dict, ecus: list[str]) -> None:
    """Host networking is required so all ECUs share vcan0 with the host."""
    for ecu in ecus:
        svc = compose["services"][ecu]
        net = svc.get("network_mode")
        assert net == "host", (
            f"{ecu}: network_mode must be 'host' for vcan0 sharing, got {net!r}"
        )


def test_compose_services_reference_dockerfile(compose: dict, ecus: list[str]) -> None:
    """Each service must build from firmware/ecu/<ecu>/Dockerfile."""
    for ecu in ecus:
        svc = compose["services"][ecu]
        build = svc.get("build")
        assert isinstance(build, dict), f"{ecu}: build must be a dict"
        df = build.get("dockerfile")
        assert df == f"firmware/ecu/{ecu}/Dockerfile", (
            f"{ecu}: dockerfile must point at firmware/ecu/{ecu}/Dockerfile, "
            f"got {df!r}"
        )
        ctx = build.get("context")
        assert ctx == ".", f"{ecu}: build.context must be '.' (repo root), got {ctx!r}"


def test_compose_no_hardcoded_vin_or_did(compose: dict) -> None:
    """Regression: no VIN literal and no 4-hex DID literal in the compose file."""
    import re
    text = COMPOSE_FILE.read_text(encoding="utf-8")
    vin_re = re.compile(r"\b[A-HJ-NPR-Z0-9]{17}\b")
    hex_did_re = re.compile(r"\b0[xX][0-9A-Fa-f]{4}\b")
    assert not vin_re.findall(text), "VIN literal found in compose file"
    assert not hex_did_re.findall(text), "4-hex DID literal found in compose file"


def _docker_compose_available() -> bool:
    if os.name == "nt":
        return False
    if shutil.which("docker") is None:
        return False
    # docker compose v2 plugin
    r = subprocess.run(
        ["docker", "compose", "version"],
        capture_output=True, text=True, timeout=10,
    )
    return r.returncode == 0


@pytest.mark.skipif(
    not _docker_compose_available(),
    reason="docker compose v2 not available on this host; run on Linux laptop",
)
def test_docker_compose_config_validates() -> None:
    """`docker compose config` must exit 0 against the compose file."""
    cmd = [
        "docker", "compose",
        "-f", str(COMPOSE_FILE),
        "config",
    ]
    result = subprocess.run(
        cmd, capture_output=True, text=True, timeout=60,
        cwd=str(REPO_ROOT),
    )
    assert result.returncode == 0, (
        f"docker compose config failed:\n"
        f"stdout:\n{result.stdout[-2000:]}\n"
        f"stderr:\n{result.stderr[-2000:]}"
    )
