---
document_id: HWDES
title: "Hardware Design"
version: "1.0"
status: draft
aspice_process: HWE.2
date: 2026-02-21
---

## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.


# Hardware Design

## 1. Purpose

This document describes the hardware design for the Taktflow Zonal Vehicle Platform per Automotive SPICE 4.0 HWE.2 (Hardware Design). It provides system-level and per-ECU circuit descriptions, power distribution design, bus topology, grounding strategy, and key circuit schematics in ASCII form. This document translates the hardware requirements (HWR) and hardware safety requirements (HSR) into concrete circuit designs.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| HWREQ | Hardware Requirements | 1.0 |
| HSR | Hardware Safety Requirements | 1.0 |
| TSR | Technical Safety Requirements | 1.0 |
| SYSARCH | System Architecture | 1.0 |
| PIN-MAP | Pin Mapping | 1.0 |
| BOM | Bill of Materials | 1.0 |

## 3. Design Overview

| ECU | MCU | Key Peripherals | CAN Transceiver |
|-----|-----|-----------------|-----------------|
| CVC | STM32G474RE Nucleo-64 | FDCAN1, SPI1, I2C1, EXTI (PC13) | TJA1051T/3 |
| FZC | STM32G474RE Nucleo-64 | FDCAN1, SPI2, USART2, TIM2 | TJA1051T/3 |
| RZC | STM32G474RE Nucleo-64 | FDCAN1, TIM1, TIM4, ADC1 (4ch) | TJA1051T/3 |
| SC | TMS570LC43x LaunchPad | DCAN1, GIO (6 pins), RTI | SN65HVD230 |

---

## 4. System Block Diagram

```
  +========================================================================+
  |                         12V BENCH SUPPLY (10A)                         |
  +====+==============+============+============+============+=============+
       |              |            |            |            |
   [SB560 Schottky - Reverse Polarity Protection]
       |
       +-- [10A Fuse] -- 12V MAIN RAIL
       |
       +------+------+------+------+------+------+
       |      |      |      |      |      |      |
    LM2596  LM2596  Nucleo Nucleo Nucleo  LP     |
    12->5V  12->3.3V LDO   LDO    LDO   LDO    |
     |       |      (CVC)  (FZC)  (RZC)  (SC)   |
     |       |       |      |      |      |      |
    5V      3.3V    3.3V   3.3V   3.3V   3.3V   |
    Rail    Rail     |      |      |      |      |
     |       |       |      |      |      |      |
   TFMini  AS5048A  CVC    FZC    RZC    SC     |
   RPi4    ACS723   MCU    MCU    MCU    MCU    |
           TJA1051                SN65HVD         |
           TPS3823                                |
           NTC divs                               |
                                                  |
       +--[Kill Relay (30A SPST-NO)]--[30A Fuse]--+
       |
       +-- 12V ACTUATOR RAIL (gated)
       |
       +------+------+------+
       |      |      |      |
    BTS7960  Steer  Brake  (Relay coil
    Motor    Servo  Servo   is on 12V
    Driver   6V reg 6V reg  main rail)
     |
    DC Motor (12V)

  ==========+===========+===========+===========+=====  CAN BUS (500 kbps)
            |           |           |           |
       +----+----+ +----+----+ +----+----+ +----+----+
       |  CVC    | |  FZC    | |  RZC    | |   SC    |
       | TJA1051 | | TJA1051 | | TJA1051 | | SN65HVD |
       | [120R]  | |         | |         | | [120R]  |
       +---------+ +---------+ +---------+ +---------+
                                                |
                                           CANable 2.0
                                           (RPi USB)
                                                |
                                         Raspberry Pi 4
                                                |
                                           MQTT/TLS
                                                |
                                            AWS Cloud
                                         (IoT Core ->
                                          Timestream ->
                                           Grafana)
```

---

## 5. Per-ECU Design

### 5.1 CVC -- Central Vehicle Computer

#### 5.1.1 CAN Transceiver Circuit

```
  STM32G474RE                TJA1051T/3 Module
  +-----------+              +------------------+
  |           |              |                  |
  | FDCAN1_TX +--- PA12 --->| TXD         CANH |--+--[CMC]--+-- CAN_H (yellow)
  | (AF9)     |              |                  |  |         |
  | FDCAN1_RX +--- PA11 ---<| RXD         CANL |--+--[CMC]--+-- CAN_L (green)
  | (AF9)     |              |                  |  |         |
  +-----------+  3.3V --+-->| VCC          GND |--+-- GND   +--[120R]--+
                        |   |                  |                       |
                    [100nF] | STB          N/C |                    CAN_L
                        |   +------------------+
                       GND        |
                                 GND (STB tied LOW = always active)

  CMC = Common-mode choke (100 uH minimum)
  TVS diodes (PESD1CAN) placed between CMC and bus connectors
  120R termination resistor at CVC end of bus
```

