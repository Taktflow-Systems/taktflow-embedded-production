# Plan: F5 — SC TMS570LC43x Target Port

> **Status**: IN PROGRESS
> **Parent plan**: `plan-firmware-target-porting.md` (Phase F5)
> **Date**: 2026-03-04

---

## Context

SC (Safety Controller) firmware is **SIL-complete** — all modules implemented, ~1,459 tests passing, MISRA clean, CI green. The SIL runs on POSIX/Docker with SocketCAN. What's missing: `sc_hw_tms570.c` (19 extern hardware functions), `Makefile.tms570`, and the HALCoGen init project.

The TMS570LC43x LaunchPad (LAUNCHXL2-570LC43) arrives tomorrow. SC is the hardest remaining firmware work because it's a **different MCU family** (Cortex-R5, not Cortex-M4), different CAN peripheral (DCAN, not FDCAN), and has real safety peripherals (lockstep CPU, PBIST, ESM, STC).

**What "done" looks like**: SC boots on LaunchPad, passes 7-step startup BIST, receives CVC+FZC heartbeats on CAN bus, LEDs show comm status, kill relay controllable, watchdog feeding. Three Nucleos + LaunchPad on one CAN bus.

---

## Status Table

| Step | What | Status |
|------|------|--------|
| 1 | HALCoGen install + project setup | PENDING (user does GUI — board arrived 2026-03-05) |
| 2 | Makefile.tms570 | DONE (2026-03-04) |
| 3 | sc_hw_tms570.c GIO+RTI+system+DCAN+ESM+stubs | DONE (2026-03-04) |
| 4 | Debug UART (SCI) in sc_main.c | DONE (2026-03-04) |
| 5 | sc_cfg.h DCAN bit timing update (SJW) | DONE (2026-03-04) |
| 6 | Self-test stubs + ESM stubs | DONE (part of sc_hw_tms570.c, 2026-03-04) |
| 7 | Build + flash + boot | PENDING (needs HALCoGen + board) |
| 8 | CAN bus integration (3 nodes) | PENDING (needs hardware) |
| 9 | Real self-tests (STC, PBIST, CRC) | PENDING (can defer) |
| 10 | Update plan-firmware-target-porting.md | PENDING |

---

## Files to Create/Modify

| # | File | Action |
|---|------|--------|
| 1 | `firmware/sc/cfg/halcogen/` | NEW — HALCoGen project for TMS570LC43x (manual GUI tool) |
| 2 | `firmware/Makefile.tms570` | NEW — Build system for SC target |
| 3 | `firmware/sc/src/sc_hw_tms570.c` | NEW — All 19 extern hardware functions |
| 4 | `firmware/sc/include/sc_cfg.h` | MODIFY — Add SJW config, update DCAN comments |
| 5 | `firmware/sc/src/sc_main.c` | MODIFY — Add debug UART print (SCI) |
| 6 | `docs/plans/plan-firmware-target-porting.md` | MODIFY — F5 status |

---

## Phase F5.0: HALCoGen Project + Toolchain

**Goal**: HALCoGen project configured, code generated, toolchain verified.

**HALCoGen** (v04.07.01, Windows only, free from TI) generates init code for TMS570:
- `system.c` — PLL, flash wait states, peripheral clock setup
- `gio.c` — GIO port init with pin direction/pull
- `rti.c` — RTI timer init + compare match notification
- `can.c` — DCAN1 init, mailbox config, bit timing
- `esm.c` — ESM group 1 channel config
- `sys_startup.c` + `sys_core.asm` — Cortex-R5 startup, MPU, VIM
- Linker script (`.ld` or `.cmd`)

**Configure in HALCoGen:**

| Peripheral | Settings |
|---|---|
| **Device** | TMS570LC4357 (matches LaunchPad) |
| **PLL** | HCLK — use default from HALCoGen for the LaunchPad crystal |
| **DCAN1** | Enable, 500 kbps, 6 RX mailboxes (IDs: 0x001, 0x010, 0x011, 0x012, 0x100, 0x301) |
| **GIO** | Port A: pins 0-5 output. Port B: pin 1 output |
| **RTI** | Compare 0 = 10ms notification |
| **SCI/LIN** | Enable SCI for debug UART via XDS110 virtual COM |
| **ESM** | Group 1 channel 2 (CPU lockstep compare) |

