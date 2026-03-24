# 10-Role BMS Gap Analysis — taktflow-embedded-production

**Date**: 2026-03-23
**Scope**: Full project audit from 10 BMS development perspectives
**Method**: Each role reviews the project independently, identifies gaps against industry expectations

---

## Role 1: Functional Safety Engineer (ISO 26262)

**What's strong:**
- ASIL D architecture with dual-core lockstep SC (TMS570) — textbook
- Complete ISO 26262 Part 3 concept phase: item definition, HARA, safety goals, FSC
- FMEA, DFA, ASIL decomposition, FTTI budget — all present
- E2E protection (AUTOSAR P01) on all 32 CAN messages
- 11 ASIL D/C integration test suites covering critical chains

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| S1 | Safety documents are "planned" status — not formally reviewed/approved | HIGH | Item definition, HARA, safety goals, FSC, safety plan, safety case all show status "planned". No evidence of independent review or sign-off. ISO 26262 Part 2 requires confirmation measures (reviews, audits) with documented evidence. |
| S2 | No Fault Tree Analysis (FTA) | MEDIUM | FMEA (bottom-up) exists but FTA (top-down) is missing. ISO 26262-9 recommends both for ASIL D. FTA from each safety goal → root causes provides complementary coverage. |
| S3 | No Common Cause Analysis (CCA) beyond DFA | MEDIUM | DFA covers dependent failures but systematic CCA per ISO 26262-9 Table 1 (common-cause initiators, coupling factors) not documented. |
| S4 | Hardware metrics (SPFM/LFM) may be placeholder | MEDIUM | File exists but needs verification against actual component FIT rates. ASIL D requires SPFM ≥ 99%, LFM ≥ 90%. |
| S5 | No confirmed safety assessor/auditor | HIGH | Safety plan should name the independent safety assessor. No confirmation review evidence. ISO 26262-2 Clause 6.4.6 requires functional safety audit. |
| S6 | Safety validation report is "planned" | HIGH | No compiled evidence that safety goals are met at vehicle level. SIL/HIL results exist but aren't formally compiled into the validation argument. |
| S7 | No proven-in-use argument for COTS components | LOW | ThreadX (vendor lib) and HALCoGen wrappers lack proven-in-use documentation per ISO 26262-8 Clause 12. |

---

## Role 2: BMS Algorithm Engineer

**What's strong:**
- Battery voltage monitoring chain tested (6-hop SIL test)
- Overtemperature fault chains with dual-NTC cross-check
- VSM fault transitions tested
- Derating chain integration test (DEM event → derating)

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| A1 | No SoC (State of Charge) estimation algorithm | CRITICAL | No Coulomb counting, EKF, or ML-based SoC estimator in firmware. A BMS without SoC is incomplete. |
| A2 | No SoH (State of Health) estimation | CRITICAL | No capacity fade tracking, impedance trending, or cycle counting. |
| A3 | No cell balancing logic (passive or active) | CRITICAL | No balancing algorithm, no balancing hardware driver, no balancing strategy. |
| A4 | No thermal management control algorithm | HIGH | Overtemp detection exists (fault path) but no active thermal management: no fan/pump control, no pre-conditioning, no thermal model. |
| A5 | No charge control algorithm | HIGH | No CC/CV charging state machine, no charge current limiting, no charge termination logic. |
| A6 | No current measurement / Coulomb integration | HIGH | ADC driver exists but no shunt/Hall sensor integration for pack current measurement. |
| A7 | No OCV-SoC lookup table or characterization data | MEDIUM | No battery cell characterization data (OCV curves, Ri vs SoC, temperature derating curves). |
| A8 | No cell voltage monitoring (individual cell level) | HIGH | Signals reference "pack voltage" but no individual cell voltage ADC (no AFE driver like LTC6813/BQ76952). |

---

## Role 3: AUTOSAR Software Architect