#### 5.1.2 Dual Pedal Sensor SPI Circuit

```
  STM32G474RE                  AS5048A #1              AS5048A #2
  +-----------+                +----------+            +----------+
  |           |                |          |            |          |
  | SPI1_SCK  +--- PA5 ------>| CLK      |    +------>| CLK      |
  | (AF5)     |                |          |    |       |          |
  | SPI1_MISO +--- PA6 ---<---| DO   VDD |--[100nF]   | DO   VDD |--[100nF]
  | (AF5)     |                |      |   |   |       |      |   |   |
  | SPI1_MOSI +--- PA7 ------>| DI   GND |   |  +--->| DI   GND |   |
  | (AF5)     |                |          |   |  |    |          |   |
  |           |                |     CSn  |   |  |    |     CSn  |   |
  |   GPIO    +--- PA4 ------>|  (act-lo) |   |  |    |  (act-lo) |  |
  |           |     |          +----------+   |  |    +----------+   |
  |   GPIO    +--- PA15 ------+---------------+--+--->               |
  |           |     |                                                |
  +-----------+     |                                                |
                [10k to 3.3V] pull-up on PA4                     [10k to 3.3V]
                                                                 pull-up on PA15

  Both sensors share SPI1 bus (SCK, MISO, MOSI).
  Independent chip-selects (PA4, PA15) with 10k pull-ups.
  Sensors powered from external 3.3V rail with local 100nF decoupling each.
  Magnetic target: diametric magnet on pedal shaft, common to both sensors.
```

#### 5.1.3 OLED Display I2C Circuit

```
  STM32G474RE                    SSD1306 128x64 OLED
  +-----------+                  +-------------------+
  |           |                  |                   |
  | I2C1_SCL  +--- PB8 ---+---->| SCL      VCC(3.3V)|--[10uF]--[100nF]--3.3V
  | (AF4)     |            |     |                   |
  | I2C1_SDA  +--- PB9 --+|-+-->| SDA          GND  |-- GND
  | (AF4)     |           || |   |                   |
  +-----------+           || |   | RST               |-- GPIO (optional)
                          || |   | ADDR = 0x3C       |
                       [4.7k]|   +-------------------+
                          |[4.7k]
                       3.3V  3.3V

  Pull-ups: 4.7k ohm on SCL and SDA to 3.3V (external).
  10uF bulk capacitor for display inrush current.
  I2C speed: 400 kHz (Fast Mode).
  Address: 0x3C (7-bit), fixed on SSD1306 module.
```

#### 5.1.4 E-Stop Button Circuit

```
  3.3V (internal pull-up on PC13, ~40k)
    |
    +--- PC13 (EXTI, rising-edge) ---[10k series R]---+--- [100nF] --- GND
    |                                                  |    (RC debounce)
    |                                              [TVS 3.3V]
    |                                                  |
    +--- [NC Push Button] --- GND                     GND

  Resting state: button closed -> PC13 = LOW (connected to GND through button)
  Button pressed: button opens -> PC13 = HIGH (pulled up by internal pull-up)
  Broken wire: PC13 = HIGH (E-stop activated) -- FAIL-SAFE

  Rising edge triggers EXTI interrupt.
  RC debounce: tau = 10k * 100nF = 1 ms.
  TVS diode protects PC13 from static on button wiring.
```

#### 5.1.5 CVC Watchdog Circuit

```
                    TPS3823DBVR
                    +-----------+
  3.3V --[100nF]--->| VDD    MR |--- 3.3V (MR tied high = no manual reset)
                    |           |
  PB0 (GPIO) ----->| WDI  RESET|---[100nF]--- STM32 NRST
                    |           |         |
                    | CT    GND |       (push-pull, active-low)
                    +---+-------+
                        |
                     [100nF] (sets timeout = 1.6 sec)
                        |
                       GND

  Firmware toggles PB0 at regular intervals (conditioned on self-checks).
  If PB0 stops toggling for 1.6 sec, TPS3823 pulls RESET low -> MCU resets.
  TPS3823 internal oscillator is independent of STM32 clock.
```

#### 5.1.6 CVC Power Supply Design

