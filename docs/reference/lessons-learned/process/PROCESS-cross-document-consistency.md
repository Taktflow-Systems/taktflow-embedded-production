# Lessons Learned — Cross-Document Consistency

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-21 to 2026-02-26
**Scope:** Fixing inconsistencies discovered across 60+ engineering documents
**Result:** 10+ inconsistencies fixed, cross-doc review checklist established

---

## 1. CAN Bit Timing: 87.5% vs 80% Sample Point

The CAN bus configuration had two different sample points in different documents:
- HW requirements: 87.5% (7 Tq NTSEG1, 1 Tq NTSEG2, 8 Tq total)
- CAN message matrix: 80% (7 Tq NTSEG1, 2 Tq NTSEG2, 10 Tq total)

Both are valid CAN configurations, but they produce different bit rates from the same prescaler.

**Fix:** Standardized on 80%/10Tq across all documents. Updated ICD, HSI, HW requirements, HW design.

**Lesson:** Bit timing must be defined in exactly one place (CAN message matrix) and referenced everywhere else. Never copy-paste parameters.

---

## 2. CAN IDs Inconsistent Between Matrix and Architecture

The CAN message matrix defined Motor_Current at 0x301, but the SW architecture document referenced it at 0x300 in some diagrams.

**Fix:** Full grep for every CAN ID across all docs. Fixed 4 mismatches.

**Lesson:** CAN IDs are the most-duplicated data in automotive documentation. Use a single authoritative source (the CAN matrix) and validate all references against it.

---

## 3. ACS723 Sensitivity: 400 vs 100 mV/A

The ACS723-20A variant has 100 mV/A sensitivity. Five documents and one test file had 400 mV/A (which is the ACS723-05A variant).

**Impact:** Current calculations were 4x wrong in documentation. Test assertions used the wrong conversion factor.

**Fix:** `grep -r "400.*mV" --include="*.md" --include="*.c"` found all 6 instances. Fixed to 100 mV/A.

**Lesson:** When a datasheet value appears in multiple files, grep for it project-wide after any correction. One wrong value propagates to every copy.

---

## 4. MG996R Servo: 180-Degree, Not 360-Degree

The BOM listed the MG996R as a 360-degree servo. It's actually 180-degree. This affects steering angle calculations.

**Lesson:** Verify BOM entries against the actual datasheet, not the Amazon listing title. Seller descriptions are unreliable.

---

## 5. SC Pin Assignment LED Conflict

The Safety Controller's LED_SYS was assigned to a port that conflicted with another function. Discovered late during bring-up planning.

**Fix:** Moved LED_SYS to port A, added heartbeat LED on port B.

**Lesson:** Pin assignment conflicts surface at integration, not design. Review pin maps against all module requirements before hardware arrives.

---

## 6. Single-Source-of-Truth Principle

Every parameter that appears in more than one document should have exactly one authoritative source. All other documents reference it, never copy it.

| Parameter | Authoritative source | References |
|-----------|---------------------|------------|
| CAN bit timing | CAN message matrix §3 | ICD, HSI, HW design, HW reqs |
| CAN IDs | CAN message matrix §5 | SW architecture, SWR-*, test files |
| Pin assignments | Pin mapping doc | HW design, HSI, ECU SWRs |
| Sensor specs | BOM | SWRs, test files, FMEA |

**Lesson:** If you find a value in 2+ places, one of them is wrong. Establish the authority and make all others references.

---

## 7. Cross-Doc Review Checklist

After finding 10 inconsistencies, we established a review checklist:

1. Grep all CAN IDs and verify against matrix
2. Grep all sensor part numbers and verify against BOM
3. Grep all pin assignments and verify against pin map
4. Grep all timing values (ms, Hz, kbps) and verify against source
5. Grep all ASIL ratings and verify against HARA

**Lesson:** Per-document review catches content errors. Cross-document review catches consistency errors. Both are needed.

---

## 8. Companion Docs Drift into Duplication Without Clear Ownership

**Date:** 2026-03-03

Hardware bringup had two docs — a high-level plan (703 lines) and a step-by-step workbook (1443 lines). The workbook was created as a "companion" but both grew independently, covering the same phases with similar detail. Phases 0-7 had full wiring instructions, verification tables, and solder steps in BOTH files.

**Fix:** Assigned clear roles — plan = strategy/overview (context, estimates, risk, safety rules, brief phase summaries), workbook = execution (all step-by-step assembly). Plan went from 703 to 294 lines. Cross-references added in both directions.

**Lesson:** When creating a companion doc, define ownership boundaries upfront — which file owns what content. "Plan = why/when, Workbook = how" prevents duplication. If both files can answer the same question, one of them shouldn't.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Bit timing | Define once in CAN matrix, reference everywhere else |
| CAN IDs | Most-duplicated data — grep project-wide after any change |
| Sensor specs | Verify against datasheet, not Amazon listing |
| Pin maps | Review against all modules before hardware arrives |
| Single source | If a value exists in 2 places, one is wrong |
| Cross-doc review | Per-doc review catches content; cross-doc review catches consistency |
| Companion docs | Define ownership boundaries upfront — plan=why/when, workbook=how |
