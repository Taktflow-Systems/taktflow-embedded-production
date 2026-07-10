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

**Update (2026-07-10, ESM-high follow-up):** the production group-2
visibility/reaction gap is closed in source and exercised on target. VIM ch0
now owns a dedicated FIQ handler both in the generated VIM initialization
table and in the runtime defensive remap. Its first MMIO action drives the
relay output low, then it snapshots retained `SR2`/`SSR2` into debugger-visible
globals and parks without acknowledging either status register. Production
startup, HAL ESM initialization, and group-3 handling no longer clear group-2
status; the boot CCM/ESM dump now prints both registers. A retained group-2
channel-3 source reasserted on target (`SR2=SSR2=0x00000008`): VIM RAM ch0
contained the new handler address and the parked handler snapshots both read
`0x00000008`. The non-diagnostic production image was restored and remains
fail-closed on that uncleared retained fault. This is fault-path evidence, not
the still-NOT-RUN CCM/ESM self-test and not full S-OS-40 closure.

**Update (2026-07-10, controlled-boot recovery):** the isolated bring-up
recovery now executes before the one-way FIQ unmask. It remains compiled only
with `OS_BOOTSTRAP_BRINGUP`; production still has no group-2 clear. UART
captured `SR2=0x00000000, SSR2=0x00000008`, then post-recovery
`SR2=SSR2=IOFFHR=INTREQ0=0`. The same boot completed 9-module init, BIST 7/7,
alarm arming, first safety-task activation, and bring-up checks 1-5; check 6
then entered with group-2 state still zero. A clean production build from
`be9675a1` is installed and has size text 41,895 bytes / BSS 8,858 bytes.
DSLite's programming reset recreated `SSR2=0x00000008` before production
module init, so production correctly parked without acknowledging it. A
physical power cycle with UART capture is now required for the controlled
normal boot; steps 2-5 remain unexecuted in this continuation.

**Update (2026-07-10, full-power-cycle follow-up):** a complete target supply
removal with all target LEDs off was performed after installing a fresh
non-diagnostic production image. The direct Ethernet link returned at
100 Mbps, but a 15-second SCET capture received 0 valid frames and XCP
CONNECT timed out. Production therefore did not reach verified scheduler
liveness. Diagnostic replay separated two reset states: some DSLite resets
recreated a real group-2 channel-3 source (`SR2=SSR2=0x00000008`), while a
stale replay left `SR2=SSR2=IOFFHR=0, INTREQ0=1`. The bring-up-only recovery
now drains VIM ch0 only after SR2/SSR2 verify zero; a retained-state replay
printed `VIM0 drained`, completed module init and BIST, and passed all six
port checks. No production SR2/SSR2 clear was added. An experimental
production VIM drain was investigated and withdrawn uncommitted because the
normal-production boot was not proven. The committed `7be31ce`
non-diagnostic image is restored and remains fail-closed.

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
| SC scheduling/watchdog/ESM contract | PASS | 15 passed, 0 failed, including periodic TMS570 switchback and fail-closed group-2 source contracts |
| SC main executable | PASS | 7 passed, 0 failed |
| SC watchdog executable | PASS | 9 passed, 0 failed |
| Clean production TMS570 build | PASS | 0 warning diagnostics, 0 errors; authored SC/kernel sources use warnings-as-errors |
| Clean isolated TMS570 build | PASS | 0 warning diagnostics, 0 errors |
| Production image size | PASS | final ESM-high image: text 41,895 bytes; data 0 bytes; BSS 8,858 bytes |
| Isolated image size | PASS | text 42,185 bytes; data 0 bytes; BSS 10,542 bytes |
| Production boot and module initialization | PASS | one boot; 9 modules initialized; relay entered MONITORING |
| Startup BIST sequencer | PASS with limitation | sequencer reported 7/7 before relay energization; the current lockstep slot is a target stub and is not independent lockstep-test evidence |
| OSEK periodic alarm/task activation | PASS | a diagnostic production build observed activations 1, 2, 10, and 100 plus the 5-second periodic marker; this exposed and verified the termination-switchback fix |
| TMS570 port checks on final source | PASS (2026-07-10 closure) | 6/6 checks pass with `[BRINGUP-SUMMARY] ALL PASS` after the F-bit fix; check 2 confirms task CPSR `0x600003DF` (System mode, I=1, F=1); check 6 observed 25 IRQs, 56 FIQs, 1 preemption with full R4-R11/SP preservation. Pre-unmask VIM/ESM dumps captured; the retained group-2 latch was reported and drained before the FIQ unmask (bench substitute for the documented power cycle, bring-up image only) |
| Production ESM-high fail-closed path | PASS (fault path) | 15/15 host source contracts; clean production and diagnostic TMS570 builds; disassembly confirms relay DCLR precedes SR2/SSR2 reads; on target VIM ch0 pointed to the dedicated handler and its parked snapshots captured `SR2=SSR2=0x00000008`. No production group-2 clear was performed |
| IRQ-stack static margin | PASS with limitation | `-fstack-usage` build at the current debug `-Og` profile gives a conservative 144-byte maximum for the RTI assembly save + tick/alarm activation + ISR2-exit dispatch chain. The 256-byte IRQ stack therefore has 112 bytes (43.75%) static margin. No runtime stack-paint high-water measurement exists |
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

