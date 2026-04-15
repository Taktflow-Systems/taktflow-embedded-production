"""Phase 4 Line B D5 — compose-managed POSIX ECUs reachable via the
CAN-to-DoIP proxy.

End-to-end flow on a Linux host with vcan0 up:

    1. Bring up the POSIX compose fleet:
         docker compose -f deploy/docker/compose-posix-ecus.yml up -d
    2. Start the CAN-to-DoIP proxy against the same vcan0 interface.
    3. Open a TCP socket to the proxy on port 13400.
    4. Send a DoIP routing activation + diagnostic message carrying
       the UDS ReadDataByIdentifier 0x22 0xF1 0x90 request for CVC's
       logical address.
    5. Read the response bytes and assert the VIN returned by the
       live container matches the VIN in cvc_identity.toml.

The test is gated on a full Linux stack: docker, docker compose, vcan0
already loaded, and a compiled proxy binary. It skips cleanly on
Windows and in environments missing any of those prerequisites.

No VIN, DID, or CAN id literal appears in this file. The DID comes
from the odx-gen parser; the VIN comes from the identity config file.
"""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import time
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
COMPOSE_FILE = REPO_ROOT / "deploy" / "docker" / "compose-posix-ecus.yml"
PROXY_CONFIG = REPO_ROOT / "gateway" / "can_to_doip_proxy" / "deploy" / "opensovd-proxy.toml"
CVC_IDENTITY = REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "cvc_identity.toml"
DCM_CFG_CVC = REPO_ROOT / "firmware" / "ecu" / "cvc" / "cfg" / "Dcm_Cfg_Cvc.c"

# ---------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------


def _parse_toml_simple(text: str) -> dict[str, str]:
    """Parse a dead-simple TOML: key = "value" lines, ignore comments."""
    out: dict[str, str] = {}
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        m = re.match(r'^(?P<k>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*"(?P<v>[^"]*)"\s*$', s)
        if m:
            out[m.group("k")] = m.group("v")
    return out


def _expected_vin() -> str:
    d = _parse_toml_simple(CVC_IDENTITY.read_text(encoding="utf-8"))
    vin = d.get("vin")
    assert vin and len(vin) == 17, f"config VIN invalid: {vin!r}"
    return vin


def _vin_did_id() -> int:
    """Resolve F190 from Dcm_Cfg_Cvc.c via the shared parser."""
    from odx_gen.parsers.dcm_cfg import parse_dcm_cfg
    model = parse_dcm_cfg(DCM_CFG_CVC, "cvc")
    for d in model.dids:
        if "vin" in (d.callback or "").lower() or "vin" in (d.name or "").lower():
            return d.did_id
    pytest.fail("no VIN DID row in Dcm_Cfg_Cvc.c")


def _cvc_logical_address_from_proxy_toml() -> int:
    """Resolve CVC's doip_logical_address from the proxy TOML."""
    text = PROXY_CONFIG.read_text(encoding="utf-8")
    # Naive scan: locate [[ecu]] blocks and match by name = "cvc".
    in_cvc = False
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("[[ecu]]"):
            in_cvc = False
            continue
        if s.startswith("name") and '"cvc"' in s:
            in_cvc = True
            continue
        if in_cvc and s.startswith("doip_logical_address"):
            m = re.search(r"0[xX][0-9A-Fa-f]+", s)
            if m:
                return int(m.group(0), 16)
    pytest.fail("doip_logical_address for cvc not found in proxy.toml")


# ---------------------------------------------------------------
# Prerequisite gates
# ---------------------------------------------------------------


def _linux_with_docker_compose_and_vcan() -> tuple[bool, str]:
    if os.name == "nt":
        return False, "windows host"
    if shutil.which("docker") is None:
        return False, "docker not installed"
    r = subprocess.run(
        ["docker", "compose", "version"],
        capture_output=True, text=True, timeout=10,
    )
    if r.returncode != 0:
        return False, "docker compose v2 plugin missing"
    # vcan0 must exist (ip link show vcan0 exits 0)
    r = subprocess.run(
        ["ip", "link", "show", "vcan0"],
        capture_output=True, text=True, timeout=5,
    )
    if r.returncode != 0:
        return False, "vcan0 not present on host; run `sudo modprobe vcan && sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0`"
    return True, "ok"


@pytest.fixture(scope="module")
def _linux_ready() -> None:
    ok, reason = _linux_with_docker_compose_and_vcan()
    if not ok:
        pytest.skip(f"integration test requires Linux + docker + vcan0: {reason}")


# ---------------------------------------------------------------
# Compose lifecycle fixture
# ---------------------------------------------------------------


@pytest.fixture(scope="module")
def compose_fleet(_linux_ready: None):
    """Bring up the POSIX compose fleet for the duration of the module."""
    # Build images first (avoids racing `up -d` with a cold cache).
    up_cmd = ["docker", "compose", "-f", str(COMPOSE_FILE), "up", "-d", "--build"]
    r = subprocess.run(
        up_cmd, cwd=str(REPO_ROOT),
        capture_output=True, text=True, timeout=1800,
    )
    if r.returncode != 0:
        pytest.fail(
            f"docker compose up failed:\nstdout:\n{r.stdout[-2000:]}\n"
            f"stderr:\n{r.stderr[-2000:]}"
        )
    # Give the ECU containers ~3s to initialise their Dcm state.
    time.sleep(3.0)
    yield
    # Tear down.
    subprocess.run(
        ["docker", "compose", "-f", str(COMPOSE_FILE), "down", "-v"],
        cwd=str(REPO_ROOT),
        capture_output=True, text=True, timeout=120,
    )


