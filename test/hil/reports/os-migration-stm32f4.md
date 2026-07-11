# HIL Report - S-OS-32 STM32F413ZH OSEK bringup

Date: 2026-07-10
Status: BLOCKED at mixed-bench CAN soak

## Scope

This report covers the spare-board S-OS-32 track only. It does not replace
the STM32G4 S-OS-31 evidence or qualify the F413ZH production CAN path.

## Build evidence

- Target: `rzc_f4`, production RZC main and generated RZC OSEK config.
- Command shape: `Makefile.stm32f4 TARGET=rzc_f4 OSEK=1`.
- Compiler policy: project sources use `-Werror`; vendor HAL/CubeMX sources
  retain the existing vendor-warning policy.
- Build ID: `<rzc-osek-build-id>` embedded in production and bringup binaries.
- F413 clock: verified HSE-bypass PLL configuration at 96 MHz; SC3 and
  bringup timing use the explicit 96 MHz override.
- Production size: 46,592 text, 180 data, 16,272 bss bytes.
- Default non-OSEK comparison: 34,020 text, 160 data, 7,432 bss bytes.
- OSEK delta: +12,572 text, +20 data, +8,840 bss bytes.
- Budget: 1,536 KiB flash and 320 KiB RAM; image links within both.
- Host regression: all 37 OS suites passed with zero failures and three
  expected TMS570 ignores.

The production ELF owns the F4 HardFault and SysTick handlers, shared
MemManage/SC3 handler, PendSV assembly handler, and generated RZC task entry.
The bringup ELF contains the six-check hardware harness. Output paths are
isolated below `build/stm32f4/<variant>/`.

## Target resolution

The ignored hardware map initially had no F4 entry. Live probe enumeration
found two unmapped probes but exactly one STM32F413 device and exactly one
VCP associated with it through the USB parent. Only after that association
was unique was the ignored local map updated. No private identifiers are
recorded here.

## Six-check bringup

The mapped F413 target was verified immediately before flash. The isolated
bringup image produced the correct F413 banner and build ID, then reported:

| Check | Result |
|---|---|
| BRINGUP-1 SysTick | PASS |
| BRINGUP-2 first task on PSP | PASS |
| BRINGUP-3 ISR return/context preservation | PASS |
| BRINGUP-4 two-task PendSV switch | PASS |
| BRINGUP-5 ISR-driven preemption | PASS |
| BRINGUP-6 cyclic time slicing | PASS |

Summary: `6/6 ALL PASS`. This is direct F413 evidence, not inherited G474
evidence.

## Production free-running attempt

Method: fresh production-image flash followed by a 300-second free-running
window, F4 UART capture, complete mapped CAN-interface capture, and adapter
counters before/after. No debugger or probe server was attached during the
window. A flashless reset then harvested the retained fault record.

Runtime results:

- UART stayed alive through ECU uptime 315 seconds with exactly one measured
  boot and no fault record or unexplained reset.
- Final status was TEC=0, REC=0, error-state=0; application heartbeat kept
  advancing.
- Hardware state remained 0, which the F4 backend defines as bxCAN INIT/INAK.
- TX-busy count grew to 18,333.
- Complete CAN capture contained zero frames. Adapter RX packet count was
  unchanged for the full window.
- Adapter remained error-active with zero warning, passive, bus-off,
  bus-error, RX-error, and TX-error deltas.
- Flashless retained-record harvest reported `no fault record` and the
  production `<rzc-osek-build-id>` banner.
- E2E CRC behavior is not assessable because no CAN frame reached the
  independent adapter.

The immediate boot diagnostic recorded correct PD0/PD1 AF9 configuration,
`MSR=0x09`, and one occupied TX mailbox. On STM32F413, CAN_MSR.INAK is bit 0
and CAN_MSR.RX is bit 11. Therefore `0x09` means INAK remained set while RX
sampled dominant/low. bxCAN requires 11 recessive bits before acknowledging
exit from initialization; the controller could not enter normal mode.
The existing diagnostic did not record MCR, so this report makes no claim
about a captured MCR value.

## Verdict

Implementation, clean builds, size budget, host regressions, and all six F413
port checks pass. S-OS-32 is not complete: the mixed-bench production soak
cannot meet CAN or E2E acceptance while the physical F4 RX segment remains
dominant/inactive. The ignored map proves probe/VCP identity but does not
establish an active shared transceiver segment. No software retry limit,
controller-state mask, or acceptance threshold was changed to hide this
physical blocker.

At session end the F413 runs the production `<rzc-osek-build-id>` image. The G474 boards
were not flashed during S-OS-32. No debugger, probe server, or capture process
remains attached.

## Disposition

By user direction on 2026-07-10, the dominant F4 CAN RX segment is deferred as
a known physical limitation so subsequent OS-adoption work may continue. This
does not convert the failed mixed-bench attempt into a pass: the unchanged
300-second CAN/E2E acceptance remains unmet and is still required before F413
production CAN qualification. The F413 OSEK implementation, build, budget,
host regression, and 6/6 hardware port-bringup evidence remain valid.
