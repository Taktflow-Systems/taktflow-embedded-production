"""
Tests for the Os_Cfg generator (S-OS-11) — OSEK task/schedule-table config
and RTE task bodies generated from the model.

TDD: these tests define the contract BEFORE the generator is implemented.

The load-bearing property is ORDER EQUIVALENCE: the generated task bodies
plus schedule table must reproduce, tick by tick, exactly the runnable
sequence that the legacy Rte_DispatchRunnables (firmware/bsw/rte/src/
Rte.c:40-105) would produce, including the once-per-unique-seId
WdgM_CheckpointReached behavior. Where the one-task-per-period-group
design cannot reproduce the legacy order (cross-period priority
interleaving), the generator must REFUSE to emit output for that ECU
(fail-closed) — it must never silently change execution order.
"""

import os
import re
import shutil
import subprocess

import pytest

from tools.arxmlgen.generators.os_cfg import (
    OsCfgGenerator,
    OsRunnable,
    collect_legacy_runnable_table,
    derive_period_groups,
    simulate_legacy_dispatch,
    simulate_osek_dispatch,
    verify_order_equivalence,
)
from tools.arxmlgen.config import load_config
from tools.arxmlgen.engine import TemplateEngine


TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
GOLDEN_OS_DIR = os.path.join(TESTS_DIR, "golden", "os_reference")
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(TESTS_DIR)))
RTE_MAX_RUNNABLES = 16


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def config():
    config_path = os.path.join(PROJECT_ROOT, "project.yaml")
    if not os.path.exists(config_path):
        pytest.skip("project.yaml not found")
    return load_config(config_path)


@pytest.fixture
def engine(config):
    return TemplateEngine(config)


@pytest.fixture
def generator():
    return OsCfgGenerator()


def _render(generator, config, engine, model, ecu_name):
    """Render Os outputs for one ECU; returns {filename: content}."""
    ecu = model.ecus[ecu_name]
    gen_config = config.generators.get("os")
    return generator.render(ecu, gen_config, engine)


# ---------------------------------------------------------------------------
# Synthetic runnable tables (independent of the ARXML model, so the
# equivalence machinery is proven even when the model cannot load)
# ---------------------------------------------------------------------------

def _r(name, period, prio, se=0xFF):
    return OsRunnable(name=name, period=period, priority=prio, se_id=se)


# A table where period groups do NOT interleave in priority order:
# 1ms band {9,8} > 10ms band {5,4} > 100ms band {2}.
BANDED_TABLE = [
    _r("Fast_A", 1, 9, 0),
    _r("Fast_B", 1, 8),
    _r("Mid_A", 10, 5, 1),
    _r("Mid_B", 10, 4, 1),   # same SE as Mid_A: once-per-tick checkpoint
    _r("Slow_A", 100, 2, 2),
]

# A table with cross-period interleaving: a 10ms runnable must run
# BETWEEN two 1ms runnables in the same tick (the plan's counterexample).
INTERLEAVED_TABLE = [
    _r("Fast_A", 1, 9),
    _r("Mid_A", 10, 8),
    _r("Fast_B", 1, 7),
]

# BCM-shaped table: equal priorities, the 100ms runnable FIRST in table
# order — legacy runs it before the 10ms group at tick 100.
TIE_ORDER_TABLE = [
    _r("DoorLock_100ms", 100, 5),
    _r("Indicators_10ms", 10, 5),
    _r("Lights_10ms", 10, 5),
]


# ===================================================================
# Legacy dispatch simulation — must replicate Rte.c:40-105 exactly
# ===================================================================

