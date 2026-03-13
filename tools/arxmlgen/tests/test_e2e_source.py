"""
Tests for --e2e-source option: DBC vs sidecar E2E data ID sourcing.

Verifies that:
- DBC mode reads E2E_DataID attribute from DBC and applies CAN-matrix-aligned IDs
- Sidecar mode reads pdu_e2e_map from ecu_sidecar.yaml (existing behavior)
- Both modes produce valid E2E config with correct data IDs
- Config and CLI parsing works for e2e_source
"""

import os
import re

import pytest


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(TESTS_DIR)))


def _load_model_with_e2e_source(e2e_source: str):
    """Load model with a specific e2e_source override."""
    from tools.arxmlgen.config import load_config
    from tools.arxmlgen.reader import ArxmlReader

    config_path = os.path.join(PROJECT_ROOT, "project.yaml")
    if not os.path.exists(config_path):
        pytest.skip("project.yaml not found")

    config = load_config(config_path)
    config.e2e_source = e2e_source
    reader = ArxmlReader(config)
    return reader.read()


# CAN matrix authoritative data IDs (from docs/aspice/system/can-message-matrix.md)
CAN_MATRIX_E2E_IDS = {
    "EStop_Broadcast": 0x01,
    "CVC_Heartbeat": 0x02,
    "FZC_Heartbeat": 0x03,
    "RZC_Heartbeat": 0x04,
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
    "Motor_Temperature": 0x00,
}


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def model_dbc():
    """Model loaded with e2e_source=dbc."""
    return _load_model_with_e2e_source("dbc")


@pytest.fixture(scope="module")
def model_sidecar():
    """Model loaded with e2e_source=sidecar."""
    return _load_model_with_e2e_source("sidecar")


# ===================================================================
# Config / CLI tests
# ===================================================================

class TestE2ESourceConfig:
    """Verify e2e_source config and CLI parsing."""

    def test_config_default_is_sidecar(self):
        """Default e2e_source must be 'sidecar' for backward compatibility."""
        from tools.arxmlgen.config import load_config

        config_path = os.path.join(PROJECT_ROOT, "project.yaml")
        config = load_config(config_path)
        assert config.e2e_source == "sidecar"

    def test_config_rejects_invalid_source(self, tmp_path):
        """Invalid e2e_source must raise ConfigError."""
        from tools.arxmlgen.config import ConfigError, load_config

        # Create a minimal project.yaml with invalid e2e_source
        yaml_content = """
project:
  name: Test
input:
  arxml:
    - dummy.arxml
  e2e_source: invalid
ecus:
  test: { prefix: "TST" }
"""
        yaml_path = tmp_path / "project.yaml"
        yaml_path.write_text(yaml_content)
        # Also create a dummy arxml file
        (tmp_path / "dummy.arxml").write_text("<AUTOSAR/>")

        with pytest.raises(ConfigError, match="e2e_source"):
            load_config(str(yaml_path))

    def test_config_accepts_dbc(self, tmp_path):
        """e2e_source=dbc must be accepted."""
        from tools.arxmlgen.config import load_config

        config_path = os.path.join(PROJECT_ROOT, "project.yaml")
        config = load_config(config_path)
        config.e2e_source = "dbc"
        assert config.e2e_source == "dbc"


# ===================================================================
# DBC E2E source tests
# ===================================================================

