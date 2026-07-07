"""
Os_Cfg generator (S-OS-11) — produces per-ECU OSEK kernel configuration
(Os_Cfg_<Ecu>.c/.h) and RTE task bodies (Rte_TaskBodies_<Ecu>.c) from the
model, DBC-first.

Design (plan-osek-os-migration.md S-OS-11):
- One OSEK basic task per period group. Period groups are the union of the
  ECU's runnable-table periods and the super-loop BSW slot periods from the
  sidecar `os:` section (plan section 1.1).
- Task priority derived so shorter period -> higher priority. Kernel
  convention (Os_Scheduler.c os_select_next_ready_task): numeric priority 0
  is highest. A mandatory idle task sits at OS_MAX_PRIORITIES-1, autostarted,
  FULL (kernel os_cfg_validate_idle_task, FMEA OS-C-03).
- One repeating schedule table per period group: Duration == period, one
  expiry point at offset == period. Offsets are phase-preserving (NOT
  staggered): the legacy dispatcher fires a period-P runnable at every tick
  with tick % P == 0, and the S-OS-11 acceptance criterion requires the
  generated activation pattern to reproduce that sequence tick-exactly.
  Same-tick pileup is resolved by task priority order.

ORDER-EQUIVALENCE GATE (fail-closed):
  Before emitting anything, the generator proves over an LCM-of-periods
  window that the one-task-per-period-group execution reproduces exactly
  the runnable + WdgM-checkpoint sequence of the legacy dispatcher
  (firmware/bsw/rte/src/Rte.c:40-105). If a counterexample exists (e.g.
  cross-period priority interleaving), the ECU is REFUSED: no files are
  emitted, the counterexample is recorded in `OsCfgGenerator.refusals`
  and printed. The generator never silently changes execution order.
"""

from __future__ import annotations

import math
import os
import sys
from dataclasses import dataclass, field

from ..engine import OutputFile, TemplateEngine
from ..model import Ecu

name = "os"

#: Mirrors RTE_MAX_RUNNABLES (Rte.h) — WdgM checkpoint guard bound used by
#: Rte_DispatchRunnables (`se < RTE_MAX_RUNNABLES`).
RTE_MAX_RUNNABLES = 16

#: Kernel limits (firmware/bsw/os/bootstrap/include/Os.h). Schedule-table
#: bound raised 4 -> 5 with the rate-monotonic re-band: cvc/rzc period
#: groups are {1,10,50,100,5000} ms (memo-rm-reband-trace.md section 3).
OS_MAX_TASKS = 8
OS_MAX_SCHEDULE_TABLES = 5
OS_MAX_PRIORITIES = 8


@dataclass
class OsRunnable:
    """One legacy runnable-table row (func/periodMs/priority/seId)."""

    name: str
    period: int
    priority: int
    se_id: int = 0xFF


@dataclass
class PeriodGroup:
    """One period group == one OSEK task."""

    period: int
    task_priority: int = 0
    runnables: list[OsRunnable] = field(default_factory=list)


@dataclass
class Counterexample:
    """First tick at which the OSEK task decomposition diverges from the
    legacy Rte_DispatchRunnables sequence."""

    tick: int
    legacy: list[tuple]
    osek: list[tuple]

    def _tick_events(self, seq):
        return [e for e in seq if e[1] == self.tick]

    def __str__(self):
        legacy_t = [
            (e[0], e[2]) for e in self._tick_events(self.legacy)
        ]
        osek_t = [
            (e[0], e[2]) for e in self._tick_events(self.osek)
        ]
        return (
            f"order divergence at tick {self.tick}: "
            f"legacy={legacy_t} vs task-groups={osek_t}"
        )


# ======================================================================
# Legacy table collection — same semantics as RteCfgGenerator.render()
# ======================================================================

def collect_legacy_runnable_table(ecu: Ecu) -> list[OsRunnable]:
    """Collect the ECU's runnable table in the exact order Rte_Cfg_<Ecu>.c
    carries it: periodic non-init runnables, sorted by priority descending
    (Python stable sort — ties keep SWC declaration order)."""
    all_runnables = []
    for swc in ecu.swcs:
        for r in swc.runnables:
            if not r.is_init and r.period_ms > 0:
                all_runnables.append(r)
    ordered = sorted(all_runnables, key=lambda r: r.priority, reverse=True)
    return [
        OsRunnable(
            name=r.name,
            period=r.period_ms,
            priority=r.priority,
            se_id=r.wdgm_se_id,
        )
        for r in ordered
    ]


