"""
Tests for Ecu_Cfg.h generator — validates generated per-ECU master config
headers against the structure of hand-written reference files.

TDD: these tests define the contract BEFORE generators are implemented.
"""

import re

import pytest


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def cfg_h_cvc(generated_files):
    return generated_files["cvc"]["Ecu_Cfg.h"]


@pytest.fixture
def cfg_h_bcm(generated_files):
    return generated_files["bcm"]["Ecu_Cfg.h"]


@pytest.fixture
def generated_files(load_model):
    """
    Run the CfgHeader generator for all ECUs and return a dict of
    { ecu_name: { filename: content } }.
    """
    from tools.arxmlgen.generators.cfg_header import CfgHeaderGenerator
    from tools.arxmlgen.engine import TemplateEngine
    from tools.arxmlgen.config import load_config
    import os

    config_path = os.path.join(
        os.path.dirname(__file__), "..", "..", "..", "project.yaml"
    )
    config = load_config(config_path)
    engine = TemplateEngine(config)
    generator = CfgHeaderGenerator()

    model = load_model
    results = {}
    for ecu_name, ecu in model.ecus.items():
        gen_config = config.generators.get("cfg")
        if not gen_config or not gen_config.enabled:
            continue
        ecu_cfg = config.ecus.get(ecu_name)
        if ecu_cfg and ecu_cfg.include_in and "cfg" not in ecu_cfg.include_in:
            continue

        output = generator.render(ecu, gen_config, engine)
        results[ecu_name] = output

    return results


# ===================================================================
# Ecu_Cfg.h — Structure Tests
# ===================================================================

class TestCfgHeaderStructure:
    """Verify structural layout of generated Ecu_Cfg.h files."""

    def test_has_generated_header(self, cfg_h_bcm):
        assert "GENERATED" in cfg_h_bcm
        assert "DO NOT EDIT" in cfg_h_bcm

    def test_has_header_guard(self, cfg_h_bcm):
        assert "#ifndef BCM_CFG_H" in cfg_h_bcm
        assert "#define BCM_CFG_H" in cfg_h_bcm
        assert "#endif" in cfg_h_bcm

    def test_cvc_has_header_guard(self, cfg_h_cvc):
        assert "#ifndef CVC_CFG_H" in cfg_h_cvc
        assert "#define CVC_CFG_H" in cfg_h_cvc


# ===================================================================
# Ecu_Cfg.h — RTE Signal IDs
# ===================================================================

class TestCfgHeaderRteSignals:
    """Verify RTE signal ID defines."""

    def test_bcm_signals_start_at_16(self, cfg_h_bcm):
        """First ECU-specific signal ID must be 16."""
        sig_defines = re.findall(r"#define\s+BCM_SIG_\w+\s+(\d+)u", cfg_h_bcm)
        assert sig_defines, "No BCM_SIG_ defines found"
        first_id = int(sig_defines[0])
        assert first_id == 16, f"First BCM signal ID is {first_id}, expected 16"

    def test_bcm_has_sig_count(self, cfg_h_bcm):
        """Must define BCM_SIG_COUNT."""
        assert re.search(r"#define\s+BCM_SIG_COUNT\s+\d+u", cfg_h_bcm)

    def test_sig_count_matches_model(self, cfg_h_bcm, load_model):
        """SIG_COUNT must equal 16 BSW + app signals from rte_signal_map."""
        model = load_model
        bcm = model.ecus["bcm"]
        app_count = sum(1 for sid in bcm.rte_signal_map.values() if sid >= 16)
        expected = 16 + app_count
        match = re.search(r"#define\s+BCM_SIG_COUNT\s+(\d+)u", cfg_h_bcm)
        assert match, "BCM_SIG_COUNT not found"
        assert int(match.group(1)) == expected

    def test_signal_ids_are_sequential(self, cfg_h_bcm):
        """Signal IDs must be sequential (no gaps), excluding SIG_COUNT."""
        # Match signal defines but exclude the SIG_COUNT define
        sig_defines = re.findall(r"#define\s+BCM_SIG_(?!COUNT)\w+\s+(\d+)u", cfg_h_bcm)
        ids = [int(x) for x in sig_defines]
        if ids:
            for i in range(1, len(ids)):
                assert ids[i] == ids[i-1] + 1, \
                    f"Gap in signal IDs: {ids[i-1]} -> {ids[i]}"

    def test_cvc_has_at_least_as_many_signals_as_bcm(self, cfg_h_cvc, cfg_h_bcm):
        """CVC should have at least as many signals as BCM (broadcast CAN)."""
        cvc_sigs = len(re.findall(r"#define\s+CVC_SIG_\w+", cfg_h_cvc))
        bcm_sigs = len(re.findall(r"#define\s+BCM_SIG_\w+", cfg_h_bcm))
        assert cvc_sigs >= bcm_sigs


# ===================================================================
# Ecu_Cfg.h — Com PDU IDs
# ===================================================================

