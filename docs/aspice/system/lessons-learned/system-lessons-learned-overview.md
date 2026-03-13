# System Requirements â€” Lessons Learned

## Rule

**Every system requirement (SYS-NNN) that undergoes HITL review discussion MUST have its own lessons-learned document in this folder.**

- File naming: `SYS-NNN-<short-title>.md`
- Created during or immediately after the review discussion
- One file per requirement â€” never combine multiple requirements into one doc
- **Use the standard template below** so every doc is reproducible in a real review meeting

## Standard Template

Every lessons-learned file MUST follow this structure:

```markdown
# Lessons Learned â€” [ID]: [Title]

**Requirement**: [Copy the requirement text]
**ASIL**: [ASIL level]
**Reviewers**: [Names + roles]
**Review date**: YYYY-MM-DD
**Status**: Open / Closed

---

## Review Discussion

### Topic 1: [e.g., ASIL Rating]
- **Raised by**: [Name]
- **Discussion**: [What was discussed, arguments for/against]
- **Resolution**: [What was agreed]
- **Action item**: [If any â€” owner + deadline]

### Topic 2: [e.g., Architecture Choice]
- **Raised by**: [Name]
- **Discussion**: [...]
- **Resolution**: [...]
- **Action item**: [...]

(repeat for each discussion point)

---

## Corrections Made During Review

| What changed | Before | After | Reason |
|-------------|--------|-------|--------|
| [field/text] | [old value] | [new value] | [why] |

(leave empty if no corrections)

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| [short label] | [one-liner takeaway] |

---

## Open Action Items

| # | Action | Owner | Deadline | Status |
|---|--------|-------|----------|--------|
| 1 | [description] | [name] | [date] | Open/Done |

(leave empty if none)
```

### Why this format

- **Reproducible**: Anyone can run a review meeting using this template
- **Traceable**: Links discussion points to people, decisions, and actions
- **Auditable**: ASPICE SUP.4 (Joint Review) expects documented review outcomes
- **Actionable**: Open items have owners and deadlines â€” nothing gets lost

## Consolidated Location

> **All lessons-learned files are now in [`docs/reference/lessons-learned/`](../../../reference/lessons-learned/apps-web-overview.md).**
> This folder retains the template and rules. Actual lesson files live in the consolidated folder.

## Index

| Requirement | File | Date | Status |
|-------------|------|------|--------|
| SYS-001 | [SYS-001-dual-pedal-sensing.md](../../../reference/lessons-learned/safety/SYS-001-dual-pedal-sensing.md) | 2026-02-27 | Closed |