```
  12V Main Rail
       |
  [Nucleo Vin pin] ---> Nucleo onboard LDO (LD39050) ---> 3.3V (MCU power)
       |
       +--- 5V (Nucleo onboard USB regulator, if USB connected)

  External 3.3V rail ---> SPI sensors (AS5048A x2)
                     ---> CAN transceiver (TJA1051T/3)
                     ---> TPS3823 watchdog
                     ---> I2C OLED (SSD1306)
                     ---> Status LEDs

  Power domains are separated:
  - MCU powered from Nucleo onboard LDO (independent of external 3.3V rail)
  - Peripherals powered from external 3.3V rail
  - No connection between MCU 3.3V and external 3.3V (separate ground paths merge at star ground point)
```

---

### 5.2 FZC -- Front Zone Controller

#### 5.2.1 CAN Transceiver Circuit

Same schematic as CVC (Section 5.1.1) using FDCAN1 on PA12 (TX) and PA11 (RX) with TJA1051T/3. No 120 ohm termination resistor on FZC (FZC is not at a bus end).

#### 5.2.2 Steering Angle Sensor SPI Circuit

```
  STM32G474RE                  AS5048A (Steering)
  +-----------+                +----------+
  |           |                |          |
  | SPI2_SCK  +--- PB13 ----->| CLK      |
  | (AF5)     |                |          |
  | SPI2_MISO +--- PB14 --<---| DO   VDD |--[100nF]-- 3.3V
  | (AF5)     |                |      |   |
  | SPI2_MOSI +--- PB15 ----->| DI   GND |-- GND
  | (AF5)     |                |          |
  |   GPIO    +--- PB12 ----->| CSn      |
  |           |     |          +----------+
  +-----------+ [10k to 3.3V]

  Single sensor on SPI2 bus (SPI2 avoids SB21 solder bridge conflict on Nucleo-64).
  Sensor mounted on steering servo output shaft.
  SPI cable routed with 10mm separation from servo power lines.
```

#### 5.2.3 Steering Servo PWM Circuit

```
  STM32G474RE                       MG996R Steering Servo
  +-----------+                     +------------------+
  |           |                     |                  |
  | TIM2_CH1  +--- PA0 ----------->| Signal (Orange)  |
  | (AF1)     |     (3.3V PWM,     |                  |
  |           |      50 Hz)        | VCC (Red)        |---+
  +-----------+                    |                  |   |
                                   | GND (Brown)      |---+---GND
                                   +------------------+   |
                                                          |
                                              [6V Regulator]---[3A Fuse]---12V Actuator Rail
                                                          |
                                                      [470uF]
                                                          |
                                                         GND

  PWM: 50 Hz, 1.0-2.0 ms pulse width.
  Servo powered from 6V regulated rail (from 12V actuator bus via kill relay).
  3A fast-blow fuse protects against servo short circuit.
  470uF bulk cap on 6V rail for servo inrush.
```

#### 5.2.4 Brake Servo PWM Circuit

Same topology as steering servo (Section 5.2.3) using TIM2_CH2 on PA1. Separate 3A fuse. Shares the 6V regulated rail with the steering servo (regulator rated for 4A total).

#### 5.2.5 TFMini-S Lidar UART Circuit

```
  STM32G474RE                   TFMini-S Lidar Sensor
  +-----------+                 +------------------+
  |           |                 |                  |
  | USART2_TX +--- PA2 ------->| RX (Green)       |
  | (AF7)     |                 |                  |
  | USART2_RX +--- PA3 ---[TVS]<--| TX (White)    |
  | (AF7)     |                 |                  |
  +-----------+            5V-->| VCC (Red)        |
                                |                  |
                           GND->| GND (Black)      |
                                +------------------+

  UART: 115200 baud, 8N1.
  TFMini-S TX is 3.3V LVTTL (direct connect, no level shifter).
  TFMini-S power: 5V from external 5V rail with 470uF bulk cap.
  TVS diode (3.3V bidirectional) on USART2_RX for ESD protection.
  4-pin keyed connector prevents reverse polarity.
  DMA enabled on USART2_RX for background frame reception.
```

#### 5.2.6 Buzzer Driver Circuit

```
  3.3V
    |
  [Active Piezo Buzzer (85dB @ 10cm)]
    |
  Drain -- [2N7002 N-MOSFET] -- Source -- GND
                |
             Gate
                |
           [10k pulldown to GND]
                |
  PB4 (GPIO) ---+

  GPIO HIGH = buzzer ON, GPIO LOW = buzzer OFF.
  10k pulldown ensures buzzer OFF on MCU reset (gate floating = OFF).
  Active buzzer: self-oscillating, no PWM tone generation required.
  Schottky diode (1N5819) across buzzer for inductive kickback.
```

