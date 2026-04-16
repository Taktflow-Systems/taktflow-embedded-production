# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Helpers for reading tools/bench/hardware-map.toml at runtime.

Bench scripts should resolve probe assignments from the checked-in map
instead of carrying stale serial literals in code.
"""
from __future__ import annotations

import os
import tomllib
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_HARDWARE_MAP = REPO_ROOT / "tools" / "bench" / "hardware-map.toml"
DEFAULT_LOCAL_HARDWARE_MAP = REPO_ROOT / "tools" / "bench" / "hardware-map.local.toml"
HARDWARE_MAP_ENV = "TAKTFLOW_HARDWARE_MAP"


def active_hardware_map_path(*, env: dict[str, str] | None = None) -> Path:
    override_env = env if env is not None else os.environ
    configured = override_env.get(HARDWARE_MAP_ENV)
    if configured:
        return Path(configured)
    if DEFAULT_LOCAL_HARDWARE_MAP.exists():
        return DEFAULT_LOCAL_HARDWARE_MAP
    return DEFAULT_HARDWARE_MAP


def load_hardware_map(
    path: Path | None = None,
    *,
    env: dict[str, str] | None = None,
) -> dict[str, Any]:
    hardware_map_path = path or active_hardware_map_path(env=env)
    if not hardware_map_path.exists():
        raise FileNotFoundError(f"hardware map missing: {hardware_map_path}")
    return tomllib.loads(hardware_map_path.read_text(encoding="utf-8"))


def resolve_stlink_probe(
    logical_ecu: str,
    *,
    path: Path | None = None,
    env: dict[str, str] | None = None,
) -> dict[str, Any]:
    data = load_hardware_map(path, env=env)
    for row in data.get("stlink", []):
        if row.get("logical_ecu") == logical_ecu:
            return row
    raise KeyError(
        f"no [[stlink]] entry with logical_ecu={logical_ecu!r} in "
        f"{path or active_hardware_map_path(env=env)}"
    )


def resolve_stlink_serial(
    logical_ecu: str,
    *,
    path: Path | None = None,
    env: dict[str, str] | None = None,
) -> str:
    override_env = env if env is not None else os.environ
    env_override = override_env.get(f"{logical_ecu.upper()}_SN")
    if env_override:
        return env_override

    row = resolve_stlink_probe(logical_ecu, path=path, env=override_env)
    serial = row.get("stlink_serial")
    if not serial:
        raise KeyError(
            f"[[stlink]] logical_ecu={logical_ecu!r} missing stlink_serial in "
            f"{path or active_hardware_map_path(env=override_env)}"
        )
    return str(serial)
