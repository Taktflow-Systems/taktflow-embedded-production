---
document_id: SWR-GW
title: "Software Requirements — Gateway"
version: "1.0"
status: draft
aspice_process: SWE.1
ecu: GW
asil: QM
date: 2026-02-25
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


# Software Requirements — Raspberry Pi Edge Gateway (GW)

## 1. Purpose

This document specifies the software requirements for the Raspberry Pi Edge Gateway of the Taktflow Zonal Vehicle Platform, per Automotive SPICE 4.0 SWE.1. The gateway bridges the CAN bus to the cloud via MQTT and provides edge ML anomaly detection and SAP QM mock integration. All gateway functions are QM-rated (informational/advisory only, no safety-critical decisions).

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SYSREQ | System Requirements Specification | 1.0 |
| CAN-MATRIX | CAN Message Matrix | 0.1 |

## 3. Requirement Conventions

### 3.1 Requirement Structure

Each requirement follows the format:

- **ID**: SWR-GW-NNN (sequential)
- **Title**: Descriptive name
- **ASIL**: QM (all GW requirements)
- **Traces up**: SYS-xxx
- **Traces down**: gateway/{file}
- **Verified by**: TC-GW-xxx (test case ID)
- **Status**: draft | reviewed | approved | implemented | verified

### 3.2 Runtime Environment

The gateway runs on a Raspberry Pi (Linux/Python). It reads CAN bus data via SocketCAN, publishes MQTT telemetry to AWS IoT Core, and forwards DTC events to a mock SAP QM API.

---

## 4. MQTT Telemetry Requirements

### SWR-GW-001: MQTT Telemetry Publishing

- **ASIL**: QM
- **Traces up**: SYS-042
- **Traces down**: gateway/telemetry.py:publish_telemetry()
- **Verified by**: TC-GW-001, TC-GW-002
- **Verification method**: Integration test + demonstration
- **Status**: draft

The gateway software shall publish vehicle telemetry to AWS IoT Core via MQTT v3.1.1 over TLS 1.2 (port 8883) using X.509 client certificate authentication. Telemetry shall be batched at a rate of 1 message per 5 seconds. The telemetry JSON payload shall include: timestamp (ISO 8601), motor speed (RPM), motor current (A), motor temperature (C), battery voltage (V), vehicle state, steering angle, and lidar distance. The software shall publish to the following topics: `vehicle/telemetry` (periodic data), `vehicle/dtc/new` (new DTC events), `vehicle/dtc/soft` (soft DTC events), and `vehicle/alerts` (anomaly alerts). On MQTT connection loss, the software shall queue messages (up to 100) and flush on reconnect with exponential backoff (1s, 2s, 4s, max 60s).

---

### SWR-GW-002: CAN Bus Data Acquisition

- **ASIL**: QM
- **Traces up**: SYS-042
- **Traces down**: gateway/can_reader.py:read_can_bus()
- **Verified by**: TC-GW-003
- **Verification method**: Integration test
- **Status**: draft

The gateway software shall read CAN messages from the bus via SocketCAN interface. The software shall decode relevant CAN message IDs (motor status, vehicle state, DTC notifications) according to the CAN message matrix. Decoded values shall be stored in a shared data structure for telemetry publishing and ML inference.

---

## 5. Edge ML Requirements

### SWR-GW-003: Edge ML Anomaly Detection

- **ASIL**: QM
- **Traces up**: SYS-043
- **Traces down**: gateway/ml_inference.py:detect_anomalies()
- **Verified by**: TC-GW-004, TC-GW-005
- **Verification method**: Unit test + demonstration
- **Status**: draft

The gateway software shall run ML inference (scikit-learn Isolation Forest or equivalent) on CAN bus data to detect anomalous patterns in: motor current waveform, motor temperature trajectory, and CAN message timing jitter. The inference shall run at a configurable interval (default: every 5 seconds). When an anomaly score exceeds the configured threshold (default: 0.7), the software shall publish an alert to the `vehicle/alerts` MQTT topic containing: timestamp, anomaly type, anomaly score, and relevant sensor values. ML inference results shall NOT be used for any safety-critical decision — they are informational only.

---

### SWR-GW-004: ML Model Loading and Validation

- **ASIL**: QM
- **Traces up**: SYS-043
- **Traces down**: gateway/ml_inference.py:load_model()
- **Verified by**: TC-GW-006
- **Verification method**: Unit test
- **Status**: draft

The gateway software shall load the ML model from a file (default: `gateway/models/anomaly_model.pkl`) on startup. On load failure (file missing, corrupt, incompatible version), the software shall log a warning and disable ML inference without affecting telemetry or SAP QM functionality. The software shall validate the model by running a known-good test vector and verifying the output is within expected bounds.

---

## 6. SAP QM Mock Integration Requirements

### SWR-GW-005: SAP QM DTC Forwarding

- **ASIL**: QM
- **Traces up**: SYS-056
- **Traces down**: gateway/sap_qm_mock/client.py:forward_dtc()
- **Verified by**: TC-GW-007, TC-GW-008
- **Verification method**: Integration test + demonstration
- **Status**: draft

The gateway software shall forward new DTC events to the SAP QM mock API endpoint via HTTP POST. The request payload shall include: DTC number, fault status byte, occurrence count, first/last occurrence timestamps, freeze-frame data (vehicle state, speed, sensor readings), and device identity. The mock API shall return a quality notification number (Q-Meldung). On HTTP failure (timeout, 4xx, 5xx), the software shall retry up to 3 times with 2-second intervals, then log the failure and continue.

---

### SWR-GW-006: 8D Report Template Generation

- **ASIL**: QM
- **Traces up**: SYS-056
- **Traces down**: gateway/sap_qm_mock/report.py:generate_8d_report()
- **Verified by**: TC-GW-009
- **Verification method**: Unit test
- **Status**: draft

The gateway software shall auto-generate an 8D report template upon receiving a Q-Meldung number from the SAP QM mock API. The template shall contain: D1 (team: auto-populated from config), D2 (problem description: DTC details and freeze-frame), D3 (containment: auto-populated with default actions), D4-D8 (empty sections for manual completion). The report shall be stored as a JSON file in `gateway/reports/` with naming convention `8D-{Q-Meldung}-{timestamp}.json`.

---

## 7. Requirements Summary

| Category | Count | SWR Range |
|----------|-------|-----------|
| MQTT Telemetry | 2 | SWR-GW-001 to SWR-GW-002 |
| Edge ML | 2 | SWR-GW-003 to SWR-GW-004 |
| SAP QM Mock | 2 | SWR-GW-005 to SWR-GW-006 |
| **Total** | **6** | SWR-GW-001 to SWR-GW-006 |