#### 5.2.7 FZC Watchdog Circuit

Same as CVC (Section 5.1.5) using PB0 as WDI GPIO.

---

### 5.3 RZC -- Rear Zone Controller

> **Note**: On the RZC, the watchdog WDI pin is PB4 (not PB0). PB0 is used for BTS7960 R_EN on this ECU. See Section 5.3.7.

#### 5.3.1 CAN Transceiver Circuit

Same schematic as CVC (Section 5.1.1) using FDCAN1 on PA12 (TX) and PA11 (RX) with TJA1051T/3. If RZC is at a physical bus end, include 120 ohm termination.

#### 5.3.2 BTS7960 H-Bridge Motor Driver Interface

```
  STM32G474RE                    BTS7960 Module
  +-----------+                  +------------------+
  |           |                  |                  |
  | TIM1_CH1  +--- PA8 -------->| RPWM             |
  | (AF6)     |   (20 kHz PWM)  |                  |
  | TIM1_CH2  +--- PA9 -------->| LPWM             |
  | (AF6)     |   (20 kHz PWM)  |         M+  M-   |---[Motor]
  |           |                  |                  |
  |   GPIO    +--- PB0 -------->| R_EN             |
  |           |     |            |                  |
  |   GPIO    +--- PB1 -------->| L_EN             |
  |           |     |            |                  |
  |  ADC1_CH3 +--- PA3 ---<-----| R_IS (current)   |
  |           |                  |                  |
  |  ADC1_CH4 +--- PA4 ---<-----| L_IS (current)   |
  |           |                  |                  |
  +-----------+                  | B+  B-           |
                                 +--+-----+---------+
                                    |     |
                                12V Act.  GND
                                Rail

  R_EN, L_EN: 10k pull-down resistors to GND (fail-safe disabled on reset).
  RPWM, LPWM: 20 kHz PWM (above audible), hardware dead-time = 10 us minimum.
  Motor rated 12V, 25A peak (stall).
  BTS7960 VCC from 12V actuator rail (through kill relay and 30A fuse).
  IS_R, IS_L: current sense outputs for backup monitoring (ACS723 is primary).
```

#### 5.3.3 ACS723 Current Sensor Circuit

```
                    ACS723LLCTR-20AB-T
                    +------------------+
  Motor supply ---->| IP+          VCC |--- 3.3V ---[100nF]
  (from BTS7960)    |                  |
  Motor return ---<-| IP-         VIOUT|---[1nF]---+--- PA0 (ADC1_CH1)
                    |                  |           |
                    |              GND |     [100nF filter]
                    +------------------+           |
                                                  GND

  Galvanic isolation between high-current path (IP+/IP-) and signal output.
  Sensitivity: 100 mV/A (ACS723LLCTR-20AB-T, 20A variant).
  Zero-current output: VCC/2 = 1.65V (bidirectional sensing).
  Full-scale: +/-20A = 1.65V +/- 2.0V = -0.35V to 3.65V (clamped by VCC to 0-3.3V).
  Practical range: +/-16.5A before output saturates at 0V/3.3V.
  For 25A motor: use BTS7960 IS pins as backup for currents > 16.5A.
  1nF capacitor: bandwidth filter (80 kHz).
  100nF capacitor: anti-aliasing at ADC input (1.6 kHz cutoff).
  Zener clamp (BZX84C3V3) across ADC input for overvoltage protection.
```

#### 5.3.4 NTC Temperature Sensing Circuit

```
  3.3V
    |
  [10k, 1% precision fixed resistor]
    |
    +--- PA1 (ADC1_CH2) ---[100nF]--- GND
    |
  [10k NTC (B=3950)]
    |
   GND

  Voltage divider: V_out = 3.3V * R_NTC / (R_fixed + R_NTC)
  At 25C: R_NTC = 10k, V_out = 1.65V (midscale)
  At 60C: R_NTC = 2.49k, V_out = 0.66V
  At 100C: R_NTC = 0.68k, V_out = 0.21V
  Open NTC: V_out = 3.3V (reads as < -30C -> sensor fault)
  Short NTC: V_out = 0V (reads as > 150C -> sensor fault)

  Second NTC on PA2 (ADC1_CH3) with identical circuit for board temperature.
```

#### 5.3.5 Battery Voltage Monitoring Circuit

