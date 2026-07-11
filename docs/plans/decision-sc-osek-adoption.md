# Decision Memo — SC (TMS570) Scheduling: Bare RTI Loop vs OSEK Kernel (S-OS-02)

Status: APPROVED — Option B selected for the experimental platform
Date: 2026-07-07
Decision owner: user (blocking gate for Phase 4 / S-OS-40 of
`docs/plans/plan-osek-os-migration.md`)

## Question

Does the Safety Controller (TMS570LC43x, lockstep, kill-relay owner) adopt
the OSEK bootstrap kernel for its scheduling (as demonstrated on hardware by
fix-branch commit `97dd2f4`), or does it keep its bare cooperative RTI
10 ms loop?

## Governing rule (exact text)

`.claude/rules/firmware-general.md`, section "Safety Controller (SC)":

> TMS570 runs lockstep — NO AUTOSAR BSW stack. Minimal, auditable code only.

Note the rule's letter forbids the AUTOSAR *BSW stack*. The OSEK bootstrap
kernel is not the AUTOSAR BSW stack (no Com/PduR/Dem/Rte), but the rule's
spirit — "minimal, auditable code only" — is the actual bar this memo
addresses. Adopting the kernel does not violate the rule's letter; whether
it violates its spirit is the user's call.

## Requirements trace (SC timing / watchdog chain)

- **SSR-A-002** (sw-safety-reqs.md, assumptions table): "The SC runs
  bare-metal (no RTOS) with a cooperative main loop — SC timing analysis
  assumes no preemption." **Option B invalidates this recorded assumption;
  it must be rewritten and the SC timing analysis redone.**
- **SSR-SC-014 "SC FTTI Monitoring"** (ASIL D, traces up TSR-046, verified
  by TC-SC-026): 10 ms main loop, fixed 6-step order (CAN receive →
  heartbeat counters → cross-plausibility → relay trigger evaluation →
  fault LEDs → watchdog feed), loop WCET <= 2 ms, overrun DTC at 5 ms via
  hardware timer.
- **SSR-SC-008 "SC External Watchdog Feed"** (ASIL D, traces up TSR-031):
  TPS3823 WDI toggled once per loop iteration only if (a) full loop
  completed, (b) RAM test pattern intact, (c) DCAN1 not bus-off,
  (d) lockstep ESM not asserted.
- **SSR-SC-016** startup self-test gates relay energize; **SSR-SC-007**
  de-energize latch (non-clearable).
- HSI spec section 7: RTI 10 ms tick (75 MHz/7500 → compare at 100);
  ~3 KB of 512 KB RAM used, "expected for a bare-metal safety monitor";
  diverse-redundancy table lists **"OS"** as one of the 8 diversity
  dimensions vs the STM32 zone ECUs.

## Option A — SC keeps the bare RTI cooperative loop (status quo)

Scheduling stays `sc_main.c` (376 LOC): RTI-flag-polled 10 ms cooperative
sequence, no preemption, watchdog fed as the final step.

