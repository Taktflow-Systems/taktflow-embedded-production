# arxmlgen ARXML Coverage Specification

**Version:** 1.0.0
**Date:** 2026-03-10

## 1. Purpose

This document specifies exactly which AUTOSAR ARXML elements arxmlgen reads, which it
ignores, and why. This serves as:

- A contract for what ARXML must contain for arxmlgen to produce correct output
- A reference for debugging "missing data" issues
- A scope boundary to prevent feature creep

## 2. ARXML Schema Support

| Property | Value |
|----------|-------|
| AUTOSAR Schema | R4.0 and later (R22-11 / AUTOSAR_00051 tested) |
| Parser | `autosar-data` Python library (v0.14+) |
| File format | `.arxml` (standard AUTOSAR XML) |
| Multi-file | Yes — multiple ARXML files merged into single model |
| Encoding | UTF-8 |

## 3. Element Coverage Matrix

### Legend

| Status | Meaning |
|--------|---------|
| **READ** | Parsed and used in code generation |
| **PARTIAL** | Some attributes read, others ignored |
| **NOTED** | Presence detected but not used for generation (e.g., warnings) |
| **IGNORED** | Not read at all |

### 3.1 System Topology

| ARXML Element | Status | Attributes Read | Used By |
|---------------|--------|----------------|---------|
| `SYSTEM` | READ | SHORT-NAME | Project model name |
| `ECU-INSTANCE` | READ | SHORT-NAME | ECU identification |
| `COMM-CONTROLLER` (CAN) | READ | SHORT-NAME | ECU ↔ CAN cluster binding |
| `CAN-CLUSTER` | READ | SHORT-NAME, BAUDRATE | Cluster identification |
| `PHYSICAL-CHANNEL` | READ | SHORT-NAME | Frame triggering container |
| `COMM-CONNECTOR` | PARTIAL | ECU ref, controller ref | ECU ↔ channel binding |

### 3.2 Communication (IPdus, Signals, Frames)

| ARXML Element | Status | Attributes Read | Used By |
|---------------|--------|----------------|---------|
| `I-SIGNAL-I-PDU` | READ | SHORT-NAME, LENGTH | PDU model (name, DLC) |
| `I-SIGNAL-TO-I-PDU-MAPPING` | READ | I-SIGNAL-REF, PACKING-BYTE-ORDER, START-POSITION | Signal ↔ PDU binding, bit layout |
| `I-SIGNAL` | READ | SHORT-NAME, I-SIGNAL-TYPE, INIT-VALUE, LENGTH | Signal model |
| `SYSTEM-SIGNAL` | READ | SHORT-NAME, COMPU-METHOD-REF, UNIT-REF | Physical attributes |
| `FRAME` | READ | SHORT-NAME, FRAME-LENGTH | DLC |
| `CAN-FRAME` | READ | (same as FRAME) | CAN-specific frame |
| `FRAME-TRIGGERING` | READ | FRAME-REF, IDENTIFIER | CAN ID assignment |
| `I-PDU-TRIGGERING` | READ | I-PDU-REF | PDU ↔ frame binding |
| `PDU-TO-FRAME-MAPPING` | READ | PDU-REF, FRAME-REF, PACKING-BYTE-ORDER, START-POSITION | PDU ↔ frame layout |
| `I-PDU-PORT` | PARTIAL | COMM-DIRECTION | TX/RX direction per ECU |
| `I-PDU-GROUP` | IGNORED | — | Group-based enable/disable not supported |

### 3.3 Data Types