class TestLegacySimulation:
    def test_fires_at_multiples_of_period(self):
        seq = simulate_legacy_dispatch(BANDED_TABLE, ticks=10)
        runs = [e for e in seq if e[0] == "run"]
        # Fast_A/Fast_B fire every tick; Mid_* only at tick 10.
        assert ("run", 10, "Mid_A") in seq
        assert not any(e[2].startswith("Mid") and e[1] < 10 for e in runs)

    def test_priority_order_within_tick(self):
        seq = simulate_legacy_dispatch(BANDED_TABLE, ticks=10)
        tick10 = [e[2] for e in seq if e[0] == "run" and e[1] == 10]
        assert tick10 == ["Fast_A", "Fast_B", "Mid_A", "Mid_B"]

    def test_tie_break_by_table_index(self):
        seq = simulate_legacy_dispatch(TIE_ORDER_TABLE, ticks=100)
        tick100 = [e[2] for e in seq if e[0] == "run" and e[1] == 100]
        # All prio 5 — legacy selection keeps table order, so the 100ms
        # DoorLock (index 0) runs BEFORE the 10ms runnables at tick 100.
        assert tick100 == ["DoorLock_100ms", "Indicators_10ms", "Lights_10ms"]

    def test_wdgm_checkpoint_once_per_unique_se_per_tick(self):
        seq = simulate_legacy_dispatch(BANDED_TABLE, ticks=10)
        tick10_wdgm = [e[2] for e in seq if e[0] == "wdgm" and e[1] == 10]
        # Mid_A and Mid_B share SE 1 — exactly one checkpoint for SE 1.
        assert tick10_wdgm.count(1) == 1
        # SE 0 (Fast_A) also raised at tick 10.
        assert 0 in tick10_wdgm

    def test_wdgm_se_out_of_range_not_raised(self):
        seq = simulate_legacy_dispatch(BANDED_TABLE, ticks=1)
        wdgm = [e for e in seq if e[0] == "wdgm"]
        assert all(e[2] < RTE_MAX_RUNNABLES for e in wdgm)
        # Fast_B has seId 0xFF — never checkpointed.
        assert all(e[2] != 0xFF for e in wdgm)


# ===================================================================
# OSEK dispatch simulation + equivalence proof
# ===================================================================

class TestOrderEquivalence:
    def test_banded_table_is_equivalent(self):
        cex = verify_order_equivalence(BANDED_TABLE)
        assert cex is None, f"banded table must be reproducible: {cex}"

    def test_banded_osek_sequence_matches_legacy_exactly(self):
        groups = derive_period_groups(BANDED_TABLE, [])
        window = 200
        legacy = simulate_legacy_dispatch(BANDED_TABLE, ticks=window)
        osek = simulate_osek_dispatch(BANDED_TABLE, groups, ticks=window)
        assert osek == legacy

    def test_interleaved_table_is_refused(self):
        cex = verify_order_equivalence(INTERLEAVED_TABLE)
        assert cex is not None
        # The counterexample must name the first diverging tick (10: the
        # legacy order is Fast_A, Mid_A, Fast_B — not reproducible by
        # contiguous per-period task blocks).
        assert cex.tick == 10
        assert "Mid_A" in str(cex)

    def test_tie_order_table_is_refused(self):
        """BCM-shaped: with shorter-period -> higher-priority tasks, the
        100ms group cannot run before the 10ms group at tick 100."""
        cex = verify_order_equivalence(TIE_ORDER_TABLE)
        assert cex is not None
        assert cex.tick == 100
        legacy_t100 = [e[2] for e in cex.legacy if e[1] == 100 and e[0] == "run"]
        assert legacy_t100[0] == "DoorLock_100ms"

    def test_lcm_window_covers_all_periods(self):
        # verify_order_equivalence must simulate at least LCM(1,10,100)
        # ticks — the tie-order divergence only appears at tick 100.
        cex = verify_order_equivalence(TIE_ORDER_TABLE)
        assert cex is not None and cex.tick == 100


# ===================================================================
# Period-group derivation
# ===================================================================

class TestPeriodGroups:
    def test_union_of_runnable_and_bsw_slot_periods(self):
        groups = derive_period_groups(BANDED_TABLE, [10, 100, 5000])
        assert [g.period for g in groups] == [1, 10, 100, 5000]

    def test_priority_shorter_period_higher(self):
        """Kernel convention: numeric priority 0 is highest
        (Os_Scheduler.c os_select_next_ready_task scans 0 upward)."""
        groups = derive_period_groups(BANDED_TABLE, [5000])
        prios = {g.period: g.task_priority for g in groups}
        assert prios[1] < prios[10] < prios[100] < prios[5000]
        assert prios[1] == 0

    def test_group_members_keep_legacy_table_order(self):
        groups = derive_period_groups(BANDED_TABLE, [])
        mid = next(g for g in groups if g.period == 10)
        assert [r.name for r in mid.runnables] == ["Mid_A", "Mid_B"]

    def test_bsw_only_group_has_no_runnables(self):
        groups = derive_period_groups(BANDED_TABLE, [5000])
        slow = next(g for g in groups if g.period == 5000)
        assert slow.runnables == []


# ===================================================================
# Real-model tests (skip when the ARXML model cannot load)
# ===================================================================

ALL_ECUS = ("bcm", "cvc", "fzc", "rzc", "icu", "tcu")

#: Per-ECU BSW slot periods from model/ecu_sidecar.yaml `os:` sections —
#: mirrored here so the committed-table proofs use the same period groups
#: as generation (cross-checked against the model by
#: test_sidecar_os_sections_loaded below).
SIDECAR_BSW_SLOTS = {
    "cvc": [10, 100, 5000], "fzc": [10, 100, 5000], "rzc": [10, 100, 5000],
    "bcm": [1, 10], "icu": [1, 10], "tcu": [1, 10],
}