**What's strong:**
- Clean BSW layering: MCAL → ECUAL → Services → RTE → SWC
- 16 AUTOSAR services implemented (Com, Dcm, Dem, E2E, WdgM, etc.)
- DBC → ARXML → C code generation pipeline
- Platform abstraction across STM32, TMS570, POSIX

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| AR1 | No BswM mode management for BMS states | HIGH | BswM exists but no BMS-specific mode definitions (STANDBY, PRECHARGE, DRIVE, CHARGE, FAULT, STORAGE). |
| AR2 | No NvM persistent storage for BMS data | HIGH | NvM module exists but no BMS-specific blocks defined (SoC at shutdown, fault history, cell characterization, cycle count). |
| AR3 | No Xcp calibration for BMS parameters | MEDIUM | Xcp module exists but no measurement/calibration events defined for BMS tuning parameters. |
| AR4 | ARXML model incomplete | MEDIUM | arxml_v2 is "next-generation" — implies current model not fully validated. 92 connectors mapped but "not yet injected" per memory. |
| AR5 | No SWC interface definitions for BMS functions | HIGH | No Rte port definitions for BMS SWCs (SoC estimator, balancing controller, thermal manager, charge controller). |
| AR6 | No AUTOSAR Diagnostic Event configuration for BMS DTCs | MEDIUM | Dem exists but no BMS-specific DTC definitions (cell overvoltage, undervoltage, overtemperature, SoC plausibility, sensor fault). |

---

## Role 4: Hardware Engineer (BMS)

**What's strong:**
- TMS570 lockstep dual-core for safety controller — excellent choice
- STM32F407 for zone controllers — adequate
- Pin maps directory exists (even if empty)
- Offline datasheets organized with markdown breakdowns

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| H1 | No BMS-specific hardware design | CRITICAL | No schematic, no PCB layout, no BOM for the actual BMS board. hardware/ directories are empty (.gitkeep only). |
| H2 | No Analog Front-End (AFE) driver | CRITICAL | No driver for cell monitoring IC (LTC6813, BQ76952, MAX17853, or similar). This is the core BMS measurement hardware. |
| H3 | No pre-charge circuit design / driver | HIGH | No pre-charge relay control, no pre-charge monitoring, no pre-charge timeout logic. |
| H4 | No contactor/relay driver | HIGH | No main contactor control (positive, negative, pre-charge), no welding detection, no coil diagnostics. |
| H5 | No current sensor interface | HIGH | No shunt amplifier or Hall sensor driver (INA240, LEM DHAB, etc.). |
| H6 | No isolation monitoring | HIGH | No isolation resistance measurement (IMD) — required for HV BMS per UN R100. |
| H7 | No HSI (Hardware-Software Interface) for BMS components | HIGH | HSI spec exists but likely doesn't cover AFE SPI protocol, contactor drive timing, pre-charge sequencing. |
| H8 | No EMC/ESD design considerations documented | MEDIUM | No EMC test plan, no ESD protection strategy for HV interfaces. |

---

## Role 5: Test Engineer (Verification & Validation)

**What's strong:**
- Complete xIL chain: unit → integration → SIL → PIL → HIL
- 7 CI pipelines (3 blocking gates)
- 11 ASIL D/C integration test suites
- Automated traceability validation
- SIL live demo with all 7 ECUs
- verdict_checker.py (~1000 assertions)

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| T1 | No BMS-specific test cases | HIGH | Tests cover vehicle-level faults (overtemp, busoff, heartbeat) but no BMS unit tests: SoC accuracy, balancing convergence, charge termination, cell drift detection. |
| T2 | No code coverage metrics published | MEDIUM | Makefile.coverage exists but no coverage reports in CI artifacts. ISO 26262 Part 6 requires statement + branch coverage for ASIL D (100% statement, MC/DC for ASIL D). |
| T3 | No back-to-back testing (MIL vs SIL) | MEDIUM | MIL directory exists but no evidence of model-vs-code comparison. ISO 26262-6 Table 7 recommends back-to-back testing. |
| T4 | No fault injection testing framework | HIGH | Integration tests check fault paths but no systematic fault injection: bit-flip, stuck-at, communication loss, sensor drift. ASIL D requires fault injection per ISO 26262-6 Table 10. |
| T5 | No environmental stress testing | MEDIUM | No thermal cycling, no vibration profile, no humidity test specifications. |
| T6 | No regression test baseline / golden results | LOW | Tests pass/fail but no golden reference data for regression detection of algorithm accuracy drift. |
| T7 | No test case review / test basis traceability for BMS | MEDIUM | Traceability exists for vehicle-level but BMS-specific requirements → test cases not linked. |

