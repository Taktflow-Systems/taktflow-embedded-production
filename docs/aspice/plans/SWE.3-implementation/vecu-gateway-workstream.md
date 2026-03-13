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

# SWE.3 Implementation Plan (vECU and Gateway)

Process area: SWE.3 Software Detailed Design and Unit Construction
Scope: phases 10 to 11

## Entry Criteria

- [ ] Physical ECU data contracts are stable
- [ ] POSIX CAN backend architecture approved
- [ ] Cloud and test account prerequisites available

## Detailed Work Breakdown

## VG1 vECU Runtime and Build Pipeline

- [ ] Implement `Can_Posix.c` with parity to STM32 CAN API
- [ ] Build Linux container toolchain and compose orchestration
- [ ] Implement BCM executable path and signal behavior
- [ ] Implement ICU executable path and dashboard behavior
- [ ] Implement TCU UDS and DTC storage behavior
- [ ] Validate CAN bridge for mixed real and simulated mode

## VG2 Edge Gateway Telemetry and ML

- [ ] Implement CAN listener and decoder pipeline
- [ ] Implement MQTT publisher and cloud schema
- [ ] Implement model inference pipeline for health and anomaly models
- [ ] Build alerting thresholds and dashboard visualization

## VG3 Quality Integration Demonstrator

- [ ] Implement SAP QM mock API and persistence layer
- [ ] Implement DTC to notification mapping
- [ ] Generate 8D template payload from DTC context
- [ ] Surface notification status in fault injector GUI

## Outputs

- [ ] 3 simulated ECU binaries and container definitions
- [ ] Operational gateway telemetry and ML services
- [ ] Operational QM mock integration flow

## Review Checklist (Gate G3)

- [ ] One command starts all vECU components
- [ ] Key UDS services respond correctly
- [ ] Faults propagate from CAN to cloud alerts
- [ ] Faults propagate from DTC to QM notification and 8D template

