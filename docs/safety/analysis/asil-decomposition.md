---
document_id: ASIL-DECOMP
title: "ASIL Decomposition"
version: "1.0"
status: draft
iso_26262_part: 9
iso_26262_clause: "5"
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


# ASIL Decomposition

## 1. Purpose

This document analyzes ASIL decomposition decisions for the Taktflow Zonal Vehicle Platform per ISO 26262-9:2018, Clause 5. ASIL decomposition is a mechanism by which a safety requirement assigned to a hardware-software element is split into redundant safety requirements allocated to sufficiently independent elements, each at a lower ASIL than the original.

The key principle: decomposition reduces the ASIL of individual elements but imposes strict independence requirements between the decomposed elements. Both elements must together achieve the original ASIL, and their independence must be formally demonstrated.

This document covers:
- The decision NOT to apply ASIL decomposition (and why)
- Analysis of where decomposition COULD apply (for completeness and assessor readiness)
- Independence arguments between system elements (useful for both decomposition and dependent failure analysis)
- Constraints that would apply if decomposition were used in future revisions
- Traceability to safety goals, DFA, and system architecture

## 2. References

| Document ID | Title | Version |
|-------------|-------|---------|
| SG | Safety Goals | 1.0 |
| FSC | Functional Safety Concept | 1.0 |
| HW-METRICS | Hardware Architectural Metrics | 1.0 |
| DFA | Dependent Failure Analysis | 0.1 |
| ITEM-DEF | Item Definition | 1.0 |
| FMEA | Failure Mode and Effects Analysis | 0.1 |
| ISO 26262-9:2018 | Road vehicles -- Functional safety -- Part 9: ASIL-oriented and safety-oriented analyses | 2018 |
| ISO 26262-5:2018 | Road vehicles -- Functional safety -- Part 5: Product development at the hardware level | 2018 |
| ISO 26262-1:2018 | Road vehicles -- Functional safety -- Part 1: Vocabulary | 2018 |

## 3. ASIL Decomposition Rules (ISO 26262-9, Clause 5)

### 3.1 Decomposition Scheme

ISO 26262-9 Table 2 defines the allowed decomposition schemes:

| Original ASIL | Decomposed To | Scheme |
|---------------|---------------|--------|
| ASIL D | ASIL D(D) + ASIL D(D) | No decomposition (both elements at full ASIL D) |
| ASIL D | ASIL C(D) + ASIL A(D) | Valid decomposition |
| ASIL D | ASIL B(D) + ASIL B(D) | Valid decomposition |
| ASIL C | ASIL B(C) + ASIL A(C) | Valid decomposition |
| ASIL C | ASIL C(C) + ASIL C(C) | No decomposition |
| ASIL B | ASIL A(B) + ASIL A(B) | Valid decomposition |
| ASIL B | ASIL B(B) + ASIL B(B) | No decomposition |

The notation "ASIL X(Y)" means: element developed to ASIL X, decomposed from original ASIL Y.

### 3.2 Independence Requirements

For decomposition to be valid, the two elements must demonstrate sufficient independence per ISO 26262-9, Clause 5.4:

1. **Independence of execution**: Each element must be capable of fulfilling its safety function without depending on the other element's correct operation.
2. **Freedom from common cause failures (CCF)**: The elements must not share failure causes that could simultaneously violate both redundant safety requirements.
3. **Freedom from cascading failures (CF)**: A failure of one element must not cause a failure of the other element.

Independence must be demonstrated through analysis (DFA) considering all coupling factors:
- Shared development process / development team
- Shared technology / silicon vendor
- Shared manufacturing process
- Shared operational environment / location
- Shared power supply
- Shared communication bus
- Shared software platform / compiler / libraries

### 3.3 Decomposition Constraints

When decomposition is applied, the decomposed ASIL in parentheses (the "initial ASIL") determines:
- The rigor of the independence analysis
- The confirmation measures (review, audit, assessment)
- The required level of verification independence

For ASIL D decomposed to ASIL B(D) + ASIL B(D): both elements are developed to ASIL B requirements, but the independence proof must meet ASIL D rigor.

## 4. Decomposition Decision: None Applied

<!-- DECISION: ADR-009 — No ASIL decomposition applied in Taktflow Zonal Vehicle Platform -->

### 4.1 Decision Statement

**The Taktflow Zonal Vehicle Platform does NOT apply ASIL decomposition to any safety goal or safety requirement.**