| ARXML Element | Status | Attributes Read | Used By |
|---------------|--------|----------------|---------|
| `SW-BASE-TYPE` | READ | SHORT-NAME, BASE-TYPE-SIZE, BASE-TYPE-ENCODING | C type mapping |
| `IMPLEMENTATION-DATA-TYPE` | READ | SHORT-NAME, CATEGORY, BASE-TYPE-REF | Signal data type |
| `COMPU-METHOD` | READ | SHORT-NAME, CATEGORY, COMPU-INTERNAL-TO-PHYS | Scaling (LINEAR), enums (TEXTTABLE) |
| `COMPU-SCALE` | READ | LOWER-LIMIT, UPPER-LIMIT, COMPU-NUMERATOR, COMPU-DENOMINATOR | Factor, offset |
| `COMPU-CONST` | READ | VT (text value) | Enum value names |
| `UNIT` | PARTIAL | SHORT-NAME | Physical unit string |
| `DATA-CONSTR` | IGNORED | — | Constraint enforcement not in scope |
| `APPLICATION-DATA-TYPE` | IGNORED | — | Only ImplementationDataType used |
| `RECORD-LAYOUT` | IGNORED | — | Memory layout not generated |

### 3.4 Software Components

| ARXML Element | Status | Attributes Read | Used By |
|---------------|--------|----------------|---------|
| `APPLICATION-SW-COMPONENT-TYPE` | READ | SHORT-NAME, PORTS, INTERNAL-BEHAVIORS | SWC model |
| `P-PORT-PROTOTYPE` | READ | SHORT-NAME, PROVIDED-INTERFACE-TREF | P-port (write) |
| `R-PORT-PROTOTYPE` | READ | SHORT-NAME, REQUIRED-INTERFACE-TREF | R-port (read) |
| `SENDER-RECEIVER-INTERFACE` | READ | SHORT-NAME, DATA-ELEMENTS | S/R interface |
| `VARIABLE-DATA-PROTOTYPE` | READ | SHORT-NAME, TYPE-TREF | Data element type |
| `SWC-INTERNAL-BEHAVIOR` | READ | SHORT-NAME, RUNNABLES, EVENTS | Behavior model |
| `RUNNABLE-ENTITY` | READ | SHORT-NAME, CAN-BE-INVOKED-CONCURRENTLY, DATA-READ-ACCESSS, DATA-WRITE-ACCESSS | Runnable model |
| `DATA-READ-ACCESS` | READ | SHORT-NAME, ACCESSED-VARIABLE | Read port binding |
| `DATA-WRITE-ACCESS` | READ | SHORT-NAME, ACCESSED-VARIABLE | Write port binding |
| `TIMING-EVENT` | READ | SHORT-NAME, PERIOD, DISABLED-MODE-IREF → RUNNABLE-REF | Periodic scheduling |
| `INIT-EVENT` | READ | SHORT-NAME, START-ON-EVENT-REF → RUNNABLE-REF | Init trigger |
| `SW-COMPONENT-PROTOTYPE` | IGNORED | — | Composition not generated |
| `ASSEMBLY-SW-CONNECTOR` | IGNORED | — | Port connections not used |
| `DELEGATION-SW-CONNECTOR` | IGNORED | — | Not in scope |
| `COMPOSITION-SW-COMPONENT-TYPE` | IGNORED | — | Flat SWC list sufficient |
| `SERVICE-SW-COMPONENT-TYPE` | IGNORED | — | BSW services not generated |
| `PARAMETER-SW-COMPONENT-TYPE` | IGNORED | — | Calibration not in scope |

### 3.5 ECU Configuration (ECUC)

| ARXML Element | Status | Notes |
|---------------|--------|-------|
| `ECUC-VALUE-COLLECTION` | IGNORED | arxmlgen uses project.yaml instead of ECUC |
| `ECUC-MODULE-CONFIGURATION-VALUES` | IGNORED | Per-module ECUC params not parsed |
| `ECUC-CONTAINER-VALUE` | IGNORED | — |
| `ECUC-PARAMETER-VALUE` | IGNORED | — |

**Rationale:** ECUC values are the core of professional tool configuration (DaVinci, tresos).
arxmlgen replaces this with `project.yaml` + sidecar for simplicity. When migrating to a
professional tool, the ECUC is configured in the GUI and replaces our YAML config.

