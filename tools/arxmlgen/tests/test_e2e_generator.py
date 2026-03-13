"""
Tests for E2E_Cfg generator — validates generated E2E_Cfg.c
against the Taktflow simplified AUTOSAR E2E configuration contract.

TDD: these tests define the contract BEFORE the generator is implemented.

E2E_Cfg maps protected PDUs to data IDs, counter/CRC bit positions.
All ECUs get E2E config -- SC must verify E2E as safety monitor.
"""

import re

import pytest


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def e2e_files(load_model):
    """
    Run the E2E generator for all ECUs and return a dict of
    { ecu_name: { filename: content } }.
    """
    from tools.arxmlgen.generators.e2e_cfg import E2ECfgGenerator
    from tools.arxmlgen.engine import TemplateEngine
    from tools.arxmlgen.config import load_config
    import os

    config_path = os.path.join(
        os.path.dirname(__file__), "..", "..", "..", "project.yaml"
    )
    config = load_config(config_path)
    engine = TemplateEngine(config)
    generator = E2ECfgGenerator()

    model = load_model
    results = {}
    for ecu_name, ecu in model.ecus.items():
        ecu_cfg = config.ecus.get(ecu_name)
        if ecu_cfg and ecu_cfg.include_in and "e2e" not in ecu_cfg.include_in:
            continue

        output = generator.render(ecu, config.generators.get("e2e"), engine)
        results[ecu_name] = output

    return results


@pytest.fixture
def e2e_cvc(e2e_files):
    return e2e_files["cvc"]["E2E_Cfg.c"]


@pytest.fixture
def e2e_fzc(e2e_files):
    return e2e_files["fzc"]["E2E_Cfg.c"]


# ===================================================================
# E2E_Cfg.c — Structure Tests
# ===================================================================

class TestE2ECfgStructure:
    """Verify the structural layout of generated E2E_Cfg.c files."""

    def test_has_generated_header(self, e2e_cvc):
        """Generated file must have DO NOT EDIT banner."""
        assert "GENERATED" in e2e_cvc
        assert "DO NOT EDIT" in e2e_cvc

    def test_includes_e2e_h(self, e2e_cvc):
        """Must include E2E.h for type definitions."""
        assert '#include "E2E.h"' in e2e_cvc

    def test_includes_ecu_cfg_h(self, e2e_cvc):
        """Must include the ECU-specific config header for data ID defines."""
        assert '#include "Cvc_Cfg.h"' in e2e_cvc

    def test_defines_pdu_protect_struct(self, e2e_cvc):
        """Must define the E2E_PduProtectCfgType struct."""
        assert "E2E_PduProtectCfgType" in e2e_cvc

    def test_has_tx_protect_config(self, e2e_cvc):
        """Must have a TX protection config array."""
        assert "e2e_tx_config[]" in e2e_cvc

    def test_has_rx_protect_config(self, e2e_cvc):
        """Must have an RX protection config array."""
        assert "e2e_rx_config[]" in e2e_cvc

    def test_has_aggregate_config(self, e2e_cvc):
        """Must have the E2E_ConfigType aggregate struct."""
        assert "E2E_ConfigType" in e2e_cvc
        assert "cvc_e2e_config" in e2e_cvc

    def test_tx_config_is_static_const(self, e2e_cvc):
        """TX E2E config must be static const."""
        assert re.search(r"static\s+const\s+E2E_PduProtectCfgType.*tx_config", e2e_cvc)

    def test_rx_config_is_static_const(self, e2e_cvc):
        """RX E2E config must be static const."""
        assert re.search(r"static\s+const\s+E2E_PduProtectCfgType.*rx_config", e2e_cvc)

    def test_aggregate_is_const_not_static(self, e2e_cvc):
        """Aggregate config must be const (externally visible) but not static."""
        match = re.search(
            r"^(.*E2E_ConfigType\s+\w+_e2e_config)",
            e2e_cvc, re.MULTILINE,
        )
        assert match, "E2E_ConfigType aggregate not found"
        line = match.group(1)
        assert "const" in line
        assert "static" not in line


# ===================================================================
# E2E_Cfg.c — Data Integrity Tests
# ===================================================================

