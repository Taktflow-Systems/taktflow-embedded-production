---
document_id: DFA
title: "Dependent Failure Analysis"
version: "1.0"
status: draft
iso_26262_part: 9
iso_26262_clause: "7"
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


# Dependent Failure Analysis

<!-- DECISION: ADR-001 — Zonal architecture over domain-based -->

## 1. Purpose

This document presents the Dependent Failure Analysis (DFA) for the Taktflow Zonal Vehicle Platform per ISO 26262-9:2018, Clause 7. The DFA identifies and evaluates dependent failures that could violate safety goals despite the presence of redundancy and safety mechanisms. Dependent failures fall into two categories:

1. **Common Cause Failures (CCF)**: Two or more elements fail simultaneously due to a single shared root cause (ISO 26262-9, 7.3).
2. **Cascading Failures (CF)**: Failure of one element causes subsequent failure of one or more other elements through a propagation path (ISO 26262-9, 7.4).

The DFA is a mandatory work product for safety-relevant systems, particularly where safety goals depend on the assumed independence of redundant elements. The Taktflow platform relies on the independence between zone controllers (CVC, FZC, RZC) and the Safety Controller (SC) to achieve ASIL D integrity. This DFA provides the evidence that this assumed independence is sufficiently justified.

## 2. Scope

### 2.1 System Under Analysis

The analysis covers all physical ECUs and shared infrastructure within the item boundary as defined in the Item Definition (ITEM-DEF v1.0):

| Element | Role | ASIL Allocation |
|---------|------|-----------------|
| CVC (STM32G474RE) | Vehicle brain, pedal input, CAN master, state machine | ASIL D (SG-001, SG-003, SG-004) |
| FZC (STM32G474RE) | Steering servo, brake servo, lidar, buzzer | ASIL D (SG-003, SG-004), ASIL C (SG-007) |
| RZC (STM32G474RE) | Motor driver, current/temperature sensing, encoder | ASIL D (SG-001), ASIL B (SG-002), ASIL A (SG-006) |
| SC (TMS570LC43x) | Independent CAN monitor, heartbeat, kill relay | ASIL C (SG-008) |
| CAN bus | Inter-ECU communication (500 kbps, 7+1 nodes) | Shared by all ASIL levels |
| Power supply | 12V → 5V → 3.3V distribution | Shared by all ECUs |

### 2.2 Exclusions

| Element | Reason |
|---------|--------|
| BCM, ICU, TCU (Docker) | QM-rated, no safety function allocation, no independence assumption |
| Raspberry Pi gateway | Outside item safety boundary |
| Cloud infrastructure | No vehicle control authority |

## 3. References

| Document ID | Title | Version | Relevance |
|-------------|-------|---------|-----------|
| ITEM-DEF | Item Definition | 1.0 | System boundary, interfaces, assumptions |
| HARA | Hazard Analysis and Risk Assessment | 1.0 | Hazardous events, ASIL assignments |
| SG | Safety Goals | 1.0 | Safety goals, safe states, FTTI |
| FSC | Functional Safety Concept | 1.0 | Safety mechanisms, independence assumptions |
| FMEA | Failure Mode and Effects Analysis | 1.0 | Component failure modes, severity, detection |
| ISO 26262-9:2018 | Road vehicles -- Functional safety -- Part 9 | 2018 | DFA methodology (Clause 7) |
| ISO 26262-4:2018 | Road vehicles -- Functional safety -- Part 4 | 2018 | System architecture, FFI requirements |
| IEC 61508-7 | Functional safety -- Part 7 | -- | Beta-factor model for CCF quantification |

## 4. Methodology

### 4.1 DFA Process

The DFA follows the process prescribed in ISO 26262-9, Clause 7:

1. **Identify elements with assumed independence**: From the FSC, identify all pairs of elements where the safety argument relies on their independent operation (e.g., dual sensors, zone ECU + SC, primary mechanism + backup).
2. **Identify coupling factors**: For each pair of elements, systematically identify shared resources, common design features, shared manufacturing processes, shared environmental exposure, and other potential coupling factors.
3. **Assess CCF potential**: Determine whether each coupling factor could plausibly cause simultaneous failure of both elements in a manner that violates a safety goal.
4. **Assess CF potential**: Determine whether failure of one element could propagate to cause failure of another element through data flow, control flow, power flow, or physical coupling.
5. **Evaluate mitigations**: For each identified CCF/CF, evaluate the effectiveness of implemented mitigations and determine residual risk.
6. **Document residual risk**: Any CCF/CF that is not fully mitigated is documented with its residual risk assessment and acceptance rationale.

### 4.2 Coupling Factor Categories

Per ISO 26262-9, Annex D, the following coupling factor categories are systematically evaluated:

| Category | Description | Examples |
|----------|-------------|---------|
| Physical proximity | Elements co-located, sharing physical space | CAN bus wires, power rail traces, sensor cables |
| Environmental | Elements sharing thermal, vibration, or EMC environment | All ECUs on same bench, shared ambient temperature |
| Design commonality | Elements sharing design (IC vendor, architecture, toolchain) | STM32G474RE shared across CVC/FZC/RZC, shared BSW stack |
| Manufacturing commonality | Elements from same production batch or process | Same PCB fab, same assembly process |
| Maintenance commonality | Elements serviced together or with shared procedures | Single operator maintaining all ECUs |
| Power supply | Elements sharing power source or regulation | 12V bench supply, shared 5V/3.3V rails |
| Communication | Elements sharing communication medium | Single CAN bus for all ECUs |
| Software | Elements sharing software components or libraries | AUTOSAR-like BSW stack shared across CVC/FZC/RZC |

### 4.3 Participants

| Role | Responsibility |
|------|----------------|
| FSE Lead | DFA facilitation, independence assessment, residual risk determination |
| System Architect | Architecture coupling analysis, shared resource identification |
| HW Engineer | Physical coupling, power supply, EMC coupling analysis |
| SW Engineer | Software commonality, BSW stack, compiler coupling analysis |

## 5. Independence Assumptions

The following independence assumptions are made in the Functional Safety Concept (FSC v1.0) and must be validated by this DFA.

| ID | Assumption | FSC Reference | Safety Goals Dependent |
|----|-----------|---------------|----------------------|
| IA-001 | CVC and SC operate independently for cross-plausibility checking (SM-003) | SM-003 | SG-001 (ASIL D) |
| IA-002 | FZC and SC operate independently for steering/brake fault detection | SM-019 | SG-003, SG-004 (ASIL D) |
| IA-003 | RZC and SC operate independently for motor fault detection | SM-019, SM-003 | SG-001 (ASIL D), SG-002 (ASIL B) |
| IA-004 | Dual pedal sensors (AS5048A x2) provide independent readings for plausibility checking (SM-001) | SM-001 | SG-001 (ASIL D) |
| IA-005 | Zone ECU safety mechanisms and SC kill relay are independent fault containment layers | SM-001 through SM-018 vs SM-005 | All safety goals |
| IA-006 | External watchdog (TPS3823) operates independently of the MCU it monitors | SM-020 | SG-008 (ASIL C) |

## 6. Common Cause Failure Analysis

### 6.1 CCF-001: Shared CAN Bus

#### 6.1.1 Description

All seven ECU nodes (CVC, FZC, RZC, SC, BCM, ICU, TCU) and the Raspberry Pi gateway share a single CAN bus at 500 kbps. The CAN bus is a linear topology with 120-ohm termination resistors at each end. The bus uses CAN_H and CAN_L differential wires with TJA1051T transceivers on STM32 ECUs and SN65HVD230 on the TMS570.

#### 6.1.2 Coupling Factors

