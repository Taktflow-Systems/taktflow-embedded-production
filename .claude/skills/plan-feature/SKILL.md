---
name: plan-feature
description: "Create a structured implementation plan for a new feature or change"
argument-hint: "<feature name or description>"
user-invocable: true
allowed-tools: "Read, Grep, Glob, Bash(ls *)"
---

## Human-in-the-Loop (HITL) Comment Lock

`HITL` means human-reviewer-owned comment content.

**Marker standard (code-friendly):**
- Markdown: `<!-- HITL-LOCK START:<id> -->` ... `<!-- HITL-LOCK END:<id> -->`
- C/C++/Java/JS/TS: `// HITL-LOCK START:<id>` ... `// HITL-LOCK END:<id>`
- Python/Shell/YAML/TOML: `# HITL-LOCK START:<id>` ... `# HITL-LOCK END:<id>`

**Rules:**
- AI must never edit, reformat, move, or delete text inside any `HITL-LOCK` block.
- Append-only: AI may add new comments/changes only; prior HITL comments stay unchanged.
- If a locked comment needs revision, add a new note outside the lock or ask the human reviewer to unlock it.


# Plan Feature

Create a structured implementation plan for: $ARGUMENTS

## Process

1. **Understand the request** — clarify requirements if ambiguous
2. **Capture the working directory** — run `ls -1` on the current working directory (and each affected subdirectory) and record the actual output. Use this listing as the ground truth for all file references; never reference a path that does not appear in the listing.
3. **Explore existing code** — find related modules, interfaces, patterns
4. **Identify affected files** — list every file that needs changes, cross-referencing against the `ls` output captured in step 2
5. **Design the approach** — architecture decisions, data flow, interfaces
6. **Write the plan** — structured plan document in `docs/plans/`

## Plan Template

Write to `docs/plans/<feature-name>.md` using this format:

```markdown
# Plan: <Feature Name>

**Status:** DRAFT
**Created:** <date>
**Author:** Claude + <user>

## Overview
One paragraph describing what this feature does and why.

## Working Directory Snapshot

<!-- Paste the actual output of `ls -1 <affected-dirs>` here so workers can verify file existence -->
    <ls output captured at plan creation time>

## Phase Table

| Phase | Name | Status |
|-------|------|--------|
| 1 | <name> | PENDING |
| 2 | <name> | PENDING |

## Phase 1: <Name>

### Tasks
- [ ] Task description
- [ ] Task description

### Files Changed
- `path/to/file.c` — description of changes
- `path/to/file.h` — description of changes

### DONE Criteria
- Criteria 1
- Criteria 2

## Phase 2: <Name>
(repeat structure)

## Security Considerations
- Input validation requirements
- Credential handling
- Attack surface changes

## Testing Plan
- Unit tests needed
- Integration tests needed
- Hardware tests needed

## Open Questions
- Question 1?
- Question 2?
```

## Rules

- DO NOT start implementing — only produce the plan document
- Present the plan for user approval before any code changes
- Each phase must have clear DONE criteria
- Security considerations are mandatory, not optional
- Testing plan is mandatory, not optional

