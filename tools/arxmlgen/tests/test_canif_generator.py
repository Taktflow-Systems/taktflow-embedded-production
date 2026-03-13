"""
Tests for CanIf_Cfg generator — validates generated CanIf_Cfg.c
against the AUTOSAR CanIf configuration contract.

TDD: these tests define the contract BEFORE the generator is implemented.

CanIf maps CAN IDs ↔ upper-layer (Com) PDU IDs.
Key invariant: CanIf PDU IDs mirror Com PDU IDs 1:1.
"""

import re

import pytest


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def canif_files(load_model):
    """
    Run the CanIf generator for all ECUs and return a dict of
    { ecu_name: { filename: content } }.
    """
    from tools.arxmlgen.generators.canif_cfg import CanIfCfgGenerator
    from tools.arxmlgen.engine import TemplateEngine
    from tools.arxmlgen.config import load_config
    import os

    config_path = os.path.join(
        os.path.dirname(__file__), "..", "..", "..", "project.yaml"
    )
    config = load_config(config_path)
    engine = TemplateEngine(config)
    generator = CanIfCfgGenerator()

    model = load_model
    results = {}
    for ecu_name, ecu in model.ecus.items():
        ecu_cfg = config.ecus.get(ecu_name)
        if ecu_cfg and ecu_cfg.include_in and "canif" not in ecu_cfg.include_in:
            continue

        output = generator.render(ecu, config.generators.get("canif"), engine)
        results[ecu_name] = output

    return results


@pytest.fixture
def canif_bcm(canif_files):
    return canif_files["bcm"]["CanIf_Cfg.c"]


@pytest.fixture
def canif_cvc(canif_files):
    return canif_files["cvc"]["CanIf_Cfg.c"]


@pytest.fixture
def canif_sc(canif_files):
    return canif_files["sc"]["CanIf_Cfg.c"]


# ===================================================================
# CanIf_Cfg.c — Structure Tests
# ===================================================================

class TestCanIfCfgStructure:
    """Verify the structural layout of generated CanIf_Cfg.c files."""

    def test_has_generated_header(self, canif_bcm):
        """Generated file must have DO NOT EDIT banner."""
        assert "GENERATED" in canif_bcm
        assert "DO NOT EDIT" in canif_bcm

    def test_includes_canif_h(self, canif_bcm):
        """Must include CanIf.h for type definitions."""
        assert '#include "CanIf.h"' in canif_bcm

    def test_includes_ecu_cfg_h(self, canif_bcm):
        """Must include the ECU-specific config header."""
        assert '#include "Bcm_Cfg.h"' in canif_bcm

    def test_has_tx_pdu_config_array(self, canif_bcm):
        """Must have a CanIf_TxPduCfgType array."""
        assert "CanIf_TxPduCfgType" in canif_bcm
        assert "tx_pdu_config[]" in canif_bcm

    def test_has_rx_pdu_config_array(self, canif_bcm):
        """Must have a CanIf_RxPduCfgType array."""
        assert "CanIf_RxPduCfgType" in canif_bcm
        assert "rx_pdu_config[]" in canif_bcm

    def test_has_aggregate_config(self, canif_bcm):
        """Must have the CanIf_ConfigType aggregate struct."""
        assert "CanIf_ConfigType" in canif_bcm
        assert "bcm_canif_config" in canif_bcm

    def test_tx_config_is_static_const(self, canif_bcm):
        """TX PDU config must be static const."""
        assert re.search(r"static\s+const\s+CanIf_TxPduCfgType", canif_bcm)

    def test_rx_config_is_static_const(self, canif_bcm):
        """RX PDU config must be static const."""
        assert re.search(r"static\s+const\s+CanIf_RxPduCfgType", canif_bcm)

    def test_aggregate_is_const_not_static(self, canif_bcm):
        """Aggregate config must be const (externally visible) but not static."""
        match = re.search(
            r"^(.*CanIf_ConfigType\s+\w+_canif_config)",
            canif_bcm, re.MULTILINE,
        )
        assert match, "CanIf_ConfigType aggregate not found"
        line = match.group(1)
        assert "const" in line
        assert "static" not in line


# ===================================================================
# CanIf_Cfg.c — Data Integrity Tests
# ===================================================================

