"""
Tests for PduR_Cfg generator — validates generated PduR_Cfg.c
against the AUTOSAR PDU Router configuration contract.

TDD: these tests define the contract BEFORE the generator is implemented.

PduR routes PDUs between CanIf and Com:
  RX path: CanIf RX PDU → Com RX PDU  (PDUR_MOD_CANIF → PDUR_MOD_COM)
  TX path: Com TX PDU → CanIf TX PDU  (PDUR_MOD_COM → PDUR_MOD_CANIF)
"""

import re

import pytest


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def pdur_files(load_model):
    """
    Run the PduR generator for all ECUs and return a dict of
    { ecu_name: { filename: content } }.
    """
    from tools.arxmlgen.generators.pdur_cfg import PduRCfgGenerator
    from tools.arxmlgen.engine import TemplateEngine
    from tools.arxmlgen.config import load_config
    import os

    config_path = os.path.join(
        os.path.dirname(__file__), "..", "..", "..", "project.yaml"
    )
    config = load_config(config_path)
    engine = TemplateEngine(config)
    generator = PduRCfgGenerator()

    model = load_model
    results = {}
    for ecu_name, ecu in model.ecus.items():
        ecu_cfg = config.ecus.get(ecu_name)
        if ecu_cfg and ecu_cfg.include_in and "pdur" not in ecu_cfg.include_in:
            continue

        output = generator.render(ecu, config.generators.get("pdur"), engine)
        results[ecu_name] = output

    return results


@pytest.fixture
def pdur_bcm(pdur_files):
    return pdur_files["bcm"]["PduR_Cfg.c"]


@pytest.fixture
def pdur_cvc(pdur_files):
    return pdur_files["cvc"]["PduR_Cfg.c"]


# ===================================================================
# PduR_Cfg.c — Structure Tests
# ===================================================================

class TestPduRCfgStructure:
    """Verify the structural layout of generated PduR_Cfg.c files."""

    def test_has_generated_header(self, pdur_bcm):
        """Generated file must have DO NOT EDIT banner."""
        assert "GENERATED" in pdur_bcm
        assert "DO NOT EDIT" in pdur_bcm

    def test_includes_pdur_h(self, pdur_bcm):
        """Must include PduR.h for type definitions."""
        assert '#include "PduR.h"' in pdur_bcm

    def test_includes_ecu_cfg_h(self, pdur_bcm):
        """Must include the ECU-specific config header."""
        assert '#include "Bcm_Cfg.h"' in pdur_bcm

    def test_has_rx_routing_table(self, pdur_bcm):
        """Must have a PduR_RoutingPathType array for RX routes."""
        assert "PduR_RoutingPathType" in pdur_bcm
        assert "rx_routing[]" in pdur_bcm

    def test_has_tx_routing_table(self, pdur_bcm):
        """Must have a PduR_RoutingPathType array for TX routes."""
        assert "tx_routing[]" in pdur_bcm

    def test_has_aggregate_config(self, pdur_bcm):
        """Must have the PduR_ConfigType aggregate struct."""
        assert "PduR_ConfigType" in pdur_bcm
        assert "bcm_pdur_config" in pdur_bcm

    def test_rx_routing_is_static_const(self, pdur_bcm):
        """RX routing table must be static const."""
        assert re.search(r"static\s+const\s+PduR_RoutingPathType.*rx_routing", pdur_bcm)

    def test_tx_routing_is_static_const(self, pdur_bcm):
        """TX routing table must be static const."""
        assert re.search(r"static\s+const\s+PduR_RoutingPathType.*tx_routing", pdur_bcm)

    def test_aggregate_is_const_not_static(self, pdur_bcm):
        """Aggregate config must be const (externally visible) but not static."""
        match = re.search(
            r"^(.*PduR_ConfigType\s+\w+_pdur_config)",
            pdur_bcm, re.MULTILINE,
        )
        assert match, "PduR_ConfigType aggregate not found"
        line = match.group(1)
        assert "const" in line
        assert "static" not in line


# ===================================================================
# PduR_Cfg.c — Data Integrity Tests
# ===================================================================