---

## Role 6: Systems Engineer

**What's strong:**
- System architecture documented (ASPICE SYS.3)
- Stakeholder requirements → system requirements → SW requirements chain
- 7-ECU topology well-defined
- CAN matrix as single source of truth

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| SE1 | System is a vehicle platform, not a BMS system | HIGH | The architecture is a zone-controller vehicle platform. BMS is one subsystem but has no dedicated system-level treatment (no BMS item definition, no BMS-specific HARA). |
| SE2 | No BMS system requirements specification | HIGH | System requirements cover vehicle functions. No dedicated BMS SRS covering: voltage range, current range, temperature range, cell count, pack topology, communication protocols. |
| SE3 | No BMS interface control document (ICD) | MEDIUM | How does the BMS communicate with VCU/charger/thermal system? No ICD for BMS ↔ external systems. |
| SE4 | No power budget analysis | MEDIUM | No sleep current, active current, or power consumption analysis for the BMS ECU. |
| SE5 | No BMS performance requirements | HIGH | No quantified requirements: SoC accuracy ±X%, measurement rate, balancing current, response time to fault. |
| SE6 | No regulatory compliance mapping (UN R100, IEC 62660, GB/T) | MEDIUM | Safety is ISO 26262 but BMS-specific regulations not addressed. |

---

## Role 7: Production / Manufacturing Engineer

**What's strong:**
- Automated code generation eliminates manual config errors
- CI/CD pipeline ensures build reproducibility
- Docker-based SIL for factory end-of-line testing potential

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| P1 | No production test specification | HIGH | No end-of-line (EOL) test procedure: what to test, pass/fail criteria, test equipment list. |
| P2 | No flash/programming procedure for production | MEDIUM | Flash commands exist for development (DSLite, STM32_Programmer_CLI) but no production-grade flashing specification. |
| P3 | No hardware test points / DFT considerations | MEDIUM | No Design-for-Test: test pads, JTAG access, measurement points documented. |
| P4 | No serial number / traceability scheme | MEDIUM | No ECU serial number, no firmware version reporting via UDS (ReadDID F189/F191/F195). |
| P5 | No calibration procedure for sensors | HIGH | No procedure for ADC calibration, current sensor offset, temperature sensor characterization. |
| P6 | No variant management | LOW | Single variant assumed. No mechanism for different cell chemistries, pack sizes, or OEM configurations. |

---

## Role 8: Cybersecurity Engineer (ISO/SAE 21434)

**What's strong:**
- Web app has security hardening (Phases 0-9 complete)
- E2E protection on CAN messages (integrity)
- UDS access via diagnostic sessions

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| C1 | No TARA (Threat Analysis and Risk Assessment) for BMS | HIGH | ISO/SAE 21434 requires TARA for cybersecurity-relevant items. BMS is safety-critical and connected. |
| C2 | No SecOC (Secure Onboard Communication) | HIGH | E2E protects against random HW faults but not malicious CAN injection. SecOC with CMAC required for ASIL D + cybersecurity. |
| C3 | No secure boot for ECUs | HIGH | No verified boot chain. Firmware can be replaced without authentication. |
| C4 | No UDS security access (0x27/0x29) | MEDIUM | Diagnostic services exist but no authentication before write/control operations. |
| C5 | No key management / HSM usage | HIGH | TMS570 has crypto accelerator but no key storage, no secure key provisioning. |
| C6 | No firmware update security (signed OTA) | MEDIUM | No secure firmware update mechanism. |
| C7 | No intrusion detection on CAN bus | LOW | No anomaly detection for unexpected CAN IDs or message timing deviations. |

---

## Role 9: Project Manager (ASPICE)

