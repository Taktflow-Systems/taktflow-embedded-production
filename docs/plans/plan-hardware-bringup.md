# Hardware Bring-Up Plan — Taktflow Zonal Vehicle Platform

> **Status**: Phase 0 PARTIAL — Inventory Confirmed 2026-02-27, gap items pending
> **Created**: 2026-02-23
> **Last Updated**: 2026-03-03
> **Builder Profile**: Solo, SW-strong / HW-beginner
> **Estimated Completion**: ~4 weekends (40-60 hours hands-on)

## Context

All BOM items have been ordered/received. Firmware phases 0-6 are DONE (BSW layer + CVC SWCs). This plan is a structured, step-by-step guide to physically assemble and bring up the 4-ECU hardware platform, then integrate it with firmware. It bridges the gap between "parts on the desk" and "working HIL platform."

This plan is written for someone who is strong on the SW side but less experienced with HW integration. Every step includes what to check and what "good" looks like.

---

## Inventory & Procurement

> **Single source of truth for all procurement status**: [`hardware/bom.md`](../../hardware/bom.md)
>
> Check the **Status** column in `bom.md` before starting any phase. Do NOT duplicate delivery status here.

### Available but Not Used in This Project

| Item | Qty | Notes |
|------|-----|-------|
| ST NUCLEO-L552ZE-Q (Nucleo-144, STM32L552) | 1 | Wrong MCU/form factor — not compatible |
| ST NUCLEO-F413ZH (Nucleo-144, STM32F413) | 1 | Wrong MCU/form factor — not compatible |
| STM32L4R9I-DISCO (Discovery, with display MB1314) | 1 | Not needed for this project |
| X-NUCLEO-NFC04A1 (NFC expansion) | 1 | Not needed |
| Arduino MKR WiFi 1010 | 1 | Not needed — could test Wi-Fi concepts |
| Arduino UNO R3 | 1 | Not needed — could use for quick prototyping |
| Arduino MKR IoT Carrier (relays + SD) | 1 | Not needed |
| Capacitive Soil Moisture Sensor v1.2 | 1 | Not needed |
| PIR motion sensor (HC-SR501 style) | 1 | Not needed |
| Micro servo Tower Pro MG90S | 1 | Too small for project (need MG996R) — could test PWM code |
| CPT C120603 DC/DC converter (18W) | 1 | Could be useful as backup power conversion |

---

## Phase Readiness

> Check [`hardware/bom.md`](../../hardware/bom.md) for current delivery status of all items before starting each phase.

| Phase | Key BOM Items |
|-------|---------------|
| **Phase 0** (inventory + board prep) | BOM #43, #45-48, #49-53, #55, #66-68 |
| **Phase 1** (power distribution) | BOM #34, #35, #36, #37-39 |
| **Phase 2** (mount + USB boot) | BOM #1, #2, #60 |
| **Phase 3** (CAN bus) | BOM #6, #7, #8, #9, #12 |
| **Phase 4** (safety chain) | BOM #2, #22, #23, #24, #25, #26, #27 |
| **Phase 5** (sensors) | BOM #13, #14, #15, #16, #17, #28 |
| **Phase 6** (actuators) | BOM #19, #20, #21, #42 |
| **Phase 7** (integration) | All above |

---

## Reference Files

- **`docs/plans/plan-hardware-bringup-workbook.md`** — **Detailed step-by-step assembly workbook** (all wiring/soldering/verification instructions live here)
- `hardware/bom.md` — **Full BOM with delivery status (single source of truth)**
- `hardware/bom-list.md` — Core 29-item quick reference
- `hardware/procurement-validation.md` — What was actually bought
- `hardware/pin-mapping.md` — All pin assignments (53 pins, 4 ECUs)
- `docs/aspice/hardware-eng/hw-design.md` — Circuit schematics (ASCII), assembly order, power-up checklist
- `docs/aspice/hardware-eng/hw-requirements.md` — 33 HWR specifications
- `docs/safety/requirements/hw-safety-reqs.md` — 25 HSR requirements
- `docs/plans/master-plan.md` — Phase 13 (HIL) definition

---

## Phase Summaries

> **Detailed step-by-step assembly instructions for every phase**: see [`plan-hardware-bringup-workbook.md`](plan-hardware-bringup-workbook.md)
>
> This section provides phase goals, key decisions, and critical notes only. The workbook has the full GATHER/TOOLS/DO/MEASURE/WRONG/FIX steps.

### Phase 0: Gap Closure & Inventory Check

> **Goal**: All parts verified, solder bridges removed, boards labeled. Before you touch a soldering iron.

