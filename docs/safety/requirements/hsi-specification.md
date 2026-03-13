---
document_id: HSI
title: "Hardware-Software Interface Specification"
version: "1.0"
status: draft
iso_26262_part: 4
iso_26262_clause: "7"
aspice_process: SYS.3
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

## Lessons Learned Rule

Every interface specification element in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/safety/lessons-learned/`](../lessons-learned/). One file per interface specification element. File naming: `HSI-<interface>.md`.


# Hardware-Software Interface Specification

## 1. Purpose and Scope

This document specifies the Hardware-Software Interface (HSI) for each physical ECU of the Taktflow Zonal Vehicle Platform per ISO 26262-4 Clause 7 and Automotive SPICE 4.0 SYS.3. The HSI defines what the hardware provides to the software and what the software expects from the hardware, covering pin mappings, peripheral configurations, electrical characteristics, timing constraints, memory maps, startup sequences, and safety-relevant hardware features.

This document covers the 4 physical ECUs:
- CVC -- Central Vehicle Computer (STM32G474RE)
- FZC -- Front Zone Controller (STM32G474RE)
- RZC -- Rear Zone Controller (STM32G474RE)
- SC -- Safety Controller (TMS570LC43x)

Simulated ECUs (BCM, ICU, TCU) run on Linux and do not have a hardware-software interface in the traditional sense.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SYSARCH | System Architecture | 1.0 |
| ICD | Interface Control Document | 1.0 |
| CAN-MATRIX | CAN Message Matrix | 1.0 |
| HSR | Hardware Safety Requirements | 1.0 |
| SWR-CVC | Software Requirements -- CVC | 1.0 |
| SWR-FZC | Software Requirements -- FZC | 1.0 |
| SWR-RZC | Software Requirements -- RZC | 1.0 |
| SWR-SC | Software Requirements -- SC | 1.0 |
| PIN-MAP | Pin Mapping | 0.1 |

## 3. Common STM32G474RE Platform

All three zone ECUs (CVC, FZC, RZC) use the STM32G474RE Nucleo-64 board. This section defines the common hardware characteristics shared across these ECUs.

### 3.1 MCU Specifications

| Parameter | Value |
|-----------|-------|
| Part number | STM32G474RET6 |
| Core | ARM Cortex-M4F, single core |
| Max frequency | 170 MHz |
| Flash | 512 KB |
| SRAM | 128 KB (96 KB SRAM1 + 32 KB SRAM2) |
| CCMRAM | 32 KB (tightly coupled, not DMA-accessible) |
| FPU | Single-precision hardware FPU |
| FDCAN | 3x FDCAN controllers (used in classic CAN mode) |
| SPI | 4x SPI |
| USART | 5x USART + 1x LPUART |
| I2C | 4x I2C |
| ADC | 5x 12-bit ADC (up to 4 Msps) |
| Timers | 14 timers (advanced, general-purpose, basic) |
| GPIO | 51 GPIO pins |
| Package | LQFP64 |
| Voltage | 1.71 V to 3.6 V |
| Temperature | -40 degC to +85 degC (industrial) |

### 3.2 Clock Configuration

| Clock Source | Frequency | Purpose |
|-------------|-----------|---------|
| HSE (external) | 8 MHz (Nucleo onboard) | PLL input |
| PLL output (SYSCLK) | 170 MHz | CPU clock, APB1/APB2 |
| APB1 (PCLK1) | 170 MHz (no prescaler) | TIM2-7, USART2-5, SPI2/3, I2C1-3, FDCAN |
| APB2 (PCLK2) | 170 MHz (no prescaler) | TIM1/8/15-17/20, SPI1, USART1, ADC1-5 |
| LSI | 32 kHz (internal) | IWDG (backup; TPS3823 is primary WDT) |
| SysTick | 170 MHz / 170000 = 1 kHz | FreeRTOS tick (1 ms) |

### 3.3 Common Memory Map

| Region | Start Address | End Address | Size | Usage |
|--------|-------------|------------|------|-------|
| Flash | 0x08000000 | 0x0807FFFF | 512 KB | Application code, const data, lookup tables |
| Flash (NVM sector) | 0x0807E000 | 0x0807FFFF | 8 KB | DTC persistence, calibration data |
| SRAM1 | 0x20000000 | 0x20017FFF | 96 KB | .data, .bss, heap (none), stacks |
| SRAM2 | 0x20018000 | 0x2001FFFF | 32 KB | DMA buffers, CAN message buffers |
| CCMRAM | 0x10000000 | 0x10007FFF | 32 KB | Safety-critical variables, stack canary region |
| Peripherals | 0x40000000 | 0x50060BFF | -- | All MCU peripherals |
| System | 0xE0000000 | 0xE00FFFFF | -- | NVIC, SCB, MPU, SysTick, DWT |

### 3.4 MPU Configuration (Common to CVC, FZC, RZC)

| Region | Start | Size | Access | Attributes | Purpose |
|--------|-------|------|--------|-----------|---------|
| 0 | 0x00000000 | 4 GB | No access | Default background | Catch wild pointers |
| 1 | 0x08000000 | 512 KB | RO, Execute | Cacheable, WB | Flash (code + const) |
| 2 | 0x20000000 | 128 KB | RW, No execute | Cacheable, WB | SRAM (data + stacks) |
| 3 | 0x10000000 | 32 KB | RW, No execute | Non-cacheable | CCMRAM (safety vars) |
| 4 | 0x40000000 | 512 MB | RW, No execute | Device, non-cacheable | Peripherals |
| 5 | 0xE0000000 | 1 MB | RW, No execute | Device, non-cacheable | System (NVIC, SCB) |
| 6 | 0x20017F00 | 256 B | No access | -- | Stack guard (bottom of main stack) |
| 7 | (ECU-specific) | (varies) | (varies) | -- | ECU-specific isolation |

Region 6 provides stack overflow detection. Any access to this 256-byte guard zone at the bottom of the main task stack triggers a MemManage fault, which the handler translates into an immediate watchdog suppress (MCU reset via TPS3823).

<!-- HITL-LOCK START:COMMENT-BLOCK-HSI-SEC3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 3 defines the common STM32G474RE platform specifications shared across CVC, FZC, and RZC. The MCU specs (170 MHz, 512 KB flash, 128 KB SRAM, 32 KB CCMRAM) are correct per the STM32G474RE datasheet. The clock configuration (8 MHz HSE -> PLL -> 170 MHz SYSCLK) is standard for the Nucleo-64 board. The memory map correctly places safety-critical variables in CCMRAM (non-DMA-accessible, reducing corruption risk). The MPU configuration with 7 regions is well-designed: Region 0 (background no-access) catches wild pointers, Region 6 (stack guard) detects overflow. The BOR Level 4 (2.7V) is appropriate for preventing execution at brownout voltages. The PVD at 2.9V provides early warning for graceful degradation. No gaps identified in the common platform specification.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSI-SEC3 -->

### 3.5 Startup Sequence (Common)

```
1. Reset vector -> SystemInit()
   - Configure flash wait states (5 WS at 170 MHz)
   - Enable HSE, configure PLL (8 MHz * 85 / 2 / 2 = 170 MHz)
   - Switch SYSCLK to PLL output
   - Set APB1 and APB2 prescalers to /1

2. HAL_Init()
   - Initialize SysTick to 1 ms
   - Enable NVIC priority grouping (4 bits preemption, 0 bits sub)

