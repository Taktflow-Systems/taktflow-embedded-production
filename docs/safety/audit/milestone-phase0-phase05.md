---
document_id: MILESTONE-P0-P05
title: "Milestone: Phase 0 (DBC Audit) + Phase 0.5 (ARXML E2E)"
version: "1.0"
status: REVIEW
date: 2026-03-20
---

# Milestone: Phase 0 + Phase 0.5 Complete

## Scope

Phase 0: DBC audit against HARA/ISO 26262 — 12-point industry-standard check.
Phase 0.5: ARXML generation with END-TO-END-PROTECTION-SET and full traceability.

## Artifacts Produced

| Artifact | Path | Description |
|----------|------|-------------|
| DBC (updated) | `gateway/taktflow_vehicle.dbc` | 37 msgs, 174 sigs, 21 E2E DataIDs (16 ASIL + 5 QM without E2E) |
| ARXML (regenerated) | `arxml/TaktflowSystem.arxml` | 920KB, 2418 elements, 0 ref errors, 20 E2E protections |
| Audit findings | `docs/safety/audit/phase0-dbc-audit-findings.md` | 7 findings, all resolved |
| Lessons learned | `docs/lessons-learned/embedded/phase0-dbc-audit-2026-03-20.md` | 10 lessons |
| Rewrite plan | `docs/plans/plan-firmware-rewrite.md` | 7 phases, HARA-driven |

## Phase 0 DBC Audit Results

| Check | Result |
|-------|--------|
| 1. E2E DataID uniqueness | PASS (16 IDs, 0-15) |
| 2. E2E DataID 4-bit range | PASS (all 0-15) |
| 3. ASIL messages have E2E | PASS (Motor_Temperature deviation documented) |
| 4. GenSigStartValue for ASIL signals | PASS (0 missing) |
| 5. FTTI budget per message | PASS (MaxDelta=1 for ASIL D tight FTTI) |
| 6. Bus load < 70% | PASS (24.4%) |
| 7. Signal overlap | PASS (DTC_Broadcast fixed) |
| 8. DLC consistency | PASS |
| 9. Signal name uniqueness | PASS (0 duplicates) |
| 10. Event retransmission | PASS (Brake_Fault, Motor_Cutoff: fast=10ms, reps=3) |
| 11. E2E_Profile / MaxDeltaCounter in DBC | PASS (BA_ attributes added) |
| 12. DBC-to-HARA traceability (Satisfies) | PASS (17 messages linked) |

## Phase 0.5 ARXML Results

| Check | Result |
|-------|--------|
| END-TO-END-PROTECTION-SET generated | PASS (20 protections) |
| Per-protection E2E profile (DataID, MaxDelta, CRC/Counter offsets) | PASS |
| Per-protection traceability (ASIL, Satisfies, CAN_ID, CycleMS, I-PDU path) | PASS |
| ARXML validation (reference errors) | PASS (0 errors) |
| ARXML element count | 2418 identifiable elements |

## DBC Changes Summary

| Message | Change | Reason | Traces To |
|---------|--------|--------|-----------|
| SC_Status (0x013) | Added E2E_DataID=0 | ASIL D requires E2E | SG-008, HE-012 |
| Motor_Temperature (0x302) | Removed E2E | ASIL A, timeout sufficient | SG-006, HE-008 (deviation) |
| ICU/TCU/BCM Heartbeat | Removed E2E | QM, not required by HARA | — |
| Battery_Status (0x303) | Removed E2E | QM, not required by HARA | — |
| Steering_Status (0x200) | Cycle 20→50ms | FTTI 200ms compliance | SG-003, HE-003/HE-004 |
| Brake_Status (0x201) | Cycle 20→50ms | FTTI 200ms compliance | SG-004, HE-005 |
| Motor_Cutoff_Req (0x211) | Cycle 10→50ms | FTTI 200ms compliance | SG-008, HE-014 |
| SC_Status (0x013) | Cycle 500→100ms | FTTI 500ms compliance | SG-008, HE-012 |
| Vehicle_State (0x100) | MaxDelta 2→1 | FTTI 50ms compliance | SG-001, HE-001 |
| Torque_Request (0x101) | MaxDelta 2→1 | FTTI 50ms compliance | SG-001, HE-001 |
| Brake_Fault (0x210) | Added fast=10ms, reps=3 | ASIL D event retransmission | SG-004, HE-005 |
| Motor_Cutoff_Req (0x211) | Added fast=10ms, reps=3 | ASIL D event retransmission | SG-008, HE-014 |
| DTC_Broadcast (0x500) | Number 24→16 bit | Signal overlap fix | Signal integrity |
| All ASIL messages | Added Satisfies attribute | DBC-to-HARA traceability | All SGs |
| All messages | Added E2E_MaxDeltaCounter | FTTI compliance proof | All SGs with FTTI |
| All messages | Added E2E_Profile default P01 | Tooling traceability | E2E specification |
| All event ASIL msgs | Added GenMsgCycleTimeFast, NrOfRepetition | Event retransmission | ASIL D safety |

## FTTI Budget Proof

