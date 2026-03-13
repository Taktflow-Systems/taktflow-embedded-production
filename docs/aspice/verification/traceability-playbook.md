# Traceability Playbook (Token-Safe)

Use this flow when asking AI (Claude/Codex) to do requirement traceability without burning context.

## 1) Work in small slices

- Never ask for full-repo traceability in one prompt.
- Use one layer at a time:
  - `STK -> SYS`
  - `SYS -> SWR`
  - `SWR -> code/tests`
- Limit by ID range (example: `STK-011..STK-020`).

## 2) Force strict output format

- Ask for CSV/table only.
- No reasoning, no full file dumps, no long explanations.
- Stop after first N unresolved rows (for example `10`).

## 3) Reuse local deterministic extraction first

Use the local script to extract mappings before asking AI:

```powershell
python scripts/trace-slice.py --layer stk-sys --from STK-011 --to STK-020
python scripts/trace-slice.py --layer sys-swr --from SYS-037 --to SYS-045
```

Optional file output:

```powershell
python scripts/trace-slice.py --layer stk-sys --from STK-011 --to STK-020 --out docs/tmp/stk_sys_011_020.csv
```

## 4) Ask AI only for deltas

- Give AI the extracted CSV and ask only:
  - missing links
  - conflicting links
  - ambiguous links needing human review

## 5) Copy-paste prompt templates

### Template A: `STK -> SYS`

```text
Build STK->SYS mapping for STK-011..STK-020 only.
Read only:
- docs/aspice/system/stakeholder-requirements.md
- docs/aspice/system/system-requirements.md
Output CSV only: STK_ID,SYS_ID,Status,Note
Rules:
- No reasoning
- No full file content
- Stop after first 10 unresolved mappings
```

### Template B: `SYS -> SWR`

```text
Build SYS->SWR mapping for SYS-037..SYS-045 only.
Read only:
- docs/aspice/system/system-requirements.md
- docs/aspice/software/sw-requirements/SWR-*.md
Output CSV only: SYS_ID,SWR_ID,Status,Note
Rules:
- No reasoning
- No full file content
- Stop after first 10 unresolved mappings
```

### Template C: Gap Review Only

```text
Given this CSV mapping, list only unresolved or conflicting rows.
Output CSV only: ID,IssueType,SuggestedFix,Owner
No extra explanation.
```

