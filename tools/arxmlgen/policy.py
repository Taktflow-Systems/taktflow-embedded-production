"""
Model-level scheduling policy transforms (single-sourced: applied by the
reader after sidecar overrides, so EVERY generator — rte_cfg, os_cfg,
trace — consumes the same resolved priorities).

RATE-MONOTONIC RE-BANDING (S-OS-11 conflict resolution; user decision
2026-07-07; requirements trace: docs/plans/memo-rm-reband-trace.md):

The legacy Rte dispatcher orders runnables by priority descending across
ALL periods (Rte.c:40-105), which lets a long-period runnable interleave
between short-period runnables inside one tick — an order that the
one-task-per-period-group OSEK design cannot reproduce (os_cfg.py
order-equivalence gate). The re-band normalizes model priorities so that:

  band(period) — shorter period => strictly higher priority band
                 (SWR-CVC-032 / SWR-FZC-029 / SWR-RZC-028 all assign
                 priority levels by period already; SWR-RZC-028 places the
                 1 ms monitor Highest, above 10 ms ASIL D control),
  within a band — the previous relative order is preserved verbatim
                 (stable sort by prior priority descending, declaration
                 order on ties — identical tie semantics to rte_cfg.py
                 and the Rte.c selection sort).

New priorities are dense and unique, N-1..0 down the final order, in the
LEGACY RTE CONVENTION: larger number = dispatched earlier within a tick.
(The Os_Cfg generator derives its own kernel task priorities per period
group, kernel convention 0 = highest; it is unaffected by the numeric
values here and only consumes the ORDER.)

Only periodic non-init runnables are re-banded; init and aperiodic
runnables keep their declared priorities (they never enter the runnable
table). The transform is deterministic and idempotent.
"""

from __future__ import annotations

from .model import Ecu


def apply_rate_monotonic_banding(ecu: Ecu) -> None:
    """Re-band the ECU's periodic runnable priorities in place.

    @param ecu  ECU model after sidecar overrides have been applied.
    """
    # Legacy table order: periodic non-init runnables in declaration
    # order, stable-sorted by priority descending (== Rte_Cfg table).
    periodic = [
        r
        for swc in ecu.swcs
        for r in swc.runnables
        if not r.is_init and r.period_ms > 0
    ]
    legacy_order = sorted(periodic, key=lambda r: r.priority, reverse=True)

    # Bands ascending by period, concatenated shortest-first, preserving
    # legacy relative order inside each band.
    banded_order = []
    for period in sorted({r.period_ms for r in legacy_order}):
        banded_order.extend(r for r in legacy_order if r.period_ms == period)

    # Dense unique priorities, legacy convention (larger = earlier).
    count = len(banded_order)
    for pos, runnable in enumerate(banded_order):
        runnable.priority = count - 1 - pos
