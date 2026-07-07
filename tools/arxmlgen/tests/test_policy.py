"""
Tests for the rate-monotonic re-banding policy (S-OS-11 conflict
resolution, user decision 2026-07-07, docs/plans/memo-rm-reband-trace.md).

TDD: this contract is defined BEFORE the policy is implemented.

Policy under test (memo section 6): for each ECU, over periodic non-init
runnables as resolved after sidecar overrides —
 1. take the legacy table order (stable sort by priority descending,
    declaration order on ties — identical to rte_cfg.py / Rte.c),
 2. partition into bands by period, bands ordered ascending by period,
 3. concatenate bands shortest-first, preserving step-1 relative order
    inside each band,
 4. assign dense unique priorities N-1..0 down that order.
Deterministic, idempotent. Init and aperiodic runnables untouched.
"""

import pytest

from tools.arxmlgen.model import Ecu, Runnable, Swc
from tools.arxmlgen.policy import apply_rate_monotonic_banding


def _ecu(runnables):
    return Ecu(name="x", prefix="X", swcs=[Swc(name="Swc_X", runnables=runnables)])


def _table(ecu):
    """Legacy Rte_Cfg table order: periodic non-init, priority desc, stable."""
    rs = [r for s in ecu.swcs for r in s.runnables
          if not r.is_init and r.period_ms > 0]
    return sorted(rs, key=lambda r: r.priority, reverse=True)


class TestBanding:
    def test_shorter_period_band_strictly_higher(self):
        """Every runnable in a shorter-period band outranks every runnable
        in any longer-period band (memo section 6, SWR-RZC-028 pattern)."""
        ecu = _ecu([
            Runnable("Slow", period_ms=100, priority=9),
            Runnable("Fast", period_ms=1, priority=1),
            Runnable("Mid", period_ms=10, priority=5),
        ])
        apply_rate_monotonic_banding(ecu)
        by = {r.name: r.priority for s in ecu.swcs for r in s.runnables}
        assert by["Fast"] > by["Mid"] > by["Slow"]

    def test_within_band_preserves_legacy_relative_order(self):
        """In-band order = prior priority desc, declaration order on ties
        (memo invariant I3: safety stays ahead of QM within its band)."""
        ecu = _ecu([
            Runnable("B_tie_first", period_ms=10, priority=5),
            Runnable("A_high", period_ms=10, priority=8),
            Runnable("C_tie_second", period_ms=10, priority=5),
            Runnable("D_low", period_ms=10, priority=2),
        ])
        apply_rate_monotonic_banding(ecu)
        table = _table(ecu)
        assert [r.name for r in table] == [
            "A_high", "B_tie_first", "C_tie_second", "D_low",
        ]

    def test_priorities_dense_unique_descending(self):
        ecu = _ecu([
            Runnable("R1", period_ms=10, priority=5),
            Runnable("R2", period_ms=10, priority=5),
            Runnable("R3", period_ms=100, priority=5),
        ])
        apply_rate_monotonic_banding(ecu)
        prios = sorted(
            (r.priority for s in ecu.swcs for r in s.runnables), reverse=True
        )
        assert prios == [2, 1, 0]

    def test_init_and_aperiodic_runnables_untouched(self):
        init_r = Runnable("Init", period_ms=0, is_init=True, priority=7)
        aper_r = Runnable("Aperiodic", period_ms=0, priority=7)
        ecu = _ecu([init_r, aper_r, Runnable("Fast", period_ms=1, priority=1)])
        apply_rate_monotonic_banding(ecu)
        assert init_r.priority == 7
        assert aper_r.priority == 7

    def test_idempotent(self):
        ecu = _ecu([
            Runnable("Slow", period_ms=100, priority=9),
            Runnable("Fast", period_ms=1, priority=1),
            Runnable("MidA", period_ms=10, priority=5),
            Runnable("MidB", period_ms=10, priority=5),
        ])
        apply_rate_monotonic_banding(ecu)
        first = [(r.name, r.priority) for s in ecu.swcs for r in s.runnables]
        apply_rate_monotonic_banding(ecu)
        second = [(r.name, r.priority) for s in ecu.swcs for r in s.runnables]
        assert first == second

    def test_empty_ecu_is_noop(self):
        ecu = Ecu(name="x", prefix="X")
        apply_rate_monotonic_banding(ecu)  # must not raise
        assert ecu.swcs == []

    def test_bcm_tie_order_resolved(self):
        """The BCM counterexample (equal priority 5, 100 ms first in
        declaration order): banding must move the 100 ms runnable below
        the 10 ms band while keeping 10 ms declaration order."""
        ecu = _ecu([
            Runnable("Swc_DoorLock_100ms", period_ms=100, priority=5),
            Runnable("Swc_Indicators_10ms", period_ms=10, priority=5),
            Runnable("Swc_Lights_10ms", period_ms=10, priority=5),
        ])
        apply_rate_monotonic_banding(ecu)
        assert [r.name for r in _table(ecu)] == [
            "Swc_Indicators_10ms", "Swc_Lights_10ms", "Swc_DoorLock_100ms",
        ]


class TestReaderIntegration:
    """The reader must apply the banding, so every generator consumes the
    banded model (single source of truth)."""

    def test_model_is_banded_for_all_ecus(self, load_model):
        for ecu in load_model.ecus.values():
            table = _table(ecu)
            periods = [r.period_ms for r in table]
            # Band monotonicity: once the table order moves to a longer
            # period, no shorter period may reappear.
            assert periods == sorted(periods), (
                f"{ecu.name}: table not period-banded: {periods}"
            )
            prios = [r.priority for r in table]
            assert prios == sorted(prios, reverse=True)
            assert len(set(prios)) == len(prios), (
                f"{ecu.name}: banded priorities must be unique"
            )

    def test_cvc_heartbeat_is_lowest(self, load_model):
        """CVC: the 50 ms heartbeat is the only member of the longest
        runnable band -> priority 0 (memo CVC table)."""
        cvc = load_model.ecus["cvc"]
        hb = next(r for s in cvc.swcs for r in s.runnables
                  if r.name == "Swc_Heartbeat_MainFunction")
        assert hb.priority == 0

    def test_rzc_can_read_above_ten_ms_band(self, load_model):
        """RZC: Can_MainFunction_Read (1 ms) must outrank the 10 ms band
        (SWR-RZC-028 Highest(1) > High(2) structure; memo RZC table)."""
        rzc = load_model.ecus["rzc"]
        by = {r.name: r.priority for s in rzc.swcs for r in s.runnables}
        assert by["Can_MainFunction_Read"] > by["Swc_Encoder_MainFunction"]
        assert by["Swc_CurrentMonitor_MainFunction"] > by["Can_MainFunction_Read"]