```
  12V Battery Bus
    |
  [47k, 1% resistor]
    |
    +--- PA3 (ADC1_CH4) ---[100nF]--- GND
    |                           |
  [10k, 1% resistor]       [BZX84C3V3 Zener]
    |                           |
   GND                        GND

  Voltage divider: V_out = V_bat * 10k / (47k + 10k) = V_bat * 0.175
  At 12V: V_out = 2.11V
  At 14.4V: V_out = 2.53V
  At 9.6V: V_out = 1.68V
  Max input before Zener clamps: 18.8V (Zener triggers at 3.3V output)
```

#### 5.3.6 Motor Encoder Interface

```
  Motor Encoder (open-collector outputs)
  +----------+
  |          |
  | Ch A     |---[10k to 3.3V]---[TVS 3.3V]--- PB6 (TIM4_CH1, AF2)
  |          |
  | Ch B     |---[10k to 3.3V]---[TVS 3.3V]--- PB7 (TIM4_CH2, AF2)
  |          |
  | VCC      |--- 5V or 3.3V (per encoder spec)
  |          |
  | GND      |--- GND
  +----------+

  TIM4 configured in encoder mode (both edges, 4x counting).
  Hardware input filter ICF = 0x0F (maximum, 8 samples at fDTS/32).
  10k pull-ups required for open-collector encoder outputs.
  TVS diodes for ESD protection (motor is a noise source).
```

#### 5.3.7 RZC Watchdog Circuit

Same as CVC (Section 5.1.5) but using **PB4** as WDI GPIO (PB0 is used for BTS7960 R_EN on RZC).

---

### 5.4 SC -- Safety Controller

#### 5.4.1 DCAN1 CAN Interface (Normal Mode, SWR-SC-029)

```
  TMS570LC43x                       SN65HVD230
  +-----------+                      +------------------+
  |           |                      |                  |
  | DCAN1_TX  +-- J10 pin 45 (TX) -->| TXD         CANH |--[CMC]--+-- CAN_H
  |           |                      |                  |         |
  | DCAN1_RX  +-- J10 pin 44 (RX) --<| RXD         CANL |--[CMC]--+-- CAN_L
  |           |                      |                  |         |
  +-----------+  3.3V --[100nF]----->| VCC          GND |  [120R termination]
                                     |                  |         |
                                     | Rs           N/C |       CAN_L
                                     +--+---------------+
                                        |
                                       GND (Rs = GND for full speed)

  DCAN1 runs in normal operation (TEST register NOT modified, SWR-SC-029).
  SC transmits SC_Status (CAN ID 0x013) via mailbox 7 every 500ms.
  SC receives heartbeat/state frames on mailboxes 1-6.
  SN65HVD230 is TI part (same vendor as TMS570).
  120R termination at SC (SC is at one end of the bus).
  J10 pin 44 = DCAN1RX, J10 pin 45 = DCAN1TX (verified from sprr397.pdf p.9, coord analysis).
  NOT DCAN4 — HALCoGen v04.07.01 mailbox bug on DCAN4.
```

#### 5.4.2 Kill Relay Circuit

<!-- DECISION: ADR-003 -- Energize-to-run relay pattern -->

```
  12V Main Rail
    |
    +---[Relay Coil (12V, 70-120 ohm)]---+
    |                                      |
    |  [1N4007 flyback diode]              |
    |  cathode (+) ---- anode (-)          |
    |     |                |               |
    +-----+                +--- Drain      |
                                |          |
                           [IRLZ44N       ]
                           [ N-MOSFET     ]
                                |          |
                           Source --- GND  |
                                |          |
                           Gate            |
                                |          |
                          [100R series]    |
                                |          |
  SC GIO_A0 ----+--- [10k pulldown] --- GND
                |
            (3.3V output)

  Relay Contact (SPST-NO):
  +-----------+         +-----------+
  | 12V Main  |---NO---+---CLOSED--| 12V Actuator Rail
  | Rail      |   (open when       | (to BTS7960, servos)
  +-----------+    de-energized)   +-----------+
                                        |
                                    [30A Fuse]

  Operation:
  - GIO_A0 = HIGH -> MOSFET ON -> Coil energized -> Relay CLOSED -> Actuator power ON
  - GIO_A0 = LOW  -> MOSFET OFF -> Coil de-energized -> Relay OPEN -> Actuator power OFF
  - SC power loss -> Gate floats -> 10k pulldown -> MOSFET OFF -> SAFE STATE
  - SC firmware hang -> TPS3823 reset -> GIO_A0 resets to input -> 10k pulldown -> SAFE STATE

  Dropout timing: < 10 ms from GIO_A0 LOW to relay contacts open.
  1N4007 flyback diode limits coil back-EMF to ~0.7V above rail.
  IRLZ44N: VGS(th) < 2V (driven directly by 3.3V GIO output).
  100R gate resistor limits dI/dt during switching.
  10k pulldown ensures OFF state during all fault conditions.
```