class TestRealModel:
    """After the rate-monotonic re-band (memo-rm-reband-trace.md, user
    decision 2026-07-07) the committed Rte_Cfg tables are period-banded,
    so the one-task-per-period-group design reproduces the legacy order
    for ALL SIX ECUs — the equivalence gate must pass everywhere."""

    def test_bcm_committed_table_is_banded(self):
        """BCM tick-100 tie-order counterexample resolved by the re-band:
        the 100ms runnable now sits below the 10ms band
        (memo-rm-reband-trace.md, BCM table)."""
        table = _parse_rte_cfg_table("bcm")
        assert [r.name for r in table] == [
            "Swc_Indicators_10ms", "Swc_Lights_10ms", "Swc_DoorLock_100ms",
        ]

    @pytest.mark.parametrize("ecu_name", ALL_ECUS)
    def test_equivalence_proof_from_committed_rte_cfg(self, ecu_name):
        """The plan's table-equivalence proof, driven from the committed
        generated Rte_Cfg_<Ecu>.c for every RTE ECU: simulate
        Rte_DispatchRunnables tick-by-tick over the LCM window and compare
        with the task-body + schedule-table execution."""
        table = _parse_rte_cfg_table(ecu_name)
        cex = verify_order_equivalence(table, SIDECAR_BSW_SLOTS[ecu_name])
        assert cex is None, (
            f"{ecu_name} diverged — the committed Rte_Cfg table is no "
            f"longer rate-monotonically banded: {cex}"
        )

    def test_tcu_equivalence_holds_trivially(self):
        table = _parse_rte_cfg_table("tcu")
        assert table == []
        assert verify_order_equivalence(table) is None

    @pytest.mark.parametrize("ecu_name", ALL_ECUS)
    def test_model_table_matches_committed_rte_cfg(self, ecu_name, load_model):
        """The generator's collected (banded) table must equal the
        committed Rte_Cfg_<Ecu>.c runnable table (same collection
        semantics as the rte generator)."""
        model = load_model
        collected = collect_legacy_runnable_table(model.ecus[ecu_name])
        committed = _parse_rte_cfg_table(ecu_name)
        assert [(r.name, r.period, r.priority, r.se_id) for r in collected] == \
               [(r.name, r.period, r.priority, r.se_id) for r in committed]


def _parse_rte_cfg_table(ecu_name):
    """Parse the committed generated Rte_Cfg_<Ecu>.c runnable table."""
    path = os.path.join(
        PROJECT_ROOT, "firmware", "ecu", ecu_name, "cfg",
        f"Rte_Cfg_{ecu_name.capitalize()}.c",
    )
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    match = re.search(r"runnable_config\[\]\s*=\s*\{(.*?)\};", content, re.DOTALL)
    assert match, f"runnable table not found in {path}"
    table = []
    for entry in re.finditer(
        r"\{\s*(\w+)\s*,\s*(\d+)u\s*,\s*(\d+)u\s*,\s*0x([0-9A-Fa-f]+)u\s*\}",
        match.group(1),
    ):
        table.append(OsRunnable(
            name=entry.group(1),
            period=int(entry.group(2)),
            priority=int(entry.group(3)),
            se_id=int(entry.group(4), 16),
        ))
    return table


# ===================================================================
# Rendered output tests (real model, ICU = provable ECU)
# ===================================================================

@pytest.fixture
def icu_files(generator, config, engine, load_model):
    return _render(generator, config, engine, load_model, "icu")


@pytest.fixture
def tcu_files(generator, config, engine, load_model):
    return _render(generator, config, engine, load_model, "tcu")


