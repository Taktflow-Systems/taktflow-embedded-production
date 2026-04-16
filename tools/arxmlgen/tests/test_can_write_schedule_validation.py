"""
Validation tests for CAN main-function scheduling in sidecar runnable maps.
"""

from __future__ import annotations

import os
from pathlib import Path

import pytest
import yaml


PROJECT_ROOT = Path(__file__).resolve().parents[3]


def test_reader_rejects_missing_can_write_runnable(tmp_path):
    """Sidecar schedules with CAN polling must also schedule TX queue drain."""
    from tools.arxmlgen.config import load_config
    from tools.arxmlgen.reader import ArxmlReadError, ArxmlReader

    sidecar_path = PROJECT_ROOT / "model" / "ecu_sidecar.yaml"
    sidecar = yaml.safe_load(sidecar_path.read_text(encoding="utf-8"))
    del sidecar["ecus"]["fzc"]["runnables"]["Can_MainFunction_Write"]

    temp_sidecar = tmp_path / "ecu_sidecar.yaml"
    temp_sidecar.write_text(
        yaml.safe_dump(sidecar, sort_keys=False),
        encoding="utf-8",
    )

    config = load_config(os.fspath(PROJECT_ROOT / "project.yaml"))
    config.sidecar_path = os.fspath(temp_sidecar)

    with pytest.raises(ArxmlReadError, match="Can_MainFunction_Write"):
        ArxmlReader(config).read()
