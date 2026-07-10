# TMS570 OSEK Migration Verification (S-OS-40)

Date: 2026-07-10  
Target: Safety Controller (TMS570LC43x)  
Decision: S-OS-02 Option B, experimental platform  
Disposition: **ACCEPTED FOR CONTINUED OSEK ADOPTION BY EXPLICIT DEVIATION**

This disposition is not full S-OS-40 closure and is not production CAN,
telemetry, or system qualification. Physical CAN-dependent acceptance remains
open. The direct Ethernet path was subsequently confirmed physically linked
and exercised, correcting the earlier disconnected-path inference. The lockstep
CCM/ESM self-test remains NOT RUN because the available reset/debug handoff
does not implement the documented method repeatably.

**Update (2026-07-10, blocker closure):** both non-CAN software blockers
(final-source isolated port check-2 stop; Ethernet-enabled first-dispatch
stop) are CLOSED. One shared root cause: the dispatch-commit mode switches
in `Os_Port_Tms570_Asm.S` used `MSR CPSR_c` immediates `0x9F`/`0x92`, whose
clear F bit permanently unmasked FIQ on this NMFI core (MSR can clear F but
never re-set it). The only enabled FIQ channels — VIM ch0/ch1 (ESM
high-level) — are mapped to HALCoGen's `phantomInterrupt`, which returns
without clearing the source, so any latched ESM-high request became an
unrecoverable silent FIQ take-return livelock at the first F-unmask. On this
bench a RETAINED ESM group-2 channel-3 latch (CCM diagnostic residue;
survives every reset short of power-on, and DSLite reflash never
power-cycles) was asserting VIM ch0 the whole time — captured on target as
`INTREQ0=0x00000001, ESMSSR2=0x00000008, IOFFHR=0x24` at bring-up test-6
entry. Fix: mode-switch immediates `0xDF`/`0xD2` (F preserved) in the RTI
handler commit path and the bring-up preemption ISR; a validity guard in
`Os_Port_Tms570_CheckPreemption` (commit requires valid save AND restore
contexts); and `os_dispatch_task` now records the configured task stack top
(not an IRQ-stack local) as the stack-monitor base for ISR-context deferred
dispatches. With the fix, task CPSR reads `0x600003DF` (F=1) on target where
the broken path produced F=0.

## Scope and unchanged limits

The fixed 10 ms SC sequence runs as one highest-priority, run-to-completion
OSEK task. The TPS3823 feed remains the final action and is permitted only
after the complete verified sequence. RAM/self-test failure, stack-canary
failure, DCAN bus-off, ESM assertion, sequence-integrity failure, or execution
beyond the existing 5 ms overrun threshold suppresses the feed. The 2 ms WCET
target and all existing watchdog, timing, ESM, relay, BIST, safe-state, CAN,
and telemetry limits remain unchanged.

## Evidence classification

