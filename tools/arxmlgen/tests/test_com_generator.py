"""
Tests for Com_Cfg generator — validates generated Com_Cfg.h and Com_Cfg.c
against the structure of hand-written reference files.

TDD: these tests define the contract BEFORE generators are implemented.
"""

import re

import pytest


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def com_cfg_c_cvc(generated_files):
    """Return generated Com_Cfg_Cvc.c content."""
    return generated_files["cvc"]["Com_Cfg.c"]


@pytest.fixture
def com_cfg_c_bcm(generated_files):
    """Return generated Com_Cfg_Bcm.c content."""
    return generated_files["bcm"]["Com_Cfg.c"]


@pytest.fixture
def generated_files(load_model):
    """
    Run the Com generator for all ECUs and return a dict of
    { ecu_name: { filename: content } }.

    This fixture will be wired to the real generator once implemented.
    For now it imports the generator module directly.
    """
    from tools.arxmlgen.generators.com_cfg import ComCfgGenerator
    from tools.arxmlgen.engine import TemplateEngine
    from tools.arxmlgen.config import load_config
    import os

    config_path = os.path.join(
        os.path.dirname(__file__), "..", "..", "..", "project.yaml"
    )
    config = load_config(config_path)
    engine = TemplateEngine(config)
    generator = ComCfgGenerator()

    model = load_model
    results = {}
    for ecu_name, ecu in model.ecus.items():
        gen_config = config.generators.get("com")
        if not gen_config or not gen_config.enabled:
            continue
        ecu_cfg = config.ecus.get(ecu_name)
        if ecu_cfg and ecu_cfg.include_in and "com" not in ecu_cfg.include_in:
            continue

        output = generator.render(ecu, gen_config, engine)
        results[ecu_name] = output

    return results


# ===================================================================
# Com_Cfg.c — Structure Tests
# ===================================================================

class TestComCfgCStructure:
    """Verify the structural layout of generated Com_Cfg_*.c files."""

    def test_has_generated_header(self, com_cfg_c_bcm):
        """Generated file must have DO NOT EDIT banner."""
        assert "GENERATED" in com_cfg_c_bcm
        assert "DO NOT EDIT" in com_cfg_c_bcm

    def test_includes_com_h(self, com_cfg_c_bcm):
        """Must include Com.h for type definitions."""
        assert '#include "Com.h"' in com_cfg_c_bcm

    def test_includes_ecu_cfg_h(self, com_cfg_c_bcm):
        """Must include the ECU-specific config header."""
        assert '#include "Bcm_Cfg.h"' in com_cfg_c_bcm

    def test_has_shadow_buffers(self, com_cfg_c_bcm):
        """Must declare static shadow buffers for signals."""
        assert re.search(r"static\s+(?:uint\d+_t|uint\d+|sint\d+_t|boolean)\s+sig_", com_cfg_c_bcm)

    def test_has_signal_config_array(self, com_cfg_c_bcm):
        """Must have a Com_SignalConfigType array."""
        assert "Com_SignalConfigType" in com_cfg_c_bcm
        assert "signal_config[]" in com_cfg_c_bcm

    def test_has_tx_pdu_config_array(self, com_cfg_c_bcm):
        """Must have a Com_TxPduConfigType array."""
        assert "Com_TxPduConfigType" in com_cfg_c_bcm
        assert "tx_pdu_config[]" in com_cfg_c_bcm

    def test_has_rx_pdu_config_array(self, com_cfg_c_bcm):
        """Must have a Com_RxPduConfigType array."""
        assert "Com_RxPduConfigType" in com_cfg_c_bcm
        assert "rx_pdu_config[]" in com_cfg_c_bcm

    def test_has_aggregate_config(self, com_cfg_c_bcm):
        """Must have the Com_ConfigType aggregate struct."""
        assert "Com_ConfigType" in com_cfg_c_bcm
        assert "bcm_com_config" in com_cfg_c_bcm

    def test_signal_count_define(self, com_cfg_c_bcm):
        """Must define signal count via sizeof."""
        assert "COM_SIGNAL_COUNT" in com_cfg_c_bcm

    def test_tx_pdu_count_define(self, com_cfg_c_bcm):
        """Must define TX PDU count."""
        assert "TX_PDU_COUNT" in com_cfg_c_bcm

    def test_rx_pdu_count_define(self, com_cfg_c_bcm):
        """Must define RX PDU count."""
        assert "RX_PDU_COUNT" in com_cfg_c_bcm