- Audit-surface LOC delta: **0**. SC application today is ~4,156 LOC
  (firmware/ecu/sc/src/*.c) plus platform glue.
- Requirements impact: none. SSR-A-002, SSR-SC-014 order, WCET analysis,
  TC-SC-026 all remain valid as written.
- Diversity argument (HSI section 7.9) keeps "OS" as a diversity dimension
  against the zone ECUs once they run OSEK (Phases 2-3): SC scheduling
  failure modes stay decorrelated from zone-ECU scheduling failure modes.
- Ethernet/XCP work: continues as today — `sc_eth_telemetry` producer
  (S-UDP-03) and `sc_xcp_eth` slave (S-XCP-02) run as steps of the 10 ms
  loop; no change.
- Cost: SC remains the one ECU outside the unified scheduler; the
  `97dd2f4` hardware bringup work stays parked (harvested, compiled
  nowhere); plan step S-OS-40 reduces to documenting the exemption in the
  safety manual (os-sc3-safety-manual.md).

## Option B — OSEK kernel on SC (per `97dd2f4` pattern)

`SC_Task_Main` becomes an alarm-driven 10 ms OSEK task: RTI compare ISR →
counter tick → alarm expiry → dispatch; ESM lockstep monitoring re-enabled
alongside (that ESM work is orthogonal and already harvested).
`Makefile.tms570` compiles kernel + port; `sc_os_cfg.c` (parked in this
branch, 50 LOC) provides the task/alarm tables.

- Audit-surface LOC delta added to the SC image (hardware-compiled TUs):
  - kernel `firmware/bsw/os/bootstrap/src/*.c`: **~3,570 LOC**
  - port hardware files (`Os_Port_Tms570_Asm.S` 881 + `_Hw.c` 180 +
    `_Target.c` 299 + `Os_Port_Tms570.h` 542 non-test-model portions +
    `Os_Port_TaskBinding.c` ~230): **~1,900 LOC**
  - `sc_os_cfg.c/.h`: **~80 LOC**
  - Total: **~5,500 LOC added** — a >2x increase over the current ~4,150
    LOC SC application. Every added line enters the ASIL D audit scope of
    the kill-relay owner. (The 4,278-LOC `Os_Port_Tms570.c` host model
    compiles only under UNIT_TEST and stays out of the image, but stays in
    the verification argument.)
- Requirements impact: SSR-A-002 must be rewritten (assumption falsified);
  SC WCET/timing analysis redone under preemption (or the OSEK config
  constrained to a single non-preemptive task group to preserve the
  no-preemption argument); TC-SC-026 re-derived; SSR-SC-008 condition (a)
  "main loop complete" re-interpreted as "task sequence complete";
  watchdog must still be fed only from the highest-priority verified task
  (S-OS-40 acceptance criterion in the migration plan).
- Diversity argument weakens: zone ECUs and SC would share one kernel code
  base — a kernel bug becomes a common-cause candidate across the diverse
  pair (DFA update required; HSI 7.9 "OS" row would need rewording).
- Evidence in favor: `97dd2f4` ran on hardware — boot → BIST 7/7 → relay →
  OSEK running → stable heartbeat, with lockstep ESM monitoring enabled;
  the kernel has SC3-level protection features (stack monitoring, timing
  protection, service protection) that a bare loop lacks; one scheduler
  across all 7 ECUs simplifies WdgM/schedule reasoning at system level.
- Ethernet/XCP impact (current in-flight work): the S-UDP-03 Ethernet
  telemetry producer step and the XCP-on-Ethernet polling step would move
  from fixed slots of the RTI loop into OSEK tasks (likely one QM 10 ms
  task group, kept below the safety task's priority). The EMAC driver,
  UDP/IP code, and XCP protocol layer are unaffected — only the call-site
  scheduling changes. The S-UDP-03 closure scenario
  (`scripts/hil/sc_eth_hil_scenario.py`) must be re-run per S-OS-40
  acceptance criteria.

## Comparison summary

| Criterion | A: bare loop | B: OSEK on SC |
|---|---|---|
| Audit surface (ASIL D image) | ~4,150 LOC (as-is) | ~9,650 LOC (+~5,500) |
| SSR-A-002 / timing analysis | valid | must be redone |
| Scheduling diversity vs zones | preserved | weakened (shared kernel; DFA update) |
| Hardware evidence | years of bench soak | one bringup session (97dd2f4) |
| Unified scheduler story | SC exempt | complete (7/7 ECUs) |
| Kernel protection features on SC | none (bare loop) | stack/timing/service protection |
| Ethernet/XCP (S-UDP-03/S-XCP-02) | unchanged | resched into QM tasks; driver untouched; re-run closure scenario |

## Recommendation

Recommend **Option A** for the current vehicle platform generation: the SC
is the last line of defense (kill relay, ASIL D, FTTI 100 ms); its
strongest properties today are smallness, auditability, and scheduling
diversity from the zone ECUs — all three are diluted by Option B, while
Option B's main benefits (unified scheduler, kernel protection features)
accrue mostly to convenience and to features the SC's simplicity already
compensates for. Revisit after Phases 2-3 land and the kernel has
accumulated SIL/HIL soak evidence on the zone ECUs; the harvested port and
`sc_os_cfg.c` remain parked and hardware-proven for that day.

The recommendation is advisory only. Phase 4 (S-OS-40) executes whichever
option is signed below and must not start before sign-off.

## Sign-off

User decision (2026-07-10): **APPROVED — Option B.** Migrate the TMS570
Safety Controller to the OSEK kernel for this experimental platform and
execute S-OS-40. This approval accepts the increased ASIL D audit surface
and the reduced OS-scheduling diversity described above; it does not relax
the SC watchdog, timing, ESM, CAN, telemetry, startup-BIST, relay-gating, or
non-clearable safe-state requirements.

Implementation constraints accepted with the decision:

- Preserve the fixed safety sequence as one highest-priority,
  run-to-completion 10 ms task. No lower-priority or QM task may feed the
  external watchdog.
- Feed the watchdog only after the complete verified sequence, and suppress
  the feed after RAM/self-test, stack-canary, DCAN bus-off, lockstep ESM,
  timing-overrun, or sequence-integrity failure.
- Retain the 2 ms WCET target and 5 ms overrun threshold. Re-verify both with
  OSEK dispatch/interrupt overhead included; approval does not increase
  either limit.
- Treat use of the shared OSEK kernel as a common-cause candidate in the
  dependent-failure argument. Hardware, CPU, compiler, safety-peripheral,
  and toolchain diversity remain; OS-scheduler diversity does not.
- Re-run the S-UDP-03 closure scenario and the existing CAN, XCP, BIST,
  lockstep ESM, relay, watchdog, and retained-fault checks on target before
  claiming S-OS-40 complete.

## S-OS-40 verification deviation (2026-07-10)

The completed non-physical-CAN evidence is **accepted for continued OSEK
adoption by explicit deviation**. This is not full S-OS-40 closure and is not
production CAN, telemetry, or system qualification. Physical CAN continuity,
on-bus SC_Status, CAN-driven S-UDP-03 phases, CAN-dependent XCP continuity,
physical DCAN bus-off injection, and system retained-fault/reset behavior
after CAN faults remain DEFERRED. The lockstep CCM/ESM target self-test is NOT
RUN because the available debugger-detach/reset handoff does not repeatably
execute the documented method. The Option B limits and safety behavior above
are unchanged.

Correction: the Ethernet cable and PHY path are present. The production image
keeps Ethernet disabled, which made the PC port appear disconnected; an
Ethernet bench image raised a 100 Mbps link. Direct SCET reception and XCP were
attempted but produced zero frames and an XCP timeout. The same investigation
found and repaired a one-shot TMS570 termination-switchback defect, with 100+
periodic activations observed afterward. The final-source isolated port suite
now stops in check 2, and the Ethernet-enabled image stops at its first task
dispatch. These are open non-CAN experimental integration blockers. The
deviation remains permission to continue experimentation, not evidence of
full closure or production qualification.
