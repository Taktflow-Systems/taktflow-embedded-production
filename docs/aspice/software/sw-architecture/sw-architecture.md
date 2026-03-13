---
document_id: SWARCH
title: "Software Architecture"
version: "1.0"
status: draft
aspice_process: SWE.2
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

Every architecture element in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/software/lessons-learned/`](../lessons-learned/). One file per architecture element. File naming: `SWARCH-<element>.md`.


# Software Architecture — Taktflow Zonal Vehicle Platform

## 1. Purpose (SWE.2)

This document defines the software architectural design for the Taktflow Zonal Vehicle Platform per Automotive SPICE SWE.2 base practices. It establishes:

- The layered software architecture for all ECUs (4 physical + 3 simulated)
- Component decomposition per ECU with interfaces and data flow
- Static view: SWC provided/required ports and runnable entities
- Dynamic view: sequence diagrams for key operational and safety scenarios
- Task scheduling and timing budgets per ECU
- Resource estimates (Flash, RAM, CPU load) per ECU
- Memory layout and MPU configuration for ASIL D spatial isolation
- Error handling and DTC mapping strategy
- SSR-to-module allocation for bidirectional traceability

The Safety Controller (SC) uses a deliberately separate, flat bare-metal architecture with no BSW stack. This architectural diversity is an ISO 26262 principle — the independent safety monitor must not share software with the systems it monitors.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The purpose section accurately captures the SWE.2 scope and correctly establishes what this document covers. The statement about the SC using a deliberately flat architecture for diversity is well-motivated from an ISO 26262 Part 9 perspective. One gap: the purpose does not explicitly mention conformance to ISO 26262 Part 6 Section 7 architectural design principles (hierarchical structure, restricted coupling, spatial isolation) which are ++ at ASIL D and should be referenced as governing constraints.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC1 -->

## 2. Referenced Documents

| Document | ID | Path |
|----------|----|------|
| Master Plan | PLAN | docs/plans/master-plan.md |
| System Architecture | SYSARCH | docs/aspice/system/system-architecture.md |
| CAN Message Matrix | CAN-MATRIX | docs/aspice/system/can-message-matrix.md |
| SW Requirements — CVC | SWR-CVC | docs/aspice/software/sw-requirements/SWR-CVC.md |
| SW Requirements — FZC | SWR-FZC | docs/aspice/software/sw-requirements/SWR-FZC.md |
| SW Requirements — RZC | SWR-RZC | docs/aspice/software/sw-requirements/SWR-RZC.md |
| SW Requirements — SC | SWR-SC | docs/aspice/software/sw-requirements/SWR-SC.md |
| SW Requirements — BSW | SWR-BSW | docs/aspice/software/sw-requirements/SWR-BSW.md |
| BSW Architecture | BSW-ARCH | docs/aspice/software/sw-architecture/bsw-architecture.md |
| vECU Architecture | VECU-ARCH | docs/aspice/software/sw-architecture/vecu-architecture.md |
| HSI Specification | HSI | docs/safety/requirements/hsi-specification.md |
| ISO 26262:2018 Part 6 | — | Software level product development |
| AUTOSAR Classic Platform | — | R22-11 specification (reference only) |

## 3. Architecture Overview

### 3.1 Layered Model — Physical ECUs (CVC, FZC, RZC)

All three STM32-based zone controllers share an identical AUTOSAR Classic-inspired layered architecture. Application Software Components (SWCs) communicate exclusively through the RTE — never directly accessing BSW or hardware.

```
+===========================================================================+
|                     APPLICATION LAYER (SWCs)                              |
|  +------------+  +------------+  +------------+  +------------+           |
|  | Swc_Pedal  |  | Swc_Motor  |  | Swc_Steer  |  | Swc_Lidar  |  ...    |
|  | (CVC only) |  | (RZC only) |  | (FZC only) |  | (FZC only) |          |
|  +-----+------+  +-----+------+  +-----+------+  +-----+------+          |
|        |               |               |               |                  |
|   Rte_Write()     Rte_Read()      Rte_Write()     Rte_Read()              |
|   Rte_Read()      Rte_Write()     Rte_Read()      Rte_Write()             |
+========|===============|===============|===============|==================+
|        v               v               v               v                  |
|                  RTE (Runtime Environment)                                |
|  Signal buffers  |  Port connections  |  Runnable scheduling              |
|  (static RAM)    |  (compile-time)    |  (tick-driven)                    |
+========|===============|===============|===============|==================+
|        v               v               v               v                  |
|                  BSW SERVICES LAYER                                       |
|  +------+  +------+  +------+  +------+  +------+  +------+              |
|  | Com  |  | Dcm  |  | Dem  |  | WdgM |  | BswM |  | E2E  |             |
|  +--+---+  +--+---+  +--+---+  +--+---+  +--+---+  +--+---+             |
+======|========|========|========|========|========|========================+
|      v        v        v        v        v        v                       |
|                  ECU ABSTRACTION LAYER                                    |
|  +--------+  +--------+  +--------+                                      |
|  | CanIf  |  | PduR   |  | IoHwAb |                                      |
|  +---+----+  +---+----+  +---+----+                                      |
+=======|==========|==========|=============================================+
|       v          v          v                                             |
|                  MCAL (Microcontroller Abstraction Layer)                 |
|  +------+  +------+  +------+  +------+  +------+  +------+             |
|  | Can  |  | Spi  |  | Adc  |  | Pwm  |  | Dio  |  | Gpt  |            |
|  +--+---+  +--+---+  +--+---+  +--+---+  +--+---+  +--+---+            |
+=======|========|========|========|========|========|=======================+
|       v        v        v        v        v        v                      |
|                  HARDWARE (STM32G474RE — 170 MHz Cortex-M4F)             |
|  FDCAN1    SPI1/SPI2   ADC1      TIM2-4    GPIO      SysTick             |
+===========================================================================+
```

### 3.2 Flat Architecture — Safety Controller (SC)

The Safety Controller runs on the TMS570LC43x with dual Cortex-R5F lockstep cores. It uses a deliberately flat, bare-metal architecture with no RTOS, no AUTOSAR BSW, and no shared code with the zone controllers. This is mandated by ISO 26262 Part 9 (diverse redundancy / freedom from interference).

```
+===========================================================================+
|                     SAFETY CONTROLLER (TMS570LC43x)                       |
|                     ~400 LOC — bare-metal, no RTOS                        |
|                                                                           |
|  +--------------------+                                                   |
|  |     main.c         |  Simple super-loop:                               |
|  |  while(1) {        |    1. Poll DCAN1 mailboxes                        |
|  |    can_poll();      |    2. Check heartbeat counters                    |
|  |    hb_check();      |    3. Run cross-plausibility                      |
|  |    plaus_check();   |    4. Update kill relay                           |
|  |    relay_update();  |    5. Update fault LEDs                           |
|  |    led_update();    |    6. Feed external watchdog (if all OK)          |
|  |    wdt_feed();      |    7. Wait for 1 ms RTI tick                      |
|  |    rti_wait();      |                                                   |
|  |  }                  |                                                   |
|  +--------------------+                                                   |
|         |         |         |         |         |                         |
|         v         v         v         v         v                         |
|  +------+  +------+  +------+  +------+  +------+                        |
|  |DCAN1 |  | GIO  |  | RTI  |  | ESM  |  | VIM  |                       |
|  |(CAN) |  |(GPIO)|  |(Timer)|  |(Err) |  |(Int) |                       |
|  +------+  +------+  +------+  +------+  +------+                        |
|                                                                           |
|  HALCoGen-generated register-level drivers (TI-certified process)         |
+===========================================================================+
```

### 3.3 Simulated ECUs (BCM, ICU, TCU)

Simulated ECUs run identical BSW stack compiled for Linux (POSIX) with SocketCAN replacing the STM32 CAN MCAL. See [vECU Architecture](vecu-architecture.md) for details.

```
+===========================================================================+
|  SIMULATED ECU (Docker container on Linux)                                |
|                                                                           |
|  Same layered architecture as physical ECUs                               |
|  Only MCAL layer differs: Can_Posix.c (SocketCAN) instead of Can.c       |
|  No SPI, ADC, PWM, Dio, Gpt — not needed (all I/O is CAN-based)         |
|                                                                           |
|  Application SWCs  -->  RTE  -->  Com/Dcm/Dem  -->  PduR  -->  CanIf     |
|                                                                     |     |
|                                                              Can_Posix    |
|                                                                     |     |
|                                                              SocketCAN    |
|                                                            (vcan0/can0)   |
+===========================================================================+
```

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The three-tier architecture overview (layered physical ECUs, flat SC, simulated vECUs) is technically sound. The AUTOSAR-inspired layering for CVC/FZC/RZC correctly shows SWC-to-RTE-to-BSW separation with no direct hardware access from application code. The SC flat architecture with ~400 LOC and no shared code provides genuine architectural diversity per ISO 26262 Part 9. The simulated ECU diagram correctly identifies only the CAN MCAL as the portability boundary. One concern: the diagram for Section 3.1 shows 6 MCAL modules but does not mention the I2C peripheral used by CVC for the SSD1306 OLED -- this is referenced later in Section 4.1 but absent from the MCAL block diagram, which could be misleading.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC3 -->

## 4. Per-ECU Module Decomposition

### 4.1 Central Vehicle Computer (CVC) — STM32G474RE

#### Component Diagram

```
+===========================================================================+
|  CVC — Central Vehicle Computer                                           |
|                                                                           |
|  APPLICATION SWCs:                                                        |
|  +-------------+  +----------------+  +-------------+  +-----------+      |
|  | Swc_Pedal   |  | Swc_StateMach  |  | Swc_Display |  | Swc_EStop |     |
|  | Dual sensor |  | Vehicle state  |  | OLED output |  | GPIO ISR  |     |
|  | plausibility|  | transitions    |  | via I2C     |  | broadcast |     |
|  +------+------+  +-------+--------+  +------+------+  +-----+-----+     |
|         |                 |                   |               |           |
|  +------+-----------------+-------------------+---------------+------+    |
|  |                        RTE                                        |    |
|  +---+--------+--------+--------+--------+--------+--------+--------+    |
|      |        |        |        |        |        |        |              |
|  +---v--+ +---v--+ +---v--+ +---v--+ +---v--+ +---v--+ +---v--+         |
|  | Com  | | Dcm  | | Dem  | | WdgM | | BswM | | E2E  | | Rte  |         |
|  +---+--+ +---+--+ +------+ +---+--+ +------+ +------+ +------+         |
|      |        |                  |                                        |
|  +---v--+ +---v--+ +--------+   |                                        |
|  | PduR | | PduR | | IoHwAb |   |                                        |
|  +---+--+ +------+ +---+----+   |                                        |
|      |                  |        |                                        |
|  +---v--+          +----v---+ +--v---+                                    |
|  |CanIf |          |  Spi   | | Gpt  |                                    |
|  +---+--+          +---+----+ +--+---+                                    |
|      |                  |        |                                        |
|  +---v--+          +----v---+ +--v------+  +-------+  +-------+          |
|  | Can  |          | SPI1   | | SysTick |  | I2C1  |  | GPIO  |          |
|  +---+--+          +---+----+ +---------+  +---+---+  +---+---+          |
|      |                  |                      |           |              |
|  FDCAN1             AS5048A x2              SSD1306     E-stop            |
|  (CAN bus)          (pedal sensors)         (OLED)     (EXTI)            |
+===========================================================================+
```

#### Module Responsibility Table

| Module | Responsibility | ASIL | Period |
|--------|---------------|------|--------|
| Swc_Pedal | Read dual AS5048A via IoHwAb/SPI, compute plausibility, output PedalPosition and PedalFault | D | 10 ms |
| Swc_StateMachine | Manage vehicle state (INIT, RUN, DEGRADED, LIMP, SAFE_STOP), react to faults and E-stop | D | 10 ms |
| Swc_CanMaster | Aggregate heartbeats from FZC/RZC, send CVC heartbeat, coordinate CAN message timing | D | 50 ms |
| Swc_Display | Render vehicle state, speed, fault indicators on SSD1306 OLED via IoHwAb/I2C | QM | 100 ms |
| Swc_EStop | Detect E-stop GPIO interrupt, set EStopActive flag, trigger CAN broadcast via Com | D | ISR (event) |

#### Inter-Module Data Flow

```
AS5048A_1 --[SPI1]--> IoHwAb --> Rte --> Swc_Pedal --> Rte --> Com --> PduR --> CanIf --> Can --> CAN bus
AS5048A_2 --[SPI1]--> IoHwAb --> Rte --> Swc_Pedal --+
                                                      |
                                                      +--> Rte --> Swc_StateMachine --> Rte --> BswM
                                                      |
                                              (PedalFault) --> Dem --> DTC storage

