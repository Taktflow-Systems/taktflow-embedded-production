# Plan: Clean Data Model Rebuild (DBC → ARXML → Codegen → Typed RTE)

**Status**: PENDING
**Date**: 2026-03-18
**Priority**: Critical — blocks SIL-010, SIL-002, SIL-014; enables typed RTE and proper AUTOSAR layering

## Problem Statement

The current data model has diverged:

| Layer | State | Issue |
|-------|-------|-------|
| DBC | 37 messages, 174 signals | Test infrastructure (0x600, 0x601, UDS) mixed with production |
| Production ARXML | 174 I-Signals, 0 SWCs | Communication-only, no software architecture |
| Roundtrip ARXML | 48 SWCs, 71 runnables | Has SWC layer but stale (12 signals behind DBC), no connectors |
| Generated ARXML | 167 I-Signals, 0 SWCs | Stale intermediate file |
| Codegen output | Hand-written signal IDs everywhere | SWCs call `Com_ReceiveSignal` instead of `Rte_Read` |
| RTE | `Rte_Read(uint16 id, uint32* val)` | Untyped, no port abstraction |

Root causes:
1. DBC mixes production and SIL-only signals
2. SWC architecture was generated once (roundtrip) but never maintained
3. No assembly connectors → no port wiring → no typed RTE generation
4. SWCs bypass RTE and call BSW APIs directly

## How Real Automotive Projects Do It

### DBC Convention (Vector/OEM standard)

**Signal names**: `<MessageShortName>_<SignalName>` (qualified, unique across DBC)
- Example: `VehSt_VehicleState`, `MotSt_MotorSpeed_RPM`, `BrkCmd_BrakeForceCmd`
- E2E signals: `VehSt_E2E_CRC8`, `VehSt_E2E_Alive`
- Ensures no collision when two messages have signals with the same concept (e.g., `VehicleState` appears in multiple heartbeats)

**Message names**: `<ECU>_<Function>` or domain-based
- Core: functional messages only (heartbeats, commands, status, faults)
- Diagnostics: separate DBC or DBC overlay (UDS, XCP)
- Test/SIL: never in the production DBC

**Separate DBC files** (industry practice):
- `taktflow_vehicle.dbc` — production CAN matrix (deployed to real ECUs)
- `taktflow_diag.dbc` — UDS/OBD diagnostic overlay
- `taktflow_sil.dbc` — virtual sensors, test injection (SIL-only, never on real bus)

### ARXML Structure (AUTOSAR R4.x)

Real projects have a layered ARXML with these packages:

```
/AUTOSAR/Platform/          — base types (uint8, uint16, etc.)
/DataTypes/                 — implementation data types
/PortInterfaces/            — SENDER-RECEIVER-INTERFACE per signal group
/ComponentTypes/            — APPLICATION-SW-COMPONENT-TYPE per SWC
  /Swc_Motor/
    PORTS: P-PORT (MotorStatus), R-PORT (TorqueCmd, VehicleState)
    INTERNAL-BEHAVIORS:
      RUNNABLES: Swc_Motor_Init (INIT-EVENT), Swc_Motor_Main (TIMING-EVENT 10ms)
      DATA-SEND-POINTS: send MotorStatus.MotorFaultStatus
      DATA-RECEIVE-POINTS: receive TorqueCmd.TorqueRequest
/Compositions/              — COMPOSITION-SW-COMPONENT-TYPE
  /CVC_TopLevel/
    COMPONENTS: Swc_VehicleState, Swc_Heartbeat, Swc_EStop, Swc_Pedal, ...
    CONNECTORS:
      ASSEMBLY: Swc_Heartbeat.FzcAlive → Swc_VehicleState.FzcCommStatus
      DELEGATION: CVC_TopLevel.TorqueRequest → Swc_Pedal.TorqueOut
/SystemDescription/         — SYSTEM with ECU-INSTANCE
  ECU-MAPPING: Swc_Motor → RZC, Swc_VehicleState → CVC
  COM-MAPPING: I-SIGNAL → Port data element
/Communication/             — I-SIGNAL, I-SIGNAL-I-PDU, I-PDU-GROUP
```

### Typed RTE Generation