3. MPU_Config()
   - Configure 7 MPU regions per Section 3.4
   - Enable MPU with default background region disabled

4. GPIO_Init()
   - Configure all GPIO pins per ECU-specific pin mapping
   - Initialize safety outputs to safe state:
     CVC: all GPIOs default (E-stop = input, WDT = output LOW)
     FZC: servo PWM pins LOW, buzzer LOW, WDT LOW
     RZC: motor enable LOW, PWM pins LOW, WDT LOW

5. Peripheral_Init()
   - Initialize peripherals in order:
     a. FDCAN1 (CAN controller)
     b. SPI (sensor interfaces)
     c. TIM (PWM timers)
     d. ADC (with DMA)
     e. USART (UART for lidar)
     f. I2C (display)

6. BSW_Init()
   - Initialize AUTOSAR-like BSW modules:
     a. Can -> CanIf -> PduR -> Com (CAN communication stack)
     b. Dcm + Dem (diagnostics)
     c. WdgM (watchdog manager)
     d. BswM (mode management)
     e. Rte (runtime environment)

7. SelfTest_Startup()
   - Execute ECU-specific self-test sequence (see per-ECU sections)
   - If all safety tests pass: enter operational mode
   - If any safety test fails: enter SAFE_STOP, blink fault pattern

8. Scheduler_Start()
   - Start FreeRTOS scheduler
   - Begin periodic task execution
```

### 3.6 Brown-Out Detection

| Parameter | Value |
|-----------|-------|
| BOR level | Level 4 (VBOR4 = 2.7 V falling threshold) |
| POR threshold | ~1.8 V |
| Action on BOR | Hardware reset (MCU restarts from reset vector) |
| PVD (Programmable) | Enabled at 2.9 V (rising) / 2.8 V (falling) for early warning |
| PVD action | EXTI interrupt -> suppress watchdog feed -> MCU reset |

## 4. CVC -- Central Vehicle Computer

### 4.1 Pin Mapping Table

| MCU Pin | Signal Name | Peripheral | AF | Direction | Net Name | External Component | Notes |
|---------|------------|-----------|-----|-----------|----------|-------------------|-------|
| PA5 | SPI1_SCK | SPI1 | AF5 | Output | CVC_SPI_CLK | AS5048A x2 | 1 MHz clock |
| PA6 | SPI1_MISO | SPI1 | AF5 | Input | CVC_SPI_MISO | AS5048A x2 | Data from sensors |
| PA7 | SPI1_MOSI | SPI1 | AF5 | Output | CVC_SPI_MOSI | AS5048A x2 | Commands to sensors |
| PA4 | PEDAL_CS1 | GPIO | -- | Output | CVC_CS1_N | AS5048A #1 | Active LOW, 10k pull-up |
| PA15 | PEDAL_CS2 | GPIO | -- | Output | CVC_CS2_N | AS5048A #2 | Active LOW, 10k pull-up |
| PA11 | FDCAN1_RX | FDCAN1 | AF9 | Input | CAN_RX | TJA1051T/3 RXD | CAN receive |
| PA12 | FDCAN1_TX | FDCAN1 | AF9 | Output | CAN_TX | TJA1051T/3 TXD | CAN transmit |
| PB8 | I2C1_SCL | I2C1 | AF4 | Bidir | CVC_SCL | SSD1306 OLED | 4.7k pull-up, 400 kHz |
| PB9 | I2C1_SDA | I2C1 | AF4 | Bidir | CVC_SDA | SSD1306 OLED | 4.7k pull-up |
| PC13 | ESTOP_IN | EXTI | -- | Input | CVC_ESTOP | NC push button | Active HIGH (NC wiring), internal pull-up |
| PB0 | WDT_FEED | GPIO | -- | Output | CVC_WDI | TPS3823 WDI | Toggle to feed watchdog |
| PA5 | STATUS_LED | GPIO | -- | Output | CVC_LED | Onboard LED | Shared with SPI1_SCK on Nucleo (use LED on PB1 instead) |
| PB1 | STATUS_LED_ALT | GPIO | -- | Output | CVC_LED2 | External green LED | 330 ohm series resistor |

Note: On the Nucleo-64 board, PA5 serves double duty as the onboard LED and SPI1_SCK. For the CVC, SPI1_SCK takes priority. An external status LED on PB1 is used instead.

### 4.2 Peripheral Configuration

#### 4.2.1 SPI1 -- Dual Pedal Sensors

| Register / Parameter | Value | Notes |
|---------------------|-------|-------|
| CR1.BR | 0b110 (PCLK2/128 = 1.328 MHz) | Closest to 1 MHz without exceeding |
| CR1.CPOL | 0 | Clock idle LOW |
| CR1.CPHA | 1 | Data sampled on trailing edge |
| CR1.MSTR | 1 | Master mode |
| CR1.LSBFIRST | 0 | MSB first |
| CR1.SSM | 1 | Software NSS management |
| CR1.SSI | 1 | Internal NSS HIGH (master) |
| CR2.DS | 0b1111 | 16-bit data frame |
| CR2.FRXTH | 0 | RXNE event on 16-bit |
| Interrupt | SPI1_IRQ (NVIC priority 5) | RXNE interrupt for data ready |
| DMA | None (polled/interrupt) | Short transactions, DMA overhead not justified |

#### 4.2.2 FDCAN1 -- CAN Controller

| Register / Parameter | Value | Notes |
|---------------------|-------|-------|
| NBTP.NBRP | 34 (170 MHz / 34 = 5 MHz) | Prescaler for 500 kbps |
| NBTP.NTSEG1 | 7 | 8 Tq (sync + TSEG1) |
| NBTP.NTSEG2 | 2 | 2 Tq |
| NBTP.NSJW | 2 | 2 Tq |
| CCCR.FDOE | 0 | Classic CAN mode (not FD) |
| CCCR.BRSE | 0 | No bit rate switching |
| GFC | Reject non-matching | Only accept filtered IDs |
| RX FIFO 0 | 3 elements | Safety messages |
| RX FIFO 1 | 3 elements | QM messages |
| TX FIFO | 3 elements | Transmit queue |
| Interrupt | FDCAN1_IT0 (priority 3) | RX FIFO 0 new message, TX complete |
| | FDCAN1_IT1 (priority 4) | RX FIFO 1, bus-off, error warning |

#### 4.2.3 I2C1 -- OLED Display

| Register / Parameter | Value | Notes |
|---------------------|-------|-------|
| TIMINGR | 0x00B03FDB (approximate) | 400 kHz Fast Mode at 170 MHz |
| CR1.ANFOFF | 0 | Analog filter enabled |
| CR1.DNF | 0b0000 | No digital filter |
| OAR1 | Not used (master mode) | |
| Interrupt | I2C1_EV_IRQ (priority 6) | Transfer complete, NACK |
| DMA | DMA1 CH6 (TX) | Bulk frame buffer transfer |

### 4.3 Electrical Characteristics

| Interface | Voltage | Current (max) | Notes |
|-----------|---------|---------------|-------|
| SPI1 (PA5-PA7) | 3.3V LVCMOS | 8 mA per pin | GPIO output drive |
| CS1, CS2 (PA4, PA15) | 3.3V | 8 mA | 10k external pull-up to 3.3V |
| FDCAN1 (PA11/PA12) | 3.3V | Via TJA1051T/3 | Transceiver handles bus levels |
| I2C1 (PB8/PB9) | 3.3V open-drain | 4.7k pull-up to 3.3V | 3 mA max sink per pin |
| E-stop (PC13) | 3.3V input | < 1 mA (pull-up current) | 10k internal pull-up |
| WDT (PB0) | 3.3V | < 1 mA | TPS3823 WDI input current ~1 uA |

### 4.4 Timing Constraints

| Function | Deadline | WCET Budget | Frequency |
|----------|----------|-------------|-----------|
| Pedal SPI read (both sensors) | 500 us | 50 us typical | 100 Hz (10 ms) |
| Pedal plausibility check | 1 ms (same cycle) | 10 us | 100 Hz |
| E-stop ISR latency | 10 us | 5 us typical | Event |
| E-stop debounce | 1 ms | Hardware RC | Event |
| CAN TX (vehicle state) | 1 ms jitter | 270 us (arbitration) | 100 Hz |
| CAN TX (heartbeat) | 5 ms jitter | 270 us | 20 Hz |
| OLED update | 50 ms | 30 ms | 5 Hz |
| Watchdog feed | < 1.6 s period | ~1 us (GPIO toggle) | ~10 Hz |
| Main loop total | 10 ms | < 8 ms (80% budget) | 100 Hz |

### 4.5 CVC Memory Layout

```
Flash (512 KB):
  0x08000000 - 0x0800007F  Vector table (128 bytes, 32 entries)
  0x08000080 - 0x0804FFFF  Application code + BSW (~320 KB max)
  0x08050000 - 0x0807BFFF  Const data, lookup tables, strings
  0x0807C000 - 0x0807DFFF  NVM: DTC storage (8 KB, dual-copy)
  0x0807E000 - 0x0807EFFF  NVM: Vehicle state persistence (4 KB, dual-copy)
  0x0807F000 - 0x0807FFFF  NVM: Calibration data (4 KB, CRC-16 protected)