All safety-critical elements are developed to their full assigned ASIL level as derived from the HARA. The system achieves ASIL D compliance through architectural diversity (TMS570 Safety Controller monitoring STM32 zone ECUs), not through ASIL decomposition.

### 4.2 Rationale

| Factor | Analysis |
|--------|----------|
| **Architectural capability** | The platform already includes an independent, diverse Safety Controller (TMS570) that inherently provides ASIL D hardware capability through lockstep cores (TUV-certified safety manual). This eliminates the primary motivation for decomposition -- reducing development cost on individual elements. |
| **Independence already exists** | The CVC/FZC/RZC (STM32, ST) and SC (TMS570, TI) use different silicon vendors, different CPU architectures, different compilers, different software stacks, and independent power regulation. The independence arguments required for decomposition are already satisfied by the system architecture. Applying decomposition would add documentation burden without architectural benefit. |
| **Development rigor unchanged** | Even with decomposition to ASIL B(D) + ASIL B(D), the independence proof must meet ASIL D rigor (ISO 26262-9, Clause 5.4.3). The verification and assessment effort for the independence proof can equal or exceed the effort saved by developing one element to a lower ASIL. |
| **Portfolio demonstration value** | For a portfolio project demonstrating ISO 26262 competence, achieving ASIL D through full development rigor is more impressive and instructive than achieving it through decomposition. Assessors reviewing this project will see both the full ASIL D safety mechanisms AND the complete independence analysis. |
| **Simplicity and auditability** | Not applying decomposition simplifies the traceability chain. Every safety requirement traces to its original ASIL without the added complexity of decomposition tags, initial ASIL tracking, and dual-element verification. |
| **Safety Controller software scope** | The TMS570 Safety Controller firmware is approximately 400 lines of bare-metal C code. The cost of developing this to full ASIL C (its allocated level for SG-008) is low. There is no economic incentive to decompose SG-008 to reduce the SC's development rigor. |

### 4.3 Consequence of This Decision

Because decomposition is not applied:

1. **CVC firmware** (SG-001, SG-003, SG-004): Developed to ASIL D for all safety-related software components (pedal plausibility, torque request, vehicle state machine).
2. **FZC firmware** (SG-003, SG-004): Developed to ASIL D for steering control and brake control software components.
3. **RZC firmware** (SG-001 via torque output, SG-006): Developed to ASIL D for motor control (SG-001 path) and ASIL A for motor protection (SG-006).
4. **SC firmware** (SG-008): Developed to ASIL C (highest ASIL from SG-008 source hazardous events).
5. **Hardware metrics**: Must meet ASIL D targets at system level for SG-001, SG-003, SG-004 (demonstrated in HW-METRICS document Section 7).
6. **Verification**: ASIL D verification methods (MC/DC coverage, requirements-based testing, fault injection) apply to all ASIL D allocated elements.
7. **Independence of assessment**: ASIL D requires I3 (fully independent) assessment per ISO 26262-2.

## 5. Where Decomposition Could Apply (Analysis for Completeness)

This section analyzes each ASIL D safety goal to identify where decomposition could theoretically be applied. This analysis serves two purposes: (1) demonstrating competence with the decomposition concept, and (2) providing a ready-made analysis if future revisions decide to apply decomposition.

### 5.1 SG-001: Prevent Unintended Acceleration (ASIL D)

**Current architecture**: Dual pedal sensors read by CVC (SM-001) + SC cross-plausibility (SM-003) + SC kill relay (SM-005).

**Potential decomposition**: ASIL B(D) + ASIL B(D)

| Element | Decomposed ASIL | Safety Function | ECU |
|---------|-----------------|-----------------|-----|
| Element A | ASIL B(D) | CVC pedal plausibility: detect pedal sensor disagreement, set torque to zero | CVC (STM32G474RE) |
| Element B | ASIL B(D) | SC cross-plausibility: compare torque request vs. actual motor current, open kill relay on violation | SC (TMS570LC43x) |

**Independence analysis (if decomposition were applied)**:

| Coupling Factor | Element A (CVC) | Element B (SC) | Independent? | Evidence |
|-----------------|-----------------|----------------|-------------|----------|
| Silicon vendor | STMicroelectronics | Texas Instruments | Yes | Different semiconductor manufacturers, different wafer fabs, different design teams |
| CPU architecture | ARM Cortex-M4F | ARM Cortex-R5F (lockstep) | Partial | Both are ARM cores, but: M4F is single-core Harvard architecture, R5F is dual-lockstep with hardware comparator. Different instruction pipelines, different safety features. Risk: both use ARM instruction set. Mitigation: lockstep on SC provides hardware redundancy absent on CVC. |
| Compiler / toolchain | GCC ARM (open-source) | TI ARM CGT (proprietary) | Yes | Different compiler vendors, different optimization strategies, different code generation |
| Software stack | AUTOSAR-like BSW (RTE, Com, Dcm, Dem, WdgM, BswM) | Minimal bare-metal (no OS, no BSW, ~400 LOC) | Yes | Fundamentally different software architectures. No shared source code, no shared libraries. |
| Development process | Same developer (portfolio project) | Same developer | No | Single developer for both elements. Mitigation: different coding patterns, different review checklists for each ECU, static analysis with different tools (PC-lint for STM32, TI tools for TMS570). |
| CAN bus | Shared physical CAN bus | Shared physical CAN bus | Partial | Shared medium, but: CVC transmits and receives, SC only listens (listen-only mode, cannot corrupt bus). E2E protection prevents message corruption. DFA analyzes CAN bus as coupling factor. |
| Power supply | 3.3V from Nucleo onboard regulator | 3.3V from LaunchPad onboard regulator | Yes | Independent voltage regulation. CVC powered from 12V rail through Nucleo buck converter; SC powered from 12V through LaunchPad regulator. Independent decoupling. |
| Physical location | Same bench, < 50 cm apart | Same bench, < 50 cm apart | No | Indoor bench environment eliminates most environmental CCF (no temperature gradient, no vibration, no EMI). Residual risk: bench power supply failure affects both. Mitigation: independent regulators, SC has independent power path from kill relay output to SC power input. |
| Clock source | Internal HSE (STM32) | Internal crystal (TMS570) | Yes | Independent oscillators. Different crystal specifications. |

**Independence verdict**: Sufficient independence exists for ASIL B(D) + ASIL B(D) decomposition, contingent on:
- Development process independence mitigated by different tools and review checklists
- CAN bus coupling mitigated by listen-only mode and E2E protection
- Physical proximity mitigated by benign indoor environment

**Decision**: NOT decomposed. Both elements already exist at full capability. No benefit to reducing development rigor.

### 5.2 SG-003: Prevent Unintended Steering (ASIL D)

**Current architecture**: FZC steering angle feedback (SM-008) + FZC rate/angle limiting (SM-009, SM-010) + CVC command validation + SC kill relay (SM-005).

**Potential decomposition**: ASIL B(D) + ASIL B(D)

| Element | Decomposed ASIL | Safety Function | ECU |
|---------|-----------------|-----------------|-----|
| Element A | ASIL B(D) | FZC steering control with angle feedback monitoring: detect servo runaway, rate limit, return to center | FZC (STM32G474RE) |
| Element B | ASIL B(D) | CVC command validation: validate steering commands before transmission; SC monitoring via kill relay | CVC + SC |

**Independence analysis (if decomposition were applied)**:

| Coupling Factor | Element A (FZC) | Element B (CVC + SC) | Independent? |
|-----------------|----------------|---------------------|-------------|
| Silicon vendor | ST STM32G474RE | ST STM32G474RE (CVC) + TI TMS570 (SC) | Partial: FZC and CVC are same vendor/MCU. SC provides vendor diversity. |
| Compiler | GCC ARM | GCC ARM (CVC) + TI CGT (SC) | Partial: FZC and CVC use same compiler. SC uses different. |
| Software stack | AUTOSAR BSW | AUTOSAR BSW (CVC) + bare-metal (SC) | Partial: FZC and CVC use same BSW. SC is independent. |
| CAN bus | Shared | Shared | Partial: all on same bus. E2E protection. SC listen-only. |
| Power supply | Independent regulator | Independent regulators | Yes |

**Independence verdict**: Marginal independence between FZC and CVC due to same MCU, compiler, and BSW. The SC provides the diversity necessary for the independence argument.

**Decision**: NOT decomposed. FZC implements the complete ASIL D steering safety chain. The SC kill relay provides an independent backup that does not require decomposition to justify.

### 5.3 SG-004: Prevent Loss of Braking (ASIL D)

**Current architecture**: FZC brake command monitoring (SM-011) + FZC auto-brake on CAN timeout (SM-012) + CVC motor cutoff command (SM-013) + SC kill relay (SM-005).