| Factor | Description |
|--------|-------------|
| Physical medium | Single pair of wires (CAN_H, CAN_L) carries all inter-ECU communication |
| Transceivers | TJA1051T shared across CVC, FZC, RZC (same vendor, same part number) |
| Termination | Two termination resistors; loss of both kills the bus |
| Connectors | Shared connector block or breadboard bus bar |
| Environment | CAN bus wires routed on the same bench, shared EMC environment |

#### 6.1.3 Affected Safety Goals

| Safety Goal | ASIL | Impact of CAN Bus Loss |
|-------------|------|----------------------|
| SG-001 | D | CVC cannot send torque commands to RZC. RZC defaults to zero torque (safe). |
| SG-002 | B | RZC cannot receive torque commands. Motor stops. |
| SG-003 | D | CVC cannot send steering commands to FZC. FZC auto-centers steering (SM-008 timeout). |
| SG-004 | D | CVC cannot send brake commands to FZC. FZC auto-brakes on CAN timeout (SM-012). |
| SG-005 | A | No CAN commands = no unintended braking from CAN. Safe condition. |
| SG-006 | A | RZC local current/temp monitoring continues without CAN. |
| SG-007 | C | FZC lidar continues locally but cannot report to CVC. FZC initiates emergency brake locally. |
| SG-008 | C | SC loses all heartbeats. SC opens kill relay (de-energize-to-run). System shutdown. |

#### 6.1.4 Mitigations

| Mitigation | Mechanism | Effectiveness |
|------------|-----------|---------------|
| SC listen-only mode | SC CAN is in listen-only (silent) mode via DCAN TEST register bit 3. SC cannot transmit on the bus, therefore SC cannot corrupt the bus or contribute to bus overload. SC monitoring is a passive observer. | High -- SC independence from bus failure is preserved for monitoring function. SC can detect bus silence and activate kill relay. |
| E2E protection | CRC-8 + alive counter + data ID on all safety-critical messages (SM-004). Detects corruption, loss, delay, and repetition. | High -- for message-level faults. Does not help with total bus loss (detected via timeout instead). |
| Timeout-based safe defaults | Every receiving ECU implements CAN message timeout with safe default: RZC (torque = 0), FZC (auto-brake + steer-center), CVC (transition to SAFE_STOP). | High -- each ECU independently achieves a safe state on CAN timeout. |
| Kill relay backup | SC detects CAN silence (all heartbeats missing) within 200 ms and opens kill relay, removing all actuator power. | High -- ultimate hardware-enforced safe state. |
| CAN ID priority assignment | Safety-critical messages (E-stop: 0x001, heartbeats: 0x010-0x013) use highest CAN priority (lowest ID numbers). Less likely to be starved by bus overload. | Moderate -- helps with bus overload but not total bus loss. |

#### 6.1.5 Residual Risk Assessment

**Total CAN bus loss** (wire break, short-to-ground, dual termination failure):
- All ECUs detect CAN timeout within their configured timeout periods (100-200 ms).
- All ECUs transition to safe defaults (torque=0, auto-brake, steer-center).
- SC opens kill relay within 200 ms + debounce.
- System enters SHUTDOWN within 300 ms of total CAN loss.

**Residual risk**: During the 100-200 ms detection window, each ECU operates with its last received commands. If the last torque command was non-zero, the motor continues running for up to 200 ms. At maximum platform speed (assumed 30 km/h = 8.3 m/s), 200 ms = 1.7 m of travel. FZC auto-brake reduces this distance after 100 ms.

**Acceptance**: Residual risk is acceptable. The timeout-based safe defaults are well within the FTTI for each safety goal (worst case: SG-001 FTTI = 50 ms; however, CAN loss is a communication fault, not a pedal sensor fault -- SM-001 and SM-002 remain operational on each ECU locally). The SC kill relay provides a hard backup within 300 ms.

---

### 6.2 CCF-002: Same MCU Vendor (STMicroelectronics) for CVC, FZC, RZC

#### 6.2.1 Description

The CVC, FZC, and RZC all use the STM32G474RE microcontroller (STMicroelectronics, Cortex-M4F, 170 MHz). A common silicon defect, errata, or design vulnerability in the STM32G474RE could cause all three zone controllers to fail simultaneously.

#### 6.2.2 Coupling Factors

| Factor | Description |
|--------|-------------|
| Silicon vendor | All three zone ECUs use the same IC vendor (STMicroelectronics) |
| IC design | Same die design, same Cortex-M4F core, same peripheral IP blocks (FDCAN, SPI, ADC, timers) |
| Manufacturing | Same wafer fab, same process node, same production line |
| Errata | Same errata apply to all three MCUs (STM32G4 errata sheet) |
| Supply chain | Same procurement path -- batch-specific defects could affect all units |

#### 6.2.3 Affected Safety Goals

If all three zone ECUs fail simultaneously, the system loses all sensor reading, actuator control, and CAN communication from the zone ECUs. This affects all safety goals (SG-001 through SG-007).

#### 6.2.4 Mitigations

| Mitigation | Mechanism | Effectiveness |
|------------|-----------|---------------|
| **Diverse Safety Controller** | SC uses TI TMS570LC43x (Texas Instruments, Cortex-R5 lockstep). Different vendor, different CPU architecture, different silicon process, different design team. A defect specific to STM32G474RE does not affect TMS570. | **High** -- this is the primary architectural mitigation for MCU vendor commonality. |
| SC kill relay | If all three zone ECUs fail (heartbeats stop), SC detects the failure within 200 ms and opens the kill relay. System enters SHUTDOWN. | High -- SC provides independent hardware-enforced safe state. |
| STM32 errata review | All applicable STM32G4 errata are reviewed during design. Workarounds are implemented for any errata affecting safety-critical peripherals (FDCAN, SPI, ADC, timers). | Moderate -- reduces but does not eliminate systematic silicon defect risk. |
| Lot diversity | When procuring STM32G474RE units, source from different production lots (different date codes) to reduce batch-specific defect risk. | Low -- difficult to enforce; primarily addresses manufacturing defects. |

#### 6.2.5 Residual Risk Assessment

**Common silicon defect**: The probability of an undiscovered silicon defect in the STM32G474RE that causes all three zone ECUs to fail simultaneously is low. The STM32G4 family is widely deployed (millions of units), reducing the likelihood of latent undiscovered defects.

**Residual risk**: The TMS570-based SC provides a diverse, independent monitoring layer. Even if all zone ECUs fail simultaneously, the SC detects heartbeat loss and opens the kill relay within 300 ms. The system achieves the SHUTDOWN safe state.

**Acceptance**: Residual risk is acceptable. The architectural decision to use a different MCU vendor (TI vs. ST) for the Safety Controller provides the independence required by ISO 26262-9, 7.3.2.2 for ASIL D. The SC's ability to force SHUTDOWN via the kill relay is independent of the STM32G474RE hardware.

---

### 6.3 CCF-003: Shared Power Supply (12V Rail)

#### 6.3.1 Description

All ECUs share a common 12V bench power supply (5A regulated supply). The 12V rail powers the motor (via BTS7960), servos (direct 12V or via buck converter), kill relay coil, and downstream buck converters for 5V and 3.3V rails. Loss of the 12V supply affects all system components.

#### 6.3.2 Coupling Factors

| Factor | Description |
|--------|-------------|
| Single supply | One bench power supply provides 12V for the entire system |
| Common fuse | A single fuse on the 12V input protects the system |
| Common wiring | 12V distribution from supply to all ECU power inputs |
| Downstream regulation | 5V and 3.3V rails derived from 12V; 12V loss cascades to all rails |

#### 6.3.3 Affected Safety Goals

Loss of 12V removes power from all components. All safety goals are affected.

#### 6.3.4 Mitigations