E-stop btn --[GPIO/EXTI]--> Swc_EStop --> Rte --> Com --> CAN broadcast (0x001)

CAN RX <-- Can <-- CanIf <-- PduR <-- Com --> Rte --> Swc_StateMachine
(FZC/RZC heartbeats, feedback)
```

---

### 4.2 Front Zone Controller (FZC) — STM32G474RE

#### Component Diagram

```
+===========================================================================+
|  FZC — Front Zone Controller                                              |
|                                                                           |
|  APPLICATION SWCs:                                                        |
|  +-------------+  +------------+  +------------+  +------------+          |
|  | Swc_Steering|  | Swc_Brake  |  | Swc_Lidar  |  | Swc_Buzzer |         |
|  | Servo + ang |  | Servo ctrl |  | TFMini-S   |  | Warning    |         |
|  | rate limit  |  | auto-brake |  | dist parse |  | patterns   |         |
|  +------+------+  +-----+------+  +-----+------+  +-----+------+         |
|         |               |               |               |                 |
|  +------+--------------+---------------+---------------+------------+     |
|  |                        RTE                                        |    |
|  +---+--------+--------+--------+--------+--------+--------+--------+    |
|      |        |        |        |        |        |        |              |
|  +---v--+ +---v--+ +---v--+ +---v--+ +---v--+ +---v--+ +---v--+         |
|  | Com  | | Dcm  | | Dem  | | WdgM | | BswM | | E2E  | | Rte  |         |
|  +---+--+ +------+ +------+ +---+--+ +------+ +------+ +------+         |
|      |                           |                                        |
|  +---v--+ +--------+            |                                        |
|  | PduR | | IoHwAb |            |                                        |
|  +---+--+ +--+--+--+            |                                        |
|      |       |  |  |             |                                        |
|  +---v--+ +--v-+ +-v----+ +-----v-+ +-------+                            |
|  |CanIf | |Spi | | Pwm  | |  Gpt  | |  Dio  |                            |
|  +---+--+ +-+--+ +--+---+ +---+---+ +---+---+                            |
|      |       |       |         |         |                                |
|  FDCAN1   SPI2    TIM2/TIM3  SysTick   GPIO                              |
|           AS5048A  Servos x2           Buzzer                             |
|  (CAN)   (angle)  (steer/brake)                                          |
|                                                                           |
|  +--------+                                                               |
|  | USART1 | <-- TFMini-S lidar (DMA RX, FZC-specific, not in shared BSW) |
|  +--------+                                                               |
+===========================================================================+
```

#### Module Responsibility Table

| Module | Responsibility | ASIL | Period |
|--------|---------------|------|--------|
| Swc_Steering | Read steering command from Com, write servo PWM via IoHwAb, read angle feedback from SPI AS5048A, enforce rate limits and angle saturation, return-to-center on fault | D | 10 ms |
| Swc_Brake | Read brake command from Com, write brake servo PWM via IoHwAb, auto-brake on Com Rx timeout, force mapping | D | 10 ms |
| Swc_Lidar | Parse TFMini-S UART frames (DMA), filter distance readings, threshold check (warning at 200 cm, brake at 100 cm, emergency at 50 cm), report via Com | D | 10 ms |
| Swc_Buzzer | Drive piezo buzzer patterns via Dio: continuous (emergency), intermittent (warning), off (normal) | QM | 100 ms |

#### Inter-Module Data Flow

```
CAN RX (SteeringCmd 0x110) --> Can --> CanIf --> PduR --> Com --> Rte --> Swc_Steering
                                                                              |
                                                                    Rte --> IoHwAb --> Pwm --> TIM2 --> Servo
                                                                              |
AS5048A --[SPI2]--> IoHwAb --> Rte --> Swc_Steering --> Rte --> Com --> CAN TX (SteeringFb 0x210)

TFMini-S --[USART1/DMA]--> Swc_Lidar --> Rte --> Com --> CAN TX (LidarDist 0x220)
                                |
                                +--> (dist < 100cm) --> Rte --> Swc_Brake --> auto-brake

Com Rx timeout (no SteeringCmd for 100ms) --> Swc_Brake --> auto-brake --> Dem (DTC 0xC10100)
```

---

### 4.3 Rear Zone Controller (RZC) — STM32G474RE

#### Component Diagram

```
+===========================================================================+
|  RZC — Rear Zone Controller                                               |
|                                                                           |
|  APPLICATION SWCs:                                                        |
|  +-------------+  +---------------+  +--------------+  +-------------+    |
|  | Swc_Motor   |  | Swc_Current   |  | Swc_Temp     |  | Swc_Encoder |   |
|  | BTS7960 PWM |  | Monitor ACS723|  | Monitor NTC  |  | Speed/dir   |   |
|  | ramp + dir  |  | overcurrent   |  | derating     |  | from timer  |   |
|  +------+------+  +------+--------+  +------+-------+  +------+------+   |
|         |                |                   |                 |          |
|  +------+----------------+-------------------+-----------------+-----+    |
|  |                        RTE                                        |    |
|  +---+--------+--------+--------+--------+--------+--------+--------+    |
|      |        |        |        |        |        |        |              |
|  +---v--+ +---v--+ +---v--+ +---v--+ +---v--+ +---v--+ +---v--+         |
|  | Com  | | Dcm  | | Dem  | | WdgM | | BswM | | E2E  | | Rte  |         |
|  +---+--+ +------+ +------+ +---+--+ +------+ +------+ +------+         |
|      |                           |                                        |
|  +---v--+ +--------+            |                                        |
|  | PduR | | IoHwAb |            |                                        |
|  +---+--+ +--+--+--+            |                                        |
|      |       |  |  |             |                                        |
|  +---v--+ +--v-+ +-v----+ +-----v-+ +-------+                            |
|  |CanIf | |Adc | | Pwm  | |  Gpt  | |  Dio  |                            |
|  +---+--+ +-+--+ +--+---+ +---+---+ +---+---+                            |
|      |       |       |         |         |                                |
|  FDCAN1   ADC1    TIM3/TIM4  SysTick   GPIO                              |
|          ACS723   BTS7960              Enable                             |
|  (CAN)   NTC x3  (motor H-bridge)     pins                               |
|          Battery  TIM4 (encoder)                                          |
+===========================================================================+
```

#### Module Responsibility Table

| Module | Responsibility | ASIL | Period |
|--------|---------------|------|--------|
| Swc_Motor | Read TorqueRequest from Com, apply ramp limiting, set PWM duty via IoHwAb/Pwm, control direction via Dio, disable on fault | D | 10 ms |
| Swc_CurrentMonitor | Read motor current from ACS723 via IoHwAb/Adc, apply low-pass filter, overcurrent detection with debounce (10 ms window), report to Dem on trip, cut motor | D | 10 ms |
| Swc_TempMonitor | Read NTC temperatures via IoHwAb/Adc, apply derating curve (>60C derate, >80C shutdown), report to Dem on over-temp | D | 100 ms |
| Swc_Encoder | Read encoder pulses via timer capture (TIM4), compute speed (RPM) and direction, report via Com | QM | 10 ms |

#### Inter-Module Data Flow

```
CAN RX (TorqueReq 0x120) --> Can --> CanIf --> PduR --> Com --> Rte --> Swc_Motor
                                                                          |
                                                              Rte --> IoHwAb --> Pwm --> TIM3 --> BTS7960
                                                                          |
                                                              Rte --> IoHwAb --> Dio --> Enable pins

ACS723 --[ADC1]--> IoHwAb --> Rte --> Swc_CurrentMonitor --+--> Rte --> Swc_Motor (cutoff)
                                                            |
                                                            +--> Dem (DTC 0xC20100)