**Potential decomposition**: ASIL B(D) + ASIL B(D)

| Element | Decomposed ASIL | Safety Function | ECU |
|---------|-----------------|-----------------|-----|
| Element A | ASIL B(D) | FZC brake control with servo feedback: detect brake failure, auto-brake on CAN timeout | FZC (STM32G474RE) |
| Element B | ASIL B(D) | Motor cutoff as backup deceleration: CVC commands RZC to zero torque, SC kill relay removes motor power | CVC + RZC + SC |

**Independence analysis (if decomposition were applied)**:

| Coupling Factor | Element A (FZC) | Element B (CVC + RZC + SC) | Independent? |
|-----------------|----------------|---------------------------|-------------|
| Physical actuator path | Brake servo (mechanical) | Motor power removal (electrical) | Yes | Fundamentally different deceleration mechanisms: Element A uses brake servo mechanical action; Element B removes drive torque via electrical cutoff. Different actuators, different physical principles. |
| CAN bus | Shared | Shared | Partial: FZC auto-brake is autonomous (no CAN dependency once activated). Motor cutoff requires CAN (CVC to RZC) but SC relay is independent. |
| Power supply | Independent | Independent | Yes |

**Independence verdict**: Strong independence. The brake servo path and motor cutoff path use completely different actuators and physical principles. Even simultaneous failure of both would require an electromechanical brake servo failure AND an electrical motor driver failure -- independent failure modes.

**Decision**: NOT decomposed. The two independent deceleration paths already exist. Decomposition would not change the architecture, only the ASIL labeling of each path.

### 5.4 SG-008: Ensure Independent Safety Monitoring (ASIL C)

**Current architecture**: SC heartbeat monitoring (SM-019) + SC lockstep (SM-021) + external watchdogs (SM-020) + kill relay (SM-005).

**Potential decomposition**: ASIL B(C) + ASIL A(C) or ASIL A(C) + ASIL A(C)

This safety goal is allocated entirely to the SC, which is a single element. Decomposition would require splitting the monitoring function across two independent elements. Possible candidates:
- Element A: SC heartbeat monitoring and kill relay (ASIL B(C))
- Element B: CVC self-monitoring with E-stop override (ASIL A(C))

**Decision**: NOT decomposed. The SC is a single, highly capable element (TMS570 lockstep). Its firmware is ~400 LOC. The cost of developing it to full ASIL C is trivial. Decomposing would require proving independence between SC and CVC monitoring functions, which adds complexity without benefit.

## 6. Independence Arguments Between System Elements

Even though ASIL decomposition is not applied, documenting the independence between system elements is valuable for:
- Supporting the dependent failure analysis (DFA)
- Demonstrating that the architectural diversity achieves its intended safety benefit
- Providing evidence for the safety case that the SC is truly an independent monitoring channel
- Assessor readiness

### 6.1 Independence Matrix

| Coupling Factor | CVC <-> FZC | CVC <-> RZC | CVC <-> SC | FZC <-> RZC | FZC <-> SC | RZC <-> SC |
|-----------------|-------------|-------------|------------|-------------|------------|------------|
| Silicon vendor | Same (ST) | Same (ST) | **Different** (ST vs TI) | Same (ST) | **Different** (ST vs TI) | **Different** (ST vs TI) |
| CPU architecture | Same (M4F) | Same (M4F) | **Different** (M4F vs R5F lockstep) | Same (M4F) | **Different** (M4F vs R5F lockstep) | **Different** (M4F vs R5F lockstep) |
| Compiler | Same (GCC ARM) | Same (GCC ARM) | **Different** (GCC vs TI CGT) | Same (GCC ARM) | **Different** (GCC vs TI CGT) | **Different** (GCC vs TI CGT) |
| SW architecture | Same (BSW) | Same (BSW) | **Different** (BSW vs bare-metal) | Same (BSW) | **Different** (BSW vs bare-metal) | **Different** (BSW vs bare-metal) |
| CAN bus | Shared | Shared | **Asymmetric** (SC listen-only) | Shared | **Asymmetric** (SC listen-only) | **Asymmetric** (SC listen-only) |
| Power supply (3.3V) | Independent regulators | Independent regulators | **Independent** regulators | Independent regulators | **Independent** regulators | **Independent** regulators |
| Clock source | Independent | Independent | **Independent** | Independent | **Independent** | **Independent** |
| Physical location | Same bench | Same bench | Same bench | Same bench | Same bench | Same bench |
| Developer | Same | Same | Same | Same | Same | Same |

