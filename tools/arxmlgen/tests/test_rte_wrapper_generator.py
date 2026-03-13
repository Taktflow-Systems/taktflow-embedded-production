"""
Tests for Phase 3: Per-SWC typed RTE wrapper headers (Rte_<SwcName>.h).

TDD: these tests define the contract BEFORE the generator is implemented.
Tests will FAIL until the Rte_Ecu.h.j2 template and generator extension exist.

Generated headers provide type-safe inline wrappers:
  - Rte_Read_<Signal>(ptr) → Rte_Read(ID, ptr) with correct C type
  - Rte_Write_<Signal>(val) → Rte_Write(ID, val) with correct C type
"""

import re

import pytest


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def rte_wrapper_files(load_model):
    """
    Run the RTE wrapper generator for all ECUs and return a dict of
    { ecu_name: { swc_name: content } }.
    """
    from tools.arxmlgen.generators.rte_cfg import RteCfgGenerator
    from tools.arxmlgen.engine import TemplateEngine
    from tools.arxmlgen.config import load_config
    import os

    config_path = os.path.join(
        os.path.dirname(__file__), "..", "..", "..", "project.yaml"
    )
    config = load_config(config_path)
    engine = TemplateEngine(config)
    generator = RteCfgGenerator()

    model = load_model
    results = {}
    for ecu_name, ecu in model.ecus.items():
        gen_config = config.generators.get("rte")
        if not gen_config or not gen_config.enabled:
            continue
        ecu_cfg = config.ecus.get(ecu_name)
        if ecu_cfg and ecu_cfg.include_in and "rte" not in ecu_cfg.include_in:
            continue

        # render_wrappers returns { "Rte_Swc_Pedal.h": content, ... }
        output = generator.render_wrappers(ecu, gen_config, engine)
        results[ecu_name] = output

    return results


@pytest.fixture
def cvc_wrappers(rte_wrapper_files):
    """All CVC wrapper files."""
    return rte_wrapper_files["cvc"]


@pytest.fixture
def bcm_wrappers(rte_wrapper_files):
    """All BCM wrapper files."""
    return rte_wrapper_files["bcm"]


@pytest.fixture
def fzc_wrappers(rte_wrapper_files):
    """All FZC wrapper files."""
    return rte_wrapper_files["fzc"]


# ===================================================================
# Structure Tests — per-SWC header layout
# ===================================================================

class TestRteWrapperStructure:
    """Verify structural layout of generated Rte_<Swc>.h files."""

    def test_cvc_has_wrapper_files(self, cvc_wrappers):
        """CVC must generate at least one per-SWC wrapper header."""
        assert len(cvc_wrappers) > 0

    def test_wrapper_filenames_match_swc_names(self, cvc_wrappers):
        """Each wrapper file must be named Rte_Swc_<Name>.h."""
        for filename in cvc_wrappers:
            assert filename.startswith("Rte_Swc_"), f"Bad filename: {filename}"
            assert filename.endswith(".h"), f"Not a header: {filename}"

    def test_has_generated_header(self, cvc_wrappers):
        """Each wrapper must have DO NOT EDIT banner."""
        for filename, content in cvc_wrappers.items():
            assert "GENERATED" in content, f"{filename} missing GENERATED"
            assert "DO NOT EDIT" in content, f"{filename} missing DO NOT EDIT"

    def test_has_header_guard(self, cvc_wrappers):
        """Each wrapper must have include guard."""
        for filename, content in cvc_wrappers.items():
            guard_name = filename.replace(".", "_").upper()
            assert f"#ifndef {guard_name}" in content, \
                f"{filename} missing guard #ifndef {guard_name}"
            assert f"#define {guard_name}" in content, \
                f"{filename} missing guard #define {guard_name}"
            assert f"#endif" in content, f"{filename} missing #endif"

    def test_includes_rte_h(self, cvc_wrappers):
        """Each wrapper must include Rte.h for base API."""
        for filename, content in cvc_wrappers.items():
            assert '#include "Rte.h"' in content, \
                f"{filename} missing Rte.h include"

    def test_includes_ecu_cfg_h(self, cvc_wrappers):
        """Each wrapper must include the ECU config header for signal IDs."""
        for filename, content in cvc_wrappers.items():
            assert '#include "Cvc_Cfg.h"' in content, \
                f"{filename} missing Cvc_Cfg.h include"

    def test_bcm_includes_bcm_cfg_h(self, bcm_wrappers):
        """BCM wrappers must include Bcm_Cfg.h."""
        for filename, content in bcm_wrappers.items():
            assert '#include "Bcm_Cfg.h"' in content, \
                f"{filename} missing Bcm_Cfg.h include"


