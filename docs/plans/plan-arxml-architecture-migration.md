# Plan: ARXML Architecture Migration

**Status:** DRAFT
**Created:** 2026-03-10
**Author:** Claude + Anh
**ADR:** T1 (architecture — locked once codegen replaces hand-written configs)

## Overview

Migrate the Taktflow embedded platform from hand-written configuration files (`*_Cfg.h`, `Com_Cfg_*.c`, `Rte_Cfg_*.c`, CanIf/PduR tables in `main.c`) to a code-generated architecture using ARXML as the single source of truth. The DBC file (`gateway/taktflow.dbc`) already contains the complete communication matrix and serves as the primary input. Open-source tooling (`autosar-data-py` for ARXML, `cantools` for DBC parsing) eliminates commercial tool dependency.

### Problem Statement

Current architecture suffers from:
1. **Configuration by copy** — 6× `Com_Cfg_*.c`, 6× `Rte_Cfg_*.c`, 6× `*_Cfg.h`, 6× CanIf/PduR tables in `main.c` — all hand-maintained with magic numbers
2. **No cross-ECU signal contract** — vehicle state enum, fault codes, E2E data IDs duplicated per ECU with no shared source
3. **Implicit CAN ID ↔ PDU ID mapping** — CanIf routing table in `main.c` must exactly match `Com_Cfg` PDU IDs by convention
4. **No type safety** — all RTE signals are `uint32`, pedal angles and motor temps use the same untyped pipe
5. **Duplication-driven bugs** — recent HIL preflight bugs (signal encoding mismatches, feedback ordering) caused by copy-paste config drift

### Architecture Decision

| Option | Pros | Cons | Score |
|--------|------|------|-------|
| A. YAML → codegen | Lightweight, fast, no schema | Custom schema = custom bugs | C:1 T:1 S:2 R:2 = 6 |
| B. **ARXML → codegen (open-source)** | Industry standard, validated schema, portable | Learning curve, verbose XML | C:1 T:2 S:3 R:3 = **9** |
| C. Commercial AUTOSAR tools | Full AUTOSAR stack | €15-30k/seat/year, overkill | C:3 T:3 S:3 R:1 = 10 |

**Chosen: Option B** — `autosar-data-py` (MIT/Apache-2.0) provides schema-validated ARXML R/W for all 22 AUTOSAR 4.x revisions. `cantools` (MIT) parses our existing DBC. Zero license cost. ARXML portable to any integrator/OEM if needed later.

## Phase Table

| Phase | Name | Status | Depends On |
|-------|------|--------|------------|
| 0 | Tooling setup & spike | PENDING | — |
| 1 | DBC → ARXML communication layer | PENDING | Phase 0 |
| 2 | SWC extraction → ARXML application layer | PENDING | Phase 1 |
| 3 | ARXML → codegen (_Cfg.h, Com_Cfg, Rte_Cfg) | PENDING | Phase 2 |
| 4 | CanIf/PduR/main.c template generation | PENDING | Phase 3 |
| 5 | Shared enums & type-safe RTE wrappers | PENDING | Phase 3 |
| 6 | CI integration & hand-written config removal | PENDING | Phase 4, 5 |
| 7 | Heartbeat framework consolidation | PENDING | Phase 6 |

---

## Phase 0: Tooling Setup & Spike

### Tasks
- [ ] Install `autosar-data` and `cantools` Python packages
- [ ] Write spike script that loads `taktflow.dbc` and creates a minimal ARXML with one ECU + one message
- [ ] Validate generated ARXML loads back without errors (round-trip test)
- [ ] Document AUTOSAR version choice (recommend R22-11 / AUTOSAR 4.7.0)
- [ ] Create `tools/arxml/` directory for all codegen scripts

### Files Changed
- `tools/arxml/spike_dbc2arxml.py` — new: spike script
- `tools/arxml/requirements.txt` — new: `autosar-data>=0.10`, `cantools>=39.0`

### DONE Criteria
- Spike script generates valid ARXML from DBC
- ARXML loads back with `autosar-data` without validation errors
- Team agrees on AUTOSAR schema version

---

## Phase 1: DBC → ARXML Communication Layer