SRAM1 (96 KB):
  0x20000000 - 0x200003FF  .data (initialized globals, ~1 KB)
  0x20000400 - 0x200013FF  .bss (uninitialized globals, ~4 KB)
  0x20001400 - 0x200053FF  FreeRTOS heap (static allocation, 16 KB)
  0x20005400 - 0x200063FF  Main task stack (4 KB, stack canary at bottom)
  0x20006400 - 0x200073FF  Safety task stack (4 KB)
  0x20007400 - 0x200083FF  QM task stack (4 KB)
  0x20008400 - 0x20017FFF  Free (~60 KB reserve)

SRAM2 (32 KB):
  0x20018000 - 0x200183FF  CAN RX message buffer (1 KB)
  0x20018400 - 0x200187FF  CAN TX message buffer (1 KB)
  0x20018800 - 0x200188FF  E2E alive counter array (256 B, 16 entries)
  0x20018900 - 0x2001FFFF  Free (~30 KB reserve)

CCMRAM (32 KB):
  0x10000000 - 0x100000FF  Safety-critical state variables (256 B)
  0x10000100 - 0x100001FF  Pedal sensor data (redundant copy, 256 B)
  0x10000200 - 0x100002FF  Vehicle state machine state (256 B)
  0x10000300 - 0x10007FFF  Free (~31 KB reserve)
```

### 4.6 CVC Startup Self-Test Sequence

| Step | Test | Duration | Failure Action | ASIL |
|------|------|----------|---------------|------|
| 1 | SPI sensor response (both AS5048A NOP read) | < 2 ms | Remain in INIT | D |
| 2 | FDCAN1 loopback TX/RX | < 5 ms | Remain in INIT, CAN fault DTC | D |
| 3 | NVM integrity (dual-copy CRC-32 check) | < 10 ms | Use defaults, log DTC | D |
| 4 | SSD1306 OLED init | < 100 ms | Log QM DTC, continue | QM |
| 5 | MPU region readback verify | < 1 ms | Remain in INIT | D |
| 6 | Stack canary write (0xDEADBEEF) + readback | < 1 ms | Remain in INIT | D |
| 7 | RAM test pattern (32 B, 0xAA/0x55) | < 1 ms | Remain in INIT | D |

Total self-test time: < 120 ms.

### 4.7 CVC Safety-Relevant HW Features

| Feature | Description | Diagnostic Coverage |
|---------|-------------|-------------------|
| Dual AS5048A sensors | Independent pedal position sensing on shared SPI bus | 99% (mutual plausibility) |
| TPS3823 external watchdog | Independent oscillator, 1.6 s timeout | 85% (detects hang, clock failure) |
| MPU (6 regions) | Stack guard, peripheral isolation, flash XN | Spatial FFI |
| Brown-out detection (BOR4) | Reset at VDD < 2.7 V | Prevents execution at low voltage |
| Flash ECC | Single-bit correction, double-bit detection | > 99% (flash data integrity) |
| SRAM parity | Parity check on SRAM1 and SRAM2 | ~90% (single-bit error detection) |
| FDCAN error counters | TEC/REC monitoring, bus-off detection | 90% (CAN bus fault detection) |

<!-- HITL-LOCK START:COMMENT-BLOCK-HSI-SEC4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 4 (CVC HSI) is comprehensive. Pin mapping is clear with net names and external component references. The SPI1 configuration (CPOL=0, CPHA=1, MSB first, 16-bit) matches the AS5048A datasheet requirements. The FDCAN1 bit timing (5 MHz TQ clock, 10 TQ per bit = 500 kbps) is correct. The note about PA5 dual-use (SPI1_SCK vs onboard LED) is an important implementation detail for Nucleo-64 boards. The CVC memory layout provides adequate reserves (~60 KB SRAM1, ~30 KB SRAM2, ~31 KB CCMRAM). The NVM dual-copy scheme for DTC and vehicle state provides corruption protection. The startup self-test sequence (7 steps, < 120 ms) covers all critical subsystems. Timing constraints sum to < 8 ms within the 10 ms main loop. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSI-SEC4 -->

## 5. FZC -- Front Zone Controller

### 5.1 Pin Mapping Table

| MCU Pin | Signal Name | Peripheral | AF | Direction | Net Name | External Component | Notes |
|---------|------------|-----------|-----|-----------|----------|-------------------|-------|
| PA0 | STEER_PWM | TIM2_CH1 | AF1 | Output | FZC_STEER | Steering servo signal | 50 Hz, 1-2 ms pulse |
| PA1 | BRAKE_PWM | TIM2_CH2 | AF1 | Output | FZC_BRAKE | Brake servo signal | 50 Hz, 1-2 ms pulse |
| PA2 | LIDAR_TX | USART2_TX | AF7 | Output | FZC_LIDAR_TX | TFMini-S RX | Config commands |
| PA3 | LIDAR_RX | USART2_RX | AF7 | Input | FZC_LIDAR_RX | TFMini-S TX | Data frames via DMA |
| PB13 | SPI2_SCK | SPI2 | AF5 | Output | FZC_SPI_CLK | AS5048A (steer) | 1 MHz clock |
| PB14 | SPI2_MISO | SPI2 | AF5 | Input | FZC_SPI_MISO | AS5048A (steer) | Angle data |
| PB15 | SPI2_MOSI | SPI2 | AF5 | Output | FZC_SPI_MOSI | AS5048A (steer) | Commands |
| PB12 | STEER_CS | GPIO | -- | Output | FZC_CS_N | AS5048A CS | Active LOW, 10k pull-up |
| PA11 | FDCAN1_RX | FDCAN1 | AF9 | Input | CAN_RX | TJA1051T/3 RXD | CAN receive |
| PA12 | FDCAN1_TX | FDCAN1 | AF9 | Output | CAN_TX | TJA1051T/3 TXD | CAN transmit |
| PB4 | BUZZER | GPIO | -- | Output | FZC_BUZZ | 2N7002 gate -> buzzer | Active HIGH via MOSFET |
| PB0 | WDT_FEED | GPIO | -- | Output | FZC_WDI | TPS3823 WDI | Toggle to feed watchdog |

### 5.2 Peripheral Configuration

#### 5.2.1 TIM2 -- Servo PWM

| Register / Parameter | Value | Notes |
|---------------------|-------|-------|
| PSC | 169 (170 MHz / 170 = 1 MHz timer clock) | 1 us resolution |
| ARR | 19999 | 20 ms period = 50 Hz |
| CH1 CCR (steering) | 1000-2000 | 1.0-2.0 ms pulse width |
| CH2 CCR (brake) | 1000-2000 | 1.0-2.0 ms pulse width |
| OCM | PWM Mode 1 (OCxREF HIGH when CNT < CCR) | |
| CCER.CC1E | 1 (steering output enable) | Software-controllable disable |
| CCER.CC2E | 1 (brake output enable) | |
| CR1.CEN | 1 (counter enable) | Start after init |

#### 5.2.2 USART2 -- TFMini-S Lidar

| Register / Parameter | Value | Notes |
|---------------------|-------|-------|
| BRR | 170000000 / 115200 = 1476 | 115200 baud |
| CR1.M | 0 | 8-bit data |
| CR1.PCE | 0 | No parity |
| CR2.STOP | 0b00 | 1 stop bit |
| CR3.DMAR | 1 | DMA receive enable |
| DMA | DMA1 CH5, circular, 18 bytes | 2-frame buffer |
| Interrupt | DMA TC + HT | Half-transfer and transfer-complete |

#### 5.2.3 SPI2 -- Steering Angle Sensor

Same configuration as CVC SPI1 (Section 4.2.1) but on SPI2 peripheral with pins PB13-PB15 and CS on PB12.

### 5.3 Timing Constraints

| Function | Deadline | WCET Budget | Frequency |
|----------|----------|-------------|-----------|
| Steering SPI read | 500 us | 25 us | 100 Hz |
| Steering angle check | 1 ms | 10 us | 100 Hz |
| Steering servo PWM update | 1 ms | 5 us | 100 Hz |
| Brake servo PWM update | 1 ms | 5 us | 100 Hz |
| Lidar frame parse (DMA) | 10 ms | 20 us (ISR) | 100 Hz |
| CAN RX process | 1 ms | 30 us per message | 100 Hz |
| Buzzer pattern update | 10 ms | 5 us | 100 Hz |
| Main loop total | 10 ms | < 8 ms | 100 Hz |

### 5.4 FZC Memory Layout

```
Flash (512 KB):
  0x08000000 - 0x0800007F  Vector table
  0x08000080 - 0x0804FFFF  Application code + BSW (~320 KB max)
  0x08050000 - 0x0807BFFF  Const data, lidar thresholds, servo calibration
  0x0807C000 - 0x0807DFFF  NVM: DTC storage (8 KB, dual-copy)
  0x0807E000 - 0x0807EFFF  NVM: Servo calibration persistence (4 KB)
  0x0807F000 - 0x0807FFFF  NVM: Lidar threshold calibration (4 KB)