NTC --[ADC1]--> IoHwAb --> Rte --> Swc_TempMonitor --+--> Rte --> Swc_Motor (derate/stop)
                                                      |
                                                      +--> Dem (DTC 0xC20200)

TIM4 (encoder) --> IoHwAb --> Rte --> Swc_Encoder --> Rte --> Com --> CAN TX (MotorStatus 0x200)
```

---

### 4.4 Safety Controller (SC) — TMS570LC43x

#### Component Diagram

```
+===========================================================================+
|  SC — Safety Controller (bare-metal, NO BSW, NO RTOS)                     |
|                                                                           |
|  +------------------------------------------------------------------+     |
|  |  main.c (~400 LOC)                                                |     |
|  |                                                                   |     |
|  |  +------------------+  +------------------+  +------------------+ |     |
|  |  | can_monitor.c    |  | heartbeat.c      |  | plausibility.c   | |     |
|  |  | Poll DCAN1 RX    |  | Per-ECU alive    |  | Torque vs current| |     |
|  |  | mailboxes,       |  | counter tracking,|  | cross-check,     | |     |
|  |  | decode messages  |  | timeout detect   |  | E-stop verify    | |     |
|  |  +--------+---------+  +--------+---------+  +--------+---------+ |     |
|  |           |                      |                      |         |     |
|  |  +--------+---------+  +--------+---------+                       |     |
|  |  | relay.c          |  | led_panel.c      |                       |     |
|  |  | Kill relay GPIO  |  | 4x red/green LED |                       |     |
|  |  | via MOSFET,      |  | per-ECU health   |                       |     |
|  |  | energize-to-run  |  | status display   |                       |     |
|  |  +--------+---------+  +--------+---------+                       |     |
|  |           |                      |                                |     |
|  |  +--------+---------+                                             |     |
|  |  | watchdog.c       |                                             |     |
|  |  | TPS3823 feed via |                                             |     |
|  |  | GIO toggle, only |                                             |     |
|  |  | if all checks OK |                                             |     |
|  |  +------------------+                                             |     |
|  +------------------------------------------------------------------+     |
|                                                                           |
|  HALCoGen register-level drivers:                                         |
|  +--------+  +--------+  +--------+  +--------+  +--------+              |
|  | DCAN1  |  |  GIO   |  |  RTI   |  |  ESM   |  |  VIM   |             |
|  | (CAN   |  | (GPIO) |  |(Timer) |  |(Error  |  |(Vector |             |
|  | silent)|  |        |  | 1ms    |  | Signal)|  | Int)   |             |
|  +--------+  +--------+  +--------+  +--------+  +--------+              |
+===========================================================================+
```

#### Module Responsibility Table

| Module | Responsibility | ASIL | Execution |
|--------|---------------|------|-----------|
| can_monitor.c | Poll DCAN1 mailboxes (3 mailboxes: CVC 0x010, FZC 0x011, RZC 0x012), decode heartbeat and status messages, extract alive counters and torque/current values | D | Every main loop iteration |
| heartbeat.c | Track per-ECU alive counter (expected increment each 50 ms), detect timeout (no valid heartbeat within 100 ms), set per-ECU fault flag | D | Every main loop iteration |
| plausibility.c | Cross-check torque request (from CVC CAN) vs actual motor current (from RZC CAN): if |expected - actual| > threshold for > 50 ms, flag plausibility fault; verify E-stop CAN message matches expected pattern | D | Every main loop iteration |
| relay.c | Control kill relay via GIO pin (drives IRLZ44N MOSFET gate): energize-to-run pattern (relay ON = system active, relay OFF = safe state). De-energize on any: heartbeat timeout, plausibility fault, E-stop received, SC internal fault | D | Every main loop iteration |
| led_panel.c | Drive 4 bi-color LED pairs (red/green) via GIO: one pair per monitored ECU (CVC, FZC, RZC) + one for SC self-status. Green = healthy, Red = fault | QM | Every main loop iteration |
| watchdog.c | Toggle TPS3823 WDI pin via GIO. Only toggle if: all heartbeats valid, no plausibility faults, no E-stop, relay energized. If any check fails, stop toggling — TPS3823 times out in 1.6 s and resets SC | D | Every main loop iteration |

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The per-ECU decomposition is thorough and well-structured. Component diagrams clearly show the data flow from sensor to actuator through the BSW stack. Module responsibility tables correctly assign ASIL levels (D for safety-critical SWCs, QM for Swc_Display, Swc_Buzzer, Swc_Encoder). The SC module table correctly shows all modules except led_panel.c as ASIL D, and the ~400 LOC budget is appropriate for a bare-metal safety monitor. The inter-module data flow diagrams for all four ECUs are complete and consistent with the CAN message matrix. One observation: The FZC lidar UART (USART1 DMA) is noted as "not in shared BSW" which is correct, but the rationale for keeping it ECU-specific rather than integrating into the BSW Spi/IoHwAb pattern should be documented -- likely because UART/DMA is FZC-only and does not justify a generic BSW module.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC4 -->

## 5. Static View — Component Interfaces

### 5.1 CVC Software Component Interfaces

#### Swc_Pedal

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpPedalSensor1 | PedalAngle1_Raw | uint16 | 0.01 deg | 0..16383 |
| Required (Read) | RpPedalSensor2 | PedalAngle2_Raw | uint16 | 0.01 deg | 0..16383 |
| Provided (Write) | PpPedalPosition | PedalPosition_Pct | uint8 | % | 0..100 |
| Provided (Write) | PpPedalFault | PedalFault_Status | uint8 | enum | 0=OK, 1=Disagree, 2=S1Fail, 3=S2Fail, 4=BothFail |

**Runnable: Swc_Pedal_10ms** — Trigger: 10 ms periodic (OS task)
```c
void Swc_Pedal_10ms(void)
{
    uint16 s1, s2;
    Rte_Read_RpPedalSensor1_PedalAngle1_Raw(&s1);   /* via IoHwAb -> Spi -> AS5048A #1 */
    Rte_Read_RpPedalSensor2_PedalAngle2_Raw(&s2);   /* via IoHwAb -> Spi -> AS5048A #2 */

    /* Plausibility: |S1 - S2| must be < 5% of full scale (819 counts) */
    uint16 diff = (s1 > s2) ? (s1 - s2) : (s2 - s1);
    if (diff > PEDAL_PLAUS_THRESHOLD) {
        Rte_Write_PpPedalFault_PedalFault_Status(PEDAL_FAULT_DISAGREE);
        Rte_Write_PpPedalPosition_PedalPosition_Pct(0u);
        Dem_ReportErrorStatus(DEM_EVENT_PEDAL_PLAUSIBILITY, DEM_EVENT_STATUS_FAILED);
    } else {
        uint8 pct = (uint8)(((uint32)(s1 + s2) * 100u) / (2u * 16383u));
        Rte_Write_PpPedalPosition_PedalPosition_Pct(pct);
        Rte_Write_PpPedalFault_PedalFault_Status(PEDAL_FAULT_NONE);
    }
}
```

#### Swc_StateMachine

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpPedalFault | PedalFault_Status | uint8 | enum | 0..4 |
| Required (Read) | RpPedalPosition | PedalPosition_Pct | uint8 | % | 0..100 |
| Required (Read) | RpEStop | EStopActive | boolean | — | TRUE/FALSE |
| Required (Read) | RpMotorFault | MotorFault_Status | uint8 | enum | 0..3 |
| Required (Read) | RpSteeringFault | SteeringFault_Status | uint8 | enum | 0..2 |
| Provided (Write) | PpVehicleState | VehicleState | uint8 | enum | 0=INIT, 1=RUN, 2=DEGRADED, 3=LIMP, 4=SAFE_STOP |
| Provided (Write) | PpTorqueRequest | TorqueRequest_Pct | uint8 | % | 0..100 |
| Provided (Write) | PpSteeringCmd | SteeringCommand_Deg | int16 | 0.1 deg | -450..+450 |
| Provided (Write) | PpBrakeCmd | BrakeCommand_Pct | uint8 | % | 0..100 |

**Runnable: Swc_StateMachine_10ms** — Trigger: 10 ms periodic

State transition table:

| Current State | Event | Next State | Action |
|--------------|-------|------------|--------|
| INIT | All BSW initialized | RUN | Enable Com TX, start heartbeat |
| RUN | PedalFault == DISAGREE | DEGRADED | Set TorqueRequest = 50% of pedal (limp) |
| RUN | PedalFault == S1Fail or S2Fail | DEGRADED | Use remaining sensor, set limp mode |
| RUN | EStopActive == TRUE | SAFE_STOP | TorqueRequest = 0, BrakeCmd = 100% |
| RUN | MotorFault != OK | DEGRADED | Reduce TorqueRequest |
| DEGRADED | PedalFault == BothFail | SAFE_STOP | TorqueRequest = 0, BrakeCmd = 100% |
| DEGRADED | Fault cleared | RUN | Resume normal operation |
| DEGRADED | Timeout (30 s in DEGRADED) | SAFE_STOP | Force safe state |
| ANY | EStopActive == TRUE | SAFE_STOP | Immediate torque cutoff |
| SAFE_STOP | Reset command (manual) | INIT | Restart initialization |

#### Swc_CanMaster

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpFzcHeartbeat | FzcAliveCounter | uint8 | count | 0..15 |
| Required (Read) | RpRzcHeartbeat | RzcAliveCounter | uint8 | count | 0..15 |
| Provided (Write) | PpCvcHeartbeat | CvcAliveCounter | uint8 | count | 0..15 |
| Provided (Write) | PpFzcStatus | FzcCommStatus | uint8 | enum | 0=OK, 1=Timeout |
| Provided (Write) | PpRzcStatus | RzcCommStatus | uint8 | enum | 0=OK, 1=Timeout |

**Runnable: Swc_CanMaster_50ms** — Trigger: 50 ms periodic

#### Swc_Display

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpVehicleState | VehicleState | uint8 | enum | 0..4 |
| Required (Read) | RpMotorSpeed | MotorSpeed_Rpm | uint16 | RPM | 0..10000 |
| Required (Read) | RpPedalFault | PedalFault_Status | uint8 | enum | 0..4 |
| Required (Read) | RpMotorFault | MotorFault_Status | uint8 | enum | 0..3 |

**Runnable: Swc_Display_100ms** — Trigger: 100 ms periodic

#### Swc_EStop

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Provided (Write) | PpEStop | EStopActive | boolean | — | TRUE/FALSE |

**Runnable: Swc_EStop_ISR** — Trigger: GPIO EXTI falling edge (event-driven)

On E-stop activation, this runnable immediately sets EStopActive = TRUE and triggers an immediate Com transmission of the E-stop broadcast message (0x001). The ISR latency from GPIO edge to CAN TX must be < 1 ms.

---

### 5.2 FZC Software Component Interfaces

#### Swc_Steering

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpSteeringCmd | SteeringCommand_Deg | int16 | 0.1 deg | -450..+450 |
| Required (Read) | RpAngleSensor | SteeringAngle_Raw | uint16 | 0.01 deg | 0..16383 |
| Provided (Write) | PpSteeringFb | SteeringAngle_Actual | int16 | 0.1 deg | -450..+450 |
| Provided (Write) | PpSteeringFault | SteeringFault_Status | uint8 | enum | 0=OK, 1=SensorFail, 2=Jammed |

**Runnable: Swc_Steering_10ms** — Trigger: 10 ms periodic

Rate limiting: max 45 deg/s (4.5 counts per 10 ms). On sensor failure: servo returns to center (0 deg) at reduced rate, Dem_ReportErrorStatus called.

#### Swc_Brake

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpBrakeCmd | BrakeCommand_Pct | uint8 | % | 0..100 |
| Required (Read) | RpEmergencyBrake | EmergencyBrakeReq | boolean | — | TRUE/FALSE |
| Provided (Write) | PpBrakeStatus | BrakeStatus | uint8 | enum | 0=Released, 1=Applied, 2=Emergency |

**Runnable: Swc_Brake_10ms** — Trigger: 10 ms periodic

Auto-brake: if no valid BrakeCommand received via Com for 100 ms (Rx timeout), apply full braking (100%) as safe state. Emergency brake overrides normal brake command.

#### Swc_Lidar

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Provided (Write) | PpLidarDistance | LidarDistance_Cm | uint16 | cm | 0..1200 |
| Provided (Write) | PpLidarValid | LidarDataValid | boolean | — | TRUE/FALSE |
| Provided (Write) | PpObjectDetected | ObjectDetected | uint8 | enum | 0=None, 1=Warning, 2=Brake, 3=Emergency |

**Runnable: Swc_Lidar_10ms** — Trigger: 10 ms periodic

TFMini-S frame format: 0x59 0x59 [DistL] [DistH] [StrL] [StrH] [Temp] [Checksum] (9 bytes at 115200 baud). Parsed from USART1 DMA ring buffer.

Thresholds:
- > 200 cm: ObjectDetected = None
- 100..200 cm: ObjectDetected = Warning (buzzer intermittent)
- 50..100 cm: ObjectDetected = Brake (auto-brake applied)
- < 50 cm: ObjectDetected = Emergency (emergency brake, E-stop broadcast)

#### Swc_Buzzer

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpObjectDetected | ObjectDetected | uint8 | enum | 0..3 |
| Required (Read) | RpVehicleState | VehicleState | uint8 | enum | 0..4 |

**Runnable: Swc_Buzzer_100ms** — Trigger: 100 ms periodic

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC5 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The static view provides well-defined SWC interfaces with typed RTE ports, signal names, units, and ranges for all CVC, FZC, and RZC components. The Swc_Pedal_10ms code snippet demonstrates the dual-sensor plausibility check correctly (|S1-S2| against threshold). The state transition table for Swc_StateMachine is comprehensive, covering all fault-to-state mappings with explicit safe-state actions. The E-stop ISR latency requirement (< 1 ms GPIO to CAN TX) is properly specified. One gap: the Swc_CanMaster interface shows heartbeat monitoring ports but does not specify the E2E check result as an input -- the alive counter is shown, but there is no port for E2E_Status which would be needed to reject heartbeats with CRC errors before incrementing the alive counter.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC5 -->

---

### 5.3 RZC Software Component Interfaces

#### Swc_Motor

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpTorqueRequest | TorqueRequest_Pct | uint8 | % | 0..100 |
| Required (Read) | RpCurrentFault | CurrentFault | boolean | — | TRUE/FALSE |
| Required (Read) | RpTempFault | TempFault | uint8 | enum | 0=OK, 1=Derate, 2=Shutdown |
| Provided (Write) | PpMotorDuty | MotorDuty_Pct | uint8 | % | 0..100 |
| Provided (Write) | PpMotorDirection | MotorDirection | uint8 | enum | 0=Stop, 1=Forward, 2=Reverse |

**Runnable: Swc_Motor_10ms** — Trigger: 10 ms periodic

Ramp limiting: max 10% duty change per 10 ms (1 second 0-to-100%). If CurrentFault == TRUE or TempFault == Shutdown, MotorDuty = 0 immediately (no ramp). If TempFault == Derate, MotorDuty capped at 50%.

#### Swc_CurrentMonitor

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpMotorCurrent | MotorCurrent_mA | uint16 | mA | 0..30000 |
| Provided (Write) | PpCurrentFault | CurrentFault | boolean | — | TRUE/FALSE |
| Provided (Write) | PpCurrentValue | MotorCurrent_Filtered | uint16 | mA | 0..30000 |

**Runnable: Swc_CurrentMonitor_10ms** — Trigger: 10 ms periodic

ACS723 produces 0.4V/A with 2.5V at 0A. ADC conversion: I(mA) = ((ADC_raw * 3300 / 4096) - 2500) / 0.4. Low-pass filter: IIR alpha = 0.1. Overcurrent threshold: 10000 mA sustained for 10 consecutive samples (100 ms debounce).

#### Swc_TempMonitor

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpMotorTemp | MotorTemp_Raw | uint16 | ADC counts | 0..4095 |
| Provided (Write) | PpTempFault | TempFault | uint8 | enum | 0=OK, 1=Derate, 2=Shutdown |
| Provided (Write) | PpMotorTempC | MotorTemp_DegC | int16 | 0.1 degC | -400..+1500 |

**Runnable: Swc_TempMonitor_100ms** — Trigger: 100 ms periodic

NTC 10K thermistor with Steinhart-Hart approximation (lookup table, 16 entries). Derating curve: linear reduction from 100% at 60C to 50% at 70C to 0% at 80C.

#### Swc_Encoder

| Direction | Port | Signal | Type | Unit | Range |
|-----------|------|--------|------|------|-------|
| Required (Read) | RpEncoderCount | EncoderPulses | uint32 | pulses | 0..UINT32_MAX |
| Provided (Write) | PpMotorSpeed | MotorSpeed_Rpm | uint16 | RPM | 0..10000 |
| Provided (Write) | PpMotorDir | MotorActualDir | uint8 | enum | 0=Stop, 1=Fwd, 2=Rev |

**Runnable: Swc_Encoder_10ms** — Trigger: 10 ms periodic

## 6. Dynamic View — Sequence Diagrams

### 6.1 Normal Drive Cycle: Pedal to Motor

```
Driver    AS5048A    CVC:Swc_Pedal    CVC:Com     CAN Bus    RZC:Com    RZC:Swc_Motor    BTS7960
  |          |            |              |           |          |             |              |
  | press    |            |              |           |          |             |              |
  | pedal    |            |              |           |          |             |              |
  +--------->|            |              |           |          |             |              |
  |          | SPI data   |              |           |          |             |              |
  |          +----------->| read S1,S2   |           |          |             |              |
  |          |            | plausibility |           |          |             |              |
  |          |            | check OK     |           |          |             |              |
  |          |            |              |           |          |             |              |
  |          |            | Rte_Write    |           |          |             |              |
  |          |            | PedalPos=75% |           |          |             |              |
  |          |            +-----.------->|           |          |             |              |
  |          |            |    SM maps   |           |          |             |              |
  |          |            |    pedal to  |           |          |             |              |
  |          |            |    torque    |           |          |             |              |
  |          |            |              | TX 0x120  |          |             |              |
  |          |            |              | TorqueReq |          |             |              |
  |          |            |              | =75%      |          |             |              |
  |          |            |              +---------->|          |             |              |
  |          |            |              |           | RX 0x120 |             |              |
  |          |            |              |           +--------->| Rte_Read   |              |
  |          |            |              |           |          | TorqueReq  |              |
  |          |            |              |           |          +----------->| ramp limit   |
  |          |            |              |           |          |            | set PWM      |
  |          |            |              |           |          |            +------------->|
  |          |            |              |           |          |             |   motor runs |
  |          |            |              |           |          |             |              |
  |          |            |              |           | TX 0x200 |             |              |
  |          |            |              |           |<---------+ MotorStatus |              |
  |          |            |              |           |          | speed, I, T |              |