| Mitigation | Mechanism | Effectiveness |
|------------|-----------|---------------|
| **Energize-to-run relay pattern** | Kill relay is wired as normally-open, energized by SC to close. Loss of 12V de-energizes relay, which opens, removing power from motor and servos. This means 12V loss = safe state (all actuators de-powered). | **High** -- 12V loss is inherently safe by design. |
| SC power path independence | SC's 3.3V supply is derived from the TMS570 LaunchPad's onboard regulator, which is powered from the same 12V rail. However, the SC's kill relay control GPIO defaults to LOW on power loss (relay opens). | Moderate -- SC loses power simultaneously, but relay behavior is correct (opens on power loss). |
| ECU brown-out detection | STM32G474RE and TMS570LC43x have internal brown-out detection (BOD/POR). If voltage drops below threshold, MCU enters reset rather than operating in undefined state. | High -- prevents undefined MCU behavior during power dip. |
| No 12V battery buffer | The system does not have a battery buffer or supercapacitor to ride through 12V dropouts. This is a deliberate design choice: power loss = instant safe state, no need to ride through. | Acceptable -- power loss is safe by design. |

#### 6.3.5 Residual Risk Assessment

**12V supply loss**: All components lose power simultaneously. The kill relay opens (safe state). Motor stops (no power). Servos coast to neutral (no power). ECUs reset (BOD). This is a safe condition.

**12V voltage sag (brownout, not full loss)**: Partial voltage reduction could cause MCU brownouts (reset) while motor/servos remain partially powered. MCU reset triggers external watchdog timeout and SC heartbeat loss. SC opens kill relay. Motor/servo power removed.

**Residual risk**: Minimal. 12V loss is inherently safe due to the energize-to-run relay pattern. Partial brownout is covered by BOD + SC heartbeat monitoring. The only concern is a very narrow voltage range where MCUs are impaired but relay remains energized (marginal voltage). Mitigation: relay coil voltage is specified with dropout voltage well above MCU BOD threshold.

**Acceptance**: Residual risk is acceptable. The energize-to-run pattern converts power loss from a hazard into an inherent safe state.

---

### 6.4 CCF-004: Shared PCB Ground

#### 6.4.1 Description

On the prototype breadboard/protoboard construction, ground return paths for analog sensors, digital logic, and power actuators share a common ground plane or ground bus. Ground loops, common impedance coupling, and high-current return paths can inject noise into analog signal chains.

#### 6.4.2 Coupling Factors

| Factor | Description |
|--------|-------------|
| Common ground bus | Single ground wire/bar connecting all ECUs and sensors |
| Motor current return | High motor current (up to 25A) returns through shared ground, creating voltage drops |
| ADC reference ground | ACS723 and NTC sensors share ground reference with motor driver |
| CAN ground | CAN transceiver ground reference shared with signal ground |

#### 6.4.3 Affected Safety Goals

Ground noise primarily affects ADC accuracy on the RZC (current sensor ACS723, NTC thermistors, battery voltage divider). This impacts SG-006 (motor protection, ASIL A) and potentially SG-001 (if ground noise couples into SPI signals for pedal sensors on CVC).

#### 6.4.4 Mitigations

| Mitigation | Mechanism | Effectiveness |
|------------|-----------|---------------|
| Star ground topology | Each ECU connects to the power supply ground via its own dedicated wire (star topology), minimizing shared ground impedance. | Moderate -- reduces but does not eliminate ground noise. |
| Separate analog/digital ground | On protoboard, analog ground traces (ADC inputs, sensor returns) are routed separately from digital ground and power ground, joined at a single star point. | Moderate -- standard practice for mixed-signal design. |
| ADC software filtering | RZC ADC readings are filtered via 3-sample median filter followed by 8-sample running average. This rejects transient noise spikes. | High -- for transient noise. Does not help with DC ground offset. |
| Sensor plausibility checks | Dual NTC cross-plausibility (SM-015), current plausibility (SM-002/SM-003), range checking on all ADC channels. | High -- detects anomalous readings regardless of cause. |
| Decoupling capacitors | 100 nF ceramic capacitors on power pins of all ICs and 10 uF bulk capacitors on power input of each ECU board. | High -- for high-frequency noise. |

#### 6.4.5 Residual Risk Assessment

**Ground noise effects**: The primary risk is incorrect ADC readings on the RZC, leading to either false overcurrent (nuisance trip, fail-safe) or missed overcurrent (reduced protection). Ground noise affecting pedal sensor SPI communication is extremely unlikely (SPI uses differential clock/data, not ground-referenced analog).

**Residual risk**: Low. ADC software filtering and sensor plausibility checks provide robust noise rejection. Ground noise is a gradual/persistent phenomenon, detectable by plausibility checks. The BTS7960 hardware overcurrent protection (43A trip) provides noise-immune backup.

**Acceptance**: Residual risk is acceptable. Multiple layers of noise mitigation (star ground, filtering, plausibility) and the hardware overcurrent backup reduce ground noise risk to a negligible level for safety.

---

### 6.5 CCF-005: Common Development Environment (GCC ARM Compiler)

#### 6.5.1 Description

The CVC, FZC, and RZC firmware are all compiled with the same toolchain: `arm-none-eabi-gcc` (GCC ARM Embedded). A systematic defect in the compiler (incorrect code generation, optimization bug, linking error) could produce identical incorrect behavior in all three zone ECUs.

#### 6.5.2 Coupling Factors

| Factor | Description |
|--------|-------------|
| Compiler vendor | Same GCC ARM version used for all zone ECU builds |
| Compiler flags | Same optimization level (-O2 or -Os) and warning flags |
| C standard library | Same newlib-nano runtime library |
| Linker | Same ld linker with same linker scripts (per-ECU memory layout differs) |
| CMSIS | Same CMSIS-Core and STM32 HAL library version |

#### 6.5.3 Affected Safety Goals

A compiler bug that generates incorrect code could affect any safety-critical function on all three zone ECUs simultaneously. All safety goals depending on zone ECU software (SG-001 through SG-007) could be affected.

#### 6.5.4 Mitigations

| Mitigation | Mechanism | Effectiveness |
|------------|-----------|---------------|
| **SC uses different compiler** | SC firmware (TMS570) is compiled with TI ARM Compiler (ti-cgt-arm). Different compiler vendor, different code generator, different optimization strategies. A bug in GCC ARM does not affect TI ARM compiler. | **High** -- compiler diversity is the primary architectural mitigation. |
| SC does not use BSW stack | SC firmware is minimal (~400 LOC), not using the shared AUTOSAR-like BSW stack. This eliminates shared library code as a coupling factor between SC and zone ECUs. | High -- SC software is entirely independent of zone ECU software. |
| Compiler qualification | Per ISO 26262-8, the compiler is classified as TCL 2 (tool confidence level). GCC ARM is not safety-certified but is validated through extensive testing. MISRA C compliance and unit testing of compiled output provide confidence. | Moderate -- validation provides evidence but not formal certification. |
| Code review and static analysis | All safety-critical code is reviewed for compiler-sensitive patterns (undefined behavior, implementation-defined behavior, optimization-sensitive constructs). Static analysis (MISRA C checker) flags constructs that may be compiled differently. | Moderate -- catches known problematic patterns. |

#### 6.5.5 Residual Risk Assessment

**Compiler bug**: The probability of an undiscovered GCC ARM optimization bug affecting safety-critical code paths is low but non-zero. GCC ARM is one of the most widely used embedded compilers with millions of deployed systems.

**Residual risk**: If a compiler bug affects all zone ECUs, the SC (compiled with TI ARM) detects the anomaly through heartbeat monitoring and cross-plausibility. SC opens kill relay. The diverse compiler strategy is the primary defense.

**Acceptance**: Residual risk is acceptable. Compiler diversity (GCC ARM vs TI ARM) combined with the SC's independent monitoring provides sufficient independence per ISO 26262-9. Additional assurance is provided by compiler validation testing and static analysis.

---

### 6.6 CCF-006: Shared BSW Stack (AUTOSAR-like)

#### 6.6.1 Description