class TestPduRCfgData:
    """Verify data correctness in generated PduR_Cfg files."""

    def test_rx_route_count_matches_rx_pdus(self, pdur_bcm, load_model):
        """Number of RX routing entries must match model rx_pdus count."""
        bcm = load_model.ecus["bcm"]
        rx_section = re.search(
            r"rx_routing\[\].*?=\s*\{(.*?)\};",
            pdur_bcm, re.DOTALL,
        )
        assert rx_section, "rx_routing array not found"
        entries = re.findall(r"\{[^}]+\}", rx_section.group(1))
        assert len(entries) == len(bcm.rx_pdus)

    def test_tx_route_count_matches_tx_pdus(self, pdur_bcm, load_model):
        """Number of TX routing entries must match model tx_pdus count."""
        bcm = load_model.ecus["bcm"]
        tx_section = re.search(
            r"tx_routing\[\].*?=\s*\{(.*?)\};",
            pdur_bcm, re.DOTALL,
        )
        assert tx_section, "tx_routing array not found"
        entries = re.findall(r"\{[^}]+\}", tx_section.group(1))
        assert len(entries) == len(bcm.tx_pdus)

    def test_rx_routes_use_canif_to_com(self, pdur_bcm):
        """RX routes must go from CANIF → COM."""
        rx_section = re.search(
            r"rx_routing\[\].*?=\s*\{(.*?)\};",
            pdur_bcm, re.DOTALL,
        )
        assert rx_section
        section = rx_section.group(1)
        assert "PDUR_MOD_CANIF" in section, "RX source should be CANIF"
        assert "PDUR_MOD_COM" in section, "RX destination should be COM"

    def test_tx_routes_use_com_to_canif(self, pdur_bcm):
        """TX routes must go from COM → CANIF."""
        tx_section = re.search(
            r"tx_routing\[\].*?=\s*\{(.*?)\};",
            pdur_bcm, re.DOTALL,
        )
        assert tx_section
        section = tx_section.group(1)
        assert "PDUR_MOD_COM" in section, "TX source should be COM"
        assert "PDUR_MOD_CANIF" in section, "TX destination should be CANIF"

    def test_rx_routes_reference_com_rx_defines(self, pdur_bcm):
        """RX routes should use COM RX PDU defines from Ecu_Cfg.h."""
        assert re.search(r"BCM_COM_RX_", pdur_bcm), \
            "RX routes should reference BCM_COM_RX_* defines"

    def test_tx_routes_reference_com_tx_defines(self, pdur_bcm):
        """TX routes should use COM TX PDU defines from Ecu_Cfg.h."""
        assert re.search(r"BCM_COM_TX_", pdur_bcm), \
            "TX routes should reference BCM_COM_TX_* defines"

    def test_rx_route_count_define(self, pdur_bcm):
        """Must define RX route count."""
        assert "PDUR_RX_ROUTE_COUNT" in pdur_bcm

    def test_tx_route_count_define(self, pdur_bcm):
        """Must define TX route count."""
        assert "PDUR_TX_ROUTE_COUNT" in pdur_bcm


# ===================================================================
# PduR_Cfg.c — Cross-ECU Tests
# ===================================================================

class TestPduRCfgAllEcus:
    """Verify all ECUs with pdur enabled get generated config."""

    def test_pdur_ecus_have_output(self, pdur_files):
        """6 ECUs (not SC) should have PduR config."""
        expected = {"cvc", "fzc", "rzc", "bcm", "icu", "tcu"}
        assert set(pdur_files.keys()) >= expected

    def test_sc_not_in_pdur(self, pdur_files):
        """SC only has canif — should NOT have PduR config."""
        assert "sc" not in pdur_files

    def test_each_ecu_has_own_prefix(self, pdur_files):
        """Each ECU's aggregate config must use its own prefix."""
        for ecu_name, files in pdur_files.items():
            content = files["PduR_Cfg.c"]
            assert f"{ecu_name}_pdur_config" in content

    def test_cvc_has_at_least_as_many_routes_as_bcm(self, pdur_cvc, pdur_bcm):
        """CVC should have >= BCM routing entries (broadcast CAN)."""
        cvc_entries = len(re.findall(r"\{[^}]+PDUR_MOD", pdur_cvc))
        bcm_entries = len(re.findall(r"\{[^}]+PDUR_MOD", pdur_bcm))
        assert cvc_entries >= bcm_entries
