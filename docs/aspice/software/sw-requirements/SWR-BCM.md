---
document_id: SWR-BCM
title: "Software Requirements — BCM"
version: "1.0"
status: draft
aspice_process: SWE.1
ecu: BCM
asil: QM
date: 2026-02-21
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

## Lessons Learned Rule

Every requirement (SWR-BCM-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/software/lessons-learned/`](../lessons-learned/). One file per requirement (SWR-BCM-NNN). File naming: `SWR-BCM-NNN-<short-title>.md`.


# Software Requirements — Body Control Module (BCM)

## 1. Purpose

This document specifies the software requirements for the Body Control Module (BCM) of the Taktflow Zonal Vehicle Platform, per Automotive SPICE 4.0 SWE.1. The BCM is a simulated ECU running as a Docker container on a POSIX platform with SocketCAN (vcan0). It handles body functions: headlight control, turn indicators, hazard lights, and door lock simulation. All BCM functions are QM-rated (no safety-critical functions).

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SYSREQ | System Requirements Specification | 1.0 |
| SG | Safety Goals | 1.0 |
| FSC | Functional Safety Concept | 1.0 |
| CAN-MATRIX | CAN Message Matrix | 0.1 |

## 3. Requirement Conventions

### 3.1 Requirement Structure

Each requirement follows the format:

- **ID**: SWR-BCM-NNN (sequential)
- **Title**: Descriptive name
- **ASIL**: QM (all BCM requirements)
- **Traces up**: SYS-xxx
- **Traces down**: firmware/bcm/src/{file}
- **Verified by**: TC-BCM-xxx (test case ID)
- **Status**: draft | reviewed | approved | implemented | verified

### 3.2 Runtime Environment

The BCM runs as a Docker container (Linux/POSIX) and accesses the CAN bus via SocketCAN (vcan0). It does not use the shared AUTOSAR-like BSW stack. CAN access is via direct POSIX socket API (AF_CAN, SOCK_RAW, CAN_RAW).

---

## 4. Requirements

### SWR-BCM-001: SocketCAN Interface Initialization

- **ASIL**: QM
- **Traces up**: SYS-031, SYS-035
- **Traces down**: firmware/bcm/src/can_interface.c:BCM_CAN_Init()
- **Verified by**: TC-BCM-001
- **Status**: draft

The BCM software shall initialize a SocketCAN interface on startup by: (a) creating a raw CAN socket (AF_CAN, SOCK_RAW, CAN_RAW), (b) binding the socket to the vcan0 interface, (c) configuring a CAN filter to accept messages with CAN IDs 0x100 (vehicle state), 0x350 (body control command), and 0x001 (E-stop), and (d) verifying the socket is operational by reading the interface flags. If socket initialization fails, the BCM shall log an error and retry every 1 second up to 10 attempts before entering an error state.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Requirement is technically sound and well-structured. Traces to SYS-031 (CAN bus config) and SYS-035 (headlight/body) are consistent. The CAN filter list matches the BCM's role (vehicle state, body commands, E-stop). The retry mechanism (10 attempts, 1s interval) is reasonable for a Docker/POSIX environment. Consider documenting the behavior after entering the error state (does the BCM attempt periodic recovery or require a restart?).
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-001 -->

---

### SWR-BCM-002: Vehicle State CAN Reception

- **ASIL**: QM
- **Traces up**: SYS-029, SYS-030, SYS-035
- **Traces down**: firmware/bcm/src/can_interface.c:BCM_CAN_ReceiveState()
- **Verified by**: TC-BCM-002, TC-BCM-003
- **Status**: draft

The BCM software shall receive the vehicle state CAN message (CAN ID 0x100) from the CVC and extract the vehicle state field (4-bit, byte 0 bits 3:0 after E2E header). The BCM shall maintain a local copy of the current vehicle state (INIT, RUN, DEGRADED, LIMP, SAFE_STOP, SHUTDOWN). If no vehicle state message is received for 500 ms, the BCM shall assume SHUTDOWN state and deactivate all outputs. The BCM shall perform E2E validation (CRC-8 and alive counter check) on the received message and discard messages that fail validation.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-029 (vehicle state machine), SYS-030 (mode management), and SYS-035 are correct. The 500 ms timeout fallback to SHUTDOWN is a safe-side default. E2E validation on a QM node receiving ASIL-rated messages is good practice. One concern: the BCM is described as not using the shared BSW stack, so it must implement its own E2E check logic (CRC-8, alive counter) independently -- ensure this is covered in the detailed design.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-002 -->

---

### SWR-BCM-003: Headlight Auto-On Control

- **ASIL**: QM
- **Traces up**: SYS-035
- **Traces down**: firmware/bcm/src/body_control.c:BCM_Headlight_Update()
- **Verified by**: TC-BCM-004, TC-BCM-005
- **Status**: draft

The BCM software shall automatically activate the headlight output when the received vehicle state is RUN, DEGRADED, or LIMP. The headlight shall be represented as a boolean state variable (headlight_on). The headlight shall be activated within 100 ms of receiving a vehicle state transition to RUN. The headlight state shall be included in the body status CAN message (SWR-BCM-010).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-035 (automatic headlight control) is correct. The 100 ms activation latency requirement is testable and appropriate for a non-safety function. The cross-reference to SWR-BCM-010 for status reporting is good for traceability. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-003 -->

---

### SWR-BCM-004: Headlight Auto-Off Control

- **ASIL**: QM
- **Traces up**: SYS-035
- **Traces down**: firmware/bcm/src/body_control.c:BCM_Headlight_Update()
- **Verified by**: TC-BCM-006
- **Status**: draft

The BCM software shall automatically deactivate the headlight output when the received vehicle state is INIT, SAFE_STOP, or SHUTDOWN, or when no vehicle state message has been received for 500 ms. The headlight shall be deactivated within 100 ms of the triggering condition.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Correctly complements SWR-BCM-003 for the off-path. The 500 ms timeout fallback is consistent with SWR-BCM-002's timeout behavior. Trace to SYS-035 is valid. The 100 ms deactivation latency matches the activation requirement for symmetry. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-004 -->

---

### SWR-BCM-005: Headlight Manual Override

- **ASIL**: QM
- **Traces up**: SYS-035
- **Traces down**: firmware/bcm/src/body_control.c:BCM_Headlight_Override()
- **Verified by**: TC-BCM-007
- **Status**: draft

The BCM software shall support a manual headlight override via a CAN command (CAN ID 0x350, body control command, byte 2 bit 0: 1 = force ON, 0 = auto mode). When manual override is active, the headlight shall remain on regardless of the vehicle state. The manual override shall be cancelled by sending a command with the override bit cleared, returning to automatic mode (SWR-BCM-003, SWR-BCM-004).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Manual override logic is clear. Cross-references to SWR-BCM-003 and SWR-BCM-004 are correct. Note: SWR-BCM-011 defines the command message format as byte 0 bit 0 for headlight override, but this requirement references byte 2 bit 0. There is an inconsistency between SWR-BCM-005 and SWR-BCM-011 on the byte offset for headlight control -- this needs resolution in the CAN matrix.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-005 -->

---

### SWR-BCM-006: Turn Indicator Control

- **ASIL**: QM
- **Traces up**: SYS-036
- **Traces down**: firmware/bcm/src/body_control.c:BCM_TurnIndicator_Update()
- **Verified by**: TC-BCM-008, TC-BCM-009, TC-BCM-010
- **Status**: draft

The BCM software shall control left and right turn indicators based on a CAN command (CAN ID 0x350, byte 3: 0x00 = off, 0x01 = left, 0x02 = right). When a turn indicator is active, the corresponding indicator state shall flash at 1.5 Hz (350 ms on, 350 ms off, total period approximately 700 ms). The flash timing shall be maintained by a software timer with 10 ms resolution. When the turn indicator command changes (e.g., left to right, or left to off), the new state shall take effect within one flash cycle (700 ms maximum). Only one turn direction shall be active at a time (left or right, never both simultaneously in turn mode).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-036 (turn indicator control) is correct. The 1.5 Hz flash rate and 350/350 ms duty cycle are within standard automotive indicator ranges. The 10 ms timer resolution gives adequate precision (3% of the 350 ms period). Note: SWR-BCM-011 defines indicator control in byte 1 (0x00-0x03), but this requirement references byte 3. Same byte offset inconsistency as SWR-BCM-005 -- needs alignment with the command message definition in SWR-BCM-011.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-006 -->

---

### SWR-BCM-007: Hazard Light Activation

- **ASIL**: QM
- **Traces up**: SYS-036
- **Traces down**: firmware/bcm/src/body_control.c:BCM_HazardLight_Update()
- **Verified by**: TC-BCM-011, TC-BCM-012, TC-BCM-013
- **Status**: draft

The BCM software shall activate hazard lights (both left and right indicators flashing simultaneously) when any of the following conditions is detected: (a) the vehicle state transitions to SAFE_STOP or SHUTDOWN, (b) a hazard light CAN command is received (CAN ID 0x350, byte 3: 0x03 = hazard), or (c) the vehicle state is DEGRADED or LIMP (automatic hazard activation for degraded operation). Hazard lights shall flash at 1.5 Hz (350 ms on, 350 ms off), synchronized between left and right. Hazard lights shall have priority over turn indicators: if hazard lights are active, any turn indicator command shall be ignored until hazard lights are deactivated.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-007 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Good multi-trigger design covering both commanded and automatic hazard activation. Trace to SYS-036 is correct. The priority rule (hazard > turn) is explicitly stated, which is important for test case derivation. The automatic hazard activation in DEGRADED/LIMP states aligns with BswM rule R2/R3 in SWR-BSW-022. Same byte offset note as SWR-BCM-006 regarding byte 3 vs SWR-BCM-011 byte 1.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-007 -->

---

### SWR-BCM-008: Hazard Light Deactivation

- **ASIL**: QM
- **Traces up**: SYS-036
- **Traces down**: firmware/bcm/src/body_control.c:BCM_HazardLight_Update()
- **Verified by**: TC-BCM-014
- **Status**: draft

The BCM software shall deactivate hazard lights when all of the following conditions are met: (a) the vehicle state is RUN (not DEGRADED, LIMP, SAFE_STOP, or SHUTDOWN), (b) no hazard light CAN command is active, and (c) the vehicle state has been RUN for at least 2 consecutive seconds (debounce to prevent hazard light flicker during transient state changes). Upon deactivation, turn indicator commands shall be honored again.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-008 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The 2-second debounce on hazard deactivation is a thoughtful design choice to prevent flicker during transient state changes. The AND-logic (all three conditions) is correct and conservative. Trace to SYS-036 is consistent. Properly complements SWR-BCM-007's activation logic. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-008 -->

---

### SWR-BCM-009: Door Lock Simulation

- **ASIL**: QM
- **Traces up**: SYS-035
- **Traces down**: firmware/bcm/src/body_control.c:BCM_DoorLock_Update()
- **Verified by**: TC-BCM-015, TC-BCM-016
- **Status**: draft

The BCM software shall simulate door lock/unlock functionality based on a CAN command (CAN ID 0x350, byte 4: 0x00 = unlock, 0x01 = lock). The BCM shall maintain a door lock state variable (locked/unlocked). Upon receiving a lock or unlock command, the state shall transition within 100 ms. The door lock state shall be included in the body status CAN message (SWR-BCM-010). The BCM shall automatically lock the doors when the vehicle state transitions to RUN (speed > 0 implied by RUN state) and automatically unlock when the vehicle state transitions to SHUTDOWN.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-009 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-035 is valid. The auto-lock on RUN and auto-unlock on SHUTDOWN are common automotive convenience features. Same byte offset concern: this references byte 4, but SWR-BCM-011 defines door lock at byte 2. The assumption "speed > 0 implied by RUN state" should be noted as an assumption -- RUN does not necessarily mean the vehicle is moving. Cross-reference to SWR-BCM-010 for status reporting is correct.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-009 -->

---

### SWR-BCM-010: Body Status CAN Transmission

- **ASIL**: QM
- **Traces up**: SYS-035, SYS-036
- **Traces down**: firmware/bcm/src/can_interface.c:BCM_CAN_TransmitStatus()
- **Verified by**: TC-BCM-017, TC-BCM-018
- **Status**: draft

The BCM software shall transmit a body status CAN message (CAN ID 0x360) every 100 ms. The message payload shall contain:

| Byte | Bits | Field | Description |
|------|------|-------|-------------|
| 0 | 7:4 | Alive counter | 4-bit, wrapping 0-15 |
| 0 | 3:0 | Reserved | Set to 0 |
| 1 | 7:0 | CRC-8 | CRC-8 over bytes 2-7 |
| 2 | 0 | Headlight state | 0 = off, 1 = on |
| 2 | 1 | Left indicator | 0 = off, 1 = on (current flash state) |
| 2 | 2 | Right indicator | 0 = off, 1 = on (current flash state) |
| 2 | 3 | Hazard active | 0 = off, 1 = active |
| 2 | 4 | Door locked | 0 = unlocked, 1 = locked |
| 2 | 5 | Headlight override | 0 = auto, 1 = manual override |
| 2 | 7:6 | Reserved | Set to 0 |

The alive counter shall increment by 1 per transmission. The CRC-8 shall use polynomial 0x1D (SAE J1850) for consistency with the E2E protection used on safety-critical messages.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-010 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Well-specified message layout with a complete bit-level definition table. Traces to both SYS-035 and SYS-036 are correct. The use of E2E-style protection (alive counter + CRC-8) on a QM message is good practice for consistency across the CAN bus. The 100 ms transmission period aligns with the BCM main loop (10th cycle at 10 ms). Using SAE J1850 polynomial matches the system-level E2E scheme (SWR-BSW-023). No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-010 -->

---

### SWR-BCM-011: Body Control Command Reception

- **ASIL**: QM
- **Traces up**: SYS-035, SYS-036
- **Traces down**: firmware/bcm/src/can_interface.c:BCM_CAN_ReceiveCommand()
- **Verified by**: TC-BCM-019, TC-BCM-020
- **Status**: draft

The BCM software shall receive body control commands on CAN ID 0x350 from the CVC. This is a QM (non-safety) message and does not use E2E protection. The command message format shall be:

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Headlight control | Bit 0: manual override (0=auto, 1=force on) |
| 1 | Indicator control | 0x00=off, 0x01=left, 0x02=right, 0x03=hazard |
| 2 | Door lock control | 0x00=unlock, 0x01=lock |

If no valid command is received for 2 seconds, the BCM shall revert all outputs to their automatic/default states.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-011 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The command message format here (byte 0 = headlight, byte 1 = indicator, byte 2 = door lock) conflicts with byte offsets referenced in SWR-BCM-005 (byte 2), SWR-BCM-006 (byte 3), and SWR-BCM-009 (byte 4). This is the authoritative command definition, so the individual requirements should be updated to match this table. The 2-second command timeout with revert to defaults is a safe fallback. Being QM with no E2E is acceptable for body control commands. Traces to SYS-035 and SYS-036 are valid.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-011 -->

---

### SWR-BCM-012: BCM Main Loop Execution

- **ASIL**: QM
- **Traces up**: SYS-035
- **Traces down**: firmware/bcm/src/main.c:BCM_Main()
- **Verified by**: TC-BCM-021
- **Status**: draft

The BCM software shall execute a main loop at a 10 ms cycle time (100 Hz) using a POSIX timer (timer_create with CLOCK_MONOTONIC) or equivalent scheduling mechanism. Each loop iteration shall: (a) read all pending CAN messages from the SocketCAN socket (non-blocking read), (b) update the vehicle state from received messages (SWR-BCM-002), (c) process body control commands (SWR-BCM-011), (d) update headlight state (SWR-BCM-003, SWR-BCM-004, SWR-BCM-005), (e) update turn indicator and hazard light flash timers (SWR-BCM-006, SWR-BCM-007, SWR-BCM-008), (f) update door lock state (SWR-BCM-009), (g) transmit body status message every 10th cycle (100 ms) (SWR-BCM-010). The main loop shall log a warning if any iteration exceeds 5 ms execution time.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-BCM-012 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The main loop structure is well-defined with clear execution order and cross-references to all subordinate requirements. The 5 ms WCET warning threshold at 50% of the 10 ms cycle is appropriate. The non-blocking CAN read is correct for a POSIX SocketCAN implementation. All internal requirement cross-references (SWR-BCM-002 through SWR-BCM-011) are accounted for. Trace to SYS-035 is valid. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-BCM-012 -->

---

## 5. Requirements Traceability Summary

### 5.1 SYS to SWR-BCM Mapping

| SYS | SWR-BCM |
|-----|---------|
| SYS-031 | SWR-BCM-001 |
| SYS-029 | SWR-BCM-002 |
| SYS-030 | SWR-BCM-002 |
| SYS-035 | SWR-BCM-001, SWR-BCM-002, SWR-BCM-003, SWR-BCM-004, SWR-BCM-005, SWR-BCM-009, SWR-BCM-010, SWR-BCM-011, SWR-BCM-012 |
| SYS-036 | SWR-BCM-006, SWR-BCM-007, SWR-BCM-008, SWR-BCM-010, SWR-BCM-011 |

### 5.2 Requirement Summary

| Metric | Value |
|--------|-------|
| Total SWR-BCM requirements | 12 |
| ASIL D | 0 |
| ASIL C | 0 |
| ASIL B | 0 |
| ASIL A | 0 |
| QM | 12 |
| Test cases (placeholder) | TC-BCM-001 to TC-BCM-021 |

## 6. Open Items and Assumptions

### 6.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| BCM-A-001 | Docker container has access to vcan0 SocketCAN interface | BCM CAN communication depends on Docker network configuration |
| BCM-A-002 | CAN message IDs 0x350 (body command) and 0x360 (body status) are not used by other ECUs | CAN arbitration conflict if IDs overlap |
| BCM-A-003 | POSIX timer provides sufficient timing accuracy for 1.5 Hz flash rate | Flash rate deviation within 10% is acceptable for turn indicators |

### 6.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| BCM-O-001 | Finalize CAN message IDs for body control domain in CAN matrix | System Architect | SYS.3 phase |
| BCM-O-002 | Define Docker container networking for SocketCAN access | DevOps Engineer | Phase 3 |

## 7. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete SWR specification: 12 requirements (SWR-BCM-001 to SWR-BCM-012), traceability, test case mapping |

