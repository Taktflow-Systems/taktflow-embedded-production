# ASIL D Full Reference Tables (ISO 26262)

This file contains the full reference tables moved from `.claude/rules/asil-d-*.md` during context window optimization. The actionable constraints remain in `.claude/rules/asil-d.md`.

---

## Software Development (Part 6)

### Software Architectural Design Principles (Section 7)

| Principle | ASIL D |
|-----------|--------|
| Hierarchical structure of software components | ++ |
| Restricted size of software components | ++ |
| Restricted size of interfaces | ++ |
| High cohesion within each software component | ++ |
| Restricted coupling between software components | ++ |
| Appropriate spatial isolation (MPU/MMU) | ++ |
| Appropriate management of shared resources | ++ |

### Software Unit Design Principles (Section 8)

| Principle | ASIL D |
|-----------|--------|
| Enforcement of low complexity | ++ |
| Use of language subsets (MISRA C) | ++ |
| Enforcement of strong typing | ++ |
| Use of defensive implementation techniques | ++ |
| Use of established design principles | ++ |
| Use of unambiguous graphical representation | ++ |
| Use of style guides | ++ |
| Use of naming conventions | ++ |

### Code Coverage Requirements (Table 9)

| Coverage Metric | ASIL A | ASIL B | ASIL C | ASIL D |
|-----------------|--------|--------|--------|--------|
| Statement coverage | ++ | ++ | ++ | ++ |
| Branch coverage | + | ++ | ++ | ++ |
| MC/DC coverage | o | + | ++ | ++ |

### Static Analysis Requirements

| Method | ASIL D |
|--------|--------|
| Static code analysis (MISRA compliance) | ++ |
| Control flow analysis | ++ |
| Data flow analysis | ++ |
| Complexity metrics (cyclomatic complexity) | ++ |
| Stack usage analysis | ++ |

### Unit Testing Methods (Table 7)

| Method | ASIL A | ASIL B | ASIL C | ASIL D |
|--------|--------|--------|--------|--------|
| Requirements-based testing | ++ | ++ | ++ | ++ |
| Interface testing | + | ++ | ++ | ++ |
| Fault injection testing | + | + | ++ | ++ |
| Resource usage testing | + | + | + | ++ |
| Back-to-back testing | o | + | + | ++ |

### Test Case Derivation (Table 8)

| Method | ASIL A | ASIL B | ASIL C | ASIL D |
|--------|--------|--------|--------|--------|
| Analysis of requirements | ++ | ++ | ++ | ++ |
| Equivalence classes | ++ | ++ | ++ | ++ |
| Boundary value analysis | ++ | ++ | ++ | ++ |
| Error guessing / fault seeding | + | + | + | ++ |

---

## Hardware Metrics (Part 5)

### Architectural Metrics by ASIL Level

| Metric | ASIL B | ASIL C | ASIL D |
|--------|--------|--------|--------|
| SPFM (Single Point Fault Metric) | >= 90% | >= 97% | >= 99% |
| LFM (Latent Fault Metric) | >= 60% | >= 80% | >= 90% |
| PMHF (Probabilistic Metric for HW Failure) | < 100 FIT | < 100 FIT | < 10 FIT |

FIT = Failures In Time = 1 failure per 10^9 device-hours

### Diagnostic Coverage Levels

| DC Level | Coverage Range |
|----------|---------------|
| Low | 60% <= DC < 90% |
| Medium | 90% <= DC < 99% |
| High | DC >= 99% (expected at ASIL D) |

### Fault Classification

| Category | Symbol | Description |
|----------|--------|-------------|
| Safe fault | lambda_S | Does not contribute to safety goal violation |
| Single-point fault | lambda_SPF | Directly violates safety goal, no safety mechanism |
| Residual fault | lambda_RF | Part of fault not covered by safety mechanism |
| Detected multi-point | lambda_MPF_det | Multi-point fault detected by safety mechanism |
| Perceived multi-point | lambda_MPF_per | Multi-point fault perceived by driver |
| Latent multi-point | lambda_MPF_lat | Neither detected nor perceived |

### Redundancy Architectures

| Architecture | Description | Use Case |
|-------------|-------------|----------|
| 1oo1D | Single channel + diagnostics | Lower ASIL; ASIL D only with very high DC |
| 1oo2 | Two independent channels, either sufficient | ASIL D fail-safe systems |
| 1oo2D | Two channels + diagnostics each | ASIL D standard architecture |
| 2oo3 | Three channels, majority voting | ASIL D fail-operational |

### Safety Mechanism Details

#### Processor Monitoring
- Lockstep CPU (dual-core running same code, outputs compared)
- OR: independent monitoring processor (checker)
- Program counter monitoring
- Instruction execution checking

#### Memory Protection
- ECC on all safety-critical RAM
- ECC or CRC on flash/ROM
- Memory BIST at startup
- Periodic memory integrity checks
- MPU for partition isolation

#### Communication Monitoring
- CRC on all bus communications
- Message alive counters / sequence numbers
- Timeout monitoring
- Bus-off detection and recovery

#### Power Supply Monitoring
- Voltage monitoring with independent comparator
- Brown-out detection and reset
- Over-voltage protection
- Power supply sequencing verification

#### Clock Monitoring
- Independent clock source for watchdog
- Clock frequency cross-check
- Clock loss detection

#### ADC/Sensor Monitoring
- ADC self-test (known reference voltage)
- Sensor plausibility checking
- Open/short circuit detection

### FMEA / FMEDA Requirements
- FMEA (qualitative): failure modes, effects, causes for every component
- FMEDA (quantitative): classify failure modes, assign rates, calculate SPFM/LFM/PMHF
- Failure rates from: IEC TR 62380, SN 29500, MIL-HDBK-217, or field data
- Update FMEDA when design changes