- Verify all BOM items physically against [`hardware/bom.md`](../../hardware/bom.md)
- **Critical items**: BOM #36 (Schottky), #37-39 (fuses), #43 (100nF caps) — cannot start without these
- **Critical items for Phase 1**: BOM #42 (6V regulators), #45-48 (electrolytic caps), #49-53 (resistors), #55 (Zener diodes)
- Remove solder bridges: FZC SB63/SB65, RZC SB63/SB65, optionally CVC SB21
- After SB63/SB65 removal: ST-LINK VCP lost — use SWO (PB3) or LPUART1 for debug printf
- Label all 4 boards (CVC, FZC, RZC, SC) before mounting

### Phase 1: Base Plate & Power Distribution

> **Goal**: 12V in, 5V/3.3V out, all rails verified. No ECUs connected yet.

- Layout: CVC — FZC — RZC — SC left-to-right, CAN bus along top edge, PSU at left rear
- Power path: PSU → SB560 Schottky → 10A fuse → 12V MAIN RAIL → buck converters → 5V/3.3V
- Star ground point: single screw terminal block, all ground returns converge here
- **Pass criteria**: 12V rail 11.5-12.0V, 5V rail 4.9-5.1V, 3.3V rail 3.2-3.4V

### Phase 2: Mount ECU Boards & Individual Power-Up

> **Goal**: Each board boots independently via USB, then from 12V. No inter-board wiring.

- Mount on M3 nylon standoffs (10mm)
- USB boot test each board individually, then wire 12V to Nucleo VIN (CN7-24)
- SC stays on USB power (independent of kill relay — safety requirement)
- **Pass criteria**: All 4 boards boot, Nucleo 3V3 output reads 3.2-3.4V

### Phase 3: CAN Bus Wiring

> **Goal**: All 4 ECUs + Pi see each other on a 500 kbps CAN bus.

- **Note (2026-02-27)**: 4× TJA1051 + 1× SN65HVD230 on hand. TJA1051 for all STM32 nodes is simplest.
- All STM32 ECUs: TXD → PA12 (CN10-12), RXD → PA11 (CN10-14), STB → GND
- SC: TXD → DCAN1TX (J5), RXD → DCAN1RX (J5), Rs → GND
- Trunk: 22AWG twisted pair (yellow=CAN_H, green=CAN_L), stubs < 100mm
- 120R termination at CVC end and SC end only (60 ohm total)
- **Pass criteria**: `candump` shows frames from all 4 ECUs, termination = 60 ohm, rest voltage ~2.5V

### Phase 4: Safety Chain

> **Goal**: Kill relay works, E-stop works, watchdogs reset MCUs. MUST work before any actuator.

- Kill relay: IRLZ44N MOSFET (10k pull-down = fail-safe OFF) → 12V relay → 30A fuse → actuator rail
- E-stop: NC button → RC debounce (10k + 100nF) → CVC PC13 (internal pull-up)
- Watchdogs: TPS3823DBVR (SOT-23-5) on each ECU — 100nF on CT sets ~1.6s timeout
- **Critical**: RZC watchdog uses **PB4** (not PB0 — PB0 is motor R_EN)
- **Pass criteria**: Relay fail-safe on SC power loss, E-stop fail-safe on wire break, watchdog resets MCU on hang

### Phase 4.6: ADC Zener Protection (Do Before Phase 5)

> **Goal**: Protect MCU analog inputs from overvoltage before connecting any sensor.

- BZX84C3V3 Zener + 100R series resistor on each ADC channel
- **Minimum**: RZC PA0 (current), PA1 (motor temp), PA2 (board temp), PA3 (battery V)
- **Pass criteria**: 5V applied through 100R → junction reads ≤3.6V (clamping)

### Phase 5: Sensors

> **Goal**: Each sensor reads valid data through the MCU.

- AS5048A angle sensors: CVC SPI1 (2× pedal, CS=PA4/PA15), FZC SPI2 (1× steering, CS=PB12). Mode 1 (CPOL=0, CPHA=1).
- TFMini-S lidar: FZC USART2 (PA2/PA3, 115200 baud). Requires SB63/SB65 removed.
- ACS723 current: RZC PA0. 0A = ~1.65V (ADC ~2048).
- NTC thermistors: RZC PA1/PA2. 25°C = ~1.65V.
- Battery voltage divider: RZC PA3. 12V → 2.11V (47k/10k).
- OLED: CVC I2C1 (PB8/PB9, 4.7k pull-ups). Address 0x3C.

### Phase 6: Actuators

> **Goal**: Motor spins, servos move. Only after Phase 4 is verified!

- 6V regulator: 12V actuator rail (kill-relay gated) → LM7806 → 470uF → servo bus
- Steering servo: FZC PA0 (TIM2_CH1), 6V, 3A fuse
- Brake servo: FZC PA1 (TIM2_CH2), 6V, 3A fuse
- Motor: RZC BTS7960 (RPWM=PA8, LPWM=PA9, R_EN=PB0 + 10k pull-down, L_EN=PB1 + 10k pull-down)
- **Critical**: 10k pull-downs on R_EN/L_EN prevent unintended motor operation during MCU reset