```

Timing: Pedal SPI read to CAN TX: < 1 ms. CAN propagation: < 1 ms. Total pedal-to-motor latency: < 12 ms (within one 10 ms control cycle + CAN).

### 6.2 Pedal Fault Detection: Sensor Disagreement

```
AS5048A#1  AS5048A#2   CVC:Swc_Pedal   CVC:Dem     CVC:StateMachine   CVC:Com    CAN     RZC:Motor
   |           |            |              |              |               |         |         |
   | S1=4000   |            |              |              |               |         |         |
   +---------->|            |              |              |               |         |         |
   |           | S2=8000    |              |              |               |         |         |
   |           +----------->| |4000-8000|  |              |               |         |         |
   |           |            | = 4000       |              |               |         |         |
   |           |            | > 819 thresh |              |               |         |         |
   |           |            |              |              |               |         |         |
   |           |            | Rte_Write    |              |               |         |         |
   |           |            | Fault=DISAGR |              |               |         |         |
   |           |            | Pedal=0%     |              |               |         |         |
   |           |            +-----.------->|              |               |         |         |
   |           |            |    report    |              |               |         |         |
   |           |            |    DTC       |              |               |         |         |
   |           |            |    0xC00100  |              |               |         |         |
   |           |            +-----------+->| store DTC    |               |         |         |
   |           |            |           |  |              |               |         |         |
   |           |            |           |  |  Rte_Read    |               |         |         |
   |           |            |           |  |  Fault       |               |         |         |
   |           |            |           |  +<-------.-----+               |         |         |
   |           |            |           |  |  RUN->DEGRADED               |         |         |
   |           |            |           |  |  TorqueReq=0 |               |         |         |
   |           |            |           |  |              +-->  TX 0x120  |         |         |
   |           |            |           |  |              |    Torque=0   |         |         |
   |           |            |           |  |              +-------------->|         |         |
   |           |            |           |  |              |               | RX      |         |
   |           |            |           |  |              |               +-------->| PWM=0   |
   |           |            |           |  |              |               |         | motor   |
   |           |            |           |  |              |               |         | stops   |
