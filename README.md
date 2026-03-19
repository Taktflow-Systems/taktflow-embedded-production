# Taktflow Embedded Production

**ISO 26262 ASIL D zonal vehicle platform** — 7 ECUs, AUTOSAR-like BSW, CAN 500 kbit/s, E2E protection, full xIL verification.

> A personal portfolio project demonstrating automotive-grade embedded systems engineering: safety-critical firmware, automated code generation from AUTOSAR models, ASPICE-compliant process artifacts, and a complete V-model verification chain from unit tests through hardware-in-the-loop.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Layered Software Architecture](#layered-software-architecture)
3. [Code Generation Pipeline (DBC → ARXML → C)](#code-generation-pipeline-dbc--arxml--c)
4. [Automotive SPICE (ASPICE) Process Compliance](#automotive-spice-aspice-process-compliance)
5. [ISO 26262 — Functional Safety (ASIL D)](#iso-26262--functional-safety-asil-d)
6. [Automated Traceability](#automated-traceability)
7. [V-Model Verification (xIL Testing)](#v-model-verification-xil-testing)
8. [MISRA C:2012 Static Analysis](#misra-c2012-static-analysis)
9. [CI/CD Pipelines](#cicd-pipelines)
10. [Quick Start](#quick-start)
11. [Project Structure](#project-structure)
12. [Key Principles](#key-principles)
13. [Metrics](#metrics)

---

## Architecture Overview

```
                         ┌─── CLOUD ──────────────────────────────────┐
                         │ Grafana · AWS IoT · SAP QM · Godot 3D     │
                         └────────────────┬───────────────────────────┘
                                     MQTT (TLS)
                         ┌────────────────┴───────────────────────────┐
                         │           GATEWAY LAYER (Docker)           │
                         │  can_gateway · mqtt_bridge · ws_bridge     │
                         │  diagnostics (UDS) · ml_inference          │
                         │  plant_sim_py · fault_inject               │
                         └────────────────┬───────────────────────────┘
                              CAN 500 kbit/s · E2E P01 (CRC-8)
               ┌──────────────────┼──────────────────┐
               ▼                  ▼                   ▼
        ┌─ FRONT ZONE ──┐ ┌── CENTRAL ──┐  ┌── REAR ZONE ──┐
        │ FZC (ASIL D)   │ │ CVC (ASIL D) │  │ RZC (ASIL C)  │
        │ STM32F407      │ │ STM32F407    │  │ STM32F407      │
        │ Steer,Brake,   │ │ Vehicle,     │  │ Motor,Traction │
        │ Lidar          │ │ Heartbeat,   │  │ RearSensors    │
        │                │ │ Coordinator  │  │                │
        └────────────────┘ └──────┬───────┘  └────────────────┘
                                  │  heartbeat monitoring
                           ┌──────┴───────┐
                           │ SC (ASIL D)   │  ← TMS570 lockstep
                           │ Monitors all  │    Heartbeat miss
                           │ alive counters│    → SAFE STATE
                           └──────────────┘
        ┌─ BODY / COMFORT (Docker vECUs) ───────────────────┐
        │ BCM (QM)  ·  ICU (QM)  ·  TCU (ASIL B)           │
        │ C++ / CMake · POSIX MCAL · same BSW interfaces    │
        └───────────────────────────────────────────────────┘
```

**Data flow**: Sensors → SWC (via RTE) → Com (signal packing + E2E) → CAN bus → SC heartbeat monitoring → CAN gateway → MQTT → cloud services. Any heartbeat miss within FTTI budget → safe state (torque cutoff, steer neutral).

**Verification**: SIL (7 Docker vECUs, live at `sil.taktflow-systems.com`) · HIL (Raspberry Pi + physical CAN) · PIL (real MCU, simulated plant) · MIL (Python plant models) · Unit tests (Unity)

---

## Layered Software Architecture

The firmware follows the **AUTOSAR layered architecture** pattern, ensuring strict separation between application logic and hardware:

```
┌─────────────────────────────────────────────────────┐
│  Application Layer (SWCs)                           │
│  Swc_Steer, Swc_Brake, Swc_Motor, Swc_Heartbeat... │
├─────────────────────────────────────────────────────┤
│  RTE (Runtime Environment)                          │  ← Signal routing between SWCs
│  Rte_Read / Rte_Write / Rte_Call                    │    and BSW, all generated
├─────────────────────────────────────────────────────┤
│  Services Layer                                     │
│  Com · Dcm · Dem · E2E · WdgM · BswM · NvM · Det   │  ← AUTOSAR service modules
│  SchM · CanTp                                       │
├─────────────────────────────────────────────────────┤
│  ECU Abstraction Layer (ECUAL)                      │
│  CanIf · PduR · IoHwAb                              │  ← Hardware-independent interface
├─────────────────────────────────────────────────────┤
│  MCAL (Microcontroller Abstraction Layer)           │
│  Can · Spi · Adc · Pwm · Dio · Gpt · Uart          │  ← Register-level HW drivers
├─────────────────────────────────────────────────────┤
│  Platform / HAL                                     │
│  stm32f4/ · stm32l5/ · tms570/ · posix/            │  ← One implementation per target
└─────────────────────────────────────────────────────┘
```

### What each layer does

| Layer | Purpose | Why it matters |
|-------|---------|----------------|
| **Application (SWCs)** | Vehicle feature logic (steering, braking, motor control). Each SWC is a self-contained software component with defined ports. | Completely hardware-independent — the same SWC source compiles for STM32, TMS570, and POSIX (Docker). |
| **RTE** | Auto-generated routing layer. Connects SWC ports to BSW signals. `Rte_Read_<Port>_<Signal>()` / `Rte_Write_<Port>_<Signal>()`. | Decouples application from communication stack — SWCs never call `Com_SendSignal` directly. |
| **Services** | AUTOSAR standardized services: **Com** (signal packing/routing), **Dcm** (UDS diagnostics), **Dem** (fault memory), **E2E** (end-to-end CRC protection), **WdgM** (watchdog supervision), **BswM** (mode management), **NvM** (non-volatile storage), **Det** (development error tracing), **SchM** (scheduler), **CanTp** (CAN transport protocol). | Industry-standard interfaces — an engineer familiar with AUTOSAR can navigate this codebase immediately. |
| **ECUAL** | Abstraction over communication hardware: **CanIf** (CAN interface multiplexing), **PduR** (PDU router — routes messages between Com, Dcm, CanTp), **IoHwAb** (I/O hardware abstraction for sensors/actuators). | Allows swapping CAN controllers or adding new bus types without touching services or application code. |
| **MCAL** | Direct register-level drivers for each peripheral. One implementation per MCU family. | Isolates all vendor-specific code. Porting to a new MCU = writing a new MCAL, nothing else changes. |
| **Platform** | Build system, startup code, linker scripts, and HAL wrappers per target (STM32F4, STM32L5, TMS570, POSIX). | Enables the same codebase to run on physical hardware AND in Docker containers for SIL testing. |

### Safety Controller (SC) — Special Case

The TMS570 Safety Controller runs a **lockstep dual-core CPU** and intentionally does NOT use the full AUTOSAR BSW stack. It runs minimal, auditable code only: heartbeat monitoring, safe-state enforcement, and watchdog supervision. This follows ISO 26262 Part 6 recommendations for reduced complexity in ASIL D safety mechanisms.

---

## Code Generation Pipeline (DBC → ARXML → C)

The **single source of truth** for all CAN communication is `gateway/taktflow.dbc`. An automated pipeline generates all BSW configuration — no hand-editing of generated files, ever. This mirrors OEM toolchains (Vector DaVinci, EB tresos) using open Python tooling.

```
gateway/taktflow.dbc                 ← CAN message matrix (source of truth)
        │
        ▼  tools/arxml/dbc2arxml.py
arxml/TaktflowSystem.arxml           ← AUTOSAR model (R22-11, PDUs + signals + topology)
        │
        ▼  tools/arxmlgen/           (Python + Jinja2)
        ├──► Com_Cfg.c/.h             Signal packing tables (byte offsets, bit positions)
        ├──► Rte_Cfg.c/.h             Type-safe Rte_Read/Rte_Write wrappers
        ├──► CanIf_Cfg.c/.h           CAN ID → PDU handle routing
        ├──► PduR_Cfg.c/.h            PDU routing paths (fan-out to Com, Dcm, CanTp)
        ├──► E2E_Cfg.c/.h             E2E Profile P01 parameters (CRC offset, DataID)
        └──► Swc_*.c/.h               SWC skeletons (one-time, overwrite=false)
```

Eliminates CAN ID mismatches, signal bit-position errors, and E2E DataID inconsistencies by generating everything from one authoritative source.

---

## Automotive SPICE (ASPICE) Process Compliance

### What is ASPICE?

**Automotive SPICE** (ASPICE) is the process assessment model used by OEMs (BMW, VW, Daimler, etc.) to evaluate supplier software development maturity. It defines process areas across the V-model — from requirements elicitation through verification — and rates organizations from Level 0 (incomplete) to Level 5 (innovating). **Level 2** (managed) is the typical minimum for Tier-1 supplier qualification.

### ASPICE process areas implemented

This project implements artifacts for all major ASPICE process areas:

#### System Engineering (SYS)

| Process | ID | Document | What it does |
|---------|----|----------|--------------|
| **Requirements Elicitation** | SYS.1 | `docs/aspice/system/stakeholder-requirements.md` | Captures what stakeholders need — translated from use cases and constraints into formal requirements. Ensures nothing is built without a traceable "why." |
| **System Requirements Analysis** | SYS.2 | `docs/aspice/system/system-requirements.md` | Refines stakeholder needs into verifiable system-level requirements (e.g., "CAN heartbeat period shall be 100 ms ± 5%"). Each requirement has an ID, rationale, and verification method. |
| **System Architectural Design** | SYS.3 | `docs/aspice/system/system-architecture.md` | Defines the zonal ECU topology, communication buses, ASIL allocation per ECU, and interface contracts. Includes the CAN message matrix derived from the DBC. |
| **System Integration Test** | SYS.4 | `docs/aspice/verification/system-integration/` | Verifies that integrated ECUs work together — CAN message exchange, E2E protection, heartbeat supervision, safe-state transitions. Executed on the HIL bench. |
| **System Verification** | SYS.5 | `docs/aspice/verification/system-verification/` | Validates the complete system against system requirements. Full end-to-end scenarios (ignition → drive → fault → safe-state) on HIL. |

#### Software Engineering (SWE)

| Process | ID | Document | What it does |
|---------|----|----------|--------------|
| **SW Requirements** | SWE.1 | `docs/aspice/software/sw-requirements/SWR-*.md` | Per-ECU software requirements derived from system requirements. One SWR document per ECU (CVC, FZC, RZC, SC, BCM, ICU, TCU) plus one for the shared BSW stack. Each requirement is traceable upward to SYS.2 and downward to code. |
| **SW Architecture** | SWE.2 | `docs/aspice/software/sw-architecture/` | Three documents: overall SW architecture, BSW architecture (layered stack), and vECU architecture (Docker containerization). Defines module boundaries, interfaces, and data flows. |
| **SW Detailed Design & Implementation** | SWE.3 | `firmware/ecu/*/src/`, `firmware/bsw/` | The actual C source code. 662 source files implementing the AUTOSAR-like BSW stack and per-ECU application SWCs. |
| **SW Unit Verification** | SWE.4 | `test/unit/`, `docs/aspice/verification/unit-test/` | Unity-based unit tests for individual BSW modules and SWC functions. Tests are compiled and run natively (POSIX) in CI. |
| **SW Integration Test** | SWE.5 | `test/framework/src/`, `docs/aspice/verification/integration-test/` | 11 integration test suites verifying cross-module interactions: CAN bus-off recovery, E2E fault chains, heartbeat loss detection, watchdog supervision, signal routing, safe-state transitions. |
| **SW Qualification Test** | SWE.6 | `test/sil/`, `docs/aspice/verification/sw-qualification/` | Full SIL simulation with all 7 ECUs in Docker containers. End-to-end qualification scenarios validating the complete software against SW requirements. |

#### Hardware Engineering (HWE)

| Process | ID | Document | What it does |
|---------|----|----------|--------------|
| **HW Requirements** | HWE.1 | `docs/aspice/hardware-eng/hw-requirements.md` | Hardware requirements derived from system architecture — MCU selection criteria, CAN transceiver specs, power budget, EMC constraints. |
| **HW Design** | HWE.2 | `docs/aspice/hardware-eng/hw-design.md` | PCB design, schematic references, pin allocations. Links to schematics in `hardware/schematics/`. |

#### Management & Support

| Process | ID | Document | What it does |
|---------|----|----------|--------------|
| **Project Management** | MAN.3 | `docs/aspice/plans/MAN.3-project-management/` | Execution roadmap, weekly status reports, daily logs, risk register, issue log, decision log, gate readiness checklists. Full project visibility. |
| **Quality Assurance** | SUP.1 | `docs/aspice/quality/qa-plan.md` | QA plan defining review gates, audit schedules, and quality criteria for each deliverable. |
| **Configuration Management** | SUP.8 | `docs/aspice/cm/` | CM strategy (branching model, baseline management), baseline templates, change request tracking. Ensures every artifact version is traceable to a configuration baseline. |

### Document master index

All 57 registered documents are tracked in [`docs/INDEX.md`](docs/INDEX.md) — the single registry that maps every artifact to its ASPICE process area, ISO 26262 part, and current status (active/draft/planned/baselined).

---

## ISO 26262 — Functional Safety (ASIL D)

### What is ISO 26262?

**ISO 26262** is the international standard for functional safety of road vehicles. It defines a safety lifecycle from concept through decommissioning, with rigor levels called **ASIL** (Automotive Safety Integrity Level) from A (lowest) to D (highest). ASIL D demands the most stringent analysis, verification, and documentation.

### Safety artifacts in this project

#### Part 3 — Concept Phase

| Document | Path | Purpose |
|----------|------|---------|
| **Item Definition** | `docs/safety/concept/item-definition.md` | Defines what the system IS — boundaries, interfaces, operating environment, assumptions. The starting point for all safety analysis. |
| **HARA** | `docs/safety/concept/hara.md` | **Hazard Analysis and Risk Assessment** — systematically identifies hazardous events (e.g., "unintended acceleration"), assesses severity/exposure/controllability, and assigns ASIL levels. |
| **Safety Goals** | `docs/safety/concept/safety-goals.md` | Top-level safety requirements derived from HARA. Each safety goal has an ASIL level and a safe state (e.g., "SG-01: Prevent unintended acceleration → ASIL D → Safe state: torque cutoff"). |
| **Functional Safety Concept** | `docs/safety/concept/functional-safety-concept.md` | Allocates safety goals to architectural elements. Defines which ECU is responsible for which safety function, and what redundancy/monitoring mechanisms are needed. |

#### Part 2 — Safety Management

| Document | Path | Purpose |
|----------|------|---------|
| **Safety Plan** | `docs/safety/plan/safety-plan.md` | Project-level plan for safety activities: who does what, when, with which methods. Defines roles (safety manager, assessor), review gates, and tool qualification requirements. |
| **Safety Case** | `docs/safety/plan/safety-case.md` | The argument that the system is safe — a structured compilation of all evidence (analyses, tests, reviews) showing that safety goals are met. The final deliverable for safety assessment. |

#### Parts 5, 9 — Safety Analysis

| Document | Path | Purpose |
|----------|------|---------|
| **FMEA** | `docs/safety/analysis/fmea.md` | **Failure Mode and Effects Analysis** — for each component, lists possible failure modes, their effects on the system, and detection/mitigation mechanisms. Quantifies risk. |
| **DFA** | `docs/safety/analysis/dfa.md` | **Dependent Failure Analysis** — identifies common-cause and cascading failures. Ensures that a single root cause cannot violate multiple safety goals (e.g., shared power supply failures). |
| **ASIL Decomposition** | `docs/safety/analysis/asil-decomposition.md` | Formally decomposes ASIL D requirements into redundant lower-ASIL elements (e.g., ASIL D = ASIL B(D) + ASIL B(D)) with independence arguments. |
| **Hardware Metrics** | `docs/safety/analysis/hardware-metrics.md` | Calculates SPFM (Single-Point Fault Metric) and LFM (Latent Fault Metric) per ISO 26262-5. Demonstrates that hardware random failures are sufficiently detected. |
| **MISRA Deviation Register** | `docs/safety/analysis/misra-deviation-register.md` | Documents every MISRA C:2012 rule deviation with justification, risk assessment, and compensating measures. Required for ASIL D certification. |
| **Heartbeat/FTTI Budget** | `docs/safety/analysis/heartbeat-ftti-budget.md` | **Fault Tolerant Time Interval** allocation — proves that the system can detect and react to faults within the time budget before a hazardous event occurs. |

#### Parts 3–6 — Safety Requirements

| Document | Path | Purpose |
|----------|------|---------|
| **Functional Safety Requirements** | `docs/safety/requirements/functional-safety-reqs.md` | Refined from safety goals — what the system must do to be safe (functional behavior). |
| **Technical Safety Requirements** | `docs/safety/requirements/technical-safety-reqs.md` | How the system achieves functional safety — specific technical mechanisms (CRC checks, timeouts, redundancy). |
| **SW Safety Requirements** | `docs/safety/requirements/sw-safety-reqs.md` | Software-specific safety requirements derived from technical safety requirements. Each maps to code modules. |
| **HW Safety Requirements** | `docs/safety/requirements/hw-safety-reqs.md` | Hardware-specific safety requirements — MCU lockstep, ECC RAM, voltage monitoring thresholds. |
| **HSI Specification** | `docs/safety/requirements/hsi-specification.md` | **Hardware-Software Interface** — defines the exact contract between HW and SW (register addresses, timing, interrupt behavior). |

#### Part 4 — Safety Validation

| Document | Path | Purpose |
|----------|------|---------|
| **Safety Validation Report** | `docs/safety/validation/safety-validation-report.md` | Final validation that the integrated system meets safety goals in the target environment. Includes HIL test results and field-representative scenarios. |

---

## Automated Traceability

### What is traceability?

Traceability is the ability to link every requirement to its origin (upstream) and to its implementation and test (downstream). Both ASPICE and ISO 26262 mandate **bidirectional traceability** — from stakeholder need → system requirement → SW requirement → code → test → result.

### How traceability works in this project

```
Stakeholder Req (SYS.1)
    │
    ▼
System Req (SYS.2)  ◄────►  System Verification (SYS.5)
    │
    ▼
SW Requirement (SWE.1)  ◄────►  SW Qualification Test (SWE.6)
    │
    ▼
SW Architecture (SWE.2)  ◄────►  SW Integration Test (SWE.5)
    │
    ▼
Code (SWE.3)  ◄────►  Unit Test (SWE.4)
```

| Artifact | Path | Purpose |
|----------|------|---------|
| **Traceability Matrix** | `docs/aspice/traceability/traceability-matrix.md` | Master mapping of requirement IDs → design elements → code files → test cases → test results. |
| **Verification Traceability** | `docs/aspice/verification/traceability-matrix.md` | Test coverage mapping — ensures every requirement has at least one verification activity. |
| **Traceability Playbook** | `docs/aspice/verification/traceability-playbook.md` | Process guide: how to maintain traceability when adding new requirements, new tests, or new code. |
| **CI Enforcement** | `.github/workflows/traceability.yml` | Automated CI pipeline that validates traceability links are complete and consistent on every push. Broken links = blocked merge. |

### Why employers care

In OEM audits (BMW SWL, VW KGAS), the first thing assessors check is traceability coverage. An incomplete trace matrix is an automatic audit finding. This project demonstrates the discipline to maintain it — and the tooling to enforce it automatically.

---

## V-Model Verification (xIL Testing)

The project implements a complete **xIL (x-in-the-loop)** verification chain matching the V-model:

```
                    Requirements                              Validation
                    ───────────                              ──────────
              SYS.2 System Req    ◄─────────────────►  HIL  (real HW, real CAN)
                    │                                     │
              SWE.1 SW Req        ◄─────────────────►  PIL  (real MCU, sim plant)
                    │                                     │
              SWE.2 SW Arch       ◄─────────────────►  SIL  (Docker, 7 vECUs)
                    │                                     │
              SWE.3 Code          ◄─────────────────►  MIL  (model verification)
                                                          │
                                                      Unit Tests (per function)
```

### Test levels explained

| Level | What it is | How it runs | What it proves |
|-------|-----------|-------------|----------------|
| **Unit Test** | Tests individual C functions in isolation | Native compilation (GCC), Unity framework, runs in CI | Each function behaves correctly given valid/invalid inputs |
| **MIL** (Model-in-the-Loop) | Tests plant model + control algorithm mathematically | Python/Simulink simulation | Control logic is correct before any C code exists |
| **SIL** (Software-in-the-Loop) | All 7 ECUs compiled for POSIX, running in Docker containers on a virtual CAN bus | `docker compose -f docker/docker-compose.sil.yml up` | Complete software system works — message routing, E2E protection, fault handling, mode transitions |
| **PIL** (Processor-in-the-Loop) | Real firmware running on real MCU, but with a simulated plant model | Flash to STM32/TMS570, inject plant signals via CAN/UART | Code works on actual hardware — timing, interrupts, memory layout, peripheral drivers |
| **HIL** (Hardware-in-the-Loop) | Real firmware on real MCU, real CAN bus, real sensor/actuator interfaces | Raspberry Pi test bench (MCP2515 CAN), physical wiring | End-to-end system works in a representative physical environment |

### Integration test suites

11 integration test suites covering critical cross-module interactions:

| Test Suite | ASIL | What it verifies |
|------------|------|------------------|
| `test_int_can_busoff_asild` | D | CAN bus-off detection, recovery sequence, and degraded-mode operation |
| `test_int_e2e_chain_asild` | D | End-to-end protection: CRC insertion at sender → CRC verification at receiver → fault on mismatch |
| `test_int_e2e_faults_asild` | D | E2E fault injection: corrupted CRC, stale counter, missing message → correct DEM event raised |
| `test_int_heartbeat_loss_asild` | D | SC detects missing heartbeat from CVC/FZC → triggers safe-state within FTTI budget |
| `test_int_safe_state_asild` | D | All fault paths lead to safe state: torque cutoff, steering to neutral, warning activated |
| `test_int_wdgm_supervision_asild` | D | Watchdog supervision: alive counter monitoring, deadline monitoring, program flow monitoring |
| `test_int_overcurrent_chain_asild` | D | Overcurrent detection → actuator shutdown → DEM event → safe state |
| `test_int_can_matrix_asilc` | C | All 32 CAN messages route correctly between ECUs per the DBC matrix |
| `test_int_signal_routing_asilc` | C | Signals flow from SWC → RTE → Com → CAN → Com → RTE → SWC across ECU boundaries |
| `test_int_dem_to_dcm_asilc` | C | DEM fault events are readable via UDS (Dcm) — diagnostic readout works |
| `test_int_bswm_mode_asilc` | C | BSW mode transitions (startup → run → shutdown → safe) execute correctly |

---

## MISRA C:2012 Static Analysis

### What is MISRA C?

**MISRA C:2012** is a set of coding guidelines for safety-critical C code, mandated by ISO 26262 for ASIL C/D software. It restricts dangerous C constructs (pointer arithmetic, implicit conversions, undefined behavior) to prevent entire categories of bugs.

### Implementation in this project

- **Tool**: cppcheck with MISRA addon (local 2.17, CI 2.13)
- **Config**: `tools/misra/misra.json` (rule selection), `tools/misra/suppressions.txt` (justified suppressions)
- **CI**: `.github/workflows/misra.yml` — **blocking** (`error-exitcode=1`), 0 violations allowed
- **Deviations**: All documented in `docs/safety/analysis/misra-deviation-register.md` with:
  - Rule number and description
  - Justification (why the deviation is necessary)
  - Risk assessment
  - Compensating measures (e.g., code review, additional testing)

Current status: **0 violations, CI green and blocking.**

---

## CI/CD Pipelines

7 GitHub Actions workflows enforce quality gates on every commit:

| Workflow | File | Purpose | Gate |
|----------|------|---------|------|
| **CI** | `ci.yml` | Linting, formatting, basic build verification | Blocking |
| **Tests** | `test.yml` | Unit tests, integration tests, SIL test subset | Blocking |
| **MISRA** | `misra.yml` | MISRA C:2012 static analysis (0 violations allowed) | Blocking |
| **Traceability** | `traceability.yml` | Validates requirement ↔ code ↔ test links are complete | Blocking |
| **SIL Nightly** | `sil-nightly.yml` | Full 7-ECU SIL simulation (all scenarios) | Nightly |
| **HIL Nightly** | `hil-nightly.yml` | Hardware-in-the-loop test suite (Raspberry Pi bench) | Nightly |
| **HIL Preflight** | `hil-preflight-nightly.yml` | Pre-flight convergence checks before HIL campaign | Nightly |

---

## Quick Start

```bash
# SIL simulation — all 7 ECUs in Docker
docker compose -f docker/docker-compose.sil.yml up

# Build for POSIX (host testing)
make -f firmware/platform/posix/Makefile.posix all

# Build for STM32 (physical ECU)
make -f firmware/platform/stm32/Makefile.stm32 build

# Build for TMS570 (Safety Controller)
make -f firmware/platform/tms570/Makefile.tms570 build

# Run MISRA analysis
cppcheck --project=compile_commands.json --addon=tools/misra/misra.json

# Regenerate configs from DBC (full pipeline)
python tools/arxml/dbc2arxml.py gateway/taktflow.dbc arxml/
python -m tools.arxmlgen
```

---

## Project Structure

```
firmware/
  bsw/                   AUTOSAR-like BSW stack (shared across all ECUs)
    mcal/                 MCAL drivers: Can, Spi, Adc, Pwm, Dio, Gpt, Uart
    ecual/                ECU abstraction: CanIf, PduR, IoHwAb
    services/             Services: Com, Dcm, Dem, E2E, WdgM, BswM, NvM, SchM, Det, CanTp
    rte/                  Runtime Environment (signal routing)
    os/                   OS abstraction (bare-metal scheduler / POSIX shim)
  ecu/                    Per-ECU application code (SWCs + generated configs)
  ecu_cpp/                C++ vECU implementations (BCM, ICU, TCU — CMake)
  platform/               Platform-specific implementations per target MCU
  lib/vendor/             Wrapped third-party libraries
arxml/                    AUTOSAR model (TaktflowSystem.arxml)
generated/                Auto-generated per-ECU configuration (Com, Rte, CanIf, PduR, E2E)
gateway/                  Edge services: CAN gateway, plant sim, diagnostics, ML, MQTT, SAP QM
  taktflow.dbc            CAN message matrix (single source of truth)
docker/                   Container orchestration (SIL, HIL, dev, GIL)
test/
  unit/                   Unit tests (Unity framework)
  framework/              Integration test suites (11 suites)
  sil/, hil/, mil/, pil/  xIL test scenarios, fixtures, reports
tools/
  arxmlgen/               ARXML → C code generator (Python + Jinja2)
  arxml/                  DBC ↔ ARXML converters
  codegen/                Generic code generation framework
  misra/                  MISRA C:2012 config and suppressions
  trace/                  Traceability validation tools
  ci/                     CI/CD scripts
docs/
  INDEX.md                Master document registry (ASPICE + ISO 26262 mapping)
  safety/                 ISO 26262 artifacts (concept, plan, analysis, requirements, validation)
  aspice/                 ASPICE deliverables (system, software, HW, verification, CM, QA)
  plans/                  Implementation plans (58 active/draft)
  reference/              Process playbook, lessons learned
hardware/                 Schematics, pin maps, BOM
scripts/                  Build, deploy, debug utilities
.github/workflows/        7 CI/CD pipelines
```

---

## Key Principles

1. **DBC is truth** — all CAN configuration is generated from `gateway/taktflow.dbc`, never hand-edited
2. **Generate, don't copy** — `ecu/*/cfg/` files are machine-generated from ARXML; fix the generator, not the output
3. **Platform abstraction** — same SWC code compiles identically for STM32, TMS570, and POSIX (Docker SIL)
4. **Fail-closed safety** — all fault paths transition to a defined safe state; errors are never silently ignored
5. **Vendor independence** — vendor SDKs are wrapped in abstraction layers; swapping a vendor = changing one file
6. **Trace everything** — every requirement links to code and tests; CI enforces completeness
7. **Plan first, code second** — every feature starts with a plan in `docs/plans/`, approved before implementation

---

## Metrics

| Metric | Value |
|--------|-------|
| Firmware source files (C/H) | 662 |
| Documentation files | 209 |
| Test files | 125 |
| ECUs | 7 (4 physical + 3 virtual) |
| Target platforms | 5 (STM32F4, STM32L5, TMS570, POSIX, Docker C++) |
| CAN messages | 32 |
| Integration test suites | 11 |
| CI/CD pipelines | 7 |
| ASPICE documents registered | 57 |
| MISRA violations | 0 |
| E2E profile | AUTOSAR P01 (CRC-8 + alive counter) |

---

## License

Copyright Taktflow Systems 2026. All rights reserved.