class TestE2ECfgData:
    """Verify data correctness in generated E2E_Cfg files."""

    def test_tx_protected_pdu_count(self, e2e_cvc, load_model):
        """TX config entries must match number of E2E-protected TX PDUs."""
        cvc = load_model.ecus["cvc"]
        e2e_tx = [p for p in cvc.tx_pdus if p.e2e_protected]
        tx_section = re.search(
            r"e2e_tx_config\[\].*?=\s*\{(.*?)\};",
            e2e_cvc, re.DOTALL,
        )
        assert tx_section, "e2e_tx_config array not found"
        entries = re.findall(r"\{[^}]+\}", tx_section.group(1))
        assert len(entries) == len(e2e_tx)

    def test_rx_protected_pdu_count(self, e2e_cvc, load_model):
        """RX config entries must match number of E2E-protected RX PDUs."""
        cvc = load_model.ecus["cvc"]
        e2e_rx = [p for p in cvc.rx_pdus if p.e2e_protected]
        rx_section = re.search(
            r"e2e_rx_config\[\].*?=\s*\{(.*?)\};",
            e2e_cvc, re.DOTALL,
        )
        assert rx_section, "e2e_rx_config array not found"
        entries = re.findall(r"\{[^}]+\}", rx_section.group(1))
        assert len(entries) == len(e2e_rx)

    def test_data_ids_in_hex(self, e2e_cvc):
        """E2E data IDs must appear in hex format."""
        assert re.search(r"0x[0-9A-Fa-f]+u", e2e_cvc)

    def test_tx_entries_reference_com_tx_defines(self, e2e_cvc):
        """TX entries should reference COM TX PDU defines."""
        tx_section = re.search(
            r"e2e_tx_config\[\].*?=\s*\{(.*?)\};",
            e2e_cvc, re.DOTALL,
        )
        if tx_section and tx_section.group(1).strip():
            assert "CVC_COM_TX_" in tx_section.group(1)

    def test_rx_entries_reference_com_rx_defines(self, e2e_cvc):
        """RX entries should reference COM RX PDU defines."""
        rx_section = re.search(
            r"e2e_rx_config\[\].*?=\s*\{(.*?)\};",
            e2e_cvc, re.DOTALL,
        )
        if rx_section and rx_section.group(1).strip():
            assert "CVC_COM_RX_" in rx_section.group(1)

    def test_data_ids_match_pdu_model(self, e2e_cvc, load_model):
        """Data IDs in config must match PDU e2e_data_id from model."""
        cvc = load_model.ecus["cvc"]
        for pdu in cvc.tx_pdus + cvc.rx_pdus:
            if pdu.e2e_protected and pdu.e2e_data_id is not None:
                hex_id = f"0x{pdu.e2e_data_id:02X}"
                assert hex_id in e2e_cvc, \
                    f"Missing E2E data ID {hex_id} for PDU {pdu.name}"

    def test_tx_count_define(self, e2e_cvc):
        """Must define E2E TX protect count."""
        assert "E2E_TX_PROTECT_COUNT" in e2e_cvc

    def test_rx_count_define(self, e2e_cvc):
        """Must define E2E RX protect count."""
        assert "E2E_RX_PROTECT_COUNT" in e2e_cvc

    def test_cvc_has_protected_pdus(self, e2e_cvc, load_model):
        """CVC must have at least one E2E-protected PDU (safety-critical ECU)."""
        cvc = load_model.ecus["cvc"]
        e2e_count = sum(1 for p in cvc.tx_pdus + cvc.rx_pdus if p.e2e_protected)
        assert e2e_count > 0, "CVC should have E2E-protected PDUs"


# ===================================================================
# E2E_Cfg.c — Cross-ECU Tests
# ===================================================================

class TestE2ECfgAllEcus:
    """Verify all ECUs with e2e enabled get generated config."""

    def test_all_ecus_have_e2e_output(self, e2e_files):
        """All 7 ECUs must have E2E config -- SC is a safety monitor that
        MUST verify E2E on incoming messages (ISO 26262 requirement)."""
        expected = {"cvc", "fzc", "rzc", "sc", "bcm", "icu", "tcu"}
        assert set(e2e_files.keys()) >= expected

    def test_sc_included_in_e2e(self, e2e_files):
        """SC (safety controller) must have E2E config -- it monitors
        safety-critical signals and must verify CRC/alive counters."""
        assert "sc" in e2e_files, "SC must have E2E config (safety monitor)"
        content = e2e_files["sc"]["E2E_Cfg.c"]
        assert "E2E_ConfigType" in content
        assert "sc_e2e_config" in content

    def test_each_ecu_has_own_prefix(self, e2e_files):
        """Each ECU's aggregate config must use its own prefix."""
        for ecu_name, files in e2e_files.items():
            content = files["E2E_Cfg.c"]
            assert f"{ecu_name}_e2e_config" in content

    def test_fzc_has_protected_pdus(self, e2e_fzc, load_model):
        """FZC (front zone) must have E2E-protected PDUs."""
        fzc = load_model.ecus["fzc"]
        e2e_count = sum(1 for p in fzc.tx_pdus + fzc.rx_pdus if p.e2e_protected)
        assert e2e_count > 0