```

Fault detection to motor stop: < 22 ms (10 ms pedal cycle + 10 ms motor cycle + CAN).

### 6.3 E-Stop Sequence

```
E-stop      CVC:EStop_ISR    CVC:Com      CAN Bus      FZC        RZC        SC
button         |                |            |           |          |          |
  |            |                |            |           |          |          |
  | pressed    |                |            |           |          |          |
  | (falling   |                |            |           |          |          |
  |  edge)     |                |            |           |          |          |
  +----------->| EXTI ISR       |            |           |          |          |
  |            | Rte_Write      |            |           |          |          |
  |            | EStop=TRUE     |            |           |          |          |
  |            +--------------->| immediate  |           |          |          |
  |            |                | TX 0x001   |           |          |          |
  |            |                | EStop bcast|           |          |          |
  |            |                +----------->|           |          |          |
  |            |                |            |           |          |          |
  |            |                |            | RX 0x001  |          |          |
  |            |                |            +---------->| brake    |          |
  |            |                |            |           | 100%     |          |
  |            |                |            |           | servo    |          |
  |            |                |            |           |          |          |
  |            |                |            | RX 0x001  |          |          |
  |            |                |            +---------->+--------->| PWM=0    |
  |            |                |            |           |          | motor    |
  |            |                |            |           |          | off      |
  |            |                |            |           |          |          |
  |            |                |            | RX 0x001  |          |          |
  |            |                |            +---------->+--------->+--------->|
  |            |                |            |           |          |          | verify
  |            |                |            |           |          |          | E-stop
  |            |                |            |           |          |          | CAN msg
  |            |                |            |           |          |          | matches
  |            |                |            |           |          |          | (redundant
  |            |                |            |           |          |          |  check)
```

E-stop GPIO to CAN broadcast: < 1 ms (ISR latency). CAN to all ECUs: < 1 ms. Total E-stop to motor stop: < 12 ms.

### 6.4 CAN Timeout — FZC Auto-Brake

```
CVC            CAN Bus        FZC:Com           FZC:Swc_Brake     Brake Servo
 |               |              |                    |                |
 | TX 0x110      |              |                    |                |
 | SteeringCmd   |              |                    |                |
 | (every 10ms)  |              |                    |                |
 +-------------->| RX 0x110     |                    |                |
 |               +------------->| reset timeout      |                |
 |               |              | timer              |                |
 |               |              +-------------------.+                |
 |               |              |                    | normal steer   |
 |               |              |                    |                |
 | CVC hangs     |              |                    |                |
 | or CAN        |              |                    |                |
 | disconnects   |              |                    |                |
 |               |              |                    |                |
 |   ...100ms passes...         |                    |                |
 |               |              |                    |                |
 |               |              | Com Rx timeout     |                |
 |               |              | (100 ms elapsed)   |                |
 |               |              +---.--------------->| Rx timeout!    |
 |               |              |   |                | auto-brake     |
 |               |              |   |                | BrakeCmd=100%  |
 |               |              |   |                +--------------->|
 |               |              |   |                |   full brake   |
 |               |              |   |                |                |
 |               |              |   Dem_Report       |                |
 |               |              |   DTC 0xC10100     |                |
 |               |              |   (CAN timeout)    |                |
```

### 6.5 SC Kill Relay Activation: Heartbeat Timeout

```
CVC        FZC        RZC        CAN Bus       SC:heartbeat    SC:relay    Kill Relay
 |          |          |            |              |               |           |
 | HB 0x010 |          |            |              |               |           |
 +--------->+--------->+----------->| RX all 3 HBs |               |           |
 |          |          |            +------------->| alive counters |           |
 |          |          |            |              | valid, reset   |           |
 |          |          |            |              | timeout timers |           |
 |          |          |            |              |               |           |
 | CVC hangs (no more heartbeats)  |              |               |           |
 |          |          |            |              |               |           |
 |   ...100ms passes (CVC timeout) |              |               |           |
 |          |          |            |              |               |           |
 |          |          |            |              | CVC timeout!   |           |
 |          |          |            |              | hb_fault[CVC]  |           |
 |          |          |            |              | = TRUE         |           |
 |          |          |            |              +--------------->| fault     |
 |          |          |            |              |               | detected  |
 |          |          |            |              |               |           |
 |          |          |            |              | confirmation:  |           |
 |          |          |            |              | wait 1 more    |           |
 |          |          |            |              | cycle (1ms)    |           |
 |          |          |            |              | re-check       |           |
 |          |          |            |              +--------------->|           |
 |          |          |            |              |               | confirmed |
 |          |          |            |              |               | de-energize|
 |          |          |            |              |               +---------->|
 |          |          |            |              |               |    relay   |
 |          |          |            |              |               |    opens   |
 |          |          |            |              |               |    12V cut |
 |          |          |            |              |               |    SAFE    |
 |          |          |            |              | LED: CVC=RED  |           |
 |          |          |            |              | stop WDT feed |           |