The CVC, FZC, and RZC share the same AUTOSAR-like Basic Software (BSW) stack implementation, including: MCAL (CAN, SPI, ADC, PWM, Dio, Gpt), EcuAL (CanIf, PduR, IoHwAb), Services (Com, Dcm, Dem, WdgM, BswM, E2E), and RTE. A systematic software defect in a shared BSW module could cause identical failures on all three zone ECUs.

#### 6.6.2 Coupling Factors

| Factor | Description |
|--------|-------------|
| Shared source code | Same BSW source files compiled into all three zone ECU firmware images |
| Shared MCAL drivers | CAN driver, SPI driver, ADC driver are the same source (configured differently per ECU) |
| Shared communication stack | CanIf → PduR → Com is identical across all three ECUs |
| Shared diagnostic stack | Dem, Dcm modules are shared |
| Shared E2E library | Same E2E protection implementation on all senders and receivers |
| Shared BSW scheduler | BswM mode management and RTE runnable scheduling are shared |

#### 6.6.3 Affected Safety Goals

A defect in a shared BSW module (e.g., CanIf message routing error, Com serialization bug, Dem DTC storage corruption) could affect all three zone ECUs. Since the CAN stack is shared, a bug in CanIf or PduR could disrupt all safety-critical CAN communication simultaneously.

#### 6.6.4 Mitigations

| Mitigation | Mechanism | Effectiveness |
|------------|-----------|---------------|
| **SC does NOT use BSW stack** | The SC firmware is a standalone, minimal implementation (~400 lines of C). It uses bare-metal DCAN register access (no CanIf, no PduR, no Com). It has its own CAN message parsing, heartbeat logic, and GPIO control. No shared code with the zone ECU BSW. | **High** -- SC is completely independent of the BSW stack. A BSW bug cannot affect SC. |
| SC kill relay | If a BSW bug causes all zone ECUs to malfunction (e.g., CAN stack fails, heartbeats stop), SC detects heartbeat loss and opens kill relay. | High -- SC provides hardware-enforced safe state independent of BSW. |
| BSW unit testing | Each BSW module has unit tests with target coverage: 100% statement/branch for ASIL D modules, MC/DC for safety-critical paths. | Moderate -- testing catches bugs but cannot guarantee absence of all defects. |
| BSW code review | All BSW modules undergo independent code review per ISO 26262-6. MISRA C compliance verified. | Moderate -- review catches systematic design flaws. |
| BSW configuration diversity | Although the BSW source is shared, each ECU has different configuration (different CAN IDs, different SPI channel assignments, different ADC channels). A configuration-triggered bug would typically affect only one ECU. | Low -- helps with configuration-specific bugs but not generic logic bugs. |

#### 6.6.5 Residual Risk Assessment

**BSW systematic defect**: A generic bug in CanIf message routing or Com serialization could disrupt CAN communication on all zone ECUs. SC detects this as heartbeat loss within 200 ms.

**BSW infinite loop or deadlock**: If the BSW scheduler enters a deadlock or the RTE dispatch hangs, all zone ECUs freeze. External watchdogs (TPS3823) reset each ECU within 1.6 seconds. SC heartbeat monitoring detects silence within 200 ms.

**Residual risk**: The SC provides diverse, independent detection and shutdown capability. A BSW bug that silently corrupts data (e.g., torque value corruption without hang) is the highest-risk scenario. SC cross-plausibility (SM-003: torque request vs actual current) provides some defense, but with limitations (60 ms detection time, 20% threshold). Zone ECU local plausibility checks (SM-001, SM-008, SM-011) may also catch data corruption depending on the bug manifestation.

**Acceptance**: Residual risk is acceptable. The SC's independence from the BSW stack is the architectural cornerstone. The combination of SC monitoring, external watchdogs, and BSW testing/review provides sufficient assurance per ISO 26262 requirements for systematic fault avoidance.

---

### 6.7 CCF Summary Table

| CCF-ID | Coupling Factor | Elements Affected | Safety Goals | Mitigation | Coverage | Residual Risk |
|--------|----------------|-------------------|-------------|------------|----------|---------------|
| CCF-001 | Shared CAN bus | CVC, FZC, RZC, SC | All (SG-001 to SG-008) | Timeout safe defaults, E2E protection, SC kill relay | High | 100-200 ms detection latency; acceptable with local safety mechanisms |
| CCF-002 | Same MCU vendor (ST) | CVC, FZC, RZC | SG-001 to SG-007 | Diverse SC (TI TMS570), SC kill relay | High | SC detects via heartbeat; architectural diversity proven |
| CCF-003 | Shared 12V supply | All ECUs | All | Energize-to-run relay pattern (power loss = safe state), BOD | High | Minimal; 12V loss is inherently safe by design |
| CCF-004 | Shared ground | RZC ADC sensors primarily | SG-006 (ASIL A) | Star ground, filtering, plausibility, HW overcurrent backup | High | Negligible; multiple noise mitigation layers |
| CCF-005 | Same compiler (GCC ARM) | CVC, FZC, RZC | SG-001 to SG-007 | SC uses TI ARM compiler (diverse toolchain), compiler validation | High | SC detection within 200 ms; compiler diversity is architectural |
| CCF-006 | Shared BSW stack | CVC, FZC, RZC | SG-001 to SG-007 | SC has no BSW (bare-metal ~400 LOC), SC kill relay, BSW testing | High | SC detects BSW failure via heartbeat; diverse software stack |

## 7. Cascading Failure Analysis

### 7.1 CF-001: CVC Sends Wrong Torque Command Leading to Motor Damage

#### 7.1.1 Failure Chain

```
CVC software fault or CAN corruption
    |
    v
CVC sends maximum torque command to RZC (100% duty cycle)
    |
    v
RZC drives motor at full power (BTS7960 full PWM)
    |
    v
Motor draws excessive current (> rated) --> overcurrent
    |
    v
Motor overheats --> winding insulation breakdown --> potential smoke/fire
    |
    v
Potential violation of SG-001 (unintended acceleration) and SG-006 (motor protection)
```

#### 7.1.2 Detection Mechanisms

| Stage | Detection | Mechanism | Time to Detect |
|-------|-----------|-----------|---------------|
| 1 | CVC sends wrong command | SM-001: Pedal plausibility (if pedal position does not match command). SM-004: E2E CRC detects CAN corruption. | 10-20 ms |
| 2 | RZC drives excessive current | SM-002: RZC overcurrent cutoff (ACS723 > 25A for > 10 ms). BTS7960 hardware overcurrent (~43A, immediate). | 10-11 ms |
| 3 | SC detects discrepancy | SM-003: Cross-plausibility (torque request vs actual current, 20% threshold, 50 ms). | 50-60 ms |
| 4 | Motor overheats | SM-015: NTC temperature monitoring (temperature threshold, 100 ms cycle). | 100 ms - minutes (depends on thermal time constant) |

#### 7.1.3 Mitigation Effectiveness

| Mitigation | Effectiveness | Justification |
|------------|---------------|---------------|
| SM-002 (overcurrent cutoff) | **High** | RZC independently limits current regardless of CVC command. Cuts motor power within 11 ms. Does not depend on CAN or CVC. |
| BTS7960 hardware protection | **High** | Hardware overcurrent at ~43A. Independent of all software. Survives MCU hang. |
| SM-003 (SC cross-plausibility) | **Moderate** | Detects torque/current mismatch but with 60 ms latency. Provides backup if SM-002 fails. |
| SM-015 (temperature monitoring) | **Moderate** | Slow response (thermal time constant). Catches sustained overcurrent that SM-002 misses. |

#### 7.1.4 Residual Risk

The cascading chain is broken at stage 2: RZC overcurrent cutoff and BTS7960 hardware protection independently prevent motor damage regardless of the CVC command. The cascade from "wrong torque command" to "motor damage" requires simultaneous failure of SM-002 and BTS7960 hardware protection -- a dual-point failure with very low probability.