### Tasks
- [ ] Parse `gateway/taktflow.dbc` with `cantools` — extract all 24 messages, 100+ signals, ECU nodes
- [ ] Generate `System.arxml`: `CommunicationCluster` (CAN, 500kbps), `PhysicalChannel`, `EcuInstance` × 7
- [ ] Generate `Communication.arxml`: `ISignalIPdu` per message (CAN ID, DLC, cycle time, send type from `BA_` attributes)
- [ ] Generate `ISignal` per DBC signal (bit position, length, factor, offset, unit, init value)
- [ ] Generate `ISignalToIPduMapping` linking signals to PDUs
- [ ] Map DBC `VAL_` definitions to `CompuMethod` (enumeration scales: OperatingMode, Direction, BrakeMode, etc.)
- [ ] Map DBC signal ranges/factor/offset to `CompuMethod` (linear scales: SteerAngleCmd 0.01 factor, -45 offset)
- [ ] Generate `E2EProtectionSet` from E2E signal patterns (DataID, AliveCounter, CRC8 per protected PDU)
- [ ] Map `BA_ "ASIL"` per message to custom ARXML annotation or admin-data
- [ ] Generate `EcuMapping` — TX/RX signal routing per ECU from DBC `SG_` receiver lists

### Source Mapping

| DBC Element | ARXML Element |
|---|---|
| `BU_: CVC FZC ...` | `EcuInstance` |
| `BO_ 256 Vehicle_State: 6 CVC` | `ISignalIPdu` (ID=0x100, DLC=6, TX=CVC) |
| `SG_ VehicleState : 16\|4@1+` | `ISignal` (startBit=16, length=4, byteOrder=LITTLE_ENDIAN) |
| `(0.01,-45.0)` | `CompuMethod` (LINEAR, factor=0.01, offset=-45.0) |
| `[-45.0\|45.0] "deg"` | `SwDataDefProps` (min=-45, max=45, unit=deg) |
| `"" FZC,RZC,BCM,ICU,SC` | `ISignalPort` RX mappings per ECU |
| `VAL_ 256 VehicleState 0 "INIT"...` | `CompuMethod` TEXTTABLE |
| `BA_ "GenMsgCycleTime" BO_ 256 10` | `ISignalIPdu` timing (cyclePeriod=0.01s) |
| `BA_ "ASIL" BO_ 256 "D"` | Annotation / AdminData |
| `CM_ BO_ 256 "Vehicle state..."` | `ISignalIPdu` desc / annotation |

### Files Changed
- `tools/arxml/dbc2arxml.py` — new: DBC-to-ARXML converter (~400 lines)
- `arxml/System.arxml` — new: generated system topology
- `arxml/Communication.arxml` — new: generated IPdus, ISignals, CompuMethods
- `arxml/E2E.arxml` — new: generated E2E protection sets

### DONE Criteria
- All 24 DBC messages appear as `ISignalIPdu` in ARXML
- All 100+ signals appear as `ISignal` with correct bit layout, factor, offset
- All `VAL_` enums appear as `CompuMethod` TEXTTABLE
- All E2E-protected messages (16/24) have `E2EProtectionSet` entries
- ARXML validates against AUTOSAR R22-11 schema
- Round-trip test: DBC → ARXML → re-extract signal info → matches DBC

---

## Phase 2: SWC Extraction → ARXML Application Layer

### Tasks
- [ ] Parse all 48 `Swc_*.h` headers — extract Init/MainFunction/RxIndication/TxConfirmation signatures
- [ ] Parse `Rte_Cfg_*.c` (6 files) — extract runnable-to-function mapping, period, priority, WdgM SE ID
- [ ] Parse `*_Cfg.h` (6 files) — extract RTE signal IDs, DTC event IDs, E2E data IDs, threshold constants
- [ ] Generate `SwComponents.arxml`: `ApplicationSwComponentType` per SWC with:
  - `PPortPrototype` / `RPortPrototype` per signal read/written
  - `InternalBehavior` with `RunnableEntity` (period from Rte_Cfg)
  - `TimingEvent` linking runnable to period
- [ ] Generate `Interfaces.arxml`: `SenderReceiverInterface` per signal group (e.g., `SR_SteerCommand` containing SteerAngleCmd + SteerRateLimit + VehicleState)
- [ ] Generate `DataTypes.arxml`: `ApplicationPrimitiveDataType` per unique signal type with constraints
- [ ] Generate `EcuComposition.arxml`: `CompositionSwComponentType` per ECU wiring SWC ports together

### Source Mapping