#### 5.4.3 Fault LED Circuit

```
  SC GIO Pins                    LEDs

  GIO_A1 ---[330R]--- LED_CVC (Red, 3mm)    --- GND
  GIO_A2 ---[330R]--- LED_FZC (Red, 3mm)    --- GND
  GIO_A3 ---[330R]--- LED_RZC (Red, 3mm)    --- GND
  GIO_A4 ---[330R]--- LED_SYS (Amber, 3mm)  --- GND

  GIO_B1 ---[330R]--- LED_HB (Green, 3mm)   --- GND  (heartbeat/status)

  LED current: I = (3.3V - 1.8V) / 330R = 4.5 mA (sufficient for visibility).
  TMS570 GIO pins can source up to 8 mA per pin (4.5 mA is within limits).
  All LEDs OFF at power-up (GIO pins initialized as inputs = high-impedance).
  After init, GIO pins configured as outputs, initialized LOW (LEDs OFF).
  Lamp test during startup: all LEDs ON for 500 ms.
```

#### 5.4.4 SC External Watchdog Circuit

```
                    TPS3823DBVR
                    +-----------+
  3.3V --[100nF]--->| VDD    MR |--- 3.3V
                    |           |
  GIO_A5 (GPIO) -->| WDI  RESET|---[100nF]--- TMS570 nRST
                    |           |
                    | CT    GND |
                    +---+-------+
                        |
                     [100nF]
                        |
                       GND

  Same configuration as STM32 ECU watchdogs.
  TPS3823 provides independent reset mechanism for faults not caught by lockstep.
  Lockstep detects computation errors; TPS3823 detects hang/timing faults.
```

#### 5.4.5 SC Power Supply Design

```
  12V Main Rail (NOT through kill relay)
       |
  [Schottky diode - reverse polarity protection]
       |
  [LM1117-3.3 Linear Regulator]
       |
  3.3V (SC dedicated rail)
       |
       +--- TMS570LC43x (via LaunchPad Vin or direct 3.3V header)
       +--- SN65HVD230 CAN transceiver
       +--- TPS3823 watchdog
       +--- Fault LEDs (5x, through current-limiting resistors)
       +--- Kill relay MOSFET gate circuit

  ALTERNATIVELY: SC powered via LaunchPad USB (5V USB -> onboard 3.3V LDO).
  The LaunchPad includes an onboard XDS110 debug probe with USB power.
  For bench operation, USB power is the simplest approach.

  CRITICAL: SC power is independent of the kill relay.
  When kill relay opens, SC remains powered to:
    - Log DTCs
    - Maintain fault LED indication
    - Continue watchdog feed (if firmware is healthy)
```

---

## 6. CAN Bus Design

### 6.1 Bus Topology

```
  [CVC]----100mm----[FZC]----100mm----[RZC]----100mm----[SC]
   |                                                      |
  [120R]                                                [120R]
  term.                                                 term.

  Bus stubs: < 100 mm from main trunk to each ECU transceiver
  Total bus length: ~300 mm main trunk + stubs = ~700 mm total
  (well within 2 m maximum specified in HWR-014)

  Additional taps (T-connection, < 100 mm stub):
  - CANable 2.0 #1: connected to Raspberry Pi 4 (USB)
  - CANable 2.0 #2: connected to development PC (USB) for CAN analyzer
```

### 6.2 CAN Connector Plan

Each ECU connects to the CAN bus via a screw terminal block:

| Terminal | Signal | Wire Color |
|----------|--------|------------|
| 1 | CAN_H | Yellow |
| 2 | CAN_L | Green |
| 3 | GND (CAN reference) | Black |

The CAN bus uses a daisy-chain wiring pattern: the main trunk wire runs along the base plate edge, with 3-position screw terminal blocks at each ECU location. The trunk wire is continuous (not broken at each terminal block -- wire-in, wire-out through the same terminal).

### 6.3 Bit Timing Configuration