# ======================================================================
# Period groups
# ======================================================================

def derive_period_groups(
    table: list[OsRunnable], bsw_slot_periods: list[int]
) -> list[PeriodGroup]:
    """Union of runnable periods and BSW slot periods, sorted ascending.
    Task priority: shorter period -> lower number (= higher priority per
    the kernel convention). Group members keep legacy execution order
    (priority desc, ties by table index)."""
    periods = sorted(
        {r.period for r in table} | {int(p) for p in bsw_slot_periods}
    )
    groups = []
    for prio, period in enumerate(periods):
        members = [r for r in table if r.period == period]
        # Within one group all members share the period, so their mutual
        # legacy order is priority desc with table-index tie-break.
        members = sorted(
            enumerate(members), key=lambda ir: (-ir[1].priority, ir[0])
        )
        groups.append(
            PeriodGroup(
                period=period,
                task_priority=prio,
                runnables=[r for _, r in members],
            )
        )
    return groups


# ======================================================================
# Dispatch simulations
# ======================================================================

def simulate_legacy_dispatch(
    table: list[OsRunnable], ticks: int
) -> list[tuple]:
    """Tick-by-tick replica of Rte_DispatchRunnables (Rte.c:40-105):
    selection-sort by strictly-greater priority (ties: lowest index),
    runnable due when tick % period == 0, WdgM checkpoint once per unique
    seId per tick for seId < RTE_MAX_RUNNABLES.

    Returns events: ("run", tick, name) and ("wdgm", tick, se_id)."""
    events: list[tuple] = []
    n = len(table)

    for tick in range(1, ticks + 1):
        visited = [False] * n
        se_checkpointed: set[int] = set()

        for _pass in range(n):
            best_idx = -1
            best_priority = 0
            found = False

            for i in range(n):
                if visited[i]:
                    continue
                r = table[i]
                if (r.period == 0) or ((tick % r.period) != 0):
                    visited[i] = True
                    continue
                if (not found) or (r.priority > best_priority):
                    best_idx = i
                    best_priority = r.priority
                    found = True

            if not found:
                break

            visited[best_idx] = True
            events.append(("run", tick, table[best_idx].name))

            se = table[best_idx].se_id
            if se < RTE_MAX_RUNNABLES and se not in se_checkpointed:
                events.append(("wdgm", tick, se))
                se_checkpointed.add(se)

    return events


def simulate_osek_dispatch(
    table: list[OsRunnable], groups: list[PeriodGroup], ticks: int
) -> list[tuple]:
    """Simulate the generated design: at each counter tick the schedule
    tables activate every due period-group task (tick % period == 0);
    ready tasks run to completion in task-priority order (0 first,
    Os_Scheduler.c); each task body calls its runnables in fixed order and
    raises WdgM checkpoints once per unique valid seId per activation
    (the per-body replica of Rte.c:93-100)."""
    events: list[tuple] = []
    ordered_groups = sorted(groups, key=lambda g: g.task_priority)

    for tick in range(1, ticks + 1):
        for group in ordered_groups:
            if (tick % group.period) != 0:
                continue
            body_checkpointed: set[int] = set()
            for r in group.runnables:
                events.append(("run", tick, r.name))
                if r.se_id < RTE_MAX_RUNNABLES and r.se_id not in body_checkpointed:
                    events.append(("wdgm", tick, r.se_id))
                    body_checkpointed.add(r.se_id)

    return events


def verify_order_equivalence(
    table: list[OsRunnable], bsw_slot_periods: list[int] | None = None
) -> Counterexample | None:
    """Prove (or refute) order equivalence over an LCM-of-periods window.

    Returns None when the one-task-per-period-group design reproduces the
    legacy sequence exactly; otherwise the first-divergence Counterexample.
    """
    if not table:
        return None

    slots = list(bsw_slot_periods or [])
    groups = derive_period_groups(table, slots)

    window = 1
    for r in table:
        window = math.lcm(window, r.period)

    legacy = simulate_legacy_dispatch(table, window)
    osek = simulate_osek_dispatch(table, groups, window)

    if legacy == osek:
        return None

    # Locate the first tick whose event slice differs.
    for tick in range(1, window + 1):
        legacy_t = [e for e in legacy if e[1] == tick]
        osek_t = [e for e in osek if e[1] == tick]
        if legacy_t != osek_t:
            return Counterexample(tick=tick, legacy=legacy, osek=osek)

    # Sequences differ but no per-tick slice does — cannot happen.
    return Counterexample(tick=0, legacy=legacy, osek=osek)


