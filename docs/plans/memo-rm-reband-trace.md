# Memo — Rate-Monotonic Re-Banding of Runnable Priorities: Requirements Trace

Date: 2026-07-07
Branch: `feat/os-osek-migration`
Author input: user decision — resolve the S-OS-11 cross-period ordering
conflict by rate-monotonic re-banding of runnable priorities in the model
(shorter period => strictly higher priority band; within a band, preserve
the existing relative order). This memo is the discipline-rule-5 check
(HARA -> code) executed BEFORE any behavior change.

## 1. Scope and the conflict being resolved

The S-OS-11 order-equivalence gate (tools/arxmlgen/generators/os_cfg.py)
refuses to emit OSEK config for bcm, cvc, fzc, rzc because the legacy
`Rte_DispatchRunnables` order (priority-descending across ALL periods,
Rte.c:40-105) interleaves long-period runnables between short-period
runnables inside one tick, which one-task-per-period-group dispatch cannot
reproduce. Recorded counterexamples (generator output, committed tables):

- cvc: tick 50 — `Swc_Heartbeat_MainFunction` (50 ms, prio 3) runs between
  `Swc_VehicleState_MainFunction` and `Swc_Dashboard_MainFunction` (10 ms).
- fzc: tick 50 — `Swc_Heartbeat_MainFunction` (50 ms, prio 7) runs between
  `Swc_Lidar_MainFunction` and `Swc_FzcSafety_MainFunction` (10 ms).
- rzc: tick 10 — `Can_MainFunction_Read` (1 ms, prio 9) runs BELOW
  `Swc_Encoder/Swc_Motor_MainFunction` (10 ms, prio 10).
- bcm: tick 100 — `Swc_DoorLock_100ms` (100 ms) runs before the 10 ms
  runnables (equal priority 5, table-index tie).

The re-band changes the single source of truth (model runnable priorities
as resolved after `model/ecu_sidecar.yaml` overrides), so the legacy
`Rte_Cfg_<Ecu>.c` tables regenerate to the same banded order and legacy +
OSEK share one documented execution order.

## 2. Requirements chain examined (rule 5: HARA -> SG -> TSR -> SSR -> SWR)

| Level | ID | Where | What it constrains |
|---|---|---|---|
| FSR | FSR-024 | docs/safety/requirements/functional-safety-reqs.md | Safe state within FTTI per SG-001..008 |
| TSR | TSR-046 | docs/safety/requirements/technical-safety-reqs.md:1100 | FTTI budgets (detection/reaction/actuation) per safety goal |
| TSR | TSR-047 | docs/safety/requirements/technical-safety-reqs.md:1132 | Fixed diagnostic runnable PERIODS; "RTOS task priority for safety-critical runnables shall be higher than QM tasks" |
| SSR | SSR-CVC-023 | docs/safety/requirements/sw-safety-reqs.md:435 | CVC period/priority/WCET table; "safety runnables (priority 2) shall preempt QM runnables (priority 4)" |
| SSR | SSR-FZC-023 | docs/safety/requirements/sw-safety-reqs.md:816 | FZC period/priority/WCET table; safety preempts QM |
| SSR | SSR-RZC-017 | docs/safety/requirements/sw-safety-reqs.md (RZC section) | RZC WCET compliance (via SWR-RZC-028) |
| SWR | SWR-CVC-032 | docs/aspice/software/sw-requirements/SWR-CVC.md:602 | CVC runnable table: 10 ms=High(2), 50 ms=Medium(3), 100 ms=Medium(3), 200 ms=Low(4); <=80% utilization |
| SWR | SWR-FZC-029 | docs/aspice/software/sw-requirements/SWR-FZC.md:535 | FZC runnable table: 10 ms=High(2), 50 ms/100 ms=Medium(3), QM buzzer Low(4) |
| SWR | SWR-RZC-028 | docs/aspice/software/sw-requirements/SWR-RZC.md:524 | RZC: CurrentMonitor 1 ms=Highest(1) ABOVE 10 ms=High(2) ABOVE 50/100 ms=Medium(3) |

Verified-by artifacts: firmware/ecu/cvc/test/test_Swc_Scheduler_asild.c,
fzc/test/test_Swc_FzcScheduler_asild.c, rzc/test/test_Swc_RzcScheduler_asild.c
(assert the Swc_*Scheduler mirror tables, NOT the generated Rte_Cfg tables).

Key observation: the requirement tables themselves are ALREADY
rate-monotonic — every SWR/SSR scheduling table assigns priority strictly
by period (1 ms Highest, 10 ms High, 50/100 ms Medium, 200 ms Low).
SWR-RZC-028 explicitly places the 1 ms CurrentMonitor ABOVE the 10 ms
ASIL D MotorControl. Rate-monotonic banding moves the generated tables
TOWARD the requirements, not away from them.