### 6.2 Detailed Independence Arguments

#### 6.2.1 Silicon Vendor Diversity

The zone ECUs use STMicroelectronics STM32G474RE. The Safety Controller uses Texas Instruments TMS570LC43x.

- ST and TI have independent design teams, independent wafer fabrication facilities, independent quality processes, and independent silicon IP.
- A systematic design defect in the STM32G474RE (e.g., errata affecting FDCAN peripheral, ALU computation error under specific conditions) would NOT affect the TMS570LC43x.
- A manufacturing defect batch at ST (e.g., contaminated wafer lot) would NOT affect TI production.
- This provides robust protection against common cause failures at the silicon level.

**Evidence**: Different part numbers from different manufacturers. Different ARM core families (Cortex-M4F vs. Cortex-R5F). Different peripheral IP blocks (FDCAN vs. DCAN).

#### 6.2.2 Toolchain Diversity

| Property | Zone ECUs | Safety Controller |
|----------|-----------|-------------------|
| Compiler | arm-none-eabi-gcc (GNU) | TI ARM Code Generation Tools (CL470) |
| Linker | GNU ld | TI linker |
| Libraries | newlib-nano | TI runtime support library |
| Static analysis | PC-lint, cppcheck | TI Code Composer Studio analysis |
| Debug probe | ST-LINK | XDS110 |
| HAL generation | STM32CubeMX | HALCoGen |

A compiler bug in GCC ARM (e.g., incorrect optimization at -O2 producing wrong branch instructions) would NOT affect TI ARM CGT compilation of SC firmware. Conversely, a known HALCoGen bug (e.g., DCAN4 mailbox bug in HALCoGen v04.07.01) does NOT affect STM32CubeMX-generated code.

**Evidence**: Different compiler vendors. Different optimization engines. Different code generation strategies.

#### 6.2.3 Software Architecture Diversity

| Property | Zone ECUs (CVC/FZC/RZC) | Safety Controller (SC) |
|----------|--------------------------|----------------------|
| Software stack | AUTOSAR-like BSW (MCAL, CanIf, PduR, Com, Dcm, Dem, WdgM, BswM, RTE) | Minimal bare-metal (no OS, no middleware, no BSW) |
| Task scheduling | Cooperative/preemptive RTOS (via RTE) | Simple main loop with RTI interrupt |
| CAN stack | CanIf -> PduR -> Com -> E2E | Direct DCAN register access |
| Error handling | Dem DTC management | Simple state variable + LED output |
| Lines of code (est.) | ~5,000-10,000 per ECU | ~400 |
| Configuration | BSW configuration parameters | Compile-time constants |
| Memory allocation | Static (no malloc) | Static (no malloc) |

The SC firmware is intentionally minimal to reduce the probability of software faults. A bug in the BSW Com module (e.g., incorrect PDU routing) would NOT affect the SC because the SC does not use the BSW stack. A stack overflow in the RTOS cannot occur on the SC because the SC does not use an RTOS.

**Evidence**: Fundamentally different software architectures. No shared source code. No shared library code.

#### 6.2.4 CAN Bus Independence (Asymmetric Coupling)

The CAN bus is a shared physical medium but the coupling is asymmetric:

| Property | Zone ECUs | Safety Controller |
|----------|-----------|-------------------|
| CAN mode | Normal (TX + RX) | **Listen-only** (RX only) |
| Can transmit? | Yes | **No** |
| Can corrupt bus? | Yes (bus-off possible) | **No** (listen-only prevents TX, ACK, error frames) |
| Effect of bus failure | Loses communication | Loses input data; detects via heartbeat timeout -> opens kill relay |

The SC cannot electrically corrupt the CAN bus because the DCAN controller is configured in listen-only (silent) mode via the TEST register bit 3 (SPNU609). In this mode:
- The SC does NOT send acknowledge bits (other nodes see the message as acknowledged by zone ECUs)
- The SC does NOT send error frames
- The SC does NOT participate in bus arbitration
- The SC CAN transceiver TX pin is held recessive

This means a fault in the SC's CAN controller cannot cause a bus-off condition or message corruption on the shared CAN bus. The coupling is one-directional: bus -> SC only.