# ======================================================================
# Generator
# ======================================================================

class OsCfgError(Exception):
    """Os generator configuration error (kernel limits exceeded)."""


class OsCfgGenerator:
    """Generates Os_Cfg_<Ecu>.c/.h and Rte_TaskBodies_<Ecu>.c per ECU."""

    def __init__(self):
        #: ecu_name -> refusal reason (Counterexample or str). ECUs listed
        #: here produced NO output (fail-closed order-equivalence gate).
        self.refusals: dict[str, object] = {}

    # ------------------------------------------------------------------

    def render(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> dict[str, str]:
        """Render all Os outputs for one ECU.

        @return {filename: content}; empty dict when the ECU is refused.
        """
        table = collect_legacy_runnable_table(ecu)
        slots = ecu.os.bsw_slot_periods

        cex = verify_order_equivalence(table, slots)
        if cex is not None:
            self.refusals[ecu.name] = cex
            _err(
                f"os: {ecu.name} REFUSED — one-task-per-period-group cannot "
                f"reproduce the legacy Rte_DispatchRunnables order; {cex}. "
                f"No Os_Cfg emitted (fail-closed)."
            )
            return {}

        groups = derive_period_groups(table, slots)

        limit_reason = self._check_kernel_limits(groups)
        if limit_reason is not None:
            self.refusals[ecu.name] = limit_reason
            _err(f"os: {ecu.name} REFUSED — {limit_reason}. No Os_Cfg emitted.")
            return {}

        context = self._build_context(ecu, groups)

        prefix_pascal = ecu.prefix.capitalize()
        files = {
            f"Os_Cfg_{prefix_pascal}.h": engine.render(
                "os/Os_Cfg.h.j2",
                dict(context,
                     filename=f"Os_Cfg_{prefix_pascal}.h",
                     brief=f"OSEK kernel configuration for {ecu.prefix} — "
                           f"task/schedule-table/stack tables (S-OS-11)"),
            ),
            f"Os_Cfg_{prefix_pascal}.c": engine.render(
                "os/Os_Cfg.c.j2",
                dict(context,
                     filename=f"Os_Cfg_{prefix_pascal}.c",
                     brief=f"OSEK kernel configuration for {ecu.prefix} — "
                           f"task/schedule-table/stack tables (S-OS-11)"),
            ),
            f"Rte_TaskBodies_{prefix_pascal}.c": engine.render(
                "rte/Rte_TaskBodies.c.j2",
                dict(context,
                     filename=f"Rte_TaskBodies_{prefix_pascal}.c",
                     brief=f"Generated OSEK task bodies for {ecu.prefix} — "
                           f"legacy-order runnable dispatch (S-OS-11)"),
            ),
        }
        return files

    # ------------------------------------------------------------------

    @staticmethod
    def _check_kernel_limits(groups: list[PeriodGroup]) -> str | None:
        task_count = len(groups) + 1  # + idle
        if task_count > OS_MAX_TASKS:
            return (
                f"{task_count} tasks (incl. idle) exceed OS_MAX_TASKS="
                f"{OS_MAX_TASKS}"
            )
        if len(groups) > OS_MAX_SCHEDULE_TABLES:
            return (
                f"{len(groups)} period groups exceed OS_MAX_SCHEDULE_TABLES="
                f"{OS_MAX_SCHEDULE_TABLES}"
            )
        if len(groups) >= (OS_MAX_PRIORITIES - 1):
            return (
                f"{len(groups)} period groups collide with the idle "
                f"priority OS_MAX_PRIORITIES-1={OS_MAX_PRIORITIES - 1}"
            )
        return None

    def _build_context(self, ecu: Ecu, groups: list[PeriodGroup]) -> dict:
        prefix_pascal = ecu.prefix.capitalize()
        prefix_upper = ecu.prefix.upper()

        group_ctx = []
        for idx, g in enumerate(groups):
            stack = ecu.os.task_stack_bytes.get(
                g.period, ecu.os.default_task_stack_bytes
            )
            # Precompute which runnable raises which WdgM checkpoint:
            # first occurrence of each unique valid seId within the group
            # (static replica of the per-dispatch bitmap in Rte.c:93-100).
            seen_se: set[int] = set()
            runnables_ctx = []
            for r in g.runnables:
                raise_se = (
                    r.se_id < RTE_MAX_RUNNABLES and r.se_id not in seen_se
                )
                if raise_se:
                    seen_se.add(r.se_id)
                runnables_ctx.append({
                    "name": r.name,
                    "se_id": r.se_id,
                    "raise_checkpoint": raise_se,
                })
            group_ctx.append({
                "period": g.period,
                "task_id": idx,
                "task_priority": g.task_priority,
                "task_name": f"Os_Task_{prefix_pascal}_{g.period}ms",
                "task_define": f"OS_TASK_{prefix_upper}_{g.period}MS",
                "prio_define": f"OS_TASK_PRIO_{prefix_upper}_{g.period}MS",
                "table_define": f"OS_TABLE_{prefix_upper}_{g.period}MS",
                "ep_symbol": f"{ecu.name}_os_ep_{g.period}ms",
                "cfg_name": f"{prefix_pascal}_{g.period}ms",
                "stack_bytes": stack,
                "runnables": runnables_ctx,
            })

        applications = self._build_applications(ecu, groups)

        return {
            "ecu": ecu,
            "tool_version": "1.0.0",
            "arxml_source": "TaktflowSystem.arxml",
            "prefix_pascal": prefix_pascal,
            "prefix_upper": prefix_upper,
            "groups": group_ctx,
            "task_count": len(groups) + 1,
            "idle_task_id": len(groups),
            "idle_task_name": f"Os_Task_{prefix_pascal}_Idle",
            "idle_task_define": f"OS_TASK_{prefix_upper}_IDLE",
            "idle_stack_bytes": ecu.os.default_task_stack_bytes,
            "scheduling": ecu.os.scheduling,
            "applications": applications,
            "has_checkpoints": any(
                r["raise_checkpoint"] for g in group_ctx for r in g["runnables"]
            ),
        }

    @staticmethod
    def _build_applications(ecu: Ecu, groups: list[PeriodGroup]) -> list[dict]:
        """OS-Application grouping — emitted only if present in the sidecar."""
        if not ecu.os.applications:
            return []

        period_to_index = {g.period: i for i, g in enumerate(groups)}
        idle_index = len(groups)

        apps = []
        for app in ecu.os.applications:
            mask = 0
            for task_key in app.get("tasks", []):
                if str(task_key).lower() == "idle":
                    mask |= 1 << idle_index
                else:
                    period = int(task_key)
                    if period not in period_to_index:
                        raise OsCfgError(
                            f"os.applications for '{ecu.name}': task period "
                            f"{period} is not a period group"
                        )
                    mask |= 1 << period_to_index[period]
            apps.append({
                "name": app.get("name", "OsApp"),
                "trusted": bool(app.get("trusted", True)),
                "task_mask": mask,
            })
        return apps

    # ------------------------------------------------------------------

    def generate(self, ecu: Ecu, gen_config, engine: TemplateEngine) -> list[OutputFile]:
        """Generate and write Os_Cfg_<Ecu>.c/.h + Rte_TaskBodies_<Ecu>.c."""
        rendered = self.render(ecu, gen_config, engine)
        results = []

        for filename, content in rendered.items():
            path = os.path.join(
                engine.config.project_root,
                engine.config.output.base_dir,
                ecu.name,
                engine.config.output.cfg_dir,
                filename,
            )
            result = engine.write_file(path, content)
            if filename.startswith("Rte_TaskBodies"):
                result.template = "rte/Rte_TaskBodies.c.j2"
            elif filename.endswith(".h"):
                result.template = "os/Os_Cfg.h.j2"
            else:
                result.template = "os/Os_Cfg.c.j2"
            results.append(result)

        return results


def _err(msg: str):
    print(f"  ERROR   {msg}", file=sys.stderr)