## 3. Per-ECU current -> banded priority tables

Convention: legacy Rte priority — larger number = dispatched earlier within
a tick (Rte.c selection sort, strictly-greater; rte_cfg.py sorts
descending). New priorities are dense, unique, descending in final band
order. Kernel task priorities (Os_Cfg) are separate (0 = highest per
period group) and unchanged in convention.

### CVC (band order: 1 ms > 10 ms > 50 ms)

| Runnable | Period | Old prio | New prio | Move |
|---|---|---|---|---|
| Can_MainFunction_Read | 1 | 9 | 9 | none |
| Can_MainFunction_Write | 1 | 8 | 8 | none |
| Com_MainFunction_Rx | 10 | 7 | 7 | none |
| Swc_EStop_MainFunction | 10 | 6 | 6 | none |
| Swc_Pedal_MainFunction | 10 | 5 | 5 | none |
| Swc_VehicleState_MainFunction | 10 | 4 | 4 | none |
| Swc_Dashboard_MainFunction | 10 | 2 | 3 | up 1 slot at tick%50==0 |
| Com_MainFunction_Tx | 10 | 1 | 2 | up 1 slot at tick%50==0 |
| Can_MainFunction_BusOff | 10 | 0 | 1 | up 1 slot at tick%50==0 |
| Swc_Heartbeat_MainFunction | 50 | 3 | 0 | now LAST at tick%50==0 |

### FZC (band order: 10 ms > 50 ms)

| Runnable | Period | Old prio | New prio | Move |
|---|---|---|---|---|
| Can_MainFunction_Read | 10 | 14 | 12 | none |
| Com_MainFunction_Rx | 10 | 13 | 11 | none |
| Swc_FzcCom_Receive | 10 | 12 | 10 | none |
| Swc_FzcSensorFeeder_MainFunction | 10 | 11 | 9 | none |
| Swc_Steering_MainFunction | 10 | 10 | 8 | none |
| Swc_Brake_MainFunction | 10 | 9 | 7 | none |
| Swc_Lidar_MainFunction | 10 | 8 | 6 | none |
| Swc_FzcSafety_MainFunction | 10 | 6 | 5 | up 1 slot at tick%50==0 |
| Swc_Buzzer_MainFunction | 10 | 5 | 4 | up 1 slot at tick%50==0 |
| Swc_FzcCanMonitor_Check | 10 | 4 | 3 | up 1 slot at tick%50==0 |
| Com_MainFunction_Tx | 10 | 2 | 2 | up 1 slot at tick%50==0 |
| Can_MainFunction_BusOff | 10 | 1 | 1 | up 1 slot at tick%50==0 |
| Swc_Heartbeat_MainFunction | 50 | 7 | 0 | now LAST at tick%50==0 |

### RZC (band order: 1 ms > 10 ms > 50 ms > 100 ms)

| Runnable | Period | Old prio | New prio | Move |
|---|---|---|---|---|
| Swc_CurrentMonitor_MainFunction | 1 | 11 | 11 | none |
| Can_MainFunction_Read | 1 | 9 | 10 | now BEFORE Encoder/Motor at tick%10==0 |
| Swc_Encoder_MainFunction | 10 | 10 | 9 | after 1 ms band |
| Swc_Motor_MainFunction | 10 | 10 | 8 | after 1 ms band |
| Com_MainFunction_Rx | 10 | 8 | 7 | none |
| Swc_RzcSensorFeeder_MainFunction | 10 | 6 | 6 | none |
| Com_MainFunction_Tx | 10 | 4 | 5 | before 100 ms band at tick%100==0 |
| Can_MainFunction_BusOff | 10 | 2 | 4 | before 50/100 ms bands |
| Swc_Heartbeat_MainFunction | 50 | 3 | 3 | after full 10 ms band |
| Swc_Battery_MainFunction | 100 | 4 | 2 | after 10/50 ms bands |
| Swc_TempMonitor_MainFunction | 100 | 4 | 1 | after 10/50 ms bands |
| Swc_RzcSafety_MainFunction | 100 | 2 | 0 | now LAST at tick%100==0 |

### BCM (band order: 10 ms > 100 ms)

| Runnable | Period | Old prio | New prio | Move |
|---|---|---|---|---|
| Swc_Indicators_10ms | 10 | 5 | 2 | first at tick%100==0 |
| Swc_Lights_10ms | 10 | 5 | 1 | none (relative) |
| Swc_DoorLock_100ms | 100 | 5 | 0 | now LAST at tick%100==0 |

### ICU / TCU

- ICU: single 50 ms band; order unchanged (`Swc_Dashboard_50ms`,
  `Swc_DtcDisplay_50ms`); priorities renumbered 5,5 -> 1,0 (dense unique).
- TCU: no periodic runnables; no change.