**If the CAN bus fails entirely** (wire break, short to ground, short to Vbatt):
- Zone ECUs lose inter-ECU communication -> E2E timeouts trigger safe states
- SC loses heartbeat input -> heartbeat timeout triggers kill relay
- Both paths independently lead to a safe state

**Evidence**: DCAN silent mode configuration (documented in TMS570 Safety Manual SPNU609). SC cannot transmit.

#### 6.2.5 Power Supply Independence

| ECU | 3.3V Source | 12V Path |
|-----|------------|----------|
| CVC | Nucleo-64 onboard LD1117S33 regulator | Direct from bench supply |
| FZC | Nucleo-64 onboard LD1117S33 regulator | Direct from bench supply |
| RZC | Nucleo-64 onboard LD1117S33 regulator | Direct from bench supply |
| SC | LaunchPad onboard TPS7A7001 regulator | Direct from bench supply |

Each ECU has its own independent voltage regulator with independent decoupling capacitors. A regulator failure on one ECU does not propagate to other ECUs.

**Shared 12V bench supply**: This is a common coupling factor. If the 12V supply fails:
- All ECUs lose power
- Kill relay de-energizes (energize-to-run: fail-safe)
- All actuators lose power (motor, servos)
- System enters the safest possible state (no power = no motion)

A 12V supply failure is therefore a safe failure. The coupling factor does not violate any safety goal because loss of power leads to the safe state for all safety goals.

**Evidence**: Independent regulators on each Nucleo/LaunchPad board. 12V failure is inherently safe due to energize-to-run relay design.

#### 6.2.6 Development Process Independence

This is the weakest independence factor. The entire platform is developed by a single developer as a portfolio project. A systematic cognitive error (e.g., misunderstanding a safety requirement) could affect both zone ECU and SC firmware.

**Mitigations**:
- Different design patterns: zone ECUs use AUTOSAR BSW patterns (layered architecture, PDU routing, service-based communication); SC uses simple bare-metal patterns (register-level programming, direct CAN mailbox access).
- Different review checklists: zone ECU code is reviewed against MISRA C, AUTOSAR coding guidelines, and BSW architecture rules; SC code is reviewed against TI-specific safety patterns and lockstep constraints.
- Static analysis with different tools: PC-lint and cppcheck for STM32; TI Code Composer Studio analysis for TMS570.
- SC specification is simple: "monitor heartbeats, compare torque vs. current, control relay." The simplicity of the SC specification (compared to the multi-function zone ECUs) reduces the probability of a specification error propagating to both.

**Residual risk**: Developer cognitive error could produce the same logical mistake in both zone ECU and SC firmware. This is partially mitigated by the fundamental architectural difference (complex BSW vs. minimal bare-metal) making identical implementation mistakes unlikely. For a production system, independent development teams would be required (ISO 26262-9, Clause 7, Annex D).

**Evidence**: Different development tools, different design patterns, simple SC specification. Residual risk documented and accepted for portfolio context.

### 6.3 Independence Summary

| Coupling Factor | Independence Level | Rating | Notes |
|-----------------|-------------------|--------|-------|
| Silicon vendor | Full | Strong | Different manufacturers (ST vs. TI) |
| CPU architecture | High | Strong | Different core families (M4F vs. R5F lockstep) |
| Compiler/toolchain | Full | Strong | Different compiler vendors (GCC vs. TI CGT) |
| Software architecture | Full | Strong | BSW vs. bare-metal, no shared code |
| CAN bus | Asymmetric (one-way) | Adequate | SC listen-only; cannot corrupt bus |
| Power supply (3.3V) | Full | Strong | Independent regulators per ECU |
| Power supply (12V) | Shared | Adequate | Failure is inherently safe (energize-to-run) |
| Physical environment | Shared | Adequate | Benign indoor environment; no thermal/vibration/EMI stress |
| Clock source | Full | Strong | Independent oscillators |
| Development process | Shared | Weak | Single developer; mitigated by different tools and patterns |

**Overall independence assessment**: The independence between zone ECUs (ST) and Safety Controller (TI) is **sufficient** for an ASIL D safety argument through architectural diversity. The dominant coupling factor is the shared development process, which is a known limitation documented in the safety case. For a production program, independent development teams would be required.

## 7. Common Cause Failure (CCF) Analysis Summary

This section summarizes the CCF considerations relevant to decomposition decisions. Full CCF analysis is in the DFA document (DFA v0.1).