### Phase 7: Full Integration & HW-SW Handoff

> **Goal**: End-to-end pedal-to-motor working. Firmware reads all sensors, controls all actuators over CAN.

- Flash production firmware on all 4 ECUs
- 10-point integration test: heartbeats, pedal→motor, lidar, safety timeout, E-stop, watchdog, fault LEDs, OLED, Pi gateway
- HSR open items to measure: relay dropout time (HSR-O-001), ACS723 sensitivity (HSR-O-002), TPS3823 POR pulse (HSR-O-005)
- 30-minute endurance test: zero CAN errors, zero watchdog resets, stable temps and voltages

---

## Phase Summary & Dependencies

```
Phase 0: Gap Closure          ← DO FIRST (order missing parts, prep boards)
    ↓
Phase 1: Power Distribution   ← Verify all voltages before anything else
    ↓
Phase 2: Mount & USB Boot     ← Confirm all boards are alive
    ↓
Phase 3: CAN Bus              ← Communication backbone
    ↓
Phase 4: Safety Chain          ← MUST pass before Phase 6 (actuators)
    ↓
Phase 4.6: ADC Zener Protection ← Install before ANY sensor (needs BZX84C3V3, BOM #55)
    ↓
Phase 5: Sensors              ← Can run in parallel with Phase 4
    ↓
Phase 6: Actuators            ← Only after Phase 4 passes!
    ↓
Phase 7: Full Integration     ← Depends on firmware phases 7-9 completion
```

## Estimated Time

### Experienced Builder

| Phase | Time | Notes |
|-------|------|-------|
| 0 | 1-3 days | Depends on shipping for gap items |
| 1 | 2 hours | Power wiring and verification |
| 2 | 1 hour | Mount boards, USB test |
| 3 | 3 hours | CAN wiring + transceiver soldering + verification |
| 4 | 3 hours | Kill relay, E-stop, watchdogs |
| 5 | 3 hours | All sensors wired and verified |
| 6 | 2 hours | Servos + motor driver |
| 7 | 4 hours | Full integration testing |
| **Total** | **~2-3 days hands-on** | Plus gap closure wait time |

### Solo Builder — Little/No HW Experience (Realistic Estimates)

| Phase | Time | Difficulty | Where You'll Get Stuck |
|-------|------|-----------|----------------------|
| 0 | 1-3 days + 1 evening | Easy | Parts shipping wait. Solder bridge removal is your first soldering task — practice on scrap first |
| 1 | 4-6 hours | Easy-Medium | First time wiring power distribution, double-checking everything with multimeter, re-doing connections |
| 2 | 1-2 hours | Easy | Straightforward USB plug-and-check. Easiest phase — quick confidence win |
| 3 | 8-12 hours | **Hard** | **Hardest phase.** Soldering 4 CAN transceivers to perfboard, running the bus trunk, debugging when candump shows nothing |
| 4 | 6-10 hours | **Hard** | **Second hardest.** Soldering TPS3823 (tiny SOT-23) onto breakout boards. Kill relay MOSFET circuit. Lots of "why isn't this working" |
| 5 | 4-8 hours | Medium | SPI angle sensors and UART lidar are fiddly (mode/baud mismatch). ADC/NTC/voltage dividers are straightforward |
| 6 | 3-4 hours | Easy-Medium | Easy IF Phase 4 works. Just connecting servos and motor driver to tested infrastructure |
| 7 | 6-10 hours | Medium-Hard | Integration bugs. CAN timing issues. Firmware-hardware mismatch debugging |
| **Total** | **~40-60 hours hands-on** | | **~4 weekends** plus gap closure wait |

### Recommended Weekend Schedule

| Weekend | Phases | Goal | What "Done" Looks Like |
|---------|--------|------|----------------------|
| **Weekend 1** | 0 (finish) + 1 + 2 | Power rails verified, all boards boot | Multimeter shows 12V/5V/3.3V correct. All 4 boards blink via USB. Quick win — builds confidence |
| **Weekend 2** | 3 | CAN bus working | `candump` on PC shows heartbeat frames from at least 1 ECU. Then 2, then all 4 |
| **Weekend 3** | 4 + 5 | Safety chain + sensors | Kill relay clicks on command, E-stop cuts power, watchdog resets MCU. Sensors return plausible data |
| **Weekend 4** | 6 + 7 | Actuators + full integration | Motor spins from pedal input. End-to-end pedal→CAN→motor working. 30-min endurance pass |

### The 3 Hardest Parts (Where Beginners Get Stuck)