# ===================================================================
# Com_Cfg.c — Data Integrity Tests
# ===================================================================

class TestComCfgCData:
    """Verify data correctness in generated Com_Cfg files."""

    def test_bcm_tx_signal_count(self, com_cfg_c_bcm, load_model):
        """Number of TX signals in config must match model."""
        model = load_model
        bcm = model.ecus["bcm"]
        tx_signal_count = sum(len(p.signals) for p in bcm.tx_pdus)
        # Count signal entries in the array (lines with braces and commas)
        entries = re.findall(r"\{\s*\d+u,", com_cfg_c_bcm)
        total_entries = len(entries)
        rx_signal_count = sum(len(p.signals) for p in bcm.rx_pdus)
        assert total_entries == tx_signal_count + rx_signal_count

    def test_bcm_tx_pdu_count(self, com_cfg_c_bcm, load_model):
        """Number of TX PDU entries must match model."""
        model = load_model
        bcm = model.ecus["bcm"]
        # Count entries in tx_pdu_config array
        # Look for the tx_pdu_config section and count entries
        tx_section = re.search(
            r"tx_pdu_config\[\].*?=\s*\{(.*?)\};",
            com_cfg_c_bcm, re.DOTALL
        )
        assert tx_section, "tx_pdu_config array not found"
        entries = re.findall(r"\{[^}]+\}", tx_section.group(1))
        assert len(entries) == len(bcm.tx_pdus)

    def test_bcm_rx_pdu_count(self, com_cfg_c_bcm, load_model):
        """Number of RX PDU entries must match model."""
        model = load_model
        bcm = model.ecus["bcm"]
        rx_section = re.search(
            r"rx_pdu_config\[\].*?=\s*\{(.*?)\};",
            com_cfg_c_bcm, re.DOTALL
        )
        assert rx_section, "rx_pdu_config array not found"
        entries = re.findall(r"\{[^}]+\}", rx_section.group(1))
        assert len(entries) == len(bcm.rx_pdus)

    def test_shadow_buffers_are_static(self, com_cfg_c_bcm):
        """All shadow buffers must be static (not extern/global)."""
        buffer_lines = re.findall(r"^.*sig_[tr]x_.*$", com_cfg_c_bcm, re.MULTILINE)
        for line in buffer_lines:
            if line.strip().startswith("/*") or line.strip().startswith("//"):
                continue
            if "&sig_" in line:
                continue  # pointer reference in struct, not declaration
            assert "static" in line, f"Non-static buffer: {line.strip()}"

    def test_signal_config_is_const(self, com_cfg_c_bcm):
        """Signal config array must be static const."""
        assert re.search(r"static\s+const\s+Com_SignalConfigType", com_cfg_c_bcm)

    def test_tx_pdu_config_is_const(self, com_cfg_c_bcm):
        """TX PDU config must be static const."""
        assert re.search(r"static\s+const\s+Com_TxPduConfigType", com_cfg_c_bcm)

    def test_rx_pdu_config_is_const(self, com_cfg_c_bcm):
        """RX PDU config must be static const."""
        assert re.search(r"static\s+const\s+Com_RxPduConfigType", com_cfg_c_bcm)

    def test_aggregate_config_is_const_not_static(self, com_cfg_c_bcm):
        """Aggregate config must be const (externally visible) but not static."""
        match = re.search(r"^(.*Com_ConfigType\s+\w+_com_config)", com_cfg_c_bcm, re.MULTILINE)
        assert match, "Aggregate Com_ConfigType not found"
        line = match.group(1)
        assert "const" in line
        assert "static" not in line

    def test_signal_ids_are_sequential(self, com_cfg_c_bcm):
        """Signal IDs in the config array must be sequential from 0."""
        entries = re.findall(r"\{\s*(\d+)u,", com_cfg_c_bcm)
        ids = [int(x) for x in entries]
        assert ids == list(range(len(ids))), f"Non-sequential signal IDs: {ids}"

    def test_com_type_matches_bit_size(self, com_cfg_c_bcm):
        """COM type enum must match signal bit size (8->COM_UINT8, 16->COM_UINT16)."""
        # Extract (bitSize, comType) pairs from signal config entries
        entries = re.findall(
            r"\{\s*\d+u,\s*\d+u,\s*(\d+)u,\s*(COM_\w+),",
            com_cfg_c_bcm
        )
        for bit_size_str, com_type in entries:
            bit_size = int(bit_size_str)
            if bit_size <= 8:
                assert com_type in ("COM_UINT8", "COM_SINT8"), \
                    f"{bit_size}-bit signal has type {com_type}"
            elif bit_size <= 16:
                assert com_type in ("COM_UINT16", "COM_SINT16"), \
                    f"{bit_size}-bit signal has type {com_type}"
            else:
                assert com_type in ("COM_UINT32", "COM_SINT32"), \
                    f"{bit_size}-bit signal has type {com_type}"


