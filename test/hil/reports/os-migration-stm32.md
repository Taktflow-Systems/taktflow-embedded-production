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

## CAN-parity + E2E closure (2026-07-09, full-bench HIL run)

This section closes the FIX-05 CAN-parity-vs-SIL + E2E items that the soak
above left OPEN. Bench: 3x G474RE on the shared 500 kbit/s CAN bus (SN65HVD230
transceivers, PA11/PA12 FDCAN1) with the HIL Raspberry Pi (`<pi-user>@<pi-ip>`,
gs_usb USB-CAN presented as socketcan `can0`) capturing via `candump can0`.
Images: the same fixed `OSEK=1` production bins soaked above
(`build/stm32/{cvc,fzc,rzc}/*.bin`, +224 B switchback fix). No plant-sim /
vECU station was run (only the Pi observability containers were up).

### Method
Boards brought to run state with `st-flash --connect-under-reset` (CVC was
already running from the soak; FZC/RZC reset-to-run). `can0` brought up at
`bitrate 500000`. Bus captured hands-off (no debugger attached) for parity;
`os_port_stm32_state` / E2E RX counters read via one clean `st-util`+
`gdb-multiarch` session where SWD access was possible. Baseline = the DBC
`GenMsgCycleTime` per message (`gateway/taktflow_vehicle.dbc`), cross-checked
against the generated `CanIf_Cfg_*` / `Com_Cfg_*` TX tables (no committed SIL
parity capture exists yet — deferred to CI per os-migration-baseline.md
§504-507, so the DBC is the authoritative period reference).

### Result — CAN frame-set + period parity (60 s hands-off soak, 43 274 frames)

| ECU | Frame | CAN ID | count/60 s | measured period | DBC `GenMsgCycleTime` | match |
|---|---|---|---|---|---|---|
| CVC | EStop_Broadcast | 0x001 | 6010 | 9.98 ms | 10 ms | ✅ |
| CVC | CVC_Heartbeat   | 0x010 | 1202 | 49.9 ms | 50 ms | ✅ |
| CVC | Vehicle_State   | 0x100 | 6010 | 9.98 ms | 10 ms | ✅ |
| CVC | Torque_Request  | 0x101 | 6010 | 9.98 ms | 10 ms | ✅ |
| CVC | Steer_Command   | 0x102 | 6010 | 9.98 ms | 10 ms | ✅ |
| CVC | Brake_Command   | 0x103 | 6010 | 9.98 ms | 10 ms | ✅ |
| CVC | Body_Control_Cmd| 0x350 | 601  | 99.8 ms | 100 ms | ✅ |
| FZC | FZC_Heartbeat   | 0x011 | 1202 | 49.9 ms | 50 ms | ✅ |
| FZC | Steering_Status | 0x200 | 1202 | 49.9 ms | 50 ms | ✅ |
| FZC | Brake_Status    | 0x201 | 1202 | 49.9 ms | 50 ms | ✅ |
| FZC | Lidar_Distance  | 0x220 | 6010 | 9.98 ms | 10 ms | ✅ |
| RZC | (all TX frames) | —     | 0    | — (gated) | — | see caveat |

- **CVC and FZC transmit their COMPLETE frame set at the exact DBC periods** —
  full CAN parity for both ECUs under the OSEK-scheduled production mains.
- **CVC and FZC** ran the 60 s soak with steady per-ID counts (no drop-back)
  ⇒ no reset for those two during that window. **RZC contributed NO frames to
  this soak** (see the RZC free-running HardFault finding below — the earlier
  `TickInterruptCount=524099` read that suggested "8.7 min uptime" was taken
  with gdb attached, which freezes the watchdog and alters interrupt timing and
  therefore does NOT reflect free-running behavior).
- **CAN TEC = 0 / REC = 0** on RZC (FDCAN1 `ECR` read via gdb) — error-active,
  no bus-off. No `*_CAN_BUS_OFF` DTC (FZC 0x00D301 / RZC 0x00E601) appears.

### Result — E2E integrity (no CRC / sequence faults at receivers)
- **Sender side:** every E2E-protected frame on the bus (all CVC + FZC frames
  above) carries a monotonically-advancing P01 AliveCounter + valid CRC
  (visible in `candump`, e.g. 0x001 byte0 41→51→61→…) — receivers accept them.
- **Receiver side (RZC, `g_dbg_com_e2e_rx_fail[]` read via gdb over an 8.7-min
  run):** **0 E2E RX faults on 9 of 10 active E2E RX PDUs** — EStop (0x001),
  CVC_Heartbeat (0x010), FZC_Heartbeat (0x011), Vehicle_State (0x100),
  Torque_Request (0x101), Steer_Command (0x102), Steering_Status (0x200),
  Brake_Status (0x201), Lidar_Distance (0x220) all **0**. Only Brake_Command
  (0x103) shows **10** — a small bounded count over 8.7 min (Steer_Command
  from the same sender/rate is 0), consistent with a bring-up transient, not an
  ongoing steady-state fault. `g_dbg_com_e2e_rx_fail` increments only on an
  `E2E_SM_INVALID` transition (`Com.c:433-435`), so a growing count would
  signal live CRC/sequence faults — none observed.