| Gate | Classification | Evidence or reason |
|---|---|---|
| Full OSEK host runner | PASS | 37 executables; 533 Unity tests; 0 failures; 3 expected ignores |
| SC scheduling/watchdog contract | PASS | 10 passed, 0 failed, including periodic TMS570 switchback source contracts |
| SC main executable | PASS | 7 passed, 0 failed |
| SC watchdog executable | PASS | 9 passed, 0 failed |
| Clean production TMS570 build | PASS | 0 warning diagnostics, 0 errors; authored SC/kernel sources use warnings-as-errors |
| Clean isolated TMS570 build | PASS | 0 warning diagnostics, 0 errors |
| Production image size | PASS | text 40,579 bytes; data 0 bytes; BSS 8,842 bytes |
| Isolated image size | PASS | text 42,185 bytes; data 0 bytes; BSS 10,542 bytes |
| Production boot and module initialization | PASS | one boot; 9 modules initialized; relay entered MONITORING |
| Startup BIST sequencer | PASS with limitation | sequencer reported 7/7 before relay energization; the current lockstep slot is a target stub and is not independent lockstep-test evidence |
| OSEK periodic alarm/task activation | PASS | a diagnostic production build observed activations 1, 2, 10, and 100 plus the 5-second periodic marker; this exposed and verified the termination-switchback fix |
| TMS570 port checks on final source | PASS (2026-07-10 closure) | 6/6 checks pass with `[BRINGUP-SUMMARY] ALL PASS` after the F-bit fix; check 2 confirms task CPSR `0x600003DF` (System mode, I=1, F=1); check 6 observed 25 IRQs, 56 FIQs, 1 preemption with full R4-R11/SP preservation. Pre-unmask VIM/ESM dumps captured; the retained group-2 latch was reported and drained before the FIQ unmask (bench substitute for the documented power cycle, bring-up image only) |
| Fixed safety-sequence order | PASS | executable host mock observed the required order and sequence counter 9 |
| Watchdog ownership and suppression | PASS | executable host injection covered RAM/self-test, stack canary, DCAN bus-off, ESM, sequence integrity, 5,001 us overrun, and the exact 5,000 us passing boundary |
| DCAN internal target logic | PASS | 24 tests, 0 failures using production timing/mailbox constants; no physical continuity claimed |
| ESM internal logic | PASS | 11 executable host tests, 0 failures |
| Ethernet telemetry internal logic | PASS | Ethernet 13/13, telemetry 6/6, UDP 6/6 |
| XCP Ethernet internal logic | PASS | XCP Ethernet 16/16; smoke-tool self-test passed |
| Lockstep CCM/ESM target self-test | NOT RUN | the available debugger-detach/reset attempts did not produce a repeatable execution handoff matching the documented method; no pass is claimed |
| Live direct Ethernet-only target telemetry/XCP | PASS (2026-07-10 closure) | direct PC link at 100 Mbps; SCET telemetry received at 100.000 Hz — 3001 valid frames in 30.0 s, 0 gaps, 0 missed, 0 invalid (`tools/bench/eth_telemetry_rx.py`). XCP-on-UDP: CONNECT, UNLOCK, and two SHORT_UPLOADs of `os_counter_value` 1.0 s apart returned a delta of 101 ticks (kernel counter live at 100 Hz). Operational note: the production image has no ARP responder; the smoke request was subnet-broadcast-addressed (RX dispatch matches UDP port only) and the SC reply is unicast-to-sender. Unicast-addressed requests need an elevated static ARP entry on the bench PC, or the memo's ARP-responder follow-up |
| No unexplained reset or retained fault | PASS for non-CAN production observation | final production observation exceeded the watchdog timeout margin with one boot, zero startup CCM/ESM status, and no unexplained reset or retained fault |
| Physical CAN continuity | DEFERRED | unavailable physical CAN segment |
| On-bus SC_Status verification | DEFERRED | requires physical CAN |
| CAN-driven S-UDP-03 closure phases | DEFERRED | requires physical CAN input |
| CAN-dependent XCP continuity | DEFERRED | requires physical CAN input |
| Physical DCAN bus-off fault injection | DEFERRED | requires physical CAN |
| System retained-fault/reset behavior after CAN faults | DEFERRED | requires physical CAN fault injection |

## Implementation and verification notes

- The production build compiles the OSEK bootstrap kernel, task binding,
  TMS570 hardware/target/assembly port, Det dependency, and production
  configuration. Generated vendor HAL sources retain their established
  warning policy; authored SC and kernel sources are compiled with warnings
  treated as errors.
- Only the safety task owns the watchdog feed. Startup configuration failure
  de-energizes the relay and starves the external watchdog.
- The fixed CAN, heartbeat, plausibility, creep-guard, relay, state,
  monitoring/telemetry, LED, bus-silence, runtime-self-test, stack-canary,
  health-gate, and watchdog ordering is retained.
- Runtime ESM monitoring is enabled. Initial and final production boot
  snapshots contained no asserted CCM/ESM status.
- On-target diagnostics found that the original production port ran the
  safety task only once: `TerminateTask` staged idle switchback but the TMS570
  hardware path never performed it. The fix uses the existing cooperative
  switch from task context, rebuilds terminated TMS570 task frames, and makes
  ISR2 exit the single scheduler/staging owner. A diagnostic production build
  then reached at least 100 activations and the 5-second marker.
- The polling Ethernet driver now explicitly masks unused EMAC pulse
  interrupts. This is necessary with CPU IRQs enabled, but it did not close
  the remaining Ethernet-enabled first-dispatch blocker.
