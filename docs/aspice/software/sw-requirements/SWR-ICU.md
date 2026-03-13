---
document_id: SWR-ICU
title: "Software Requirements — ICU"
version: "1.0"
status: draft
aspice_process: SWE.1
ecu: ICU
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

Every requirement (SWR-ICU-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/software/lessons-learned/`](../lessons-learned/). One file per requirement (SWR-ICU-NNN). File naming: `SWR-ICU-NNN-<short-title>.md`.


# Software Requirements — Instrument Cluster Unit (ICU)

## 1. Purpose

This document specifies the software requirements for the Instrument Cluster Unit (ICU) of the Taktflow Zonal Vehicle Platform, per Automotive SPICE 4.0 SWE.1. The ICU is a simulated ECU running as a Docker container on a POSIX platform with SocketCAN (vcan0). It aggregates vehicle data from CAN messages and presents gauges, warning indicators, DTC display, and vehicle state to the operator. All ICU functions are QM-rated (display only, no actuation, no safety-critical feedback path).

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

- **ID**: SWR-ICU-NNN (sequential)
- **Title**: Descriptive name
- **ASIL**: QM (all ICU requirements)
- **Traces up**: SYS-xxx
- **Traces down**: firmware/icu/src/{file}
- **Verified by**: TC-ICU-xxx (test case ID)
- **Status**: draft | reviewed | approved | implemented | verified

### 3.2 Runtime Environment

The ICU runs as a Docker container (Linux/POSIX) and accesses the CAN bus via SocketCAN (vcan0). It does not use the shared AUTOSAR-like BSW stack. CAN access is via direct POSIX socket API (AF_CAN, SOCK_RAW, CAN_RAW). The ICU is a receive-only node for vehicle data (it does not transmit control messages). It may transmit diagnostic responses on UDS functional addressing.

---

## 4. Requirements

### SWR-ICU-001: SocketCAN Interface Initialization

- **ASIL**: QM
- **Traces up**: SYS-031
- **Traces down**: firmware/icu/src/can_interface.c:ICU_CAN_Init()
- **Verified by**: TC-ICU-001
- **Status**: draft

The ICU software shall initialize a SocketCAN interface on startup by: (a) creating a raw CAN socket (AF_CAN, SOCK_RAW, CAN_RAW), (b) binding the socket to the vcan0 interface, (c) configuring CAN filters to accept messages with CAN IDs: 0x001 (E-stop), 0x100 (vehicle state / torque request), 0x301 (motor current), 0x302 (motor temperature), 0x303 (battery voltage), 0x010 (CVC heartbeat), 0x011 (FZC heartbeat), 0x012 (RZC heartbeat), 0x360 (body status), 0x400 (DTC notification). If socket initialization fails, the ICU shall log an error and retry every 1 second up to 10 attempts before entering an error state.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Comprehensive CAN filter list covering all message types needed by the ICU for display purposes. Trace to SYS-031 is correct. The filter set is appropriate for a receive-only display node. The retry mechanism (10 attempts, 1s interval) matches the BCM pattern for consistency. No safety controller heartbeat (SC) is listed, which is correct since SC has no heartbeat per the architecture.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-001 -->

---

### SWR-ICU-002: Speed Gauge Data Acquisition

- **ASIL**: QM
- **Traces up**: SYS-009, SYS-044
- **Traces down**: firmware/icu/src/gauges.c:ICU_Gauge_Speed()
- **Verified by**: TC-ICU-002, TC-ICU-003
- **Status**: draft

The ICU software shall extract the motor speed value from the motor status CAN message (CAN ID 0x301, bytes 4-5, 16-bit unsigned, RPM). The speed value shall be converted to a display-ready format (0-9999 RPM or equivalent km/h using a configurable wheel circumference and gear ratio). The speed gauge shall be updated every 100 ms. If no valid motor status message is received for 500 ms, the speed gauge shall display "--" (no data) and set a speed data timeout flag.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-009 (encoder feedback) and SYS-044 (OLED display) are correct. The configurable wheel circumference and gear ratio for km/h conversion is a good design choice for flexibility. The 500 ms timeout with "--" display is consistent with other timeout behaviors in the system. The 100 ms update rate is appropriate for human-readable gauge updates. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-002 -->

---

### SWR-ICU-003: Torque Gauge Data Acquisition

- **ASIL**: QM
- **Traces up**: SYS-044
- **Traces down**: firmware/icu/src/gauges.c:ICU_Gauge_Torque()
- **Verified by**: TC-ICU-004
- **Status**: draft

The ICU software shall extract the torque request percentage from the vehicle state CAN message (CAN ID 0x100, byte 4, 8-bit unsigned, 0-100%). The torque gauge shall display the current torque request as a percentage bar or numerical value. The torque gauge shall be updated every 100 ms. If no valid vehicle state message is received for 500 ms, the torque gauge shall display "--" (no data).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-044 is correct. The byte 4 offset for torque request in CAN ID 0x100 must be validated against the CAN message matrix once finalized (open item CAN-MATRIX v0.1). The 100 ms update rate and 500 ms timeout are consistent with the other gauges. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-003 -->

---

### SWR-ICU-004: Temperature Gauge Data Acquisition

- **ASIL**: QM
- **Traces up**: SYS-006, SYS-044
- **Traces down**: firmware/icu/src/gauges.c:ICU_Gauge_Temperature()
- **Verified by**: TC-ICU-005, TC-ICU-006
- **Status**: draft

The ICU software shall extract the motor temperature from the motor temperature CAN message (CAN ID 0x302, bytes 2-3, 16-bit signed, degrees Celsius multiplied by 10 for 0.1 degree resolution). The temperature gauge shall display the current motor temperature in degrees Celsius. The gauge shall indicate zones: green (below 60 C), yellow (60-79 C), orange (80-99 C), red (100 C and above). The temperature gauge shall be updated every 200 ms. If no valid temperature message is received for 2 seconds, the gauge shall display "--" (no data).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-006 (motor temperature monitoring) and SYS-044 are correct. The temperature zones (green/yellow/orange/red) are well-defined with specific thresholds. The 0.1 C resolution using x10 scaling is appropriate. The 200 ms update rate and 2-second timeout are appropriate for a slowly-changing thermal parameter. The red zone threshold at 100 C should be cross-checked against the motor derating threshold in the system requirements (SYS-006).
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-004 -->

---

### SWR-ICU-005: Battery Gauge Data Acquisition

- **ASIL**: QM
- **Traces up**: SYS-008, SYS-044
- **Traces down**: firmware/icu/src/gauges.c:ICU_Gauge_Battery()
- **Verified by**: TC-ICU-007
- **Status**: draft

The ICU software shall extract the battery voltage from the battery voltage CAN message (CAN ID 0x303, bytes 2-3, 16-bit unsigned, millivolts). The battery gauge shall display the current voltage in Volts with one decimal place (e.g., "12.4 V"). The gauge shall indicate zones: red (below 10.0 V), yellow (10.0-11.5 V), green (11.5-14.5 V), yellow (14.5-15.0 V), red (above 15.0 V). The battery gauge shall be updated every 500 ms. If no valid battery message is received for 2 seconds, the gauge shall display "--" (no data).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-008 (battery voltage monitoring) and SYS-044 are correct. The voltage zones are appropriate for a 12V automotive system. The dual-sided zones (both under and over voltage) are good engineering practice. The 500 ms update rate is appropriate for a stable power rail. The millivolt-to-Volt display conversion with one decimal is clear and testable. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-005 -->

---

### SWR-ICU-006: Warning Indicator Management

- **ASIL**: QM
- **Traces up**: SYS-044, SYS-046
- **Traces down**: firmware/icu/src/warnings.c:ICU_Warning_Update()
- **Verified by**: TC-ICU-008, TC-ICU-009, TC-ICU-010, TC-ICU-011
- **Status**: draft

The ICU software shall manage the following warning indicators based on received CAN data:

| Indicator | Trigger Condition | Source |
|-----------|------------------|--------|
| Check Engine | Any active DTC received (SWR-ICU-008) | CAN ID 0x400 |
| Temperature Warning | Motor temperature exceeds 80 C | CAN ID 0x302 |
| Battery Warning | Battery voltage below 10.5 V or above 15.0 V | CAN ID 0x303 |
| E-Stop Indicator | E-stop CAN message received (active) | CAN ID 0x001 |
| Overcurrent Warning | Motor current exceeds 20 A (80% of rated) | CAN ID 0x301 |

Each warning indicator shall be represented as a boolean state variable. Warning indicators shall be activated within 100 ms of the triggering condition being detected in the received CAN data. Warning indicators shall be deactivated when the triggering condition clears, with a 2-second minimum display time to ensure the operator has time to observe the warning.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-044 and SYS-046 (fault LED panel) are correct. The warning indicator table is comprehensive with clear trigger conditions and CAN sources. The 2-second minimum display time for transient warnings is good HMI practice. The overcurrent threshold of 20 A (80% of rated 25 A) aligns with the system's motor current monitoring requirements. The 100 ms activation latency is testable. One minor note: the Battery Warning threshold (10.5 V) here differs from the ICU-005 red zone threshold (10.0 V) -- clarify whether warning activation should match the display zone.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-006 -->

---

### SWR-ICU-007: Vehicle State Display

- **ASIL**: QM
- **Traces up**: SYS-029, SYS-044
- **Traces down**: firmware/icu/src/state_display.c:ICU_State_Update()
- **Verified by**: TC-ICU-012, TC-ICU-013
- **Status**: draft

The ICU software shall display the current vehicle state extracted from the vehicle state CAN message (CAN ID 0x100, byte 0 bits 3:0 after E2E header). The vehicle state shall be displayed as a text label:

| State Value | Display Label | Color |
|-------------|---------------|-------|
| 0 (INIT) | "INITIALIZING" | White |
| 1 (RUN) | "RUN" | Green |
| 2 (DEGRADED) | "DEGRADED" | Yellow |
| 3 (LIMP) | "LIMP HOME" | Orange |
| 4 (SAFE_STOP) | "SAFE STOP" | Red |
| 5 (SHUTDOWN) | "SHUTDOWN" | Red (blinking) |

The state display shall be updated within 100 ms of receiving a new vehicle state message. If no vehicle state message is received for 500 ms, the display shall show "NO COMM" in red. The display shall also show the active fault bitmask (bytes 2-3 of vehicle state message) as individual fault code indicators when faults are present.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-007 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-029 (vehicle state machine) and SYS-044 are correct. The state-to-label-to-color mapping table is complete and covers all 6 defined vehicle states. The "NO COMM" timeout display at 500 ms is consistent with the system-wide communication timeout. The use of blinking red for SHUTDOWN is a good visual distinction. Displaying the fault bitmask as individual indicators adds diagnostic value. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-007 -->

---

### SWR-ICU-008: DTC Display

- **ASIL**: QM
- **Traces up**: SYS-038, SYS-041, SYS-044
- **Traces down**: firmware/icu/src/dtc_display.c:ICU_DTC_Update()
- **Verified by**: TC-ICU-014, TC-ICU-015
- **Status**: draft

The ICU software shall receive DTC notification CAN messages (CAN ID 0x400) and maintain a list of active DTCs (maximum 16 entries). Each DTC entry shall contain: DTC number (24-bit, SAE J2012 format), fault status byte (8-bit), and occurrence counter (8-bit). The DTC display shall show the count of active DTCs and, on request (via a scroll mechanism or sequential display), show individual DTC details including the DTC number in hexadecimal format (e.g., "P0562") and the status. When a DTC is cleared (status byte indicates resolved), it shall be removed from the active list after a 5-second display delay. If the DTC list is empty, the display shall show "NO FAULTS".

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-008 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-038 (read/clear DTC), SYS-041 (DTC storage), and SYS-044 are correct. The 16-entry DTC list is a reasonable capacity for a display node. The 5-second display delay on DTC clearing gives the operator time to notice. The scroll/sequential display mechanism is left intentionally flexible, which is appropriate at the requirements level. Note: the DTC notification message format (CAN ID 0x400) is listed as an open item (ICU-O-002) -- this requirement depends on that being finalized.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-008 -->

---

### SWR-ICU-009: ECU Health Monitoring Display

- **ASIL**: QM
- **Traces up**: SYS-021, SYS-044, SYS-046
- **Traces down**: firmware/icu/src/health_display.c:ICU_Health_Update()
- **Verified by**: TC-ICU-016, TC-ICU-017
- **Status**: draft

The ICU software shall monitor heartbeat CAN messages from all zone ECUs (CVC: 0x010, FZC: 0x011, RZC: 0x012) and display the health status of each ECU. For each ECU, the ICU shall track: (a) heartbeat reception (present/absent within 200 ms), (b) operating mode (extracted from heartbeat payload), (c) fault status bitmask (extracted from heartbeat payload). The display shall show each ECU with a status indicator: green circle = heartbeat received and no faults, yellow circle = heartbeat received with faults, red circle = heartbeat timeout. The heartbeat status shall be updated every 100 ms.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-009 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-021 (heartbeat transmission), SYS-044, and SYS-046 are correct. The 200 ms heartbeat timeout aligns with the system-level heartbeat monitoring requirements. The three-state indicator (green/yellow/red) provides clear visual feedback. Monitoring only CVC, FZC, and RZC is correct -- BCM, ICU, and TCU are simulated nodes without heartbeats, and SC is listen-only. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-009 -->

---

### SWR-ICU-010: ICU Main Loop Execution

- **ASIL**: QM
- **Traces up**: SYS-044
- **Traces down**: firmware/icu/src/main.c:ICU_Main()
- **Verified by**: TC-ICU-018
- **Status**: draft

The ICU software shall execute a main loop at a 50 ms cycle time (20 Hz) using a POSIX timer (timer_create with CLOCK_MONOTONIC) or equivalent scheduling mechanism. Each loop iteration shall: (a) read all pending CAN messages from the SocketCAN socket (non-blocking read), (b) update vehicle state from received messages (SWR-ICU-007), (c) update all gauge data (SWR-ICU-002 through SWR-ICU-005), (d) update warning indicators (SWR-ICU-006), (e) update DTC display (SWR-ICU-008), (f) update ECU health display (SWR-ICU-009), (g) render the display output (stdout logging or terminal UI). The ICU display update rate of 20 Hz provides smooth gauge updates for human perception. The main loop shall log a warning if any iteration exceeds 25 ms execution time.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-ICU-010 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The 50 ms (20 Hz) cycle time is appropriate for a display-only node. The 25 ms WCET threshold at 50% of the cycle is consistent with BCM's pattern. The execution order is logical: CAN read first, then data processing, then rendering. All subordinate requirements (SWR-ICU-002 through SWR-ICU-009) are referenced. The display rendering approach (stdout or terminal UI) is intentionally flexible, matching open item ICU-O-003. Trace to SYS-044 is valid. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-ICU-010 -->

---

## 5. Requirements Traceability Summary

### 5.1 SYS to SWR-ICU Mapping

| SYS | SWR-ICU |
|-----|---------|
| SYS-006 | SWR-ICU-004 |
| SYS-008 | SWR-ICU-005 |
| SYS-009 | SWR-ICU-002 |
| SYS-021 | SWR-ICU-009 |
| SYS-029 | SWR-ICU-007 |
| SYS-031 | SWR-ICU-001 |
| SYS-038 | SWR-ICU-008 |
| SYS-041 | SWR-ICU-008 |
| SYS-044 | SWR-ICU-002, SWR-ICU-003, SWR-ICU-004, SWR-ICU-005, SWR-ICU-006, SWR-ICU-007, SWR-ICU-008, SWR-ICU-009, SWR-ICU-010 |
| SYS-046 | SWR-ICU-006, SWR-ICU-009 |

### 5.2 Requirement Summary

| Metric | Value |
|--------|-------|
| Total SWR-ICU requirements | 10 |
| ASIL D | 0 |
| ASIL C | 0 |
| ASIL B | 0 |
| ASIL A | 0 |
| QM | 10 |
| Test cases (placeholder) | TC-ICU-001 to TC-ICU-018 |

## 6. Open Items and Assumptions

### 6.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| ICU-A-001 | Docker container has access to vcan0 SocketCAN interface | ICU CAN communication depends on Docker network configuration |
| ICU-A-002 | Motor status and temperature messages use CAN IDs 0x301, 0x302, 0x303 | Must match CAN matrix finalization |
| ICU-A-003 | DTC notification CAN message (0x400) is broadcast by the Dem module on all ECUs | DTC forwarding mechanism to be defined in BSW requirements |
| ICU-A-004 | ICU display output is to stdout (terminal logging) or a simple terminal UI | No graphical display hardware required for Docker simulation |

### 6.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| ICU-O-001 | Finalize CAN message IDs for telemetry/display domain in CAN matrix | System Architect | SYS.3 phase |
| ICU-O-002 | Define DTC notification CAN message format (ID 0x400) | System Architect | SWE.1 phase |
| ICU-O-003 | Define Docker container UI rendering approach (terminal, web, or log-based) | SW Engineer | Phase 3 |

## 7. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete SWR specification: 10 requirements (SWR-ICU-001 to SWR-ICU-010), traceability, test case mapping |