### 7.1 CCF Scenarios Considered

| CCF Scenario | Coupling Factor | Safety Goals Affected | Mitigated? | Mitigation |
|-------------|-----------------|----------------------|-----------|------------|
| Simultaneous dual pedal sensor failure | Shared SPI bus, shared magnetic field, shared power | SG-001 | Yes | SC cross-plausibility (SM-003) detects incorrect torque independent of pedal readings; separate CS lines isolate SPI faults; sensor range check detects power rail corruption |
| CAN bus total failure | Shared physical medium | SG-001 to SG-008 | Yes | SC detects heartbeat loss, opens kill relay (SM-005); FZC auto-brake on CAN timeout (SM-012); each ECU has autonomous safe state on CAN loss |
| 12V power supply failure | Shared bench supply | All SGs | Yes (inherently safe) | Kill relay de-energizes; all actuators lose power; system is in safest state |
| 3.3V regulator failure on single ECU | Independent, but same regulator part number across Nucleo boards | Depends on which ECU | Yes | TPS3823 detects MCU loss; SC detects heartbeat loss; other ECUs unaffected (independent regulators) |
| Software defect in BSW stack | Shared BSW code across CVC/FZC/RZC | SG-001 to SG-007 | Partial | SC does not use BSW (diverse architecture); SC detects behavioral anomalies through cross-plausibility. Residual: subtle BSW bug producing incorrect but plausible behavior may not be detected by SC. |
| Compiler bug affecting all STM32 ECUs | Same GCC ARM compiler | SG-001 to SG-007 | Partial | SC uses different compiler (TI CGT). Residual: GCC bug would affect CVC+FZC+RZC but not SC. SC can detect and respond to zone ECU misbehavior. |
| Temperature excursion affecting all ECUs | Same bench location | All SGs | Yes | Indoor environment; temperature range +15 to +35 degC is well within component ratings (-40 to +85 degC for automotive). |

### 7.2 CCF and Decomposition Interaction

If decomposition were applied, each CCF scenario above would need to be explicitly evaluated for the decomposed element pair. The analysis above demonstrates that the existing architectural diversity (ST vs. TI, BSW vs. bare-metal, normal CAN vs. listen-only CAN) provides sufficient CCF protection for all ASIL D safety goals.

The fact that CCF protection already exists at the architectural level is a further reason that formal ASIL decomposition is unnecessary: the system already implements the diversity required for decomposition without explicitly invoking the decomposition mechanism.

## 8. Constraints if Decomposition Were Applied in Future

If a future revision of this platform decides to apply ASIL decomposition, the following constraints MUST be satisfied:

### 8.1 General Constraints

1. **Decomposition scheme per ISO 26262-9, Table 2**: Only the schemes listed in Section 3.1 are permitted.
2. **Independence proof per ISO 26262-9, Clause 5.4**: A formal independence analysis must demonstrate sufficient independence between the decomposed elements, addressing all coupling factors in Section 6.1.
3. **DFA update required**: The DFA document must be updated to explicitly analyze CCF and cascading failures for each decomposed element pair.
4. **Both elements must meet their decomposed ASIL**: If SG-001 is decomposed to ASIL B(D) + ASIL B(D), both Element A and Element B must independently achieve ASIL B compliance for both hardware and software.
5. **Initial ASIL determines rigor**: The initial ASIL (D in parentheses) determines the rigor of the independence analysis, confirmation measures, and safety assessment. Decomposing to ASIL B(D) does NOT reduce the assessment rigor from ASIL D.
6. **Traceability update**: All safety requirements must be updated with decomposition tags. The traceability matrix must show which element implements which decomposed requirement.
7. **Hardware metrics per element**: SPFM, LFM, and PMHF must be recalculated per decomposed element to verify that each element meets its decomposed ASIL targets independently.

### 8.2 Element-Specific Constraints

**If CVC pedal monitoring is decomposed from SG-001**:
- CVC must still detect pedal sensor disagreement and command zero torque (ASIL B(D) minimum)
- SC must still perform cross-plausibility and operate the kill relay (ASIL B(D) minimum)
- CVC and SC must demonstrate independence per Section 6.2
- CVC development may use ASIL B methods (e.g., statement coverage instead of MC/DC for unit testing) but independence proof remains at ASIL D rigor