- Host mocks and software injection verify CAN-dependent internal logic only.
  They do not substitute for physical CAN continuity, traffic, or fault
  injection.

## Lockstep self-test limitation

The documented TMS570LC43x CCM-R5 self-test flow was evaluated against the TI
technical reference manual (SPNU563A, CCM-R5 self-test and ESM mapping). A
halted debugger disables the relevant diagnostic behavior, so a passing test
requires debugger-free execution. Isolated attempts using a detach delay and
a controlled software reset did not provide a repeatable, single-shot handoff
with the current XDS-only setup and startup reset-source behavior. Experimental
instrumentation was removed and no threshold or retry was changed. This check
is therefore NOT RUN, not failed or passed; that classification is about
evidence quality for the experimental migration, not a claim of physical
hazard.

## Deviation and open closure

S-OS-40 non-physical evidence is accepted by explicit deviation for continued
OSEK adoption. The deviation does not close the unmet acceptance criteria and
does not qualify production CAN, live telemetry over CAN-driven scenarios, XCP
continuity over CAN, or system-level CAN-fault behavior. All physical
CAN-dependent gates listed above remain open until a functional segment is
available. The final-source isolated port check-2 regression and the
Ethernet-enabled first-dispatch stop are CLOSED (2026-07-10, shared FIQ-unmask
root cause above). The repeatable CCM/ESM execution handoff remains the one
open non-CAN item.

## New findings from the 2026-07-10 closure (open, non-blocking)

- **Production ESM group-2 visibility gap.** VIM ch0/ch1 (ESM high-level,
  hardwired FIQ class, always REQMASK-enabled) are mapped to HALCoGen's
  `phantomInterrupt`, which returns without acknowledging the source.
  `sc_esm.c` monitors group-1 channel 2 only, and the boot CCM/ESM dump
  prints SR1/SR3 but not group-2 SR2/SSR2 — so a latched group-2 error
  (e.g. CCM-R5F) is invisible to both boot evidence and runtime monitoring
  while F=1. Recommended follow-up: a fail-closed ESM-high handler on VIM
  ch0 (de-energize relay, record `SR2/SSR2`, park) and SR2/SSR2 in the boot
  dump. Until then, "no asserted CCM/ESM status" claims from the existing
  boot dump do not cover group 2.
- **ESM group-1 channel 21 latched** (`ESMSR1=0x00200000`) throughout the
  bring-up runs; not high-level-routed (INTREQ0 stayed clear of it) and no
  behavioral effect observed. Raw observation only; channel disposition not
  yet traced.
- **Kernel tick path runs on the 256-byte HALCoGen IRQ stack.** The RTI
  service now nests ~10 C frames (counter tick, alarm expiry, activation,
  frame rebuild) in IRQ mode. No overflow observed, but the margin is
  unquantified; resize (>=1 KB) plus a stack-paint check is the safe
  follow-up.
- **`os_commit_dispatch_live` is never TRUE on TMS570.** The FIX-10 commit
  gate is `PLATFORM_STM32`-only; TMS570 runs the legacy tick-staging route
  (speculative advance + double staging). Consistent and passing today —
  documented here as a deviation from the STM32 dispatch model, to be
  unified or formally accepted before production qualification.

## Bench end state (2026-07-10 closure session)

- The production non-Ethernet OSEK image, rebuilt from the fixed final
  source (`build/tms570-prod/sc.elf`, fresh build directory), is installed
  on the SC; boot observed over UART with one boot, 9 modules, BIST 7/7,
  relay MONITORING, and no fault or reset marker during the observation
  window.
- No debugger, probe server, UART capture, or HIL process is left attached.
- Raw flash and UART evidence remains outside the repository.
- No physical CAN wiring, connection, termination, transceiver, or routing
  change was made. The direct SC-to-PC Ethernet cable is unchanged; with the
  production (non-ETH) image installed the SC PHY is asleep and the PC link
  is down, as before.
- The retained ESM group-2 latch observed during bring-up was drained during
  the instrumented runs (IOFFHR reads plus the bring-up-only reporting
  clear); production images never clear group-2 status.
