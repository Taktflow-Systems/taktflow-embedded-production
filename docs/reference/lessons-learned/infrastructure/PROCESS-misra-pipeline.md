# Lessons Learned — MISRA C:2012 Compliance Pipeline

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Completed:** 2026-02-24
**Scope:** Static analysis pipeline from zero to CI-blocking green
**Result:** 1,536 violations → 0 in one day

---

## 1. cppcheck Version Mismatch (Local 2.17 vs CI 2.13)

The biggest time sink was discovering that GitHub Actions Ubuntu 24.04 ships cppcheck 2.13 via apt, while local dev used 2.17 via pip.

| Issue | cppcheck 2.13 behavior | Fix |
|-------|----------------------|-----|
| `#` comments in suppressions.txt | Parse error — cannot handle them | Strip all comments, document justifications elsewhere |
| Style findings + `--error-exitcode=1` | Style findings (variableScope, etc.) trigger error exit | Add `--suppress` for 5 style IDs in Makefile |
| Rule 8.5 on cfg/header cross-refs | False positives on AUTOSAR cfg pattern | Suppress Rule 8.5 globally (real violations caught by Rule 8.4) |
| `misra-config` on POSIX ioctl | Unknown Linux kernel constant | Suppress misra-config for `*_hw_posix.c` |
| Rule 11.8 on Com.c | Not flagged in 2.17, flagged in 2.13 | Add suppression (same AUTOSAR pattern as DEV-002) |

**Lesson:** Always test your static analysis pipeline on the CI runner version first. Pin the tool version or accept the delta.

---

## 2. .gitignore Blocks Suppression File

`.gitignore` had `tools/misra/*.txt` to exclude copyrighted MISRA rule texts. This also blocked `suppressions.txt` from being tracked.

**Fix:** `!tools/misra/suppressions.txt` exception in `.gitignore`.

**Lesson:** When gitignoring a directory pattern, immediately check if any project-owned files match. Use `!` exceptions.

---

## 3. MISRA Rule Texts Are Not Redistributable

MISRA provides headline texts on their public GitLab for tool use, but they are copyrighted. They must be downloaded per-developer and per-CI-run, never committed to Git.

**Pattern:** `scripts/setup-misra-rules.sh` downloads on first run; CI downloads fresh each run.

---

## 4. Triage Strategy: Advisory vs Required

Of 1,536 violations:
- ~1,000 were Advisory rules (2.3, 2.5, 5.9, 8.7, 15.5, etc.) — suppressed with documented justification
- ~400 were Required rules — all fixed in code
- 12 were formal deviations (AUTOSAR void* and const-removal patterns)

**Lesson:** Don't try to fix everything at once. Suppress advisory rules first (with rationale), then focus on required rules. The 74% reduction from advisory suppressions makes the remaining required fixes manageable.

---

## 5. AUTOSAR Patterns Create Unavoidable MISRA Deviations

Two formal deviations were needed:

| ID | Rule | Pattern | Why unavoidable |
|----|------|---------|----------------|
| DEV-001 | 11.5 | `void*` → typed pointer in Com.c | AUTOSAR Com uses void* for generic signal data by design |
| DEV-002 | 11.8 | const-removal in CanIf.c, Dcm.c, Com.c | AUTOSAR BSW callbacks store const pointers for deferred processing |

**Lesson:** If you use AUTOSAR patterns, expect 2-3 MISRA deviations. Document them formally in a deviation register with risk assessment and compensating measures. This is normal and expected.

---

## 6. stderr Redirect Order Matters

cppcheck sends violations to stderr and progress to stdout. The redirect order matters:

```bash
# WRONG — stderr goes to terminal
cppcheck ... 2>&1 >> file

# CORRECT — stdout to file first, then stderr follows
cppcheck ... >> file 2>&1
```

**Lesson:** This is basic Unix but costs a debugging round every time.

---

## 7. Relative Path Trap in misra.json

The `--rule-texts` path in `misra.json` is relative to CWD (where cppcheck runs), not relative to the JSON file location. Since the Makefile runs from `firmware/`, the path must be `../tools/misra/...`.

---

## 8. CI Blocking is the Goal

Started with `--error-exitcode=0` (non-blocking) for triage. After all violations were resolved, switched to `--error-exitcode=1` (blocking). New MISRA violations now fail CI.

**Lesson:** Non-blocking is for triage only. Switch to blocking as soon as possible — otherwise the violation count creeps back up.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| Tool version parity | Always match CI and local tool versions, or document the delta |
| .gitignore exceptions | Check for collateral damage when ignoring directory patterns |
| Triage order | Suppress advisory first, fix required second, deviate last |
| AUTOSAR deviations | Normal — document formally, don't fight the pattern |
| CI blocking | Switch from non-blocking to blocking ASAP |
| One-day pipeline | 1,536 → 0 is achievable in one focused day with a clear triage strategy |