SRAM1 (96 KB):
  0x20000000 - 0x200003FF  .data (~1 KB)
  0x20000400 - 0x200013FF  .bss (~4 KB)
  0x20001400 - 0x200053FF  FreeRTOS heap (static, 16 KB)
  0x20005400 - 0x200063FF  Main task stack (4 KB)
  0x20006400 - 0x200073FF  Safety task stack (4 KB)
  0x20007400 - 0x200083FF  QM task stack (4 KB)
  0x20008400 - 0x20017FFF  Free (~60 KB)

SRAM2 (32 KB):
  0x20018000 - 0x200183FF  CAN message buffers (1 KB)
  0x20018400 - 0x200184FF  E2E state (256 B)
  0x20018500 - 0x20018523  UART DMA circular buffer (18 B = 2 lidar frames)
  0x20018524 - 0x2001FFFF  Free (~30 KB)

CCMRAM (32 KB):
  0x10000000 - 0x100000FF  Safety state (steering angle, brake position)
  0x10000100 - 0x100001FF  Lidar data (distance, zone, fault flags)
  0x10000200 - 0x10007FFF  Free (~31 KB)
```

### 5.5 FZC Startup Self-Test Sequence

| Step | Test | Duration | Failure Action | ASIL |
|------|------|----------|---------------|------|
| 1 | Servo neutral: steering to center (1.5 ms), brake to release (1.0 ms) | < 50 ms | Disable servos, remain in INIT | D |
| 2 | SPI2 steering sensor NOP read | < 2 ms | Remain in INIT, steering fault DTC | D |
| 3 | UART lidar handshake (3 valid frames in 500 ms) | < 500 ms | Log DTC, continue with safe default 0 cm | C |
| 4 | FDCAN1 loopback test | < 5 ms | Remain in INIT, CAN fault DTC | D |
| 5 | MPU region readback verify | < 1 ms | Remain in INIT | D |
| 6 | Stack canary write + readback | < 1 ms | Remain in INIT | D |
| 7 | RAM test pattern (32 B) | < 1 ms | Remain in INIT | D |

Total self-test time: < 560 ms.

### 5.6 FZC Safety-Relevant HW Features

| Feature | Description | Diagnostic Coverage |
|---------|-------------|-------------------|
| AS5048A steering angle sensor | 14-bit angle feedback for command-vs-feedback | 95% (with SW plausibility) |
| TIM2 PWM output control | 3-level disable (CCR, CCER, GPIO) | 80% (defense-in-depth) |
| TPS3823 external watchdog | Independent 1.6 s timeout | 85% |
| MPU | Stack guard, peripheral isolation | Spatial FFI |
| USART2 + DMA | Autonomous lidar reception | 80% (checksum + timeout) |
| Brown-out detection | Reset at VDD < 2.7 V | Prevents low-voltage operation |

<!-- HITL-LOCK START:COMMENT-BLOCK-HSI-SEC5 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 5 (FZC HSI) correctly specifies the steering and brake servo PWM, lidar UART, and steering angle sensor interfaces. The TIM2 PWM configuration (50 Hz, 1-2 ms pulse) matches standard hobby servo protocol. The USART2 DMA circular buffer (18 bytes = 2 lidar frames) enables autonomous reception without CPU intervention. The SPI2 for steering sensor reuses the CVC SPI1 configuration, which is appropriate given the same AS5048A part. The FZC startup self-test is the longest at 560 ms due to the lidar handshake (500 ms for 3 valid frames). The pin mapping correctly uses PA0/PA1 for steering/brake PWM, which avoids conflicts with FDCAN1 on PA11/PA12. The 3-level PWM disable documented in HSR-FZC-006 is reflected in the TIM2 CCER configuration. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSI-SEC5 -->

## 6. RZC -- Rear Zone Controller

### 6.1 Pin Mapping Table

| MCU Pin | Signal Name | Peripheral | AF | Direction | Net Name | External Component | Notes |
|---------|------------|-----------|-----|-----------|----------|-------------------|-------|
| PA8 | MOTOR_RPWM | TIM1_CH1 | AF6 | Output | RZC_RPWM | BTS7960 RPWM | 20 kHz, forward |
| PA9 | MOTOR_LPWM | TIM1_CH2 | AF6 | Output | RZC_LPWM | BTS7960 LPWM | 20 kHz, reverse |
| PB0 | MOTOR_R_EN | GPIO | -- | Output | RZC_MOT_REN | BTS7960 R_EN | 10k pull-down, active HIGH |
| PB1 | MOTOR_L_EN | GPIO | -- | Output | RZC_MOT_LEN | BTS7960 L_EN | 10k pull-down, active HIGH |
| PB6 | ENCODER_A | TIM4_CH1 | AF2 | Input | RZC_ENC_A | Motor encoder A | 10k pull-up |
| PB7 | ENCODER_B | TIM4_CH2 | AF2 | Input | RZC_ENC_B | Motor encoder B | 10k pull-up |
| PA0 | CURRENT_SENSE | ADC1_IN1 | Analog | Input | RZC_ISENSE | ACS723 output | 100 mV/A, mid-rail zero |
| PA1 | TEMP_NTC1 | ADC1_IN2 | Analog | Input | RZC_NTC1 | NTC #1 (motor winding) | 10k pullup divider |
| PA2 | TEMP_NTC2 | ADC1_IN3 | Analog | Input | RZC_NTC2 | NTC #2 (board) | 10k pullup divider |
| PA3 | BATT_VOLTAGE | ADC1_IN4 | Analog | Input | RZC_VBAT | Voltage divider | 47k/10k, Zener clamp |
| PA11 | FDCAN1_RX | FDCAN1 | AF9 | Input | CAN_RX | TJA1051T/3 RXD | CAN receive |
| PA12 | FDCAN1_TX | FDCAN1 | AF9 | Output | CAN_TX | TJA1051T/3 TXD | CAN transmit |
| PB4 | WDT_FEED | GPIO | -- | Output | RZC_WDT_WDI | TPS3823 WDI | Toggle to feed watchdog |

### 6.2 Peripheral Configuration

#### 6.2.1 TIM1 -- Motor PWM

| Register / Parameter | Value | Notes |
|---------------------|-------|-------|
| PSC | 0 (no prescaler) | Full 170 MHz timer clock |
| ARR | 8499 (170 MHz / 20 kHz - 1) | 20 kHz PWM frequency |
| CH1 CCR (RPWM) | 0-8074 (0-95% of ARR) | Forward direction duty |
| CH2 CCR (LPWM) | 0-8074 (0-95% of ARR) | Reverse direction duty |
| OCM | PWM Mode 1 | Output HIGH when CNT < CCR |
| BDTR.DTG | Dead-time available on TIM1 (advanced timer) | Software-enforced 10 us dead-time |
| CCER.CC1E, CC2E | 1, 1 | Both channels enabled |

#### 6.2.2 TIM4 -- Quadrature Encoder

| Register / Parameter | Value | Notes |
|---------------------|-------|-------|
| SMCR.SMS | 0b011 (Encoder Mode 3) | Count on both TI1 and TI2 edges |
| CCMR1.IC1F | 0b1111 | Maximum input filter (noise rejection) |
| CCMR1.IC2F | 0b1111 | Maximum input filter |
| ARR | 0xFFFF (65535) | 16-bit counter range |
| CR1.CEN | 1 | Counter enable |

#### 6.2.3 ADC1 -- 4-Channel Scan

| Register / Parameter | Value | Notes |
|---------------------|-------|-------|
| CFGR.RES | 0b00 (12-bit) | 12-bit resolution |
| CFGR.CONT | 0 | Single conversion per trigger |
| CFGR.EXTEN | 0b01 (rising edge) | Timer-triggered |
| CFGR.EXTSEL | TIM6 TRGO | Timer 6 triggers ADC scan |
| CFGR.DMAEN | 1 | DMA transfers results |
| SQR1.L | 3 (4 conversions) | Sequence: CH1, CH2, CH3, CH4 |
| SMPR1.SMP1 | 0b100 (47.5 cycles) | ~1.3 us per channel |
| DMA | DMA1, circular, 4x uint16_t | Auto-fills result buffer |

**ADC Trigger Timing**:

| ADC Channel | Timer | Trigger Rate | Purpose |
|-------------|-------|-------------|---------|
| CH1 (current) | TIM6 at 1 kHz | 1 kHz | Motor current (fast for overcurrent) |
| CH2 (NTC 1) | Same scan | 1 kHz (SW reads at 10 Hz) | Motor temperature |
| CH3 (NTC 2) | Same scan | 1 kHz (SW reads at 10 Hz) | Board temperature |
| CH4 (battery) | Same scan | 1 kHz (SW reads at 10 Hz) | Battery voltage |

Note: All 4 channels are scanned at 1 kHz (driven by TIM6), but software processes temperature and battery data only at 10 Hz. Current data is processed at the full 1 kHz rate for fast overcurrent detection.

### 6.3 Timing Constraints

| Function | Deadline | WCET Budget | Frequency |
|----------|----------|-------------|-----------|
| Current ADC sample + filter | 1 ms | 100 us | 1 kHz |
| Overcurrent detection | 10 ms debounce | 10 us per check | 1 kHz |
| Motor PWM update | 1 ms | 20 us | 100 Hz |
| Encoder speed calculation | 10 ms | 10 us | 100 Hz |
| Temperature derating update | 100 ms | 30 us | 10 Hz |
| Battery voltage check | 100 ms | 10 us | 10 Hz |
| CAN RX process | 1 ms | 30 us per message | 100 Hz |
| Motor disable on zero-torque | 5 ms from CAN RX | 10 us | On event |
| Main loop total | 10 ms | < 8 ms | 100 Hz |

### 6.4 RZC Memory Layout

```
Flash (512 KB):
  0x08000000 - 0x0800007F  Vector table
  0x08000080 - 0x0804FFFF  Application code + BSW
  0x08050000 - 0x0807BFFF  Const data, torque lookup tables
  0x0807C000 - 0x0807DFFF  NVM: DTC storage (8 KB, dual-copy)
  0x0807E000 - 0x0807EFFF  NVM: Motor calibration (4 KB)
  0x0807F000 - 0x0807FFFF  NVM: ACS723/NTC calibration (4 KB)