| Parameter | STM32G474RE (FDCAN) | TMS570LC43x (DCAN) |
|-----------|--------------------|--------------------|
| Input clock | 170 MHz (PCLK1) | 75 MHz (VCLK1, after PLL) |
| Prescaler | 34 | 15 |
| Nominal bit rate clock | 5 MHz | 5 MHz |
| Time quanta per bit | 10 | 10 |
| TSEG1 | 7 tq | 7 tq |
| TSEG2 | 2 tq | 2 tq |
| SJW | 2 tq | 2 tq |
| Sample point | 80% | 80% |
| Actual bit rate | 500.000 kbps | 500.000 kbps |

Both controllers use the same number of time quanta and sample point to ensure identical bit timing on the bus.

---

## 7. Power Distribution Design

### 7.1 Power Distribution Diagram

```
  [12V Bench PSU 10A]
         |
    [SB560 Schottky] -- reverse polarity protection
         |
    [10A ATC Fuse] -- main rail fuse
         |
    ===== 12V MAIN RAIL (16 AWG) =====
         |         |         |         |         |         |
     [LM2596]  [LM2596]  [Nucleo]  [Nucleo]  [Nucleo]  [LaunchPad]
     12->5V    12->3.3V   Vin(CVC)  Vin(FZC)  Vin(RZC)  Vin(SC)
      |          |
    5V Rail    3.3V Rail
    (3A PTC)   (1A PTC)
      |          |
    TFMini     Sensors
    RPi4       Transceivers
               Watchdogs
               LEDs
         |
    [Kill Relay Coil Circuit] (SC controlled)
         |
    [30A ATC Fuse]
         |
    ===== 12V ACTUATOR RAIL (16 AWG) =====
         |              |              |
    [BTS7960]       [6V Reg #1]    [6V Reg #2]
    Motor Driver    Steering Servo  Brake Servo
    (25A peak)      (2A peak)       (2A peak)
         |              |              |
    [DC Motor]     [MG996R]        [MG996R]
                   [3A Fuse]       [3A Fuse]
```

### 7.2 Total System Power Budget

| Domain | Voltage | Current (typical) | Current (peak) | Source |
|--------|---------|-------------------|----------------|--------|
| CVC logic | 3.3V | 230 mA | 250 mA | Nucleo LDO |
| FZC logic | 3.3V | 194 mA | 220 mA | Nucleo LDO |
| RZC logic | 3.3V | 162 mA | 180 mA | Nucleo LDO |
| SC logic | 3.3V | 150 mA | 200 mA | LaunchPad LDO |
| External 3.3V rail | 3.3V | 74 mA | 100 mA | LM2596 buck |
| External 5V rail | 5V | 700 mA | 1340 mA | LM2596 buck |
| Motor (12V actuator rail) | 12V | 5 A | 25 A | Kill relay |
| Servos (6V from 12V act.) | 6V | 1 A | 5 A | 6V regulators |
| Kill relay coil | 12V | 100 mA | 170 mA | Main rail |
| **Total from 12V supply** | **12V** | **~3.5 A** | **~8.5 A** | |

Peak current (8.5A) is within the 10A PSU rating. The motor stall current (25A) is transient and lasts less than 100 ms before the overcurrent protection triggers.

---

## 8. Grounding Strategy

### 8.1 Star Ground Topology

```
  [Star Ground Point] (single screw terminal or copper bus bar on base plate)
         |
    +----+----+----+----+----+----+----+
    |    |    |    |    |    |    |    |
   PSU  CVC  FZC  RZC   SC  Motor Servo
   GND  GND  GND  GND  GND  GND   GND
              |
         [CAN GND]
         (common ref)
```

### 8.2 Ground Plane and Separation Rules

1. **Star ground**: All ground returns converge at a single star ground point (screw terminal or bus bar). This prevents ground loops and ensures all ECUs share the same ground reference.

2. **Analog/digital ground separation**: On each ECU's protoboard, the analog ground (ADC reference, sensor returns) and digital ground (MCU, CAN transceiver) shall be separated by at least 5 mm and connected at a single point near the MCU's VSS/VSSA pins.

3. **ACS723 ground reference**: The ACS723 analog output ground (GND pin) shall be connected directly to the RZC's analog ground, with a short, low-impedance trace. The ACS723 power path ground (IP- pin) connects to the motor power ground, which is a separate high-current path.

4. **Motor power ground**: The motor power return (from BTS7960 B- to PSU GND) shall use 16 AWG wire and connect directly to the star ground point. This high-current path shall not pass through any signal ground connections.

5. **CAN bus ground**: A CAN reference ground wire (CAN_GND) shall run alongside the CAN_H/CAN_L twisted pair. This provides a common-mode reference for all CAN transceivers. CAN_GND connects to the star ground point.

