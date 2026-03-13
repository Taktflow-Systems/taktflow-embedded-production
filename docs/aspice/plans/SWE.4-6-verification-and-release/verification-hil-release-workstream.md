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

# SWE.4/5/6 Verification, HIL, and Release Plan

Process areas:
- SWE.4 Software Unit Verification
- SWE.5 Software Integration and Integration Test
- SWE.6 Software Qualification Test

Scope: phases 12 to 14

## Entry Criteria

- [ ] SWE.3 outputs are baseline-ready
- [ ] Test environment definitions exist for MIL/SIL/PIL/HIL
- [ ] Acceptance criteria traceable to requirements

## Detailed Work Breakdown

## VR1 Unit and Static Verification (SWE.4)

- [ ] Implement unit tests for BSW modules and SWCs
- [ ] Execute static analysis and MISRA subset checks
- [ ] Publish `docs/aspice/verification/unit-test/unit-test-report.md`
- [ ] Publish `docs/aspice/verification/unit-test/static-analysis-report.md`
- [ ] Publish `docs/aspice/verification/unit-test/coverage-report.md`

## VR2 Integration Verification (SWE.5)

- [ ] Build MIL plant models and scenario tests
- [ ] Build SIL orchestration and closed-loop plant simulation
- [ ] Build PIL setup for at least one real target MCU
- [ ] Publish `docs/aspice/verification/xil/mil-report.md`
- [ ] Publish `docs/aspice/verification/xil/sil-report.md`
- [ ] Publish `docs/aspice/verification/xil/pil-report.md`

## VR3 System Qualification and HIL (SWE.6)

- [ ] Assemble and wire full HIL bench
- [ ] Execute functional, safety-chain, diagnostics, cloud, and endurance tests
- [ ] Publish `docs/aspice/verification/xil/hil-report.md`
- [ ] Publish `docs/aspice/verification/integration-test/integration-test-report.md`
- [ ] Publish `docs/aspice/verification/xil/xil-comparison-report.md`

## VR4 Release Evidence and Portfolio Package

- [ ] Record and catalog all 16 demo scenarios
- [ ] Finalize safety case and traceability evidence
- [ ] Finalize release README and media links
- [ ] Perform release tagging workflow

## Review Checklist (Gate G4)

- [ ] Required reports exist and are internally consistent
- [ ] All critical safety scenarios have pass evidence
- [ ] Traceability is complete from requirements to test results
- [ ] Release tag has supporting artifacts

