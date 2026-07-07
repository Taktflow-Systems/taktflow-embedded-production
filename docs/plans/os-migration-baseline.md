# OS Migration Baseline (S-OS-00)

Date: 2026-07-07
Branch: `feat/os-osek-migration`
Branch point: `eb56b0b` ("feat(bench): xcp_smoke broadcast request support"),
tip of `hil-tx-debug-2026-04-21`. Note: the migration plan was drafted against
`814b9f7`; three commits (`3a2fc4f`, `1d4a0de`, `eb56b0b` — SC XCP/bench work,
no OS files) landed between plan drafting and branch creation. Plan findings
remain valid; no `firmware/bsw/os/**` or `firmware/platform/**` OS sources
changed in that window.

Host: Windows 11, Git Bash environment, host `gcc` 15.2.0
(x86_64-posix-seh, MinGW-Builds), GNU Make 4.4.1, Python 3.14.

## Suites that ran on this host

### OSEK kernel + port smoke suites (host gcc)

Command (from repo root):

```
make -f firmware/bsw/os/bootstrap/test/Makefile
```

Result: ALL 11 suites PASS — 242 tests, 0 failures, 0 ignored.

Verbatim runner output tail:

```
PASS  test_Os_asild  (58 Tests 0 Failures 0 Ignored)
PASS  test_Os_Interrupt  (12 Tests 0 Failures 0 Ignored)
PASS  test_Os_MemProt  (17 Tests 0 Failures 0 Ignored)
PASS  test_Os_ProtectionHook  (13 Tests 0 Failures 0 Ignored)
PASS  test_Os_ScheduleTable  (13 Tests 0 Failures 0 Ignored)
PASS  test_Os_ServiceProt  (19 Tests 0 Failures 0 Ignored)
PASS  test_Os_TimingProt  (12 Tests 0 Failures 0 Ignored)
PASS  test_Os_Conformance_Sched  (14 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Tms570_bootstrap  (41 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32_bootstrap  (23 Tests 0 Failures 0 Ignored)
PASS  test_Os_Port_Stm32L5_bootstrap  (20 Tests 0 Failures 0 Ignored)
```

Per-suite raw outputs are written by the runner to
`build/os-tests/test_*.out` (not committed).

## Suites that could NOT run on this host

- `make test` from repo root: there is no root `Makefile`/`GNUmakefile`
  (only `Makefile.pipeline`, which has codegen step targets, no `test`
  target). Running `make test` prints "Nothing to be done for 'test'"
  because GNU make, with no makefile, treats the existing `test/`
  directory as an up-to-date file target. The documented `make test`
  entry point exists only inside the Docker/Linux SIL environment.
  NOT RUN on this Windows host per plan instruction (do not spin up
  Docker for baseline).
- POSIX SIL build (`make -f firmware/platform/posix/Makefile.posix build`)
  and `test/sil/` scenario suite: the POSIX MCAL uses SocketCAN
  (`Can_Posix.c`, vcan) which is Linux-only. Must be captured in
  CI/Docker; recorded as a baseline gap to be closed before S-OS-20
  frame-parity comparisons (which need a Linux candump capture anyway).
- STM32 (`Makefile.stm32`) and TMS570 (`Makefile.tms570`) target builds:
  cross-toolchains not exercised for baseline; these makefiles compile
  no OS sources at HEAD, so the kernel suites above are the relevant OS
  baseline.

## S-OS-30 flash/RAM budget note (appended 2026-07-07, step pulled forward)

STM32 cvc image, arm-none-eabi-gcc 13.3.0, debug (-Og) build:

- Pre-change baseline: text 39480, data 160, bss 7664 (dec 47304).
- With OSEK kernel + port wired (`OSEK=1`): identical 39480/160/7664 —
  `-Wl,--gc-sections` removes the entire kernel because no ECU main calls
  `StartOS` yet (scheduler cutover is a later step by design).
- Potential footprint once referenced (sum of kernel + port object
  sections): text 11471, data 17, bss 2393 (~13.9 KB). Projected flash
  after cutover: ~50.9 KB of the 409.6 KB budget (SYS-054) — ample margin.