| Message | CycleMs | MaxDelta | T_detect | T_react | T_safe | T_total | FTTI | Margin |
|---------|---------|----------|----------|---------|--------|---------|------|--------|
| EStop_Broadcast | 10 | 2 | 30 | 10 | 20 | 60 | 100 | 40ms |
| CVC_Heartbeat | 50 | 2 | 150 | 10 | 20 | 180 | 500 | 320ms |
| FZC_Heartbeat | 50 | 2 | 150 | 10 | 20 | 180 | 500 | 320ms |
| RZC_Heartbeat | 50 | 2 | 150 | 10 | 20 | 180 | 500 | 320ms |
| SC_Status | 100 | 2 | 300 | 10 | 20 | 330 | 500 | 170ms |
| Vehicle_State | 10 | **1** | 20 | 10 | 20 | **50** | 50 | **0ms** |
| Torque_Request | 10 | **1** | 20 | 10 | 20 | **50** | 50 | **0ms** |
| Steer_Command | 10 | 2 | 30 | 10 | 20 | 60 | 100 | 40ms |
| Brake_Command | 10 | 2 | 30 | 10 | 20 | 60 | 100 | 40ms |
| Steering_Status | 50 | 2 | 150 | 10 | 20 | 180 | 200 | 20ms |
| Brake_Status | 50 | 2 | 150 | 10 | 20 | 180 | 200 | 20ms |
| Motor_Cutoff_Req | 50 | 2 | 150 | 10 | 20 | 180 | 200 | 20ms |
| Motor_Status | 100 | 2 | 300 | 10 | 20 | 330 | 500 | 170ms |
| Motor_Current | 100 | 2 | 300 | 10 | 20 | 330 | 1000 | 670ms |

**Note**: Vehicle_State and Torque_Request have 0ms margin (T_total = FTTI exactly). This is by design — MaxDelta=1 is the tightest possible for 10ms cycle at 50ms FTTI. Any additional jitter margin must come from reducing T_react or T_safe (requires WCET analysis in Phase 6).

---

<!-- HITL-LOCK START:MILESTONE-P0-P05-REVIEW -->
## HITL Review (An Dao) — Required Before Phase 1

### Phase 0 DBC Audit

**Q1**: Motor_Temperature (0x302) ASIL A — E2E removed, using timeout-based detection instead. 1000ms FTTI allows 10+ missed frames before fault detection via Com RX deadline monitoring.
- [x] **APPROVED** (An Dao, 2026-03-20): Timeout-based detection is sufficient for ASIL A

**Q2**: Vehicle_State (0x100) and Torque_Request (0x101) — MaxDeltaCounter reduced from 2 to 1. This means a single missed frame + one more triggers E2E failure. More sensitive to CAN jitter.
- [x] **APPROVED** (An Dao, 2026-03-20): OK for bench. Tighter detection correct for ASIL D.

**Q3**: DTC_Broadcast Number field reduced from 24-bit to 16-bit to fix signal overlap. ISO 14229 UDS DTCs are 3 bytes (24-bit) but standard DTCs (P0000-P3FFF, C0000-C3FFF, etc.) fit in 16 bits. OEM-specific DTCs in the upper byte are lost.
- [x] **APPROVED** (An Dao, 2026-03-20): Our DTCs are all in 16-bit range. OK for bench.

**Q4**: Cycle time changes — Steering_Status/Brake_Status at 50ms (was 20ms), Motor_Cutoff at 50ms (was 10ms), SC_Status at 100ms (was 500ms). All derived from FTTI budgets.
- [x] **APPROVED** (An Dao, 2026-03-20): 50ms is acceptable. SC_Status at 100ms justified — SC monitors CVC at 50ms (critical path), CVC monitors SC at 100ms (secondary path, 500ms FTTI gives 170ms margin).

**Q5**: Event retransmission — Brake_Fault and Motor_Cutoff now have GenMsgCycleTimeFast=10ms, NrOfRepetition=3. On event trigger, send 3 times at 10ms intervals to survive CAN errors.
- [x] **APPROVED** (An Dao, 2026-03-20)

### Phase 0.5 ARXML

**Q6**: ARXML now contains END-TO-END-PROTECTION-SET with 20 protections. Each has structured E2E profile parameters + ADMIN-DATA traceability (ASIL, Satisfies, CAN_ID, CycleMS, I-PDU path).
- [x] **APPROVED** (An Dao, 2026-03-20)

**Q7**: I-SIGNAL-I-PDUS reference not supported under END-TO-END-PROTECTION in AUTOSAR R22-11. I-PDU link stored in ADMIN-DATA/SDGS as custom GID="I-PDU-PATH" instead.
- [x] **APPROVED** (An Dao, 2026-03-20): Custom metadata acceptable for our codegen.

### Process

**Q8**: Mandatory 7-step process per phase (research → gap analysis → execute → verify → lesson learned → HITL review → next phase). Applied from Phase 0 onwards.
- [x] **APPROVED** (An Dao, 2026-03-20)

**Sign-off**: An Dao — 2026-03-20 — Phase 0 + 0.5 APPROVED. Proceed to Phase 1.
<!-- HITL-LOCK END:MILESTONE-P0-P05-REVIEW -->