class TestRenderedOutput:
    def test_icu_produces_three_files(self, icu_files):
        assert set(icu_files.keys()) == {
            "Os_Cfg_Icu.c", "Os_Cfg_Icu.h", "Rte_TaskBodies_Icu.c",
        }

    def test_generated_banner_present(self, icu_files):
        for name, content in icu_files.items():
            assert "GENERATED" in content, name
            assert "DO NOT EDIT" in content, name

    def test_aggregate_config_name(self, icu_files):
        assert re.search(
            r"const\s+Os_ConfigType\s+icu_os_config\s*=", icu_files["Os_Cfg_Icu.c"]
        )

    def test_task_table_one_task_per_group_plus_idle(self, icu_files):
        c = icu_files["Os_Cfg_Icu.c"]
        # ICU groups: 1 (BSW slot), 10 (BSW heartbeat slot), 50 (runnables)
        for sym in ("Os_Task_Icu_1ms", "Os_Task_Icu_10ms",
                    "Os_Task_Icu_50ms", "Os_Task_Icu_Idle"):
            assert sym in c, sym

    def test_idle_task_convention(self, icu_files):
        """Idle task: priority OS_MAX_PRIORITIES-1, autostarted, FULL
        (kernel os_cfg_validate_idle_task, FMEA OS-C-03)."""
        c = icu_files["Os_Cfg_Icu.c"]
        m = re.search(r"\{[^{}]*Os_Task_Icu_Idle[^{}]*\}", c)
        assert m, "idle task entry not found"
        entry = m.group(0)
        assert "OS_MAX_PRIORITIES - 1u" in entry.replace("(", " ").replace(")", " ") \
            or re.search(r"7u", entry)
        assert "FULL" in entry

    def test_task_priorities_shorter_period_first(self, icu_files):
        h = icu_files["Os_Cfg_Icu.h"]
        # Priority defines: 1ms task prio 0, 10ms prio 1, 50ms prio 2
        assert re.search(r"OS_TASK_PRIO_ICU_1MS\s+\(?\(uint8\)0u", h)
        assert re.search(r"OS_TASK_PRIO_ICU_10MS\s+\(?\(uint8\)1u", h)
        assert re.search(r"OS_TASK_PRIO_ICU_50MS\s+\(?\(uint8\)2u", h)

    def test_schedule_table_per_period_group(self, icu_files):
        c = icu_files["Os_Cfg_Icu.c"]
        assert "Os_ScheduleTableConfigType" in c
        # One repeating table per group, Duration == period, one expiry
        # point at offset == period (phase-preserving, see template header)
        assert c.count("Os_ExpiryPointConfigType") >= 3
        assert re.search(r"\.Duration\s*=\s*50u", c) or re.search(r"50u,\s*/\*\s*Duration", c) or "50u" in c

    def test_stack_config_entries(self, icu_files):
        c = icu_files["Os_Cfg_Icu.c"]
        assert "Os_StackMonitorConfigType" in c
        # Default stack budget from sidecar os section (default 1024)
        assert "1024u" in c

    def test_counter_config_defines(self, icu_files):
        h = icu_files["Os_Cfg_Icu.h"]
        assert "ICU_OS_COUNTER_MAXALLOWEDVALUE" in h
        assert "ICU_OS_COUNTER_TICKSPERBASE" in h
        assert "ICU_OS_COUNTER_MINCYCLE" in h

    def test_wdgm_note_present(self, icu_files):
        assert "WdgM" in icu_files["Os_Cfg_Icu.c"]

    def test_task_bodies_call_runnables_in_legacy_order(self, icu_files):
        body = icu_files["Rte_TaskBodies_Icu.c"]
        m = re.search(
            r"void\s+Os_Task_Icu_50ms\(void\)\s*\{(.*?)\n\}", body, re.DOTALL
        )
        assert m, "50ms task body not found"
        calls = re.findall(r"(Swc_\w+)\(\);", m.group(1))
        assert calls == ["Swc_Dashboard_50ms", "Swc_DtcDisplay_50ms"]

    def test_task_bodies_terminate(self, icu_files):
        body = icu_files["Rte_TaskBodies_Icu.c"]
        for task in ("Os_Task_Icu_1ms", "Os_Task_Icu_10ms", "Os_Task_Icu_50ms"):
            m = re.search(
                r"void\s+" + task + r"\(void\)\s*\{(.*?)\n\}", body, re.DOTALL
            )
            assert m, task
            assert "TerminateTask()" in m.group(1), task

    def test_tcu_has_no_runnable_calls(self, tcu_files):
        body = tcu_files["Rte_TaskBodies_Tcu.c"]
        assert "Swc_" not in body

    def test_all_six_ecus_render(self, generator, config, engine, load_model):
        """Post re-band the equivalence gate passes for every RTE ECU —
        each renders exactly the three Os outputs (memo-rm-reband-trace.md,
        gate re-check section 3)."""
        for ecu_name in ("bcm", "cvc", "fzc", "rzc", "icu", "tcu"):
            files = _render(generator, config, engine, load_model, ecu_name)
            pascal = ecu_name.capitalize()
            assert set(files.keys()) == {
                f"Os_Cfg_{pascal}.c", f"Os_Cfg_{pascal}.h",
                f"Rte_TaskBodies_{pascal}.c",
            }, f"{ecu_name} refused or incomplete: {list(files)}"
        assert generator.refusals == {}

    def test_gate_still_refuses_synthetic_conflicting_model(
            self, generator, config, engine):
        """The fail-closed refusal path stays alive: a synthetic ECU whose
        table interleaves a 10ms runnable between 1ms runnables (i.e. NOT
        banded — the reader policy was bypassed) must be refused with no
        output."""
        from tools.arxmlgen.model import Ecu, Runnable, Swc
        ecu = Ecu(name="synth", prefix="SYN", swcs=[Swc(
            name="Swc_Synth",
            runnables=[
                Runnable("Fast_A", period_ms=1, priority=9),
                Runnable("Mid_A", period_ms=10, priority=8),
                Runnable("Fast_B", period_ms=1, priority=7),
            ],
        )])
        files = generator.render(ecu, config.generators.get("os"), engine)
        assert files == {}
        reason = generator.refusals.get("synth")
        assert reason is not None
        assert "tick 10" in str(reason)