| Code Element | ARXML Element |
|---|---|
| `Swc_Steering.h` (Init, MainFunction) | `ApplicationSwComponentType` "Swc_Steering" |
| `Rte_Cfg_Fzc.c`: period=10, prio=6 | `TimingEvent` (period=0.01), `RunnableEntity` |
| `FZC_SIG_STEER_CMD` (Rte_Read) | `RPortPrototype` → `SenderReceiverInterface` |
| `FZC_SIG_STEER_ANGLE_OUT` (Rte_Write) | `PPortPrototype` → `SenderReceiverInterface` |
| `FZC_DTC_STEER_PLAUSIBILITY` | `DiagnosticEventMapping` |
| `Fzc_Cfg.h` thresholds | `ParameterSwComponentType` or `CalibrationParameter` |

### Files Changed
- `tools/arxml/swc_extractor.py` — new: parses Swc_*.h, Rte_Cfg, *_Cfg.h (~500 lines)
- `arxml/SwComponents.arxml` — new: generated SWC descriptions
- `arxml/Interfaces.arxml` — new: generated SR interfaces
- `arxml/DataTypes.arxml` — new: generated data types + constraints
- `arxml/EcuComposition_Cvc.arxml` — new: CVC SWC wiring (1 per ECU)

### DONE Criteria
- All 48 SWCs appear as `ApplicationSwComponentType` in ARXML
- All runnables have correct period and priority
- All RTE signals mapped to ports with typed interfaces
- Signal type constraints match DBC ranges (e.g., SteerAngle -45..+45)
- ARXML validates against AUTOSAR schema

---

## Phase 3: ARXML → Codegen (Config Files)

### Tasks
- [ ] Write codegen script that reads ARXML and generates:
  - `*_Cfg.h` — RTE signal IDs, Com PDU IDs, DTC IDs, E2E data IDs, enums, thresholds
  - `Com_Cfg_*.c` — signal config table, TX/RX PDU config tables
  - `Rte_Cfg_*.c` — signal config table, runnable config table
- [ ] Add `/* GENERATED — DO NOT EDIT */` header to all generated files
- [ ] Implement diff-test: generated files vs current hand-written files (must match semantically)
- [ ] Handle platform-specific constants (`PLATFORM_POSIX` guards) via ARXML variant handling or codegen flag

### Generated File Mapping

| ARXML Source | Generated File | Replaces |
|---|---|---|
| `EcuInstance` + `ISignalIPdu` TX/RX | `Cvc_Cfg.h` (Com PDU IDs) | Hand-written `#define CVC_COM_TX_*` |
| `RunnableEntity` + port connections | `Cvc_Cfg.h` (RTE signal IDs) | Hand-written `#define CVC_SIG_*` |
| `CompuMethod` TEXTTABLE | `Cvc_Cfg.h` (enums) | Hand-written `#define CVC_STATE_*` |
| `ISignal` + `ISignalToIPduMapping` | `Com_Cfg_Cvc.c` | Hand-written signal table |
| `TimingEvent` + priority | `Rte_Cfg_Cvc.c` | Hand-written runnable table |

### Files Changed
- `tools/arxml/arxml2cfg.py` — new: ARXML-to-C codegen (~600 lines)
- `firmware/cvc/cfg/Cvc_Cfg.h` — regenerated (verify identical semantics)
- `firmware/cvc/cfg/Com_Cfg_Cvc.c` — regenerated
- `firmware/cvc/cfg/Rte_Cfg_Cvc.c` — regenerated
- (same for fzc, rzc, bcm, icu, tcu — 18 files total)

### DONE Criteria
- `arxml2cfg.py` generates all 18 config files from ARXML
- Diff-test: generated configs compile and produce identical binary behavior
- All 1,459 unit tests pass with generated configs
- MISRA: 0 new violations
- CI build passes for all 6 ECUs (POSIX) and all 3 ECUs (STM32)

---

## Phase 4: CanIf/PduR/main.c Template Generation

### Tasks
- [ ] Extract CanIf TX config from ARXML: `ISignalIPdu` CAN ID ↔ Com PDU ID mapping per ECU
- [ ] Extract CanIf RX config from ARXML: CAN ID → Com RX PDU ID per ECU
- [ ] Extract PduR routing from ARXML: RX PDU → destination (Com or Dcm)
- [ ] Generate `CanIf_Cfg_*.c` — standalone CanIf config (currently embedded in main.c)
- [ ] Generate `PduR_Cfg_*.c` — standalone PduR config (currently embedded in main.c)
- [ ] Create `main_template.c` — common BSW init sequence, parameterized by ECU config
- [ ] Refactor 6× main.c files to use generated CanIf/PduR configs + common template