Gate re-check (simulation over LCM window, script parity with
verify_order_equivalence): after re-banding ALL SIX ECUs are
order-equivalent (one-task-per-period-group reproduces the re-banded
legacy dispatch tick-exactly, including WdgM checkpoint pattern).
Resulting period groups: cvc {1,10,50,100,5000}=5, rzc {1,10,50,100,5000}=5,
fzc {10,50,100,5000}=4, bcm {1,10,100}=3, icu {1,10,50}=3, tcu {1,10}=2.
cvc/rzc therefore require OS_MAX_SCHEDULE_TABLES >= 5 (kernel bound is 4
at HEAD -> raised in this package; group structure is period-driven and
unchanged by the re-band, so the bump remains necessary).

## 4. Invariants — held or broken

| # | Invariant | Source | Verdict |
|---|---|---|---|
| I1 | Diagnostic runnable PERIODS fixed (CurrentMonitor 1 ms, monitors 10 ms, TempMonitor 100 ms, etc.) | TSR-047 table | HELD — re-band never touches periods |
| I2 | Detection/reaction/actuation budgets per SG within FTTI | TSR-046 table | HELD — periods unchanged; per-tick WCET sum unchanged (same runnable set per tick, order permuted). Worst-case added output latency is one 10 ms Com_MainFunction_Tx cycle for runnables moved behind Com_Tx (see 4.1) — bounded by existing margins |
| I3 | Safety runnables above QM runnables within the SAME period band | SSR-CVC-023 / SSR-FZC-023 / SWR tables ("safety preempts QM") | HELD — within-band relative order is preserved verbatim; every band keeps safety members ahead of QM members exactly as today (e.g. fzc Swc_FzcSafety > Swc_Buzzer) |
| I4 | 1 ms safety monitor above everything (RZC CurrentMonitor Highest) | SWR-RZC-028 row 1 | HELD (strengthened — the whole 1 ms band now strictly outranks 10 ms, matching the requirement's Highest(1) > High(2) structure) |
| I5 | Utilization <= 80% of shortest cycle | SWR-CVC-032 / SWR-FZC-029 / SWR-RZC-028 | HELD — WCET budgets and periods unchanged; permuting intra-tick order does not change sums |
| I6 | WdgM alive supervision: one checkpoint per unique seId per due tick | Rte.c:93-100 replica in the gate | HELD — proven by the order-equivalence simulation for all 6 ECUs post-band |
| I7 | Strict runnable-level reading of TSR-047 sentence "safety-critical priority higher than QM tasks" across DIFFERENT periods | TSR-047 (sentence, not table) | NOT NEWLY BROKEN — see 4.1; already not satisfied by the committed pre-change tables, and not the verified invariant |

### 4.1 Analysis of I7 (the only debatable point) and the +10 ms latency

Cross-band consequences of rate-monotonic banding: a shorter-period QM/BSW
runnable can now rank above a longer-period safety runnable
(cvc: Swc_Dashboard QM 10 ms > Swc_Heartbeat ASIL-C 50 ms;
rzc: Com/Can BSW 10 ms > Swc_TempMonitor ASIL-A / Swc_RzcSafety 100 ms).
This does NOT violate the safety chain, for four reasons:

1. **No preemption exists.** Both the legacy dispatcher and the generated
   OSEK design run runnables to completion, cooperatively; "priority" is
   intra-tick ORDER selection only (Rte.c:40-105; Os task bodies are
   run-to-completion basic tasks). A QM runnable cannot interrupt or
   starve a safety runnable; every due runnable completes in its tick.
   Freedom from interference is enforced by WCET measurement
   (SchM_Timing) + WdgM alive supervision, both unchanged.
2. **The pre-change tables already contain the same pattern.** Committed
   Rte_Cfg tables place BSW QM mains (Can_MainFunction_Read/Write,
   Com_MainFunction_Rx) above every ASIL-D safety runnable on every ECU,
   and fzc places QM Swc_Buzzer (prio 5) above ASIL-C
   Swc_FzcCanMonitor_Check (prio 4). The strict runnable-level reading of
   the TSR-047 sentence was never the implemented or verified invariant.
3. **The verified invariant lives in the Swc_*Scheduler mirror tables**
   (test_Swc_Scheduler_asild.c, test_Swc_FzcScheduler_asild.c,
   test_Swc_RzcScheduler_asild.c verify SWR-CVC-032/SWR-FZC-029/
   SWR-RZC-028 against the Swc_*Scheduler config tables). Those tables and
   the SWR tables they mirror are themselves period-monotonic and are NOT
   touched by this change; the suites keep passing with the invariant
   genuinely held.
4. **Deadline impact is bounded and inside TSR-046 margins.** The only
   directional change is that 50/100 ms runnables now run AFTER the 10 ms
   band (incl. Com_MainFunction_Tx), so a signal they write is transmitted
   on the NEXT 10 ms Com_Tx cycle: +10 ms worst-case output latency.
   - Heartbeat (cvc/fzc/rzc, 50 ms period): SC heartbeat timeout is
     150 ms (TSR-047 SC row); 50 ms + 10 ms shift keeps > 2 transmissions
     per timeout window. SG-008 margin 35 ms also absorbs the shift
     (detection is SC-side timeout, unchanged).
   - Swc_RzcSafety / Swc_TempMonitor / Swc_Battery (100 ms): SG-006 FTTI
     500 ms, margin 389 ms (TSR-046) — +10 ms is 2.6% of margin.
   - Swc_DoorLock (bcm, 100 ms): QM body function, no safety goal.
   - RZC Can_MainFunction_Read now runs BEFORE Swc_Encoder/Swc_Motor on
     shared ticks: motor control consumes FRESHER torque-request data —
     latency improvement on the SG-001 reaction path (10 ms budget row,
     TSR-046).

## 5. Verdict

**No safety invariant is violated by rate-monotonic banding. PROCEED** to
implementation (policy transform in tools/arxmlgen, regeneration, kernel
schedule-table bound 4 -> 5, downstream test retrace).

Notes carried forward (pre-existing, out of scope, not worsened):

- SWR-CVC-032/SSR tables use aspirational FreeRTOS runnable names
  (Swc_PedalPlausibility, rte_schedule.c) that do not match the
  implemented artifact names — status "draft", tracked by the ASPICE
  requirement docs, unchanged here.
- firmware/ecu/fzc/src/Swc_FzcScheduler.c places WatchdogFeed (100 ms) at
  FZC_SCHED_PRIO_HIGH while SWR-FZC-029 says Medium(3) — pre-existing
  mirror-table discrepancy, untouched by this change.

## 5.1 Closure record (appended 2026-07-07, post-implementation)

- Policy implemented: `tools/arxmlgen/policy.py`
  (`apply_rate_monotonic_banding`), applied in `reader.py` after sidecar
  overrides. Tests: `tools/arxmlgen/tests/test_policy.py` (banding
  properties + reader integration), `test_os_generator.py` (equivalence
  proof extended to ALL SIX ECUs from the committed regenerated tables;
  refusal path kept alive with a synthetic non-banded model).
- Two hand-carried drifts discovered during regeneration and wired into
  the model instead of being silently reverted (project rule: fix the
  pipeline, never the output):
  - `Swc_FzcCom_Receive` (fix f22eb15, CanMon silence-counter reset) —
    new sidecar `keep: true` opt-out of the legacy Com-bridge skip;
    closes the `TODO:CODEGEN` notes in Rte_Cfg_Fzc.c / fzc Rte_PbCfg.h.
  - `Can_MainFunction_Write` (cvc 1 ms TX pump, hand-added in b14cbc1) —
    new sidecar entry; also fixes latent cvc `RTE_MAX_RUNNABLES` 9 vs 10
    undersizing.
  - NOT regenerated (out of re-band scope, would revert committed fixes
    the pipeline cannot yet express): `CanIf_Cfg_Cvc.c`/`Com_Cfg_Cvc.c`
    0x600/0x601 TX_MODE_NONE (4ef2bde) and `Com_Cfg_Fzc.c`
    MOTOR_CUTOFF_REQ direct-mode (46629dc). These remain open codegen
    gaps (candidate: sidecar tx-mode override), tracked for
    plan-codegen-gap-closure.md.
- Kernel `OS_MAX_SCHEDULE_TABLES` 4 -> 5 (Os.h + generator mirror), with
  new bound tests in `test_Os_ScheduleTable.c`; OS suite 32/32 green.
- Downstream scheduler suites (cvc/fzc/rzc) pass UNCHANGED — they assert
  the Swc_*Scheduler mirror tables (section 4.1 point 3); dated @note
  trace comments added to the three test files.
- Evidence + required CI SIL parity gate recorded in
  `docs/plans/os-migration-baseline.md` (2026-07-07 rm re-band section);
  S-OS-11 status updated in `plan-osek-os-migration.md`.

## 6. Policy statement (implemented in tools/arxmlgen)

For each ECU, over periodic non-init runnables as resolved after sidecar
overrides: (1) take the legacy table order (stable sort by priority
descending, declaration order on ties — identical to rte_cfg.py and
Rte.c tie semantics); (2) partition into bands by period, bands ordered
ascending by period; (3) concatenate bands (shortest first), preserving
step-1 relative order inside each band; (4) assign dense unique
priorities N-1..0 down that order. The transform is deterministic and
idempotent; both the Rte_Cfg generator (larger number = earlier, legacy
convention) and the Os_Cfg generator (task priority 0 = highest, kernel
convention) consume the same banded model.
