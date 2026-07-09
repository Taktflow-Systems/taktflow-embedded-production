"""Tests for fail-closed selection of the E2E parameter source."""

from __future__ import annotations

import os

import pytest


TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(TESTS_DIR)))

EXPLICIT_DBC_E2E_IDS = {
    "EStop_Broadcast": 0x01,
    "CVC_Heartbeat": 0x02,
    "FZC_Heartbeat": 0x03,
    "RZC_Heartbeat": 0x04,
    "SC_Status": 0x00,
    "Vehicle_State": 0x05,
    "Torque_Request": 0x06,
    "Steer_Command": 0x07,
    "Brake_Command": 0x08,
    "Steering_Status": 0x09,
    "Brake_Status": 0x0A,
    "Brake_Fault": 0x0B,
    "Motor_Cutoff_Req": 0x0C,
    "Lidar_Distance": 0x0D,
    "Motor_Status": 0x0E,
    "Motor_Current": 0x0F,
}


def _load_model_with_e2e_source(e2e_source: str):
    from tools.arxmlgen.config import load_config
    from tools.arxmlgen.reader import ArxmlReader

    config = load_config(os.path.join(PROJECT_ROOT, "project.yaml"))
    config.e2e_source = e2e_source
    return ArxmlReader(config).read()


def test_project_uses_arxml_source():
    from tools.arxmlgen.config import load_config

    config = load_config(os.path.join(PROJECT_ROOT, "project.yaml"))
    assert config.e2e_source == "arxml"


def test_config_rejects_invalid_source(tmp_path):
    from tools.arxmlgen.config import ConfigError, load_config

    (tmp_path / "dummy.arxml").write_text("<AUTOSAR/>", encoding="utf-8")
    (tmp_path / "project.yaml").write_text(
        """project:
  name: Test
input:
  arxml: [dummy.arxml]
  e2e_source: invalid
ecus:
  test: {prefix: TST}
""",
        encoding="utf-8",
    )

    with pytest.raises(ConfigError, match="e2e_source"):
        load_config(str(tmp_path / "project.yaml"))


def test_config_accepts_dbc():
    from tools.arxmlgen.config import load_config

    config = load_config(os.path.join(PROJECT_ROOT, "project.yaml"))
    config.e2e_source = "dbc"
    assert config.e2e_source == "dbc"


def test_incomplete_dbc_e2e_source_fails_closed():
    from tools.arxmlgen.reader import ArxmlReadError

    with pytest.raises(ArxmlReadError) as exc_info:
        _load_model_with_e2e_source("dbc")

    diagnostic = exc_info.value.diagnostics[0]
    assert diagnostic.code == "DBC006"
    assert diagnostic.source == "E2E PDU 'ICU_Heartbeat'"


def test_explicit_dbc_e2e_ids_are_read_exactly():
    from tools.arxmlgen.config import load_config
    from tools.arxmlgen.reader import ArxmlReader

    config = load_config(os.path.join(PROJECT_ROOT, "project.yaml"))
    reader = ArxmlReader(config)
    reader._load_dbc_routing()

    assert reader._dbc_e2e_map == EXPLICIT_DBC_E2E_IDS


def test_complete_dbc_e2e_mapping_is_applied():
    from tools.arxmlgen.config import ProjectConfig
    from tools.arxmlgen.model import Ecu, Pdu
    from tools.arxmlgen.reader import ArxmlReader

    reader = ArxmlReader(ProjectConfig())
    reader._dbc_e2e_map = {"BrakeStatus": 9}
    reader._dbc_e2e_max_delta = {"BrakeStatus": 4}
    pdu = Pdu(name="BrakeStatus", e2e_protected=True)
    ecu = Ecu(name="brake", prefix="BRK", tx_pdus=[pdu])

    reader._apply_dbc_e2e({"brake": ecu})

    assert pdu.e2e_data_id == 9
    assert pdu.e2e_max_delta == 4


@pytest.fixture(scope="module")
def model_sidecar():
    return _load_model_with_e2e_source("sidecar")


def test_sidecar_source_populates_protected_tx_pdus(model_sidecar):
    for pdu in model_sidecar.ecus["cvc"].tx_pdus:
        if pdu.e2e_protected:
            assert pdu.e2e_data_id is not None


def test_sidecar_estop_data_id(model_sidecar):
    pdu = next(
        item
        for item in model_sidecar.ecus["cvc"].tx_pdus
        if item.name == "EStop_Broadcast"
    )
    assert pdu.e2e_data_id == 0x01