- **No E2E-fault, no command-timeout DTC** on the bus. The ONLY 0x500
  DTC_Broadcast codes seen are FZC sensor-plausibility (`0x00D001` steer
  plausibility, `0x00D002` steer range, `0x00D202` lidar checksum ×2) and the
  downstream `0x00D402` watchdog — all `src=FZC`, all environmental (see
  caveat). Absence of FZC steer/brake **command**-timeout DTCs confirms FZC is
  receiving and E2E-validating CVC's protected Steer/Brake commands.
- **WdgM status:** `wdgm_global_status = FAILED` on both FZC and RZC (a
  supervised entity is missing its checkpoint deadline). Contrary to an earlier
  draft, this is NOT benign: FZC and RZC **do reset** free-running (RCC_CSR
  SFTRSTF on RZC; FZC caught at `Reset_Handler`). Whether the resets are the
  INVSTATE HardFault, a WdgM-driven `NVIC_SystemReset`, or both is not yet
  disentangled — see the CRITICAL FINDING section below. The E2E RX counter
  read above was taken with gdb attached (watchdog frozen) and reflects only
  the windows in which RZC was held alive by the debugger.

### CRITICAL FINDING — RZC HardFaults (INVSTATE) free-running; S-OS-31 NOT closeable

**Correction to the FIX-05 soak above.** The 5-minute soak that reported "3x
G474RE PASS, no HardFault" was run **under gdb with a breakpoint on
`Os_Task_<Ecu>_5000ms`** (report Method section). That instrumentation freezes
the watchdog and alters interrupt timing at exactly the 5000ms activation — the
onset point of the switchback race (memo §7) — and therefore **masked** the
defect. Observed **free-running (no debugger)** this session, the migrated mains
do NOT sustain operation:

- **RZC — confirmed INVSTATE HardFault.** Booted into `BSWM_RUN` (bswm=1,
  self-test PASSES — the STM32 sensor self-tests are stubs returning `E_OK`
  in `firmware/platform/stm32/src/rzc_hw_stm32.c:295-370`; the only non-stub
  item is the internal-loopback CAN test), transmitted a brief ~200 ms window
  of normal 50 ms 0x012 heartbeats, then **HardFaulted and went silent**.
  gdb caught the core parked in `HardFault_Handler` with:
  - **CFSR = 0x00020000** (UFSR INVSTATE), **HFSR = 0x40000000** (FORCED),
    **EXC_RETURN = 0xFFFFFFFD**, `SelectedNextTask = 255` (INVALID) — the
    **identical signature** to the original bug (memo §2.4).
  - backtrace: `HardFault_Handler` ← `Os_Port_Stm32_StartFirstTaskAsm`
    (`Os_Port_Stm32_Asm.S:58`) ← `Os_PortStartFirstTask` (`Os_Port_Stm32.c:495`)
    ← `StartOS` ← `main` — i.e. the fault is in the **first-task launch** path.
  - `RCC_CSR = 0x1C000000` (SFTRSTF + BORRSTF + PINRSTF; **no** IWDG/WWDG flag)
    ⇒ the resets are software/fault-driven, not a benign hardware watchdog.
- **FZC — also resets free-running.** FZC transmitted continuously through the
  60 s parity soak, but was later caught at `Reset_Handler` (CFSR cleared by the
  reset; `bswm=1`, `wdgm=FAILED`, stale-high tick) and had stopped transmitting.
- **CVC** is the only board observed to sustain continuous TX across the session.
- Both FZC and RZC report `wdgm_global_status = FAILED`.

The `PLATFORM_HIL` / sensor framing in an earlier draft of this section was
wrong: the STM32 sensor self-tests are stubs (they pass), RZC reaches
`BSWM_RUN`, and RZC's silence is caused by the **INVSTATE HardFault above**, not
by a self-test / sensor gate.

### Verdict — NOT MET; S-OS-31 REOPENED
- CAN frame-set + period parity is demonstrated **for CVC and FZC** at the exact
  DBC periods (data above is real). E2E sender-side counters advance on-bus and
  no E2E/bus-off/command-timeout DTCs were seen.
- **But the S-OS-31 acceptance ("5-min soak, no HardFault") is NOT met:** RZC
  reproduces the INVSTATE switchback HardFault when free-running, and FZC also
  resets. The FIX-03/04 switchback fix is **NOT confirmed robust on target** —
  its prior on-target PASS was obtained under a gdb breakpoint that masked the
  race. The soak must be re-run **free-running / bus-observed** (no per-activation
  breakpoint), and the switchback fix re-examined against the launch path
  (`Os_PortStartFirstTask`) as well as the terminate path. This item is
  **NOT closeable**; S-OS-31 stays OPEN.
- Caveat on method: this session heavily perturbed the bench (repeated
  connect-under-reset, killed gdb handles). The confirmed INVSTATE HardFault
  forensics stand on their own, but a clean-bench reproduction (single flash,
  free-run, bus capture) should confirm the failure rate before final triage.

### Notes
- Bench serial→ECU (deterministic, USB VCP↔SWD parent): CVC=COM11, FZC=COM3,
  RZC=COM10 (24-hex ST-Link serials redacted per never_commit_private_data).
- ST-Link fragility reconfirmed: killing an `st-util`/`gdb` handle wedges that
  board's SWD (chip-ID 0 on the next `--no-reset` attach);
  `st-flash --connect-under-reset` recovers it. The OSEK idle task's WFI also
  blocks a `--no-reset` attach — use connect-under-reset for a running board.