From the ARXML ports + connectors, the RTE generator produces:

```c
/* Generated per SWC — type-safe, no casting */
Std_ReturnType Rte_Read_Swc_Motor_TorqueCmd_TorqueRequest(uint16 *val);
Std_ReturnType Rte_Write_Swc_Motor_MotorStatus_MotorFaultStatus(uint8 val);

/* SWC code — clean, typed, no signal IDs */
void Swc_Motor_MainFunction(void) {
    uint16 torque;
    Rte_Read_Swc_Motor_TorqueCmd_TorqueRequest(&torque);
    /* ... */
    Rte_Write_Swc_Motor_MotorStatus_MotorFaultStatus(fault);
}
```

No `Rte_Read(173, &uint32)`. No `Com_ReceiveSignal`. No hand-written signal IDs.

## Plan

### Phase 0: Freeze Current State (1 hour)
- [ ] Tag current state as `v0.9.0-pre-cleanup`
- [ ] Run full SIL suite, record 13/18 baseline
- [ ] Document which tests pass/fail and why

### Phase 1: Clean DBC (2 hours)

**1a. Split DBC into production + SIL overlay**
- [ ] Create `gateway/taktflow_vehicle.dbc` — 26 core messages only
- [ ] Create `gateway/taktflow_sil.dbc` — 0x600, 0x601 virtual sensors
- [ ] Create `gateway/taktflow_diag.dbc` — UDS messages (0x7xx)
- [ ] Keep `gateway/taktflow_vehicle.dbc` as merged file (generated from the 3 above for backward compat)

**1b. Fix signal naming convention**
- [ ] Qualify ALL signal names: `<MsgShortName>_<Signal>`
  - `VehicleState` → `VehSt_VehicleState`
  - `E2E_DataID` in Vehicle_State → `VehSt_E2E_DataID`
  - `E2E_DataID` in CVC_Heartbeat → `CvcHb_E2E_DataID`
- [ ] Verify: 0 duplicate signal names across all messages
- [ ] Run `cantools` load test — no warnings

**1c. Validate**
- [ ] `cantools.database.load_file()` succeeds for each DBC
- [ ] Signal count matches expected (26 msgs × avg 6 signals ≈ 156 core signals)
- [ ] All E2E_DataID BA_ attributes present and correct

### Phase 2: Merge ARXML (3 hours)

**2a. Start from roundtrip ARXML (has SWC layer)**
- [ ] Copy `tmp/armodel_roundtrip/TaktflowSystem.arxml` → `arxml/TaktflowSystem.arxml`
- [ ] Run `dbc2arxml` with clean production DBC to update communication layer
- [ ] Verify: I-Signal count matches DBC signal count
- [ ] Verify: I-PDU count matches DBC message count
- [ ] Verify: ECU-INSTANCE count = 7 (no Tester, no Plant_Sim in production)

**2b. Update SWC definitions to match current code**
- [ ] Audit each SWC: ports match `Rte_Read`/`Rte_Write` calls in current code
- [ ] Add missing ports (e.g., Swc_Motor needs R-PORT for TEMP_FAULT, OVERCURRENT)
- [ ] Add missing SWCs if any were added since roundtrip was generated
- [ ] Update runnable periods to match `Rte_Cfg_*.c` scheduling

**2c. Add assembly connectors**
- [ ] For each SWC R-port, identify the sender SWC P-port
- [ ] Create ASSEMBLY-SW-CONNECTOR for each connection
- [ ] Source: trace `Rte_Read`/`Rte_Write` calls in SWC code
- [ ] Target: ~50-100 connectors (each R-port needs one)

**2d. Add compositions and ECU mapping**
- [ ] Create COMPOSITION-SW-COMPONENT-TYPE per ECU (CVC_Composition, FZC_Composition, etc.)
- [ ] Create top-level system composition
- [ ] Add SWC-TO-ECU-MAPPING for all 48 SWCs → 7 ECUs
- [ ] Add DELEGATION-SW-CONNECTOR for ECU boundary ports

**2e. Validate**
- [ ] `autosar_data` reference check: 0 errors
- [ ] Every R-port has exactly 1 assembly connector
- [ ] Every SWC mapped to exactly 1 ECU
- [ ] Signal count matches DBC