### Files Changed
- `tools/arxml/arxml2canif.py` — new: CanIf/PduR codegen
- `firmware/cvc/cfg/CanIf_Cfg_Cvc.c` — new: generated (extracted from main.c lines 125-161)
- `firmware/cvc/cfg/PduR_Cfg_Cvc.c` — new: generated (extracted from main.c lines 165-190)
- `firmware/cvc/src/main.c` — refactored: include generated configs, remove inline tables
- (same for fzc, rzc, bcm, icu, tcu)
- `firmware/shared/bsw/main_common.h` — new: common BSW init macro/template

### DONE Criteria
- CanIf/PduR configs no longer live inside main.c
- main.c files reduced by ~100 lines each (routing tables extracted)
- All unit tests pass
- SIL integration test: 7 ECUs boot and communicate on vcan0

---

## Phase 5: Shared Enums & Type-Safe RTE Wrappers

### Tasks
- [ ] Generate `Taktflow_Types.h` from ARXML `CompuMethod` TEXTTABLE — shared enums across all ECUs:
  - `VehicleState_t` (INIT=0, RUN=1, DEGRADED=2, ...)
  - `BrakeMode_t` (RELEASE=0, NORMAL=1, EMERGENCY=2, AUTO=3)
  - `MotorDirection_t`, `ObstacleZone_t`, `BatteryStatus_t`, etc.
- [ ] Generate typed RTE wrappers per ECU from ARXML port definitions:
  - `Rte_Write_Swc_Steering_SteerAngle(sint16 angle_deg)` instead of `Rte_Write(FZC_SIG_STEER_ANGLE_OUT, (uint32)angle)`
  - `Rte_Read_Swc_Steering_SteerCmd(sint16 *cmd_deg)` instead of `Rte_Read(FZC_SIG_STEER_CMD, &val)`
- [ ] Replace enum `#define` blocks in SWC source with shared `Taktflow_Types.h` includes
- [ ] Update SWC source to use typed wrappers (incremental — start with CVC, FZC, RZC)

### Files Changed
- `firmware/shared/bsw/include/Taktflow_Types.h` — new: generated shared enums
- `firmware/cvc/include/Rte_Swc_Pedal.h` — new: generated typed wrappers for Swc_Pedal
- `firmware/fzc/include/Rte_Swc_Steering.h` — new: generated typed wrappers
- (one per SWC — 48 files)
- `firmware/cvc/src/Swc_VehicleState.c` — updated: use `VehicleState_t` enum
- `firmware/fzc/src/Swc_Steering.c` — updated: use typed Rte_Read/Write
- (incremental across all SWCs)

### DONE Criteria
- Vehicle state enum defined once in `Taktflow_Types.h`, used by all ECUs
- At least CVC + FZC + RZC SWCs use typed RTE wrappers
- Compile error if wrong type passed to Rte_Write (type mismatch)
- All tests pass

---

## Phase 6: CI Integration & Hand-Written Config Removal

### Tasks
- [ ] Add CI step: `python tools/arxml/arxml2cfg.py --verify` — checks generated configs match committed configs
- [ ] Add CI step: ARXML schema validation (`autosar-data` strict mode)
- [ ] Add `Makefile` target: `make generate-config` — runs full ARXML → C codegen pipeline
- [ ] Remove hand-written `_Cfg.h` / `Com_Cfg` / `Rte_Cfg` comments saying "manual" — replace with `GENERATED`
- [ ] Add `arxml/` directory to `.gitattributes` as generated source
- [ ] Document codegen workflow in `docs/reference/arxml-codegen-workflow.md`

### Files Changed
- `.github/workflows/ci.yml` — updated: add ARXML validation + config freshness check
- `firmware/Makefile.posix` — updated: add `generate-config` target
- `docs/reference/arxml-codegen-workflow.md` — new: developer guide
- `arxml/*.arxml` — committed as source of truth

### DONE Criteria
- CI fails if hand-edited config doesn't match ARXML-generated config
- `make generate-config` regenerates all 18+ config files in <5 seconds
- Developer docs explain: "edit ARXML or DBC → run codegen → commit generated files"

---

## Phase 7: Heartbeat Framework Consolidation

### Tasks
- [ ] Create `Swc_Heartbeat_Common.c/h` — single parameterized heartbeat implementation
- [ ] Configuration via ARXML: ECU ID, TX CAN ID, TX period, E2E data ID, monitored ECUs list
- [ ] Generate per-ECU heartbeat config from ARXML (replaces 7 separate Swc_Heartbeat.c files)
- [ ] Remove duplicated Swc_Heartbeat.c from cvc, fzc, rzc, bcm, icu, tcu
- [ ] Update Rte_Cfg to reference common heartbeat runnable