```

SC heartbeat timeout to relay open: < 102 ms (100 ms timeout + 1 ms confirmation + 1 ms relay actuation). TPS3823 backup: if SC itself hangs, watchdog resets SC in 1.6 s.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC6 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The five sequence diagrams cover the critical operational and safety scenarios: normal drive cycle, pedal fault detection, E-stop, CAN timeout auto-brake, and SC kill relay activation. Timing budgets are explicitly stated and realistic (pedal-to-motor < 12 ms, E-stop to motor stop < 12 ms, fault detection to motor stop < 22 ms, SC timeout to relay open < 102 ms). These are consistent with the FTTI values that would be expected for a low-speed vehicle platform. The SC kill relay sequence correctly shows a confirmation cycle before de-energizing, which prevents spurious relay trips from single-sample CAN glitches. One concern: no sequence diagram is provided for the CAN bus-off recovery scenario, which is an ASIL D safety mechanism and should be illustrated to show the recovery timing and fallback behavior.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC6 -->

## 7. Task Scheduling

### 7.1 CVC Task Table

| Task | Period | Priority | Runnables | WCET Budget | Stack |
|------|--------|----------|-----------|-------------|-------|
| Task_1ms | 1 ms | Highest (5) | SysTick handler, Gpt_MainFunction | 50 us | 256 B |
| Task_EStop_ISR | Event (EXTI) | ISR (above all tasks) | Swc_EStop_ISR | 20 us | 256 B |
| Task_10ms_Control | 10 ms | High (4) | Swc_Pedal_10ms, Swc_StateMachine_10ms, Com_MainFunction_Rx, Com_MainFunction_Tx, E2E_Check, Can_MainFunction_Read | 2 ms | 1024 B |
| Task_50ms_Heartbeat | 50 ms | Medium (3) | Swc_CanMaster_50ms, WdgM_MainFunction, WdgM_CheckpointReached | 500 us | 512 B |
| Task_100ms_Display | 100 ms | Low (2) | Swc_Display_100ms, Dem_MainFunction, BswM_MainFunction | 5 ms | 512 B |
| Task_Background | Idle | Lowest (1) | Dcm_MainFunction (UDS processing), Dem_MainFunction (NVM write) | N/A | 512 B |

**Total stack: 3072 B (3 KB)**

### 7.2 FZC Task Table

| Task | Period | Priority | Runnables | WCET Budget | Stack |
|------|--------|----------|-----------|-------------|-------|
| Task_1ms | 1 ms | Highest (5) | SysTick handler, Gpt_MainFunction | 50 us | 256 B |
| Task_10ms_Control | 10 ms | High (4) | Swc_Steering_10ms, Swc_Brake_10ms, Swc_Lidar_10ms, Com_MainFunction_Rx, Com_MainFunction_Tx, E2E_Check, Can_MainFunction_Read | 3 ms | 1024 B |
| Task_50ms_Heartbeat | 50 ms | Medium (3) | WdgM_MainFunction, WdgM_CheckpointReached, heartbeat TX | 500 us | 512 B |
| Task_100ms_Buzzer | 100 ms | Low (2) | Swc_Buzzer_100ms, Dem_MainFunction, BswM_MainFunction | 2 ms | 512 B |
| Task_Background | Idle | Lowest (1) | Dcm_MainFunction | N/A | 512 B |

**Total stack: 2816 B (2.75 KB)**

### 7.3 RZC Task Table

| Task | Period | Priority | Runnables | WCET Budget | Stack |
|------|--------|----------|-----------|-------------|-------|
| Task_1ms | 1 ms | Highest (5) | SysTick handler, Gpt_MainFunction | 50 us | 256 B |
| Task_10ms_Control | 10 ms | High (4) | Swc_Motor_10ms, Swc_CurrentMonitor_10ms, Swc_Encoder_10ms, Com_MainFunction_Rx, Com_MainFunction_Tx, E2E_Check, Can_MainFunction_Read | 2.5 ms | 1024 B |
| Task_50ms_Heartbeat | 50 ms | Medium (3) | WdgM_MainFunction, WdgM_CheckpointReached, heartbeat TX | 500 us | 512 B |
| Task_100ms_Temp | 100 ms | Low (2) | Swc_TempMonitor_100ms, Dem_MainFunction, BswM_MainFunction | 2 ms | 512 B |
| Task_Background | Idle | Lowest (1) | Dcm_MainFunction | N/A | 512 B |

**Total stack: 2816 B (2.75 KB)**

### 7.4 SC Task Table (No RTOS — super-loop)

| Execution | Period | Content | WCET Budget |
|-----------|--------|---------|-------------|
| RTI ISR | 1 ms | Set tick flag, increment ms counter | 5 us |
| Main loop — CAN poll | 1 ms (tick-gated) | Read DCAN1 mailboxes (3 mailboxes), decode messages | 50 us |
| Main loop — Heartbeat check | 1 ms (tick-gated) | Check 3 alive counters against timeout (100 ms window) | 20 us |
| Main loop — Plausibility | 1 ms (tick-gated) | Cross-check torque request vs motor current | 20 us |
| Main loop — Relay control | 1 ms (tick-gated) | Update kill relay GPIO based on fault flags | 5 us |
| Main loop — LED update | 1 ms (tick-gated) | Update 4 LED pairs based on per-ECU status | 10 us |
| Main loop — WDT feed | 1 ms (tick-gated) | Toggle TPS3823 WDI pin if all checks passed | 5 us |

**Total loop time: ~115 us per 1 ms tick (< 12% of available time at 300 MHz)**

**Stack: 1024 B (single stack, no RTOS)**

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC7 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Task scheduling tables for all four ECUs are complete with period, priority, runnables, WCET budgets, and stack sizes. The priority assignment follows correct safety practice: safety-critical 10 ms control tasks at high priority, heartbeat/watchdog at medium, display/diagnostics at low. The SC super-loop budget (115 us per 1 ms tick, < 12% CPU) provides massive margin on the 300 MHz TMS570. WCET budgets are reasonable estimates but are marked as budgets, not measured values -- this is appropriate for the architecture phase, but actual WCET measurement/analysis is required at SWE.3/SWE.4. One concern: the CVC Task_10ms_Control bundles Swc_Pedal, Swc_StateMachine, and all Com/E2E/CAN runnables into a single 2 ms WCET budget. If any of these runnables overruns, all of them are affected. Consider documenting per-runnable WCET sub-budgets.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC7 -->

## 8. Resource Estimates

### 8.1 Per-ECU Resource Table

| ECU | Flash (est.) | RAM (est.) | CPU Load (est.) | Notes |
|-----|-------------|------------|-----------------|-------|
| CVC | ~60 KB | ~20 KB | ~15% @ 170 MHz | Heaviest: dual SPI, OLED I2C, state machine, CAN master |
| FZC | ~55 KB | ~18 KB | ~12% @ 170 MHz | UART DMA for lidar, dual servo PWM, SPI angle sensor |
| RZC | ~50 KB | ~16 KB | ~10% @ 170 MHz | ADC DMA scan (3 channels), PWM motor, encoder timer |
| SC | ~8 KB | ~2 KB | ~1% @ 300 MHz | Minimal: CAN poll, GPIO, timer. Lockstep = full CPU for safety |

### 8.2 Flash Breakdown (CVC — largest ECU)

| Component | Estimated Size | Notes |
|-----------|---------------|-------|
| BSW stack (shared) | ~25 KB | Com, Dcm, Dem, WdgM, BswM, E2E, CanIf, PduR, IoHwAb, Rte |
| MCAL drivers | ~10 KB | Can, Spi, Adc, Pwm, Dio, Gpt (STM32 HAL wrappers) |
| STM32 HAL (used functions) | ~8 KB | FDCAN, SPI, I2C, GPIO, TIM, ADC HAL functions (link-time optimized) |
| Application SWCs | ~8 KB | Swc_Pedal, Swc_StateMachine, Swc_CanMaster, Swc_Display, Swc_EStop |
| Configuration tables | ~3 KB | Com_SignalConfig, E2E_Config, Dem_DtcConfig, Rte port tables |
| Startup + vector table | ~2 KB | Reset handler, SystemInit, vector table |
| Lookup tables (NTC, etc.) | ~1 KB | Temperature conversion, pedal linearization |
| Padding + alignment | ~3 KB | Section alignment, const padding |
| **Total** | **~60 KB** | **11.7% of 512 KB Flash** |

### 8.3 RAM Breakdown (CVC — largest ECU)

| Component | Estimated Size | Notes |
|-----------|---------------|-------|
| Task stacks | 3 KB | 5 tasks + ISR (see Section 7.1) |
| RTE signal buffers | 2 KB | All SWC port signals, double-buffered for consistency |
| Com I-PDU buffers | 2 KB | TX and RX PDU buffers (15 messages x ~64 bytes each) |
| CAN driver buffers | 1 KB | TX queue (8 messages), RX FIFO shadow |
| Dem DTC storage | 2 KB | 32 DTCs x 64 bytes (DTC number + status + freeze frame) |
| Dcm session state | 512 B | Session management, request/response buffers |
| WdgM supervision data | 256 B | Per-entity alive counters and deadline timers |
| BswM mode state | 128 B | Current mode, rule evaluation state |
| SPI DMA buffers | 256 B | TX/RX buffers for AS5048A communication |
| ADC DMA buffer | 128 B | Scan results for analog channels |
| OLED framebuffer | 1 KB | 128x64 pixels, 1 bpp |
| Global variables + .bss | 4 KB | Module-level static variables, configuration copies |
| Heap | 0 B | **No dynamic allocation (ASIL D)** |
| **Total** | **~17 KB** | **13.3% of 128 KB RAM** (+3 KB margin to 20 KB estimate) |

### 8.4 Hardware Capacity Margin

| ECU | Flash Used | Flash Available | Margin | RAM Used | RAM Available | Margin |
|-----|-----------|----------------|--------|---------|--------------|--------|
| CVC | ~60 KB | 512 KB | 88% free | ~20 KB | 128 KB | 84% free |
| FZC | ~55 KB | 512 KB | 89% free | ~18 KB | 128 KB | 86% free |
| RZC | ~50 KB | 512 KB | 90% free | ~16 KB | 128 KB | 87% free |
| SC | ~8 KB | 4 MB | 99% free | ~2 KB | 512 KB | 99% free |

All ECUs have substantial margin. ISO 26262 Part 6 recommends maintaining > 50% margin for safety-critical systems to accommodate bug fixes and safety patches.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC8 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Resource estimates are well-structured with per-ECU Flash, RAM, and CPU load. The CVC Flash breakdown (Section 8.2) and RAM breakdown (Section 8.3) are detailed to component level, which is good practice for SWE.2. All ECUs show > 84% free margins, which exceeds the ISO 26262 > 50% recommendation. The SC at ~8 KB Flash and ~2 KB RAM on a 4 MB/512 KB chip is deliberately oversized for the safety monitor role, which provides ample room for future safety mechanisms. The explicit "Heap: 0 B (No dynamic allocation - ASIL D)" note correctly enforces the ASIL D prohibition. These are estimates at the architecture phase; they should be verified against actual compiled binaries during SWE.3 implementation.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC8 -->

## 9. Memory Layout

### 9.1 STM32G474RE Memory Map (CVC/FZC/RZC)

```
Flash (512 KB): 0x0800_0000 - 0x0807_FFFF
+---------------------------+ 0x0800_0000
|  Vector Table (512 B)     |
+---------------------------+ 0x0800_0200
|  .text (code)             |
|  ~40-50 KB                |
+---------------------------+ 0x0800_C800 (approx)
|  .rodata (const data)     |
|  Config tables, LUTs      |
|  ~4 KB                    |
+---------------------------+ 0x0800_D800 (approx)
|  .data init values        |
|  (copied to RAM at boot)  |
|  ~2 KB                    |
+---------------------------+ 0x0800_E000 (approx)
|  UNUSED (~440 KB)         |
|                           |
+---------------------------+ 0x0807_FFFF

RAM (128 KB): 0x2000_0000 - 0x2001_FFFF
+---------------------------+ 0x2000_0000
|  MPU Region 0: BSW Data   |
|  .data (initialized)      |
|  .bss  (zero-initialized) |
|  BSW module static vars   |
|  ~8 KB                    |
+---------------------------+ 0x2000_2000
|  MPU Region 1: RTE Buffers|
|  Signal buffers            |
|  Com I-PDU buffers         |
|  ~4 KB                    |
+---------------------------+ 0x2000_3000
|  MPU Region 2: SWC Data   |
|  Application variables    |
|  ~4 KB                    |
+---------------------------+ 0x2000_4000
|  MPU Region 3: DMA Buffers|
|  SPI, ADC, UART DMA       |
|  ~1 KB                    |
+---------------------------+ 0x2000_4400
|  UNUSED (~108 KB)         |
|                           |
+---------------------------+ 0x2001_F000
|  MPU Region 4: ISR Stack  |
|  E-stop ISR, SysTick ISR  |
|  512 B                    |
+---------------------------+ 0x2001_F200
|  MPU Region 5: Task Stacks|
|  Task stacks (3 KB total) |
|  Grows downward            |
+---------------------------+ 0x2001_FFFF
```

### 9.2 MPU Configuration (ASIL D Spatial Isolation)

The Cortex-M4 MPU provides spatial isolation between software partitions. This is required by ISO 26262 Part 6 for ASIL D to achieve freedom from interference between SWC and BSW, and between safety-critical and QM software.

| MPU Region | Start | Size | Access | Purpose |
|------------|-------|------|--------|---------|
| 0 | 0x2000_0000 | 8 KB | RW, privileged only | BSW module data (Com, Dem, WdgM internal state) |
| 1 | 0x2000_2000 | 4 KB | RW, all tasks | RTE signal buffers (shared between SWC and BSW) |
| 2 | 0x2000_3000 | 4 KB | RW, unprivileged | SWC application data |
| 3 | 0x2000_4000 | 1 KB | RW, DMA only | DMA transfer buffers (SPI, ADC, UART) |
| 4 | 0x2001_F000 | 512 B | RW, privileged only | ISR stack (isolated from task stacks) |
| 5 | 0x2001_F200 | 3.5 KB | RW, per-task | Task stacks with stack overflow detection |
| 6 | 0x0800_0000 | 512 KB | RO, execute | Flash: code + const (no write, no execute from RAM) |
| 7 | 0x4000_0000 | 512 MB | RW, privileged only | Peripheral registers (BSW/MCAL access only) |

### 9.3 TMS570LC43x Memory Map (SC)

```
Flash (4 MB): 0x0000_0000 - 0x003F_FFFF
+---------------------------+ 0x0000_0000
|  Vector table             |
|  ESM, abort handlers      |
+---------------------------+ 0x0000_0100
|  .text (code) ~8 KB      |
+---------------------------+ 0x0000_2100 (approx)
|  .rodata (const) ~512 B  |
+---------------------------+ 0x0000_2300 (approx)
|  UNUSED (~4 MB)           |
+---------------------------+ 0x003F_FFFF