**If FZC steering control is decomposed from SG-003**:
- FZC steering path must still implement feedback monitoring, rate limiting, and return-to-center (ASIL B(D))
- CVC + SC must provide independent command validation and kill relay (ASIL B(D))
- Independence between FZC and CVC is limited (same MCU, same compiler, same BSW) -- SC provides the diversity
- A three-element decomposition may be needed: FZC steering + CVC validation + SC monitoring

**If FZC braking control is decomposed from SG-004**:
- FZC brake path must still implement brake monitoring and auto-brake (ASIL B(D))
- Motor cutoff path (CVC + RZC) must provide backup deceleration (ASIL B(D))
- Strong independence exists between brake servo path and motor cutoff path (different actuators, different physical principles)

## 9. Traceability

### 9.1 Safety Goal to Decomposition Decision

| SG-ID | ASIL | Decomposition Applied? | Rationale |
|-------|------|----------------------|-----------|
| SG-001 | D | No | ASIL D achieved through architectural diversity (CVC + SC) |
| SG-002 | B | No | Single-element implementation adequate for ASIL B |
| SG-003 | D | No | ASIL D achieved through FZC feedback + SC kill relay |
| SG-004 | D | No | ASIL D achieved through dual deceleration paths (brake + motor cutoff) |
| SG-005 | A | No | ASIL A requires no decomposition |
| SG-006 | A | No | ASIL A requires no decomposition |
| SG-007 | C | No | Single-element implementation with SC backup adequate for ASIL C |
| SG-008 | C | No | SC with lockstep hardware provides ASIL C capability directly |

### 9.2 Traceability to Related Documents

| This Document Section | Traces To |
|----------------------|-----------|
| Section 4 (decomposition decision) | Safety goals (SG), system architecture (ITEM-DEF Section 3) |
| Section 5 (decomposition analysis) | Functional Safety Concept (FSC, safety mechanism allocation) |
| Section 6 (independence arguments) | Dependent Failure Analysis (DFA), Hardware Metrics (HW-METRICS Section 9) |
| Section 7 (CCF summary) | DFA (full CCF analysis) |
| Section 8 (constraints) | Safety Plan (SP), Technical Safety Requirements (TSR) |

### 9.3 Upstream and Downstream Traceability

```
HARA (hazardous events + ASIL assignment)
    |
    v
Safety Goals (SG-001 to SG-008 with ASIL D/C/B/A)
    |
    v
ASIL Decomposition Analysis (THIS DOCUMENT)
    Decision: No decomposition applied
    |
    v
Functional Safety Requirements (FSR) — at full original ASIL
    |
    v
Technical Safety Requirements (TSR) — at full original ASIL
    |
    +---> Hardware Safety Requirements (HSR) — at full original ASIL
    |
    +---> Software Safety Requirements (SSR) — at full original ASIL
```

## 10. Assessor Notes

This section provides notes for an independent safety assessor reviewing this document.

1. **Decomposition is intentionally not applied.** The system achieves ASIL D through architectural diversity (diverse redundancy with TMS570 Safety Controller) rather than through ASIL decomposition. This is a valid approach per ISO 26262-9, Clause 5.1 Note 1: "ASIL decomposition is an optional method."

2. **Independence analysis is provided for reference.** Section 6 documents the independence between system elements even though decomposition is not formally invoked. This analysis also supports the DFA and the safety case argument for the SC as an independent monitoring channel.

3. **The weakest independence factor is the shared development process.** For a production program, the assessor would require independent development teams for the zone ECUs and the Safety Controller. For this portfolio project, the single-developer limitation is documented and mitigated through different tools, design patterns, and architectural simplicity of the SC.

4. **Hardware metrics compliance.** The HW-METRICS document demonstrates that system-level SPFM and PMHF meet ASIL D targets for all applicable safety goals. Element-level metrics for the STM32-based ECUs do not individually meet ASIL D targets, but the system-level analysis including the TMS570 Safety Controller achieves compliance. This architectural approach is consistent with ISO 26262-5, Clause 8.4.3 Note 2.

5. **The SC firmware scope is approximately 400 lines of bare-metal C.** The simplicity of the SC firmware reduces the probability of software systematic faults. Combined with the TMS570 lockstep hardware, the SC provides a high-integrity independent monitoring channel without requiring formal ASIL decomposition.

## 11. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete ASIL decomposition analysis: no decomposition decision with full rationale, decomposition analysis for all ASIL D safety goals, independence arguments for all element pairs, CCF summary, constraints for future decomposition, assessor notes |