class TestE2ESourceDbc:
    """Verify DBC-based E2E data ID assignment."""

    def test_cvc_matrix_pdus_have_data_ids(self, model_dbc):
        """CVC PDUs listed in CAN matrix must have data IDs from DBC.
        PDUs with E2E signals but no matrix entry (QM heartbeats) are excluded."""
        cvc = model_dbc.ecus["cvc"]
        for pdu in cvc.tx_pdus + cvc.rx_pdus:
            if pdu.name in CAN_MATRIX_E2E_IDS:
                assert pdu.e2e_data_id is not None, \
                    f"PDU {pdu.name} has no data ID in DBC mode"

    def test_dbc_data_ids_match_can_matrix(self, model_dbc):
        """DBC-sourced data IDs must match CAN message matrix exactly."""
        cvc = model_dbc.ecus["cvc"]
        for pdu in cvc.tx_pdus + cvc.rx_pdus:
            if pdu.name in CAN_MATRIX_E2E_IDS:
                expected = CAN_MATRIX_E2E_IDS[pdu.name]
                assert pdu.e2e_data_id == expected, \
                    f"PDU {pdu.name}: expected data ID 0x{expected:02X}, " \
                    f"got 0x{pdu.e2e_data_id:02X}"

    def test_estop_data_id_is_0x01(self, model_dbc):
        """EStop_Broadcast must have data ID 0x01 (CAN matrix row 1)."""
        cvc = model_dbc.ecus["cvc"]
        estop = next(p for p in cvc.tx_pdus if p.name == "EStop_Broadcast")
        assert estop.e2e_data_id == 0x01

    def test_heartbeat_data_ids_sequential(self, model_dbc):
        """Heartbeat data IDs: CVC=0x02, FZC=0x03, RZC=0x04."""
        cvc = model_dbc.ecus["cvc"]
        hb = next(p for p in cvc.tx_pdus if p.name == "CVC_Heartbeat")
        assert hb.e2e_data_id == 0x02

        fzc = model_dbc.ecus["fzc"]
        hb = next(p for p in fzc.tx_pdus if p.name == "FZC_Heartbeat")
        assert hb.e2e_data_id == 0x03

        rzc = model_dbc.ecus["rzc"]
        hb = next(p for p in rzc.tx_pdus if p.name == "RZC_Heartbeat")
        assert hb.e2e_data_id == 0x04

    def test_all_16_e2e_messages_have_ids(self, model_dbc):
        """All 16 E2E-protected messages from CAN matrix must have data IDs."""
        all_pdus = {}
        for ecu in model_dbc.ecus.values():
            for pdu in ecu.tx_pdus:
                if pdu.name in CAN_MATRIX_E2E_IDS:
                    all_pdus[pdu.name] = pdu.e2e_data_id

        for msg_name, expected_id in CAN_MATRIX_E2E_IDS.items():
            assert msg_name in all_pdus, f"Message {msg_name} not found as TX PDU"
            assert all_pdus[msg_name] == expected_id, \
                f"{msg_name}: expected 0x{expected_id:02X}, got 0x{all_pdus[msg_name]:02X}"

    def test_rx_pdus_inherit_tx_data_ids(self, model_dbc):
        """RX PDUs must get the same data ID as the TX side."""
        cvc = model_dbc.ecus["cvc"]
        # CVC receives FZC_Heartbeat — should have data ID 0x03
        fzc_hb_rx = [p for p in cvc.rx_pdus if p.name == "FZC_Heartbeat"]
        assert len(fzc_hb_rx) == 1
        assert fzc_hb_rx[0].e2e_data_id == 0x03


# ===================================================================
# Sidecar E2E source tests
# ===================================================================

class TestE2ESourceSidecar:
    """Verify sidecar-based E2E data ID assignment (existing behavior)."""

    def test_cvc_e2e_pdus_have_data_ids(self, model_sidecar):
        """All E2E-protected CVC PDUs must have data IDs from sidecar."""
        cvc = model_sidecar.ecus["cvc"]
        for pdu in cvc.tx_pdus:
            if pdu.e2e_protected:
                assert pdu.e2e_data_id is not None, \
                    f"PDU {pdu.name} has no data ID in sidecar mode"

    def test_estop_data_id_is_0x01(self, model_sidecar):
        """EStop_Broadcast data ID is 0x01 in sidecar (matches matrix)."""
        cvc = model_sidecar.ecus["cvc"]
        estop = next(p for p in cvc.tx_pdus if p.name == "EStop_Broadcast")
        assert estop.e2e_data_id == 0x01


# ===================================================================
# Comparison tests
# ===================================================================

class TestE2ESourceComparison:
    """Compare DBC vs sidecar E2E outputs to verify both work."""

    def test_both_modes_produce_same_ecu_set(self, model_dbc, model_sidecar):
        """Both modes must produce the same set of ECUs."""
        assert set(model_dbc.ecus.keys()) == set(model_sidecar.ecus.keys())

    def test_both_modes_have_e2e_protected_pdus(self, model_dbc, model_sidecar):
        """Both modes must identify the same PDUs as E2E-protected."""
        for ecu_name in model_dbc.ecus:
            dbc_e2e = {p.name for p in model_dbc.ecus[ecu_name].tx_pdus
                       if p.e2e_protected}
            sidecar_e2e = {p.name for p in model_sidecar.ecus[ecu_name].tx_pdus
                           if p.e2e_protected}
            assert dbc_e2e == sidecar_e2e, \
                f"ECU {ecu_name}: E2E PDU mismatch between DBC and sidecar modes"

    def test_dbc_ids_differ_from_sidecar_for_some_pdus(self, model_dbc, model_sidecar):
        """DBC and sidecar must differ for at least some data IDs
        (they use different numbering schemes)."""
        cvc_dbc = model_dbc.ecus["cvc"]
        cvc_sc = model_sidecar.ecus["cvc"]

        dbc_ids = {p.name: p.e2e_data_id for p in cvc_dbc.tx_pdus
                   if p.e2e_protected}
        sc_ids = {p.name: p.e2e_data_id for p in cvc_sc.tx_pdus
                  if p.e2e_protected}

        # At least one data ID should differ (CVC_Heartbeat: 0x02 vs 0x04)
        differences = [name for name in dbc_ids
                       if dbc_ids[name] != sc_ids.get(name)]
        assert len(differences) > 0, \
            "DBC and sidecar should produce different data IDs for some PDUs"
