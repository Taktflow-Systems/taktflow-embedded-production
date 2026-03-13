# Lessons Learned — HITL Review Methodology

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-25 to 2026-03-01
**Scope:** Developing and applying Human-in-the-Loop (HITL) review process for AI-generated engineering documents
**Result:** 443+ HITL comments across 29 documents, reproducible review methodology

---

## 1. HITL-LOCK Marker Standard

AI must never edit, reformat, move, or delete human review comments. The HITL-LOCK markers enforce this:

```markdown
<!-- HITL-LOCK START:COMMENT-BLOCK-SYS001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-26:** ...
<!-- HITL-LOCK END:COMMENT-BLOCK-SYS001 -->
```

**Why:** AI assistants reformat, rephrase, or "improve" text during edits. HITL-LOCK prevents this for human-authored review content.

**Lesson:** Human review content is immutable. Protect it with markers that the AI workflow respects.

---

## 2. Why / Tradeoff / Alternative Structure

Every HITL review comment follows a consistent structure:
1. **Assessment**: Is the requirement correct? Is the ASIL rating justified?
2. **Why**: The engineering rationale behind the design choice
3. **Tradeoff**: What was sacrificed for this choice
4. **Alternative**: What else was considered and why it was rejected

**Why this structure:**
- Reproducible: Any reviewer can apply the same framework
- Auditable: ASPICE SUP.4 (Joint Review) expects documented rationale
- Educational: AI learns from the pattern and improves future outputs
- Defensible: A TUV assessor can follow the reasoning chain

**Lesson:** Structured review comments are 10x more valuable than "LGTM" or "looks good". The structure IS the review.

---

## 3. Review Date in Every Comment

Format: `Reviewed: YYYY-MM-DD`

**Why:** Traceability. When a requirement changes, you need to know which review comments are stale (reviewed before the change).

**Lesson:** Undated review comments are worthless for configuration management.

---

## 4. HITL Comments Only After Discussion

Never pre-write HITL comments. The human must discuss and understand the requirement before writing the review.

**Anti-pattern:** AI generates the requirement AND the review comment → no actual human review occurred.

**Lesson:** The value of HITL is the discussion, not the comment. The comment is evidence that discussion happened.

---

## 5. Per-Requirement Lessons Learned

Every system requirement (SYS-NNN) that undergoes HITL review discussion gets its own lessons-learned file: `SYS-NNN-<short-title>.md`.

**Why:** Review discussions surface engineering insights that don't belong in the requirement text (e.g., "DMA complexity is NOT a valid argument against SPI1+SPI2").

**Lesson:** Requirements capture the WHAT. Lessons learned capture the WHY NOT (rejected alternatives) and the CORRECTION (misconceptions fixed during review).

---

## 6. 443 Comments Across 29 Docs in One Pass

The bulk review pass covered:
- System requirements (SYS-001 through SYS-041)
- CAN message matrix (9 message groups)
- Stakeholder requirements (STK-001 through STK-031)
- Process playbook (7 sections)

**Pattern:** Review by document, not by topic. Complete one document before moving to the next.

**Lesson:** Batch review is more efficient than incremental review. Context loading is expensive — minimize context switches.

---

## 7. Consolidated Lessons Folder > Scattered Files

Initially, lessons were scattered: `plans/lessons-learned-security-hardening.md`, `aspice/system/lessons-learned/`, etc.

**Problem:** No one finds lessons if they're in 5 different folders.

**Fix:** Single consolidated folder: `docs/reference/lessons-learned/` with naming convention:
- `PROCESS-*` for workflow/tooling topics
- `SYS-NNN-*` for requirement review discussions

**Lesson:** Lessons learned are only useful if they're findable. One folder, one index, one naming convention.

---

## 8. Human-AI Co-Development Improvement Cycle

The HITL review process creates a feedback loop:
1. AI generates engineering content (requirements, architecture, code)
2. Human reviews with structured comments (Why/Tradeoff/Alternative)
3. AI corrects misconceptions (e.g., DMA complexity argument removed)
4. Lesson is captured in lessons-learned file
5. AI reads lessons-learned in future sessions → avoids repeating the mistake

**Example:** SYS-001 review: AI initially argued SPI2 adds "DMA/driver complexity." Human corrected: SPI2 config is trivial copy-paste. AI will not repeat this argument.

**Lesson:** This is the core value proposition of human-AI co-development: iterative improvement through documented corrections.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| HITL-LOCK | Human review content is immutable — protect with markers |
| Review structure | Why/Tradeoff/Alternative makes reviews reproducible and auditable |
| Review dates | Undated comments are worthless for configuration management |
| Discussion first | The value is the discussion, not the comment |
| Per-requirement lessons | Capture WHY NOT and CORRECTIONS, not just WHAT |
| Batch review | Complete one document before switching context |
| Single folder | Lessons are only useful if findable — one folder, one index |
| Improvement cycle | Documented corrections → AI avoids repeating mistakes |
