# Lessons Learned: Phase 0 DBC Audit (2026-03-20)

## Lesson 1: DBC Audit Must Cover 12 Checks, Not 3

**Context**: Initial Phase 0 audit only checked 3 items (cycle time, E2E uniqueness, ASIL presence). Research showed industry expects 12 checks.

**Mistake**: Assumed DBC audit = basic attribute check. Missed event retransmission, bus load, FTTI math proof, signal overlap, start value vs safe state.

**Fix**: Created comprehensive 12-point audit script. Run it every time DBC changes.

**Principle**: Before claiming "audit done", research what industry actually audits. Our intuition covers ~40% of what's needed.

---

## Lesson 2: E2E DataID Must Fit the Field Width

**Context**: Used DataIDs 16-19 for QM messages, but E2E_DataID signal is 4-bit (0-15).

**Mistake**: Assigned DataIDs without checking the physical signal width in the DBC. The DBC attribute `E2E_DataID` has no range constraint — the constraint comes from the signal layout.

**Fix**: Remove E2E from QM messages (don't need it per HARA). Reassigned to fit 0-15.

**Principle**: Every DBC attribute value must be validated against the signal it maps to. An attribute saying "DataID=19" is meaningless if the signal can only hold 0-15.

---

## Lesson 3: FTTI Budget Is a Mathematical Proof, Not a Guess

**Context**: Set cycle times based on "feels right" (10ms, 50ms, 100ms) without proving they meet FTTI.

**Mistake**: `CycleTime × (MaxDeltaCounter+1) + T_react + T_safe` must be <= FTTI. We never calculated this. 5 messages failed.

**Fix**: Formal budget per message. Introduced `E2E_MaxDeltaCounter` as DBC attribute. ASIL D with 50ms FTTI gets MaxDelta=1 (tighter).

**Principle**: Every timing value in the DBC must have a mathematical derivation from HARA FTTI. No arbitrary numbers.

---

## Lesson 4: Event Messages Need Retransmission Strategy

**Context**: Brake_Fault and Motor_Cutoff are ASIL D event-triggered messages with GenMsgCycleTime=0.

**Mistake**: A single CAN error silently drops the frame. No retry. The safety-critical fault report is lost.

**Fix**: Added `GenMsgCycleTimeFast=10ms` and `GenMsgNrOfRepetition=3`. On event, send 3 times at 10ms intervals.

**Principle**: Every ASIL-rated event message MUST have a retransmission strategy. "Fire and forget" is not acceptable for safety.

---

## Lesson 5: DBC-to-HARA Traceability Must Be in the DBC

**Context**: Safety goals existed in docs, DBC existed separately. No link between them.

**Mistake**: A reviewer looking at the DBC cannot tell which safety goal a message satisfies without opening 3 other documents.

**Fix**: Added `Satisfies` BA_ attribute to every ASIL message (e.g., `BA_ "Satisfies" BO_ 256 "SG-001,SG-008";`).

**Principle**: Traceability should be in the artifact itself, not only in external matrices. The DBC should be self-documenting.

---

## Lesson 6: ARXML Must Be the Single Intermediate Representation

**Context**: Our codegen reads DBC directly for E2E parameters, bypassing ARXML. The ARXML only has signal structure, not E2E configuration.

**Mistake**: The traceability chain breaks: DBC → (skip ARXML) → codegen. An assessor expects DBC → ARXML → codegen, with ARXML as the complete system description.

**Fix**: (Upcoming) Add `END-TO-END-PROTECTION-SET` to ARXML with full E2E Profile 1 config per message. Codegen reads from ARXML, not DBC.

**Principle**: Every step in the chain must be complete. If ARXML doesn't contain E2E parameters, it's not a valid system description. No shortcuts.

---

## Lesson 7: Research Before Claiming Done

**Context**: Claimed "Phase 0 PASS" after 3 checks. Research showed 12 checks needed. 40% coverage.

**Mistake**: Confidence without validation. "It looks right" is not "it is right."

**Fix**: For every phase, research industry best practice BEFORE executing. Compare our approach against the research. Close all gaps before claiming done.

**Principle**: Every phase must include: (1) research what industry does, (2) gap analysis against our approach, (3) fix gaps, (4) re-verify, (5) HITL review. This is the standard from now on.

---

## Lesson 8: Every Phase Needs a Lesson Learned

**Context**: Previously, lessons were only written after bugs. Not after process improvements.

**Fix**: From now on, every phase writes lessons learned — what we did right, what we missed, what to do differently next time. This file is the start.

**Principle**: Lessons learned are not just for failures. They capture the evolution of our process. Future sessions start by reading the lessons from previous sessions.