RAM (512 KB): 0x0800_0000 - 0x0807_FFFF
+---------------------------+ 0x0800_0000
|  .data + .bss (~1 KB)    |
|  heartbeat counters       |
|  fault flags              |
|  plausibility state       |
+---------------------------+ 0x0800_0400
|  UNUSED (~511 KB)         |
+---------------------------+ 0x0807_FC00
|  Stack (1 KB)             |
|  (grows downward)         |
+---------------------------+ 0x0807_FFFF
```

The TMS570 hardware lockstep cores provide error detection without software MPU configuration — the CPU itself detects execution divergence and triggers ESM (Error Signaling Module).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC9 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The memory layout section is excellent for an SWE.2 deliverable. The STM32G474RE memory map shows explicit MPU regions with access permissions (privileged-only for BSW, unprivileged for SWC, DMA-only for transfer buffers). This correctly implements ASIL D spatial isolation per ISO 26262 Part 6 Section 7. The separation of ISR stack from task stacks prevents stack corruption cascading between interrupt and task context. The TMS570 memory map is minimal (~8 KB code, ~1 KB data), consistent with the ~400 LOC bare-metal design. The note about TMS570 lockstep providing error detection without software MPU is correct -- the hardware lockstep is a stronger mechanism than software partitioning for the SC role. One gap: the MPU configuration table does not specify the background region behavior (what happens when access falls outside all defined regions) -- this should be defined as "default deny" for ASIL D.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC9 -->

## 10. Error Handling Strategy

### 10.1 Return Type Convention

All BSW and SWC functions use `Std_ReturnType` (AUTOSAR convention):

```c
typedef uint8 Std_ReturnType;

#define E_OK            ((Std_ReturnType)0x00u)
#define E_NOT_OK        ((Std_ReturnType)0x01u)
#define E_BUSY          ((Std_ReturnType)0x02u)
#define E_PENDING       ((Std_ReturnType)0x03u)

/* Module-specific errors (0x10..0xFF) */
#define COM_E_TIMEOUT   ((Std_ReturnType)0x10u)
#define DCM_E_SESSION   ((Std_ReturnType)0x11u)
#define CAN_E_BUSOFF    ((Std_ReturnType)0x12u)
#define E2E_E_WRONG_CRC ((Std_ReturnType)0x20u)
#define E2E_E_WRONG_SEQ ((Std_ReturnType)0x21u)
```

### 10.2 DTC Mapping

Every detectable fault maps to a DTC stored by Dem. DTCs follow SAE J2012 / ISO 15031-6 format (3-byte DTC number):

| DTC | Fault | Source SWC | Dem Event ID | ASIL | Safe State Action |
|-----|-------|-----------|--------------|------|-------------------|
| 0xC00100 | Pedal sensor plausibility failure | Swc_Pedal | DEM_EVENT_PEDAL_PLAUSIBILITY | D | TorqueRequest = 0, DEGRADED state |
| 0xC00200 | Pedal sensor 1 communication loss | Swc_Pedal | DEM_EVENT_PEDAL_S1_COMM | D | Use sensor 2 only, DEGRADED |
| 0xC00300 | Pedal sensor 2 communication loss | Swc_Pedal | DEM_EVENT_PEDAL_S2_COMM | D | Use sensor 1 only, DEGRADED |
| 0xC00400 | Both pedal sensors failed | Swc_Pedal | DEM_EVENT_PEDAL_BOTH_FAIL | D | TorqueRequest = 0, SAFE_STOP |
| 0xC10100 | CAN Rx timeout (FZC lost CVC) | Com | DEM_EVENT_CAN_TIMEOUT_CVC | D | Auto-brake applied |
| 0xC10200 | CAN Rx timeout (RZC lost CVC) | Com | DEM_EVENT_CAN_TIMEOUT_CVC_RZC | D | Motor stop |
| 0xC10300 | CAN bus-off detected | Can | DEM_EVENT_CAN_BUSOFF | D | Attempt recovery, DTC stored |
| 0xC10400 | E2E CRC error on safety message | E2E | DEM_EVENT_E2E_CRC | D | Discard message, use last valid |
| 0xC10500 | E2E alive counter error | E2E | DEM_EVENT_E2E_ALIVE | D | Discard message, increment error counter |
| 0xC20100 | Motor overcurrent (> 10 A for 100 ms) | Swc_CurrentMonitor | DEM_EVENT_MOTOR_OVERCURRENT | D | Motor PWM = 0, SAFE_STOP |
| 0xC20200 | Motor over-temperature (> 80 C) | Swc_TempMonitor | DEM_EVENT_MOTOR_OVERTEMP | D | Motor PWM = 0, SAFE_STOP |
| 0xC20300 | Motor over-temperature warning (> 60 C) | Swc_TempMonitor | DEM_EVENT_MOTOR_TEMP_WARN | B | Derate to 50% |
| 0xC30100 | Steering angle sensor failure | Swc_Steering | DEM_EVENT_STEER_SENSOR | D | Return to center |
| 0xC30200 | Steering servo jammed | Swc_Steering | DEM_EVENT_STEER_JAMMED | D | Report fault, manual override |
| 0xC30300 | Lidar sensor failure | Swc_Lidar | DEM_EVENT_LIDAR_FAIL | B | Reduce speed limit, warning |
| 0xC40100 | E-stop activated | Swc_EStop | DEM_EVENT_ESTOP | D | System-wide SAFE_STOP |
| 0xC50100 | WdgM supervision expired | WdgM | DEM_EVENT_WDGM_EXPIRED | D | External watchdog reset |
| 0xC50200 | BswM mode transition failure | BswM | DEM_EVENT_BSWM_MODE_FAIL | D | Force SAFE_STOP mode |

### 10.3 Defensive Programming Practices

Per ISO 26262 Part 6, Table 1 (highly recommended for ASIL D):

1. **Range checks on all inputs**: Every Rte_Read result is checked against valid range before use
2. **Default cases in all switch statements**: Default case always transitions to safe state or returns E_NOT_OK
3. **Assertions in debug builds**: `ASSERT(condition)` macro active in debug, compiled out in release
4. **Null pointer checks**: Every pointer parameter validated before dereference
5. **Array bounds checks**: All array indices validated against array size
6. **Divide-by-zero guards**: All divisors checked before division
7. **Overflow protection**: All arithmetic checked for uint8/uint16/uint32 overflow before assignment
8. **Redundant storage for safety-critical variables**: Critical state variables stored twice and compared (e.g., vehicle state, kill relay command)
9. **Inverse redundancy**: Safety-critical boolean flags stored as value and inverse (e.g., `estop_active` and `estop_active_inv = ~estop_active`)
10. **Control flow monitoring**: WdgM alive supervision verifies that each safety-critical runnable executes within its expected period

### 10.4 Error Propagation Path

```
Fault detected (SWC or BSW)
    |
    v
Dem_ReportErrorStatus(event_id, DEM_EVENT_STATUS_FAILED)
    |
    v
Dem: debounce counter increments
    |
    v (counter >= threshold)
    |
Dem: DTC stored in RAM array, status byte updated
    |
    +--> Dem_MainFunction: DTC persisted to NVM (stub — RAM persistence only)
    |
    +--> BswM notified: mode rule evaluation triggered
    |        |
    |        v
    |    BswM: if DTC is safety-critical, transition to DEGRADED or SAFE_STOP mode
    |        |
    |        v
    |    Com: mode-dependent behavior (e.g., disable TX in SAFE_STOP)
    |
    +--> Com: DTC broadcast CAN message sent to all ECUs + SC
    |
    +--> TCU (simulated): receives DTC via CAN, stores in DTC database
    |
    +--> ICU (simulated): receives DTC via CAN, displays warning on dashboard
    |
    +--> Gateway (Pi): receives DTC via CAN, publishes to cloud, triggers SAP QM