### 3.6 E2E Protection

| ARXML Element | Status | Attributes Read | Used By |
|---------------|--------|----------------|---------|
| `ADMIN-DATA` / `SDGS` annotations | READ | E2E marker values | E2E flag on PDU/signal |
| `END-TO-END-PROTECTION-SET` | NOTED | Not standard in our ARXML | Falls back to annotation |
| `E2E-PROFILE-CONFIGURATION` | NOTED | If present, profile type extracted | E2E profile selection |

**Note:** Our `dbc2arxml.py` encodes E2E protection as ARXML annotations (ADMIN-DATA/SDGS)
on IPdus. The reader checks both standard E2E elements and annotation markers.

### 3.7 Not Supported (Intentionally Out of Scope)

| AUTOSAR Domain | Why Ignored |
|----------------|-------------|
| **OS Configuration** (OsTask, OsAlarm, OsResource) | Too platform-specific. We use a custom bare-metal scheduler, not AUTOSAR OS. |
| **NvM Block Configuration** | Storage layout is platform-dependent. Not generated. |
| **Dcm / UDS Configuration** | Diagnostic service config is hand-written due to complexity. |
| **BswM Mode Management** | Mode arbitration rules too complex for template generation. |
| **EcuM / Startup Sequence** | Boot sequence is platform-specific (STM32 vs TMS570 vs POSIX). |
| **FlexRay / LIN / Ethernet / SOME-IP** | v1.0 supports CAN only. Architecture is transport-agnostic — adding new bus types requires a reader backend + generator, not a rewrite. See architecture.md §10 for SDV extensibility roadmap. |
| **Calibration (XCP/CCP)** | Not in scope for code generation. |
| **Variant Handling (POST-BUILD, PRE-COMPILE)** | Single variant assumed. |

## 4. ARXML Package Structure (Reference)

Our `dbc2arxml.py` generates ARXML with this package layout. The reader discovers
elements by type, not by path, so other layouts also work.

```
/AUTOSAR/
  Platform/
    BaseTypes/              ← 7 SwBaseType (boolean, uint/sint 8/16/32)
    ImplementationDataTypes/← 7 ImplementationDataType

/Taktflow/                  (or /<ProjectName>/)
  System/
    TaktflowSystem          ← System element
  ECUs/
    <ECU>/                  ← EcuInstance + CommController + CommConnector
  Communication/
    Signals/                ← ISignal (one per DBC signal)
    SystemSignals/          ← SystemSignal (one per DBC signal)
    IPdus/
      <PDU>/                ← ISignalIPdu + ISignalToIPduMappings
    Frames/                 ← Frame elements
    CAN_500k/               ← CanCluster, PhysicalChannel, FrameTriggerings
  Interfaces/
    SRI_<Signal>/           ← SenderReceiverInterface + DataElement
  SWCs/
    <ECU>/
      <SWC>/                ← ApplicationSwComponentType + Behavior + Events
  DataTypes/
    CompuMethods/           ← CompuMethod (LINEAR, TEXTTABLE)
  E2E/
    E2E_<PDU>/              ← E2E annotations (if separate package)
```

### Element Counts (Taktflow Reference System)

| Package Path | Element Count | Description |
|---|---|---|
| `/AUTOSAR/Platform/BaseTypes/` | 7 | SwBaseType (uint/sint 8/16/32, boolean) |
| `/AUTOSAR/Platform/ImplementationDataTypes/` | 7 | ImplementationDataType |
| `/Taktflow/System/` | 1 | System |
| `/Taktflow/ECUs/` | 24 | 8 ECUs × 3 elements (instance + controller + connector) |
| `/Taktflow/Communication/Signals/` | 163 | ISignal |
| `/Taktflow/Communication/SystemSignals/` | 163 | SystemSignal |
| `/Taktflow/Communication/IPdus/` | 197 | ISignalIPdu + mappings |
| `/Taktflow/Communication/Frames/` | 69 | Frame + triggerings |
| `/Taktflow/Communication/CAN_500k/` | 232 | Cluster + channel + triggerings |
| `/Taktflow/Interfaces/` | 170 | 85 S/R interfaces × 2 (interface + data element) |
| `/Taktflow/SWCs/` | 599 | 48 SWCs with ports, behaviors, runnables, events |
| `/Taktflow/DataTypes/` | 26 | CompuMethods |
| **Total** | **1689** | Identifiable elements |