**Verdict**: Cascade adequately mitigated. No additional action required.

---

### 7.2 CF-002: CAN Bus Flood Leading to SC False Kill

#### 7.2.1 Failure Chain

```
One ECU enters babbling state (e.g., CVC interrupt storm)
    |
    v
CAN bus flooded with high-priority messages from babbling node
    |
    v
Other ECU heartbeat messages delayed or lost (cannot arbitrate)
    |
    v
SC perceives heartbeat timeout for delayed ECUs
    |
    v
SC opens kill relay (false kill activation)
    |
    v
System enters SHUTDOWN -- safe but unnecessary availability loss
```

#### 7.2.2 Detection Mechanisms

| Stage | Detection | Mechanism | Time to Detect |
|-------|-----------|-----------|---------------|
| 1 | Babbling node | CAN protocol bus-off mechanism (128 error frames = bus-off, ~2 ms at 500 kbps). | ~2 ms |
| 2 | Heartbeat delays | SM-019: Heartbeat timeout (150 ms + 50 ms debounce = 200 ms). | 200 ms |
| 3 | SC evaluates bus state | SC monitors total bus activity pattern. If SC sees messages from the babbling node but not from others, it can distinguish babbling from total bus failure. | Implementation-dependent |

#### 7.2.3 Mitigation Effectiveness

| Mitigation | Effectiveness | Justification |
|------------|---------------|---------------|
| CAN bus-off mechanism | **High** | CAN protocol automatically disables the babbling node within ~2 ms (128 error frames). After bus-off, other nodes resume normal communication. |
| SC debounce timer (50 ms) | **Moderate** | Reduces false triggers from transient CAN bus contention. If the babbling node is removed by bus-off within 2 ms, the heartbeat messages resume before the debounce timer expires. |
| SC timeout-based detection | **Moderate** | SC uses absence of heartbeats (timeout) rather than presence of specific messages. A flooded bus does not generate false heartbeats -- it only delays real ones. |
| Heartbeat CAN ID priority | **High** | Heartbeat messages use high-priority CAN IDs (0x010-0x013). Only messages with CAN ID < 0x010 (E-stop at 0x001) can preempt heartbeats. A babbling node with a lower-priority CAN ID cannot delay heartbeats. |

#### 7.2.4 Residual Risk

The CAN bus-off mechanism resolves the babbling node within ~2 ms, well before the SC heartbeat timeout (200 ms). The cascade from "babbling node" to "false kill" requires the babbling node to sustain flooding for > 200 ms while defeating the CAN bus-off mechanism -- this is contradictory (bus-off activates within 2 ms). The only scenario where the cascade completes is if the babbling node uses a CAN ID with higher priority than all heartbeat messages AND the CAN controller does not enter bus-off (hardware fault). The probability of this exact combination is very low.

**Verdict**: Cascade adequately mitigated. False kill is the worst outcome, which is safe (availability loss, not a hazard).

---

### 7.3 CF-003: FZC Emergency Brake Leading to Occupant Injury

#### 7.3.1 Failure Chain

```
FZC detects obstacle via lidar (legitimate or false positive)
    |
    v
FZC initiates emergency braking (SM-017, full brake servo)
    |
    v
Vehicle decelerates at maximum rate (estimated 5 m/s^2)
    |
    v
Unsecured objects in vehicle slide forward
    |
    v
Minor occupant injury from loose objects (whiplash, object impact)
```

#### 7.3.2 Detection Mechanisms

This cascade is an operational scenario rather than a fault cascade. The initial trigger (lidar obstacle detection) is a designed function, not a failure.

| Stage | Detection | Mechanism | Time to Detect |
|-------|-----------|-----------|---------------|
| 1 | Lidar detection | SM-017: Distance monitoring with graduated thresholds (100 cm warning, 50 cm brake, 20 cm emergency). | 10 ms |
| 2 | Emergency brake | Brake servo actuated to maximum. No fault -- this is designed behavior. | Immediate |
| 3 | Deceleration | Vehicle decelerates. Occupant experiences deceleration force. | Continuous |

#### 7.3.3 Mitigation Effectiveness

| Mitigation | Effectiveness | Justification |
|------------|---------------|---------------|
| Graduated response | **High** | Warning at 100 cm gives occupant time to brace. Braking at 50 cm is moderate. Emergency at 20 cm is full brake. Progressive approach reduces surprise deceleration. |
| Brake ramp limiting | **Moderate** | FZC applies brake force via a ramp (not instantaneous step) to reduce jerk. SM-014 validates brake command magnitude. |
| Lidar false positive filtering (SM-018) | **Moderate** | Multi-reading confirmation (3 consecutive readings below threshold before triggering emergency brake) reduces false positive emergency braking. |

#### 7.3.4 Residual Risk

Emergency braking at maximum deceleration is inherent to the safety function. The residual risk is minor occupant discomfort or loose object injury, which is an operational hazard inherent to any emergency braking system. This is accepted as a necessary tradeoff: the consequence of NOT braking (collision with obstacle) is more severe than the consequence of braking (deceleration discomfort).

**Verdict**: Cascade is an operational scenario. Mitigations (graduated response, ramp limiting) reduce severity. Residual risk accepted as inherent to emergency braking function.

---

### 7.4 CF-004: RZC Overcurrent Trip Leading to Loss of Propulsion

#### 7.4.1 Failure Chain

```
Motor experiences temporary overcurrent (startup inrush, load spike, brief stall)
    |
    v
SM-002 overcurrent threshold exceeded for > 10 ms
    |
    v
RZC disables motor (R_EN=LOW, L_EN=LOW)
    |
    v
Motor stops -- loss of drive torque during driving
    |
    v
Vehicle decelerates unexpectedly in traffic
    |
    v
Risk of rear-end collision from following vehicle (SG-002)
```

#### 7.4.2 Detection Mechanisms

| Stage | Detection | Mechanism | Time to Detect |
|-------|-----------|-----------|---------------|
| 1 | Overcurrent event | SM-002: ACS723 > 25A for > 10 ms. Debounce timer (10 ms) is designed to ride through normal transients. | 10 ms |
| 2 | Motor disabled | RZC sets enable pins LOW, broadcasts motor fault DTC on CAN. | < 1 ms |
| 3 | Speed decrease | CVC detects motor status change via CAN. Encoder confirms speed decreasing. | 10-50 ms |
| 4 | Controlled response | CVC transitions to DEGRADED or SAFE_STOP. FZC applies brakes for controlled stop. | 10-20 ms |

#### 7.4.3 Mitigation Effectiveness

| Mitigation | Effectiveness | Justification |
|------------|---------------|---------------|
| Overcurrent debounce (10 ms) | **High** | Prevents nuisance trips from normal motor transients (startup inrush ~5 ms, load spikes ~1 ms). Only sustained overcurrent triggers cutoff. |
| Controlled stop (not abrupt relay kill) | **High** | SM-002 disables the motor driver (gradual deceleration via friction) rather than opening the kill relay (abrupt power loss). This provides a smoother deceleration profile. |
| CVC state machine coordination | **High** | CVC receives motor fault notification via CAN and coordinates FZC brake application for a controlled stop. Hazard lights activated via BCM. |
| Automatic restart attempt | **Moderate** | After overcurrent clears (current drops below threshold), RZC may attempt controlled re-enable with reduced torque. Requires CVC authorization. |

#### 7.4.4 Residual Risk

The cascade from "overcurrent trip" to "loss of propulsion" is inherent to the overcurrent protection function. The residual risk is a rear-end collision from the following vehicle, which corresponds to SG-002 (ASIL B, S2). The controlled stop sequence (motor coast-down + FZC brake application + hazard lights) minimizes this risk by providing a gradual, visible deceleration.

**Verdict**: Cascade is an expected operational scenario for overcurrent protection. Controlled stop sequence mitigates the rear-end collision risk. Residual risk is within SG-002 ASIL B acceptance criteria.