# ---------------------------------------------------------------
# Actual interop test
# ---------------------------------------------------------------


def _build_doip_routing_activation() -> bytes:
    """Build a minimal DoIP routing activation request frame.

    DoIP header: version(1) version_inv(1) payload_type(2) payload_len(4)
    then payload. For type 0x0005 (routing activation request) the
    payload is client_logical_address(2) + activation_type(1) +
    reserved(4) + optional oem(4) — we use client=0x0E00 activation=0.
    """
    payload = b"\x0e\x00" + b"\x00" + (b"\x00" * 4)
    header = b"\x02\xfd" + b"\x00\x05" + len(payload).to_bytes(4, "big")
    return header + payload


def _build_doip_uds(cvc_logical: int, uds: bytes) -> bytes:
    """Build a DoIP diagnostic message frame carrying a UDS request."""
    # payload: source addr(2) + target addr(2) + UDS
    payload = (0x0e00).to_bytes(2, "big") + cvc_logical.to_bytes(2, "big") + uds
    header = b"\x02\xfd" + b"\x80\x01" + len(payload).to_bytes(4, "big")
    return header + payload


def _parse_doip_diag_positive_response(buf: bytes) -> bytes | None:
    """Very lenient DoIP parser: find a payload with type 0x8001 and
    return the UDS bytes. Returns None if nothing matched."""
    i = 0
    while i + 8 <= len(buf):
        if buf[i] != 0x02 or buf[i + 1] != 0xFD:
            i += 1
            continue
        payload_type = int.from_bytes(buf[i + 2: i + 4], "big")
        payload_len = int.from_bytes(buf[i + 4: i + 8], "big")
        frame_end = i + 8 + payload_len
        if frame_end > len(buf):
            break
        if payload_type == 0x8001 and payload_len >= 4:
            return buf[i + 8 + 4: frame_end]
        i = frame_end
    return None


@pytest.fixture(scope="module")
def proxy_binary() -> Path:
    """Locate the compiled proxy binary. Skip if not present."""
    candidates = [
        REPO_ROOT / "gateway" / "can_to_doip_proxy" / "target" / "release"
        / "opensovd-can-to-doip-proxy",
        REPO_ROOT / "gateway" / "can_to_doip_proxy" / "target" / "debug"
        / "opensovd-can-to-doip-proxy",
    ]
    for c in candidates:
        if c.is_file() and os.access(c, os.X_OK):
            return c
    pytest.skip(
        "proxy binary not built; run `cargo build -p opensovd-can-to-doip-proxy-main` "
        "in gateway/can_to_doip_proxy/ first"
    )


@pytest.fixture(scope="module")
def proxy_process(proxy_binary: Path, compose_fleet):
    """Spawn the proxy pointed at the compose fleet's vcan0."""
    proc = subprocess.Popen(
        [str(proxy_binary), "--config-file", str(PROXY_CONFIG)],
        cwd=str(REPO_ROOT / "gateway" / "can_to_doip_proxy"),
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
    )
    time.sleep(2.0)
    if proc.poll() is not None:
        out, err = proc.communicate(timeout=5)
        pytest.fail(
            f"proxy exited early rc={proc.returncode}\n"
            f"stdout:\n{out.decode(errors='replace')[-2000:]}\n"
            f"stderr:\n{err.decode(errors='replace')[-2000:]}"
        )
    yield proc
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()


def test_compose_cvc_returns_vin_via_proxy(proxy_process) -> None:
    """Full round-trip: containerised CVC answers F190 through the proxy."""
    import socket

    cvc_logical = _cvc_logical_address_from_proxy_toml()
    vin_did = _vin_did_id()
    expected_vin = _expected_vin()

    uds_req = bytes([0x22, (vin_did >> 8) & 0xFF, vin_did & 0xFF])

    with socket.create_connection(("127.0.0.1", 13400), timeout=5) as sock:
        sock.sendall(_build_doip_routing_activation())
        # Drain routing activation response
        _ = sock.recv(4096)
        sock.sendall(_build_doip_uds(cvc_logical, uds_req))
        chunks: list[bytes] = []
        deadline = time.time() + 5.0
        while time.time() < deadline:
            try:
                sock.settimeout(max(0.1, deadline - time.time()))
                data = sock.recv(4096)
            except socket.timeout:
                break
            if not data:
                break
            chunks.append(data)
            if _parse_doip_diag_positive_response(b"".join(chunks)) is not None:
                break

    uds = _parse_doip_diag_positive_response(b"".join(chunks))
    assert uds is not None, "no DoIP diagnostic response received"
    # UDS positive response: 0x62 did_high did_low vin[17]
    assert uds[0] == 0x62, f"expected positive 0x62, got {uds[0]:#x}"
    assert uds[1] == (vin_did >> 8) & 0xFF
    assert uds[2] == vin_did & 0xFF
    assert uds[3:20].decode("ascii") == expected_vin, (
        f"VIN mismatch: expected {expected_vin!r}, got {uds[3:20]!r}"
    )