- All kernel + port TUs compile clean under the image's `-Werror` flag set.
- The `OSEK=1` image does not LINK yet: single collision, `PendSV_Handler`
  defined by both the harvested `Os_Port_Stm32_Asm.S` and the CubeMX stub
  in `firmware/ecu/cvc/cfg/Core/Src/stm32g4xx_it.c` (hands-off generated
  file). See os-harvest-report.md section 7 for the blocker record.

## S-OS-30 follow-up: PendSV unblock + OSEK=1 link status (appended 2026-07-07)

User decision (2026-07-07): the PendSV collision is resolved by porting
fcf3188's edit of `firmware/ecu/cvc/cfg/Core/Src/stm32g4xx_it.c` (user
explicitly approved hand-editing this one CubeMX-owned file). Applied as a
compile guard, not an unconditional removal: `OSEK=1` now defines
`USE_OSEK` (Makefile.stm32) and the CubeMX `PendSV_Handler` stub is
compiled only when `USE_OSEK` is absent, so PendSV yields to
`Os_Port_Stm32_Asm.S` in OSEK builds while the default build is unchanged.
fcf3188's other two hunks in that file were NOT taken (not needed to
link): the HardFault CFSR/HFSR diagnostic dump (references bringup-only
`Dbg_Uart_Print`/`bringup_put_hex`) and the SysTick OSEK tick wiring
(`Os_PortEnterIsr2`/`Os_Port_Stm32_TickIsr`/`Os_PortExitIsr2` — runtime
wiring that belongs to the Phase 2/3 main cutover, deferred).

Build matrix, arm-none-eabi-gcc 13.3.0, debug (-Og), clean builds:

| Image | Variant | text | data | bss | Result |
|---|---|---|---|---|---|
| cvc | default | 39480 | 160 | 7664 | LINKS (matches pre-change baseline exactly) |
| fzc | default | 37780 | 160 | 7680 | LINKS |
| rzc | default | 36880 | 160 | 7552 | LINKS |
| cvc | OSEK=1 | — | — | — | LINK BLOCKED (new blocker, below) |
| fzc | OSEK=1 | — | — | — | LINK BLOCKED (same symbol) |
| rzc | OSEK=1 | — | — | — | LINK BLOCKED (same symbol) |

NEW BLOCKER (unmasked, not introduced, by the PendSV fix): with
`PendSV_Handler` now supplied by `Os_Port_Stm32_Asm.S`, the vector table
anchors the port/kernel object graph, so `-Wl,--gc-sections` no longer
discards the kernel. The link then fails on