**What's strong:**
- 70 ASPICE documents covering SYS/SWE/HWE/MAN/SUP
- Execution roadmap, weekly status, risk register, decision log
- Gate readiness checklist exists
- 57 documents in INDEX.md
- Traceability automation in CI

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| PM1 | Single-person project — no peer reviews | HIGH | ISO 26262 and ASPICE both require independence in reviews. All artifacts created by one person. No evidence of independent review. |
| PM2 | No baselined releases | MEDIUM | Documents are "planned" status. No formal baseline (version-controlled snapshots approved for a milestone). |
| PM3 | No change management process evidence | MEDIUM | Decision log exists but no formal Change Request → Impact Analysis → Approval → Implementation flow. |
| PM4 | No supplier management (SUP.10) | LOW | Third-party components (ThreadX, HALCoGen) have no incoming quality checks or supplier qualification. |
| PM5 | No resource plan / effort estimation | LOW | Roadmap exists but no effort/resource allocation (acceptable for solo project, gap for team scaling). |
| PM6 | HWE.1 and HWE.2 are draft/planned | MEDIUM | Hardware engineering processes not yet executed — blocks ASPICE Level 2 for HWE. |
| PM7 | No lessons-learned retrospective documented | LOW | Lessons-learned files exist in web/UI but no formal BMS/embedded retrospective linked from ASPICE. |

---

## Role 10: Battery Cell Specialist / Electrochemist

**What's strong:**
- Overtemperature protection with dual-NTC cross-check shows awareness of thermal safety
- Derating chain shows understanding of graceful degradation

**Gaps identified:**

| # | Gap | Severity | Detail |
|---|-----|----------|--------|
| B1 | No cell model (equivalent circuit or electrochemical) | CRITICAL | No Thevenin/Randles model, no diffusion dynamics. SoC/SoH estimation requires a cell model. |
| B2 | No cell characterization data | CRITICAL | No OCV-SoC curves, no impedance spectra, no capacity vs temperature data, no aging model. |
| B3 | No cell chemistry specification | HIGH | Which chemistry? NMC, LFP, NCA? Voltage limits, C-rate limits, temperature limits all depend on chemistry. |
| B4 | No abuse protection logic | HIGH | No nail penetration response, no overcharge protection beyond voltage limit, no vent detection, no thermal runaway propagation model. |
| B5 | No calendar/cycle aging model | MEDIUM | No degradation prediction for warranty/lifetime estimation. |
| B6 | No cell-to-pack thermal model | MEDIUM | No understanding of temperature gradients within the pack. Dual-NTC monitors but doesn't model heat distribution. |
| B7 | No self-discharge detection | LOW | No algorithm to detect cells with abnormal self-discharge (internal short circuit precursor). |

---

## Gap Summary Matrix

| Role | Critical | High | Medium | Low | Total |
|------|----------|------|--------|-----|-------|
| 1. Functional Safety | 0 | 3 | 3 | 1 | 7 |
| 2. BMS Algorithm | 3 | 3 | 1 | 0 | **8** |
| 3. AUTOSAR Architect | 0 | 3 | 3 | 0 | 6 |
| 4. Hardware Engineer | 2 | 4 | 1 | 0 | **8** |
| 5. Test Engineer | 0 | 2 | 4 | 1 | 7 |
| 6. Systems Engineer | 0 | 3 | 3 | 0 | 6 |
| 7. Manufacturing | 0 | 2 | 3 | 1 | 6 |
| 8. Cybersecurity | 0 | 4 | 2 | 1 | 7 |
| 9. Project Manager | 0 | 1 | 3 | 3 | 7 |
| 10. Electrochemist | 2 | 2 | 2 | 1 | **7** |
| **TOTAL** | **7** | **27** | **25** | **8** | **69** |

---

## Key Insight

**The project is an excellent vehicle-level embedded platform** with strong safety, AUTOSAR, and process discipline. However, **it is not yet a BMS** — the BMS-specific domain logic (cell monitoring, SoC/SoH, balancing, charge control, thermal management) is almost entirely missing. The platform is the foundation; the BMS application layer needs to be built on top.

---

# Mitigation Plan

## Phase 0: Foundation (Week 1-2) — Systems + Safety Alignment