6. **Decoupling ground returns**: All decoupling capacitor grounds shall connect to the local ground plane/wire within 5 mm of the capacitor.

---

## 9. Design Decisions

### 9.1 Key Design References

| Decision | ADR Reference | Rationale |
|----------|--------------|-----------|
| Zonal architecture (4-ECU physical) | ADR-001 | Modern E/E pattern, fewer ECUs, more resume impact |
| STM32G474RE for zone controllers | ADR-002 | Cortex-M4F, FDCAN, sufficient pins, good tooling |
| TMS570LC43x for safety controller | ADR-003 | Lockstep Cortex-R5F, TUV-certified safety manual |
| Energize-to-run kill relay pattern | ADR-003 | Gold standard for safety-critical power cutoff |
| TJA1051T/3 for STM32 CAN transceivers | ADR-004 | 3.3V native, CAN FD capable, ISO 11898-2 |
| SN65HVD230 for TMS570 CAN transceiver | ADR-004 | TI vendor consistency, 3.3V, proven with DCAN |
| DCAN1 (not DCAN4) for TMS570 | ADR-005 | HALCoGen v04.07.01 mailbox bug on DCAN4 |
| TPS3823 external watchdog | ADR-006 | Independent oscillator, push-pull reset, SOT-23-5 |
| AS5048A magnetic angle sensors | ADR-007 | 14-bit, SPI, 10 MHz max, contactless |
| BTS7960 H-bridge module | ADR-008 | 43A rated, 3.3V logic, built-in current sense |

### 9.2 Design Trade-offs

| Trade-off | Chosen | Alternative | Why Chosen |
|-----------|--------|-------------|------------|
| CAN transceiver for STM32 | TJA1051T/3 (NXP, 3.3V) | MCP2551 (Microchip, 5V) | 3.3V native eliminates level shifter |
| Motor current sensor | ACS723 (Hall) + BTS7960 IS (built-in) | Shunt resistor + INA219 | Galvanic isolation, simpler circuit |
| Servo voltage | 6V regulated from 12V act. rail | Direct 5V from 5V rail | Higher torque at 6V, separate from logic power |
| E-stop wiring | NC button (fail-safe) | NO button (simpler) | Wire break = E-stop activated (fail-safe) |
| MCU power | Nucleo onboard LDO | External 3.3V regulator | Simplicity, Nucleo is a dev board with built-in regulation |

---

## 10. Assembly Notes

### 10.1 Assembly Order

1. Mount all Nucleo and LaunchPad boards on the base plate with standoffs.
2. Install the kill relay, E-stop button, and fault LED panel.
3. Wire the 12V main rail (16 AWG) from PSU entry point to distribution terminals.
4. Install and wire the 5V and 3.3V buck converter modules.
5. Wire the kill relay circuit (MOSFET, flyback diode, gate resistor, pulldown).
6. Wire the CAN bus trunk (twisted pair) along the base plate edge with 120 ohm terminators.
7. Connect CAN transceivers to each ECU and to the bus trunk via screw terminals.
8. Wire sensor interfaces (SPI, UART, ADC) with color-coded cables.
9. Wire actuator interfaces (PWM servos, BTS7960 motor driver).
10. Install watchdog ICs and connect WDI/RESET lines to each MCU.
11. Install protection components (TVS diodes, Zener clamps, fuses).
12. Connect the Raspberry Pi and CANable 2.0 to the CAN bus.
13. Perform continuity check on all connections before applying power.

### 10.2 First Power-Up Checklist

1. Verify 12V main rail voltage at each ECU (expected: 11.5-12.5V after Schottky drop).
2. Verify 5V rail voltage (expected: 4.9-5.1V).
3. Verify 3.3V rail voltage (expected: 3.2-3.4V).
4. Verify kill relay is OPEN (no 12V on actuator rail) before SC firmware runs.
5. Verify each Nucleo board boots via USB (LED blinks).
6. Verify LaunchPad boots via USB (XDS110 LED).
7. Verify CAN bus termination resistance (60 ohm between CAN_H and CAN_L, all nodes off).
8. Verify E-stop button: pressing button causes PC13 to go HIGH.
9. Verify TPS3823 power-on reset pulse on each MCU NRST pin (oscilloscope).
10. Verify kill relay closes when SC drives GIO_A0 HIGH (12V appears on actuator rail).

---

## 11. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete HW design: system block diagram, per-ECU circuit descriptions (CVC/FZC/RZC/SC), CAN bus design with bit timing, power distribution, kill relay circuit, grounding strategy, assembly notes |

