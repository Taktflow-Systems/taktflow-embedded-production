# HIL Report — S-OS-31 STM32 OSEK on-target bringup + switchback root-fix soak

Date: 2026-07-09
Branch: `feat/os-osek-migration`
Bench: 3x STM32G474RE (Nucleo-64), CVC / FZC / RZC.
Toolchain: arm-none-eabi-gcc 13.3.0; st-flash / st-util / gdb-multiarch 1.8.0.
Images: `OSEK=1` production images (StartOS engaged, PendSV preemptive dispatch),
built this session (memo-s-os-31 §5a: cvc 50132/180/16336, fzc 48216/180/15328,
rzc 47376/180/16224 text/data/bss). ST-Link serials redacted per
never_commit_private_data (mapping resolved by USB VCP↔SWD pairing:
CVC=COM11, FZC=COM3, RZC=COM10).

## Purpose

Re-verify S-OS-31 on target AFTER the switchback resume-without-rebuild root
fix (FIX-03/04, memo `docs/plans/memo-s-os-31-switchback-resume-defect.md`).
Before the fix, CVC HardFaulted (INVSTATE, CFSR=0x00020000 / HFSR=0x40000000)
at the FIRST `Cvc_5000ms` activation (~5 s) due to a PendSV coalescing race
that resumed a task from a stale/zeroed `SavedPsp` frame. FIX-03 (per-task
`SavedContextValid` resume gate) + FIX-04 (one-shot `SaveSuppressed` on the
terminated task) target that race.

## Method

Per board: flash the `OSEK=1` image (`st-flash --connect-under-reset --reset
write <ecu>.bin 0x08000000`), then attach one clean `st-util` + `gdb-multiarch`
session and run the scheduler at FULL SPEED, breaking only at
`HardFault_Handler` (fault catch) and `Os_Task_<Ecu>_5000ms` (the longest-period
task = the pre-fix fault trigger; one stop per 5000-tick cycle, no per-tick
overhead). At each stop, read `os_port_stm32_state` counters and the fault
registers CFSR (0xE000ED28) / HFSR (0xE000ED2C).

Acceptance signals:
- CFSR == 0 and HFSR == 0 (no HardFault/INVSTATE) throughout.
- `PendSvRequestCount == PendSvCompleteCount` (every requested context switch
  completes — nothing lost to the coalescing race).
- `TickInterruptCount` reaches 60x5000 with no drop-back (no WdgM/watchdog
  reset over the soak — a reset would restart the tick counter).
- preemption counters (`PendSvRequestCount` / `TaskSwitchCount`) grow steadily.

## Results — 6-cycle progressive check (each board)

Every sample below has CFSR=0x00000000, HFSR=0x00000000, and Req==Cplt.

| Board | cyc1 (Tick/Req=Cplt) | cyc3 | cyc6 | 1ms task? | Fault |
|---|---|---|---|---|---|
| CVC | 5001 / 10652 | 15001 / 31954 | 30001 / 63907 | yes (deep nest) | none |
| FZC | 5000 / 1150   | 15000 / 3452  | 30000 / 6905  | no (10ms fastest) | none |
| RZC | 5000 / 10650 | 15001 / 31954 | 30000 / 63905 | yes (deep nest) | none |

The pre-fix CVC HardFault occurred at cycle 1 (first 5000ms activation); all
three boards now pass cycle 1 and beyond with the fault registers clear and
PendSV request/complete balanced.

## Results — 5-minute soak (60 x 5000ms activations = 300000 ticks)

| Board | Tick end | PendSvReq | PendSvCplt | Req==Cplt | TaskSwitch | CFSR | HFSR | Verdict |
|---|---|---|---|---|---|---|---|---|
| CVC | 300001 | 639060 | 639060 | YES | 639060 | 0 | 0 | PASS |
| FZC | 300000 | 69059  | 69059  | YES | 69059  | 0 | 0 | PASS |
| RZC | 300000 | 639059 | 639059 | YES | 639059 | 0 | 0 | PASS |

- `Tick` reaching ~300000 with no drop-back ⇒ **no WdgM/watchdog reset** over
  the 5-minute soak on any board.
- CFSR/HFSR = 0 at soak end ⇒ **no HardFault** (the pre-fix CVC INVSTATE fault
  is resolved on the board that exhibited it).
- Req==Cplt at 639060 (CVC) / 69059 (FZC) / 639059 (RZC) ⇒ every context
  switch completed; the coalescing desync did not strand a PendSV.
- CVC and RZC carry a 1 ms task (deepest nested preemption, the exact fault
  precondition); FZC's fastest is 10 ms (lower switch rate, hence ~69k vs
  ~639k switches).

## Verdict

S-OS-31 switchback HardFault (FIX-03/04) — **RESOLVED on target**. All three
G474RE boards run the OSEK-scheduled production mains through a 5-minute soak
with zero HardFault, zero watchdog reset, and fully balanced PendSV
request/complete counts, including the CVC scenario that deterministically
HardFaulted before the fix.

## Remaining FIX-05 item (NOT covered here)

- CAN traffic parity vs SIL (`candump can0` on the HIL Raspberry Pi) and E2E
  CRC-fault checks at receiving ECUs are NOT exercised in this report — they
  require the HIL CAN bus + peer ECUs / the Pi capture path, which was not
  engaged this session. The three boards were verified standalone (SWD debug
  observation). This gap is tracked for a follow-up HIL run with the full
  bench; it does not affect the HardFault/soak verdict above.

## Notes

- ST-Link session fragility: aggressive repeated `--connect-under-reset` +
  a hard-killed gdb wedged the CVC ST-Link's SWD session mid-run
  (LIBUSB_ERROR_TIMEOUT); it recovered after clearing all handles and a
  ~15 s USB settle. Prefer one clean st-util session per board and a
  full-speed run (break only on a low-frequency task) over per-tick stepping.
- UART (COM11) emitted no periodic lines after a plain `--reset` flash
  (board left halted by st-flash `--reset`); the authoritative run state was
  read via SWD/gdb after a gdb `continue` engaged the scheduler.