# ===================================================================
# Typed Read Wrappers
# ===================================================================

class TestRteWrapperReads:
    """Verify Rte_Read_<Signal> inline functions."""

    def test_has_read_wrappers(self, cvc_wrappers):
        """At least one wrapper file must contain Rte_Read_ functions."""
        has_read = any(
            "Rte_Read_" in content
            for content in cvc_wrappers.values()
        )
        assert has_read, "No Rte_Read_ wrappers found in CVC"

    def test_read_is_static_inline(self, cvc_wrappers):
        """Read wrappers must be static inline functions."""
        for filename, content in cvc_wrappers.items():
            reads = re.findall(r"^.*Rte_Read_\w+.*$", content, re.MULTILINE)
            for line in reads:
                if line.strip().startswith("/*") or line.strip().startswith("//"):
                    continue
                if line.strip().startswith("#"):
                    continue
                assert "static" in line and "inline" in line, \
                    f"{filename}: Read wrapper not static inline: {line.strip()}"

    def test_read_returns_rte_status(self, cvc_wrappers):
        """Read wrappers must return Rte_StatusType."""
        for filename, content in cvc_wrappers.items():
            # Match function signatures
            reads = re.findall(
                r"static\s+inline\s+(\w+)\s+Rte_Read_\w+",
                content
            )
            for ret_type in reads:
                assert ret_type == "Rte_StatusType", \
                    f"{filename}: Read returns {ret_type}, expected Rte_StatusType"

    def test_read_takes_typed_pointer(self, cvc_wrappers):
        """Read wrappers must take a valid C type pointer parameter."""
        valid_types = {"uint8_t", "uint16_t", "sint16_t",
                       "uint32_t", "sint32_t", "boolean"}
        for filename, content in cvc_wrappers.items():
            reads = re.findall(
                r"Rte_Read_\w+\s*\(\s*(\w+)\s*\*",
                content
            )
            for param_type in reads:
                assert param_type in valid_types, \
                    f"{filename}: Unknown type {param_type}"

    def test_read_calls_rte_read(self, cvc_wrappers):
        """Read wrappers must delegate to Rte_Read() with signal ID."""
        for filename, content in cvc_wrappers.items():
            if "Rte_Read_" not in content:
                continue
            # The wrapper body must call Rte_Read(ID, &tmp)
            assert re.search(r"Rte_Read\s*\(\s*CVC_SIG_\w+", content), \
                f"{filename}: No Rte_Read(CVC_SIG_...) call found"

    def test_read_casts_from_uint32(self, cvc_wrappers):
        """Read wrappers for sub-32-bit types must cast from uint32_t."""
        for filename, content in cvc_wrappers.items():
            # For uint8_t/uint16_t signals, should see a cast
            reads_8 = re.findall(
                r"Rte_Read_\w+\s*\(\s*uint8_t\s*\*",
                content
            )
            if reads_8:
                assert "(uint8_t)" in content, \
                    f"{filename}: uint8_t read missing cast"


# ===================================================================
# Typed Write Wrappers
# ===================================================================