SRAM1 (96 KB):
  0x20000000 - 0x200003FF  .data (~1 KB)
  0x20000400 - 0x200013FF  .bss (~4 KB)
  0x20001400 - 0x200053FF  FreeRTOS heap (static, 16 KB)
  0x20005400 - 0x200063FF  Main task stack (4 KB)
  0x20006400 - 0x200073FF  Safety task stack (4 KB)
  0x20007400 - 0x200083FF  Current monitor task stack (4 KB, highest priority)
  0x20008400 - 0x20017FFF  Free (~60 KB)

SRAM2 (32 KB):
  0x20018000 - 0x200183FF  CAN message buffers (1 KB)
  0x20018400 - 0x200184FF  E2E state (256 B)
  0x20018500 - 0x20018507  ADC DMA buffer (4x uint16_t = 8 B)
  0x20018508 - 0x2001FFFF  Free (~30 KB)

CCMRAM (32 KB):
  0x10000000 - 0x100000FF  Motor control state (current, direction, duty)
  0x10000100 - 0x100001FF  Temperature data (filtered values, derating state)
  0x10000200 - 0x100002FF  ACS723 calibration data (zero offset, gain)
  0x10000300 - 0x10007FFF  Free (~31 KB)
```

### 6.5 RZC Startup Self-Test Sequence

| Step | Test | Duration | Failure Action | ASIL |
|------|------|----------|---------------|------|
| 1 | BTS7960 enable test (toggle R_EN, L_EN, readback) | < 5 ms | Motor permanently disabled, DTC | D |
| 2 | ACS723 zero-current calibration (64 samples, 64 ms) | 64 ms | Motor permanently disabled, DTC | A |
| 3 | NTC range check (reading in -30 to 150 degC) | < 5 ms | Motor permanently disabled, DTC | A |
| 4 | Encoder check (TIM4 counter not stuck at 0 or 0xFFFF) | < 5 ms | Log DTC, encoder degraded | C |
| 5 | FDCAN1 loopback test | < 5 ms | Remain in INIT, CAN fault DTC | D |
| 6 | MPU region readback verify | < 1 ms | Remain in INIT | D |
| 7 | Stack canary write + readback | < 1 ms | Remain in INIT | D |
| 8 | RAM test pattern (32 B) | < 1 ms | Remain in INIT | D |

Total self-test time: < 90 ms.

### 6.6 RZC Safety-Relevant HW Features

| Feature | Description | Diagnostic Coverage |
|---------|-------------|-------------------|
| ACS723 current sensor | Galvanically isolated motor current measurement | 95% (with BTS7960 IS backup) |
| BTS7960 built-in protection | Hardware overcurrent trip at 43 A, thermal shutdown | Diverse backup to software |
| BTS7960 enable pull-downs | 10k to GND on R_EN/L_EN ensures disabled on reset | 99% (fail-safe default) |
| NTC open/short detection | ADC range check detects wire faults | 90% |
| TPS3823 external watchdog | Independent 1.6 s timeout | 85% |
| MPU | Stack guard, peripheral isolation | Spatial FFI |
| TIM4 encoder mode | Zero-CPU-overhead speed/direction measurement | 75% (with SW plausibility) |
| Brown-out detection | Reset at VDD < 2.7 V | Prevents low-voltage operation |

<!-- HITL-LOCK START:COMMENT-BLOCK-HSI-SEC6 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 6 (RZC HSI) correctly specifies the motor driver (BTS7960), current sensor (ACS723), temperature sensor (NTC), encoder (TIM4), and battery monitoring interfaces. The TIM1 PWM at 20 kHz is appropriate for DC motor drive (above audible range). The ADC scan at 1 kHz with DMA provides continuous 4-channel monitoring without CPU intervention. The note that all 4 ADC channels are scanned at 1 kHz but software processes temperature/battery at 10 Hz is an important implementation detail. The pin mapping correctly uses separate pins for motor PWM (PA8/PA9) and motor enable (PB0/PB1) with pull-downs documented. The encoder interface on TIM4 (PB6/PB7) with encoder mode 3 (both edges) provides maximum resolution. The RZC startup self-test includes ACS723 zero-current calibration (64 samples over 64 ms), which is critical for accurate current measurement. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSI-SEC6 -->

## 7. SC -- Safety Controller

### 7.1 MCU Specifications

| Parameter | Value |
|-----------|-------|
| Part number | TMS570LC4357BZWTQQ1 |
| Core | Dual ARM Cortex-R5F (lockstep) |
| Max frequency | 300 MHz |
| Flash | 4 MB |
| RAM | 512 KB (with ECC) |
| DCAN | 4 controllers (use DCAN1 only) |
| GIO | 32+ general-purpose I/O |
| ESM | Error Signaling Module for lockstep CPU comparison |
| PBIST | Programmable Built-In Self-Test (RAM March13N) |
| ECC | Flash ECC and RAM ECC |
| Safety manual | TI TUV-certified safety manual available |
| Package | BGA |
| Voltage | 1.2 V core + 3.3 V I/O |
| Temperature | -40 degC to +125 degC (automotive) |

### 7.2 Pin Mapping Table

| MCU Pin / GIO | Signal Name | Peripheral | Direction | Net Name | External Component | Notes |
|--------------|------------|-----------|-----------|----------|-------------------|-------|
| DCAN1_TX | CAN_TX | DCAN1 | Output (recessive in silent) | SC_CAN_TX | SN65HVD230 TXD | Listen-only; TX drives recessive |
| DCAN1_RX | CAN_RX | DCAN1 | Input | SC_CAN_RX | SN65HVD230 RXD | Receives all CAN frames |
| GIO_A[0] | KILL_RELAY | GIO | Output | SC_RELAY | IRLZ44N gate (100 ohm) | Active HIGH = relay energized |
| GIO_A[1] | FAULT_LED_CVC | GIO | Output | SC_LED_CVC | Red LED (330 ohm) | Active HIGH = fault |
| GIO_A[2] | FAULT_LED_FZC | GIO | Output | SC_LED_FZC | Red LED (330 ohm) | Active HIGH = fault |
| GIO_A[3] | FAULT_LED_RZC | GIO | Output | SC_LED_RZC | Red LED (330 ohm) | Active HIGH = fault |
| GIO_A[4] | WDT_FEED | GIO | Output | SC_WDI | TPS3823 WDI | Toggle to feed watchdog |
| GIO_B[1] | FAULT_LED_SYS | GIO | Output | SC_LED_SYS | Amber LED (330 ohm) | System fault / self-test fail |

Note: GIO_A[4] is used for the TPS3823 WDI (watchdog feed), and GIO_B[1] is used for the system fault LED. This differs slightly from the initial pin mapping document where GIO_A[4] was listed as "Status LED" -- the actual assignment prioritizes watchdog on the GIO_A port for reliability.

### 7.3 Clock Configuration

| Clock Source | Frequency | Purpose |
|-------------|-----------|---------|
| External oscillator | 16 MHz | PLL input |
| PLL output (GCLK) | 300 MHz | CPU clock |
| VCLK1 | 75 MHz (GCLK / 4) | Peripheral clock, DCAN clock |
| VCLK2 | 75 MHz | Secondary peripheral clock |
| RTI clock | 75 MHz / prescaler | Real-time interrupt (10 ms tick) |

### 7.4 Peripheral Configuration

#### 7.4.1 DCAN1 -- Listen-Only CAN

| Register | Value | Notes |
|----------|-------|-------|
| DCAN_CTL.Init | 1 (during config) | Enter initialization mode |
| DCAN_CTL.CCE | 1 (during config) | Configuration change enable |
| DCAN_BTR | 500 kbps baud rate | BRP=15, TSEG1=7, TSEG2=2 from 75 MHz VCLK1 |
| DCAN_TEST.Silent | 1 | **Listen-only mode** (bit 3) |
| DCAN_CTL.Init | 0 (after config) | Exit initialization, enter normal mode |
| Mailbox 1 | ID = 0x001, mask = 0x7FF | E-stop (exact match) |
| Mailbox 2 | ID = 0x010, mask = 0x7FF | CVC heartbeat |
| Mailbox 3 | ID = 0x011, mask = 0x7FF | FZC heartbeat |
| Mailbox 4 | ID = 0x012, mask = 0x7FF | RZC heartbeat |
| Mailbox 5 | ID = 0x100, mask = 0x7FF | Vehicle state |
| Mailbox 6 | ID = 0x101, mask = 0x7FF | Torque request (for current-vs-torque plausibility) |
| Mailbox 7 | ID = 0x301, mask = 0x7FF | Motor current |
| Mailboxes | 7 configured (exact-match acceptance masks) |
| Interrupt | DCAN1 Level 0 | New message notification |

#### 7.4.2 RTI -- 10 ms Tick Timer

| Register | Value | Notes |
|----------|-------|-------|
| RTIGCTRL.CNT0EN | 1 | Enable counter 0 |
| RTIUC0 | (75 MHz / 10000) - 1 = 7499 | Up counter prescaler (10 kHz tick) |
| RTIFRC0 | Free-running counter | Not used directly |
| RTICOMP0 | 100 | Compare register (100 * 0.1 ms = 10 ms) |
| RTIINTFLAG | Compare 0 interrupt | Sets tick flag for main loop |

#### 7.4.3 GIO -- GPIO

| Register | Value | Notes |
|----------|-------|-------|
| GIODIRA (direction) | 0x1F (A[0]-A[4] output) | All port A pins as output |
| GIODIRB (direction) | 0x02 (B[1] output) | System LED as output |
| GIODOUTA (data) | 0x00 (all LOW initially) | Relay open, LEDs off |
| GIODOUTB (data) | 0x00 | System LED off |

### 7.5 Memory Map

| Region | Start Address | End Address | Size | Usage |
|--------|-------------|------------|------|-------|
| Flash | 0x00000000 | 0x003FFFFF | 4 MB | Application code (~16 KB used, <0.4%) |
| Flash (CRC ref) | 0x003FF000 | 0x003FFFFF | 4 KB | Reference CRC-32 for flash verification |
| RAM | 0x08000000 | 0x0807FFFF | 512 KB | ~4 KB used (<0.8%) |
| Peripherals | 0xFFF7FC00 | -- | -- | GIO, DCAN, ESM, RTI registers |
| VIM | 0xFFFFFE00 | -- | -- | Vectored Interrupt Manager |

**RAM Usage Detail**:

```
0x08000000 - 0x080000FF  Global variables (heartbeat counters, plausibility state)
0x08000100 - 0x080001FF  CAN RX message buffer (6 mailbox copies)
0x08000200 - 0x080002FF  E2E state (alive counters per message type)
0x08000300 - 0x080003FF  Self-test state (runtime CRC partial result, test flags)
0x08000400 - 0x0800041F  RAM test pattern (32 bytes, 0xAA/0x55 alternating)
0x08000420 - 0x08000BFF  Stack (2 KB, canary at bottom 0x08000420 = 0xDEADBEEF)
0x08000C00 - 0x0807FFFF  Unused (~509 KB)
```

### 7.6 Timing Constraints

| Function | Deadline | WCET Budget | Frequency |
|----------|----------|-------------|-----------|
| CAN message receive (all mailboxes) | 1 ms | 200 us | Interrupt-driven |
| Heartbeat timeout check (3 ECUs) | 10 ms | 50 us | 100 Hz |
| Cross-plausibility check | 10 ms | 100 us | 100 Hz |
| Relay trigger evaluation | 10 ms | 20 us | 100 Hz |
| LED update | 10 ms | 10 us | 100 Hz |
| Runtime self-test (1 step) | 10 ms | 200 us | 100 Hz |
| Stack canary check | 10 ms | 5 us | 100 Hz |
| Watchdog feed | 10 ms | 5 us | 100 Hz |
| **Total main loop** | **10 ms** | **< 2 ms** | **100 Hz** |
| ESM lockstep ISR | Immediate | < 100 clock cycles | Event |
| Kill relay de-energize | < 5 ms from trigger | < 1 ms (GIO write) | Event |

### 7.7 Startup Sequence

```
1. TMS570 hardware startup:
   - Lockstep CPU comparison self-test (automatic at POR)
   - Flash ECC check (automatic)
   - RAM ECC initialization (automatic)