**Goal**: Establish BMS as a first-class subsystem within the existing vehicle platform.

### 0.1 BMS Item Definition & HARA
- **Gap**: SE1, S1
- **Action**: Write `docs/safety/concept/bms-item-definition.md` — define BMS boundaries, interfaces, operating modes
- **Action**: Extend HARA with BMS-specific hazards: thermal runaway, overcharge, deep discharge, contactor welding, isolation loss
- **Deliverable**: BMS safety goals with ASIL assignment
- **Owner**: Safety Engineer + Systems Engineer

### 0.2 BMS System Requirements Specification
- **Gap**: SE2, SE5, B3
- **Action**: Write `docs/aspice/system/bms-system-requirements.md`
  - Cell chemistry selection (NMC/LFP) with voltage/current/temperature limits
  - Pack topology (series × parallel count)
  - SoC accuracy requirement (±2% for NMC, ±5% for LFP)
  - Measurement rates (cell voltage 10ms, temperature 100ms, current 1ms)
  - Balancing current specification
  - Response time to fault (< FTTI budget)
- **Deliverable**: Baselined BMS SRS with traceability to stakeholder requirements
- **Owner**: Systems Engineer + Electrochemist

### 0.3 Regulatory Mapping
- **Gap**: SE6
- **Action**: Create `docs/safety/compliance/regulatory-mapping.md` — map UN R100, IEC 62660, ECE R136 requirements to BMS functions
- **Deliverable**: Compliance matrix
- **Owner**: Systems Engineer

---

## Phase 1: Hardware & Cell Foundation (Week 2-4)

**Goal**: Select BMS hardware, define cell model, create AFE driver.

### 1.1 BMS Hardware Design
- **Gap**: H1, H2, H3, H4, H5, H6
- **Action**: Select AFE IC (recommendation: **BQ76952** for ≤16S, **LTC6813** for >16S)
- **Action**: Design BMS schematic with:
  - AFE + cell connections
  - Pre-charge circuit (relay + resistor + timeout monitoring)
  - Main contactors (positive + negative + pre-charge relay)
  - Current sensor (shunt + INA240 or Hall sensor)
  - Isolation monitor (Bender ISOMETER or Lem ISO165C)
  - MCU (STM32 or dedicated BMS MCU)
- **Action**: Populate `hardware/schematics/`, `hardware/bom/`, `hardware/pinmaps/`
- **Deliverable**: BMS schematic, BOM, HSI specification update
- **Owner**: Hardware Engineer

### 1.2 Cell Characterization Data
- **Gap**: B1, B2, B3, B7
- **Action**: Source cell datasheet OR use published characterization data for chosen chemistry
- **Action**: Create `firmware/ecu/bms/data/`:
  - `ocv_soc_table.c` — OCV vs SoC lookup (25°C reference + temperature compensation)
  - `ri_soc_table.c` — Internal resistance vs SoC and temperature
  - `capacity_temp_table.c` — Usable capacity vs temperature
- **Action**: Implement Thevenin equivalent circuit model in `firmware/ecu/bms/src/bms_cell_model.c`
- **Deliverable**: Cell model validated against datasheet
- **Owner**: Electrochemist + Algorithm Engineer

### 1.3 AFE Driver (MCAL Layer)
- **Gap**: H2, A8
- **Action**: Implement `firmware/bsw/mcal/Afe/` — SPI driver for chosen AFE
  - Cell voltage reading (all cells, < 2ms conversion)
  - Temperature reading (NTC channels)
  - Balancing switch control
  - Fault register reading (OV/UV/OT/UT hardware comparators)
  - Communication CRC verification
- **Action**: Write unit tests in `test/unit/bsw/mcal/test_afe_driver.c`
- **Deliverable**: AFE driver with 100% statement coverage
- **Owner**: Hardware Engineer + AUTOSAR Architect

---

## Phase 2: Core BMS Algorithms (Week 4-8)

**Goal**: Implement SoC, SoH, balancing, and charge control.