---

### 7.5 CF-005: CVC State Machine Stuck in INIT

#### 7.5.1 Failure Chain

```
CVC startup self-test fails partially (e.g., one sensor intermittent)
    |
    v
CVC state machine remains in INIT (self-test not passed, not failed)
    |
    v
CVC never transitions to RUN mode
    |
    v
No torque/steering/brake commands sent (commands only valid in RUN mode)
    |
    v
FZC/RZC receive no commands -- timeout-based safe defaults activate
    |
    v
SC detects CVC heartbeat but with INIT mode flag -- no state transition
    |
    v
System remains permanently in INIT -- no driving possible
```

#### 7.5.2 Detection Mechanisms

| Stage | Detection | Mechanism | Time to Detect |
|-------|-----------|-----------|---------------|
| 1 | Self-test incomplete | CVC internal INIT timeout timer (calibratable, default: 5 seconds). If self-test not complete within timeout, CVC transitions to SAFE_STOP. | 5000 ms |
| 2 | No RUN transition | SC monitors CVC heartbeat mode field. If CVC remains in INIT for > startup timeout, SC flags "CVC INIT stuck" condition. | 5000 ms |
| 3 | External watchdog | TPS3823 watchdog. If CVC firmware hangs during self-test (not just slow), external watchdog resets CVC within 1.6 seconds. | 1600 ms |
| 4 | Operator observation | OLED displays INIT state persistently. Operator observes system is not responding. | Human-dependent |

#### 7.5.3 Mitigation Effectiveness

| Mitigation | Effectiveness | Justification |
|------------|---------------|---------------|
| INIT timeout | **High** | CVC firmware includes a maximum INIT duration. After timeout, state machine transitions to SAFE_STOP rather than remaining in INIT indefinitely. |
| External watchdog | **High** | If the self-test code itself hangs (infinite loop in self-test), TPS3823 forces reset. After reset, CVC retries self-test. |
| Retry with degraded mode | **Moderate** | If one sensor fails self-test, CVC may transition to DEGRADED mode (single sensor operation) rather than blocking in INIT. Requires careful design to avoid masking genuine faults. |

#### 7.5.4 Residual Risk

CVC stuck in INIT prevents all driving. This is a fail-safe condition (no motion = no hazard). The residual risk is availability loss only. The INIT timeout and external watchdog ensure that the system does not remain indefinitely in an ambiguous state -- it either completes startup or transitions to a defined safe state.

**Verdict**: Cascade results in availability loss, not a safety hazard. Mitigations (INIT timeout, watchdog) prevent indefinite ambiguous state. Residual risk is acceptable.

---

### 7.6 CF Summary Table

| CF-ID | Source Element | Initial Failure | Target Element | Propagated Effect | Safety Goal | Mitigation | Residual Risk |
|-------|---------------|-----------------|----------------|-------------------|-------------|------------|---------------|
| CF-001 | CVC | Wrong torque command | RZC → Motor | Motor overcurrent, potential damage | SG-001 (D), SG-006 (A) | RZC overcurrent (SM-002), BTS7960 HW, SC cross-plausibility (SM-003) | Very low -- dual independent protection layers |
| CF-002 | Any ECU (babbling) | CAN bus flood | SC → Kill relay | False kill relay activation | SG-008 (C) | CAN bus-off (2 ms), heartbeat priority, SC debounce | Very low -- bus-off resolves before SC timeout |
| CF-003 | FZC lidar | Emergency brake trigger | Vehicle occupants | Deceleration injury | SG-005 (A) | Graduated response, brake ramp, false positive filtering | Low -- inherent to emergency braking function |
| CF-004 | Motor (overcurrent) | Overcurrent trip | Vehicle (propulsion loss) | Rear-end collision risk | SG-002 (B) | Debounce (10 ms), controlled stop, CVC coordination | Low -- controlled stop reduces collision risk |
| CF-005 | CVC (self-test) | INIT stuck | System (no operation) | Permanent INIT, no driving | SG-002 (B) | INIT timeout (5 s), external watchdog, OLED display | Minimal -- fail-safe (no motion = safe) |

## 8. Mitigation Effectiveness Summary

### 8.1 CCF Mitigations

| CCF-ID | Primary Mitigation | Coverage Rating | Secondary Mitigation | Combined Coverage |
|--------|-------------------|-----------------|---------------------|-------------------|
| CCF-001 | Timeout-based safe defaults per ECU | High (95%) | SC kill relay on heartbeat timeout | Very High (99%) |
| CCF-002 | Diverse SC vendor (TI TMS570) | High (98%) | SC kill relay on heartbeat loss | Very High (99.5%) |
| CCF-003 | Energize-to-run relay pattern | Very High (99%) | MCU brown-out detection | Very High (99.9%) |
| CCF-004 | Star ground + ADC filtering | High (95%) | Sensor plausibility + BTS7960 HW protection | Very High (99%) |
| CCF-005 | Diverse SC compiler (TI ARM) | High (98%) | SC bare-metal firmware (no shared code) | Very High (99.5%) |
| CCF-006 | SC does not use BSW stack | Very High (99%) | SC kill relay + external watchdogs | Very High (99.9%) |

### 8.2 CF Mitigations

| CF-ID | Primary Mitigation | Coverage Rating | Secondary Mitigation | Combined Coverage |
|-------|-------------------|-----------------|---------------------|-------------------|
| CF-001 | RZC overcurrent cutoff (SM-002) | High (95%) | BTS7960 HW overcurrent (43A) | Very High (99.5%) |
| CF-002 | CAN bus-off mechanism (~2 ms) | Very High (99%) | SC heartbeat debounce (50 ms) | Very High (99.9%) |
| CF-003 | Graduated lidar response | High (90%) | Brake ramp limiting | High (95%) |
| CF-004 | Overcurrent debounce (10 ms) | High (95%) | Controlled stop coordination | Very High (99%) |
| CF-005 | INIT timeout (5 seconds) | High (95%) | External watchdog (TPS3823, 1.6 s) | Very High (99%) |

## 9. Independence Arguments

This section provides the evidence that the assumed independence between the zone ECU cluster (CVC, FZC, RZC) and the Safety Controller (SC) is valid. This is the core independence argument for the Taktflow safety architecture.

### 9.1 Hardware Independence

| Dimension | Zone ECUs (CVC, FZC, RZC) | Safety Controller (SC) | Independence Achieved |
|-----------|---------------------------|------------------------|----------------------|
| **MCU vendor** | STMicroelectronics | Texas Instruments | **Yes** -- different vendor eliminates common silicon defects |
| **CPU architecture** | ARM Cortex-M4F (single core, no lockstep) | ARM Cortex-R5F (dual-core lockstep, hardware comparator) | **Yes** -- different CPU IP, different design team (ARM Cortex-M vs Cortex-R division) |
| **Safety certification** | STM32G474RE: no ASIL certification; safety achieved through architecture | TMS570LC43x: TUV-certified safety manual, ASIL D capable (hardware) | **Yes** -- SC hardware has formal safety certification |
| **CAN interface** | FDCAN (ST peripheral IP) | DCAN (TI peripheral IP) | **Yes** -- different CAN controller implementations |
| **CAN transceiver** | TJA1051T/3 (NXP) | SN65HVD230 (TI) | **Yes** -- different transceiver vendor and model |
| **CAN mode** | Normal mode (TX + RX) | **Listen-only mode (RX only)** | **Yes** -- SC cannot transmit, cannot corrupt bus, cannot be a babbling node |
| **Clock source** | Internal HSI + PLL (STM32) | Internal oscillator + PLL (TMS570) | **Yes** -- independent clock sources |
| **Reset source** | TPS3823 external watchdog per ECU | TPS3823 external watchdog + lockstep ESM | **Yes** -- independent watchdog ICs (one per ECU) |
| **Power regulation** | Nucleo onboard 3.3V regulator | LaunchPad onboard 3.3V regulator | **Partial** -- both derive from same 12V rail, but separate 3.3V regulators |