class TestCfgHeaderComPdus:
    """Verify Com TX/RX PDU ID defines."""

    def test_bcm_tx_pdus_start_at_0(self, cfg_h_bcm):
        """TX PDU IDs start at 0."""
        tx_defines = re.findall(r"#define\s+BCM_COM_TX_\w+\s+(\d+)u", cfg_h_bcm)
        assert tx_defines, "No BCM_COM_TX_ defines found"
        assert int(tx_defines[0]) == 0

    def test_bcm_rx_pdus_start_at_0(self, cfg_h_bcm):
        """RX PDU IDs start at 0 (separate numbering from TX)."""
        rx_defines = re.findall(r"#define\s+BCM_COM_RX_\w+\s+(\d+)u", cfg_h_bcm)
        assert rx_defines, "No BCM_COM_RX_ defines found"
        assert int(rx_defines[0]) == 0

    def test_tx_pdu_count_matches_model(self, cfg_h_bcm, load_model):
        """Number of TX PDU defines must match model."""
        model = load_model
        bcm = model.ecus["bcm"]
        tx_defines = re.findall(r"#define\s+BCM_COM_TX_\w+\s+\d+u", cfg_h_bcm)
        assert len(tx_defines) == len(bcm.tx_pdus)

    def test_rx_pdu_count_matches_model(self, cfg_h_bcm, load_model):
        """Number of RX PDU defines must match model."""
        model = load_model
        bcm = model.ecus["bcm"]
        rx_defines = re.findall(r"#define\s+BCM_COM_RX_\w+\s+\d+u", cfg_h_bcm)
        assert len(rx_defines) == len(bcm.rx_pdus)

    def test_pdu_defines_have_can_id_comment(self, cfg_h_bcm):
        """Each PDU define should have a CAN ID comment."""
        pdu_lines = re.findall(
            r"#define\s+BCM_COM_[TR]X_\w+\s+\d+u\s+/\*.*0x[0-9A-Fa-f]+.*\*/",
            cfg_h_bcm
        )
        total_pdus = len(re.findall(r"#define\s+BCM_COM_[TR]X_\w+", cfg_h_bcm))
        assert len(pdu_lines) == total_pdus, \
            f"Only {len(pdu_lines)}/{total_pdus} PDU defines have CAN ID comments"


# ===================================================================
# Ecu_Cfg.h — DTC Event IDs (CVC only)
# ===================================================================

class TestCfgHeaderDtcs:
    """Verify DTC event ID defines for ECUs that have them."""

    def test_cvc_has_dtc_defines(self, cfg_h_cvc):
        """CVC must have DTC event defines."""
        dtc_defines = re.findall(r"#define\s+CVC_DTC_\w+\s+\d+u", cfg_h_cvc)
        assert len(dtc_defines) > 0

    def test_cvc_dtc_count_matches_sidecar(self, cfg_h_cvc, load_model):
        """DTC count must match sidecar data."""
        model = load_model
        cvc = model.ecus["cvc"]
        expected = len(cvc.dtc_events)
        dtc_defines = re.findall(r"#define\s+CVC_DTC_\w+\s+\d+u", cfg_h_cvc)
        assert len(dtc_defines) == expected

    def test_bcm_has_no_dtc_defines(self, cfg_h_bcm):
        """BCM has no DTCs in sidecar, so no DTC defines."""
        dtc_defines = re.findall(r"#define\s+BCM_DTC_\w+", cfg_h_bcm)
        assert len(dtc_defines) == 0


# ===================================================================
# Ecu_Cfg.h — E2E Data IDs
# ===================================================================

class TestCfgHeaderE2e:
    """Verify E2E data ID defines."""

    def test_cvc_has_e2e_defines(self, cfg_h_cvc):
        """CVC must have E2E data ID defines."""
        e2e_defines = re.findall(r"#define\s+CVC_E2E_\w+\s+0x[0-9A-Fa-f]+u", cfg_h_cvc)
        assert len(e2e_defines) > 0

    def test_cvc_e2e_count_matches_sidecar(self, cfg_h_cvc, load_model):
        """E2E data ID count must match sidecar."""
        model = load_model
        cvc = model.ecus["cvc"]
        expected = len(cvc.e2e_data_ids)
        e2e_defines = re.findall(r"#define\s+CVC_E2E_\w+DATA_ID\s+", cfg_h_cvc)
        assert len(e2e_defines) == expected


# ===================================================================
# Ecu_Cfg.h — Enums and Thresholds
# ===================================================================

class TestCfgHeaderEnumsThresholds:
    """Verify enum and threshold defines from sidecar."""

    def test_cvc_has_enum_defines(self, cfg_h_cvc, load_model):
        """CVC must have enum defines from sidecar."""
        model = load_model
        cvc = model.ecus["cvc"]
        for name in cvc.enums:
            assert re.search(rf"#define\s+{name}\s+", cfg_h_cvc), \
                f"Missing enum define: {name}"

    def test_cvc_has_threshold_defines(self, cfg_h_cvc, load_model):
        """CVC must have threshold defines from sidecar."""
        model = load_model
        cvc = model.ecus["cvc"]
        for name in cvc.thresholds:
            assert re.search(rf"#define\s+{name}\s+", cfg_h_cvc), \
                f"Missing threshold define: {name}"

    def test_bcm_has_threshold_defines(self, cfg_h_bcm, load_model):
        """BCM must have threshold defines."""
        model = load_model
        bcm = model.ecus["bcm"]
        for name in bcm.thresholds:
            assert re.search(rf"#define\s+{name}\s+", cfg_h_bcm), \
                f"Missing threshold define: {name}"


# ===================================================================
# All ECUs
# ===================================================================

class TestCfgHeaderAllEcus:
    """Verify all ECUs get their config header."""

    def test_all_ecus_have_output(self, generated_files):
        """All 7 ECUs must get a config header -- SC needs PDU defines
        for CanIf and E2E configs."""
        expected = {"cvc", "fzc", "rzc", "sc", "bcm", "icu", "tcu"}
        assert set(generated_files.keys()) >= expected

    def test_each_ecu_uses_own_prefix_in_guard(self, generated_files):
        """Each ECU's header guard must use its own prefix."""
        for ecu_name, files in generated_files.items():
            content = files["Ecu_Cfg.h"]
            prefix = ecu_name.upper()
            assert f"#ifndef {prefix}_CFG_H" in content
