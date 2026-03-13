"""
Tests for SWC skeleton generator — validates generated Swc_*.c and Swc_*.h
against the AUTOSAR SWC skeleton contract.

TDD: these tests define the contract BEFORE the generator is implemented.

SWC skeletons are generate-if-absent only — never overwrite existing source.
Each SWC gets a .c with runnable stubs and a .h with function declarations.
"""

import re

import pytest


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def swc_files(load_model):
    """
    Run the SWC skeleton generator for all ECUs and return a dict of
    { ecu_name: { filename: content } }.
    """
    from tools.arxmlgen.generators.swc_skeleton import SwcSkeletonGenerator
    from tools.arxmlgen.engine import TemplateEngine
    from tools.arxmlgen.config import load_config
    import os

    config_path = os.path.join(
        os.path.dirname(__file__), "..", "..", "..", "project.yaml"
    )
    config = load_config(config_path)
    engine = TemplateEngine(config)
    generator = SwcSkeletonGenerator()

    model = load_model
    results = {}
    for ecu_name, ecu in model.ecus.items():
        ecu_cfg = config.ecus.get(ecu_name)
        if ecu_cfg and ecu_cfg.include_in and "swc" not in ecu_cfg.include_in:
            continue

        output = generator.render(ecu, config.generators.get("swc"), engine)
        results[ecu_name] = output

    return results


@pytest.fixture
def cvc_swc_files(swc_files):
    """All SWC files for CVC ECU."""
    return swc_files["cvc"]


@pytest.fixture
def bcm_swc_files(swc_files):
    """All SWC files for BCM ECU."""
    return swc_files["bcm"]


# ===================================================================
# SWC Skeleton — File Naming Tests
# ===================================================================

class TestSwcSkeletonNaming:
    """Verify correct file naming for SWC skeletons."""

    def test_cvc_has_swc_files(self, cvc_swc_files):
        """CVC must have at least one SWC skeleton file."""
        assert len(cvc_swc_files) > 0

    def test_each_swc_has_c_and_h(self, cvc_swc_files, load_model):
        """Each SWC must have both a .c and .h file."""
        cvc = load_model.ecus["cvc"]
        for swc in cvc.swcs:
            c_name = f"{swc.name}.c"
            h_name = f"{swc.name}.h"
            assert c_name in cvc_swc_files, f"Missing {c_name}"
            assert h_name in cvc_swc_files, f"Missing {h_name}"

    def test_filenames_match_swc_names(self, cvc_swc_files, load_model):
        """File names must derive from SWC names."""
        cvc = load_model.ecus["cvc"]
        expected_names = set()
        for swc in cvc.swcs:
            expected_names.add(f"{swc.name}.c")
            expected_names.add(f"{swc.name}.h")
        assert expected_names == set(cvc_swc_files.keys())


# ===================================================================
# SWC Skeleton .h — Header Structure Tests
# ===================================================================

class TestSwcSkeletonHeader:
    """Verify the structural layout of generated SWC .h files."""

    def test_has_header_guard(self, cvc_swc_files, load_model):
        """Each .h file must have a header guard."""
        for fname, content in cvc_swc_files.items():
            if fname.endswith(".h"):
                guard = fname.replace(".", "_").upper()
                assert f"#ifndef {guard}" in content
                assert f"#define {guard}" in content

    def test_has_function_declarations(self, cvc_swc_files, load_model):
        """Each .h file must declare the SWC's runnable functions."""
        cvc = load_model.ecus["cvc"]
        for swc in cvc.swcs:
            h_name = f"{swc.name}.h"
            content = cvc_swc_files[h_name]
            for runnable in swc.runnables:
                assert f"void {runnable.name}(void)" in content, \
                    f"Missing declaration for {runnable.name} in {h_name}"

    def test_has_generated_banner(self, cvc_swc_files):
        """Each .h must note it was generated but is editable."""
        for fname, content in cvc_swc_files.items():
            if fname.endswith(".h"):
                assert "arxmlgen" in content.lower() or "generated" in content.upper()