### 9.2 Software Independence

| Dimension | Zone ECUs (CVC, FZC, RZC) | Safety Controller (SC) | Independence Achieved |
|-----------|---------------------------|------------------------|----------------------|
| **Compiler** | arm-none-eabi-gcc (GCC ARM Embedded) | ti-cgt-arm (TI ARM Compiler) | **Yes** -- different compiler vendor, different code generator |
| **RTOS / scheduler** | AUTOSAR-like BSW scheduler (BswM + RTE) | Bare-metal (no RTOS, no scheduler, simple main loop) | **Yes** -- completely different execution model |
| **BSW stack** | Full AUTOSAR-like BSW (MCAL, CanIf, PduR, Com, Dcm, Dem, WdgM, E2E) | **None** -- bare-metal register access, ~400 LOC | **Yes** -- no shared software libraries between SC and zone ECUs |
| **CAN message parsing** | CanIf → PduR → Com layered stack | Direct DCAN mailbox register read with manual byte extraction | **Yes** -- completely different CAN handling code |
| **Application logic** | Complex state machines, sensor fusion, actuator control | Simple heartbeat counters + threshold comparison + GPIO toggle | **Yes** -- SC logic is minimal and independently verifiable |
| **Code size** | Thousands of LOC per ECU | ~400 LOC total | **Yes** -- SC code is small enough for exhaustive review and formal verification |
| **Development team** | Could share developers | Should have independent reviewer at minimum | **Partial** -- portfolio project; mitigated by independent review + 100% MC/DC testing |

### 9.3 Functional Independence

| Dimension | Zone ECUs (CVC, FZC, RZC) | Safety Controller (SC) | Independence Achieved |
|-----------|---------------------------|------------------------|----------------------|
| **Primary function** | Implement vehicle functions (drive, steer, brake) | Monitor vehicle functions (heartbeat, plausibility, kill) | **Yes** -- different functional purpose |
| **CAN bus role** | Active participants (TX + RX) | Passive observer (RX only, listen-only mode) | **Yes** -- SC cannot influence CAN bus traffic |
| **Fault response** | Degrade, limp, controlled stop | Kill (ultimate shutdown via relay) | **Yes** -- different response strategies |
| **Sensor inputs** | Direct sensor interfaces (SPI, UART, ADC) | CAN messages only (indirect observation) | **Yes** -- SC does not share any sensor hardware |
| **Actuator outputs** | Motor driver (PWM), servos (PWM), OLED, buzzer | Kill relay (GPIO), fault LEDs (GPIO) | **Yes** -- SC controls only the kill relay, not the motor/servos |
| **Safe state enforcement** | Software-commanded safe defaults (torque=0, brake=on, steer=center) | Hardware-enforced power removal (relay opens, actuator power cut) | **Yes** -- SC's safe state does not depend on zone ECU cooperation |

### 9.4 Independence Verdict

| Independence Assumption | Verdict | Justification |
|------------------------|---------|---------------|
| IA-001: CVC and SC independent | **Satisfied** | Different MCU vendor, different compiler, different software stack, CAN listen-only mode. No shared hardware other than 12V rail (mitigated by energize-to-run). |
| IA-002: FZC and SC independent | **Satisfied** | Same as IA-001. FZC and SC share no sensors, no actuators, no software. |
| IA-003: RZC and SC independent | **Satisfied** | Same as IA-001. RZC and SC share no sensors, no actuators, no software. |
| IA-004: Dual pedal sensors independent | **Partially satisfied** | Two AS5048A sensors on the same SPI bus with separate chip selects. Independent sensor ICs but shared SPI bus (CCF risk). SPI bus failure = both sensors lost (FM-CVC-012). Mitigated by SM-003 (SC cross-plausibility). For full ASIL D, separate SPI buses would strengthen independence. |
| IA-005: Zone ECU mechanisms and SC kill relay independent | **Satisfied** | Zone ECU mechanisms are software-based on STM32. SC kill relay is hardware-based on TMS570. No shared code, no shared hardware (except 12V rail). Kill relay works without zone ECU cooperation. |
| IA-006: External watchdog and MCU independent | **Satisfied** | TPS3823 is an independent IC with its own oscillator. It monitors only the WDI toggle signal. MCU firmware must actively service the watchdog; any MCU failure that stops the toggle triggers a reset. |

### 9.5 Independence Gap: Dual Pedal Sensors on Shared SPI Bus

**Finding**: IA-004 is only partially satisfied because both pedal sensors (AS5048A) share SPI1 on the CVC. A single SPI bus failure (FM-CVC-012, severity 9) causes both sensors to become unreadable, defeating the dual-sensor redundancy.

**Risk**: This is a common cause failure path for SG-001 (ASIL D). The SPI bus is a single point of failure for pedal input.

**Current mitigation**: SM-001 detects SPI errors (timeout, CRC, parity) and sets torque = 0 within 10 ms. SM-003 (SC cross-plausibility) provides diverse backup detection.

**Recommended improvement**: For production ASIL D compliance, separate SPI buses (SPI1 for sensor 1, SPI2 for sensor 2) would eliminate this common cause. On the STM32G474RE, SPI2 is available on alternate pins. This improvement is documented as a design recommendation for future revisions.

**Acceptance for current design**: The shared SPI bus is accepted for the portfolio demonstration because:
1. SPI bus failure is detectable within 10 ms (SM-001).
2. The failure mode is fail-safe (both sensors unreadable = torque request = 0).
3. SC cross-plausibility provides independent diverse detection.
4. The limitation is documented and the improvement path is identified.

## 10. Beta-Factor Analysis for CCF Quantification

Per IEC 61508-7, Annex D, the beta-factor method provides a quantitative estimate of the fraction of random hardware failures that are common cause. The beta-factor (beta) represents the probability that a fault affecting one channel also affects the redundant channel.

### 10.1 Zone ECU to SC Beta-Factor

| Defense Measure | Score | Justification |
|-----------------|-------|---------------|
| Separation/segregation | 3 (best) | SC is a separate PCB (LaunchPad), different physical location on bench, separate power regulation |
| Diversity | 3 (best) | Different MCU vendor (TI vs ST), different CPU architecture (Cortex-R5 vs Cortex-M4), different compiler |
| Complexity/design | 2 | SC is minimal (~400 LOC) but not trivially simple. Zone ECUs are complex but well-structured. |
| Assessment/analysis | 2 | FMEA and DFA performed. Code review planned. No formal verification (yet). |
| Procedures/testing | 2 | Unit testing, integration testing planned. No operating experience in the field. |
| Competence/training | 1 | Single developer (portfolio project). Mitigated by comprehensive rules and review processes. |
| Environmental control | 2 | Indoor bench (controlled temperature, no vibration). Not automotive environment. |

**Beta-factor estimate**: Based on IEC 61508-7 scoring: total score = 15 out of 21. This maps to approximately beta = 1% (beta = 0.01). This means that approximately 1% of random hardware failures in a zone ECU would also affect the SC, despite the diversity measures.

**Interpretation**: A beta-factor of 1% is consistent with a "high diversity" architectural design, which is appropriate for ASIL D. The dominant residual common cause is the shared 12V power supply (CCF-003), which is mitigated by the energize-to-run relay pattern.

### 10.2 Dual Pedal Sensor Beta-Factor

| Defense Measure | Score | Justification |
|-----------------|-------|---------------|
| Separation/segregation | 1 | Same SPI bus, same PCB area, same connector block |
| Diversity | 0 | Same sensor IC (AS5048A), same measurement principle (magnetic), same manufacturer |
| Complexity/design | 1 | Simple sensor interface but shared bus is a significant coupling factor |
| Assessment/analysis | 2 | FMEA and DFA identify the shared SPI bus as a common cause |
| Procedures/testing | 1 | Plausibility testing validates both sensors but does not exercise SPI bus failures |
| Environmental control | 2 | Indoor bench, controlled environment |

