# Lessons Learned — Traceability Automation

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-02-25 to 2026-02-28
**Scope:** Building automated V-model traceability from requirements to tests
**Result:** trace-gen.py generating full matrix, CI enforcement blocking on gaps

---

## 1. Stub Script Is Worthless — Rewrite from Scratch

The first `trace-gen.py` was a stub that generated placeholder output. It existed for months before anyone noticed it didn't actually parse requirement files.

**Fix:** Complete rewrite: 500 lines of Python that parses `@traces_to`, `@verifies` tags from all requirement docs, source files, and test files.

**Lesson:** A stub that passes CI gives false confidence. Either build it properly or don't build it at all.

---

## 2. Traceability Gaps Are Invisible Without Automation

After the rewrite, `trace-gen.py` immediately found:
- 4 broken links (requirements referencing non-existent IDs)
- 6 untested SWRs (software requirements with no `@verifies` tag in any test file)

These had existed since the requirements were written. No manual review caught them.

**Lesson:** Traceability gaps are the most common ASPICE finding. Automated detection is the only reliable way to catch them.

---

## 3. CI Blocks Merge on Traceability Gaps

Added `trace-gen.py --check` to CI pipeline. It fails with exit code 1 if:
- Any requirement has no downstream trace (orphan requirement)
- Any test has no `@verifies` tag (orphan test)
- Any trace link target doesn't exist (broken link)

**Lesson:** Traceability, like MISRA, must be CI-blocking. Advisory-mode traceability checks are ignored.

---

## 4. Suspect Links on Requirement Changes

When a requirement text changes, all downstream traces become "suspect" — they may no longer be valid. `trace-gen.py` detects suspect links by comparing requirement hashes against a baseline.

**Lesson:** Suspect link detection is an ASPICE Level 3 requirement. It sounds complex but is just hash comparison: `hash(requirement_text) != stored_hash → suspect`.

---

## 5. Token-Safe Traceability Playbook

For AI-assisted development, the traceability matrix can be huge. Dumping the full matrix into a prompt wastes tokens.

**Pattern:** Slice by layer + ID range:
```bash
# Only show SYS-001 through SYS-010 traces
python trace-gen.py --filter "SYS-00[0-9]" --format csv
```

**Lesson:** Never dump full traceability matrices into AI prompts. Slice by scope, output CSV only, cap at N rows.

---

## 6. File-Based Traceability vs Tool-Based

Chose `@traces_to` / `@verifies` tags in source files over Polarion/DOORS/Jama.

**Why it works:**
- Tags live with the code — they're versioned, diffable, reviewable
- `git blame` shows when a trace link was added
- No license cost, no server, no training
- CI can parse tags with grep + Python

**What it doesn't do:**
- No visual matrix editor (use generated HTML/CSV)
- No impact analysis GUI (use `trace-gen.py --impact SYS-001`)
- No multi-project linking (single-project tool)

**Lesson:** For a single-project embedded development, file-based traceability covers 90% of commercial tool functionality at 0% cost.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Stubs | A stub that passes CI gives false confidence — build it or skip it |
| Automation | Traceability gaps are invisible without automated detection |
| CI blocking | Advisory traceability checks are ignored — make them blocking |
| Suspect links | Hash comparison on requirement text — simple but ASPICE Level 3 |
| Token safety | Never dump full matrices into prompts — slice by scope |
| File-based | Tags in source files cover 90% of DOORS at 0% cost |