2. SC_Init():
   a. Initialize system clocks (PLL to 300 MHz)
   b. Configure GIO: A[0] output LOW (relay safe), A[1-4] output LOW, B[1] output LOW
   c. Configure RTI: 10 ms tick timer
   d. Configure loop execution timer (RTI counter for WCET measurement)
   e. Write stack canary: *(uint32_t*)0x08000420 = 0xDEADBEEF
   f. Write RAM test pattern: 32 bytes of 0xAA/0x55 at 0x08000400

3. SC_SelfTest_Startup():
   Step 1: Lockstep CPU BIST (ESM trigger, verify no error flag)
           Failure: blink GIO_B[1] 1x/sec, halt
   Step 2: RAM PBIST (March13N over full 256 KB, ~1 second)
           Failure: blink GIO_B[1] 2x/sec, halt
   Step 3: Flash CRC-32 (compute over application region, compare with reference)
           Failure: blink GIO_B[1] 3x/sec, halt
   Step 4: DCAN1 loopback test (internal loopback, TX/RX known payload)
           Then switch to silent mode
           Failure: blink GIO_B[1] 4x/sec, halt
   Step 5: GPIO readback (drive A[0]-A[4] to known states, readback verify)
           Failure: blink GIO_B[1] 5x/sec, halt
   Step 6: Fault LED lamp test (all 4 LEDs ON for 500 ms, then OFF)
           Visual confirmation to operator
   Step 7: Watchdog test (toggle WDI, verify TPS3823 RESET not asserted)
           Failure: blink GIO_B[1] 7x/sec, halt