### 2.1 SoC Estimation
- **Gap**: A1, A6
- **Action**: Implement in `firmware/ecu/bms/src/bms_soc.c`:
  - **Coulomb counting** (primary, 1ms integration)
  - **OCV correction** at rest (secondary, when pack current < threshold for > 30min)
  - **Extended Kalman Filter** (optional, for production-grade accuracy)
  - NvM storage of SoC at shutdown (NvM block definition)
- **Action**: Define Rte ports: `Rte_Read_BmsSoc_Percent`, `Rte_Read_BmsPackCurrent_mA`
- **Test**: SoC accuracy ±2% over drive cycle (SIL with recorded drive profile)
- **Owner**: Algorithm Engineer

### 2.2 SoH Estimation
- **Gap**: A2, B5
- **Action**: Implement in `firmware/ecu/bms/src/bms_soh.c`:
  - **Capacity tracking**: full-charge to full-discharge Coulomb integration (opportunistic)
  - **Impedance trending**: R0 estimation from voltage step response
  - **Cycle counter**: full-equivalent cycles (NvM persistent)
- **Test**: SoH tracks capacity fade within ±5%
- **Owner**: Algorithm Engineer + Electrochemist

### 2.3 Cell Balancing
- **Gap**: A3
- **Action**: Implement in `firmware/ecu/bms/src/bms_balancing.c`:
  - **Passive balancing** (discharge through AFE balance resistors)
  - Strategy: balance to minimum cell voltage, hysteresis to prevent oscillation
  - Thermal derating: reduce balancing current when cell temperature > threshold
  - NvM: store imbalance history for SoH correlation
- **Test**: Balancing converges within spec (simulated 10mV spread → <5mV in N cycles)
- **Owner**: Algorithm Engineer

### 2.4 Charge Control
- **Gap**: A5
- **Action**: Implement in `firmware/ecu/bms/src/bms_charge.c`:
  - CC/CV state machine: IDLE → PRE-CHARGE → CC → CV → TAPER → DONE
  - Current limit based on: SoC, temperature, cell voltage, SoH
  - Charge termination: taper current threshold + voltage hold time
  - Communication with charger via CAN (charge current request, voltage limit)
- **Test**: Full charge cycle SIL test with simulated charger
- **Owner**: Algorithm Engineer

### 2.5 Thermal Management
- **Gap**: A4
- **Action**: Implement in `firmware/ecu/bms/src/bms_thermal.c`:
  - Temperature monitoring (min/max/avg across all NTCs)
  - Fan/pump control output (PWM duty cycle vs temperature delta)
  - Pre-conditioning request (heat before charge in cold weather)
  - Thermal derating curves (power limit vs temperature)
- **Test**: Thermal control response time and stability
- **Owner**: Algorithm Engineer

---

## Phase 3: AUTOSAR Integration (Week 6-8, overlaps Phase 2)

**Goal**: Integrate BMS SWCs into the AUTOSAR architecture.

### 3.1 BMS SWC Port Definitions
- **Gap**: AR5, AR1
- **Action**: Add BMS ports to RTE code generation:
  - `Rte_Read_BmsCellVoltage_mV[N]`, `Rte_Read_BmsPackCurrent_mA`
  - `Rte_Write_BmsSoc_Percent`, `Rte_Write_BmsSoh_Percent`
  - `Rte_Write_BmsChargeRequest_A`, `Rte_Write_BmsContactorCmd`
  - `Rte_Write_BmsBalancingActive`, `Rte_Write_BmsFaultStatus`
- **Action**: Define BswM modes: `BMS_STANDBY`, `BMS_PRECHARGE`, `BMS_DRIVE`, `BMS_CHARGE`, `BMS_FAULT`, `BMS_STORAGE`
- **Owner**: AUTOSAR Architect

### 3.2 BMS DTC Definitions
- **Gap**: AR6
- **Action**: Add to Dem configuration:
  - `DTC_BMS_CellOvervoltage` (0xC10100)
  - `DTC_BMS_CellUndervoltage` (0xC10200)
  - `DTC_BMS_Overtemperature` (0xC10300)
  - `DTC_BMS_Undertemperature` (0xC10400)
  - `DTC_BMS_Overcurrent` (0xC10500)
  - `DTC_BMS_IsolationFault` (0xC10600)
  - `DTC_BMS_SocPlausibility` (0xC10700)
  - `DTC_BMS_AfeCommunication` (0xC10800)
  - `DTC_BMS_ContactorWelding` (0xC10900)
  - `DTC_BMS_PrechargeTimeout` (0xC10A00)