### Common Cause Failure (CCF) Factors
- Shared power supply
- Physical proximity (temperature, vibration, EMC)
- Common design methodology
- Shared manufacturing process
- Shared software/firmware
- Environmental stress (ESD, humidity)

Mitigations: diverse hardware, physical separation, independent power supplies, environmental shielding.

### Hardware-Software Interface (HSI) Specification

| Item | Description |
|------|-------------|
| RAM mapping | Safety-critical vs non-safety memory regions |
| Non-volatile memory | Flash/EEPROM layout, wear leveling |
| Bus interfaces | CAN, LIN, SPI, I2C — speeds, protocols, error handling |
| ADC/DAC | Channels, resolution, reference voltage, sampling rates |
| PWM | Channels, frequency, duty cycle ranges |
| GPIO | Pin assignments, directions, pull-up/pull-down, interrupt config |
| Interrupts | Assignments, priorities, nesting rules |
| Clock sources | Main, RTC, PLL configuration |
| Diagnostic resources | Watchdog, ECC, MPU, temperature sensor |
| Power modes | Sleep states, wake sources, transition timing |

---

## Architecture (Part 4)

### FFI Demonstration Methods
1. Hardware separation (strongest): Separate microcontrollers per ASIL level
2. Hardware partitioning: MPU/MMU enforced memory regions on same MCU
3. Temporal partitioning: Safety-qualified RTOS with time-partitioned scheduling
4. Monitoring/validation: Higher-ASIL validates ALL lower-ASIL data
5. Static analysis: Prove no unprotected control/data flow from low to high ASIL
6. Fault injection testing: Inject faults in low-ASIL, verify high-ASIL unaffected

### E2E Communication Fault Coverage

| Fault Type | Detection Method |
|-----------|------------------|
| Repetition | Sequence counter |
| Loss | Sequence counter + timeout |
| Delay | Timeout monitoring |
| Insertion | Data ID + CRC |
| Masquerade | Data ID + CRC |
| Corruption | CRC (8/32/64-bit) |
| Asymmetric info | CRC + sequence counter |

### System Design Verification Methods

| Method | ASIL D |
|--------|--------|
| Walk-through | o |
| Inspection | ++ |
| Semi-formal verification | ++ |
| Formal verification | ++ |
| Simulation/prototyping | ++ |

### Fail-Safe vs Fail-Operational

| Pattern | Architecture | When |
|---------|-------------|------|
| Fail-Safe | 1oo2D minimum | "Off" is acceptable safe state |
| Fail-Operational | 2oo3 minimum | "Off" is NOT safe (steer/brake-by-wire, L3+) |

---

## Verification (Parts 4, 6, 8)

### Verification Methods by ASIL Level

#### Software Safety Requirements Verification

| Method | ASIL A | ASIL B | ASIL C | ASIL D |
|--------|--------|--------|--------|--------|
| Walk-through | ++ | + | + | + |
| Inspection | + | ++ | ++ | ++ |
| Semi-formal verification | o | + | ++ | ++ |
| Formal verification | o | o | + | ++ |
| Prototyping / simulation | o | + | + | ++ |

#### Software Architecture Verification

| Method | ASIL A | ASIL B | ASIL C | ASIL D |
|--------|--------|--------|--------|--------|
| Walk-through | ++ | + | o | o |
| Inspection | + | ++ | ++ | ++ |
| Semi-formal verification | o | + | ++ | ++ |
| Formal verification | o | o | + | ++ |
| Simulation | o | + | ++ | ++ |

#### Software Unit Design Verification

| Method | ASIL A | ASIL B | ASIL C | ASIL D |
|--------|--------|--------|--------|--------|
| Walk-through | ++ | + | o | o |
| Inspection | + | ++ | ++ | ++ |
| Semi-formal verification | o | + | + | ++ |
| Formal verification | o | o | + | ++ |
| Static code analysis | + | + | ++ | ++ |

### Independence Requirements

| Level | Meaning |
|-------|---------|
| I0 | Should be performed; different person than creator |
| I1 | Shall be performed; different person |
| I2 | Shall be performed; independent from team |
| I3 | Shall be performed; independent from department (external org) |

#### Independence by Activity at ASIL D

| Activity | ASIL D |
|----------|--------|
| Confirmation review of safety plan | I3 (external org) |
| Confirmation review of safety case | I3 (external org) |
| Confirmation review of HARA | I2 (independent from team) |
| Functional safety audit | I2 (independent from team) |
| Functional safety assessment | I3 (external org: TUV, SGS, exida) |

---

## ASIL Decomposition (Part 9)

### Full Decomposition Table

| Original ASIL | Option 1 | Option 2 | Option 3 |
|---------------|----------|----------|----------|
| ASIL D | D(D) + QM(D) | C(D) + A(D) | B(D) + B(D) |
| ASIL C | C(C) + QM(C) | B(C) + A(C) | -- |
| ASIL B | B(B) + QM(B) | A(B) + A(B) | -- |
| ASIL A | A(A) + QM(A) | -- | -- |

Notation: `B(D)` = developed to ASIL B processes, original requirement was ASIL D.

### Decomposition Documentation Requirements

| Document | Content |
|----------|---------|
| Decomposition rationale | Why decomposition is needed and appropriate |
| ASIL allocation | Which element gets which ASIL level |
| Independence argument | How spatial, temporal, communication independence is achieved |
| DFA report | Common cause failure and cascading failure analysis |
| FFI evidence | Test results, analysis, and architecture proof |
| Integration test plan | Testing at the original ASIL level |