class TestCanIfCfgData:
    """Verify data correctness in generated CanIf_Cfg files."""

    def test_tx_pdu_count_matches_model(self, canif_bcm, load_model):
        """Number of TX PDU entries must match model tx_pdus count."""
        bcm = load_model.ecus["bcm"]
        tx_section = re.search(
            r"tx_pdu_config\[\].*?=\s*\{(.*?)\};",
            canif_bcm, re.DOTALL,
        )
        assert tx_section, "tx_pdu_config array not found"
        entries = re.findall(r"\{[^}]+\}", tx_section.group(1))
        assert len(entries) == len(bcm.tx_pdus)

    def test_rx_pdu_count_matches_model(self, canif_bcm, load_model):
        """Number of RX PDU entries must match model rx_pdus count."""
        bcm = load_model.ecus["bcm"]
        rx_section = re.search(
            r"rx_pdu_config\[\].*?=\s*\{(.*?)\};",
            canif_bcm, re.DOTALL,
        )
        assert rx_section, "rx_pdu_config array not found"
        entries = re.findall(r"\{[^}]+\}", rx_section.group(1))
        assert len(entries) == len(bcm.rx_pdus)

    def test_can_ids_in_hex(self, canif_bcm):
        """CAN IDs must appear in hex format (0x...)."""
        assert re.search(r"0x[0-9A-Fa-f]+u", canif_bcm)

    def test_tx_can_ids_match_model(self, canif_bcm, load_model):
        """TX CAN IDs in CanIf must match model PDU CAN IDs."""
        bcm = load_model.ecus["bcm"]
        expected_ids = {f"0x{p.can_id:03X}" for p in bcm.tx_pdus}
        for cid in expected_ids:
            assert cid in canif_bcm, f"Missing TX CAN ID {cid} in CanIf config"

    def test_rx_can_ids_match_model(self, canif_bcm, load_model):
        """RX CAN IDs in CanIf must match model PDU CAN IDs."""
        bcm = load_model.ecus["bcm"]
        expected_ids = {f"0x{p.can_id:03X}" for p in bcm.rx_pdus}
        for cid in expected_ids:
            assert cid in canif_bcm, f"Missing RX CAN ID {cid} in CanIf config"

    def test_dlc_values_present(self, canif_bcm, load_model):
        """DLC values must appear in the config entries."""
        bcm = load_model.ecus["bcm"]
        for pdu in bcm.tx_pdus + bcm.rx_pdus:
            assert f"{pdu.dlc}u" in canif_bcm

    def test_tx_pdu_count_define(self, canif_bcm):
        """Must define TX PDU count."""
        assert "CANIF_TX_PDU_COUNT" in canif_bcm

    def test_rx_pdu_count_define(self, canif_bcm):
        """Must define RX PDU count."""
        assert "CANIF_RX_PDU_COUNT" in canif_bcm

    def test_upper_pdu_ids_reference_com_defines(self, canif_bcm):
        """Upper-layer PDU IDs should reference COM PDU defines from Ecu_Cfg.h."""
        # CanIf TX maps to COM TX PDU IDs, should use BCM_COM_TX_* defines
        assert re.search(r"BCM_COM_TX_", canif_bcm), \
            "TX entries should reference BCM_COM_TX_* defines"
        assert re.search(r"BCM_COM_RX_", canif_bcm), \
            "RX entries should reference BCM_COM_RX_* defines"


# ===================================================================
# CanIf_Cfg.c — Cross-ECU Tests
# ===================================================================

class TestCanIfCfgAllEcus:
    """Verify all ECUs get generated CanIf config."""

    def test_all_canif_ecus_have_output(self, canif_files):
        """All 7 ECUs (including SC) should have CanIf config."""
        expected = {"cvc", "fzc", "rzc", "sc", "bcm", "icu", "tcu"}
        assert set(canif_files.keys()) >= expected

    def test_each_ecu_has_own_prefix(self, canif_files):
        """Each ECU's aggregate config must use its own prefix."""
        for ecu_name, files in canif_files.items():
            content = files["CanIf_Cfg.c"]
            assert f"{ecu_name}_canif_config" in content

    def test_sc_has_canif_output(self, canif_sc):
        """SC (canif-only ECU) must still get valid CanIf config."""
        assert "CanIf_ConfigType" in canif_sc
        assert "sc_canif_config" in canif_sc

    def test_cvc_has_at_least_as_many_pdus_as_bcm(self, canif_cvc, canif_bcm):
        """CVC should have >= BCM PDU count (broadcast CAN)."""
        cvc_entries = len(re.findall(r"\{[^}]*0x[0-9A-Fa-f]+", canif_cvc))
        bcm_entries = len(re.findall(r"\{[^}]*0x[0-9A-Fa-f]+", canif_bcm))
        assert cvc_entries >= bcm_entries

    def test_no_ecu_has_empty_tx_and_rx(self, canif_files, load_model):
        """Every ECU should have at least one TX or RX PDU."""
        for ecu_name, files in canif_files.items():
            ecu = load_model.ecus[ecu_name]
            total = len(ecu.tx_pdus) + len(ecu.rx_pdus)
            assert total > 0, f"{ecu_name} has zero PDUs"