class TestRteWrapperWrites:
    """Verify Rte_Write_<Signal> inline functions."""

    def test_has_write_wrappers(self, cvc_wrappers):
        """At least one wrapper file must contain Rte_Write_ functions."""
        has_write = any(
            "Rte_Write_" in content
            for content in cvc_wrappers.values()
        )
        assert has_write, "No Rte_Write_ wrappers found in CVC"

    def test_write_is_static_inline(self, cvc_wrappers):
        """Write wrappers must be static inline functions."""
        for filename, content in cvc_wrappers.items():
            writes = re.findall(r"^.*Rte_Write_\w+.*$", content, re.MULTILINE)
            for line in writes:
                if line.strip().startswith("/*") or line.strip().startswith("//"):
                    continue
                if line.strip().startswith("#"):
                    continue
                assert "static" in line and "inline" in line, \
                    f"{filename}: Write wrapper not static inline: {line.strip()}"

    def test_write_returns_rte_status(self, cvc_wrappers):
        """Write wrappers must return Rte_StatusType."""
        for filename, content in cvc_wrappers.items():
            writes = re.findall(
                r"static\s+inline\s+(\w+)\s+Rte_Write_\w+",
                content
            )
            for ret_type in writes:
                assert ret_type == "Rte_StatusType", \
                    f"{filename}: Write returns {ret_type}, expected Rte_StatusType"

    def test_write_takes_typed_value(self, cvc_wrappers):
        """Write wrappers must take a valid C type value parameter."""
        valid_types = {"uint8_t", "uint16_t", "sint16_t",
                       "uint32_t", "sint32_t", "boolean"}
        for filename, content in cvc_wrappers.items():
            writes = re.findall(
                r"Rte_Write_\w+\s*\(\s*(\w+)\s+\w+\s*\)",
                content
            )
            for param_type in writes:
                assert param_type in valid_types, \
                    f"{filename}: Unknown type {param_type}"

    def test_write_calls_rte_write(self, cvc_wrappers):
        """Write wrappers must delegate to Rte_Write() with signal ID."""
        for filename, content in cvc_wrappers.items():
            if "Rte_Write_" not in content:
                continue
            assert re.search(r"Rte_Write\s*\(\s*CVC_SIG_\w+", content), \
                f"{filename}: No Rte_Write(CVC_SIG_...) call found"

    def test_write_casts_to_uint32(self, cvc_wrappers):
        """Write wrappers must cast value to uint32_t for Rte_Write()."""
        for filename, content in cvc_wrappers.items():
            if "Rte_Write_" not in content:
                continue
            # Should cast to Rte_SignalValueType or uint32_t
            assert re.search(r"\((?:uint32_t|Rte_SignalValueType)\)", content), \
                f"{filename}: Write missing uint32_t cast"


# ===================================================================
# Port-to-Signal Type Resolution
# ===================================================================

class TestRteWrapperTypes:
    """Verify that port data types are correctly resolved from signals."""

    def test_port_types_not_all_uint32(self, load_model):
        """After model load, ports must have resolved types, not all uint32_t."""
        model = load_model
        cvc = model.ecus["cvc"]
        all_port_types = set()
        for swc in cvc.swcs:
            for port in swc.ports:
                all_port_types.add(port.data_type)
        # Must have at least 2 distinct types (not everything uint32_t)
        assert len(all_port_types) >= 2, \
            f"All ports are {all_port_types} — types not resolved"

    def test_port_type_matches_signal_type(self, load_model):
        """Port data_type must match the corresponding signal's data_type."""
        model = load_model
        cvc = model.ecus["cvc"]
        # Build signal name → type lookup from all PDUs
        signal_types = {}
        for pdu in cvc.tx_pdus + cvc.rx_pdus:
            for sig in pdu.signals:
                signal_types[sig.name] = sig.data_type

        # Check that ports with matching signal names have the right type
        mismatches = []
        for swc in cvc.swcs:
            for port in swc.ports:
                if port.signal_name in signal_types:
                    expected = signal_types[port.signal_name]
                    if port.data_type != expected:
                        mismatches.append(
                            f"{swc.name}.{port.name}: "
                            f"port={port.data_type}, signal={expected}"
                        )
        assert not mismatches, \
            f"Port/signal type mismatches:\n" + "\n".join(mismatches)


# ===================================================================
# SWC Scoping — each SWC only sees its own signals
# ===================================================================

