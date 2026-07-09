"""
Tests for PduR_Cfg generator — validates generated PduR_Cfg.c
against the AUTOSAR PDU Router configuration contract.

PduR in this project uses a single-table RX routing model (aligned with
the BSW PduR_ConfigType):
  RX path: CanIf RX PDU → PDUR_DEST_COM|PDUR_DEST_XCP → upper-layer PDU
  TX path: Com → CanIf directly (no PduR table entry needed)
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

    def test_has_routing_table(self, pdur_bcm):
        """Must have a PduR_RoutingTableType array (single RX routing table)."""
        assert "PduR_RoutingTableType" in pdur_bcm
        assert "bcm_pdur_routing[]" in pdur_bcm

    def test_has_aggregate_config(self, pdur_bcm):
        """Must have the PduR_ConfigType aggregate struct."""
        assert "PduR_ConfigType" in pdur_bcm
        assert "bcm_pdur_config" in pdur_bcm

    def test_routing_is_static_const(self, pdur_bcm):
        """Routing table must be static const."""
        assert re.search(r"static\s+const\s+PduR_RoutingTableType.*bcm_pdur_routing", pdur_bcm)

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

    def test_route_count_matches_rx_pdus(self, pdur_bcm, load_model):
        """Number of routing entries must match model rx_pdus count (single RX table)."""
        bcm = load_model.ecus["bcm"]
        section = re.search(
            r"bcm_pdur_routing\[\].*?=\s*\{(.*?)\};",
            pdur_bcm, re.DOTALL,
        )
        assert section, "bcm_pdur_routing array not found"
        entries = re.findall(r"\{[^}]+\}", section.group(1))
        assert len(entries) == len(bcm.rx_pdus)

    def test_routes_have_pdur_destinations(self, pdur_bcm):
        """Routing entries must specify a PDUR_DEST_* destination module."""
        section = re.search(
            r"bcm_pdur_routing\[\].*?=\s*\{(.*?)\};",
            pdur_bcm, re.DOTALL,
        )
        assert section
        body = section.group(1)
        assert "PDUR_DEST_COM" in body, "routing must include PDUR_DEST_COM entries"
        # PDUR_DEST_XCP is optional but valid; at minimum COM must be present.

    def test_routes_reference_com_rx_defines(self, pdur_bcm):
        """Routes must reference the ECU's Com RX PDU ID defines."""
        assert re.search(r"BCM_COM_RX_", pdur_bcm), \
            "routes should reference BCM_COM_RX_* defines"

    def test_routing_count_field_populated(self, pdur_bcm):
        """Aggregate config must set routingCount via sizeof pattern."""
        assert ".routingCount" in pdur_bcm
        assert "sizeof(bcm_pdur_routing)" in pdur_bcm


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

    def test_every_ecu_has_nonempty_routing(self, pdur_files):
        """Every ECU with pdur enabled must have at least one routing entry."""
        for ecu_name, files in pdur_files.items():
            content = files["PduR_Cfg.c"]
            entries = re.findall(r"\{[^}]+PDUR_DEST_", content)
            assert len(entries) > 0, f"{ecu_name} has no PduR routing entries"