### Files Changed
- `firmware/shared/bsw/services/Swc_Heartbeat_Common.c` — new: unified heartbeat
- `firmware/shared/bsw/services/Swc_Heartbeat_Common.h` — new: interface
- `firmware/cvc/src/Swc_Heartbeat.c` — deleted (replaced by common)
- `firmware/fzc/src/Swc_Heartbeat.c` — deleted
- `firmware/rzc/src/Swc_Heartbeat.c` — deleted
- (4 more ECUs)

### DONE Criteria
- Single heartbeat implementation serves all 6 AUTOSAR ECUs
- All heartbeat unit tests pass with common implementation
- E2E alive counter + CRC behavior identical to current per-ECU implementations
- SIL integration: all 7 ECUs heartbeat on CAN bus, SC monitors successfully

---

## Security Considerations

- **No secrets in ARXML** — ARXML contains only signal definitions, no credentials or keys
- **Codegen output validation** — generated C must pass MISRA static analysis (no injection vectors)
- **Supply chain** — `autosar-data` and `cantools` are MIT/Apache-2.0 open source; pin versions in `requirements.txt`
- **Build reproducibility** — codegen must be deterministic (same ARXML → same C output, byte-for-byte)

## Testing Plan

### Unit Tests
- Existing 1,459 tests must pass with generated configs (regression gate)
- New tests for codegen scripts: DBC parsing edge cases, ARXML round-trip, config semantic equivalence

### Integration Tests
- SIL 7-ECU boot test with generated configs
- CAN signal exchange validation (compare DBC signal values vs ARXML-generated Com signal extraction)

### Safety Validation
- Diff-audit: generated `*_Cfg.h` vs hand-written originals — verify no semantic changes
- E2E protection: verify generated E2E data IDs match current values
- Traceability: generated files carry `@traces_to` from ARXML annotations

### Hardware Tests (TODO:HARDWARE)
- Flash STM32 with generated configs → CAN bus communication with existing firmware
- Heartbeat common implementation on 3 physical ECUs

## Open Questions

1. **AUTOSAR schema version** — R22-11 (4.7.0) or R23-11 (latest)? Depends on `autosar-data` support.
2. **Platform variants** — how to represent `PLATFORM_POSIX` vs `PLATFORM_STM32` threshold differences in ARXML? Options: variant coding, separate variant ARXML, or codegen flag.
3. **SC (Safety Controller)** — SC uses flat non-AUTOSAR architecture. Include SC signals in ARXML for communication only, or also model SC internal structure?
4. **DBC vs ARXML as communication source** — keep DBC as the primary CAN database (used by `cantools`, can-monitor, gateway) and generate ARXML from it? Or migrate fully to ARXML and generate DBC from ARXML for tools that need it?
5. **Threshold/calibration values** — put in ARXML as `CalibrationParameter`? Or keep in a separate calibration file and only generate structural configs from ARXML?

## File Inventory (Current → Generated)

| Current File | Count | Generated From | Phase |
|---|---|---|---|
| `*_Cfg.h` | 6 | ARXML SWC + DataTypes | 3 |
| `Com_Cfg_*.c` | 6 | ARXML Communication | 3 |
| `Rte_Cfg_*.c` | 6 | ARXML SWC behaviors | 3 |
| CanIf tables in `main.c` | 6 | ARXML Communication | 4 |
| PduR tables in `main.c` | 6 | ARXML Communication | 4 |
| State/fault enums in `*_Cfg.h` | 6 | ARXML DataTypes | 5 |
| `Swc_Heartbeat.c` | 6 | Common framework + ARXML config | 7 |
| **Total hand-written configs eliminated** | **42** | | |

## Timeline Estimate

| Phase | Effort |
|-------|--------|
| Phase 0: Spike | 1 session |
| Phase 1: DBC → ARXML | 1-2 sessions |
| Phase 2: SWC extraction | 2 sessions |
| Phase 3: ARXML → codegen | 2-3 sessions |
| Phase 4: CanIf/PduR gen | 1-2 sessions |
| Phase 5: Typed wrappers | 2-3 sessions |
| Phase 6: CI integration | 1 session |
| Phase 7: Heartbeat consolidation | 1-2 sessions |
