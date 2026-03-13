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

# SWE.3 Firmware Implementation Plan (Physical ECUs)

Process area: SWE.3 Software Detailed Design and Unit Construction
Scope: phases 5 to 9 for physical ECU path

## Entry Criteria

- [ ] Interface baseline frozen
- [ ] Shared architecture baseline frozen
- [ ] Build environments prepared (STM32CubeIDE, CCS)

## Detailed Work Breakdown

## FW1 Shared BSW Construction

- [ ] MCAL wrappers ready and buildable
- [ ] CanIf, PduR, Com data path tested with loopback
- [ ] Dcm and Dem baseline services validated
- [ ] WdgM and BswM mode behavior validated
- [ ] RTE signal read/write mechanism validated

## FW2 CVC Construction

- [ ] Implement pedal plausibility, vehicle state, dashboard, E-stop, heartbeat SWCs
- [ ] Configure CVC signal and runnable mapping
- [ ] Verify CVC functional and fault behavior against requirements

## FW3 FZC Construction

- [ ] Implement UART path and lidar decode
- [ ] Implement steering, brake, local safety SWCs
- [ ] Verify emergency and timeout behavior

## FW4 RZC Construction

- [ ] Implement motor, current monitor, temp monitor, battery, safety SWCs
- [ ] Verify overcurrent, overtemp, and emergency brake behavior

## FW5 Safety Controller Construction (Independent Path)

- [ ] Implement heartbeat monitor and timeout policy
- [ ] Implement relay kill chain and watchdog feed gating
- [ ] Verify independent safe-state authority

## Outputs

- [ ] Firmware source complete for CVC/FZC/RZC/SC
- [ ] Build artifacts generated for all four physical ECUs
- [ ] ECU-level construction evidence recorded

## Review Checklist (Gate G2)

- [ ] All ECU projects build cleanly
- [ ] All safety-critical behaviors exercised at least once
- [ ] Safety Controller can force stop regardless of zone state

