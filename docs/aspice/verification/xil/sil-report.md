---
document_id: SIL-RPT
title: "SIL Test Report"
version: "0.2"
status: in_progress
aspice_process: SWE.5
date: 2026-07-06
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


# Software-in-the-Loop (SIL) Test Report

<!-- Phase 12 deliverable -->

## Purpose

Validate firmware logic with all 7 ECUs compiled for Linux, running on Docker + vcan0.

This report is also the ASPICE evidence container for the public SIL upstream.
The upstream is active, but this document is not complete until each scenario
has a recorded baseline, pass/fail result, logs, and CAN trace.

## Configuration

- All ECUs as Linux processes/containers
- Virtual CAN bus (vcan0)
- Plant simulator providing sensor feedback
- Public telemetry upstream: `wss://sil.taktflow-systems.com/ws/telemetry`
- Public fault/test API base: `https://sil.taktflow-systems.com`

## Current Upstream Status

| Interface | Check | Status |
|-----------|-------|--------|
| Telemetry WebSocket | Connects and publishes snapshot telemetry. | Active |
| Fault control API | `/api/fault/control/status` responds. | Active |
| Test specification API | `/api/test/specs` responds. | Active |
| Scenario evidence | 16 scenario rows with logs and CAN traces. | Pending |

## Evidence Required Before Closure

| Evidence | Required Content |
|----------|------------------|
| Baseline | Git commit or release tag, container image identifiers, DBC version, and scenario spec version. |
| Test environment | Host OS, Docker compose file, vCAN setup, API/WS endpoint configuration. |
| Scenario record | Scenario ID, requirement links, injected fault or stimulus, expected result, actual result, pass/fail. |
| Logs and traces | ECU logs, gateway logs, plant simulator log, CAN trace, telemetry capture, and fault API response. |
| Timing | Detection time, transition time to safe state, and comparison against the credited FTTI. |
| Review | Reviewer, independence level, findings, and disposition. |

## Test Scenarios

All 16 demo scenarios — see master-plan.md.

| # | Scenario | Result | CAN Trace |
|---|----------|--------|-----------|
| 1-16 | See master-plan.md | Pending scenario-level execution record | Pending |

## Results

SIL upstream is active as of 2026-07-06. Scenario-level evidence is still
pending, so this report supports integration readiness but not final SWE.6,
SYS.4, SYS.5, or ISO 26262 validation closure.

