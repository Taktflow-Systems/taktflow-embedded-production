---
document_id: SWR-TCU
title: "Software Requirements — TCU"
version: "1.0"
status: draft
aspice_process: SWE.1
ecu: TCU
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

Every requirement (SWR-TCU-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/software/lessons-learned/`](../lessons-learned/). One file per requirement (SWR-TCU-NNN). File naming: `SWR-TCU-NNN-<short-title>.md`.


# Software Requirements — Telematics Control Unit (TCU)

## 1. Purpose

This document specifies the software requirements for the Telematics Control Unit (TCU) of the Taktflow Zonal Vehicle Platform, per Automotive SPICE 4.0 SWE.1. The TCU is a simulated ECU running as a Docker container on a POSIX platform with SocketCAN (vcan0). It provides UDS diagnostic services (ISO 14229), DTC storage and management, OBD-II PID support, and acts as the diagnostic gateway between external test tools and the vehicle CAN bus. All TCU functions are QM-rated.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SYSREQ | System Requirements Specification | 1.0 |
| SG | Safety Goals | 1.0 |
| FSC | Functional Safety Concept | 1.0 |
| CAN-MATRIX | CAN Message Matrix | 0.1 |
| ISO 14229 | Unified Diagnostic Services | 2020 |
| ISO 15031-5 | OBD-II Diagnostic Services | 2015 |
| SAE J2012 | Diagnostic Trouble Code Definitions | 2018 |

## 3. Requirement Conventions

### 3.1 Requirement Structure

Each requirement follows the format:

- **ID**: SWR-TCU-NNN (sequential)
- **Title**: Descriptive name
- **ASIL**: QM (all TCU requirements)
- **Traces up**: SYS-xxx
- **Traces down**: firmware/tcu/src/{file}
- **Verified by**: TC-TCU-xxx (test case ID)
- **Status**: draft | reviewed | approved | implemented | verified

### 3.2 Runtime Environment

The TCU runs as a Docker container (Linux/POSIX) and accesses the CAN bus via SocketCAN (vcan0). It does not use the shared AUTOSAR-like BSW stack. CAN access is via direct POSIX socket API. The TCU acts as a diagnostic server responding to UDS requests from external diagnostic tools.

### 3.3 UDS Addressing

- **Functional addressing**: CAN ID 0x7DF (broadcast to all ECUs)
- **Physical addressing to TCU**: CAN ID 0x604 (request), 0x644 (response)
- **Physical addressing per ECU**: CAN ID 0x600 (CVC), 0x601 (FZC), 0x602 (RZC), 0x603 (SC)
- **Response IDs**: Request ID + 0x40 (e.g., 0x600 request -> 0x640 response)

---

## 4. Requirements

### SWR-TCU-001: SocketCAN Interface Initialization

- **ASIL**: QM
- **Traces up**: SYS-031, SYS-037
- **Traces down**: firmware/tcu/src/can_interface.c:TCU_CAN_Init()
- **Verified by**: TC-TCU-001
- **Status**: draft

The TCU software shall initialize a SocketCAN interface on startup by: (a) creating a raw CAN socket (AF_CAN, SOCK_RAW, CAN_RAW), (b) binding the socket to the vcan0 interface, (c) configuring CAN filters to accept: functional diagnostic requests (0x7DF), physical diagnostic requests (0x604), vehicle status messages (0x100, 0x301, 0x302, 0x303), heartbeat messages (0x010, 0x011, 0x012), DTC notification messages (0x400), and E-stop (0x001). If initialization fails, the TCU shall log an error and retry every 1 second up to 10 attempts.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-001 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-031 and SYS-037 are correct. The CAN filter set is comprehensive for a diagnostic gateway node. The filter includes both functional (0x7DF) and physical (0x604) UDS addressing, vehicle data messages for DID/PID serving, and ECU heartbeats. Note: the filter should also include physical addresses for ECU routing (0x600-0x603 per SWR-TCU-012) -- these appear to be missing from the filter list.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-001 -->

---

### SWR-TCU-002: UDS Diagnostic Session Control (0x10)

- **ASIL**: QM
- **Traces up**: SYS-037
- **Traces down**: firmware/tcu/src/uds/uds_session.c:UDS_SessionControl()
- **Verified by**: TC-TCU-002, TC-TCU-003, TC-TCU-004
- **Status**: draft

The TCU software shall implement UDS Diagnostic Session Control (service ID 0x10) per ISO 14229 with the following sub-functions:

| Sub-function | Session | Requirements |
|-------------|---------|-------------|
| 0x01 | Default Session | Always accessible. No security access required. Limited to read-only services. |
| 0x02 | Programming Session | Requires Security Access level 3 (SWR-TCU-007). Enables ECU reprogramming services. |
| 0x03 | Extended Diagnostic Session | Requires Security Access level 1 (SWR-TCU-007). Enables write services and DTC clearing. |

The TCU shall maintain a session timer (P2_server = 50 ms, P2_star_server = 5000 ms). If no diagnostic request is received within the session timeout period (default: 5 seconds for non-default sessions), the TCU shall automatically revert to the Default Session. On positive response, the TCU shall send response 0x50 followed by the sub-function byte and the session timing parameters (P2 and P2_star).

**Negative Response Codes (NRC)**:
- 0x12 (subFunctionNotSupported): Sub-function other than 0x01, 0x02, 0x03.
- 0x22 (conditionsNotCorrect): Attempt to enter Programming Session while vehicle state is RUN.
- 0x33 (securityAccessDenied): Attempt to enter restricted session without prior Security Access.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-002 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-037 is correct. The session types (Default, Programming, Extended) follow ISO 14229 convention. The P2/P2* timing parameters are specified. The session timeout (5s for non-default) prevents stuck sessions. The NRC list covers the key failure modes. The restriction on Programming Session while RUN is a good safety precaution. One note: the requirement does not specify behavior for concurrent session requests from multiple testers -- consider if this is relevant for the simulated environment.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-002 -->

---

### SWR-TCU-003: UDS Clear Diagnostic Information (0x14)

- **ASIL**: QM
- **Traces up**: SYS-038
- **Traces down**: firmware/tcu/src/uds/uds_dtc.c:UDS_ClearDTC()
- **Verified by**: TC-TCU-005, TC-TCU-006
- **Status**: draft

The TCU software shall implement UDS Clear Diagnostic Information (service ID 0x14) per ISO 14229. The request shall contain a 3-byte DTC group identifier:

| Group ID | Meaning |
|----------|---------|
| 0xFFFFFF | Clear all DTCs |
| 0x000000 to 0xFFFFFE | Clear specific DTC group (per SAE J2012 grouping) |

On successful clearing, the TCU shall: (a) reset the fault status byte of matching DTCs to 0x00, (b) reset occurrence counters to 0, (c) remove freeze-frame data for cleared DTCs, (d) send positive response 0x54. Clearing requires Extended Diagnostic Session (0x03) with Security Access level 1 granted.

**Negative Response Codes (NRC)**:
- 0x13 (incorrectMessageLengthOrInvalidFormat): Request length not equal to 4 bytes (SID + 3-byte group).
- 0x22 (conditionsNotCorrect): Not in Extended Diagnostic Session.
- 0x31 (requestOutOfRange): DTC group identifier not recognized.
- 0x33 (securityAccessDenied): Security Access level 1 not granted.
- 0x72 (generalProgrammingFailure): NVM write failure during DTC clear.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-038 is correct. The clear DTC service follows ISO 14229 conventions properly. The group clearing (0xFFFFFF for all, specific group for selective) is standard. The four-step clearing process (reset status, reset counters, remove freeze-frames, positive response) is complete. The NRC list covers all relevant failure modes including NVM write failure. The requirement for Extended Diagnostic Session + Security Access is properly gatekeeping destructive operations. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-003 -->

---

### SWR-TCU-004: UDS Read DTC Information (0x19)

- **ASIL**: QM
- **Traces up**: SYS-038
- **Traces down**: firmware/tcu/src/uds/uds_dtc.c:UDS_ReadDTC()
- **Verified by**: TC-TCU-007, TC-TCU-008, TC-TCU-009, TC-TCU-010
- **Status**: draft

The TCU software shall implement UDS Read DTC Information (service ID 0x19) per ISO 14229 with the following sub-functions:

| Sub-function | Name | Description |
|-------------|------|-------------|
| 0x01 | reportNumberOfDTCByStatusMask | Return count of DTCs matching the status mask |
| 0x02 | reportDTCByStatusMask | Return list of DTCs matching the status mask |
| 0x04 | reportDTCSnapshotRecordByDTCNumber | Return freeze-frame data for a specific DTC |
| 0x06 | reportDTCExtendedDataRecordByDTCNumber | Return extended data (occurrence count, aging counter) for a specific DTC |

For sub-function 0x01, the response shall contain: availability status mask (1 byte), DTC format identifier (0x01 = SAE J2012-DA), and DTC count (2 bytes, high byte first).

For sub-function 0x02, the response shall contain: availability status mask, followed by a list of (DTC number [3 bytes] + status byte [1 byte]) entries for all DTCs matching the requested status mask.

For sub-function 0x04, the response shall contain: DTC number (3 bytes), status byte (1 byte), snapshot record number (1 byte), followed by snapshot data (vehicle state, speed, temperature, battery voltage, motor current at time of fault).

**Negative Response Codes (NRC)**:
- 0x12 (subFunctionNotSupported): Sub-function not in {0x01, 0x02, 0x04, 0x06}.
- 0x13 (incorrectMessageLengthOrInvalidFormat): Invalid request length for the sub-function.
- 0x31 (requestOutOfRange): DTC number not found (for sub-functions 0x04, 0x06).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-004 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-038 is correct. The four supported sub-functions (0x01, 0x02, 0x04, 0x06) cover the most commonly used DTC reporting services. The response formats for each sub-function are well-specified with byte-level detail. The snapshot data fields in sub-function 0x04 align with the freeze-frame content defined in SWR-TCU-009. The NRC list is complete for the supported sub-functions. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-004 -->

---

### SWR-TCU-005: UDS Read Data By Identifier (0x22)

- **ASIL**: QM
- **Traces up**: SYS-039
- **Traces down**: firmware/tcu/src/uds/uds_rdbi.c:UDS_ReadDataByIdentifier()
- **Verified by**: TC-TCU-011, TC-TCU-012, TC-TCU-013
- **Status**: draft

The TCU software shall implement UDS Read Data By Identifier (service ID 0x22) per ISO 14229 for the following Data Identifiers (DIDs):

| DID | Name | Length | Source |
|-----|------|--------|--------|
| 0xF100 | Vehicle Speed | 2 bytes (RPM, unsigned) | CAN ID 0x301 (last received) |
| 0xF101 | Motor Temperature | 2 bytes (0.1 C, signed) | CAN ID 0x302 (last received) |
| 0xF102 | Battery Voltage | 2 bytes (mV, unsigned) | CAN ID 0x303 (last received) |
| 0xF103 | Motor Current | 2 bytes (mA, unsigned) | CAN ID 0x301 (last received) |
| 0xF104 | Vehicle State | 1 byte (state enum) | CAN ID 0x100 (last received) |
| 0xF190 | VIN (Vehicle Identification Number) | 17 bytes (ASCII) | NVM stored |
| 0xF191 | Hardware Version | 8 bytes (ASCII) | Compiled constant |
| 0xF195 | Software Version | 8 bytes (ASCII) | Compiled constant |
| 0xF1A0 | Torque Request | 1 byte (0-100%) | CAN ID 0x100 (last received) |

Multiple DIDs may be requested in a single 0x22 request (multi-DID read). The response shall contain each DID followed by its data. DIDs sourced from CAN messages shall return the most recently received value. If no CAN message has been received for a given source, the DID shall return 0xFF fill bytes and set a "data not available" flag in the response.

**Negative Response Codes (NRC)**:
- 0x13 (incorrectMessageLengthOrInvalidFormat): Request length not a multiple of 2 after SID.
- 0x14 (responseTooLong): Total response exceeds maximum CAN frame capacity (use multi-frame if needed).
- 0x31 (requestOutOfRange): DID not in the supported list.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-005 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-039 is correct. The DID table is comprehensive with clear source mappings to CAN message IDs. The multi-DID read support is a useful feature. The "data not available" handling with 0xFF fill bytes is a reasonable default. Note: DID 0xF1A0 (Torque Request) is an addition beyond the standard VIN/version DIDs -- ensure this is documented in the DID catalog (open item TCU-O-004). The NRC list includes 0x14 (responseTooLong) which shows awareness of multi-frame transport limits. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-005 -->

---

### SWR-TCU-006: UDS Write Data By Identifier (0x2E)

- **ASIL**: QM
- **Traces up**: SYS-039
- **Traces down**: firmware/tcu/src/uds/uds_wdbi.c:UDS_WriteDataByIdentifier()
- **Verified by**: TC-TCU-014, TC-TCU-015
- **Status**: draft

The TCU software shall implement UDS Write Data By Identifier (service ID 0x2E) per ISO 14229 for the following writable DIDs:

| DID | Name | Length | Security Level Required |
|-----|------|--------|------------------------|
| 0xF190 | VIN | 17 bytes | Level 1 (calibration) |
| 0xF1B0 | Pedal Threshold | 2 bytes (counts) | Level 1 (calibration) |
| 0xF1B1 | Overcurrent Threshold | 2 bytes (mA) | Level 1 (calibration) |
| 0xF1B2 | Lidar Warning Distance | 2 bytes (cm) | Level 1 (calibration) |
| 0xF1B3 | Lidar Braking Distance | 2 bytes (cm) | Level 1 (calibration) |
| 0xF1B4 | Lidar Emergency Distance | 2 bytes (cm) | Level 1 (calibration) |

Write operations shall require: (a) Extended Diagnostic Session (0x03) active, (b) Security Access level 1 granted, (c) vehicle state is not RUN (writing calibration data while driving is prohibited). Written values shall be stored in non-volatile memory (simulated via file persistence in the Docker container). A positive response (0x6E + DID) shall be sent on successful write.

**Negative Response Codes (NRC)**:
- 0x13 (incorrectMessageLengthOrInvalidFormat): Data length does not match expected DID length.
- 0x22 (conditionsNotCorrect): Vehicle state is RUN or not in Extended Session.
- 0x31 (requestOutOfRange): DID not writable or unknown.
- 0x33 (securityAccessDenied): Security Access not granted.
- 0x72 (generalProgrammingFailure): NVM write failure.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-039 is correct. The writable DID table includes both standard (VIN) and project-specific calibration parameters (pedal threshold, overcurrent threshold, lidar distances). The triple-gating (Extended Session + Security Access + vehicle not RUN) is appropriately restrictive for calibration writes. NVM persistence via file I/O in Docker is a practical simulation approach. The NRC list is complete. The lidar distance DIDs (0xF1B2-0xF1B4) provide runtime-configurable obstacle detection thresholds, which is a good design choice.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-006 -->

---

### SWR-TCU-007: UDS Security Access (0x27)

- **ASIL**: QM
- **Traces up**: SYS-040
- **Traces down**: firmware/tcu/src/uds/uds_security.c:UDS_SecurityAccess()
- **Verified by**: TC-TCU-016, TC-TCU-017, TC-TCU-018, TC-TCU-019
- **Status**: draft

The TCU software shall implement UDS Security Access (service ID 0x27) per ISO 14229 with a seed-and-key mechanism supporting two access levels:

| Level | Sub-function (seed) | Sub-function (key) | Purpose |
|-------|--------------------|--------------------|---------|
| Level 1 | 0x01 | 0x02 | Calibration access (write DIDs, clear DTCs) |
| Level 3 | 0x05 | 0x06 | Programming access (ECU reprogramming) |

**Seed generation**: The TCU shall generate a 4-byte random seed using a pseudo-random number generator seeded with the system clock. A zero seed (0x00000000) shall indicate that security is already unlocked for the requested level.

**Key computation**: The expected key shall be computed as: key = seed XOR 0xA5A5A5A5 (for level 1) or key = seed XOR 0x5A5A5A5A (for level 3). This is a simplified algorithm suitable for a demonstration project.

**Lockout**: After 3 consecutive failed key attempts, the TCU shall lock security access for 10 seconds (NRC 0x37 requiredTimeDelayNotExpired). A failed attempt counter shall be maintained per access level.

**Negative Response Codes (NRC)**:
- 0x12 (subFunctionNotSupported): Sub-function not in {0x01, 0x02, 0x05, 0x06}.
- 0x22 (conditionsNotCorrect): Key sent without a prior seed request.
- 0x24 (requestSequenceError): Key sub-function received without matching seed sub-function.
- 0x35 (invalidKey): Key does not match expected value.
- 0x36 (exceededNumberOfAttempts): 3 consecutive failures.
- 0x37 (requiredTimeDelayNotExpired): Lockout period not elapsed.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-007 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-040 is correct. The two-level security access (Level 1 for calibration, Level 3 for programming) follows ISO 14229 sub-function conventions. The XOR-based key computation is clearly documented as a simplified demo algorithm (TCU-A-005 assumption). The lockout mechanism (3 failures, 10 seconds) provides basic brute-force protection. The NRC list is comprehensive and covers the full seed-key handshake failure modes. The zero seed indicating "already unlocked" is per ISO 14229 convention. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-007 -->

---

### SWR-TCU-008: DTC Storage and Management

- **ASIL**: QM
- **Traces up**: SYS-041
- **Traces down**: firmware/tcu/src/dtc/dtc_storage.c:DTC_Store()
- **Verified by**: TC-TCU-020, TC-TCU-021, TC-TCU-022
- **Status**: draft

The TCU software shall maintain a DTC database with a capacity of at least 64 DTC entries. Each DTC entry shall contain:

| Field | Size | Description |
|-------|------|-------------|
| DTC Number | 3 bytes | SAE J2012 format (e.g., 0x056200 = P0562) |
| Status Byte | 1 byte | ISO 14229 DTC status bits (testFailed, confirmedDTC, etc.) |
| Occurrence Counter | 1 byte | Increments each time the fault is detected (saturates at 255) |
| Aging Counter | 1 byte | Increments each drive cycle where the fault is not detected |
| First Occurrence Timestamp | 4 bytes | UNIX timestamp of first fault detection |
| Last Occurrence Timestamp | 4 bytes | UNIX timestamp of most recent fault detection |
| Freeze-Frame Data | 16 bytes | Snapshot: vehicle state (1), speed (2), temp (2), voltage (2), current (2), torque (1), reserved (6) |

The TCU shall receive DTC events from physical ECUs via CAN message (CAN ID 0x400) and store them in the local database. The DTC database shall be persisted to a file (simulating NVM in Docker) at every DTC status change and on graceful shutdown. On startup, the TCU shall load the persisted DTC database from the file.

**DTC Status Management**:
- **testFailed** (bit 0): Set when fault is actively detected, cleared when fault condition clears.
- **confirmedDTC** (bit 3): Set after the fault has been detected for 3 consecutive drive cycles (maturation).
- **testNotCompletedSinceLastClear** (bit 4): Set after DTC clear, cleared after first successful test cycle.

**Aging**: If a confirmed DTC is not detected for 40 consecutive drive cycles, the DTC shall be aged out (status byte set to 0x00, entry marked as available for reuse).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-008 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-041 is correct. The DTC database structure is well-defined with 64-entry capacity and complete field descriptions. The DTC status management follows ISO 14229 conventions (testFailed, confirmedDTC, testNotCompletedSinceLastClear). The maturation count of 3 drive cycles and aging count of 40 drive cycles are reasonable values. File-based persistence in Docker is practical. One consideration: define how "drive cycle" is detected in the simulated environment (e.g., INIT-to-RUN transition).
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-008 -->

---

### SWR-TCU-009: DTC Freeze-Frame Capture

- **ASIL**: QM
- **Traces up**: SYS-041
- **Traces down**: firmware/tcu/src/dtc/dtc_storage.c:DTC_CaptureSnapshot()
- **Verified by**: TC-TCU-023, TC-TCU-024
- **Status**: draft

The TCU software shall capture a freeze-frame snapshot at the moment a DTC is first detected (testFailed transitions from 0 to 1). The freeze-frame shall contain the following data captured from the most recently received CAN messages:

| Field | Source | Offset | Size |
|-------|--------|--------|------|
| Vehicle State | CAN 0x100 | 0 | 1 byte |
| Motor Speed | CAN 0x301 | 1 | 2 bytes |
| Motor Temperature | CAN 0x302 | 3 | 2 bytes |
| Battery Voltage | CAN 0x303 | 5 | 2 bytes |
| Motor Current | CAN 0x301 | 7 | 2 bytes |
| Torque Request | CAN 0x100 | 9 | 1 byte |
| Reserved | - | 10 | 6 bytes |

The freeze-frame shall not be overwritten on subsequent occurrences of the same DTC (first occurrence snapshot is retained until the DTC is cleared).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-009 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-041 is correct. The freeze-frame data layout is detailed with byte offsets and sizes, totaling 16 bytes (10 data + 6 reserved). The field set (vehicle state, speed, temp, voltage, current, torque) captures the essential operating conditions at fault time. The "first occurrence only" retention policy is standard practice. The freeze-frame layout is consistent with the snapshot data described in SWR-TCU-004 sub-function 0x04. The 6 reserved bytes provide room for future expansion. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-009 -->

---

### SWR-TCU-010: OBD-II PID Support

- **ASIL**: QM
- **Traces up**: SYS-038
- **Traces down**: firmware/tcu/src/obd/obd_pids.c:OBD_ProcessRequest()
- **Verified by**: TC-TCU-025, TC-TCU-026, TC-TCU-027
- **Status**: draft

The TCU software shall respond to OBD-II PID requests (ISO 15031-5 / SAE J1979) received on CAN ID 0x7DF (functional addressing). The TCU shall support the following standard PIDs:

| Mode | PID | Name | Formula | Source |
|------|-----|------|---------|--------|
| 0x01 | 0x00 | Supported PIDs (01-20) | Bitmask | Computed |
| 0x01 | 0x04 | Calculated Engine Load | torque_pct * 2.55 | CAN 0x100 |
| 0x01 | 0x05 | Engine Coolant Temperature | temp_C + 40 | CAN 0x302 |
| 0x01 | 0x0C | Engine RPM | rpm * 4 | CAN 0x301 |
| 0x01 | 0x0D | Vehicle Speed | speed_kmh | Computed from RPM |
| 0x01 | 0x42 | Control Module Voltage | voltage_mV / 1000 * 1000 | CAN 0x303 |
| 0x03 | - | Request Emission-Related DTCs | DTC list | DTC database |
| 0x04 | - | Clear Emission-Related DTCs | Clear all | DTC database |
| 0x09 | 0x02 | VIN | 17-char ASCII | NVM stored |

OBD-II responses shall use CAN ID 0x644 (TCU response). Mode 0x01 PID 0x00 shall report the supported PID bitmask. Unsupported PIDs shall be silently ignored (no negative response per OBD-II convention).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-010 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-038 is correct. The PID table covers essential Mode 01 vehicle parameters plus Modes 03, 04, and 09 for DTC and VIN access. The OBD-II formulas (e.g., temp + 40, rpm * 4) follow SAE J1979 standard scaling. The silent ignore for unsupported PIDs is per OBD-II convention. One note: Mode 04 (Clear DTCs) via OBD-II does not appear to require Security Access, unlike UDS service 0x14 (SWR-TCU-003) -- clarify if OBD-II clearing should also require security gating or if it follows the less restrictive OBD-II convention.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-010 -->

---

### SWR-TCU-011: UDS Transport Layer (ISO-TP)

- **ASIL**: QM
- **Traces up**: SYS-037, SYS-038
- **Traces down**: firmware/tcu/src/uds/iso_tp.c:ISOTP_Process()
- **Verified by**: TC-TCU-028, TC-TCU-029
- **Status**: draft

The TCU software shall implement ISO 15765-2 (ISO-TP) for multi-frame UDS message transport over CAN. The implementation shall support:

- **Single Frame (SF)**: Messages up to 7 bytes (SID + data) transmitted in a single CAN frame.
- **First Frame (FF)**: First frame of a multi-frame message, containing total length and first 6 data bytes.
- **Consecutive Frame (CF)**: Subsequent frames with sequence number (0x00-0x0F, wrapping).
- **Flow Control (FC)**: Sent by receiver to control transmission rate (block size, separation time).

The TCU shall support a maximum message length of 4095 bytes (ISO-TP maximum). The block size shall be set to 0 (unlimited consecutive frames) and the separation time shall be set to 0 ms (fastest transmission) for simplicity in the simulated environment. A receive timeout of 1000 ms shall be applied between consecutive frames; if exceeded, the multi-frame reception shall be aborted.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-011 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-037 and SYS-038 are correct. The ISO-TP implementation covers all four frame types (SF, FF, CF, FC). The simplified block size (0) and separation time (0 ms) are appropriate for a simulated vcan0 environment where bus load is not a concern. The 1000 ms consecutive frame timeout is standard. The 4095-byte maximum follows ISO 15765-2. No gaps identified; this provides the transport foundation for all multi-frame UDS services.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-011 -->

---

### SWR-TCU-012: Diagnostic Request Routing

- **ASIL**: QM
- **Traces up**: SYS-037
- **Traces down**: firmware/tcu/src/uds/uds_router.c:UDS_RouteRequest()
- **Verified by**: TC-TCU-030, TC-TCU-031
- **Status**: draft

The TCU software shall route incoming UDS diagnostic requests based on the CAN ID:

| Receive CAN ID | Target | Action |
|---------------|--------|--------|
| 0x7DF | Functional (all ECUs) | TCU processes locally; forwards to physical ECUs if service requires hardware access |
| 0x604 | Physical (TCU) | TCU processes locally |
| 0x600 | Physical (CVC) | Forward to CVC via CAN (if CVC supports UDS) |
| 0x601 | Physical (FZC) | Forward to FZC via CAN (if FZC supports UDS) |
| 0x602 | Physical (RZC) | Forward to RZC via CAN (if RZC supports UDS) |
| 0x603 | Physical (SC) | Not supported (SC is listen-only); return NRC 0x11 |

For requests addressed to the TCU (0x604 or 0x7DF), the TCU shall process the request locally using its UDS service handlers. For requests addressed to physical ECUs (0x600-0x603), the TCU shall act as a diagnostic gateway and forward the request on the CAN bus, then relay the response back to the tester.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-012 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-037 is correct. The routing table clearly defines all CAN IDs and their dispatch actions. The NRC 0x11 for SC (0x603) is correct since SC is listen-only. The gateway forwarding for CVC/FZC/RZC depends on open item TCU-O-003 (whether physical ECUs support local UDS). This is a critical architectural decision. The routing reinforces the earlier note that SWR-TCU-001 should include CAN IDs 0x600-0x603 in its filter list.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-012 -->

---

### SWR-TCU-013: Negative Response Code Handling

- **ASIL**: QM
- **Traces up**: SYS-037
- **Traces down**: firmware/tcu/src/uds/uds_nrc.c:UDS_SendNRC()
- **Verified by**: TC-TCU-032, TC-TCU-033
- **Status**: draft

The TCU software shall send UDS Negative Response messages (SID 0x7F) when a service request cannot be fulfilled. The negative response format shall be: 0x7F + request SID + NRC byte. The TCU shall support the following NRCs per ISO 14229:

| NRC | Value | Meaning |
|-----|-------|---------|
| serviceNotSupported | 0x11 | Requested SID not implemented |
| subFunctionNotSupported | 0x12 | Sub-function not supported for the given SID |
| incorrectMessageLengthOrInvalidFormat | 0x13 | Message length incorrect |
| responseTooLong | 0x14 | Response exceeds transport capacity |
| conditionsNotCorrect | 0x22 | Preconditions not met |
| requestSequenceError | 0x24 | Messages received in wrong sequence |
| requestOutOfRange | 0x31 | Parameter value out of range |
| securityAccessDenied | 0x33 | Security level not sufficient |
| invalidKey | 0x35 | Security key incorrect |
| exceededNumberOfAttempts | 0x36 | Too many failed security attempts |
| requiredTimeDelayNotExpired | 0x37 | Lockout timer active |
| generalProgrammingFailure | 0x72 | NVM write failed |

For SID 0x7F responses on functionally addressed requests (0x7DF), the TCU shall only send a negative response for NRC 0x12 and 0x13. All other NRCs on functional requests shall be suppressed (no response sent) per ISO 14229 convention.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-013 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-037 is correct. The NRC table is comprehensive and covers all ISO 14229 NRCs used across the TCU's UDS services. The functional addressing NRC suppression rule (only respond with 0x12 and 0x13 on 0x7DF) is per ISO 14229 convention and correctly specified. The 0x7F response format is standard. This requirement properly centralizes all NRC definitions, which aids consistency. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-013 -->

---

### SWR-TCU-014: CAN Data Aggregation for DIDs

- **ASIL**: QM
- **Traces up**: SYS-037, SYS-039
- **Traces down**: firmware/tcu/src/data/data_aggregator.c:TCU_Data_Update()
- **Verified by**: TC-TCU-034
- **Status**: draft

The TCU software shall continuously receive and cache the latest values from the following CAN messages for use by UDS Read Data By Identifier and OBD-II PID services:

| CAN ID | Data Cached | Cache Timeout |
|--------|-------------|---------------|
| 0x100 | Vehicle state, torque request, fault bitmask | 500 ms |
| 0x301 | Motor current, motor speed, motor direction | 500 ms |
| 0x302 | Motor temperature | 2000 ms |
| 0x303 | Battery voltage | 2000 ms |
| 0x010, 0x011, 0x012 | ECU heartbeats (mode, fault status) | 200 ms |

Each cached value shall include a timestamp of last reception. When a DID or PID is requested and the cached value is older than its cache timeout, the TCU shall return the stale value but include a "data not current" indication in the DID response (if applicable) or log a warning.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-014 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Traces to SYS-037 and SYS-039 are correct. The cache timeout values are appropriate: 500 ms for fast-changing data (vehicle state, motor), 2000 ms for slow-changing data (temperature, voltage), 200 ms for heartbeats. The stale data handling (return with warning) is a pragmatic approach for a diagnostic gateway. This requirement properly decouples CAN reception from UDS response timing. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-014 -->

---

### SWR-TCU-015: TCU Main Loop Execution

- **ASIL**: QM
- **Traces up**: SYS-037
- **Traces down**: firmware/tcu/src/main.c:TCU_Main()
- **Verified by**: TC-TCU-035
- **Status**: draft

The TCU software shall execute a main loop at a 10 ms cycle time (100 Hz) using a POSIX timer or equivalent scheduling mechanism. Each loop iteration shall: (a) read all pending CAN messages from the SocketCAN socket (non-blocking read), (b) update cached data values (SWR-TCU-014), (c) process any pending UDS/OBD-II requests (SWR-TCU-002 through SWR-TCU-013), (d) process ISO-TP multi-frame state machine (SWR-TCU-011), (e) manage session timeout timers (SWR-TCU-002), (f) manage security access lockout timers (SWR-TCU-007), (g) manage DTC aging counters (SWR-TCU-008). The main loop shall log a warning if any iteration exceeds 5 ms execution time.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-TCU-015 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Trace to SYS-037 is correct. The 10 ms cycle time matches the BCM's main loop for consistency. The execution order is well-structured: CAN read, then cache update, then request processing, then timer management. All subordinate requirements are cross-referenced. The 5 ms WCET threshold at 50% of the cycle is consistent across all simulated ECUs. The DTC aging counter management in the main loop is correctly included. No gaps identified.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-TCU-015 -->

---

## 5. Requirements Traceability Summary

### 5.1 SYS to SWR-TCU Mapping

| SYS | SWR-TCU |
|-----|---------|
| SYS-031 | SWR-TCU-001 |
| SYS-037 | SWR-TCU-001, SWR-TCU-002, SWR-TCU-011, SWR-TCU-012, SWR-TCU-013, SWR-TCU-014, SWR-TCU-015 |
| SYS-038 | SWR-TCU-003, SWR-TCU-004, SWR-TCU-010, SWR-TCU-011 |
| SYS-039 | SWR-TCU-005, SWR-TCU-006, SWR-TCU-014 |
| SYS-040 | SWR-TCU-007 |
| SYS-041 | SWR-TCU-008, SWR-TCU-009 |

### 5.2 Requirement Summary

| Metric | Value |
|--------|-------|
| Total SWR-TCU requirements | 15 |
| ASIL D | 0 |
| ASIL C | 0 |
| ASIL B | 0 |
| ASIL A | 0 |
| QM | 15 |
| Test cases (placeholder) | TC-TCU-001 to TC-TCU-035 |

## 6. Open Items and Assumptions

### 6.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| TCU-A-001 | Docker container has access to vcan0 SocketCAN interface | TCU CAN communication depends on Docker network configuration |
| TCU-A-002 | DTC notification messages (CAN ID 0x400) are broadcast by physical ECU Dem modules | DTC forwarding mechanism must be implemented in BSW Dem |
| TCU-A-003 | NVM persistence is simulated via file I/O in the Docker container | DTC database and calibration data use file storage |
| TCU-A-004 | ISO-TP multi-frame support uses a maximum message size of 4095 bytes | Sufficient for all UDS services in this project |
| TCU-A-005 | Security Access seed-key algorithm (XOR-based) is for demonstration only | Production would use a cryptographically secure algorithm |

### 6.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| TCU-O-001 | Finalize UDS CAN addressing (request/response IDs) in CAN matrix | System Architect | SYS.3 phase |
| TCU-O-002 | Define DTC notification CAN message format (ID 0x400) | System Architect | SWE.1 phase |
| TCU-O-003 | Determine if physical ECUs (CVC, FZC, RZC) need local UDS service handlers | System Architect | SWE.2 phase |
| TCU-O-004 | Define complete DID catalog with all readable/writable parameters | SW Engineer | SWE.1 phase |

## 7. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete SWR specification: 15 requirements (SWR-TCU-001 to SWR-TCU-015), UDS services, OBD-II, DTC management, full traceability |