- **Owner**: AUTOSAR Architect

### 3.3 DBC Extension for BMS
- **Gap**: SE3
- **Action**: Add BMS messages to `gateway/taktflow_vehicle.dbc`:
  - `BMS_CellVoltages1` (0x620) — cells 1-4
  - `BMS_CellVoltages2` (0x621) — cells 5-8 (etc.)
  - `BMS_PackStatus` (0x630) — SoC, SoH, pack voltage, pack current
  - `BMS_Temperatures` (0x640) — NTC readings
  - `BMS_ContactorStatus` (0x650) — contactor states, pre-charge status
  - `BMS_ChargeControl` (0x660) — charge request to charger
  - `BMS_FaultStatus` (0x670) — active DTCs summary
- **Action**: Re-run DBC → ARXML → C codegen pipeline
- **Owner**: AUTOSAR Architect + Systems Engineer

### 3.4 NvM Blocks for BMS
- **Gap**: AR2
- **Action**: Define NvM blocks:
  - `NvM_BmsSocAtShutdown` (4 bytes, CRC-protected)
  - `NvM_BmsCycleCount` (4 bytes)
  - `NvM_BmsFaultHistory` (64 bytes, ring buffer)
  - `NvM_BmsCellCharacterization` (variable, per-cell offsets)
- **Owner**: AUTOSAR Architect

---

## Phase 4: Safety Hardening (Week 8-10)

**Goal**: Close all functional safety gaps for BMS.

### 4.1 BMS Fault Tree Analysis
- **Gap**: S2, S3
- **Action**: Create `docs/safety/analysis/bms-fta.md`:
  - Top event: "Thermal runaway" → AFE failure AND monitoring failure AND...
  - Top event: "Uncontrolled discharge" → contactor welding AND backup failure
  - Top event: "Overcharge" → charge control failure AND cell OV detection failure
- **Action**: Extend CCA with BMS-specific common causes (connector corrosion, vibration, thermal cycling)
- **Owner**: Safety Engineer

### 4.2 BMS Safety Mechanisms
- **Gap**: B4
- **Action**: Implement in `firmware/ecu/bms/src/bms_safety.c`:
  - AFE hardware comparator as independent OV/UV/OT check (redundant path)
  - Pack fuse as last-resort protection (hardware, document in safety case)
  - Contactor welding detection (voltage check after open command)
  - Pre-charge timeout (< FTTI, independent timer)
  - Current sensor plausibility (compare AFE current sense vs external shunt)
- **Test**: Fault injection test for each safety mechanism
- **Owner**: Safety Engineer + Algorithm Engineer

### 4.3 Formal Safety Document Review
- **Gap**: S1, S5, S6, PM1
- **Action**: Conduct tabletop review of all safety documents — simulate independent review
- **Action**: Update status from "planned" → "reviewed" with review date and findings
- **Action**: Compile safety validation report linking SIL/HIL evidence to safety goals
- **Deliverable**: Safety case updated with BMS-specific evidence
- **Owner**: Safety Engineer (with peer reviewer if available)

### 4.4 Code Coverage to ASIL D
- **Gap**: T2
- **Action**: Enable gcov/lcov in CI, publish coverage reports
- **Target**: 100% statement, 100% branch, MC/DC for ASIL D safety-critical functions
- **Owner**: Test Engineer

---

## Phase 5: Cybersecurity (Week 10-11)

**Goal**: Address automotive cybersecurity gaps.

### 5.1 Threat Analysis (TARA)
- **Gap**: C1
- **Action**: Create `docs/security/tara-bms.md` — identify attack surfaces (CAN, diagnostic, OTA, physical)
- **Owner**: Cybersecurity Engineer