### Phase 3: Typed RTE Codegen (4 hours)

**3a. Generate typed RTE header per SWC**
- [ ] Template: `Rte_<SWC>.h` with typed accessor functions
- [ ] Each port generates: `Rte_Read_<SWC>_<Port>_<Element>(<type>* val)`
- [ ] Type from ARXML port interface → implementation data type → base type
- [ ] Init values from ARXML NONQUEUED-SENDER-COM-SPEC

**3b. Generate typed RTE implementation**
- [ ] Each accessor maps to RTE signal buffer slot (from assembly connector routing)
- [ ] Signal type determines buffer access width (uint8, uint16, sint16, uint32)
- [ ] No casting — buffer is typed per signal

**3c. Update SWC code**
- [ ] Replace all `Rte_Read(SIGNAL_ID, &uint32)` with typed accessor
- [ ] Replace all `Rte_Write(SIGNAL_ID, uint32)` with typed accessor
- [ ] Remove ALL `Com_ReceiveSignal` calls from SWC code (except VehicleState safety pattern)
- [ ] Remove ALL hand-written signal IDs from `App.h` files
- [ ] Compile — zero warnings, zero casts

**3d. Validate**
- [ ] Unit tests pass (update mock to use typed accessors)
- [ ] SIL boots to RUN state
- [ ] SIL-001 passes

### Phase 4: Re-run SIL Suite (1 hour)
- [ ] Run all 18 SIL scenarios
- [ ] Compare to Phase 0 baseline
- [ ] Fix any regressions
- [ ] Target: ≥ 14/18 (current 13 + SIL-010 fixed by typed RTE)

### Phase 5: Virtual Sensor Overlay (1 hour)
- [ ] Codegen reads `taktflow_sil.dbc` as overlay when `--platform=posix`
- [ ] Virtual sensor Com routing generated only for SIL builds
- [ ] Sensor feeder SWCs use typed RTE (auto-generated from SIL overlay)
- [ ] Production build excludes virtual sensor signals entirely

## Feasibility Assessment

| Step | Complexity | Risk | Mitigation |
|------|-----------|------|------------|
| DBC split | Low | Signal renames break codegen | Regen all after rename |
| ARXML merge | Medium | Roundtrip ARXML stale by 12 signals | dbc2arxml updates comm layer |
| Assembly connectors | Medium | ~100 connectors to define manually | Trace from SWC code, script-assisted |
| Typed RTE codegen | High | New Jinja2 templates, new generator | cogu/autosar library for reference |
| SWC code update | Medium | ~50 files to update Rte_Read/Write calls | Mechanical — regex-assisted |
| SIL re-test | Low | Regressions from signal rename | Phase 0 baseline comparison |

**Total estimated effort**: 12 hours
**Dependencies**: None — all tools (cantools, autosar-data, Jinja2) already installed
**Blocked by**: Nothing — can start immediately

## References

- [AUTOSAR System Configuration Workflow (RTA-CAR)](https://rtahotline.etas.com/confluence/display/RH/02+-+System+Configuration+-+RTA-CAR+9.1.0)
- [Export Composition ARXML from AUTOSAR Blockset (MATLAB)](https://www.mathworks.com/help/autosar/ug/export-composition-and-component-arxml-descriptions.html)
- [cogu/autosar Python SWC types](https://deepwiki.com/cogu/autosar/5.1-software-component-types)
- [AUTOSAR SWC Structure (Jerin Jose)](https://medium.com/@jerinjose/an-in-depth-exploration-of-the-structure-of-classic-autosar-software-components-8137e1adc17d)
- [canmatrix DBC↔ARXML converter](https://github.com/ebroecker/canmatrix)
- [EEA COM Expert ARXML editor](https://eeacom-docs.intrepidcs.com/intro/)
- [Understanding ARXML for Communication Networks (Intrepid)](https://www.intrepidcs.net.cn/wp-content/uploads/2019/05/202._Understanding_ARXML_EEA_COM_TD_USA_2019.pdf)
- [CAN DBC File Format Explained (CSS Electronics)](https://www.csselectronics.com/pages/can-dbc-file-database-intro)