#### 1. Soldering TPS3823 onto SOT-23 Breakout (Phase 4)
The IC pins are ~0.95mm apart. Your first attempt will probably bridge pins.
- Watch 2-3 YouTube videos on "drag soldering SOT-23" before attempting
- Use flux generously — flux is the difference between success and bridged pins
- Have solder wick ready for cleanup
- Practice on a spare breakout board first (you have 6 spares)

#### 2. CAN Bus Debugging (Phase 3)
When `candump` shows nothing, the possible causes are: TX/RX swapped, termination wrong, bit timing mismatch, transceiver wired wrong, transceiver not powered, stub too long. You'll be checking 6 things before finding the one wrong one.
- Get ONE node working first (CVC → PC via CANable). Don't wire all 4 at once
- Verify 60 ohm termination with multimeter FIRST (power off)
- Check CAN_H and CAN_L rest voltage (both ~2.5V) before expecting traffic

#### 3. Knowing What "Wrong" Looks Like (All Phases)
Experienced people glance at an oscilloscope trace and say "that's ringing" or "that's ground bounce." You'll need to learn what good signals look like.
- Screenshot/photo every "good" measurement as you go
- When something breaks later, you have a reference to compare against
- The verification tables in each phase tell you what "good" numbers are — trust them

### Risk Assessment (Solo Beginner)

| Scenario | Likelihood | Impact | Recovery |
|----------|-----------|--------|----------|
| Bad solder bridge removal on Nucleo SB63/65 | Medium | Low | Fixable with solder wick + patience + flux |
| Burn out a Nucleo (wrong voltage) | Low (if fuse + Schottky steps followed) | Medium | ~€15 replacement |
| CAN bus won't communicate on first attempt | **High** | Low | Systematic debug: termination → wiring → transceiver → firmware bit timing |
| TPS3823 SOT-23 soldering failure | Medium-High | Low | You have 6 spare breakout boards. Just try again |
| Motor/servo smoke | Very Low | Medium | Plan enforces Phase 4 (safety chain) before Phase 6 (actuators) — by design |
| Complete stuck, can't debug | Low-Medium | High | Post on EEVblog forum or r/AskElectronics with photos — community is helpful |

### Essential Tips for HW Beginners

1. **Get a helping hands / PCB holder** (~€10) — soldering while holding the board + wire + iron with 2 hands doesn't work
2. **One change at a time** — wire one thing, test it, then wire the next. Never wire 5 things and then wonder which one is wrong
3. **Take photos before and after** every wiring step — invaluable for debugging and for documenting HSR open items
4. **Label every wire** with masking tape — "12V MAIN", "GND", "CAN_H", "CVC_WDI". Your future self will thank you
5. **Flux is your best friend** — apply it before every solder joint, especially on SOT-23 and perfboard work
6. **When stuck for 30+ minutes on the same problem**: stop, take a photo, describe the problem in writing. Often the act of writing it down reveals the issue
7. **Keep the plan open on a second screen** — check off each verification step as you go

## Safety Rules (Follow These Always)

### General Rules

1. **Never connect actuators (motor, servos) until the kill relay is tested and working**
2. **Always check 12V polarity before connecting a new board**
3. **Always use fuses** — a short circuit without a fuse = fire risk
4. **Start motor at low PWM duty (10%)** — don't jump to 100%
5. **Keep the E-stop within arm's reach** at all times during actuator testing
6. **Disconnect USB from PC before measuring high-current paths** with oscilloscope — ground loop risk
7. **One change at a time** — wire one thing, test it, then wire the next

### Fault Injection & HIL Testing Rules (added 2026-03-01)

> Cross-reference: Full safety cautions in `gap-analysis-hil-bench-vs-professional.md` Section 10.

8. **No 12V (VBAT) fault paths on MCU signal pins** — 12V on a 3.3V GPIO/ADC kills the STM32 instantly. Only short-to-GND faults on MCU-facing channels. Short-to-VBAT only on actuator power lines (motor, relay coil, servo power rail).
9. **100R current-limiting resistor on every FIU relay channel** connected to an MCU pin — limits short-circuit current to 33mA at 3.3V.
10. **Verify voltage with oscilloscope before triggering any fault** — probe the wire, confirm it matches expectation (3.3V signal, not 12V power), only then trigger the fault from the test script. The oscilloscope does not protect anything — it lets you see miswiring before it destroys hardware.
11. **Hard timeout on every destructive test** — motor at >80% PWM: max 5s, electronic load >5A: max 10s, short-to-GND: max 2s. FIU Arduino must auto-restore all relays after 5s serial timeout.
12. **Start every test at minimum stress** — electronic load: start at 1A; DAC: start at mid-range (1.65V); motor PWM: start at 10%.
13. **Never hot-swap FIU wiring** — power off entire bench before connecting/disconnecting any FIU harness wire.
14. **Install BZX84C3V3 Zener clamps on all ADC pins** (Phase 4.6) before connecting any sensor — prevents accidental overvoltage from miswiring.
