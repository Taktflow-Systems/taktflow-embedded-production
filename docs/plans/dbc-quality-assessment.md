# DBC Quality Assessment: Ours vs Production Grade

**Date**: 2026-03-18

## 1. Signal Naming Convention

### Production grade (OEM standard)
- **Globally unique**: every signal name unique across entire DBC
- **Qualified**: `<MsgPrefix>_<SignalName>` (e.g., `VehSt_VehicleState`)
- **No bare duplicates**: `E2E_DataID` never appears twice
- **32-char limit**: DBC spec allows 1-32 characters
- **Underscore-separated PascalCase**: `MotSt_MotorSpeed_RPM`

### Ours (current)
- **12 duplicate signal names** across messages
- `E2E_DataID` appears in 20 messages — impossible to reference unambiguously
- `VehicleState` in 3 messages, `OperatingMode` in 3, `ECU_ID` in 6
- No message prefix on signals

### Verdict: FAIL — needs qualification

---

## 2. Message Topology

### Production grade
- **One sender per message**: DBC enforces `BO_ <id> <name>: <dlc> <sender>`
- **Clear priority scheme**: lower CAN ID = higher priority
- **Domain grouping**: 0x0xx safety, 0x1xx powertrain, 0x2xx chassis, 0x3xx body, 0x5xx diagnostic
- **No test infrastructure**: virtual sensors, tester nodes in separate DBC

### Ours (current)
- One sender per message: **PASS**
- Priority scheme: 0x001 E-Stop (highest), 0x01x heartbeats, 0x1xx commands, 0x2xx-3xx feedback: **PASS**
- Domain grouping: reasonable but not strict: **PARTIAL**
- Test infrastructure mixed: 0x600/0x601 virtual sensors + 0x7xx UDS in same DBC: **FAIL**

### Verdict: PARTIAL — need to split SIL/diag out

---

## 3. Message Attributes

### Production grade
- `GenMsgCycleTime`: every cyclic message has a defined cycle time
- `GenMsgSendType`: `cyclic`, `event`, `cyclicIfActive`, `noMsgSendType`
- `ASIL`: safety classification per message (QM, A, B, C, D)
- `E2E_DataID`: E2E protection profile per safety-relevant message
- `GenSigStartValue`: initial/default value per signal

### Ours (current)
- `GenMsgCycleTime`: **PASS** — defined for all 26 production messages
- `GenMsgSendType`: **PASS** — cyclic/event correctly assigned
- `ASIL`: **PASS** — every message has ASIL rating (QM to D)
- `E2E_DataID`: **PASS** — all E2E-protected messages have DataID
- `GenSigStartValue`: **FAIL** — not defined (caused the SteerAngleCmd offset=−45 boot issue)

### Verdict: MOSTLY PASS — add `GenSigStartValue` for all signals

---

## 4. Node (ECU) Definitions

### Production grade
- **Only production ECUs**: no test tools, no simulators
- **Comments with ASIL and MCU**: `CM_ BU_ CVC "Central Vehicle Computer — STM32F446RE, ASIL D"`
- **Separate DBC for test tools**: Tester, Plant_Sim, Calibration

### Ours (current)
- 9 nodes: 7 production + Tester + Plant_Sim: **FAIL** (test nodes in production DBC)
- Comments with ASIL and MCU: **PASS** — every node has description

### Verdict: FAIL — remove Tester and Plant_Sim from production DBC

---

## 5. Signal Receiver Lists

### Production grade
- **Explicit receivers**: every signal lists exactly which ECUs receive it
- **No broadcast-to-all**: only listed ECUs see the signal (gateway filtering)
- **Consistent**: if ECU X receives message M, it receives ALL signals in M

### Ours (current)
- Explicit receivers: **PASS** — every signal has receiver list
- Some inconsistencies: `EStop_Active` receivers differ from `EStop_Source` receivers (ICU missing from Source): **MINOR**
- CVC added as receiver of its own EStop (SIL hack): **FAIL** — sender should not be receiver

### Verdict: PARTIAL — clean up receiver inconsistencies, remove self-reception

---

## 6. Value Descriptions (VAL_)

### Production grade
- Every enum signal has `VAL_` definitions
- Values match firmware constants exactly
- No magic numbers in firmware — all from DBC

### Ours (current)
- Enum signals have VAL_: **PASS** — OperatingMode, BrakeMode, Direction, etc.
- Match firmware: **PARTIAL** — some mismatch (CVC had SELF_TEST=1 which wasn't in DBC)

### Verdict: PARTIAL — audit VAL_ against firmware enums

---

## 7. Documentation (CM_)

### Production grade
- Every message has a comment: purpose, cycle time, ASIL
- Every signal has a comment: physical meaning, range, accuracy

### Ours (current)
- Message comments: **PASS** — all 26 production messages documented
- Signal comments: **FAIL** — no signal-level comments

### Verdict: PARTIAL — add signal comments

---

## Summary

| Criterion | Production Grade | Ours | Status |
|-----------|-----------------|------|--------|
| Signal naming (unique) | Qualified, globally unique | 12 duplicates | FAIL |
| Test infrastructure | Separate DBC | Mixed in | FAIL |
| Message attributes | GenMsgCycleTime, ASIL, E2E | Present | PASS |
| Signal init values | GenSigStartValue | Missing | FAIL |
| Nodes | Production only | +Tester, +Plant_Sim | FAIL |
| Receiver lists | Consistent | Minor gaps | PARTIAL |
| Value descriptions | All enums defined | Present | PASS |
| Message comments | All documented | Present | PASS |
| Signal comments | All documented | Missing | FAIL |

**4 PASS, 4 FAIL, 1 PARTIAL.**

## Phase 1 Deliverables

1. **Split**: `taktflow_vehicle.dbc` (26 msgs, 7 nodes) + `taktflow_sil.dbc` + `taktflow_diag.dbc`
2. **Qualify**: all 158 production signals get `<MsgPrefix>_<Signal>` names
3. **Add**: `GenSigStartValue` for all signals (0 for unsigned, 0 for signed = neutral)
4. **Add**: signal-level `CM_` comments
5. **Remove**: CVC self-reception of EStop_Broadcast
6. **Verify**: 0 duplicate signal names, `cantools` loads clean

## References

- [Vector CANdb++ Manual](https://cdn.vector.com/cms/content/products/candb/Docs/CANdb_Manual_EN.pdf)
- [CAN DBC File Explained (CSS Electronics)](https://www.csselectronics.com/pages/can-dbc-file-database-intro)
- [Advanced DBC File Format (Embien)](https://www.embien.com/automotive-insights/advanced-dbc-file-format-concepts)
- [DBC Format Specification (CANpy)](https://github.com/stefanhoelzl/CANpy/blob/master/docs/DBC_Specification.md)
- [DBC Introduction (Open Vehicles)](https://docs.openvehicles.com/en/latest/components/vehicle_dbc/docs/dbc-primer.html)