# ===================================================================
# SWC Skeleton .c — Source Structure Tests
# ===================================================================

class TestSwcSkeletonSource:
    """Verify the structural layout of generated SWC .c files."""

    def test_includes_own_header(self, cvc_swc_files, load_model):
        """Each .c file must include its own .h header."""
        cvc = load_model.ecus["cvc"]
        for swc in cvc.swcs:
            c_name = f"{swc.name}.c"
            h_name = f"{swc.name}.h"
            content = cvc_swc_files[c_name]
            assert f'#include "{h_name}"' in content

    def test_includes_rte_wrapper(self, cvc_swc_files, load_model):
        """Each .c file must include its typed RTE wrapper header."""
        cvc = load_model.ecus["cvc"]
        for swc in cvc.swcs:
            c_name = f"{swc.name}.c"
            content = cvc_swc_files[c_name]
            assert f'#include "Rte_{swc.name}.h"' in content

    def test_has_runnable_function_bodies(self, cvc_swc_files, load_model):
        """Each .c must have function bodies for all runnables."""
        cvc = load_model.ecus["cvc"]
        for swc in cvc.swcs:
            c_name = f"{swc.name}.c"
            content = cvc_swc_files[c_name]
            for runnable in swc.runnables:
                assert f"void {runnable.name}(void)" in content, \
                    f"Missing function body for {runnable.name}"

    def test_init_runnable_has_init_comment(self, cvc_swc_files, load_model):
        """Init runnables should be marked as initialization functions."""
        cvc = load_model.ecus["cvc"]
        for swc in cvc.swcs:
            c_name = f"{swc.name}.c"
            content = cvc_swc_files[c_name]
            for runnable in swc.runnables:
                if runnable.is_init:
                    # Init runnables should have some indication
                    idx = content.find(f"void {runnable.name}(void)")
                    assert idx >= 0
                    # Check surrounding context mentions "init"
                    context = content[max(0, idx - 100):idx + 50].lower()
                    assert "init" in context, \
                        f"{runnable.name} should be marked as init runnable"

    def test_periodic_runnable_has_period_comment(self, cvc_swc_files, load_model):
        """Periodic runnables should document their period."""
        cvc = load_model.ecus["cvc"]
        for swc in cvc.swcs:
            c_name = f"{swc.name}.c"
            content = cvc_swc_files[c_name]
            for runnable in swc.runnables:
                if not runnable.is_init and runnable.period_ms > 0:
                    idx = content.find(f"void {runnable.name}(void)")
                    assert idx >= 0
                    context = content[max(0, idx - 150):idx + 50]
                    assert str(runnable.period_ms) in context, \
                        f"{runnable.name} should document period {runnable.period_ms}ms"


# ===================================================================
# SWC Skeleton — Cross-ECU Tests
# ===================================================================

class TestSwcSkeletonAllEcus:
    """Verify all ECUs with swc enabled get skeletons."""

    def test_swc_ecus_have_output(self, swc_files):
        """ECUs with swc enabled should have skeleton output."""
        expected = {"cvc", "fzc", "rzc", "bcm", "icu", "tcu"}
        assert set(swc_files.keys()) >= expected

    def test_sc_excluded(self, swc_files):
        """SC has include_in: [canif] — no SWC skeletons."""
        assert "sc" not in swc_files

    def test_total_file_count(self, swc_files, load_model):
        """Total file count = 2 * total SWCs across all enabled ECUs."""
        total_files = sum(len(files) for files in swc_files.values())
        total_swcs = sum(
            len(load_model.ecus[ecu].swcs)
            for ecu in swc_files.keys()
        )
        assert total_files == total_swcs * 2

    def test_bcm_has_fewer_swcs_than_fzc(self, swc_files, load_model):
        """BCM should have fewer SWC skeletons than FZC."""
        bcm_count = len(load_model.ecus["bcm"].swcs)
        fzc_count = len(load_model.ecus["fzc"].swcs)
        assert fzc_count > bcm_count