class TestRteWrapperScoping:
    """Verify that each SWC wrapper only contains its own port signals."""

    def test_swc_wrapper_count_matches_swc_count(self, cvc_wrappers, load_model):
        """Number of generated wrapper files must match number of SWCs."""
        model = load_model
        cvc = model.ecus["cvc"]
        swc_count = len(cvc.swcs)
        assert len(cvc_wrappers) == swc_count, \
            f"CVC has {swc_count} SWCs but {len(cvc_wrappers)} wrapper files"

    def test_each_swc_has_both_read_and_write(self, cvc_wrappers, load_model):
        """SWCs with both provided and required ports should have both Read and Write."""
        model = load_model
        cvc = model.ecus["cvc"]
        for swc in cvc.swcs:
            has_provided = any(p.direction == "PROVIDED" for p in swc.ports)
            has_required = any(p.direction == "REQUIRED" for p in swc.ports)
            filename = f"Rte_Swc_{swc.short_name or swc.name}.h"

            if filename not in cvc_wrappers:
                continue

            content = cvc_wrappers[filename]
            if has_provided:
                assert "Rte_Write_" in content, \
                    f"{filename}: SWC has PROVIDED ports but no Rte_Write_"
            if has_required:
                assert "Rte_Read_" in content, \
                    f"{filename}: SWC has REQUIRED ports but no Rte_Read_"

    def test_wrapper_signal_ids_use_ecu_prefix(self, cvc_wrappers):
        """All signal IDs in CVC wrappers must use CVC_ prefix."""
        for filename, content in cvc_wrappers.items():
            # Find all signal ID references
            sig_refs = re.findall(r"(?:Rte_Read|Rte_Write)\s*\(\s*(\w+_SIG_\w+)", content)
            for ref in sig_refs:
                assert ref.startswith("CVC_"), \
                    f"{filename}: Signal ref {ref} doesn't use CVC_ prefix"

    def test_bcm_wrapper_uses_bcm_prefix(self, bcm_wrappers):
        """BCM wrappers must use BCM_ signal IDs."""
        for filename, content in bcm_wrappers.items():
            sig_refs = re.findall(r"(?:Rte_Read|Rte_Write)\s*\(\s*(\w+_SIG_\w+)", content)
            for ref in sig_refs:
                assert ref.startswith("BCM_"), \
                    f"{filename}: Signal ref {ref} doesn't use BCM_ prefix"


# ===================================================================
# All ECUs — Completeness Tests
# ===================================================================

class TestRteWrapperAllEcus:
    """Verify all ECUs get typed wrapper headers."""

    def test_all_rte_ecus_have_wrappers(self, rte_wrapper_files):
        """All ECUs with rte enabled should have wrapper output."""
        # SC has no rte, so 6 ECUs
        expected = {"cvc", "fzc", "rzc", "bcm", "icu", "tcu"}
        assert set(rte_wrapper_files.keys()) >= expected

    def test_fzc_has_more_swcs_than_bcm(self, rte_wrapper_files):
        """FZC (10 SWCs) should have more wrappers than BCM (3 SWCs)."""
        fzc_count = len(rte_wrapper_files["fzc"])
        bcm_count = len(rte_wrapper_files["bcm"])
        assert fzc_count > bcm_count, \
            f"FZC={fzc_count} wrappers, BCM={bcm_count} — expected FZC > BCM"

    def test_no_ecu_has_zero_wrappers(self, rte_wrapper_files):
        """Every RTE-enabled ECU must have at least one wrapper."""
        for ecu_name, wrappers in rte_wrapper_files.items():
            assert len(wrappers) > 0, f"{ecu_name} has zero wrapper files"

    def test_total_wrapper_count(self, rte_wrapper_files, load_model):
        """Total wrappers across all ECUs should match total SWC count."""
        model = load_model
        total_wrappers = sum(len(w) for w in rte_wrapper_files.values())
        total_swcs = sum(
            len(ecu.swcs) for name, ecu in model.ecus.items()
            if name in rte_wrapper_files
        )
        assert total_wrappers == total_swcs, \
            f"Total wrappers={total_wrappers}, total SWCs={total_swcs}"
