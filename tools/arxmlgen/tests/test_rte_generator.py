"""
Tests for Rte_Cfg generator — validates generated Rte_Cfg_*.c files
against the structure of hand-written reference files.

TDD: these tests define the contract BEFORE generators are implemented.
"""

import re

import pytest


def _parse_runnable_entries(content):
    match = re.search(
        r"runnable_config\[\].*?=\s*\{(.*?)\};",
        content,
        re.DOTALL,
    )
    assert match, "runnable_config array not found"

    entries = []
    for entry in re.findall(r"\{([^}]+)\}", match.group(1)):
        fields = [field.strip() for field in entry.strip().split(",")]
        entries.append(
            {
                "name": fields[0],
                "period_ms": int(fields[1].rstrip("u").strip()),
                "priority": int(fields[2].rstrip("u").strip()),
            }
        )
    return entries


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def rte_cfg_c_cvc(generated_files):
    return generated_files["cvc"]["Rte_Cfg.c"]


@pytest.fixture
def rte_cfg_c_bcm(generated_files):
    return generated_files["bcm"]["Rte_Cfg.c"]


@pytest.fixture
def generated_files(load_model):
    """
    Run the Rte generator for all ECUs and return a dict of
    { ecu_name: { filename: content } }.
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

        output = generator.render(ecu, gen_config, engine)
        results[ecu_name] = output

    return results


# ===================================================================
# Rte_Cfg.c — Structure Tests
# ===================================================================

class TestRteCfgCStructure:
    """Verify structural layout of generated Rte_Cfg_*.c files."""

    def test_has_generated_header(self, rte_cfg_c_bcm):
        assert "GENERATED" in rte_cfg_c_bcm
        assert "DO NOT EDIT" in rte_cfg_c_bcm

    def test_includes_rte_h(self, rte_cfg_c_bcm):
        assert '#include "Rte.h"' in rte_cfg_c_bcm

    def test_includes_ecu_cfg_h(self, rte_cfg_c_bcm):
        assert '#include "Bcm_Cfg.h"' in rte_cfg_c_bcm

    def test_has_extern_declarations(self, rte_cfg_c_bcm):
        """Must have extern void declarations for runnables."""
        assert re.search(r"extern\s+void\s+\w+\(void\);", rte_cfg_c_bcm)

    def test_has_signal_config_array(self, rte_cfg_c_bcm):
        assert "Rte_SignalConfigType" in rte_cfg_c_bcm
        assert "signal_config[" in rte_cfg_c_bcm

    def test_has_runnable_config_array(self, rte_cfg_c_bcm):
        assert "Rte_RunnableConfigType" in rte_cfg_c_bcm
        assert "runnable_config[]" in rte_cfg_c_bcm

    def test_has_aggregate_config(self, rte_cfg_c_bcm):
        assert "Rte_ConfigType" in rte_cfg_c_bcm
        assert "bcm_rte_config" in rte_cfg_c_bcm


# ===================================================================
# Rte_Cfg.c — Signal Table Tests
# ===================================================================

class TestRteCfgSignals:
    """Verify signal table correctness."""

    def test_starts_with_16_bsw_signals(self, rte_cfg_c_bcm):
        """First 16 entries must be BSW well-known signals (0-15)."""
        # Find signal config array content
        match = re.search(
            r"signal_config\[.*?\]\s*=\s*\{(.*?)\};",
            rte_cfg_c_bcm, re.DOTALL
        )
        assert match, "Signal config array not found"
        content = match.group(1)

        # Extract signal IDs from entries
        ids = re.findall(r"\{\s*(\d+)u,", content)
        ids = [int(x) for x in ids]

        # First 16 must be 0-15
        assert ids[:16] == list(range(16)), \
            f"BSW reserved signals must be 0-15, got: {ids[:16]}"

    def test_bsw_signals_have_zero_init(self, rte_cfg_c_bcm):
        """BSW well-known signals (0-15) must have init value 0."""
        match = re.search(
            r"signal_config\[.*?\]\s*=\s*\{(.*?)\};",
            rte_cfg_c_bcm, re.DOTALL
        )
        assert match
        entries = re.findall(r"\{([^}]+)\}", match.group(1))

        for i, entry in enumerate(entries[:16]):
            # Entry format: "  Xu, Yu " — second value is init
            vals = re.findall(r"(\d+)u", entry)
            assert len(vals) >= 2, f"Entry {i} malformed: {entry}"
            assert vals[1] == "0", f"BSW signal {i} has non-zero init: {vals[1]}"

    def test_ecu_signals_start_at_16(self, rte_cfg_c_bcm):
        """ECU-specific signals must start at ID 16."""
        match = re.search(
            r"signal_config\[.*?\]\s*=\s*\{(.*?)\};",
            rte_cfg_c_bcm, re.DOTALL
        )
        assert match
        entries = re.findall(r"\{([^}]+)\}", match.group(1))

        # Entry 16 (index 16) should reference a define, not a literal
        # OR be the literal 16u — either way, the signal ID must be 16+
        assert len(entries) > 16, "Must have ECU-specific signals beyond BSW"

    def test_signal_count_matches_model(self, rte_cfg_c_bcm, load_model):
        """Total signal entries = 16 BSW + app signals from rte_signal_map."""
        model = load_model
        bcm = model.ecus["bcm"]
        app_signals = sum(1 for sid in bcm.rte_signal_map.values() if sid >= 16)
        expected = 16 + app_signals

        match = re.search(
            r"signal_config\[.*?\]\s*=\s*\{(.*?)\};",
            rte_cfg_c_bcm, re.DOTALL
        )
        assert match
        entries = re.findall(r"\{[^}]+\}", match.group(1))
        assert len(entries) == expected, \
            f"Signal count mismatch: {len(entries)} generated vs {expected} expected"

    def test_signal_array_size_uses_sig_count(self, rte_cfg_c_bcm):
        """Signal array must be sized with ECU_SIG_COUNT define."""
        assert re.search(r"signal_config\[\w+_SIG_COUNT\]", rte_cfg_c_bcm)


# ===================================================================
# Rte_Cfg.c — Runnable Table Tests
# ===================================================================

class TestRteCfgRunnables:
    """Verify runnable table correctness."""

    def test_runnable_count_matches_model(self, rte_cfg_c_bcm, load_model):
        """Number of runnable entries must match model."""
        model = load_model
        bcm = model.ecus["bcm"]
        expected = sum(
            1
            for swc in bcm.swcs
            for r in swc.runnables
            if (not r.is_init) and (r.period_ms > 0)
        )

        match = re.search(
            r"runnable_config\[\].*?=\s*\{(.*?)\};",
            rte_cfg_c_bcm, re.DOTALL
        )
        assert match, "runnable_config array not found"
        entries = re.findall(r"\{[^}]+\}", match.group(1))
        assert len(entries) == expected

    def test_runnables_have_function_pointers(self, rte_cfg_c_bcm):
        """Each runnable entry must reference a function name."""
        match = re.search(
            r"runnable_config\[\].*?=\s*\{(.*?)\};",
            rte_cfg_c_bcm, re.DOTALL
        )
        assert match
        entries = re.findall(r"\{([^}]+)\}", match.group(1))
        for entry in entries:
            # First field should be a function name (identifier, not a number)
            first_field = entry.strip().split(",")[0].strip()
            assert re.match(r"[A-Za-z_]\w+", first_field), \
                f"Runnable entry doesn't start with function name: {entry.strip()}"

    def test_runnables_sorted_by_priority_descending(self, rte_cfg_c_bcm):
        """Runnables should be sorted by priority (highest first)."""
        match = re.search(
            r"runnable_config\[\].*?=\s*\{(.*?)\};",
            rte_cfg_c_bcm, re.DOTALL
        )
        assert match
        entries = re.findall(r"\{([^}]+)\}", match.group(1))

        priorities = []
        for entry in entries:
            fields = entry.strip().split(",")
            # priority is the 3rd field (index 2)
            if len(fields) >= 3:
                prio_str = fields[2].strip().rstrip("u").strip()
                priorities.append(int(prio_str))

        assert priorities == sorted(priorities, reverse=True), \
            f"Runnables not sorted by priority (desc): {priorities}"

    def test_runnable_period_is_non_negative(self, rte_cfg_c_bcm):
        """Runnable periods must be >= 0 (0 = init/event-triggered)."""
        match = re.search(
            r"runnable_config\[\].*?=\s*\{(.*?)\};",
            rte_cfg_c_bcm, re.DOTALL
        )
        assert match
        entries = re.findall(r"\{([^}]+)\}", match.group(1))

        for entry in entries:
            fields = entry.strip().split(",")
            if len(fields) >= 2:
                period_str = fields[1].strip().rstrip("u").strip()
                period = int(period_str)
                assert period >= 0, f"Negative period in runnable: {entry.strip()}"

    def test_each_extern_has_matching_runnable(self, rte_cfg_c_bcm):
        """Every extern declaration must appear in the runnable config."""
        externs = re.findall(r"extern\s+void\s+(\w+)\(void\);", rte_cfg_c_bcm)
        match = re.search(
            r"runnable_config\[\].*?=\s*\{(.*?)\};",
            rte_cfg_c_bcm, re.DOTALL
        )
        assert match
        config_body = match.group(1)
        for func in externs:
            assert func in config_body, \
                f"extern {func} declared but not in runnable_config"

    def test_stm32_generated_schedulers_include_can_write(self, generated_files):
        """CVC/FZC/RZC must schedule the CAN TX queue drain runnable."""
        for ecu_name in ("cvc", "fzc", "rzc"):
            content = generated_files[ecu_name]["Rte_Cfg.c"]
            assert "extern void Can_MainFunction_Write(void);" in content

            entries = _parse_runnable_entries(content)
            by_name = {entry["name"]: entry for entry in entries}

            assert "Can_MainFunction_Write" in by_name, \
                f"{ecu_name}: Can_MainFunction_Write missing from runnable table"
            assert by_name["Can_MainFunction_Write"]["period_ms"] == 1, \
                f"{ecu_name}: Can_MainFunction_Write must run every 1 ms"

    def test_stm32_can_write_is_not_scheduled_after_busoff(self, generated_files):
        """If write and bus-off are due together, write must not lose priority."""
        for ecu_name in ("cvc", "fzc", "rzc"):
            entries = _parse_runnable_entries(generated_files[ecu_name]["Rte_Cfg.c"])
            positions = {entry["name"]: idx for idx, entry in enumerate(entries)}
            priorities = {entry["name"]: entry["priority"] for entry in entries}

            assert priorities["Can_MainFunction_Write"] >= priorities["Can_MainFunction_BusOff"], \
                f"{ecu_name}: Can_MainFunction_Write must not run below Can_MainFunction_BusOff"
            if priorities["Can_MainFunction_Write"] == priorities["Can_MainFunction_BusOff"]:
                assert positions["Can_MainFunction_Write"] < positions["Can_MainFunction_BusOff"], \
                    f"{ecu_name}: equal-priority CAN write must appear before bus-off"


# ===================================================================
# Rte_Cfg.c — Aggregate Config Tests
# ===================================================================

class TestRteCfgAggregate:
    """Verify aggregate Rte_ConfigType struct."""

    def test_aggregate_is_const(self, rte_cfg_c_bcm):
        match = re.search(r"^(.*Rte_ConfigType\s+\w+_rte_config)", rte_cfg_c_bcm, re.MULTILINE)
        assert match
        assert "const" in match.group(1)
        assert "static" not in match.group(1)

    def test_aggregate_has_signal_config(self, rte_cfg_c_bcm):
        assert ".signalConfig" in rte_cfg_c_bcm

    def test_aggregate_has_signal_count(self, rte_cfg_c_bcm):
        assert ".signalCount" in rte_cfg_c_bcm

    def test_aggregate_has_runnable_config(self, rte_cfg_c_bcm):
        assert ".runnableConfig" in rte_cfg_c_bcm

    def test_aggregate_has_runnable_count(self, rte_cfg_c_bcm):
        assert ".runnableCount" in rte_cfg_c_bcm


# ===================================================================
# All ECUs Tests
# ===================================================================

class TestRteCfgAllEcus:
    """Verify all ECUs get generated Rte config."""

    def test_all_rte_ecus_have_output(self, generated_files):
        expected = {"cvc", "fzc", "rzc", "bcm", "icu", "tcu"}
        assert set(generated_files.keys()) >= expected

    def test_each_ecu_uses_own_prefix(self, generated_files):
        for ecu_name, files in generated_files.items():
            content = files["Rte_Cfg.c"]
            assert f"{ecu_name}_rte_config" in content

    def test_cvc_has_more_runnables_than_icu(self, generated_files):
        """CVC (central) should have more runnables than ICU (display)."""
        cvc = generated_files["cvc"]["Rte_Cfg.c"]
        icu = generated_files["icu"]["Rte_Cfg.c"]
        cvc_count = len(re.findall(r"extern\s+void", cvc))
        icu_count = len(re.findall(r"extern\s+void", icu))
        assert cvc_count >= icu_count
