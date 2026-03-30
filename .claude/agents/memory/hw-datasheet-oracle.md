# HW Datasheet Oracle — Accumulated Knowledge

## TMS570LC4357 (337ZWT Package) — DCAN1 Pin Mapping

### CRITICAL: DCAN1TX/DCAN1RX Are Dedicated Pins — No PINMUX Needed

- **DCAN1TX** = Ball **A10** (dedicated, not in PINMUX output mux table 6-1)
- **DCAN1RX** = Ball **B10** (dedicated, not in PINMUX output mux table 6-1)
- These balls do NOT appear in `HL_pinmux.h` at all — they are NOT multiplexed
- Source: SPNS195C Table 4-10 (ZWT Controller Area Network Controllers)
- Both are I/O with Pullup, Programmable 20uA, 2mA ZD drive

### DCAN1 Pins Are NOT on Balls C1 or C3

- **Ball C3** = MIBSPI3NCS[3] (default) | I2C1_SCL | N2HET1[29] | nTZ1_1
  - PINMUX[23] bits [23:16], shift=16
- **Ball C1** = GIOA[2] (default) | N2HET2[00] | eQEP2I
  - PINMUX[19] bits [7:0], shift=0
- Neither C1 nor C3 has any DCAN function available

### DCAN4 IS Multiplexed (for contrast)

- DCAN4TX on ball F2: `PINMUX_BALL_F2_DCAN4TX = 0x8 << shift` (PINMUX[20])
- DCAN4RX on ball W10: `PINMUX_BALL_W10_DCAN4RX = 0x8 << shift` (PINMUX[20])

### LaunchPad Connector Mapping (verified from sprr397.pdf schematic)

- J10 pin 44 = DCAN1RX (ball B10)
- J10 pin 45 = DCAN1TX (ball A10)
- WARNING: Old markdown summaries incorrectly listed J10.44/45 as FlexRay/GIOB

### Gotcha: HALCoGen Does Not Generate PINMUX Code for DCAN1

Since DCAN1TX/RX are dedicated pins, HALCoGen correctly does NOT generate
any PINMUX register writes for them. If DCAN1 isn't working, the problem
is NOT pin muxing — look at:
1. CAN transceiver VCC wiring
2. Transceiver standby pin (Rs)
3. HALCoGen mailbox direction (Dir bit in IF1ARB)
4. BE32 linker flag (--be32 for tiarmclang)

---
Last updated: 2026-03-27