# ===================================================================
# Com_Cfg.c — CVC (complex ECU) Tests
# ===================================================================

class TestComCfgCvc:
    """CVC-specific tests for the more complex ECU."""

    def test_cvc_has_at_least_as_many_signals_as_bcm(self, generated_files):
        """CVC should have at least as many signals as BCM (broadcast CAN)."""
        cvc = generated_files["cvc"]["Com_Cfg.c"]
        bcm = generated_files["bcm"]["Com_Cfg.c"]
        cvc_entries = len(re.findall(r"\{\s*\d+u,", cvc))
        bcm_entries = len(re.findall(r"\{\s*\d+u,", bcm))
        assert cvc_entries >= bcm_entries

    def test_cvc_includes_cvc_cfg_h(self, generated_files):
        """CVC file must include Cvc_Cfg.h."""
        cvc = generated_files["cvc"]["Com_Cfg.c"]
        assert '#include "Cvc_Cfg.h"' in cvc

    def test_cvc_aggregate_uses_cvc_prefix(self, generated_files):
        """CVC aggregate struct must use cvc_ prefix."""
        cvc = generated_files["cvc"]["Com_Cfg.c"]
        assert "cvc_com_config" in cvc


# ===================================================================
# All ECUs — Completeness Tests
# ===================================================================

class TestComCfgAllEcus:
    """Verify all ECUs get generated Com config."""

    def test_all_com_ecus_have_output(self, generated_files):
        """All ECUs with com enabled should have Com_Cfg.c output."""
        # SC has canif only, no com — so 6 ECUs
        expected = {"cvc", "fzc", "rzc", "bcm", "icu", "tcu"}
        assert set(generated_files.keys()) >= expected

    def test_each_ecu_has_unique_prefix(self, generated_files):
        """Each ECU's aggregate config must use its own prefix."""
        for ecu_name, files in generated_files.items():
            content = files["Com_Cfg.c"]
            assert f"{ecu_name}_com_config" in content

    def test_no_ecu_has_zero_signals(self, generated_files, load_model):
        """Every ECU with com enabled should have at least one signal."""
        for ecu_name, files in generated_files.items():
            content = files["Com_Cfg.c"]
            entries = re.findall(r"\{\s*\d+u,", content)
            assert len(entries) > 0, f"{ecu_name} has zero signal entries"