4. If all tests pass:
   - Energize kill relay: GIO_A[0] = HIGH
   - Verify GIO_A[0] readback = HIGH
   - Enter main loop

5. SC_Main_Loop() (10 ms cooperative loop):
   a. SC_CAN_Receive()
   b. SC_Heartbeat_Monitor()
   c. SC_Plausibility_Check()
   d. SC_Relay_CheckTriggers()
   e. SC_LED_Update()
   f. SC_SelfTest_Runtime()  (1 step per iteration)
   g. SC_SelfTest_StackCanary()
   h. SC_Watchdog_Feed()
```

### 7.8 SC Safety-Relevant HW Features

| Feature | Description | Diagnostic Coverage |
|---------|-------------|-------------------|
| Lockstep CPU | Dual Cortex-R5F, cycle-by-cycle comparison | 99% (ISO 26262-5 Table D.7) |
| ESM | Error Signaling Module, NMI on lockstep error | Immediate fault detection |
| PBIST | March13N RAM self-test (startup + periodic) | > 99% (stuck-at, coupling faults) |
| Flash ECC | SEC-DED (single error correct, double detect) | > 99% (flash integrity) |
| RAM ECC | SEC-DED on all RAM banks | > 99% (RAM integrity) |
| DCAN1 silent mode | Listen-only via TEST register bit 3 | Prevents bus corruption |
| GIO readback | Data-in register reflects actual pin state | Detects GPIO driver faults |
| TPS3823 watchdog | Independent oscillator, 1.6 s timeout | 85% (detects hang/clock fail) |
| Independent power supply | Powered from 12V bus, NOT through kill relay | SC operates after relay opens |
| Kill relay energize-to-run | IRLZ44N + gate pull-down, default = relay OPEN | 99% (any SC failure = safe state) |

### 7.9 SC Diverse Redundancy Summary

| Dimension | Zone ECUs (CVC/FZC/RZC) | Safety Controller (SC) |
|-----------|-------------------------|----------------------|
| Vendor | STMicroelectronics | Texas Instruments |
| CPU | Cortex-M4F (single core) | Cortex-R5F (dual lockstep) |
| Compiler | arm-none-eabi-gcc | TI ARM CGT (CCS) |
| BSW | AUTOSAR-like (~2500 LOC) | Bare-metal (~400 LOC) |
| CAN mode | Normal TX/RX (FDCAN) | Listen-only (DCAN silent) |
| OS | FreeRTOS | None (bare-metal cooperative) |
| Safety HW | External WDT only | Lockstep + ESM + PBIST + ECC + external WDT |
| Dev tool | STM32CubeIDE + CubeMX | Code Composer Studio + HALCoGen |

<!-- HITL-LOCK START:COMMENT-BLOCK-HSI-SEC7 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 7 (SC HSI) is the most detailed and critical section, covering the TMS570LC43x lockstep platform. The DCAN1 configuration correctly shows 7 mailboxes with exact-match acceptance masks for the monitored CAN IDs. The mailbox list includes 0x101 (torque request) which is separate from 0x100 (vehicle state) -- this should be verified against the CAN matrix to confirm these are indeed separate CAN IDs. The GIO configuration note about GIO_A[4] being WDT (not status LED) and GIO_B[1] being system fault LED is an important deviation from the initial pin mapping that must be kept in sync. The RTI configuration for 10 ms tick (75 MHz / 7500 = 10 kHz, then compare at 100 = 10 ms) is correct. The RAM usage detail shows only ~3 KB of 512 KB used, which is expected for a bare-metal safety monitor. The diverse redundancy summary (Section 7.9) is valuable -- it clearly demonstrates diversity across 8 dimensions (vendor, CPU, compiler, BSW, CAN mode, OS, safety HW, dev tool). The startup sequence with blink-count error indication is consistent with SSR-SC-016. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSI-SEC7 -->

## 8. Cross-ECU Interface Summary

### 8.1 Interrupt Priority Assignment

**STM32G474RE (CVC, FZC, RZC)**:

| Priority | ISR | ASIL | Notes |
|----------|-----|------|-------|
| 0 (highest) | E-stop EXTI (CVC only) | B | Non-preemptible, < 10 us |
| 1 | ADC DMA complete (RZC only) | A | Current measurement at 1 kHz |
| 2 | SysTick (1 ms) | D | FreeRTOS scheduler tick |
| 3 | FDCAN1 FIFO 0 (safety RX) | D | Safety message reception |
| 4 | FDCAN1 FIFO 1 (QM RX) | QM | QM message reception |
| 5 | SPI TX/RX complete | D | Pedal/steering sensor data |
| 6 | I2C event (CVC), UART DMA (FZC) | QM/C | Display/lidar |
| 7-15 | Reserved | -- | Available for future use |

**TMS570LC43x (SC)**:

| Priority | ISR | ASIL | Notes |
|----------|-----|------|-------|
| FIQ (highest) | ESM high-level (lockstep error) | D | Immediate relay de-energize |
| IRQ Channel 0 | DCAN1 new message | D | CAN message reception |
| IRQ Channel 1 | RTI compare 0 (10 ms tick) | D | Main loop tick |

### 8.2 Power Domain Dependencies

| ECU | Power Source | Kill Relay Gate | Behavior on Relay Open |
|-----|-------------|-----------------|----------------------|
| CVC | 12V -> Nucleo LDO -> 3.3V | Not gated | Continues operating |
| FZC | 12V -> Nucleo LDO -> 3.3V | Not gated | Continues operating (servos lose power) |
| RZC | 12V -> Nucleo LDO -> 3.3V | Not gated | Continues operating (motor driver loses power) |
| SC | 12V -> Dedicated LDO -> 3.3V | Not gated | Continues operating (controls relay) |
| Steering servo | 12V actuator rail | **Gated** | Loses power (mechanical hold) |
| Brake servo | 12V actuator rail | **Gated** | Loses power (mechanical hold) |
| BTS7960 | 12V actuator rail | **Gated** | Loses power (motor stops) |
| Relay coil | 12V main rail | SC GIO_A[0] | De-energized = contacts open |

<!-- HITL-LOCK START:COMMENT-BLOCK-HSI-SEC8 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Section 8 provides critical cross-ECU integration information. The interrupt priority assignment table correctly places E-stop at highest priority (0) on CVC, with SysTick at priority 2 (required for FreeRTOS). The SC uses FIQ (highest) for ESM lockstep error, ensuring immediate relay de-energize. The power domain dependency table (Section 8.2) is the most critical cross-ECU information: it clearly shows that all ECU compute boards remain powered when the kill relay opens, but actuators (steering servo, brake servo, BTS7960) lose power. This is correct -- ECUs must continue operating to log DTCs and maintain safe state monitoring. The SC's independent power path (not gated by kill relay) is correctly documented. One observation: the CVC, FZC, and RZC are powered via "Nucleo LDO" which is the Nucleo-64 board's onboard 3.3V regulator powered from 12V -- this should be verified for adequate current capacity when all peripherals are active.
<!-- HITL-LOCK END:COMMENT-BLOCK-HSI-SEC8 -->

## 9. Traceability

| HSI Section | SYS Requirement | HSR | SWR |
|------------|----------------|-----|-----|
| 4.1 CVC SPI1 | SYS-001, SYS-047 | HSR-CVC-001 | SWR-CVC-001, SWR-CVC-002 |
| 4.2.2 CVC FDCAN1 | SYS-031 | HSR-CVC-004 | SWR-CVC-014 to SWR-CVC-017 |
| 4.2.3 CVC I2C1 | SYS-044 | HSR-CVC-005 | SWR-CVC-026 to SWR-CVC-028 |
| 5.1 FZC TIM2 | SYS-010, SYS-014, SYS-050 | HSR-FZC-002, HSR-FZC-006 | SWR-FZC-008, SWR-FZC-009 |
| 5.2.2 FZC USART2 | SYS-018, SYS-048 | HSR-FZC-003 | SWR-FZC-013 |
| 5.1 FZC SPI2 | SYS-011, SYS-047 | HSR-FZC-001 | SWR-FZC-001 |
| 6.1 RZC TIM1 | SYS-004, SYS-050 | HSR-RZC-004 | SWR-RZC-003, SWR-RZC-004 |
| 6.2.3 RZC ADC1 | SYS-005, SYS-006, SYS-008, SYS-049 | HSR-RZC-001, HSR-RZC-002 | SWR-RZC-005, SWR-RZC-009, SWR-RZC-017 |
| 6.1 RZC TIM4 | SYS-009 | HSR-RZC-006 | SWR-RZC-012 |
| 7.4.1 SC DCAN1 | SYS-025 | HSR-SC-004 | SWR-SC-001, SWR-SC-002 |
| 7.2 SC GIO_A[0] | SYS-024 | HSR-SC-001 | SWR-SC-010, SWR-SC-011 |
| 7.8 SC Lockstep | SYS-026 | HSR-SC-005 | SWR-SC-014, SWR-SC-015 |
| 3.5 Startup | SYS-027, SYS-029 | All WDT HSRs | SWR-CVC-029, SWR-FZC-025, SWR-RZC-025, SWR-SC-019 |
| 3.4 MPU | SYS-052, SYS-053 | -- | SWR-CVC-032, SWR-FZC-029, SWR-RZC-028 |

## 10. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete HSI specification: 4 ECUs (CVC, FZC, RZC, SC) with pin mappings, peripheral configurations, memory maps, startup sequences, timing constraints, MPU configuration, safety HW features, interrupt priorities, power domain analysis, traceability |