**Beta-factor estimate**: Total score = 7 out of 21. This maps to approximately beta = 5% (beta = 0.05). This is a relatively high beta-factor, reflecting the significant coupling through the shared SPI bus.

**Interpretation**: The high beta-factor for dual pedal sensors confirms the finding in Section 9.5: the shared SPI bus weakens the independence of the dual sensor arrangement. For production ASIL D, this beta-factor would require either: (a) separate SPI buses to reduce coupling, or (b) an independent diverse sensor technology (e.g., one magnetic + one resistive). The current design is accepted for the portfolio demonstration with the documented limitation.

## 11. Summary of Findings

### 11.1 Overall Assessment

The Taktflow Zonal Vehicle Platform architecture provides robust defense against dependent failures:

1. **Diverse redundancy**: The Safety Controller (TI TMS570) provides diverse, independent monitoring of the zone ECU cluster (ST STM32). This is the strongest architectural defense against common cause failures.

2. **Energize-to-run relay pattern**: Power supply loss results in inherent safe state (relay opens, actuators de-powered). This converts the shared power supply from a hazard into a safety mechanism.

3. **Timeout-based safe defaults**: Every receiving ECU implements independent CAN timeout detection with safe default actions. CAN bus failure (the primary shared infrastructure) results in all ECUs independently achieving safe states.

4. **Multi-layer protection**: Critical safety functions (SG-001, SG-003, SG-004) are protected by 3-4 independent layers: local software mechanism, diverse software mechanism (SC), hardware mechanism (BTS7960/TPS3823), and ultimate hardware backup (kill relay).

### 11.2 Identified Weaknesses

| ID | Weakness | Severity | Recommendation |
|----|----------|----------|----------------|
| DFA-W-001 | Dual pedal sensors share SPI bus (common cause path for SG-001) | High | Separate SPI buses in future revision (SPI1 for sensor 1, SPI2 for sensor 2) |
| DFA-W-002 | All zone ECUs share single CAN bus (no redundant communication) | Medium | Accepted for portfolio demo. Production system would require redundant CAN/Ethernet. |
| DFA-W-003 | SC cross-plausibility timing (60 ms) exceeds SG-001 FTTI (50 ms) | Medium | Optimize SC processing to reduce detection time to < 50 ms. Primary mechanisms (SM-001, SM-002) provide FTTI-compliant coverage. |
| DFA-W-004 | Kill relay stuck-closed is a residual single-point failure | Medium | Implement relay contact feedback circuit (ADC readback). Kill relay self-test at every startup. |
| DFA-W-005 | Shared 12V supply for SC and zone ECUs | Low | Energize-to-run pattern makes this safe by design. No additional mitigation needed. |

### 11.3 Actions Required

| Priority | Action | Weakness | Target Phase |
|----------|--------|----------|-------------|
| 1 | Implement relay contact feedback circuit on SC | DFA-W-004 | Phase 3 (hardware integration) |
| 2 | Validate SPI bus failure detection timing (SM-001) via fault injection | DFA-W-001 | Phase 4 (SIL testing) |
| 3 | Optimize SC cross-plausibility processing time | DFA-W-003 | Phase 3 (SC firmware) |
| 4 | Document SPI bus limitation in technical safety concept | DFA-W-001 | Phase 2 (TSC) |
| 5 | CAN bus failure fault injection testing (disconnect, short, babbling) | DFA-W-002 | Phase 4 (SIL/PIL testing) |

## 12. Traceability

### 12.1 DFA to Safety Goals

| CCF/CF | Safety Goals Affected | FMEA Cross-Reference |
|--------|----------------------|---------------------|
| CCF-001 (Shared CAN bus) | SG-001 to SG-008 | FM-CAN-001, FM-CAN-002, FM-CAN-005 |
| CCF-002 (Same MCU vendor) | SG-001 to SG-007 | FM-CVC-005 (common cause sensors) |
| CCF-003 (Shared 12V supply) | All | FM-PWR-001, FM-PWR-003 |
| CCF-004 (Shared ground) | SG-006 | FM-PWR-004 |
| CCF-005 (Same compiler) | SG-001 to SG-007 | N/A (systematic fault, not component failure) |
| CCF-006 (Shared BSW) | SG-001 to SG-007 | N/A (systematic fault, not component failure) |
| CF-001 (Wrong torque → overcurrent) | SG-001, SG-006 | FM-CVC-005, FM-RZC-001 |
| CF-002 (CAN flood → false kill) | SG-008 | FM-CAN-005, FM-SC-002 |
| CF-003 (Emergency brake → injury) | SG-005 | FM-FZC-005 |
| CF-004 (Overcurrent → propulsion loss) | SG-002 | FM-RZC-003 |
| CF-005 (INIT stuck → no operation) | SG-002 | FM-CVC-011 |

### 12.2 DFA to Safety Mechanisms

| CCF/CF | Primary Mitigation (SM) | Secondary Mitigation (SM) |
|--------|------------------------|--------------------------|
| CCF-001 | SM-012 (auto-brake), SM-004 (E2E) | SM-005 (kill relay), SM-019 (heartbeat) |
| CCF-002 | SM-019 (heartbeat on diverse SC) | SM-005 (kill relay), SM-021 (lockstep) |
| CCF-003 | SM-005 (energize-to-run relay) | Brown-out detection (MCU BOD) |
| CCF-004 | SM-015 (temperature plausibility) | SM-002 (overcurrent HW backup, BTS7960) |
| CCF-005 | SM-019 (heartbeat on diverse compiler) | SM-005 (kill relay), SM-021 (lockstep) |
| CCF-006 | SM-019 (heartbeat on non-BSW SC) | SM-005 (kill relay), SM-020 (external WDT) |
| CF-001 | SM-002 (overcurrent cutoff) | SM-003 (SC cross-plausibility), BTS7960 HW |
| CF-002 | CAN bus-off mechanism | SM-019 (heartbeat debounce) |
| CF-003 | SM-017 (graduated lidar response) | SM-014 (brake plausibility) |
| CF-004 | SM-002 (overcurrent debounce) | SM-022 (CVC state machine, controlled stop) |
| CF-005 | INIT timeout (CVC firmware) | SM-020 (external watchdog) |

### 12.3 DFA to Independence Assumptions

| Independence Assumption | CCFs Evaluated | Verdict |
|------------------------|---------------|---------|
| IA-001 (CVC vs SC) | CCF-001, CCF-002, CCF-003, CCF-005, CCF-006 | Satisfied |
| IA-002 (FZC vs SC) | CCF-001, CCF-002, CCF-003, CCF-005, CCF-006 | Satisfied |
| IA-003 (RZC vs SC) | CCF-001, CCF-002, CCF-003, CCF-005, CCF-006 | Satisfied |
| IA-004 (Dual pedal sensors) | CCF-004 (ground noise on SPI), SPI bus commonality | Partially satisfied (shared SPI bus) |
| IA-005 (Zone ECU mechanisms vs SC relay) | CCF-002, CCF-005, CCF-006 | Satisfied |
| IA-006 (External watchdog vs MCU) | N/A (separate IC with independent oscillator) | Satisfied |

## 13. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete DFA: 6 CCFs, 5 CFs, independence arguments, beta-factor analysis, mitigation effectiveness, identified weaknesses and actions |

## 14. Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| FSE Lead | _________________ | __________ | __________ |
| System Architect | _________________ | __________ | __________ |
| HW Engineer | _________________ | __________ | __________ |
| SW Engineer | _________________ | __________ | __________ |
| Safety Manager | _________________ | __________ | __________ |

