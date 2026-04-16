# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B follow-up - HIL test lib must resolve ST-LINK serials from the
checked-in hardware map, not stale literals.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import textwrap
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
HIL_TEST_LIB = REPO_ROOT / "test" / "hil" / "hil_test_lib.py"

CVC_STLINK_SERIAL = "TEST-CVC-SN"
FZC_STLINK_SERIAL = "TEST-FZC-SN"
RZC_STLINK_SERIAL = "TEST-RZC-SN"


def _write_hardware_map(path: Path) -> Path:
    path.write_text(
        textwrap.dedent(
            """
            [[stlink]]
            logical_ecu = "cvc"
            stlink_serial = "TEST-CVC-SN"
            com_port = "COM11"

            [[stlink]]
            logical_ecu = "fzc"
            stlink_serial = "TEST-FZC-SN"
            com_port = "COM22"

            [[stlink]]
            logical_ecu = "rzc"
            stlink_serial = "TEST-RZC-SN"
            com_port = "COM33"
            """
        ).strip()
        + "\n",
        encoding="utf-8",
    )
    return path


def test_hil_test_lib_does_not_hardcode_stlink_serials() -> None:
    source = HIL_TEST_LIB.read_text(encoding="utf-8")
    for serial in (CVC_STLINK_SERIAL, FZC_STLINK_SERIAL, RZC_STLINK_SERIAL):
        assert serial not in source, (
            f"{HIL_TEST_LIB} still hard-codes ST-LINK serial {serial}; "
            "bench reset helpers must read tools/bench/hardware-map.toml"
        )


def test_hil_test_lib_resolves_serials_from_hardware_map(tmp_path) -> None:
    hardware_map = _write_hardware_map(tmp_path / "hardware-map.toml")
    code = f"""
import importlib.util
import json
import pathlib
import sys
import types

sys.modules['can'] = types.SimpleNamespace(
    interface=types.SimpleNamespace(Bus=None),
    Message=object,
)
sys.modules['cantools'] = types.SimpleNamespace()
publish_mod = types.ModuleType('paho.mqtt.publish')
publish_mod.single = lambda *args, **kwargs: None
mqtt_mod = types.ModuleType('paho.mqtt')
mqtt_mod.publish = publish_mod
paho_mod = types.ModuleType('paho')
paho_mod.mqtt = mqtt_mod
sys.modules['paho'] = paho_mod
sys.modules['paho.mqtt'] = mqtt_mod
sys.modules['paho.mqtt.publish'] = publish_mod

path = pathlib.Path(r'{HIL_TEST_LIB}')
spec = importlib.util.spec_from_file_location('hil_test_lib_runtime', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
print(json.dumps({{
    'cvc': module.resolve_stlink_serial('cvc'),
    'rzc': module.resolve_stlink_serial('rzc'),
}}))
"""
    result = subprocess.run(
        [sys.executable, "-c", code],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        env={
            **os.environ,
            "TAKTFLOW_HARDWARE_MAP": str(hardware_map),
        },
    )
    assert result.returncode == 0, (
        f"hil_test_lib serial resolution helper failed:\n"
        f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    payload = json.loads(result.stdout)
    assert payload == {"cvc": CVC_STLINK_SERIAL, "rzc": RZC_STLINK_SERIAL}
