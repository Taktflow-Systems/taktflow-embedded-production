"""Tests for fail-closed selection of the E2E parameter source."""

from __future__ import annotations

import os
from pathlib import Path

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

COMPLETED_DBC_E2E_IDS = {
    **EXPLICIT_DBC_E2E_IDS,
    "ICU_Heartbeat": 0x31,
    "TCU_Heartbeat": 0x41,
    "BCM_Heartbeat": 0x51,
    "Motor_Temperature": 0x00,
    "Battery_Status": 0x13,
}


def _load_model_with_e2e_source(e2e_source: str, dbc_path=None):
    from tools.arxmlgen.config import load_config
    from tools.arxmlgen.reader import ArxmlReader

    config = load_config(os.path.join(PROJECT_ROOT, "project.yaml"))
    config.e2e_source = e2e_source
    if dbc_path is not None:
        config.dbc_path = str(dbc_path)
    return ArxmlReader(config).read()


@pytest.fixture(scope="module")
def completed_dbc(tmp_path_factory):
    source = Path(PROJECT_ROOT) / "gateway" / "taktflow_vehicle.dbc"
    content = source.read_text(encoding="utf-8")
    content += """
BA_ "E2E_DataID" BO_ 20 49;
BA_ "E2E_DataID" BO_ 21 65;
BA_ "E2E_DataID" BO_ 22 81;
BA_ "E2E_DataID" BO_ 770 0;
BA_ "E2E_DataID" BO_ 771 19;
"""
    path = tmp_path_factory.mktemp("complete-e2e-dbc") / "complete.dbc"
    path.write_text(content, encoding="utf-8")
    return path


@pytest.fixture(scope="module")
def model_dbc_complete(completed_dbc):
    return _load_model_with_e2e_source("dbc", completed_dbc)


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


def test_16_message_data_ids_reach_transmitting_pdus(model_dbc_complete):
    tx_pdus = {
        pdu.name: pdu
        for ecu in model_dbc_complete.ecus.values()
        for pdu in ecu.tx_pdus
    }

    for name, expected in EXPLICIT_DBC_E2E_IDS.items():
        assert tx_pdus[name].e2e_data_id == expected


def test_heartbeat_data_ids_are_preserved(model_dbc_complete):
    expected = {
        "CVC_Heartbeat": 0x02,
        "FZC_Heartbeat": 0x03,
        "RZC_Heartbeat": 0x04,
        "ICU_Heartbeat": 0x31,
        "TCU_Heartbeat": 0x41,
        "BCM_Heartbeat": 0x51,
    }
    tx_pdus = {
        pdu.name: pdu
        for ecu in model_dbc_complete.ecus.values()
        for pdu in ecu.tx_pdus
    }

    assert {name: tx_pdus[name].e2e_data_id for name in expected} == expected


def test_rx_pdus_inherit_transmitter_data_ids(model_dbc_complete):
    tx_ids = {
        pdu.name: pdu.e2e_data_id
        for ecu in model_dbc_complete.ecus.values()
        for pdu in ecu.tx_pdus
    }

    for ecu in model_dbc_complete.ecus.values():
        for pdu in ecu.rx_pdus:
            if pdu.name in COMPLETED_DBC_E2E_IDS:
                assert pdu.e2e_data_id == tx_ids[pdu.name]


@pytest.fixture(scope="module")
def model_sidecar():
    return _load_model_with_e2e_source("sidecar")


def test_dbc_and_sidecar_models_have_ecu_pdu_parity(
    model_dbc_complete, model_sidecar
):
    assert set(model_dbc_complete.ecus) == set(model_sidecar.ecus)
    for ecu_name in model_dbc_complete.ecus:
        dbc_ecu = model_dbc_complete.ecus[ecu_name]
        sidecar_ecu = model_sidecar.ecus[ecu_name]
        assert {pdu.name for pdu in dbc_ecu.tx_pdus} == {
            pdu.name for pdu in sidecar_ecu.tx_pdus
        }
        assert {pdu.name for pdu in dbc_ecu.rx_pdus} == {
            pdu.name for pdu in sidecar_ecu.rx_pdus
        }


def test_dbc_and_sidecar_ids_match_for_protected_pdus(
    model_dbc_complete, model_sidecar
):
    for ecu_name in model_dbc_complete.ecus:
        dbc_pdus = {
            (pdu.direction, pdu.name): pdu
            for pdu in (
                model_dbc_complete.ecus[ecu_name].tx_pdus
                + model_dbc_complete.ecus[ecu_name].rx_pdus
            )
            if pdu.e2e_protected
        }
        sidecar_ids = {
            (pdu.direction, pdu.name): pdu.e2e_data_id
            for pdu in (
                model_sidecar.ecus[ecu_name].tx_pdus
                + model_sidecar.ecus[ecu_name].rx_pdus
            )
            if pdu.e2e_protected
        }
        assert {key: pdu.e2e_data_id for key, pdu in dbc_pdus.items()} == sidecar_ids


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