## Findings from the 2026-07-10 closure

- **Production ESM group-2 gap CLOSED in implementation/fault-path evidence.**
  VIM ch0 is fail-closed from VIM initialization onward, SR2/SSR2 are recorded
  and never acknowledged by production startup or handler paths, and both are
  present in the boot dump. The uncleared channel-3 latch was captured by the
  handler on target. This does not substitute for the CCM/ESM self-test.
- **ESM group-1 channel 21 traced to DCAN1 message-RAM ECC.** The device ESM
  assignment table maps group-1 channel 21 to DCAN1 ECC uncorrectable error;
  DCC1 is channel 30. Read-only target forensics captured
  `ESM_SR1=0x00200000`, DCAN1 `ECC_CS=0x050A0101` (double- and single-bit
  flags set), `PERR=5`, and `ECC_SERR=6`, implicating message objects 5 and 6.
  The source is therefore dispositioned as retained DCAN1 ECC evidence; the
  original triggering access and correct source-level recovery remain open.
- **IRQ-stack static margin quantified.** The current 100 Hz RTI path has a
  conservative 144-byte static maximum: 24-byte assembly exception save plus
  the deepest `-fstack-usage` call chain through tick/alarm activation or
  ISR2-exit dispatch. The 256-byte IRQ stack has 112 bytes (43.75%) remaining.
  Because this is profile-sensitive static evidence and no stack-paint
  high-water exists, resizing to at least 1 KiB plus runtime watermarking
  remains the production-qualification recommendation.
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

### Follow-up bench end state

- The non-diagnostic production image from `build/tms570-esm-high/sc.elf` is
  installed with DSLite `flash --run`.
- Retained group-2 channel 3 remains asserted and uncleared. The controller
  enters the dedicated VIM ch0 handler before normal module completion,
  drives the relay output low, records `SR2=SSR2=0x00000008`, and parks.
- No debugger, serial monitor, or flash process remains attached. Physical
  CAN wiring and bench networking were not changed.

### Controlled-boot continuation end state

- The prior `build/tms570-esm-high/sc.elf` artifact was stale despite its
  timestamp: its boot banner was `56f174eb`, not the handoff's stated
  `be9675a1`. It is not used as current-source evidence.
- A fresh non-diagnostic image was built in a clean directory from
  `be9675a1`; its observed banner and size match the source and recorded
  production budget.
- The fresh image is installed via DSLite `flash --run`. Its programming
  reset recreated `ESM_SSR2=0x00000008`; the production image did not clear
  it and remains fail-closed before module completion.
- No debugger, serial monitor, or flash process remains attached. The next
  target action is a physical power cycle followed by XDS110 Application/User
  UART capture; do not reflash before that observation.

### Full-power-cycle follow-up end state

- Full target power removal did not produce scheduler or XCP evidence: SCET
  was 0 frames in 15 seconds and XCP CONNECT timed out, despite a 100 Mbps
  physical link.
- The isolated recovery's guarded VIM0 drain is target-proven with a nonzero
  retained-state replay and all six bring-up checks passing.
- The current installed image is the non-diagnostic production build from
  `7be31ce`, flashed with DSLite `flash --run`; it remains fail-closed.
- Task 2 production liveness is not passed. DCAN1 recovery, IRQ-stack work,
  and the CCM/ESM self-test were not started because the ordered normal-boot
  gate remains blocked.
