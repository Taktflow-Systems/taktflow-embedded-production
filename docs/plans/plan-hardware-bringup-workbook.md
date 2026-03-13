# Hardware Assembly Workbook — Taktflow Zonal Vehicle Platform

> **Purpose**: Step-by-step physical assembly instructions for a HW-beginner
> **Created**: 2026-02-23
> **Last Updated**: 2026-03-03
> **Convention**: Every action is atomic — pick up, measure, wire, verify. Nothing is assumed.
>
> **See also**: [`plan-hardware-bringup.md`](plan-hardware-bringup.md) for project overview, time estimates, risk assessment, phase dependencies, and safety rules. This workbook covers execution only.

---

## How to Use This Workbook

1. **Open the high-level plan** (`plan-hardware-bringup.md`) on a second screen to track phase status.
2. **Follow this workbook step by step**. Do NOT skip ahead.
3. **Check off each verification** before moving to the next sub-step.
4. **Take a photo** after each major wiring step — your future self will thank you.
5. **If a measurement is wrong**, STOP. Debug before continuing. The verification tables tell you what "good" looks like.

### Icons Used

- **GATHER** = collect these parts before starting
- **TOOLS** = have these on the desk
- **DO** = physical action
- **MEASURE** = take a measurement and compare to expected value
- **WRONG** = what a common mistake looks like
- **FIX** = how to recover from the mistake

---

## Your Workspace Setup

Before you start any phase, set up your workspace:

1. **Desk space**: Clear at least 80cm x 60cm. You need room for the base plate, parts, and tools.
2. **ESD mat** (optional but recommended): Place under your work area. Touch the mat before handling boards.
3. **Good lighting**: You'll be reading tiny markings on ICs and resistors.
4. **Second screen or tablet**: Keep this workbook open. Check off steps as you go.

### Tool Checklist

Have ALL of these before starting:

| Tool | Why You Need It | Phase First Used |
|------|----------------|------------------|
| Soldering iron (adjustable, 300-400C) | Solder bridges, perfboard, TPS3823 | 0 |
| Solder (0.5-0.8mm, 60/40 or lead-free) | All soldering | 0 |
| Flux pen or paste | Makes solder flow. Essential for SOT-23 work | 0 |
| Solder wick (desoldering braid) | Removing solder bridges, fixing mistakes | 0 |
| Helping hands / PCB holder | Holds board + wire while you solder | 0 |
| Multimeter (continuity, DC voltage, resistance) | Every verification step | 0 |
| Wire strippers (16-22 AWG) | Cutting and stripping all wires | 1 |
| Small Phillips screwdriver | Screw terminals | 1 |
| Small flat screwdriver | Some screw terminals | 1 |
| USB cables: 3x micro-USB + 1x micro-USB | Power/program each board | 2 |
| Masking tape + permanent marker | Labeling everything | 0 |
| Oscilloscope (strongly recommended) | CAN waveforms, PWM, timing | 3+ |
| Tweezers (fine tip) | Placing small components | 4 |

---

## Component Reference Quick-Card

Clip this or keep it visible. These are the most-referenced BOM items.

| Short Name | Full Name | BOM # | Qty | What It Looks Like |
|------------|-----------|-------|-----|--------------------|
| Nucleo | STM32G474RE Nucleo-64 | 1 | 3 | Blue PCB, ~70x50mm, micro-USB |
| LaunchPad | LAUNCHXL2-570LC43 | 2 | 1 | Red PCB, larger, micro-USB |
| TJA1051 | TJA1051T/3 CAN transceiver module | 6 | 3 | Small breakout board, 8 pins |
| SN65HVD230 | SN65HVD230 CAN transceiver module | 7 | 1 | Small breakout board |
| AS5048A | AS5048A magnetic angle sensor | 13 | 3 | Small breakout board with chip |
| TFMini-S | TFMini-S lidar sensor | 15 | 1 | Small black box, 4-wire cable |
| ACS723 | ACS723 current sensor module | 16 | 1 | Small breakout board |
| TPS3823 | TPS3823DBVR watchdog IC | 22 | 4 | Tiny SOT-23-5 (5 pins, ~3mm) |
| SOT-23 board | SOT-23-5 breakout adapter | 23 | 4 | Tiny PCB adapter for TPS3823 |
| Relay | 12V 30A SPST-NO relay | 24 | 1 | Cube, ~30x30x25mm, 5 pins |
| IRLZ44N | N-channel logic-level MOSFET | 25 | 1 | TO-220 package, 3 pins, tab |
| BTS7960 | BTS7960 H-bridge motor driver | 20 | 1 | Red PCB module, screw terminals |
| LM2596 | LM2596 buck converter module | 35 | 2 | Small PCB with potentiometer |
| SB560 | SB560 Schottky diode (5A/60V) | 36 | 1 | DO-201AD, black cylinder, stripe |
| LM7806 | 6V voltage regulator module | 42 | 2 | TO-220 or module |
| 100nF cap | 100nF (0.1uF) ceramic capacitor | 43 | 30 | Tiny disc or rectangle, marked "104" |
| 10k resistor | 10k ohm 1/4W metal film | 49 | 20 | Brown-black-orange-red-brown bands |
| 330R resistor | 330 ohm 1/4W | 51 | 10 | Orange-orange-brown bands |

---

# WEEKEND 1: Inventory, Power, and Board Mounting

---

## Step 0: Inventory & Board Prep

### 0.1 Lay Out and Verify All Components

**GATHER**: Every box/bag you received from your orders.

**TOOLS**: Table space, [`hardware/bom.md`](../../hardware/bom.md) (printed or on screen), pen to check off items.

**DO**:
1. Open every package and lay components out on the desk, grouped by category.
2. Walk through **every item in [`hardware/bom.md`](../../hardware/bom.md)**. For each one with status **Delivered**, physically find it and place it in the correct group pile.
3. If any item marked Delivered in `bom.md` is actually missing, update `bom.md` status and stop.

**STOP CONDITION**: Do not proceed with soldering if BOM #36 (Schottky diode), #37-39 (fuses), or #43 (100nF caps) are missing. These protect your boards from the first power-up.

---

### 0.2 Label All 4 Boards

**GATHER**: 3 Nucleo-64 boards, 1 LaunchPad, masking tape, marker.

**TOOLS**: Masking tape, permanent marker.

**DO**:
1. Tear off a 3cm strip of masking tape. Write **CVC** in large letters. Stick it on the TOP of the first Nucleo board (on the white silkscreen area, not covering any pins).
2. Repeat for the second Nucleo: label it **FZC**.
3. Repeat for the third Nucleo: label it **RZC**.
4. Label the LaunchPad: **SC**.

**WRONG**: If you skip labeling, you WILL wire to the wrong board later (all 3 Nucleos look identical). This is the #1 beginner mistake in multi-board projects.

---

### 0.3 Solder Bridge Removal (FZC, RZC, and optionally CVC)

> This is your first soldering task. If you've never soldered before, watch 2-3 YouTube videos on "desoldering solder bridges on Nucleo" before starting. Practice on scrap first.

#### 0.3.1 Remove SB63 on FZC Nucleo

**GATHER**: FZC Nucleo board.

**TOOLS**: Soldering iron (set to 350C), flux pen, solder wick, magnifying glass/loupe, multimeter.