**Important**: Use **DCAN1** (not DCAN4) — HALCoGen v04.07.01 has a known mailbox bug on DCAN4 (ADR-005).

**DCAN1 mailboxes** (6 RX, each filtered to one CAN ID):

| MB | CAN ID | Message |
|----|--------|---------|
| 1 | 0x001 | E-Stop |
| 2 | 0x010 | CVC Heartbeat |
| 3 | 0x011 | FZC Heartbeat |
| 4 | 0x012 | RZC Heartbeat |
| 5 | 0x100 | Vehicle State |
| 6 | 0x301 | Motor Current |

**Toolchain**: `arm-none-eabi-gcc` already installed — same binary, different `-mcpu=cortex-r5`.

**Flash tool**: TI UniFlash CLI (or CCS). XDS110 debug probe is built into the LaunchPad.

---

## Phase F5.1: Build System (Makefile.tms570)

**Goal**: `make -f Makefile.tms570` produces `build/tms570/sc.elf` and `sc.bin`.

**Key differences from Makefile.stm32:**

| Aspect | STM32 | TMS570 |
|--------|-------|--------|
| CPU | `-mcpu=cortex-m4 -mthumb` | `-mcpu=cortex-r5 -marm` |
| FPU | `-mfpu=fpv4-sp-d16` | `-mfpu=vfpv3-d16` |
| Startup | CubeMX assembly | HALCoGen `sys_core.asm` + `sys_startup.c` |
| Defines | `-DPLATFORM_STM32` | `-DPLATFORM_TMS570` |
| Flash | STM32CubeProgrammer | UniFlash CLI |

**Sources**: HALCoGen generated drivers + SC app sources (`sc_main.c`, `sc_can.c`, `sc_e2e.c`, `sc_heartbeat.c`, `sc_relay.c`, `sc_plausibility.c`, `sc_led.c`, `sc_watchdog.c`, `sc_esm.c`, `sc_selftest.c`, `sc_hw_tms570.c`). NOT `sc_hw_posix.c`.

---

## Phase F5.2: sc_hw_tms570.c — 19 Hardware Functions

### Group A: System + GIO + RTI (9 functions — straightforward)

These call HALCoGen APIs or access registers directly:
- `systemInit()` → already in HALCoGen startup
- `gioInit()` → call HALCoGen `gioInit()`
- `gioSetBit(port,pin,val)` → map port 0→`gioPORTA`, 1→`gioPORTB`, call HALCoGen
- `gioGetBit(port,pin)` → map port, read GIO DIN register
- `gioSetDirection(port,pin,dir)` → set GIO DIR register bit
- `rtiInit()` → call HALCoGen `rtiInit()`
- `rtiStartCounter()` → call HALCoGen `rtiStartCounter(rtiCOUNTER_BLOCK0)`
- `rtiIsTickPending()` → check RTI compare 0 interrupt flag
- `rtiClearTick()` → clear RTI compare 0 flag

### Group C: DCAN1 (3 functions — hardest part)

DCAN1 base address: `0xFFF7DC00` on TMS570LC43x.

```
dcan1_reg_read(offset)              → *(volatile uint32*)(0xFFF7DC00 + offset)
dcan1_reg_write(offset, value)      → *(volatile uint32*)(0xFFF7DC00 + offset) = value
dcan1_get_mailbox_data(mb,data,dlc) → Read DCAN message object RAM
```

DCAN uses **message objects** (not FIFO). Each has: arbitration reg (CAN ID), message control (DLC, NewDat), Data A (bytes 0-3), Data B (bytes 4-7). Must check NewDat bit, read data, clear NewDat.

SC in **silent mode** (TEST register bit 3) — receives but doesn't ACK or transmit. Works on a 3+ node bus where CVC+FZC ACK each other.

