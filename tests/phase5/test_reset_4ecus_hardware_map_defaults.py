# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Taktflow Systems
"""
Phase 5 Line B follow-up - reset_4ecus should prefer hardware-map COM defaults.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import textwrap
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPO_ROOT / "scripts" / "debug" / "reset_4ecus.py"


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


def test_reset_4ecus_hardware_map_ports(tmp_path) -> None:
    hardware_map = _write_hardware_map(tmp_path / "hardware-map.toml")
    code = f"""
import importlib.util
import json
import pathlib
import sys
import types

serial_mod = types.ModuleType('serial')
serial_mod.SerialException = Exception
serial_mod.Serial = object
list_ports_mod = types.ModuleType('serial.tools.list_ports')
list_ports_mod.comports = lambda: []
tools_mod = types.ModuleType('serial.tools')
tools_mod.list_ports = list_ports_mod
serial_mod.tools = tools_mod
sys.modules['serial'] = serial_mod
sys.modules['serial.tools'] = tools_mod
sys.modules['serial.tools.list_ports'] = list_ports_mod

path = pathlib.Path(r'{SCRIPT}')
spec = importlib.util.spec_from_file_location('reset_4ecus_runtime', path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
print(json.dumps({{
    'cvc': module.hardware_map_port('cvc'),
    'fzc': module.hardware_map_port('fzc'),
    'rzc': module.hardware_map_port('rzc'),
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
        f"reset_4ecus hardware-map helper failed:\n"
        f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
    )
    payload = json.loads(result.stdout)
    assert payload == {"cvc": "COM11", "fzc": "COM22", "rzc": "COM33"}
