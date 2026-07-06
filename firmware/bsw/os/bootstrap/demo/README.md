# OSEK Bootstrap Demo Application

This folder contains a larger same-source demo application for the shared OSEK
bootstrap kernel and the three platform ports:

- TMS570
- STM32
- STM32L5 / L552

The quickest runnable target is a host-executable port simulation. It builds
the same task graph four ways: plain host kernel, TMS570 port model, STM32 port
model, and STM32L5 port model.

Run from the repository root:

```sh
make -f firmware/bsw/os/bootstrap/demo/Makefile
```

Expected result:

```text
osek_demo_host: OSEK_DEMO PASS scenarios=6 trace=PRE:LHlAB;NON:NnH;RES:RHr;EVT:WEw;ALM:PP;ISR:IH;
osek_demo_Tms570: OSEK_DEMO PASS scenarios=6 trace=PRE:LHlAB;NON:NnH;RES:RHr;EVT:WEw;ALM:PP;ISR:IH;
osek_demo_Stm32: OSEK_DEMO PASS scenarios=6 trace=PRE:LHlAB;NON:NnH;RES:RHr;EVT:WEw;ALM:PP;ISR:IH;
osek_demo_Stm32L5: OSEK_DEMO PASS scenarios=6 trace=PRE:LHlAB;NON:NnH;RES:RHr;EVT:WEw;ALM:PP;ISR:IH;
```

The demo exercises:

- priority preemption
- FIFO order within equal priority
- non-preemptive dispatch deferral
- priority ceiling resource release
- event wait/set wakeup
- cyclic alarm activation
- Cat2 ISR activation and deferred dispatch

Regression check:

```sh
make -f firmware/bsw/os/bootstrap/test/Makefile
```

This currently runs 11 suites / 242 tests, including the STM32, STM32L5, and
TMS570 bootstrap port suites.

## Physical target builds

The demo also has target harnesses for the connected STM32G474, STM32L5, and
TMS570 paths:

```sh
make -f firmware/bsw/os/bootstrap/demo/Makefile.target stm32g474
make -f firmware/bsw/os/bootstrap/demo/Makefile.target flash-stm32g474 STLINK_SERIAL=<g4-stlink-serial>

make -f firmware/bsw/os/bootstrap/demo/Makefile.target stm32l5
make -f firmware/bsw/os/bootstrap/demo/Makefile.target flash-stm32l5 STLINK_SERIAL=<l5-stlink-serial>
make -f firmware/bsw/os/bootstrap/demo/Makefile.target verify-stm32l5-openocd STLINK_SERIAL=<l5-stlink-serial>

make -f firmware/bsw/os/bootstrap/demo/Makefile.target tms570
make -f firmware/bsw/os/bootstrap/demo/Makefile.target flash-tms570
make -f firmware/bsw/os/bootstrap/demo/Makefile.target verify-tms570-xds
```

The STM32G474 harness flashes with OpenOCD and signals:

- pass: slow blink on PB4 and PA5
- fail: fast blink on PB5

The STM32L5 harness flashes with OpenOCD and exposes debugger-readable verdict
globals. A passing verifier run prints:

```text
STM32L5_READ verdict=0x50415353
STM32L5_READ scenario_count=6
STM32L5_OPENOCD_VERDICT PASS
```

The TMS570 harness flashes with DSLite and signals:

- pass: slow blink on GIOB6
- fail: fast blink on GIOB7

The TMS570 XDS verifier reloads the same ELF through DSS, arms breakpoints on
the pass and fail signal functions, runs the target, and reads the demo result
bytes and trace string from target RAM. A passing run prints:

```text
DSS_READ PC=0x8fb0
DSS_READ os_demo_passed=0x1
DSS_READ os_demo_scenario_count=0x6
DSS_READ os_demo_trace=PRE:LHlAB;NON:NnH;RES:RHr;EVT:WEw;ALM:PP;ISR:IH;
DSS_VERDICT PASS_BREAKPOINT
```

The TMS570 Ethernet connection is not used for this OSEK demo verdict yet; the
demo image does not initialize EMAC or publish a network-readable status.

Use `st-info --probe` to choose the correct ST-Link probe when multiple
ST-Link devices are attached. Do not use an STM32F4 or G4 probe for the L5
flash target, and do not use the L5 probe for the G4 target.

This target harness runs the OSEK demo code on the MCU cores, but it is still a
`UNIT_TEST` hardware harness. It uses dynamic test configuration helpers and
local no-op stubs for timing/memory-protection port hooks that are not
exercised by the demo. It is evidence that the bootstrap kernel task-service
slice runs on these boards, not yet a production static OSEK application.