**Bit timing**: Must match 500 kbps. Calculate from actual VCLK1 shown in HALCoGen clock tree. Update `sc_cfg.h` if needed. Use SJW≥4 (same lesson as STM32).

### Group D: ESM hardware (3 functions)

ESM base: `0xFFFFF500`. Direct register access:
- `esm_enable_group1_channel(ch)` — set enable register bit
- `esm_clear_flag(grp,ch)` — write flag clear register
- `esm_is_flag_set(grp,ch)` — read flag status register

### Group E: Self-Test (9 functions — stubs first)

**Start with stubs returning TRUE** — same as POSIX. Then implement real tests incrementally:

| Priority | Function | Implementation |
|---|---|---|
| 1st | `hw_dcan_loopback_test()` | DCAN test mode — most useful for bring-up |
| 2nd | `hw_gpio_readback_test()` | Write relay pin, read back — simplest real test |
| 3rd | `hw_lamp_test()` | All LEDs ON then OFF — visual verification |
| Later | `hw_lockstep_bist()` | STC module — complex TMS570 safety test |
| Later | `hw_ram_pbist()` | PBIST module — hardware pattern memory test |
| Later | `hw_flash_crc_check()` | CRC module over flash region |
| Later | `hw_watchdog_test()` | Toggle WDI, verify no reset |
| Runtime | `hw_flash_crc_incremental()` | CRC one flash sector per call |
| Runtime | `hw_dcan_error_check()` | Read DCAN ES register |

---

## Phase F5.3: Debug UART (SCI)

Add debug output via SCI (TMS570 UART) through XDS110 virtual COM port. Same pattern as STM32 USART2:
- Boot banner: `"=== SC Boot (TMS570LC43x) ==="`
- Self-test results
- 5-second heartbeat status print
- Guarded by `#ifdef PLATFORM_TMS570`

---

## Phase F5.4: Verification

### V1: Build
`make -f Makefile.tms570` — zero warnings.

### V2: Boot (LaunchPad alone)
Expected UART output:
```
=== SC Boot (TMS570LC43x) ===
BIST: 7/7 PASS
SC_Relay: energized (MONITORING)
[5s] SC: CVC=TIMEOUT FZC=TIMEOUT RZC=TIMEOUT relay=ON
```

### V3: Three-node CAN (CVC + FZC + SC)
Add SC to bus with SN65HVD230 transceiver (3.3V).
- SC LEDs: CVC=ON, FZC=ON, RZC=BLINK
- Kill relay stays energized
- Stable 5+ minutes

### V4: Kill relay test
Disconnect CVC CAN → SC detects HB timeout → relay de-energizes → kill LED on. Reconnect → stays killed (latch). Power cycle to reset.

---

## Execution Order

| Step | What | Effort |
|------|------|--------|
| 1 | HALCoGen install + project setup | 2h (user does GUI) |
| 2 | Makefile.tms570 | 2h |
| 3 | sc_hw_tms570.c GIO+RTI+system | 2h |
| 4 | Debug UART (SCI) | 1h |
| 5 | sc_cfg.h update | 0.5h |
| 6 | Self-test stubs + ESM | 1h |
| 7 | Build + flash + boot | 1h |
| 8 | CAN bus integration (3 nodes) | 1h |
| 9 | Real self-tests (STC, PBIST, CRC) | 4h (can defer) |
| **Total** | | **~14.5h** |

Steps 1-8 are the critical path. Step 9 (real self-tests) can be deferred — stubs are fine for bring-up demo.

---

## Risk Mitigations

| Risk | Mitigation |
|------|------------|
| HALCoGen version | Use v04.07.01 — DCAN4 bug documented in ADR-005 |
| DCAN bit timing | Calculate from actual VCLK1. SJW≥4 for oscillator tolerance |
| DCAN mailbox vs FDCAN FIFO | Different register layout — message objects not FIFOs |
| UniFlash not finding board | XDS110 needs TI USB driver — install from CCS or standalone |
| SC silent mode = no ACK | Works on 3+ node bus (CVC+FZC provide ACK). Would fail on 2-node |
| STC/PBIST complexity | Start with stubs, implement after CAN works |