## 5. Minimum ARXML Requirements

For arxmlgen to produce any useful output, the ARXML must contain at minimum:

### Tier 1: Communication Only (Com_Cfg, CanIf_Cfg, PduR_Cfg)

| Required Elements | Why |
|---|---|
| At least 1 `ECU-INSTANCE` | Identifies the target ECU |
| At least 1 `I-SIGNAL-I-PDU` with signal mappings | Defines PDUs and signals |
| `FRAME-TRIGGERING` with CAN IDs | Maps PDUs to CAN frames |
| ECU-to-PDU routing (via controllers/connectors) | Determines TX vs RX per ECU |

### Tier 2: + RTE (Rte_Cfg, typed wrappers)

All of Tier 1, plus:

| Required Elements | Why |
|---|---|
| `SENDER-RECEIVER-INTERFACE` per signal | Defines port interfaces for typed wrappers |

### Tier 3: + SWC Skeletons

All of Tier 2, plus:

| Required Elements | Why |
|---|---|
| `APPLICATION-SW-COMPONENT-TYPE` | Defines SWC names and ports |
| `SWC-INTERNAL-BEHAVIOR` with runnables | Defines function signatures |
| `TIMING-EVENT` or `INIT-EVENT` | Defines scheduling triggers |

### Tier 4: + E2E

All of Tier 1, plus:

| Required Elements | Why |
|---|---|
| E2E annotations on IPdus | Identifies protected messages |

## 6. Cross-Reference Resolution

arxmlgen validates all ARXML cross-references during reading. Unresolved references
produce warnings but do not block generation (graceful degradation).

| Reference Type | From → To | Behavior if Missing |
|---|---|---|
| ISignal → SystemSignal | Signal → physical attributes | Warning: signal treated as IDENTICAL scaling |
| SystemSignal → CompuMethod | Signal → scaling/enum | Warning: no physical conversion |
| ISignalToIPduMapping → ISignal | PDU → signal binding | Error: signal dropped from PDU |
| FrameTriggering → Frame | Triggering → DLC | Warning: DLC defaults to 8 |
| FrameTriggering → IDENTIFIER | Triggering → CAN ID | Error: PDU has no CAN routing |
| Port → SenderReceiverInterface | SWC port → interface | Warning: port dropped |
| DataElement → ImplementationDataType | Element → C type | Warning: defaults to uint32_t |
| TimingEvent → RunnableEntity | Event → function | Warning: runnable has no trigger |
| BaseType → encoding | IDT → SwBaseType | Warning: defaults to unsigned |

## 7. Versioning and Forward Compatibility

| ARXML Version | Support Level |
|---|---|
| AUTOSAR 4.0 (R4.0.3) | Tested |
| AUTOSAR 4.2 | Expected compatible |
| AUTOSAR 4.3 (R19-11) | Expected compatible |
| AUTOSAR 4.5 (R20-11) | Expected compatible |
| AUTOSAR R22-11 (AUTOSAR_00051) | Primary target, fully tested |
| AUTOSAR R23-11 | Expected compatible (autosar-data tracks latest) |
| AUTOSAR Adaptive | Not supported (Classic Platform only) |

**Forward compatibility strategy:** arxmlgen reads by element type, not schema version.
New optional attributes in newer schemas are silently ignored. Required structural changes
(e.g., renamed elements) would require a reader update.