**DO**:
1. Flip the FZC Nucleo board over (bottom side up). Look at the silkscreen for "SB63". It's a tiny solder blob connecting two pads near the ST-LINK section.
2. Apply flux generously on the solder bridge with the flux pen.
3. Place the solder wick flat on top of the solder bridge.
4. Press the hot soldering iron tip on top of the solder wick, directly over the bridge.
5. Hold for 2-3 seconds. The solder will wick up into the braid. You'll see the braid darken as it absorbs solder.
6. Lift the iron AND the wick together (don't drag — it can tear pads).
7. Cut off the used section of solder wick.

**MEASURE**: Set multimeter to continuity mode (beep mode). Touch one probe to each pad of the former SB63. **No beep = success** (bridge removed). If it still beeps, repeat steps 2-6.

**WRONG**: If you see the copper pad lifting or delaminating, you're pressing too hard or too long. Reduce pressure and time. The pad is delicate.

**FIX**: If you accidentally lift a pad, the bridge is still removed (which is what you wanted). The trace is broken. Verify with continuity.

#### 0.3.2 Remove SB65 on FZC Nucleo

**DO**: Identical procedure to 0.3.1, but locate **SB65** (near SB63, same area).

**MEASURE**: Continuity check — no beep on SB65 pads.

**Why SB63 + SB65 on FZC**: These solder bridges connect PA2 and PA3 to the ST-LINK virtual COM port (USART2). We need PA2 for lidar UART TX and PA3 for lidar UART RX. Removing the bridges frees these pins.

**After removal**: You lose the ST-LINK VCP printf. Use SWO (PB3) or LPUART1 for debug output instead.

#### 0.3.3 Remove SB63 on RZC Nucleo

**GATHER**: RZC Nucleo board.

**DO**: Same procedure as 0.3.1, but on the **RZC** board.

**MEASURE**: Continuity check — no beep on SB63 pads.

**Why**: PA2 needed for ADC board temperature (ADC1_IN3). PA3 needed for ADC battery voltage (ADC1_IN4).

#### 0.3.4 Remove SB65 on RZC Nucleo

**DO**: Same procedure, SB65 on RZC board.

**MEASURE**: Continuity check — no beep on SB65 pads.

#### 0.3.5 Optional: Remove SB21 on CVC Nucleo

**GATHER**: CVC Nucleo board.

**DO**: Locate SB21 on the CVC Nucleo. This connects PA5 (SPI1_SCK) to the onboard LED LD2. If you remove it, the LED won't blink with SPI clock activity (cleaner SPI signal). If you leave it, the LED will flicker during SPI transfers (harmless but noisy).

**Recommendation**: Remove it. A flickering LED during SPI can be confusing when debugging.

**MEASURE**: Continuity check — no beep on SB21 pads.

---

### 0.4 Verification Checkpoint — Board Prep Complete

| Check | Result |
|-------|--------|
| [ ] All components inventoried and verified | |
| [ ] All 4 boards labeled (CVC, FZC, RZC, SC) | |
| [ ] FZC SB63 removed (continuity: no beep) | |
| [ ] FZC SB65 removed (continuity: no beep) | |
| [ ] RZC SB63 removed (continuity: no beep) | |
| [ ] RZC SB65 removed (continuity: no beep) | |
| [ ] CVC SB21 removed or decision documented | |

---

## Step 1: Power Distribution

> **Goal**: 12V goes in from PSU, 5V and 3.3V come out, all verified. No boards connected yet.

### 1.1 Mount Buck Converters on Base Plate

**GATHER**: 2x LM2596 buck converter modules (BOM #35), base plate (BOM #59), M3 standoffs or double-sided tape.

**TOOLS**: Small Phillips screwdriver (for standoffs) or double-sided foam tape.

**DO**:
1. Place the base plate on your desk. Orient it landscape (wider than tall).
2. Identify the **left rear corner** — this is where PSU power enters.
3. Mount the first LM2596 module near the left rear corner. This will be the **12V-to-5V** converter. Secure with standoffs or foam tape.
4. Mount the second LM2596 module next to it (within 5cm). This will be the **12V-to-3.3V** converter.
5. If using TSR 2-2433N modules instead of the second LM2596 for 3.3V: mount those here instead.

**WRONG**: Don't mount converters right at the edge — leave 2cm margin for screw terminals and wiring.

---

### 1.2 Adjust 5V Buck Converter Output

**GATHER**: 5V buck converter (LM2596 #1), 12V PSU, 2 short pieces of 18AWG wire (red and black, ~15cm each).

**TOOLS**: Multimeter (DC voltage mode), small screwdriver (for potentiometer).

**DO**:
1. Set your bench PSU to 12.0V, current limit to 1A (for safety during adjustment).
2. Cut a 15cm length of red 18AWG wire. Strip 5mm from each end.
3. Cut a 15cm length of black 18AWG wire. Strip 5mm from each end.
4. Connect red wire from PSU (+) to the buck converter **IN+** terminal.
5. Connect black wire from PSU (-) to the buck converter **IN-** terminal.
6. **Do NOT connect anything to the output yet.**
7. Turn on the PSU.
8. Touch multimeter probes to the buck converter **OUT+** and **OUT-** terminals.
9. Read the voltage. It will likely show some random value (factory default).
10. Using the small screwdriver, **slowly turn the potentiometer** on the LM2596 module. Turn clockwise to decrease voltage, counter-clockwise to increase (varies by module — just turn and watch the multimeter).
11. Adjust until the multimeter reads **5.0V** (+/- 0.05V, so 4.95V to 5.05V is fine).
12. Turn off the PSU.

**MEASURE**: Output voltage = **4.9V to 5.1V** with no load. Write down the exact value: _________ V.

**WRONG**: If the output shows 0V, check that IN+ and IN- are not swapped. If the voltage won't change when you turn the pot, the potentiometer might be a multi-turn — keep turning.

**FIX**: If you overshoot and read >6V momentarily, that's fine with no load. Just keep adjusting down to 5.0V.

---

### 1.3 Adjust 3.3V Buck Converter Output

**GATHER**: 3.3V buck converter (LM2596 #2 or TSR 2-2433N).

**DO**: Repeat the same procedure as Step 1.2, but adjust the output to **3.3V**.

If using TSR 2-2433N: This is a fixed-output module. No adjustment needed. Just verify the output.

**MEASURE**: Output voltage = **3.2V to 3.4V** with no load. Write down: _________ V.

---

### 1.4 Wire Schottky Reverse Protection Diode

**GATHER**: SB560 Schottky diode (BOM #36), 16AWG red wire (~10cm), heat shrink tubing.

**TOOLS**: Wire strippers, soldering iron, heat shrink or electrical tape.

**DO**:
1. Identify the SB560 diode. It's a black cylindrical component (DO-201AD package) with a **silver/white stripe** on one end. The stripe marks the **cathode** (the "exit" side).
2. Cut a 10cm length of red 16AWG wire. Strip 8mm from each end.
3. The diode goes in the positive 12V line: PSU (+) connects to the **anode** (no stripe end), and the **cathode** (stripe end) connects to your 12V main rail.
4. Solder the anode lead of the diode to one end of the red wire.
5. Apply heat shrink over the solder joint.

This diode assembly will be wired into the power path in the next step.

**WRONG**: If you install the diode backwards (stripe toward PSU), current won't flow. Your rails will show 0V. Swap the diode orientation.

---

### 1.5 Wire 10A Fuse + 12V Main Rail Screw Terminal

**GATHER**: 10A blade fuse (BOM #38), inline fuse holder (BOM #37), 16AWG red wire (~30cm), 16AWG black wire (~30cm), 2-position screw terminal (BOM #62).

**TOOLS**: Wire strippers, small screwdriver.

**DO**:
1. Mount a 2-position screw terminal on the base plate near the PSU entry corner. This is your **12V MAIN RAIL** distribution point.
2. Wire the power path in this order:

```
PSU (+) red 16AWG → SB560 anode → SB560 cathode (stripe) → fuse holder wire 1
                                                              → [10A fuse inside]
                                                              → fuse holder wire 2 → 12V MAIN RAIL terminal (+)
```

3. Cut a 30cm length of red 16AWG wire for the run from the SB560 cathode through the fuse to the main rail terminal.
4. Insert the 10A fuse into the inline fuse holder. Snap it closed.
5. Connect one end of the fuse holder to the Schottky diode cathode (solder or screw terminal, depending on your fuse holder type).
6. Connect the other end of the fuse holder to the (+) position of the main rail screw terminal.
7. Tighten the screw terminal firmly.

**MEASURE**: With PSU off, set multimeter to continuity. Touch one probe to PSU (+) output terminal and the other to the 12V MAIN RAIL screw terminal (+). You should hear a **beep** (continuity through diode and fuse). If no beep: check fuse is seated, diode not backwards, all connections tight.

---

### 1.6 Wire Star Ground Point

**GATHER**: 3-position or 2-position screw terminal block (large one), 16AWG black wire (~30cm).

**DO**:
1. Mount a large screw terminal block on the base plate, near the 12V main rail. This is your **STAR GROUND POINT**. All ground returns from every subsystem converge here.
2. Cut a 30cm length of black 16AWG wire. Strip 5mm from each end.
3. Connect one end to the PSU (-) output terminal.
4. Connect the other end to the star ground screw terminal.
5. Later, you'll add more ground wires from each ECU, motor, and servo to this same terminal.

---

### 1.7 Wire Buck Converter Inputs from 12V Rail

**GATHER**: 18AWG red wire (~20cm x2), 18AWG black wire (~20cm x2).

**DO**:
1. Cut 2 lengths of red 18AWG wire, ~20cm each. Strip 5mm from each end.
2. Cut 2 lengths of black 18AWG wire, ~20cm each. Strip 5mm from each end.
3. Connect red wire #1 from the 12V MAIN RAIL (+) terminal to the 5V buck converter IN+.
4. Connect black wire #1 from the STAR GROUND terminal to the 5V buck converter IN-.
5. Connect red wire #2 from the 12V MAIN RAIL (+) terminal to the 3.3V buck converter IN+.
6. Connect black wire #2 from the STAR GROUND terminal to the 3.3V buck converter IN-.

**Label each wire** with masking tape: "12V TO 5V BUCK" and "12V TO 3.3V BUCK".

---

### 1.8 Verify All Power Rails (6-Point Checklist)

**TOOLS**: Multimeter (DC voltage mode).

**DO**:
1. Double-check all connections visually. No bare wire touching anything it shouldn't.
2. Set PSU to 12.0V, current limit to 2A.
3. Turn on the PSU.

| # | Test | Probe (+) | Probe (-) | Expected | Actual | Pass? |
|---|------|-----------|-----------|----------|--------|-------|
| 1 | 12V main rail | Main rail (+) | Star ground | 11.5-12.0V (after Schottky drop ~0.4V) | ___V | [ ] |
| 2 | 5V rail | 5V buck OUT+ | Star ground | 4.9-5.1V | ___V | [ ] |
| 3 | 3.3V rail | 3.3V buck OUT+ | Star ground | 3.2-3.4V | ___V | [ ] |
| 4 | Reverse polarity (CAREFUL) | Swap PSU leads briefly | | 0V on all rails, nothing smokes | | [ ] |
| 5 | 5V ripple (optional, needs scope) | Scope on 5V, AC coupled | | < 50mV peak-to-peak | ___mV | [ ] |
| 6 | 3.3V ripple (optional, needs scope) | Scope on 3.3V, AC coupled | | < 30mV peak-to-peak | ___mV | [ ] |

4. Turn off the PSU.

**STOP if any measurement is wrong. Debug before proceeding.**

**WRONG**: If 12V rail reads 0V: check Schottky diode orientation (stripe toward rail), check fuse is seated, check PSU is on and set to 12V. If 5V or 3.3V reads wrong: re-adjust the potentiometer on the buck converter.

---

## Step 2: Mount Boards & USB Boot

> **Goal**: All 4 boards are mounted, boot via USB, then boot from 12V.

### 2.1 Mount Standoffs on Base Plate

**GATHER**: 16x M3 nylon standoffs (10mm, BOM #60), 16x M3 screws, 4 boards.

**TOOLS**: Small Phillips screwdriver, ruler/tape measure.

**DO**:
1. Plan the layout on the base plate. Left-to-right: CVC — FZC — RZC — SC. Space them at least 50mm apart.
2. Place the CVC Nucleo on the base plate. Mark the 4 mounting holes with a pencil.
3. Repeat for FZC, RZC, and SC.
4. At each marked location, push a M3 screw through from the bottom of the base plate, thread a standoff on top.
5. You should now have 16 standoffs poking up from the base plate in the pattern of 4 boards.

Don't mount the boards yet — do that in 2.2.

---

### 2.2 Mount Each Board

**DO**:
1. Place the **CVC Nucleo** on its 4 standoffs. Align the mounting holes.
2. Thread M3 screws (or nuts) on top to secure it. Finger-tight is fine — don't overtighten nylon standoffs.
3. Repeat for **FZC Nucleo**, **RZC Nucleo**, and **SC LaunchPad**.
4. Verify each board is stable and doesn't wobble.

**Layout on base plate (looking from above):**

```
   [PSU entry + Bucks]         [12V RAIL + STAR GND]

   [CVC]    [FZC]    [RZC]    [SC]
     |        |        |        |
   ================ CAN BUS ROUTE (top edge) ================

   [E-stop] [Relay] [LEDs]      [Motor area]     [BTS7960]
                                             (front/operator side)
```

---

### 2.3 USB Power-Up Test (Each Board, One at a Time)

**GATHER**: USB micro-B cables (3 for Nucleos + 1 for LaunchPad), your PC/laptop.

**DO for each board**:
1. **Disconnect all other boards** (only one USB at a time for now).
2. Connect the USB cable from your PC to the board.
3. Observe the board:

| Board | What to Look For | Expected |
|-------|-----------------|----------|
| CVC Nucleo | Green LED (LD2) near USB connector | Blinks (factory demo firmware) |
| FZC Nucleo | Green LED (LD2) | Blinks |
| RZC Nucleo | Green LED (LD2) | Blinks |
| SC LaunchPad | LED near XDS110 debugger section | Lights up or blinks |

4. On your PC, verify the board appears:
   - **Nucleo**: Shows as "STM32 STLink" USB device. On Windows: check Device Manager. On Linux: `lsusb` shows STMicroelectronics.
   - **LaunchPad**: Shows as "XDS110" USB device.
5. (Optional) Flash a simple blink firmware to confirm programming works.
6. Disconnect USB.

**WRONG**: If a Nucleo doesn't light up via USB: try a different USB cable (some cables are charge-only, no data). If it appears in Device Manager but doesn't program: update ST-LINK firmware using STM32CubeProgrammer.

---

### 2.4 Wire 12V to CVC Nucleo VIN

**GATHER**: 18AWG red wire (~20cm), 18AWG black wire (~20cm).

**TOOLS**: Wire strippers, small screwdriver or soldering iron (depending on how you connect to Nucleo).

**DO**:
1. Cut a 20cm length of red 18AWG wire. Strip 5mm from each end.
2. Cut a 20cm length of black 18AWG wire. Strip 5mm from each end.
3. Connect red wire from the 12V MAIN RAIL (+) terminal to the **CVC Nucleo CN7 pin 24 (VIN)**. This is the morpho connector on the left side. Count from the top: VIN is clearly labeled on the Nucleo silkscreen. Alternatively, use a Dupont male pin crimped onto the wire end and insert into the morpho header.
4. Connect black wire from the STAR GROUND terminal to **CVC Nucleo CN7 pin 20 (GND)**.
5. Label both wires: "CVC 12V" and "CVC GND".

**MEASURE**: With PSU on at 12V, measure voltage between CN7 pin 24 (VIN) and CN7 pin 20 (GND) on the CVC Nucleo. Expected: **11.5-12.0V** (after Schottky drop from main rail).

---

### 2.5 Wire 12V to FZC Nucleo VIN

**DO**: Same procedure as 2.4, connecting to the **FZC Nucleo** CN7 pin 24 (VIN) and CN7 pin 20 (GND).

**Label**: "FZC 12V" and "FZC GND".

---

### 2.6 Wire 12V to RZC Nucleo VIN

**DO**: Same procedure as 2.4, connecting to the **RZC Nucleo** CN7 pin 24 (VIN) and CN7 pin 20 (GND).

**Label**: "RZC 12V" and "RZC GND".

---

### 2.7 SC Power: Keep on USB (Simplest for Bench)

The SC LaunchPad can be powered via its USB cable from the XDS110 debugger. For bench operation, this is the simplest approach. The LaunchPad's onboard LDO converts USB 5V to 3.3V for the TMS570.

**Critical**: The SC power MUST be independent of the kill relay. USB power from your PC satisfies this requirement — the SC stays powered even when the kill relay opens.

If you later want to power the SC from the 12V rail (without USB), you'll need a separate 3.3V regulator for the SC that connects directly to the 12V main rail (NOT through the kill relay). For now, USB is fine.

---

### 2.8 Standalone Boot Test

**DO**:
1. Disconnect all USB cables from all boards.
2. Turn on the 12V PSU.
3. Check each Nucleo board:

| Board | Check | Expected |
|-------|-------|----------|
| CVC | Green LED (LD2) or power LED | ON or blinking (board is alive from VIN) |
| FZC | Green LED (LD2) or power LED | ON or blinking |
| RZC | Green LED (LD2) or power LED | ON or blinking |

4. Check the SC LaunchPad: it should NOT be powered (no USB connected, no separate 12V supply yet). This is expected.
5. Measure 3.3V output on each Nucleo: Touch multimeter (+) to CN7 pin 16 (3V3) and (-) to CN7 pin 20 (GND).

| Board | Expected 3V3 | Actual |
|-------|-------------|--------|
| CVC | 3.2-3.4V | ___V |
| FZC | 3.2-3.4V | ___V |
| RZC | 3.2-3.4V | ___V |

6. Turn off PSU.

### Weekend 1 Checkpoint

| Check | Done? |
|-------|-------|
| [ ] 12V main rail: 11.5-12.0V verified | |
| [ ] 5V rail: 4.9-5.1V verified | |
| [ ] 3.3V rail: 3.2-3.4V verified | |
| [ ] Reverse polarity test passed | |
| [ ] All 4 boards labeled | |
| [ ] All solder bridges removed (FZC SB63/65, RZC SB63/65) | |
| [ ] CVC, FZC, RZC boot via USB (LED blinks) | |
| [ ] CVC, FZC, RZC boot from 12V (standalone, no USB) | |
| [ ] SC boots via USB | |

---

# WEEKEND 2: CAN Bus

---

## Step 3: CAN Bus

> **Goal**: All 4 ECUs + Pi see each other on a 500 kbps CAN bus.
> **This is the hardest phase for a beginner.** Take it one node at a time.

### 3.1 Solder CVC CAN Transceiver (TJA1051) onto Perfboard

**GATHER**: 1x TJA1051T/3 module (BOM #6), 1x perfboard 5x7cm (BOM #61), 1x 100nF ceramic cap (BOM #43), 22AWG hookup wire (multiple colors from BOM #66).

**TOOLS**: Soldering iron, solder, flux, wire strippers.

**DO**:
1. The TJA1051 module is a small breakout board with pins: VCC, GND, TXD, RXD, CANH, CANL, and possibly STB. Identify each pin by reading the silkscreen or module documentation.
2. Place the TJA1051 module on the perfboard. The module may have header pins — solder them to the perfboard.
3. Solder a 100nF ceramic capacitor between the VCC and GND pins of the module, as close to the module as possible (within 10mm). This is the decoupling capacitor.

---

### 3.2 Wire CVC Transceiver to Nucleo + 3.3V + Decoupling Cap

**GATHER**: 22AWG wires (red, black, blue, yellow, green), Dupont jumper wires for Nucleo connections.

**DO**:
1. **VCC** on TJA1051 module → Connect to the external **3.3V rail** (from buck converter output). Use red 22AWG wire. NOT the Nucleo 3V3 pin.
2. **GND** on TJA1051 module → Connect to **star ground**. Use black wire.
3. **TXD** on TJA1051 module → Connect to **CVC Nucleo PA12 (FDCAN1_TX)**. This is **CN10 pin 12** on the Nucleo morpho connector. Use a Dupont M-F jumper wire.
4. **RXD** on TJA1051 module → Connect to **CVC Nucleo PA11 (FDCAN1_RX)**. This is **CN10 pin 14**. Use a Dupont jumper.
5. **STB** (standby) on TJA1051 module → Connect to **GND** (always active mode).
6. Leave CANH and CANL unconnected for now — they'll connect to the bus trunk in step 3.7.

**Label** the perfboard: "CVC CAN".

**MEASURE**: With PSU on, measure voltage on TJA1051 VCC pin: Expected **3.2-3.4V**.

---

### 3.3 Repeat CAN Transceiver for FZC

**GATHER**: 1x TJA1051T/3 module, 1x perfboard, 1x 100nF cap.

**DO**: Repeat steps 3.1 and 3.2, but for the FZC Nucleo:
- TXD → **FZC Nucleo PA12 (CN10-12)**
- RXD → **FZC Nucleo PA11 (CN10-14)**
- VCC → external 3.3V rail
- GND → star ground
- STB → GND

**Label**: "FZC CAN".

---

### 3.4 Repeat CAN Transceiver for RZC

**DO**: Repeat again for RZC:
- TXD → **RZC Nucleo PA12 (CN10-12)**
- RXD → **RZC Nucleo PA11 (CN10-14)**
- VCC → external 3.3V rail
- GND → star ground
- STB → GND

**Label**: "RZC CAN".

---

### 3.5 Solder SC CAN Transceiver (SN65HVD230)

**GATHER**: 1x SN65HVD230 module (BOM #7), 1x perfboard, 1x 100nF cap.

**DO**:
1. Mount the SN65HVD230 module on perfboard.
2. Solder 100nF decoupling cap between VCC and GND.

---

### 3.6 Wire SC Transceiver to LaunchPad

**DO**:
1. **VCC** → 3.3V rail + 100nF cap (already soldered).
2. **GND** → star ground.
3. **TXD** → LaunchPad **DCAN1TX** on J5 edge connector. Consult the LaunchPad documentation for the exact pin on J5.
4. **RXD** → LaunchPad **DCAN1RX** on J5 edge connector.
5. **Rs** (slope control) → **GND** (full speed, no slope limiting at 500 kbps).

**Label**: "SC CAN".

---

### 3.7 Run CAN Trunk Wire (CAN_H + CAN_L Twisted Pair)

**GATHER**: 22AWG twisted pair wire (BOM #12, yellow + green), 4x 3-position screw terminal blocks (BOM #63).

**TOOLS**: Wire strippers, small screwdriver.

**DO**:
1. Mount 4x 3-position screw terminal blocks along the top edge of the base plate, one near each ECU. Label each terminal block with the ECU name and: position 1 = CAN_H (yellow), position 2 = CAN_L (green), position 3 = CAN_GND (black).
2. Cut the twisted pair wire to run along the top edge from the CVC position to the SC position. Add 5cm extra at each end for service loop.
3. This is the CAN **trunk** wire. It runs continuously from end to end. At each ECU terminal block, do NOT cut the trunk — instead, strip 5mm of insulation from the middle of the wire, and insert both the continuing trunk wire and a stripped section into the same screw terminal position.
4. Alternatively, cut short stub wires (5cm each) and use them to "T-tap" from the trunk into each terminal block.
5. Tighten all screw terminals firmly.

**Wire color convention** (per HWR-015):
- **Yellow** = CAN_H
- **Green** = CAN_L
- **Black** = CAN_GND (reference ground — connect to star ground)

---

### 3.8 T-Tap Stub Wires to Each Transceiver

**GATHER**: Short 22AWG wire pieces (yellow, green, ~5-10cm each x 4 sets).

**DO**:
For each ECU CAN transceiver:
1. Cut a 10cm yellow wire. Strip 5mm from each end.
2. Connect one end to the ECU's CAN terminal block position 1 (CAN_H).
3. Connect the other end to the **CANH** pin on the CAN transceiver module.
4. Cut a 10cm green wire. Strip 5mm from each end.
5. Connect one end to the ECU's CAN terminal block position 2 (CAN_L).
6. Connect the other end to the **CANL** pin on the CAN transceiver module.

**Critical**: Keep all stub wires (from trunk to transceiver) **under 100mm** (10cm). Longer stubs cause signal reflections.

---

### 3.9 Install 120R Termination Resistors (2x: CVC End + SC End)

**GATHER**: 2x 120 ohm resistors (BOM #9).

**DO**:
1. At the **CVC end** of the bus: Solder or insert a 120 ohm resistor across CAN_H and CAN_L at the CVC CAN terminal block (between terminal positions 1 and 2).
2. At the **SC end** of the bus: Same thing — 120 ohm resistor across CAN_H and CAN_L at the SC terminal block.
3. The FZC and RZC terminals get **NO** termination resistor (they are in the middle of the bus).

---

### 3.10 Connect CANable Adapters (Pi + Dev PC)

**GATHER**: 2x USB-CAN adapters (BOM #8), short stub wires.

**DO**:
1. T-tap a short stub from the CAN trunk to CANable #1 (for Raspberry Pi).
2. T-tap a short stub from the CAN trunk to CANable #2 (for development PC).
3. Keep stubs under 100mm.
4. Plug CANable #1 into Pi USB (or set aside for later if Pi isn't set up yet).
5. Plug CANable #2 into your development PC.

---

### 3.11 Electrical Verification (Termination, Rest Voltage)

**DO**: Turn off ALL power supplies. Disconnect all USB cables.

| # | Test | How | Expected | Actual | Pass? |
|---|------|-----|----------|--------|-------|
| 1 | Termination resistance | Multimeter resistance mode: probe CAN_H and CAN_L at any terminal block | **60 ohm** (two 120R in parallel) | ___R | [ ] |
| 2 | CAN_H to GND | Multimeter resistance | > 1k ohm (no short) | ___R | [ ] |
| 3 | CAN_L to GND | Multimeter resistance | > 1k ohm (no short) | ___R | [ ] |

Now turn on power (12V PSU + SC USB):

| 4 | CAN_H rest voltage | Multimeter DC: CAN_H to GND | ~2.5V | ___V | [ ] |
| 5 | CAN_L rest voltage | Multimeter DC: CAN_L to GND | ~2.5V | ___V | [ ] |

**WRONG**: If termination reads 120R instead of 60R: one of the two resistors is not connected. Check both ends. If it reads <50R: you may have an extra resistor somewhere, or a short.

**WRONG**: If rest voltage is 0V on both lines: the transceiver may not be powered. Check 3.3V at the transceiver VCC pin.

---

### 3.12 Flash CAN Test Firmware + candump Verification

**DO**:
1. Flash a minimal CAN TX test firmware onto the **CVC Nucleo**. This firmware should:
   - Initialize FDCAN1 at 500 kbps (prescaler=34, TSEG1=7, TSEG2=2 for 170MHz clock)
   - Send a CAN frame with ID 0x010 every 100ms
2. On your development PC with the USB-CAN adapter, open a terminal and run:
   - Linux: `sudo ip link set can0 type can bitrate 500000 && sudo ip link set can0 up && candump can0`
   - Windows: Use a CAN analyzer tool (e.g., PCAN-View, or the tool that came with your USB-CAN adapter)
3. You should see frames with ID 0x010 appearing at ~10 Hz.

| # | Test | Expected |
|---|------|----------|
| 1 | CVC sends 0x010 frames | Visible on candump at ~10 Hz |
| 2 | Flash CAN TX on FZC | FZC frames visible (use different ID, e.g., 0x011) |
| 3 | Flash CAN TX on RZC | RZC frames visible (e.g., 0x012) |
| 4 | Flash CAN TX on SC | SC frames visible (e.g., 0x013, using DCAN1) |
| 5 | Each ECU receives frames from others | Flash CAN RX firmware, verify reception |

**WRONG**: No frames at all? Systematic debug:
1. Check termination (60 ohm between CAN_H and CAN_L, power off)
2. Check rest voltage (both lines ~2.5V, power on)
3. Check TX/RX not swapped (PA12=TX to TXD, PA11=RX to RXD)
4. Check transceiver powered (3.3V on VCC)
5. Check bit timing matches between STM32 and your PC CAN adapter (both must be 500 kbps)
6. Check STB pin is tied to GND (not floating)

### Weekend 2 Checkpoint

| Check | Done? |
|-------|-------|
| [ ] 4 CAN transceivers soldered and wired | |
| [ ] CAN trunk wire runs along base plate edge | |
| [ ] 120R termination at CVC end and SC end | |
| [ ] Termination resistance = 60 ohm (measured) | |
| [ ] CAN_H and CAN_L rest voltage = ~2.5V each | |
| [ ] CVC sends CAN frames, visible on candump | |
| [ ] All 4 ECUs communicate on the bus | |

---

# WEEKEND 3: Safety Chain + Sensors

---

## Step 4: Safety Chain

> **Goal**: Kill relay works, E-stop works, watchdogs reset MCUs. **This must work before connecting any actuator.**

### 4.1 Wire IRLZ44N MOSFET Gate Circuit (100R + 10k Pull-Down)

**GATHER**: 1x IRLZ44N MOSFET (BOM #25), 1x 100 ohm resistor (BOM #52), 1x 10k ohm resistor (BOM #49), 22AWG wire.

**TOOLS**: Soldering iron, perfboard or breadboard.

**DO**:
1. Identify the IRLZ44N pinout (looking at the front of the TO-220 package, tab facing away):
   - Pin 1 (left) = **Gate**
   - Pin 2 (center) = **Drain**
   - Pin 3 (right) = **Source**
2. Mount the IRLZ44N on perfboard or breadboard.
3. Solder a **100 ohm resistor** in series between the Gate pin and the wire that will go to SC GIO_A0. One end of the 100R connects to the MOSFET Gate pin; the other end is the input from SC.
4. Solder a **10k ohm resistor** from the Gate pin to GND (pull-down). This ensures the MOSFET is OFF when the SC is not driving the gate.
5. Connect **Source** to GND (star ground).
6. Leave **Drain** unconnected for now — it connects to the relay coil in the next step.

**MEASURE**: With no power applied, measure resistance from Gate to GND: should read ~10k ohm (the pull-down).

---

### 4.2 Wire Relay Coil to MOSFET Drain + Flyback Diode

**GATHER**: 1x 12V 30A relay (BOM #24), 1x relay socket (BOM #72, if using), 1x 1N4007 diode (BOM #26), 18AWG red and black wire.

**DO**:
1. Identify the relay pins. A typical 5-pin automotive relay has:
   - Pin 85: Coil (-) — this connects to MOSFET Drain
   - Pin 86: Coil (+) — this connects to 12V Main Rail
   - Pin 30: Common contact — this connects to 12V Main Rail
   - Pin 87: Normally-Open contact — this is the 12V Actuator Rail output
   - Pin 87a: Normally-Closed (may not exist on SPST-NO)
2. If using a relay socket, mount it and insert the relay.
3. Wire **relay pin 86 (coil +)** to the **12V MAIN RAIL** using 18AWG red wire.
4. Wire **relay pin 85 (coil -)** to the **IRLZ44N Drain** (pin 2).
5. Install the **1N4007 flyback diode** across the relay coil:
   - **Cathode (stripe end)** → connects to pin 86 (coil +, the 12V side)
   - **Anode (no stripe)** → connects to pin 85 (coil -, the MOSFET drain side)
   - This diode protects against voltage spikes when the coil de-energizes.

**WRONG**: If the flyback diode is backwards (cathode to drain), it will short-circuit the 12V rail through the diode when the MOSFET turns on. The fuse should blow. Fix: swap the diode orientation.

---

### 4.3 Wire Relay Contacts (12V Main -> Relay -> Actuator Rail + 30A Fuse)

**GATHER**: 30A blade fuse (BOM #39), fuse holder, 16AWG red wire, screw terminal for actuator rail.

**DO**:
1. Wire relay **pin 30 (common)** to the **12V MAIN RAIL** using 16AWG red wire.
2. Wire relay **pin 87 (normally-open)** through a **30A fuse** to a new screw terminal that you'll label **12V ACTUATOR RAIL**.
3. The actuator rail screw terminal is where motor driver and servo power will connect later.

**Label**: "12V ACTUATOR RAIL (KILL RELAY GATED)".

---

### 4.4 Verify Kill Relay (6-Point Test)

**GATHER**: Wire from SC GIO_A0 (J3-1 on LaunchPad) to the 100R gate resistor input.

**DO**: Connect a wire from **SC LaunchPad J3 pin 1 (GIO_A0)** to the input side of the 100R gate resistor (the end NOT connected to the MOSFET gate).

| # | Test | How | Expected | Pass? |
|---|------|-----|----------|-------|
| 1 | Default state (SC not powered) | Measure voltage on actuator rail | **0V** (relay open) | [ ] |
| 2 | SC powered, GIO_A0 = LOW | Power SC via USB, GIO default is LOW | **0V** (relay open) | [ ] |
| 3 | SC drives GIO_A0 = HIGH | Flash test FW that sets GIO_A0 HIGH | **~11.5V** on actuator rail (relay closed, audible click) | [ ] |
| 4 | SC drives GIO_A0 = LOW | Firmware sets GIO_A0 LOW | **0V** on actuator rail (relay opens, click) | [ ] |
| 5 | SC power loss | Unplug USB from SC | **0V** (fail-safe: 10k pulls gate LOW) | [ ] |
| 6 | Dropout time (scope) | Trigger on GIO_A0 falling edge, measure time to rail=0V | **< 10ms** | [ ] |

**WRONG**: If actuator rail shows 12V even when SC is off: the 10k pull-down resistor on the gate is missing or disconnected. Fix: add or re-solder the 10k pull-down from gate to GND.

---

### 4.5 Wire E-Stop Button

**GATHER**: NC E-stop button (BOM #27), 1x 10k ohm resistor (BOM #49), 1x 100nF capacitor (BOM #43), 22AWG wire.

**DO**:
1. Wire one terminal of the NC E-stop button to **GND**.
2. Wire the other terminal through a **10k series resistor** to **CVC Nucleo PC13 (CN7 pin 23)**.
3. At the junction between the 10k resistor and the wire going to PC13, solder a **100nF capacitor** from this junction to GND. This is the RC debounce filter (tau = 10k * 100nF = 1ms).
4. Enable the internal pull-up on PC13 in firmware (~40k ohm).

```
Circuit:
GND ←── [NC button] ──── junction ──── [10k resistor] ──── PC13 (CN7-23)
                            |
                         [100nF]
                            |
                           GND
```

---

### 4.6 Verify E-Stop (3-Point Test)

| # | Test | How | Expected | Pass? |
|---|------|-----|----------|-------|
| 1 | Button NOT pressed (resting) | Measure DC voltage on PC13 | **~0V** (LOW — button shorts to GND through NC contacts) | [ ] |
| 2 | Button PRESSED (locked) | Press and lock the mushroom button | **~3.3V** (HIGH — button opens, internal pull-up drives HIGH) | [ ] |
| 3 | Wire disconnected | Disconnect the button wire | **~3.3V** (fail-safe: same as pressed) | [ ] |

---

### 4.7 Solder TPS3823 #1 (CVC) onto SOT-23 Breakout Board

> **This is the hardest soldering task.** The TPS3823 is a SOT-23-5 package — pins are ~0.95mm apart. Watch YouTube videos on "drag soldering SOT-23" before attempting. Use flux generously. You have spare breakout boards — practice first.

**GATHER**: 1x TPS3823DBVR IC (BOM #22), 1x SOT-23 breakout board (BOM #23), solder, flux.

**TOOLS**: Soldering iron (fine tip, 300-350C), flux pen, solder wick, magnifying glass/loupe, tweezers.

**DO**:
1. Apply flux to the pads on the SOT-23 breakout board.
2. Place the TPS3823 IC on the breakout board pads. Align pin 1 (marked with a dot on the IC) to pad 1 on the breakout board. The **TPS3823DBVR** pinout (SOT-23-5, top view, dot = pin 1):
   - Pin 1 = **GND**
   - Pin 2 = **CT** (timeout capacitor — connects to 100nF cap)
   - Pin 3 = **WDI** (watchdog input — connects to MCU GPIO)
   - Pin 4 = **RESET** (output, active-low, open-drain — connects to MCU NRST)
   - Pin 5 = **VDD** (power — connects to 3.3V)

3. Tack-solder one corner pin first. Check alignment. Then solder the remaining pins.
4. Inspect under magnification for solder bridges between pins. If bridged, apply flux and drag solder wick across the bridge.

**WRONG**: If pins are bridged (solder connecting two adjacent pins): apply flux, then touch the bridge with solder wick and a hot iron. The wick absorbs the excess solder.

**FIX**: If you can't get a clean solder job, use one of your spare breakout boards and try again. You have 6 spares.

---

### 4.8 Wire CVC Watchdog (VDD, GND, WDI->PB0, RESET->NRST, CT Cap)

**GATHER**: Soldered TPS3823 breakout from 4.7, 3x 100nF capacitors (BOM #43), 22AWG wire.

**DO**:
1. **Pin 5 (VDD)** → external 3.3V rail. Solder a **100nF capacitor** between VDD and GND, as close to the IC as possible.
2. **Pin 1 (GND)** → star ground.
3. **Pin 3 (WDI)** → **CVC Nucleo PB0 (CN10 pin 31)**. Use a Dupont jumper wire.
4. **Pin 4 (RESET)** → **CVC Nucleo NRST (CN7 pin 14)**. Solder a **100nF capacitor** from RESET to GND (debounce). Use a jumper wire from the breakout board to CN7-14.
5. **Pin 2 (CT)** → solder a **100nF capacitor** from CT to GND. This sets the watchdog timeout to ~1.6 seconds.
6. The TPS3823DBVR has no separate MR (manual reset) pin — all 5 pins are accounted for above. No additional wiring needed.

**Label**: "CVC WDT".

---

### 4.9 Verify CVC Watchdog (3-Point Test)

| # | Test | How | Expected | Pass? |
|---|------|-----|----------|-------|
| 1 | No WDI toggling | Power up CVC with firmware that does NOT toggle PB0. Observe with oscilloscope on NRST | Periodic LOW pulses on NRST every ~1.6 seconds (watchdog resetting MCU) | [ ] |
| 2 | WDI toggling | Flash firmware that toggles PB0 every 500ms | MCU stays running, NO resets on NRST (oscilloscope shows NRST stays HIGH) | [ ] |
| 3 | Hang detection | Flash firmware that hangs after 5 seconds (stops toggling PB0) | MCU resets ~1.6s after hang (oscilloscope shows NRST going LOW) | [ ] |

---

### 4.10 Repeat Watchdog for FZC (#2), RZC (#3), SC (#4)

**DO**: Repeat steps 4.7-4.9 for each remaining ECU.

| ECU | WDI GPIO | WDI Morpho Pin | NRST Location |
|-----|----------|----------------|---------------|
| **FZC** | PB0 | CN10-31 | CN7 pin 14 (NRST) |
| **RZC** | **PB4** (NOT PB0!) | CN10-27 | CN7 pin 14 (NRST) |
| **SC** | GIO_A5 | J3-6 (HDR1) | nRST on LaunchPad |

**Critical**: The RZC watchdog uses **PB4**, not PB0. On the RZC, PB0 is used for the BTS7960 motor R_EN. Don't confuse them.

Verify each watchdog with the same 3-point test.

---

## Step 4.5: Preemptive ADC/GPIO Overvoltage Protection

> **Goal**: Protect all MCU analog/digital inputs from accidental overvoltage before connecting any sensor. Do this BEFORE Phase 5.

**Why now**: A miswired sensor or loose jumper can put 12V on a 3.3V ADC pin. The Zener + resistor clamp costs ~€0.10 per channel and takes 5 minutes to add. Replacing a dead Nucleo costs €16 and a weekend.

### 4.5.1 Install Zener Clamps on RZC ADC Pins (Minimum 4 Channels)

**GATHER**: 4-6x BZX84C3V3 Zener diodes (BOM #55), 4-6x 100R resistors (BOM #52), small perfboard strip or breadboard.

**TOOLS**: Soldering iron, solder, multimeter, bench PSU (set to 5V for testing).

**DO**:
1. For each ADC channel listed below, build this protection circuit:

```
Sensor output → [100R series resistor] → junction → MCU ADC pin
                                            |
                                     BZX84C3V3 Zener
                                     (cathode to junction,
                                      anode to GND)
                                            |
                                           GND
```

2. Solder the Zener + 100R pairs onto a small perfboard strip near the RZC Nucleo, or use breadboard for initial testing.

**Channels to protect (minimum — install on all 4)**:

| ECU | Pin | Signal | Risk Source |
|-----|-----|--------|-------------|
| RZC | PA0 (CN7-28) | ACS723 current sense | Sensor fault, miswire |
| RZC | PA1 (CN7-30) | NTC motor temperature | Divider miswire |
| RZC | PA2 (CN10-35) | NTC board temperature | Divider miswire |
| RZC | PA3 (CN10-37) | Battery voltage (47k/10k divider) | 12V proximity |

**Optional additional channels** (lower risk, 3.3V-only signals):

| ECU | Pin | Signal | Risk |
|-----|-----|--------|------|
| CVC | PA4 (CN7-32) | SPI CS1 (digital, 3.3V) | Low — SPI is 3.3V only |
| FZC | PA3 (CN10-37) | Lidar UART RX | Low — UART is 3.3V logic |

---

### 4.5.2 Verify Each Zener Clamp

**TOOLS**: Bench PSU, multimeter.

**DO** (for each protected channel):
1. Set bench PSU to **5V** (simulating overvoltage).
2. Connect PSU (+) through the 100R resistor to the junction point (where the Zener cathode connects).
3. Connect PSU (-) to GND.
4. Measure voltage at the junction (ADC pin side).

**MEASURE**: Junction voltage should read **≤ 3.6V** (Zener is clamping).

5. Now set bench PSU to **3.3V** (normal operating voltage).
6. Measure voltage at the junction.

**MEASURE**: Junction voltage should read **~3.2V** (Zener inactive, small drop across 100R).

| Channel | 5V test (≤3.6V?) | 3.3V test (~3.2V?) | Pass? |
|---------|-------------------|---------------------|-------|
| RZC PA0 | ___V | ___V | [ ] |
| RZC PA1 | ___V | ___V | [ ] |
| RZC PA2 | ___V | ___V | [ ] |
| RZC PA3 | ___V | ___V | [ ] |

**WRONG**: If junction reads 5V during the 5V test, the Zener is not conducting — check polarity (cathode must face the junction/signal side, anode to GND). If junction reads 0V during the 3.3V test, the Zener may be shorted — replace it.

---

## Step 5: Sensors

> **Goal**: Each sensor reads valid data through the MCU.

### 5.1 Wire CVC Pedal Sensor #1 (AS5048A on SPI1, CS1=PA4)

**GATHER**: 1x AS5048A module (BOM #13), 1x 10k resistor (BOM #49), 1x 100nF capacitor (BOM #43), 1x diametric magnet (BOM #14), 22AWG wire (blue for SPI signals).

**DO**:
1. Mount the AS5048A module on perfboard near the CVC Nucleo.
2. Wire **CLK** → CVC **PA5 (SPI1_SCK, CN10 pin 11)**. Use blue wire.
3. Wire **DO (MISO)** → CVC **PA6 (SPI1_MISO, CN10 pin 13)**.
4. Wire **DI (MOSI)** → CVC **PA7 (SPI1_MOSI, CN10 pin 15)**.
5. Wire **CSn** → CVC **PA4 (CN7 pin 32)**.
6. Solder a **10k pull-up resistor** from the CSn line to 3.3V (ensures sensor deselected on MCU reset).
7. Wire **VDD** → external 3.3V rail.
8. Solder **100nF decoupling cap** from VDD to GND, within 10mm of the sensor module.
9. Wire **GND** → star ground.
10. Place the diametric magnet on/near the sensor IC (centered over the chip). The magnet should be within 1-3mm of the sensor surface.

**Label**: "CVC PEDAL 1 (CS=PA4)".

---

### 5.2 Wire CVC Pedal Sensor #2 (AS5048A on SPI1, CS2=PA15)

**GATHER**: 1x AS5048A module, 1x 10k resistor, 1x 100nF capacitor.

**DO**: Same wiring as 5.1, but:
- CLK, MISO, MOSI are **shared** with sensor #1 (same SPI1 bus).
- **CSn** → CVC **PA15 (CN7 pin 17)**.
- Solder a **10k pull-up resistor** from this CSn to 3.3V.
- Separate 100nF decoupling cap on this sensor's VDD.
- This sensor shares the same magnet on the pedal shaft as sensor #1 (both read the same shaft).

**Label**: "CVC PEDAL 2 (CS=PA15)".

---

### 5.3 Verify Pedal Sensors (SPI Read, Rotate Magnet)

**DO**:
1. Flash SPI test firmware on CVC that reads the AS5048A angle register (address 0x3FFF, SPI Mode 1: CPOL=0, CPHA=1, 1 MHz clock).
2. Select sensor #1 (PA4 LOW) and read. You should get a 14-bit value (0-16383).
3. Rotate the magnet. The value should change smoothly through the full range.
4. Deselect sensor #1 (PA4 HIGH). Select sensor #2 (PA15 LOW). Repeat.

| Check | Expected | Pass? |
|-------|----------|-------|
| Sensor #1 responds (non-zero, non-0x3FFF stuck value) | 14-bit value changes with magnet rotation | [ ] |
| Sensor #2 responds independently | 14-bit value changes with magnet rotation | [ ] |
| Both sensors give similar readings for same magnet position | Values within ~100 counts of each other | [ ] |

**WRONG**: If stuck at 0 or 0x3FFF: check SPI mode (must be Mode 1), check MISO/MOSI not swapped, check CS pull-up is present, check 3.3V power to sensor.

---

### 5.4 Wire FZC Steering Sensor (AS5048A on SPI2)

**GATHER**: 1x AS5048A module, 1x 10k resistor, 1x 100nF cap.

**DO**:
1. Mount near FZC Nucleo.
2. Wire **CLK** → FZC **PB13 (SPI2_SCK, CN10 pin 30)**.
3. Wire **DO (MISO)** → FZC **PB14 (SPI2_MISO, CN10 pin 28)**.
4. Wire **DI (MOSI)** → FZC **PB15 (SPI2_MOSI, CN10 pin 26)**.
5. Wire **CSn** → FZC **PB12 (CN10 pin 16)** + 10k pull-up to 3.3V.
6. VDD → 3.3V rail + 100nF cap. GND → star ground.

**Label**: "FZC STEERING SENSOR (CS=PB12)".

---

### 5.5 Verify Steering Sensor

Same test as 5.3 but on FZC with SPI2.

| Check | Expected | Pass? |
|-------|----------|-------|
| Sensor responds with changing 14-bit value | Value changes when magnet rotates | [ ] |

---

### 5.6 Wire TFMini-S Lidar to FZC USART2

**GATHER**: TFMini-S lidar (BOM #15), 470uF electrolytic cap (BOM #48).

**DO**:
1. Identify TFMini-S wires: **Red** = 5V, **Black** = GND, **White** = TX (sensor output), **Green** = RX (sensor input).
2. Wire **Red (VCC)** → external **5V rail** (NOT the Nucleo 5V pin). Add a **470uF electrolytic cap** from 5V to GND near the sensor for startup inrush.
3. Wire **Black (GND)** → star ground.
4. Wire **White (TX, sensor output)** → FZC **PA3 (USART2_RX, CN10 pin 37)**. The sensor's TX goes to the MCU's RX.
5. Wire **Green (RX, sensor input)** → FZC **PA2 (USART2_TX, CN10 pin 35)**. The MCU's TX goes to the sensor's RX.

**Critical reminder**: SB63 and SB65 must have been removed from the FZC board (Step 0.3). If not, PA2/PA3 are still connected to the ST-LINK VCP and the lidar won't work.

**Label**: "FZC LIDAR (USART2)".

---

### 5.7 Verify Lidar (115200 Baud, Distance Reading)

**DO**: Flash firmware on FZC that configures USART2 at 115200 baud, 8N1, and reads incoming data.

The TFMini-S sends 9-byte frames at ~100Hz. Parse bytes 2-3 (little-endian, distance in cm).

| Check | Expected | Pass? |
|-------|----------|-------|
| UART receives data at 115200 | Non-zero bytes arriving at ~100 Hz | [ ] |
| Distance value changes | Point at wall, value matches approximate distance in cm | [ ] |

**WRONG**: If no data received: check that White wire (sensor TX) goes to PA3 (MCU RX), not the other way around. Check SB63/SB65 were removed.

---

### 5.8 Wire ACS723 Current Sensor to RZC ADC (PA0)

**GATHER**: ACS723 module (BOM #16), 1x 1nF cap (BOM #44), 1x 100nF cap (BOM #43), 1x BZX84C3V3 Zener (BOM #55).

**DO**:
1. Wire **VCC** on ACS723 module → external 3.3V rail + 100nF decoupling cap.
2. Wire **GND** → analog ground (star ground).
3. Wire **VIOUT** (analog output) through:
   - A **1nF capacitor** from VIOUT to GND (bandwidth filter, right at the sensor)
   - A **100nF capacitor** from the ADC input to GND (anti-aliasing)
   - A **BZX84C3V3 Zener diode** from the ADC input to GND (overvoltage protection, cathode to signal, anode to GND)
   - Then connect to **RZC PA0 (ADC1_IN1, CN7 pin 28)**.
4. Leave IP+ and IP- (the current path) unconnected for now — they'll be wired in series with the motor in Step 6.

**MEASURE**: With no current flowing, read ADC on PA0. At 0A, ACS723 output = VCC/2 = ~1.65V. ADC 12-bit value = ~2048.

---

### 5.9 Wire NTC Thermistors to RZC ADC (PA1, PA2)

**GATHER**: 2x NTC 10k thermistors (BOM #17), 2x 10k ohm resistors (1% precision, BOM #49), 2x 100nF caps (BOM #43).

**DO for Motor Temperature NTC (PA1)**:
1. Connect a **10k fixed resistor** from the **3.3V rail** to a junction point.
2. Connect the **NTC thermistor** from the junction point to **GND**.
3. Connect a **100nF capacitor** from the junction point to GND.
4. Connect the junction point to **RZC PA1 (ADC1_IN2, CN7 pin 30)**.

**DO for Board Temperature NTC (PA2)**:
5. Repeat the same circuit, connecting to **RZC PA2 (ADC1_IN3, CN10 pin 35)**.

**Note**: SB63/SB65 must have been removed from RZC (Step 0.3) for PA2 to work as ADC.

---

### 5.10 Wire Battery Voltage Divider to RZC ADC (PA3)

**GATHER**: 1x 47k ohm resistor (BOM #50), 1x 10k ohm resistor (BOM #49), 1x 100nF cap (BOM #43), 1x BZX84C3V3 Zener (BOM #55).

**DO**:
1. Connect a **47k resistor** from the **12V main rail** to a junction point.
2. Connect a **10k resistor** from the junction point to **GND**.
3. Connect a **100nF capacitor** from the junction point to GND.
4. Connect a **BZX84C3V3 Zener** from the junction point to GND (cathode to junction, anode to GND).
5. Connect the junction point to **RZC PA3 (ADC1_IN4, CN10 pin 37)**.

---

### 5.11 Verify All RZC Analog Inputs

**DO**: Flash ADC test firmware on RZC that reads all 4 ADC channels.

| Channel | Pin | What It Reads | Expected at Room Temp / No Load | Pass? |
|---------|-----|---------------|--------------------------------|-------|
| ADC1_IN1 | PA0 | ACS723 (0A current) | ~1.65V (ADC ~2048) | [ ] |
| ADC1_IN2 | PA1 | Motor NTC (room temp ~25C) | ~1.65V (ADC ~2048), NTC=10k | [ ] |
| ADC1_IN3 | PA2 | Board NTC (room temp) | ~1.65V (ADC ~2048) | [ ] |
| ADC1_IN4 | PA3 | Battery voltage (12V) | ~2.1V (ADC ~2612) | [ ] |

---

### 5.12 Wire OLED Display to CVC I2C (PB8/PB9)

**GATHER**: SSD1306 OLED module (BOM #28), 2x 4.7k resistors (BOM #53), 1x 10uF electrolytic cap (BOM #45), 1x 100nF cap (BOM #43).

**DO**:
1. Wire **SCL** on OLED → CVC **PB8 (I2C1_SCL, CN10 pin 3)**.
2. Wire **SDA** on OLED → CVC **PB9 (I2C1_SDA, CN10 pin 5)**.
3. Solder a **4.7k pull-up resistor** from the SCL line to 3.3V (external pull-up).
4. Solder a **4.7k pull-up resistor** from the SDA line to 3.3V.
5. Wire **VCC** → 3.3V rail + **10uF bulk cap** + **100nF** decoupling.
6. Wire **GND** → star ground.

**Label**: "CVC OLED (I2C1)".

---

### 5.13 Verify OLED (I2C Scan -> 0x3C -> Display Text)

**DO**:
1. Flash I2C scan firmware on CVC. It should find a device at address **0x3C**.
2. Flash SSD1306 display driver firmware. The OLED should display text (e.g., "Taktflow CVC").

| Check | Expected | Pass? |
|-------|----------|-------|
| I2C scan finds device at 0x3C | Address detected, ACK received | [ ] |
| OLED displays text | Readable text on 128x64 display | [ ] |

### Weekend 3 Checkpoint

| Check | Done? |
|-------|-------|
| [ ] Kill relay: opens/closes on SC GIO_A0 command | |
| [ ] Kill relay: fail-safe on SC power loss (opens) | |
| [ ] E-stop: PC13 LOW when resting, HIGH when pressed | |
| [ ] E-stop: fail-safe on wire disconnect (reads HIGH) | |
| [ ] All 4 watchdogs: reset MCU when WDI stops toggling | |
| [ ] Zener clamps on RZC PA0-PA3: all 4 verified (clamp at ≤3.6V) | |
| [ ] CVC pedal sensors: both read 14-bit angle, change with magnet | |
| [ ] FZC steering sensor: reads angle | |
| [ ] FZC lidar: distance readings at 115200 baud | |
| [ ] RZC analog inputs: current, temp, battery voltage all plausible | |
| [ ] CVC OLED: displays text | |

---

# WEEKEND 4: Actuators + Full Integration

---

## Step 6: Actuators

> **Goal**: Motor spins, servos move. **Only after safety chain (Step 4) is verified!**

### 6.1 Wire 6V Regulator (12V Actuator Rail -> LM7806 -> 470uF -> Servo Bus)

**GATHER**: 2x LM7806 6V regulator modules (BOM #42), 1x 470uF electrolytic cap (BOM #48), 2x 3A glass fuses + holders (BOM #40-41).

**DO**:
1. Wire the **input** of the LM7806 module to the **12V ACTUATOR RAIL** (the one gated by the kill relay, after the 30A fuse).
2. Solder a **470uF electrolytic capacitor** on the output side of the regulator (+ to output, - to GND). This handles servo inrush current.
3. The output is your **6V servo bus**.

---

### 6.2 Verify 6V Output

**DO**:
1. Close the kill relay: flash SC firmware that drives GIO_A0 HIGH.
2. Measure voltage on the 6V regulator output.

| Check | Expected | Pass? |
|-------|----------|-------|
| 6V output with kill relay closed | 5.8V - 6.2V | [ ] |
| 6V output with kill relay open | 0V | [ ] |

---

### 6.3 Wire Steering Servo to FZC (Signal=PA0, Power=6V)

**GATHER**: 1x MG996R servo (BOM #21), 3A glass fuse + holder.

**DO**:
1. Identify servo wires: **Orange** = signal, **Red** = VCC, **Brown** = GND.
2. Wire **Orange (signal)** → FZC **PA0 (TIM2_CH1, CN7 pin 28)**. This is the PWM output.
3. Wire **Red (VCC)** → 6V servo bus, through a **3A fast-blow fuse**.
4. Wire **Brown (GND)** → star ground.

**Label**: "FZC STEERING SERVO (PA0, TIM2_CH1)".

---

### 6.4 Verify Steering Servo (50Hz PWM Sweep)

**DO**:
1. Ensure kill relay is closed (SC GIO_A0 = HIGH, so 12V flows to actuator rail → 6V regulator → servo).
2. Flash PWM test firmware on FZC: 50 Hz (20ms period), sweep pulse width from 1.0ms to 2.0ms.
3. The servo should sweep from 0 degrees to 180 degrees. Center is 1.5ms = 90 degrees.

| Check | Expected | Pass? |
|-------|----------|-------|
| Servo moves with PWM changes | Smooth rotation 0-180 degrees | [ ] |
| Center position at 1.5ms | Servo at ~90 degrees | [ ] |

---

### 6.5 Wire Brake Servo to FZC (Signal=PA1, Power=6V)

**GATHER**: 1x MG996R servo, 3A fuse + holder.

**DO**:
1. Wire **Orange (signal)** → FZC **PA1 (TIM2_CH2, CN7 pin 30)**.
2. Wire **Red (VCC)** → 6V servo bus, through a separate **3A fuse**.
3. Wire **Brown (GND)** → star ground.

**Label**: "FZC BRAKE SERVO (PA1, TIM2_CH2)".

---

### 6.6 Verify Brake Servo

Same test as 6.4 but on PA1/TIM2_CH2. Verify sweep 0-180 degrees.

---

### 6.7 Wire BTS7960 Motor Driver to RZC

**GATHER**: BTS7960 module (BOM #20), 2x 10k resistors (BOM #49), 16AWG wire (for motor power).

**DO**:
1. Wire **RPWM** input → RZC **PA8 (TIM1_CH1, CN10 pin 23)**.
2. Wire **LPWM** input → RZC **PA9 (TIM1_CH2, CN10 pin 21)**.
3. Wire **R_EN** input → RZC **PB0 (CN10 pin 31)**. Solder a **10k pull-down resistor** from this line to GND.
4. Wire **L_EN** input → RZC **PB1 (CN10 pin 24)**. Solder a **10k pull-down resistor** from this line to GND.
5. Wire **B+** → **12V ACTUATOR RAIL** (kill relay gated, after 30A fuse). Use 16AWG red wire.
6. Wire **B-** → **star ground**. Use 16AWG black wire.
7. Wire **VCC** (logic power on the BTS7960 module) → 3.3V or 5V as specified by the module (check your module documentation — some need 5V logic power, some accept 3.3V).

**Label each wire**: "RPWM", "LPWM", "R_EN", "L_EN", "B+", "B-".

**Critical**: The 10k pull-down resistors on R_EN and L_EN ensure the motor driver is **disabled** when the RZC MCU is in reset or the pins are floating. This is the primary hardware safety mechanism for preventing unintended motor operation.

---

### 6.8 Wire DC Motor to BTS7960 M+/M-

**GATHER**: 12V DC motor (BOM #19), 18AWG wire.

**DO**:
1. Wire motor terminal 1 to BTS7960 **M+** output.
2. Wire motor terminal 2 to BTS7960 **M-** output.
3. Use 18AWG wire (the motor can draw up to 25A peak stall current, but 18AWG is fine for short runs; 16AWG is better for runs >20cm).

---

### 6.9 Verify Motor (Low Duty First, Both Directions, Kill Relay Test)

**DO**:
1. Ensure kill relay is closed (SC GIO_A0 = HIGH).
2. Flash motor test firmware on RZC:
   - Set R_EN = HIGH, L_EN = LOW.
   - Apply **10% duty cycle** on RPWM (20 kHz frequency). Motor should spin slowly in one direction.
3. Stop. Set R_EN = LOW, L_EN = HIGH, apply 10% duty on LPWM. Motor spins the other direction.
4. Ramp up to 50% duty. Motor speeds up.
5. **Kill relay test**: While motor is spinning, drive SC GIO_A0 = LOW (or unplug SC USB). Motor should stop immediately as 12V is removed from the actuator rail.

| # | Check | Expected | Pass? |
|---|-------|----------|-------|
| 1 | Enables LOW, motor off | Motor does NOT spin | [ ] |
| 2 | R_EN HIGH + 10% RPWM | Motor spins slowly, direction 1 | [ ] |
| 3 | L_EN HIGH + 10% LPWM | Motor spins slowly, direction 2 | [ ] |
| 4 | 50% duty | Motor speeds up noticeably | [ ] |
| 5 | Kill relay opens | Motor stops immediately | [ ] |
| 6 | PWM frequency on scope | 20 kHz, clean edges | [ ] |

**WRONG**: If motor spins when enables are LOW: the 10k pull-down resistors are missing or disconnected. Fix: add pull-downs on R_EN and L_EN to GND.

---

## Step 7: Full Integration

> **Goal**: Firmware reads all sensors and controls all actuators over CAN. End-to-end pedal-to-motor working.

### 7.1 Flash Real Firmware on All 4 ECUs

**DO**: Flash the actual production ECU firmware (from completed firmware development phases):

| ECU | Firmware Functions |
|-----|--------------------|
| **CVC** | Pedal SPI reading → CAN TX torque request (0x101) + vehicle state (0x100) + E-stop monitoring (PC13) + OLED display |
| **FZC** | CAN RX commands → steering servo (PA0) + brake servo (PA1) + lidar (PA2/PA3) + buzzer + steering angle feedback |
| **RZC** | CAN RX torque request → BTS7960 motor PWM (PA8/PA9) + ACS723 current monitoring (PA0) + NTC temp (PA1/PA2) + battery V (PA3) |
| **SC** | CAN listen-only heartbeat monitoring → kill relay control (GIO_A0) + fault LEDs (GIO_A1-A4) + watchdog feed (GIO_A5) |

---

### 7.2 Run 10-Point Integration Test Sequence

| # | Test | How to Test | Pass Criteria | Pass? |
|---|------|-------------|---------------|-------|
| 1 | CAN heartbeats | candump on PC: look for 0x010 (CVC), 0x011 (FZC), 0x012 (RZC) | All 3 heartbeats at ~100ms intervals. SC listens only. | [ ] |
| 2 | Pedal → CAN | Push/rotate pedal magnet | CAN frame 0x101 (TorqueReq) changes value on candump | [ ] |
| 3 | CAN → Motor | Observe motor while pushing pedal | Motor speed matches pedal position (RZC receives TorqueReq, applies PWM) | [ ] |
| 4 | Lidar → CAN | Place object in front of TFMini-S | CAN frame with distance updates visible on candump | [ ] |
| 5 | Safety heartbeat timeout | Unplug CVC USB (simulates crash) | SC detects missing heartbeat → kill relay opens within FTTI, motor stops | [ ] |
| 6 | E-stop | Press E-stop button | CVC detects → sends emergency CAN → SC opens relay → motor stops | [ ] |
| 7 | Watchdog | Flash "hang after 5s" firmware on CVC | TPS3823 resets CVC → SC detects heartbeat gap → relay opens | [ ] |
| 8 | Fault LEDs | Unplug FZC USB | SC lights FZC fault LED (GIO_A2) + system LED (GIO_A4) | [ ] |
| 9 | OLED | Normal operation | CVC OLED shows: speed, torque %, system status | [ ] |
| 10 | Pi gateway | candump on Pi | All CAN traffic visible; MQTT publish to cloud if configured | [ ] |

---

### 7.3 Measure HSR Open Items (Relay Dropout, ACS723 Cal, TPS3823 POR)

These are open safety items that must be measured and documented during bring-up.

| ID | What to Measure | Tool | How | Expected | Actual | Document In |
|----|----------------|------|-----|----------|--------|-------------|
| HSR-O-001 | Kill relay dropout time | Oscilloscope | Ch1=GIO_A0, Ch2=actuator rail voltage. Trigger on GIO_A0 falling edge. Measure time from GIO_A0 LOW to actuator rail <1V. | < 10ms | ___ms | hw-safety-reqs.md |
| HSR-O-002 | ACS723 sensitivity at room temp | Multimeter + known load | Apply a known current (e.g., 1A using a resistive load). Read ADC. Calculate mV/A. | ~100 mV/A | ___mV/A | hw-safety-reqs.md |
| HSR-O-005 | TPS3823 POR pulse width | Oscilloscope | Trigger on power-up (3.3V rail rising edge). Measure NRST LOW duration. | >= 200ms | ___ms | hw-safety-reqs.md |

---

### 7.4 30-Minute Endurance Test

**DO**: Run the full system continuously for 30 minutes with:
- Pedal inputs cycling (manual or scripted via CAN from PC)
- Motor running at varying speeds (10% → 50% → 10% → 50%...)
- Monitor CAN bus: `candump -e can0` (the `-e` flag shows error frames)
- Monitor MCU temperatures via NTC readings
- Monitor for watchdog resets (should be ZERO)
- Monitor for CAN bus-off events (should be ZERO)

| Check | Expected | Actual | Pass? |
|-------|----------|--------|-------|
| CAN error frames in 30 min | 0 | ___ | [ ] |
| Watchdog resets in 30 min | 0 | ___ | [ ] |
| Motor temp NTC at end | < 60C (ADC > 0.66V) | ___C | [ ] |
| Board temp NTC at end | < 50C | ___C | [ ] |
| 12V rail voltage stability | 11.5-12.0V throughout | ___V | [ ] |
| 5V rail stability | 4.9-5.1V throughout | ___V | [ ] |
| 3.3V rail stability | 3.2-3.4V throughout | ___V | [ ] |

---

## Final Integration Checkpoint

| # | Milestone | Status |
|---|-----------|--------|
| 1 | Power: 12V/5V/3.3V rails stable, reverse-polarity protected | [ ] |
| 2 | Boards: All 4 ECUs boot from 12V (SC from USB) | [ ] |
| 3 | CAN: All 4 ECUs communicate at 500 kbps, 60 ohm termination | [ ] |
| 4 | Kill relay: Energize-to-run, fail-safe on SC power loss | [ ] |
| 5 | E-stop: NC button on CVC PC13, fail-safe on wire break | [ ] |
| 6 | Watchdogs: 4x TPS3823, each resets its MCU on hang | [ ] |
| 7 | Pedal sensors: Dual AS5048A on CVC SPI1, 14-bit angle | [ ] |
| 8 | Steering sensor: AS5048A on FZC SPI2 | [ ] |
| 9 | Lidar: TFMini-S on FZC USART2, distance readings | [ ] |
| 10 | Current sensor: ACS723 on RZC ADC PA0 | [ ] |
| 11 | Temperature: 2x NTC on RZC ADC PA1/PA2 | [ ] |
| 12 | Battery voltage: Divider on RZC ADC PA3 | [ ] |
| 13 | OLED: SSD1306 on CVC I2C1 PB8/PB9 | [ ] |
| 14 | Steering servo: MG996R on FZC PA0 TIM2_CH1, 6V power | [ ] |
| 15 | Brake servo: MG996R on FZC PA1 TIM2_CH2, 6V power | [ ] |
| 16 | Motor driver: BTS7960 on RZC PA8/PA9 TIM1, 12V actuator rail | [ ] |
| 17 | Fault LEDs: 4x LEDs on SC GIO_A1-A4 | [ ] |
| 18 | End-to-end: Pedal → CAN → Motor spins | [ ] |
| 19 | Safety chain: Heartbeat timeout → kill relay opens | [ ] |
| 20 | 30-minute endurance: Zero errors, stable temps | [ ] |

---

## Appendix A: Wire Color Code Reference

Per HWR-004:

| Color | Signal |
|-------|--------|
| Red | Power (+12V, +5V, +3.3V) |
| Black | Ground |
| Yellow | CAN_H |
| Green | CAN_L |
| Blue | SPI signals |
| White | UART signals |
| Orange | PWM signals |

---

## Appendix B: Nucleo Morpho Pin Quick Reference

The most-used morpho pins across all ECUs:

| Pin | CN Connector | Pin # | CVC Function | FZC Function | RZC Function |
|-----|-------------|-------|-------------|-------------|-------------|
| PA0 | CN7 | 28 | (unused) | Steer servo PWM | ADC current |
| PA1 | CN7 | 30 | (unused) | Brake servo PWM | ADC motor temp |
| PA2 | CN10 | 35 | (unused) | Lidar UART TX | ADC board temp |
| PA3 | CN10 | 37 | (unused) | Lidar UART RX | ADC battery V |
| PA4 | CN7 | 32 | Pedal CS1 | (unused) | BTS7960 IS_L |
| PA5 | CN10 | 11 | SPI1 SCK | (unused) | (unused) |
| PA6 | CN10 | 13 | SPI1 MISO | (unused) | (unused) |
| PA7 | CN10 | 15 | SPI1 MOSI | (unused) | (unused) |
| PA8 | CN10 | 23 | (unused) | (unused) | Motor RPWM |
| PA9 | CN10 | 21 | (unused) | (unused) | Motor LPWM |
| PA11 | CN10 | 14 | CAN RX | CAN RX | CAN RX |
| PA12 | CN10 | 12 | CAN TX | CAN TX | CAN TX |
| PA15 | CN7 | 17 | Pedal CS2 | (unused) | (unused) |
| PB0 | CN10 | 31 | WDT WDI | WDT WDI | Motor R_EN |
| PB1 | CN10 | 24 | (unused) | LED Red | Motor L_EN |
| PB4 | CN10 | 27 | LED Green | Buzzer | WDT WDI |
| PB5 | CN10 | 29 | LED Red | LED Green | LED Green |
| PB6 | CN10 | 17 | (unused) | (unused) | Encoder A |
| PB7 | CN7 | 21 | (unused) | (unused) | Encoder B |
| PB8 | CN10 | 3 | I2C SCL | (unused) | (unused) |
| PB9 | CN10 | 5 | I2C SDA | (unused) | (unused) |
| PB12 | CN10 | 16 | (unused) | Steer CS | (unused) |
| PB13 | CN10 | 30 | (unused) | SPI2 SCK | (unused) |
| PB14 | CN10 | 28 | (unused) | SPI2 MISO | (unused) |
| PB15 | CN10 | 26 | (unused) | SPI2 MOSI | BTS7960 IS_R |
| PC13 | CN7 | 23 | E-stop input | (unused) | (unused) |

---

## Appendix C: SC LaunchPad Pin Quick Reference

| Pin | Connector | SC Function |
|-----|-----------|-------------|
| DCAN1TX | J5 (edge) | CAN TX |
| DCAN1RX | J5 (edge) | CAN RX |
| GIO_A0 | J3-1 | Kill relay control |
| GIO_A1 | J3-2 | CVC fault LED |
| GIO_A2 | J3-3 | FZC fault LED |
| GIO_A3 | J3-4 | RZC fault LED |
| GIO_A4 | J3-5 | System fault LED |
| GIO_A5 | J3-6 | WDT feed (TPS3823 WDI) |
| GIO_B1 | J12 | Heartbeat LED (onboard) |

---

## Appendix D: Troubleshooting Quick Reference

| Symptom | Most Likely Cause | Fix |
|---------|------------------|-----|
| Board won't power from 12V | VIN not connected to CN7-24, or GND not connected | Check wiring to CN7-24 (VIN) and CN7-20 (GND) |
| CAN: candump shows nothing | TX/RX swapped on transceiver | Swap TXD↔RXD wires at the transceiver module |
| CAN: termination != 60 ohm | Missing or extra 120R resistor | Verify exactly 2 resistors: one at CVC end, one at SC end |
| CAN: rest voltage != 2.5V | Transceiver not powered | Check 3.3V at transceiver VCC pin |
| SPI sensor stuck at 0 | Wrong SPI mode | Must be Mode 1 (CPOL=0, CPHA=1) |
| SPI sensor stuck at 0x3FFF | CS not driving low, or MISO/MOSI swapped | Check CS pull-up present, check MISO↔MOSI wiring |
| UART lidar: no data | SB63/SB65 not removed on FZC | Remove solder bridges (Step 0.3) |
| ADC reads 0 everywhere | SB63/SB65 not removed on RZC (for PA2/PA3) | Remove solder bridges |
| Motor spins when it shouldn't | 10k pull-down missing on R_EN or L_EN | Add 10k pull-down to GND on both enable lines |
| Kill relay won't open | 10k gate pull-down missing on IRLZ44N | Add 10k from MOSFET gate to GND |
| Watchdog not resetting MCU | CT capacitor missing or wrong value | Verify 100nF on CT pin |
| Smoke / burning smell | Wrong voltage, reversed polarity, or short circuit | IMMEDIATELY turn off PSU. Check all connections before re-powering |