```

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC10 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The error handling strategy is comprehensive. The Std_ReturnType convention follows AUTOSAR practice, and the DTC mapping table (Section 10.2) covers 18 fault types across all ECUs with correct ASIL assignments and explicit safe-state actions for each. The defensive programming practices list (Section 10.3) directly maps to ISO 26262 Part 6 Table 1 requirements for ASIL D, including range checks, null pointer checks, redundant storage, inverse redundancy, and control flow monitoring via WdgM. The error propagation path (Section 10.4) correctly shows the fault flow from detection through Dem debouncing, DTC storage, BswM mode transition, and finally to cloud/SAP QM reporting. One concern: the DTC numbers appear to be custom-assigned (0xCxxxxx format) -- this is acceptable for a portfolio project, but for production the DTC assignment should follow SAE J2012 UDS fault code structure with proper Powertrain/Chassis/Body/Network prefixes.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC10 -->

## 11. SSR to Module Allocation

### 11.1 CVC SSR Allocation (23 SSRs)

| SSR ID | Title | Allocated Module | ASIL |
|--------|-------|-----------------|------|
| SSR-CVC-001 | Dual pedal sensor reading via SPI | Swc_Pedal, IoHwAb, Spi | D |
| SSR-CVC-002 | Pedal plausibility check (|S1-S2| < 5%) | Swc_Pedal | D |
| SSR-CVC-003 | Pedal position output to CAN (10 ms cycle) | Swc_Pedal, Com, E2E | D |
| SSR-CVC-004 | Pedal sensor fault reporting to Dem | Swc_Pedal, Dem | D |
| SSR-CVC-005 | Vehicle state machine (INIT/RUN/DEGRADED/LIMP/SAFE_STOP) | Swc_StateMachine | D |
| SSR-CVC-006 | Torque request computation from pedal position | Swc_StateMachine | D |
| SSR-CVC-007 | Torque cutoff on pedal fault | Swc_StateMachine | D |
| SSR-CVC-008 | Torque cutoff on E-stop | Swc_StateMachine, Swc_EStop | D |
| SSR-CVC-009 | Steering command forwarding to FZC | Swc_StateMachine, Com | D |
| SSR-CVC-010 | Brake command forwarding to FZC | Swc_StateMachine, Com | D |
| SSR-CVC-011 | E-stop GPIO detection (EXTI, < 1 ms latency) | Swc_EStop, Dio | D |
| SSR-CVC-012 | E-stop CAN broadcast (0x001) | Swc_EStop, Com | D |
| SSR-CVC-013 | CVC heartbeat transmission (50 ms, alive counter) | Swc_CanMaster, Com, E2E | D |
| SSR-CVC-014 | FZC heartbeat monitoring and timeout detection | Swc_CanMaster, Com | D |
| SSR-CVC-015 | RZC heartbeat monitoring and timeout detection | Swc_CanMaster, Com | D |
| SSR-CVC-016 | Vehicle state CAN broadcast (100 ms) | Swc_StateMachine, Com | D |
| SSR-CVC-017 | OLED display of vehicle state | Swc_Display, IoHwAb | QM |
| SSR-CVC-018 | OLED display of speed and fault indicators | Swc_Display, IoHwAb | QM |
| SSR-CVC-019 | E2E protection on all safety-critical TX messages | E2E, Com | D |
| SSR-CVC-020 | E2E validation on all safety-critical RX messages | E2E, Com | D |
| SSR-CVC-021 | WdgM alive supervision for all safety runnables | WdgM | D |
| SSR-CVC-022 | External watchdog (TPS3823) feeding | WdgM, Dio | D |
| SSR-CVC-023 | BSW mode management (startup, run, degraded, safe stop) | BswM | D |

### 11.2 FZC SSR Allocation (24 SSRs)

| SSR ID | Title | Allocated Module | ASIL |
|--------|-------|-----------------|------|
| SSR-FZC-001 | Steering command reception via CAN | Com, E2E | D |
| SSR-FZC-002 | Steering servo PWM output | Swc_Steering, IoHwAb, Pwm | D |
| SSR-FZC-003 | Steering angle sensor reading via SPI | Swc_Steering, IoHwAb, Spi | D |
| SSR-FZC-004 | Steering rate limiting (max 45 deg/s) | Swc_Steering | D |
| SSR-FZC-005 | Steering angle saturation (+/- 45 deg) | Swc_Steering | D |
| SSR-FZC-006 | Steering return-to-center on sensor fault | Swc_Steering | D |
| SSR-FZC-007 | Steering angle feedback CAN transmission | Swc_Steering, Com, E2E | D |
| SSR-FZC-008 | Steering sensor fault reporting to Dem | Swc_Steering, Dem | D |
| SSR-FZC-009 | Brake command reception via CAN | Com, E2E | D |
| SSR-FZC-010 | Brake servo PWM output | Swc_Brake, IoHwAb, Pwm | D |
| SSR-FZC-011 | Brake force mapping (command % to PWM duty) | Swc_Brake | D |
| SSR-FZC-012 | Auto-brake on CAN Rx timeout (100 ms) | Swc_Brake, Com | D |
| SSR-FZC-013 | Emergency brake on lidar close detection | Swc_Brake, Swc_Lidar | D |
| SSR-FZC-014 | Brake status CAN transmission | Swc_Brake, Com | D |
| SSR-FZC-015 | Lidar distance reading via UART (TFMini-S) | Swc_Lidar | D |
| SSR-FZC-016 | Lidar frame parsing and checksum validation | Swc_Lidar | D |
| SSR-FZC-017 | Lidar distance filtering (median of 3) | Swc_Lidar | D |
| SSR-FZC-018 | Lidar distance threshold detection (warning/brake/emergency) | Swc_Lidar | D |
| SSR-FZC-019 | Lidar distance CAN transmission | Swc_Lidar, Com | D |
| SSR-FZC-020 | Buzzer warning patterns (continuous/intermittent/off) | Swc_Buzzer, Dio | QM |
| SSR-FZC-021 | FZC heartbeat transmission (50 ms, alive counter) | Com, E2E | D |
| SSR-FZC-022 | E2E protection on all safety-critical messages | E2E, Com | D |
| SSR-FZC-023 | WdgM alive supervision for all safety runnables | WdgM | D |
| SSR-FZC-024 | External watchdog (TPS3823) feeding | WdgM, Dio | D |

### 11.3 RZC SSR Allocation (17 SSRs)

| SSR ID | Title | Allocated Module | ASIL |
|--------|-------|-----------------|------|
| SSR-RZC-001 | Torque request reception via CAN | Com, E2E | D |
| SSR-RZC-002 | Motor PWM output via BTS7960 H-bridge | Swc_Motor, IoHwAb, Pwm | D |
| SSR-RZC-003 | Motor ramp limiting (max 10%/10ms) | Swc_Motor | D |
| SSR-RZC-004 | Motor direction control via GPIO | Swc_Motor, IoHwAb, Dio | D |
| SSR-RZC-005 | Motor disable on overcurrent fault | Swc_Motor, Swc_CurrentMonitor | D |
| SSR-RZC-006 | Motor disable on over-temperature | Swc_Motor, Swc_TempMonitor | D |
| SSR-RZC-007 | Motor current reading via ACS723 ADC | Swc_CurrentMonitor, IoHwAb, Adc | D |
| SSR-RZC-008 | Motor current low-pass filtering | Swc_CurrentMonitor | D |
| SSR-RZC-009 | Overcurrent detection (>10A, 100ms debounce) | Swc_CurrentMonitor | D |
| SSR-RZC-010 | Overcurrent DTC reporting to Dem | Swc_CurrentMonitor, Dem | D |
| SSR-RZC-011 | Temperature reading via NTC ADC | Swc_TempMonitor, IoHwAb, Adc | D |
| SSR-RZC-012 | Temperature derating curve (60C derate, 80C shutdown) | Swc_TempMonitor | D |
| SSR-RZC-013 | Over-temperature DTC reporting to Dem | Swc_TempMonitor, Dem | D |
| SSR-RZC-014 | Encoder speed computation | Swc_Encoder, IoHwAb | QM |
| SSR-RZC-015 | Motor status CAN transmission (speed, current, temp) | Com, E2E | D |
| SSR-RZC-016 | RZC heartbeat transmission (50 ms, alive counter) | Com, E2E | D |
| SSR-RZC-017 | External watchdog (TPS3823) feeding | WdgM, Dio | D |

### 11.4 SC SSR Allocation (17 SSRs)

| SSR ID | Title | Allocated Module | ASIL |
|--------|-------|-----------------|------|
| SSR-SC-001 | CAN listen-only mode (DCAN1 silent) | can_monitor.c | D |
| SSR-SC-002 | CVC heartbeat monitoring | heartbeat.c | D |
| SSR-SC-003 | FZC heartbeat monitoring | heartbeat.c | D |
| SSR-SC-004 | RZC heartbeat monitoring | heartbeat.c | D |
| SSR-SC-005 | Heartbeat timeout detection (100 ms window) | heartbeat.c | D |
| SSR-SC-006 | Alive counter validation (increment check) | heartbeat.c | D |
| SSR-SC-007 | Torque-vs-current cross-plausibility check | plausibility.c | D |
| SSR-SC-008 | E-stop CAN message verification | plausibility.c | D |
| SSR-SC-009 | Kill relay control (energize-to-run) | relay.c | D |
| SSR-SC-010 | Kill relay de-energize on heartbeat timeout | relay.c | D |
| SSR-SC-011 | Kill relay de-energize on plausibility fault | relay.c | D |
| SSR-SC-012 | Kill relay de-energize on E-stop | relay.c | D |
| SSR-SC-013 | Fault LED per-ECU health display | led_panel.c | QM |
| SSR-SC-014 | External watchdog (TPS3823) safety-gated feeding | watchdog.c | D |
| SSR-SC-015 | SC state machine (INIT/MONITORING/FAULT/KILL) | main.c | D |
| SSR-SC-016 | Lockstep CPU error detection via ESM | main.c (HALCoGen ESM handler) | D |
| SSR-SC-017 | Confirmation cycle before kill relay actuation | relay.c | D |

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC11 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The SSR-to-module allocation is complete at 81/81 (100%) coverage across all four ECUs. Each SSR is mapped to specific SWC and BSW modules, enabling bidirectional traceability between safety requirements and implementation. The ASIL assignments are consistent: CVC has 18 ASIL D SSRs out of 23 (QM only for display), FZC has 21 ASIL D out of 24 (QM for buzzer), RZC has 14 ASIL D out of 17 (QM for encoder), and SC has 15 ASIL D out of 17 (QM for LED panel). This allocation is consistent with the module responsibility tables in Section 4. The SC allocation correctly assigns all safety-critical functions (CAN monitoring, heartbeat, plausibility, relay, watchdog) to ASIL D.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC11 -->

## 12. Traceability and Revision History

### 12.1 Traceability Summary

| From | To | Coverage |
|------|----|----------|
| SSR-CVC-001..023 | SWC and BSW modules (Section 11.1) | 23/23 (100%) |
| SSR-FZC-001..024 | SWC and BSW modules (Section 11.2) | 24/24 (100%) |
| SSR-RZC-001..017 | SWC and BSW modules (Section 11.3) | 17/17 (100%) |
| SSR-SC-001..017 | SC source modules (Section 11.4) | 17/17 (100%) |
| **Total** | **All SSRs allocated** | **81/81 (100%)** |

Reverse traceability (module to SSR) is maintained in the traceability matrix: [docs/aspice/traceability/traceability-matrix.md](../../traceability/traceability-matrix.md).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWARCH-SEC12 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The traceability summary confirms 100% SSR allocation coverage (81 SSRs across 4 ECUs), which satisfies ASPICE SWE.2 BP5 (establish bidirectional traceability). The reference to the external traceability matrix for reverse traceability (module-to-SSR) is correct practice. For full ASPICE compliance, this section should also confirm traceability upward to SWR (software requirements) documents and downward to SWE.3 detailed design, not just the SSR-to-module link. The revision history shows a jump from 0.1 stub to 1.0 complete, which is fine for initial creation but should be updated as review comments are addressed.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWARCH-SEC12 -->

### 12.2 Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | — | Initial stub |
| 1.0 | 2026-02-21 | Taktflow Team | Complete SWE.2 software architecture: layered model, per-ECU decomposition, SWC interfaces with ports and runnables, sequence diagrams (5 scenarios), task scheduling (4 ECUs), resource estimates, memory layout with MPU, error handling and DTC mapping, SSR-to-module allocation (81 SSRs) |