```
Os_TimingProt.c:133: undefined reference to `Os_PortTimingProtElapsedUs'
```

identically for all three images. Root cause: the SC3 timing-protection
port hooks declared in `port/include/Os_Port.h`
(`Os_PortTimingProtArmBudget`/`Disarm`/`ElapsedUs`, and likewise the
mem-prot and interrupt-control hook families) have implementations only
in the POSIX port (`Os_Port_Posix.c`) and in UNIT_TEST host stubs
(gated `!PLATFORM_STM32`). The harvested fix-branch STM32 port never
implemented them because that branch had deleted the SC3 kernel modules
entirely. Providing an STM32 microsecond timebase for timing protection
is new port design (ASIL-relevant), out of scope of the PendSV decision —
recorded here as the next S-OS-30 user-decision item rather than
improvised. Until resolved, `OSEK=1` compiles all TUs clean and fails
only at final link on this hook family.

## S-OS-30 closure: SC3 port hooks implemented, OSEK=1 links (appended 2026-07-07)

User decision (2026-07-07): implement the STM32 SC3 port hooks per the
documented backbone design (plan-osek-sc3-backbone.md) — DWT CYCCNT
microsecond timebase for timing protection, Cortex-M4 MPU (8 regions) for
memory protection, BASEPRI/PRIMASK interrupt-mask hooks. Implemented in
`firmware/platform/stm32/src/Os_Port_Stm32_Sc3.c` (+ contract header
`firmware/platform/stm32/include/Os_Port_Stm32_Sc3.h`), wired into
`Makefile.stm32` under the existing `OSEK=1` gate only. Design decisions:

- Budget timer: TIM7 one-shot (TIM6 is the HAL timebase in these images;
  the backbone Phase-3B implementation note records "STM32: DWT + TIM7 —
  BRINGUP-7 PASS"). Expiry ISR = `TIM7_DAC_IRQHandler` (IRQ 55, weak in
  startup, no CubeMX collision), NVIC priority 0x40 (= bootstrap SysTick).
- Elapsed-us: unsigned 32-bit CYCCNT delta — wrap-safe for intervals below
  one wrap period (~25.2 s @ 170 MHz, per backbone risk register). ARR
  rounds UP (ceil) so the budget timer never fires before the granted
  budget; budgets > ~25.2 s saturate the 16-bit PSC/ARR pair and fire
  early (safe direction).
- DWT discipline: the port only ORs TRCENA/CYCCNTENA and never resets
  CYCCNT; SchM_Timing.c (WCET measurement) zeroes CYCCNT once in
  SchM_TimingInit at boot and never after — both users read wrap-safe
  deltas, so the shared counter is safe.
- MPU: regions 4-7 carry the per-task records (regions 0-3 reserved for
  the static image map, to be programmed at scheduler cutover); RASR
  normal memory TEX=1/C=1/B=1; AP mapping RO=0b010+XN, RW=0b011+XN,
  RX=0b010, RWX=0b011, NONE=0b001+XN; PRIVDEFENA keeps the kernel on the
  privileged background map; fail-closed re-validation (power-of-2, size
  >= 32 B, size-aligned base, count <= 4) leaves rejected slots DISABLED.
- MemManage: fault-handler BODY `Os_Port_Stm32_Sc3_MemManageHandler()` is
  provided; the `MemManage_Handler` vector symbol remains the CubeMX
  stub in `firmware/ecu/cvc/cfg/Core/Src/stm32g4xx_it.c` (hands-off
  generated file; wiring it is a scheduler-cutover item needing the same
  kind of user-approved edit as the PendSV guard). Until then a MemManage
  fault lands in the CubeMX while(1) stub and escalates via watchdog.

Host verification: 3 new suites (test_Os_Port_Stm32_sc3_timingprot 11,
_memprot 14, _intmask 4 tests), full runner 32/32 suites PASS
(488 tests, 0 failures, 3 pre-existing TMS570 ignores).

Build matrix, arm-none-eabi-gcc 13.3.0, debug (-Og), clean builds:

| Image | Variant | text | data | bss | Result |
|---|---|---|---|---|---|
| cvc | default | 39480 | 160 | 7664 | LINKS (byte-identical to baseline) |
| fzc | default | 37780 | 160 | 7680 | LINKS (byte-identical to baseline) |
| rzc | default | 36880 | 160 | 7552 | LINKS (byte-identical to baseline) |
| cvc | OSEK=1 | 44344 | 180 | 9276 | LINKS |
| fzc | OSEK=1 | 42640 | 180 | 9292 | LINKS |
| rzc | OSEK=1 | 41740 | 180 | 9164 | LINKS |

Kernel+port delta (OSEK=1 minus default): cvc +4864 text / +20 data /
+1612 bss; fzc +4860/+20/+1612; rzc +4860/+20/+1612 (~6.3 KB dec each).
Note this is smaller than the ~13.9 KB full-kernel projection because no
ECU main calls StartOS yet — `--gc-sections` keeps only the object graph
anchored by the vector table (PendSV/SysTick/TIM7 paths + SC3 hooks);
the remainder arrives at scheduler cutover.

## S-OS-11 rate-monotonic re-band: sizes + parity note (appended 2026-07-07)

User decision (2026-07-07): the S-OS-11 order-equivalence refusals for
bcm/cvc/fzc/rzc are resolved by rate-monotonic re-banding of runnable
priorities in the model (trace: `docs/plans/memo-rm-reband-trace.md`).
The legacy `Rte_Cfg_<Ecu>.c` tables regenerated under the banded order,
so **the default STM32 images are intentionally NO LONGER byte-identical
to the S-OS-00 baseline images** (runnable table order/priorities
changed; section sizes below are coincidentally unchanged because the
tables have identical entry counts and the new Os_Cfg/Rte_TaskBodies TUs
are dropped by `--gc-sections` in default builds; cvc gains
`RTE_MAX_RUNNABLES` 9 -> 10, a latent undersizing fixed by wiring
`Can_MainFunction_Write` into the sidecar).

Build matrix, arm-none-eabi-gcc 13.3.0, debug (-Og), clean builds:

| Image | Variant | text | data | bss | Result |
|---|---|---|---|---|---|
| cvc | default | 39480 | 160 | 7664 | LINKS (same sizes, NOT byte-identical — re-banded Rte_Cfg) |
| fzc | default | 37780 | 160 | 7680 | LINKS (same sizes, NOT byte-identical) |
| rzc | default | 36880 | 160 | 7552 | LINKS (same sizes, NOT byte-identical) |
| cvc | OSEK=1 | 44480 | 180 | 9316 | LINKS (+136 text / +40 bss vs prior OSEK=1: OS_MAX_SCHEDULE_TABLES 4 -> 5) |
| fzc | OSEK=1 | 42780 | 180 | 9332 | LINKS (+140 text / +40 bss) |
| rzc | OSEK=1 | 41880 | 180 | 9204 | LINKS (+140 text / +40 bss) |

Verification gate before merge (REQUIRED, CI/Docker Linux — cannot run
on this Windows host): Layer-4/6 SIL parity re-run of the full
`test/sil/` suite against the re-banded images:

- `test/sil/test_vsm_fault_transitions.py`
- `test/sil/test_battery_chain.py`
- `test/sil/test_overtemp_hops.py`
- `test/sil/test_scenario_display.py`
- scenario YAMLs `test/sil/scenarios/sil_001..sil_017` via
  `test/sil/run_sil.sh` (incl. `sil_005_watchdog_timeout_cvc.yaml` and
  `sil_009_e2e_corruption.yaml` — WdgM/E2E are the detection nets for
  the intra-tick order change; memo section 4.1)

Host evidence this session: OS kernel suite 32/32 (490 tests, 0
failures, 3 pre-existing TMS570 ignores — includes the two new
schedule-table bound tests); arxmlgen pytest 287 passed / 32 failed
(failure set byte-for-byte identical to the pre-change 32 pre-existing
failures); cvc/fzc/rzc scheduler mirror suites 6/3/4 tests, all PASS
unchanged. Per-ECU full unit suites (`Makefile.posix TARGET=<ecu> test`)
are SocketCAN/Linux-bound -> CI-deferred, same as the S-OS-00 baseline.

## S-OS-22 STM32 main cutover: sizes + parity (appended 2026-07-07)

STM32 mains now boot the OSEK kernel under `OSEK=1` (`USE_OSEK`):
SysTick -> OSEK counter (guarded CubeMX IT-file hunk, fcf3188
provenance), `Os_PortTargetInit` -> `Os_TaskMap_SetTable` ->
`Os_Configure(<ecu>_os_config)` fail-closed -> `StartOS`; the generated
idle task starts the period schedule tables and runs the task-map idle
hook (`Main_Hw_Wfi`). Legacy super-loop remains the default path.

Defect found and fixed during cutover: the Os_TaskMap weak-symbol
override pattern was silently dead — with the weak-empty
`os_task_map_count = 0` defined in the consumer TU, arm-none-eabi-gcc
13.3.0 constant-folded the count at -Og/-O2 and compiled
`Os_TaskMap_RunMappedFunctions` to `bx lr`, so a strong per-ECU table
was never read (and MinGW COFF hosts do not link weak data at all).
Replaced by an explicit registration API `Os_TaskMap_SetTable()`;
regression guard `test_Os_TaskMap` is compiled at -O2.

Build matrix, arm-none-eabi-gcc 13.3.0, debug (-Og), clean builds:

| Image | Variant | text | data | bss | Result |
|---|---|---|---|---|---|
| cvc | default | 39480 | 160 | 7664 | LINKS, .bin BYTE-IDENTICAL to HEAD-clean build |
| fzc | default | 37780 | 160 | 7680 | LINKS, .bin BYTE-IDENTICAL |
| rzc | default | 36880 | 160 | 7552 | LINKS, .bin BYTE-IDENTICAL |
| cvc | OSEK=1 | 48916 | 180 | 10156 | LINKS (StartOS + task map anchor the full kernel/BSW slot graph) |
| fzc | OSEK=1 | 47012 | 180 | 10180 | LINKS |
| rzc | OSEK=1 | 46160 | 180 | 10044 | LINKS |

Byte-identity method: default `.bin` sha256 of the edited tree vs a
clean worktree at the same HEAD (same embedded GIT_HASH) — all three
pairs identical. OSEK=1 flash use ~49.1 KB worst case of the 409.6 KB
budget (SYS-054) — ample margin.

Host evidence this session: OS kernel + port runner 33/33 suites PASS
(496 tests, 0 failures, 3 pre-existing TMS570 ignores — includes the
new 6-test `test_Os_TaskMap` suite); arxmlgen pytest 288 passed / 32
failed (failure set identical to the 32 pre-existing failures; +1 new
passing idle-body contract test); cvc/fzc/rzc scheduler mirror suites
6/3/4 tests all PASS unchanged.

Runtime validation is PENDING — this closure is build-level only:

- S-OS-31 on-target bringup is OPEN and needs the physical bench. Known
  blocking design item recorded there: the production `StartOS` dispatch
  loop direct-calls the autostarted idle task on MSP and never calls
  `Os_PortStartFirstTask`, so PendSV-based preemptive dispatch cannot
  engage; the kernel/port first-task-launch seam plus per-task stack
  binding (`Os_Port_PrepareConfiguredTask` with real stack memory,
  budgets from the generated stack table) must be designed and
  user-approved before on-target runs — same decision-item pattern as
  the earlier PendSV and SC3-hook items.
- Linux CI SIL parity (Layer 4/6 re-run of `test/sil/`, unchanged list
  from the S-OS-11 note above) remains REQUIRED before merge.

## S-OS-31 first-task launch seam: CLOSED at build+host level (appended 2026-07-07)

User decision (2026-07-07): implement the production launch path — on
PLATFORM_STM32 (and identically STM32L5/TMS570 via the shared seam),
StartOS prepares per-task stacks from the configured stack table through
the task-binding seam and launches the first task through
Os_PortStartFirstTask, engaging the hardware-validated PendSV dispatch.
Host-cooperative behavior (POSIX builds, UNIT_TEST suites) unchanged.

What the bringup line did manually vs what StartOS now does:

| Bringup (6/6 PASS on G474RE) | Production StartOS now |
|---|---|
| static aligned(8) stack arrays in the test TU | generated stack arrays in `Os_Cfg_<Ecu>.c` (new `Os_TaskStackConfigType` table, `Os_ConfigType.TaskStacks`) |
| `Os_Port_PrepareConfiguredTask` per task | same call for every configured task, stack top from `os_task_stack_top_cfg` |
| `Os_Port_PrepareConfiguredFirstTask` on the launch task | same, on the highest-priority ready autostart task (`os_select_next_ready_task`) |
| `Os_PortStartFirstTask` (PendSV pops frame, PSP thread mode) | same, after kernel dispatch bookkeeping (RUNNING state, ready bitmap, dispatch count, PreTaskHook) |

Design points:

- `os_cfg_apply_task_stacks` (Os_Core.c) validates each entry: valid
  task, non-NULL 8-byte-aligned base (AAPCS), non-zero size multiple of
  8, monitor budget <= storage size. On the three hardware platforms
  Os_Configure additionally REJECTS (E_OS_VALUE, fail-closed wipe) any
  configuration where a task lacks a stack entry.
- StartOS launch is gated on a configured task-stack table: without one
  (bringup builds, Os_TestConfigure*-driven suites) the pre-existing
  host-cooperative dispatch runs unchanged. If stacks are configured but
  the launch cannot engage, StartOS reports via Det/ErrorHook and parks
  without dispatching anything (watchdog-starvation safe state).
- Stack-corruption lesson respected (stm32-bringup-p3.md): frames are
  written only into the dedicated static arrays, never into the live
  boot (MSP) stack.
- Generator: `fix(arxmlgen)` emits per-task static stack arrays sized by
  the sidecar stack budgets + the task-stack table; all 6 RTE ECUs
  regenerated (`chore(codegen)`), ICU golden updated.

Host evidence: OS kernel + port runner 34/34 suites PASS (508 tests, 0
failures, 3 pre-existing TMS570 ignores — includes the new 7-test
test_Os_Port_Stm32_bootstrap_production_launch suite and 5 new
test_Os_ConfigValidation stack tests); arxmlgen pytest 290 passed / 32
failed (failure set identical to the pre-existing 32, verified against a
clean worktree at the same HEAD; +2 new passing stack-storage tests).

Build matrix, arm-none-eabi-gcc 13.3.0, debug (-Og), clean builds:

| Image | Variant | text | data | bss | Result |
|---|---|---|---|---|---|
| cvc | default | 39480 | 160 | 7664 | LINKS, .bin BYTE-IDENTICAL to clean HEAD build |
| fzc | default | 37780 | 160 | 7680 | LINKS, .bin BYTE-IDENTICAL |
| rzc | default | 36880 | 160 | 7552 | LINKS, .bin BYTE-IDENTICAL |
| cvc | OSEK=1 | 49668 | 180 | 16336 | LINKS (+752 text / +6180 bss vs S-OS-22: launch path + 6x1024 B task stacks) |
| fzc | OSEK=1 | 47752 | 180 | 15328 | LINKS (+740 / +5148: 5 task stacks) |
| rzc | OSEK=1 | 46912 | 180 | 16224 | LINKS (+752 / +6180) |

OSEK=1 flash worst case ~49.9 KB of the 409.6 KB budget (SYS-054).

Known follow-on design item (NOT closed here, next user-decision item in
the PendSV/SC3/launch pattern): the task-termination switchback. On
hardware, a period task that calls TerminateTask() returns into its body
and then to the poisoned frame LR (0xFFFFFFFF -> fault) because nothing
re-stages a PendSV back to the restored (preempted) task. BRINGUP-6
validated the sequence manually (TerminateTask; SelectNextTask(restored);
RequestContextSwitch; spin) — wiring that sequence into the kernel
termination path (Select WITHOUT frame rebuild + request + never-return)
is new kernel behavior needing user approval before S-OS-31 hardware
runs. Until decided, on-target soak beyond first-launch + first
preemption is expected to fault at the first task termination.

S-OS-31 on-bench checklist (physical bench, no probe access this session):

1. Re-run the harvested bringup suite (`OS_BOOTSTRAP_BRINGUP` image,
   `Os_Port_Stm32_BringupAll`) on CVC G474RE — re-confirm 6/6 before any
   production-launch attempt (hardware pass claims are not inherited).
2. Flash order: cvc first (UART2 debug console), then rzc, then fzc.
   `make -f firmware/platform/stm32/Makefile.stm32 TARGET=<ecu> OSEK=1
   flash`.
3. Observation points: (a) StartOS reached (Det trace
   DET_E_DBG_SYSTICK_START precedes it); (b) idle task entered on PSP —
   CONTROL.SPSEL=1 (BRINGUP-2 check) and schedule tables started; (c)
   first SysTick preemption: port state PendSvRequestCount/TaskSwitchCount
   via debugger or XCP; (d) EXPECTED FAULT at first task termination
   until the switchback decision lands — capture CFSR/HFSR + stacked PC
   to confirm the poisoned-LR signature (0xFFFFFFFF).
4. WdgM/E2E remain the detection nets for order/timing drift once the
   switchback lands (S-OS-11 memo section 4.1 list).

## Baseline interpretation

- The 8 kernel suites + 3 port smoke suites (Tms570/Stm32/Stm32L5
  bootstrap) at the branch point are 100% green. Any post-harvest
  (S-OS-01) failure in these suites is a regression introduced by the
  harvest, not pre-existing.
- SIL scenario baseline (frame sets/periods per ECU) is deferred to the
  Linux CI environment; S-OS-20 acceptance requires capturing it there
  before the BCM migration lands.
