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
normal-production boot was not proven. The committed
`<pre-fix-production-build-id>`
non-diagnostic image is restored and remains fail-closed.

**Update (2026-07-11, S-OS-41 normal-boot root cause and fix):** the
normal-production-boot blocker is CLOSED. Root cause, with an attribution
correction: ESM group-2 channel 3 is NOT a CCM channel and the earlier
"CCM diagnostic residue" reading is withdrawn. Per SPNS195C Table 6-45
(`hardware/datasheets/combined-ds-sections/ds-30-esm.md`), group-2 ch3 is
the Cortex-R5F core fatal-bus-error event, commonly invalid flash ECC.
`HL_sys_link.cmd` padded no flash section ends, so the ETH production
image's last programmed byte began an otherwise-erased ECC doubleword and
cache line; the first CPU consumption of that word latched `SR2=0x8` with
no abort on EVERY boot of that image, on both reset types — proven by a
power-on-boot forensic read of the parked fail-closed handler snapshots
(`SR2=SSR2=0x00000008`, read via a verified non-resetting DSS `connect()`
with no `reset()`, ESM IOFFHR deliberately never read) and by
layout-dependence across images: the bring-up and non-ETH images with
benign tails booted clean, only the ETH layout latched. The earlier
"retained latch survives all non-power-on resets" behavior was this fresh
per-boot re-latch. Fix (S-OS-41): flash output sections in
`HL_sys_link.cmd` are wrapped in a `GROUP` with `palign(32),
fill = 0x00000000`, `.rodata` is placed explicitly, and a programmed
256-byte `.flashguard` band terminates the image; no flash word reachable
by the CPU carries invalid ECC. Production still never writes
`SR2`/`SSR2`. Evidence: 18/18 host contracts (two new flash-layout
contracts); clean production, ETH-production, and bring-up cross-builds
(text 42,176 / 51,232 / 44,384 bytes; growth vs pre-fix is exactly the
palign padding plus the guard band); maps and ELF program headers show
contiguous fully-programmed flash segments with 32-byte-aligned ends and
the guard band last. On target: a DSLite programming-reset boot of the
fixed ETH image dumped fresh `SR2=0x00000000` and completed 9-module
init, BIST 7/7, and relay energization — the first clean ETH-production
programming-reset boot. A physical power cycle (>=10 s off) then produced
the controlled normal production boot: Ethernet telemetry OK, 9 modules,
BIST 7/7, relay energized into MONITORING; SCET telemetry received 3001
valid frames in 30.0 s at 100.000 Hz with 0 gaps; XCP CONNECT/UNLOCK
succeeded and `os_counter_value` advanced 101 ticks/s. Direct XCP
readback of ESM registers is rejected by the slave's flash/SRAM address
whitelist (`sc_xcp_eth.c`), a deliberate design limit; zero group-2
status at the power-on boot is evidenced structurally (the boot passed
the one-way FIQ unmask without parking, which any group-2 assertion
prevents) plus the nPORRST-clear property. Negative control: reflashing
the preserved pre-fix ETH image freshly latched live `SR2=0x00000008`
and parked fail-closed after Ethernet init; reflashing the fixed image
booted clean again with the retained `SSR2=0x0000000C` shadow preserved
and never written — detection intact, source removed, retained-evidence
semantics honored. NEW OBSERVATION for separate disposition: SCET status
bytes during the post-power-cycle capture showed the SC transitioned to
SAFE_STOP with relay de-energized and fault reason READBACK (two
consecutive relay GPIO readback mismatches) within ~90 s of the power-on
boot, alongside heartbeat faults for all three monitored ECUs (no
CVC/FZC/RZC nodes are on this bench's CAN). Prior SCET evidence never
decoded status bytes, so it is unknown whether this readback kill is new;
it does not affect the boot/ESM verdict but requires bench
relay-feedback investigation before relay steady-state claims. The
CCM/ESM self-test remains NOT RUN and all physical-CAN gates remain
DEFERRED.

**Update (2026-07-11, S-OS-42 relay READBACK disposition):** the observed
READBACK kill is the specified fail-closed response to unavailable relay
readback on this bench, not a source defect. Production energizes GIOA0 and
checks its input value every 10 ms; SWR-SC-012 requires a kill after two
consecutive mismatches. On this LaunchPad no relay/feedback circuit is
connected, and the same package ball is deliberately remuxed from GIOA0 to
the debug-UART transmit function, so a production build cannot obtain valid
relay feedback. The existing `PLATFORM_HIL` bypass documents this bench
limitation but was correctly absent from the production evidence image. A
pre-armed SCET capture around a fresh `flash --run` replay observed the first
post-reset status as SAFE_STOP, relay off, reason READBACK, with no heartbeat
faults and all three ECU-health bits set. After 4.73 s, the three expected
no-CAN heartbeat timeout flags appeared and ECU health became zero, while the
latched reason remained READBACK. This matches the source ordering: READBACK
kills on the second 10 ms safety-task pass, before the 5 s heartbeat startup
grace expires; once killed, trigger evaluation returns without overwriting
the reason. Therefore heartbeat loss did not de-energize the relay first and
READBACK is not secondary recording. No code change is warranted. Relay
steady-state behavior cannot be qualified on this hardware configuration;
physical CAN and external relay-feedback qualification remain DEFERRED.

**Update (2026-07-11, S-OS-43 DCAN1 ECC partial result and blocker):** the
message-RAM ECC source work is implemented and host-green but T2 is NOT
complete. Per SPNU563A 27.4.1, DCAN1 RAM is now hardware-initialized through
`MINITGCR` and `MSINENA[5]` before HALCoGen `canInit()` makes any message-object
access. The bounded sequence records the initial `ECC_CS`, PERR, and ECC_SERR
values in SRAM, clears single/double-bit flags at the DCAN source, acknowledges
only ESM group-1 channel 21 after the source is clean, and never accesses
group-2 SR2/SSR2. A second source check runs after all mailbox writes and
before normal CAN mode; either initialization timeout or source assertion
keeps CAN uninitialized and reports bus-off to the existing fail-closed path.
Host evidence is 19/19 source contracts and 26/26 executable SC CAN tests,
including both failure gates. A fresh Ethernet production image containing
the ECC work (before the internal-loopback implementation) built cleanly,
booted 9 modules and BIST 7/7, and produced 501 valid SCET frames in 5.0 s at
100.002 Hz with no gaps. XCP SRAM readback showed initial and final
`ECC_CS=0x050A0000`, final ESM group-1 status zero, and preserved nonzero
PERR/ECC_SERR historical object codes; the read-only error-code registers
were not written. The required bus-independent target exercise is BLOCKED:
implementing startup internal loopback with a temporary RX object caused BIST
step 4 to fail. A second fresh image using the TRM 27.14.4 hot-self-test mode
(internal plus silent, so no dominant CAN_TX output) and a 100-fold longer
bounded poll failed at the same step. No physical CAN was connected or used,
and no further workaround was attempted. The installed image is the latter
fail-closed diagnostic state; S-OS-44 and S-OS-45 were not started.

**Update (2026-07-11, S-OS-43 DCAN1 loopback blocker closure):** T2 is DONE.
Failure-path instrumentation on a fresh image separated the controller stages:
hot-self-test mode was active (`CTL=0x000000C0`, `TEST=0x00000098`), TX object
7 requested and completed transmission (`TXRQ1 0x40 -> 0`), object 8 was valid,
and IF2 showed received NewDat with DLC 4 (`IF2MCTL=0x00009084`). Timing and the
TMS570 hot-loopback hardware behavior were therefore not the blocker. The
software failure was compound: the custom receive path extracted native
32-bit IF2 words and reversed each four-byte group on BE32, while generated
`canInit()` selected PMD=5 (SECDED disabled) and rewrote objects 1-6 after the
pre-HAL MINIT sequence. When SC re-enabled SECDED, message-handler scanning
encountered stale check bits and asserted both DCAN ECC flags plus ESM group-1
channel 21. The corrected sequence enables SECDED before the first MINIT,
preserves the original power-on diagnostics, repeats full RAM/ECC hardware
initialization immediately after `canInit()`, then installs the SC mailboxes.
IF2 data now uses HALCoGen's proven BE32 lane order; all IF1/IF2 waits propagate
failure to the existing fail-closed CAN-init/BIST gates, and the temporary RX
object must be invalidated successfully. The diagnostic 100-fold loopback wait
extension was removed.

Verification on final source: 22/22 SC contracts, 28/28 SC CAN executable
tests, SC main 7/7, and SC Ethernet 13/13 passed. A fresh ETH production build
in `build/tms570-prod-final-dcan1-loopback` linked cleanly under authored-source
`-Werror` (text 55,776 bytes, data 0, BSS 27,305 bytes). The exact flashed image
booted with clean startup ESM status, initialized 9 modules, passed BIST 7/7,
and energized the relay. Its loopback return is gated on clean final DCAN
`ECC_CS` flags and ESM group-1 channel 21, so the pass is also the post-traffic
ECC acceptance evidence. SCET then delivered 511 valid frames over 5.10 active
seconds at 100.001 Hz, zero gaps, zero missed, zero invalid. No physical CAN was
connected or used; retained PERR/ECC_SERR history was not written; production
still never acknowledges group-2 SR2/SSR2. S-OS-44 and S-OS-45 remain not
started.

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
| DCAN internal target logic | PASS | 28 tests, 0 failures including ECC reinitialization and mailbox-setup failure gates; no physical continuity claimed |
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
| S-OS-41 flash-image ECC layout contracts | PASS (2026-07-11) | 18 host contracts, 0 failures: linker-shape contract (GROUP, palign(32), zero fill, explicit .rodata, .flashguard) and fresh-map layout contract (contiguous flash sections from 0x0, 32-byte-aligned image end, guard present) across production, ETH-production, and bring-up maps |
| Controlled normal production boot (power-on, ETH image) | PASS (2026-07-11, S-OS-41) | physical power cycle >=10 s off; boot completed Ethernet telemetry OK, 9-module init, BIST 7/7, relay energized into MONITORING; SCET 3001 valid frames / 30.0 s at 100.000 Hz, 0 gaps; XCP CONNECT/UNLOCK with `os_counter_value` +101 ticks/s. Zero group-2 status evidenced structurally (boot passed the one-way FIQ unmask, which any group-2 assertion parks) plus the nPORRST-clear property; direct ESM readback over XCP is design-blocked by the flash/SRAM address whitelist |
| S-OS-41 negative control | PASS (2026-07-11) | preserved pre-fix ETH image freshly latched live `SR2=0x00000008` on a programming-reset boot and parked fail-closed after Ethernet init; reflashed fixed image booted clean again with retained `SSR2=0x0000000C` shadow preserved and never written |
| SC steady-state relay/mode after normal boot | OPEN (2026-07-11 observation) | SCET status bytes showed SAFE_STOP, relay de-energized, fault reason READBACK (2 consecutive relay GPIO readback mismatches) within ~90 s of the power-on boot, with heartbeat faults for all three monitored ECUs (none on bench CAN); novelty unknown — prior SCET evidence never decoded status bytes; needs bench relay-feedback investigation |
| SC relay READBACK disposition | DISPOSITIONED (2026-07-11, S-OS-42) | Correct fail-closed response on a LaunchPad with no relay feedback and GIOA0 remuxed to debug UART; fresh replay first showed READBACK with heartbeat flags clear/health `0b111`, then 4.73 s later showed the three expected no-CAN heartbeat flags with READBACK still latched. Heartbeat loss was later, not the original kill. No source fix; relay steady-state qualification remains unavailable on this bench. |
| DCAN1 message-RAM ECC initialization/recovery | PASS (2026-07-11, S-OS-43) | Root-caused to BE32 IF2 payload extraction plus generated `canInit()` rewriting objects with PMD=5 after the first MINIT. Final source enables SECDED before MINIT, reinitializes full RAM/ECC after HAL, preserves power-on diagnostics, and fail-closes IF timeouts. 22 contracts, 28 SC CAN tests, and fresh target ETH image pass; target booted 9 modules, BIST 7/7, then produced 511 SCET frames at 100.001 Hz with zero gaps/invalid. No physical CAN claim; T3/T4 not started. |

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
  `<pre-fix-production-build-id>`, flashed with DSLite `flash --run`; it
  remains fail-closed.
- Task 2 production liveness is not passed. DCAN1 recovery, IRQ-stack work,
  and the CCM/ESM self-test were not started because the ordered normal-boot
  gate remains blocked.
