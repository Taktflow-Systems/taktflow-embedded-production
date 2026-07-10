# Plan - RZC STM32G4 FDCAN transmit wedge

Date: 2026-07-10
Status: OPEN - independent defect; blocks the S-OS-31 CAN-parity gate

## 1. Scope and evidence boundary

This plan owns the RZC transmit-path wedge observed after the S-OS-31 FIX-10
scheduler repair. It does not reopen or extend the kernel/port reconciliation.
FIX-10c completed 15 free-running board-runs with zero fault records, zero
silent parks, and zero unexplained resets, yet RZC lost all cyclic transmit
IDs in 5/5 runs while its OS, UART status, and application heartbeat stayed
alive. CVC and FZC continued at their DBC periods for each full capture.

The RZC heartbeat 0x012 produced only 40-54 frames per 305 s capture and was
then absent for a 292.3-294.0 s tail. IDs 0x300-0x303 disappeared with it.
RZC continued to report TEC=0, REC=0, ERR=0, and HAL state 2. The software
`TXbusy` counter was nonzero in two runs and zero in three, so neither bus-off
nor software-queue saturation is yet proven as the cause.

## 2. Suspect path

- `firmware/platform/stm32/src/Can_Hw_STM32.c`: transmit retries
  `HAL_FDCAN_AddMessageToTxFifoQ` and returns `E_NOT_OK` after the bounded
  retry loop.
- `firmware/bsw/mcal/Can/src/Can.c`: failed hardware writes enter a 16-slot
  software queue drained by `Can_MainFunction_Write`; its comment assumes a
  32-deep FDCAN FIFO, while the G4 diagnostic path reports three TX buffers.
- `firmware/platform/stm32/src/rzc_hw_stm32.c`: existing diagnostics expose
  HAL state and decode TXFQS free/full only at boot, before the wedge.

The leading hypotheses are FDCAN TX FIFO/index state that stops advancing,
HAL handle lock/state corruption or re-entrancy, and a software-queue drain
contract that loses the distinction between accepted, queued, and dropped
frames. Register evidence at the first failed enqueue is required before a
repair is selected.

## 3. Work items

### RZC-FDCAN-01 - Capture the first wedge without a debugger - COMPLETE

- Goal: leave a bounded, machine-readable record at the first persistent TX
  failure without changing real-time behavior through breakpoints.
- Inputs: FIX-10c raw UART/CAN evidence; `Can.c`, `Can_Hw_STM32.c`, and the
  FIX-07 retained-record pattern.
- Deliverables: one-shot capture of CCCR, ECR, PSR, IR, TXFQS, HAL state and
  lock, software queue head/tail/high-water mark, failed CAN ID, and return
  path; boot-time report and clear; host tests for one-shot and wrap behavior.
- Acceptance: a free-running reproduction yields the complete record and the
  capture itself does not add per-activation UART or debugger traffic.
- Gate: Layer 1 host tests, then one short Layer 4 reproduction.
- Definition of done: the first failed enqueue is classified as hardware
  full/error, HAL rejection, software queue overflow, or contract loss.

Implementation result (2026-07-10):

- Added a one-shot `.noinit` record committed by a magic/inverse-magic pair.
- The RZC OSEK hardware hook captures CCCR, ECR, PSR, IR, TXFQS, HAL state,
  HAL lock/error, queue head/tail/high-water, failed CAN ID and return path.
- Capture performs no UART output; the next RZC boot prints and clears the
  complete record before the OS starts.
- Direct-enqueue, queue-overflow and queue-drain paths are distinguished.
- Four retained-record host tests and 34 CAN driver tests pass; tests cover
  one-shot behavior, clear/re-arm, sequence wrap and queue-state forwarding.
- A clean RZC OSEK `-Werror` cross-build passes, the strong RZC capture hook
  is linked, and the 64-byte record is present in `.noinit`.
- The short free-running reproduction captured failed ID 0x500 on the direct
  enqueue path with CCCR=0x00001001, ECR TEC=251, PSR.BO=1,
  TXFQS.TFQF=1, HAL state BUSY, and HAL FIFO_FULL. The later UART state of
  TEC=0 was post-recovery state, not the state that caused the wedge.
- Direct SRAM harvest under reset confirmed the retained magic and complete
  64-byte snapshot before boot reporting.

### RZC-FDCAN-02 - Repair the classified transmit contract

- Goal: fix the proven failure at the narrowest MCAL/HAL boundary.
- Inputs: RZC-FDCAN-01 record and a deterministic HAL mock reproducer.
- Deliverables: red-first tests covering the three-buffer full condition,
  queue drain/retry, confirmation ordering, wraparound, and any proven
  preemptive-call interleaving; implementation with bounded execution and no
  frame reordering beyond the documented CAN contract.
- Acceptance: reproducer red on the old path and green on the repair; existing
  CAN/CanIf/Com suites and all STM32 `-Werror` builds green.
- Gate: Layers 1-3. Do not weaken periods, discard required frames, or hide a
  persistent failure by resetting the controller without a safety rationale.
- Definition of done: every accepted or queued frame has one deterministic
  completion/drop outcome and the wedge reproducer cannot strand TX progress.

Implementation result (2026-07-10) - SOFTWARE COMPLETE, HIL BLOCKED:

- Bus-off detection is now side-effect free. CanSM owns STOPPED and the L1/L2
  recovery delay; the successful STOPPED-to-STARTED transition owns hardware
  recovery, so logical and physical controller states cannot diverge.
- STM32G4 recovery performs HAL deinit, RCC FDCAN peripheral reset, normal-mode
  reinit/filter restore, and start. Failed recovery keeps the logical driver
  STOPPED and is retried by CanSM instead of being reported as STARTED.
- The generated RZC 1 ms task now calls `Can_MainFunction_Write` after RX. The
  source-of-truth sidecar and generator policy test prevent regeneration from
  dropping the software TX queue pump again.
- UART diagnostics now expose hardware TX calls, queue high-water, TXFQS,
  pending buffers, COM calls, and per-PDU send counts.
- Targeted verification passes: 34 CAN tests, 4 retained-record tests,
  67 policy/OS generator tests, ARXML dry-run, and clean RZC OSEK `-Werror`
  cross-build.
- The current bench still drives RZC repeatedly to TEC 250-253 and PSR.BO.
  A 30 s run carried 0x012 for only 15.48 s and 0x300/0x301 for 14.87 s;
  CanSM then exhausted recovery against the persistent error. This is not a
  zero-error software FIFO wedge and does not satisfy RZC-FDCAN-03.
- Next gate: isolate the physical/protocol error on the RZC branch (transceiver,
  termination, wiring, clock/bit timing, and analyzer error frames) before a
  five-run software acceptance. Do not weaken CanSM exhaustion limits to hide
  a continuously faulting bus.

### RZC-FDCAN-03 - Free-running HIL closure

- Goal: prove the FDCAN repair independently, then replay the S-OS-31 CAN
  gate.
- Inputs: post-RZC-FDCAN-02 images and the FIX-08 soak tooling.
- Deliverables: five fresh-flash, 300 s free-running runs with RZC UART and
  full-bus capture; retained-record harvest; updated HIL report.
- Acceptance: no fault records, parks, or unexplained resets; RZC 0x012 at
  20 Hz for every full window; 0x300-0x302 at 10 Hz and 0x303 at 1 Hz; CVC,
  FZC, and RZC cyclic frame-set parity with the DBC.
- Gate: Layer 4 bus-observed evidence, with no soak under gdb.
- Definition of done: the independent FDCAN plan closes and S-OS-31 can be
  reconsidered against its unchanged acceptance criteria.