### 5.2 SecOC Implementation
- **Gap**: C2
- **Action**: Add CMAC authentication to safety-critical BMS CAN messages
- **Action**: Implement freshness value management
- **Owner**: Cybersecurity Engineer + AUTOSAR Architect

### 5.3 Secure Boot + UDS Authentication
- **Gap**: C3, C4
- **Action**: Enable TMS570 secure boot (eFuse-based)
- **Action**: Implement UDS SecurityAccess (0x27) with seed-key for write services
- **Owner**: Cybersecurity Engineer

---

## Phase 6: Production Readiness (Week 11-13)

**Goal**: Prepare for manufacturing.

### 6.1 EOL Test Specification
- **Gap**: P1, P3, P5
- **Action**: Write `docs/production/eol-test-spec.md`:
  - Cell voltage measurement accuracy check
  - Current sensor calibration procedure
  - Contactor operation test
  - Isolation resistance measurement
  - CAN communication verify
  - Flash firmware + verify
  - UDS ReadDID for serial number / SW version
- **Owner**: Manufacturing Engineer + Test Engineer

### 6.2 UDS Identity DIDs
- **Gap**: P4
- **Action**: Implement ReadDID for F189 (SW version), F191 (HW version), F195 (serial number)
- **Owner**: AUTOSAR Architect

### 6.3 Variant Management
- **Gap**: P6
- **Action**: Define `firmware/ecu/bms/cfg/bms_variant.h` — compile-time selection of cell count, chemistry, pack topology
- **Owner**: AUTOSAR Architect

---

## Phase 7: Verification & Validation (Week 12-14, overlaps Phase 6)

**Goal**: Prove the BMS works.

### 7.1 BMS SIL Test Suite
- **Gap**: T1
- **Action**: Create `test/sil/test_bms_soc.py` — drive cycle SoC accuracy
- **Action**: Create `test/sil/test_bms_balancing.py` — balancing convergence
- **Action**: Create `test/sil/test_bms_charge.py` — full charge cycle
- **Action**: Create `test/sil/test_bms_fault.py` — all BMS fault paths → safe state
- **Owner**: Test Engineer

### 7.2 Fault Injection Testing
- **Gap**: T4
- **Action**: Create `test/framework/test_int_bms_fault_injection_asild.c`:
  - AFE SPI communication loss → fallback to hardware comparators
  - Current sensor stuck-at → SoC freeze + fault
  - Cell voltage sensor drift → plausibility check triggers DTC
  - Contactor command disagree → emergency open
- **Owner**: Test Engineer + Safety Engineer

### 7.3 Back-to-Back MIL/SIL
- **Gap**: T3
- **Action**: Create MATLAB/Python cell model → compare SoC output with firmware SoC estimator
- **Acceptance**: ±1% SoC difference over standard drive cycle
- **Owner**: Test Engineer + Algorithm Engineer

---

## Timeline Summary

```
Week  1-2:  Phase 0 — Systems alignment, BMS requirements, safety goals
Week  2-4:  Phase 1 — Hardware design, cell data, AFE driver
Week  4-8:  Phase 2 — Core algorithms (SoC, SoH, balancing, charge, thermal)
Week  6-8:  Phase 3 — AUTOSAR integration (parallel with Phase 2)
Week  8-10: Phase 4 — Safety hardening (FTA, safety mechanisms, reviews)
Week 10-11: Phase 5 — Cybersecurity (TARA, SecOC, secure boot)
Week 11-13: Phase 6 — Production readiness (EOL, identity, variants)
Week 12-14: Phase 7 — V&V (SIL tests, fault injection, back-to-back)
```

## Priority Order (if time-constrained)

1. **Phase 0** — Without requirements, everything else is guesswork
2. **Phase 2.1** (SoC) — The single most important BMS function
3. **Phase 1.3** (AFE driver) — Can't measure without hardware abstraction
4. **Phase 3.3** (DBC) — Enables all CAN communication for BMS
5. **Phase 4.2** (Safety mechanisms) — Required for ASIL claim
6. **Phase 2.3** (Balancing) — Second most important BMS function
7. Everything else follows naturally

---

*Generated: 2026-03-23 | 69 gaps identified across 10 BMS roles*