# ===================================================================
# Idempotency + golden fixture
# ===================================================================

class TestIdempotency:
    def test_two_renders_byte_identical(self, generator, config, engine, load_model):
        a = _render(generator, config, engine, load_model, "icu")
        b = _render(OsCfgGenerator(), config, engine, load_model, "icu")
        assert a == b


class TestGolden:
    @pytest.mark.parametrize("filename", [
        "Os_Cfg_Icu.c", "Os_Cfg_Icu.h", "Rte_TaskBodies_Icu.c",
    ])
    def test_icu_matches_golden(self, icu_files, filename):
        golden_path = os.path.join(GOLDEN_OS_DIR, "icu", filename)
        assert os.path.exists(golden_path), (
            f"golden fixture missing: {golden_path}"
        )
        with open(golden_path, "r", encoding="utf-8", newline="") as f:
            golden = f.read()
        assert icu_files[filename] == golden, (
            f"{filename} diverged from golden fixture — if the change is "
            f"intentional, regenerate the fixture and review the diff"
        )


# ===================================================================
# Compile smoke — generated config must compile standalone (-Werror)
# ===================================================================

class TestCompileSmoke:
    @pytest.mark.skipif(shutil.which("gcc") is None, reason="host gcc unavailable")
    def test_icu_outputs_compile_with_werror(self, icu_files, tmp_path):
        for name, content in icu_files.items():
            (tmp_path / name).write_text(content, encoding="utf-8", newline="\n")

        includes = [
            str(tmp_path),
            os.path.join(PROJECT_ROOT, "firmware", "bsw", "include"),
            os.path.join(PROJECT_ROOT, "firmware", "bsw", "os", "bootstrap", "include"),
            os.path.join(PROJECT_ROOT, "firmware", "bsw", "services", "WdgM", "include"),
        ]
        flags = ["-Wall", "-Wextra", "-Werror", "-Wshadow", "-Wundef", "-std=c99", "-c"]
        for src in ("Os_Cfg_Icu.c", "Rte_TaskBodies_Icu.c"):
            cmd = (["gcc"] + flags
                   + [f"-I{inc}" for inc in includes]
                   + [str(tmp_path / src), "-o", str(tmp_path / (src + ".o"))])
            proc = subprocess.run(cmd, capture_output=True, text=True)
            assert proc.returncode == 0, (
                f"{src} failed to compile:\n{proc.stderr}"
            )


# ===================================================================
# Sidecar os: schema — reader/model support and defaults
# ===================================================================

class TestSidecarOsSection:
    def test_model_has_os_config_defaults(self):
        from tools.arxmlgen.model import Ecu, EcuOsConfig
        ecu = Ecu(name="x", prefix="X")
        assert isinstance(ecu.os, EcuOsConfig)
        assert ecu.os.bsw_slot_periods == []
        assert ecu.os.default_task_stack_bytes == 1024
        assert ecu.os.scheduling == "FULL"
        assert ecu.os.applications == []

    def test_sidecar_os_sections_loaded(self, load_model):
        model = load_model
        assert model.ecus["icu"].os.bsw_slot_periods == [1, 10]
        assert model.ecus["tcu"].os.bsw_slot_periods == [1, 10]
        assert model.ecus["bcm"].os.bsw_slot_periods == [1, 10]
        assert model.ecus["cvc"].os.bsw_slot_periods == [10, 100, 5000]

    def test_absent_os_section_uses_defaults(self, load_model):
        # SC has no os: section in the sidecar
        model = load_model
        sc = model.ecus.get("sc")
        if sc is None:
            pytest.skip("sc not in model")
        assert sc.os.bsw_slot_periods == []
        assert sc.os.scheduling == "FULL"
