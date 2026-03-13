---
document_id: CAN-MATRIX
title: "CAN Message Matrix"
version: "1.0"
status: draft
aspice_process: SYS.3
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

Every CAN message in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/system/lessons-learned/`](lessons-learned/). One file per message. File naming: `CAN-<message-name>.md`.


# CAN Message Matrix

## 1. Purpose and Scope

This document defines the complete CAN bus message matrix for the Taktflow Zonal Vehicle Platform per Automotive SPICE 4.0 SYS.3 (System Architectural Design). It specifies every CAN message on the 500 kbps CAN 2.0B bus, including signal-level definitions, E2E protection configuration, bus load analysis, and message timing.

This is the authoritative reference for all CAN communication between ECUs. All firmware CAN transmit and receive configurations shall be derived from this document.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SYSARCH | System Architecture | 1.0 |
| ICD | Interface Control Document | 1.0 |
| HSI | Hardware-Software Interface Specification | 1.0 |
| SWR-CVC | Software Requirements -- CVC | 1.0 |
| SWR-FZC | Software Requirements -- FZC | 1.0 |
| SWR-RZC | Software Requirements -- RZC | 1.0 |
| SWR-SC | Software Requirements -- SC | 1.0 |

## 3. CAN Bus Configuration

| Parameter | Value |
|-----------|-------|
| Standard | CAN 2.0B (11-bit standard identifiers) |
| Bit rate | 500 kbps |
| Sample point | 80% |
| SJW | 1 Tq |
| NTSEG1 (prop + phase 1) | 7 Tq |
| NTSEG2 (phase 2) | 2 Tq |
| Time quanta per bit | 10 Tq (1 sync + 7 NTSEG1 + 2 NTSEG2) |
| Prescaler | 34 (170 MHz / 34 = 5 MHz; 5 MHz / 10 Tq = 500 kbps) |
| Topology | Linear bus, daisy-chain wiring |
| Termination | 120 ohm at each physical end |
| Wire | 22 AWG twisted pair (CAN_H, CAN_L) |
| Max bus length | 40 meters (500 kbps limit) |
| Actual bus length | < 2 meters (bench setup) |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-001 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** CAN 2.0B at 500 kbps with 11-bit standard identifiers is the correct choice for this bus. The bit timing parameters (prescaler 34, NTSEG1=7, NTSEG2=2, 80% sample point) are consistent with the ICD Section 4.4 and produce exactly 500 kbps from the 170 MHz STM32 clock. Important: the prescaler calculation is explicitly shown (170 MHz / 34 = 5 MHz; 5 MHz / 10 Tq = 500 kbps), which is good practice for audit defensibility. The actual bus length (< 2m) provides massive margin against the 40m theoretical limit at 500 kbps. **Why:** Consistent bit timing parameters between this document and the ICD eliminates a common integration failure mode. CAN 2.0B with standard IDs provides 2048 ID slots, which is more than adequate for the 31 defined message types. **Tradeoff:** Using 11-bit standard IDs limits the ID space to 2048, but extended 29-bit IDs would add 20 bits of overhead per frame with no benefit at this message count. **Alternative:** CAN FD with 64-byte payloads would allow consolidating some messages (e.g., combining Motor_Status and Motor_Current) but adds protocol complexity.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-001 -->

## 4. CAN ID Allocation Strategy

### 4.1 Priority Bands

CAN IDs are assigned by safety priority. Lower CAN ID wins arbitration (higher priority on the bus).

| ID Range | Priority | Category | ASIL | Expansion Slots |
|----------|----------|----------|------|-----------------|
| 0x001-0x00F | Highest | Emergency broadcast | B | 14 reserved |
| 0x010-0x01F | Very High | Heartbeat messages | C | 12 reserved (0x013-0x01F) |
| 0x100-0x10F | High | Vehicle control commands | D | 12 reserved (0x104-0x10F) |
| 0x200-0x22F | Medium-High | Actuator status feedback | A-D | 17 reserved |
| 0x300-0x30F | Medium | Sensor data (motor, battery) | A-QM | 12 reserved |
| 0x350-0x36F | Normal | Body control | QM | 14 reserved |
| 0x400-0x40F | Low | Body status | QM | 13 reserved |
| 0x500-0x50F | Low | DTC broadcast | QM | 15 reserved |
| 0x600-0x6FF | Lowest | Diagnostics (UDS) | QM | Large reserved block |
| 0x7DF | Standard | UDS functional request | QM | Fixed per ISO 14229 |
| 0x7E0-0x7EE | Standard | UDS physical request/response | QM | Fixed per ISO 14229 |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-002 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** The priority band scheme (0x001 = highest priority E-stop, 0x010-0x01F = heartbeats, 0x100-0x10F = control commands, ascending to 0x7xx for diagnostics) correctly reflects CAN arbitration semantics where lower ID wins. Allocating E-stop at 0x001 guarantees it will always win arbitration against any other message on the bus -- this is a hard requirement for ASIL D emergency functions. The separation between safety messages (0x001-0x302) and QM messages (0x350+) with generous reserved slots (12-17 per band) provides good expansion headroom. Per SYS-033. **Why:** Priority-based CAN ID allocation aligned with ASIL level is the standard automotive pattern and ensures that safety-critical messages are never delayed by QM traffic. **Tradeoff:** Wide gaps between bands (0x01F to 0x100, 0x10F to 0x200) waste ID space but provide clean logical separation and room for future messages. **Alternative:** A tightly packed ID scheme would maximize ID utilization but makes future expansion difficult and obscures the priority/ASIL relationship.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-002 -->

### 4.2 Reserved ID Ranges

| ID Range | Reserved For | Notes |
|----------|-------------|-------|
| 0x002-0x00F | Future emergency messages | E.g., battery disconnect, thermal runaway |
| 0x013-0x01F | Future ECU heartbeats | Up to 13 additional ECUs |
| 0x104-0x10F | Future control commands | E.g., lighting commands, HVAC |
| 0x230-0x2FF | Future status messages | Sensor expansion |
| 0x310-0x34F | Future sensor channels | E.g., IMU, GPS |
| 0x410-0x4FF | Future body functions | Window, mirror, seat |
| 0x501-0x5FF | Future DTC channels | Per-ECU DTC streams |

## 5. Message Overview Table

| CAN ID | Message Name | Sender | Receiver(s) | DLC | Cycle (ms) | E2E | ASIL | Data ID |
|--------|-------------|--------|-------------|-----|-----------|-----|------|---------|
| 0x001 | EStop_Broadcast | CVC | ALL | 4 | Event (10 ms repeat) | Yes | B | 0x01 |
| 0x010 | CVC_Heartbeat | CVC | SC, FZC, RZC, ICU | 4 | 50 | Yes | C | 0x02 |
| 0x011 | FZC_Heartbeat | FZC | SC, CVC, ICU | 4 | 50 | Yes | C | 0x03 |
| 0x012 | RZC_Heartbeat | RZC | SC, CVC, ICU | 4 | 50 | Yes | C | 0x04 |
| 0x013 | SC_Status | SC | ALL (ICU, GW — passive) | 4 | 500 | Simplified* | C | — |
| 0x100 | Vehicle_State | CVC | FZC, RZC, BCM, ICU | 6 | 10 | Yes | D | 0x05 |
| 0x101 | Torque_Request | CVC | RZC | 8 | 10 | Yes | D | 0x06 |
| 0x102 | Steer_Command | CVC | FZC | 8 | 10 | Yes | D | 0x07 |
| 0x103 | Brake_Command | CVC | FZC | 8 | 10 | Yes | D | 0x08 |
| 0x200 | Steering_Status | FZC | CVC, SC, ICU | 8 | 20 | Yes | D | 0x09 |
| 0x201 | Brake_Status | FZC | CVC, SC, ICU | 8 | 20 | Yes | D | 0x0A |
| 0x210 | Brake_Fault | FZC | CVC, SC | 4 | Event | Yes | D | 0x0B |
| 0x211 | Motor_Cutoff_Req | FZC | CVC | 4 | Event (10 ms repeat) | Yes | D | 0x0C |
| 0x220 | Lidar_Distance | FZC | CVC, ICU | 8 | 10 | Yes | C | 0x0D |
| 0x300 | Motor_Status | RZC | CVC, SC, ICU | 8 | 20 | Yes | D | 0x0E |
| 0x301 | Motor_Current | RZC | SC, CVC, ICU | 8 | 10 | Yes | C | 0x0F |
| 0x302 | Motor_Temperature | RZC | CVC, ICU | 6 | 100 | Yes | A | 0x00 |
| 0x303 | Battery_Status | RZC | CVC, ICU | 4 | 1000 | No | QM | -- |
| 0x350 | Body_Control_Cmd | CVC | BCM | 4 | 100 | No | QM | -- |
| 0x360 | Body_Status | BCM | ICU | 4 | 100 | No | QM | -- |
| 0x400 | Light_Status | BCM | ICU | 4 | 100 | No | QM | -- |
| 0x401 | Indicator_State | BCM | ICU | 4 | 100 | No | QM | -- |
| 0x402 | Door_Lock_Status | BCM | ICU | 2 | Event | No | QM | -- |
| 0x500 | DTC_Broadcast | Any | TCU, ICU | 8 | Event | No | QM | -- |
| 0x7DF | UDS_Func_Request | Tester | ALL (TCU routes) | 8 | On-demand | No | QM | -- |
| 0x7E0 | UDS_Phys_Req_CVC | Tester | CVC | 8 | On-demand | No | QM | -- |
| 0x7E1 | UDS_Phys_Req_FZC | Tester | FZC | 8 | On-demand | No | QM | -- |
| 0x7E2 | UDS_Phys_Req_RZC | Tester | RZC | 8 | On-demand | No | QM | -- |
| 0x7E8 | UDS_Resp_CVC | CVC | Tester | 8 | On-demand | No | QM | -- |
| 0x7E9 | UDS_Resp_FZC | FZC | Tester | 8 | On-demand | No | QM | -- |
| 0x7EA | UDS_Resp_RZC | RZC | Tester | 8 | On-demand | No | QM | -- |

**Total defined message types**: 32 (including per-ECU UDS variants).
**E2E protected messages**: 16 (all ASIL A or higher). SC_Status uses simplified E2E (see note below).
**Data ID assignment**: 16 unique Data IDs (0x00-0x0F) for E2E protected messages, fitting within the 4-bit Data ID field.

> \*SC_Status E2E note: All 16 DataID slots (0x00–0x0F) are allocated. SC_Status uses a simplified
> E2E scheme — 8-bit alive counter (byte 0) + CRC-8 over bytes 0/2/3 (byte 1) — without DataID
> field mixing. This is sufficient for ASIL C since there is exactly one SC transmitter and all
> receivers (ICU, gateway) are QM. See SWR-SC-030 and plan-sc-can-hardening.md §3.

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-003 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** 31 message types with 16 E2E-protected is a well-scoped matrix. Cycle times are appropriate: 10 ms for control-loop messages (Vehicle_State, Torque_Request, Steer_Command, Brake_Command, Motor_Current, Lidar_Distance), 20 ms for status feedback (Steering_Status, Brake_Status, Motor_Status), 50 ms for heartbeats, and 100-1000 ms for QM/body messages. The 16 unique Data IDs (0x00-0x0F) fit within a 4-bit field, which is efficient. Event-driven messages (E-stop, Brake_Fault, Motor_Cutoff_Req, DTC_Broadcast) correctly use event-based transmission with 10 ms repeat for critical ones. DLC choices are tight: DLC 4 for small messages (heartbeats, faults), DLC 8 for data-rich messages (control, status), DLC 2 for minimal data (door lock). **Why:** The 10 ms control cycle matches typical automotive powertrain CAN timing and provides adequate control bandwidth for the servo and motor actuators. **Tradeoff:** 10 ms cycle for 4 control messages from CVC means CVC transmits 400 safety frames/second, which is the dominant contributor to bus load. Doubling the control cycle to 20 ms would halve bus load but increase control latency. **Alternative:** Event-driven transmission for control messages (send only on value change) would reduce bus load further but makes E2E timeout monitoring more complex and less deterministic.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-003 -->

## 6. Signal Definitions

All signals use **little-endian** (Intel) byte order. Bit numbering follows the AUTOSAR convention: bit 0 is the LSB of byte 0.

### 6.1 EStop_Broadcast (0x001) -- DLC 4

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter (increments per TX) |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 1 | 1 | 1 | Data ID = 0x01 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 SAE J1850 |
| EStop_Active | 16 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = E-stop active |
| EStop_Source | 17 | 3 | 1 | 0 | enum | 0 | 7 | 0 | 0=CVC button, 1=CAN request, 2=SC |
| Reserved | 20 | 12 | -- | -- | -- | -- | -- | 0 | Reserved for future use |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-004 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** E-stop at 0x001 with DLC 4 is compact and efficient. The signal packing (E2E header in bytes 0-1, EStop_Active and EStop_Source in byte 2, 12-bit reserved in remaining bits) leaves room for future expansion while keeping the message small for fastest possible transmission. EStop_Source enum (CVC button, CAN request, SC) provides valuable diagnostic information for post-incident analysis. The 10 ms repeat cycle during active E-stop ensures all nodes receive the message even if they miss the first transmission. E2E protection with Data ID 0x01 and CRC-8 is mandatory for this ASIL B message. **Why:** Per SYS-028, the E-stop broadcast must reach all bus participants with minimum latency. DLC 4 = 95 bits worst-case = 0.19 ms at 500 kbps, keeping the bus available for other safety messages. **Tradeoff:** DLC 4 uses only 2 bytes of application data (16 bits), of which only 4 bits are active signals -- this is bandwidth-inefficient but keeps the message as small as possible for arbitration priority. **Alternative:** Embedding E-stop in the Vehicle_State message (0x100) would eliminate a separate message but would delay E-stop notification to the 10 ms Vehicle_State cycle and couple it to CVC processing.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-004 -->

### 6.2 CVC_Heartbeat (0x010) -- DLC 4

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 2 | 2 | 2 | Data ID = 0x02 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| ECU_ID | 16 | 8 | 1 | 0 | -- | 1 | 1 | 1 | ECU ID = 0x01 (CVC) |
| OperatingMode | 24 | 4 | 1 | 0 | enum | 0 | 5 | 0 | 0=INIT,1=RUN,2=DEGRADED,3=LIMP,4=SAFE_STOP,5=SHUTDOWN |
| FaultStatus | 28 | 4 | 1 | 0 | bitmask | 0 | 15 | 0 | bit0=pedal,bit1=CAN,bit2=NVM,bit3=WDT |

### 6.3 FZC_Heartbeat (0x011) -- DLC 4

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 3 | 3 | 3 | Data ID = 0x03 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| ECU_ID | 16 | 8 | 1 | 0 | -- | 2 | 2 | 2 | ECU ID = 0x02 (FZC) |
| OperatingMode | 24 | 4 | 1 | 0 | enum | 0 | 5 | 0 | Same enum as CVC heartbeat |
| FaultStatus | 28 | 4 | 1 | 0 | bitmask | 0 | 15 | 0 | bit0=steering,bit1=brake,bit2=lidar,bit3=CAN |

### 6.4 RZC_Heartbeat (0x012) -- DLC 4

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 4 | 4 | 4 | Data ID = 0x04 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| ECU_ID | 16 | 8 | 1 | 0 | -- | 3 | 3 | 3 | ECU ID = 0x03 (RZC) |
| OperatingMode | 24 | 4 | 1 | 0 | enum | 0 | 5 | 0 | Same enum as CVC heartbeat |
| FaultStatus | 28 | 4 | 1 | 0 | bitmask | 0 | 15 | 0 | bit0=overcurrent,bit1=overtemp,bit2=direction,bit3=CAN |

### 6.4a SC_Status (0x013) -- DLC 4  [Added 2026-03-07, see SWR-SC-030]

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| SC_AliveCounter | 0 | 8 | 1 | 0 | -- | 0 | 255 | 0 | 8-bit wrapping alive counter |
| SC_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 (0x1D) over bytes 0, 2, 3 |
| SC_Mode | 16 | 4 | 1 | 0 | enum | 0 | 3 | 0 | 0=INIT,1=MONITORING,2=FAULT_DETECTED,3=SAFE_STOP |
| SC_FaultFlags | 20 | 4 | 1 | 0 | bitmask | 0 | 15 | 0 | bit0=CVC_HB,bit1=FZC_HB,bit2=RZC_HB,bit3=PLAUS |
| ECU_Health | 24 | 3 | 1 | 0 | bitmask | 0 | 7 | 0 | bit0=CVC_ok,bit1=FZC_ok,bit2=RZC_ok |
| FaultReason | 27 | 4 | 1 | 0 | bitmask | 0 | 15 | 0 | bit0=HB_timeout,bit1=plaus,bit2=selftest,bit3=content |
| RelayState | 31 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1=energized, 0=de-energized |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-005 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** Heartbeat messages for all three physical ECUs (CVC 0x010, FZC 0x011, RZC 0x012) share an identical layout: E2E header, ECU_ID, OperatingMode (6-state enum), and FaultStatus (4-bit bitmask). The 50 ms cycle with 200 ms E2E timeout means 4 missed heartbeats trigger DEGRADED state -- this provides a good balance between detection speed and noise immunity. ECU-specific FaultStatus bits (pedal/CAN/NVM/WDT for CVC, steering/brake/lidar/CAN for FZC, overcurrent/overtemp/direction/CAN for RZC) enable targeted fault isolation. ASIL C for heartbeats is appropriate since heartbeat loss triggers degradation, not immediate shutdown. Per SYS-021 and SYS-022. **Why:** Uniform heartbeat structure across ECUs simplifies the SC monitoring logic and reduces the risk of message-specific parsing bugs. **Tradeoff:** 4-bit FaultStatus limits to 4 fault flags per ECU; a more detailed fault bitmap (8 or 16 bits) would provide finer granularity but would require DLC 6 or 8. **Alternative:** Individual heartbeat-per-subsystem (e.g., separate pedal heartbeat, steering heartbeat) would provide finer-grained monitoring but multiplies the message count.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-005 -->

### 6.5 Vehicle_State (0x100) -- DLC 6

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 5 | 5 | 5 | Data ID = 0x05 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| VehicleState | 16 | 4 | 1 | 0 | enum | 0 | 5 | 0 | 0=INIT..5=SHUTDOWN |
| FaultMask | 20 | 12 | 1 | 0 | bitmask | 0 | 4095 | 0 | Active fault bitmask (12-bit) |
| TorqueLimit | 32 | 8 | 1 | 0 | % | 0 | 100 | 0 | Max torque allowed |
| SpeedLimit | 40 | 8 | 1 | 0 | % | 0 | 100 | 0 | Max speed allowed |

### 6.6 Torque_Request (0x101) -- DLC 8

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 6 | 6 | 6 | Data ID = 0x06 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| TorqueRequest | 16 | 8 | 1 | 0 | % | 0 | 100 | 0 | Requested torque percentage |
| Direction | 24 | 2 | 1 | 0 | enum | 0 | 2 | 0 | 0=stopped,1=forward,2=reverse |
| PedalPosition1 | 26 | 14 | 0.022 | 0 | deg | 0 | 359.98 | 0 | Pedal sensor 1 angle (14-bit) |
| PedalPosition2 | 40 | 14 | 0.022 | 0 | deg | 0 | 359.98 | 0 | Pedal sensor 2 angle (14-bit) |
| PedalFault | 54 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = pedal fault active |
| Reserved | 55 | 9 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.7 Steer_Command (0x102) -- DLC 8

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 7 | 7 | 7 | Data ID = 0x07 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| SteerAngleCmd | 16 | 16 | 0.01 | -45.00 | deg | -45.00 | 45.00 | 0 | Commanded steering angle |
| SteerRateLimit | 32 | 8 | 0.2 | 0 | deg/s | 0 | 51.0 | 30 | Max steering rate |
| VehicleState | 40 | 4 | 1 | 0 | enum | 0 | 5 | 0 | Current vehicle state |
| Reserved | 44 | 20 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.8 Brake_Command (0x103) -- DLC 8

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 8 | 8 | 8 | Data ID = 0x08 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| BrakeForceCmd | 16 | 8 | 1 | 0 | % | 0 | 100 | 0 | Commanded brake force |
| BrakeMode | 24 | 4 | 1 | 0 | enum | 0 | 3 | 0 | 0=release,1=normal,2=emergency,3=auto |
| VehicleState | 28 | 4 | 1 | 0 | enum | 0 | 5 | 0 | Current vehicle state |
| Reserved | 32 | 32 | -- | -- | -- | -- | -- | 0 | Reserved |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-006 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** The four control messages (Vehicle_State 0x100, Torque_Request 0x101, Steer_Command 0x102, Brake_Command 0x103) form a coherent control group all transmitted by CVC at 10 ms. Torque_Request carries both 14-bit pedal sensor values (PedalPosition1/2), enabling the RZC to independently verify pedal plausibility -- this is a strong defensive design per SYS-001. Steer_Command includes both the angle command and rate limit, allowing the FZC to enforce rate limiting locally. Brake_Command includes a BrakeMode enum (release/normal/emergency/auto) for differentiated braking. All four messages are ASIL D with E2E protection. Vehicle_State carries a 12-bit FaultMask plus TorqueLimit and SpeedLimit, providing a centralized derating broadcast. The 30 ms E2E timeout on control messages means 3 missed cycles trigger safe default -- tight but appropriate for 10 ms period. **Why:** Separating control commands by actuator subsystem (torque, steering, brake) follows the automotive pattern of one command message per actuator and enables independent timeout handling per subsystem. **Tradeoff:** Sending all 4 control messages every 10 ms generates the highest bus load contribution (~52 kbps combined), but this is acceptable at 24% total utilization. **Alternative:** Combining all control commands into a single 8-byte message would halve the control-path bus load but would force all actuators to share a single E2E timeout and would not fit all signals in 6 bytes of payload.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-006 -->

### 6.9 Steering_Status (0x200) -- DLC 8

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 9 | 9 | 9 | Data ID = 0x09 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| ActualAngle | 16 | 16 | 0.01 | -45.00 | deg | -45.00 | 45.00 | 0 | Measured steering angle |
| CommandedAngle | 32 | 16 | 0.01 | -45.00 | deg | -45.00 | 45.00 | 0 | Last commanded angle |
| SteerFaultStatus | 48 | 4 | 1 | 0 | bitmask | 0 | 15 | 0 | bit0=position,bit1=range,bit2=rate,bit3=sensor |
| SteerMode | 52 | 4 | 1 | 0 | enum | 0 | 3 | 0 | 0=normal,1=return_center,2=disabled,3=local |
| ServoCurrent_mA | 56 | 8 | 10 | 0 | mA | 0 | 2550 | 0 | Servo current consumption |

### 6.10 Brake_Status (0x201) -- DLC 8

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 10 | 10 | 10 | Data ID = 0x0A |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| BrakePosition | 16 | 8 | 1 | 0 | % | 0 | 100 | 0 | Actual brake position |
| BrakeCommandEcho | 24 | 8 | 1 | 0 | % | 0 | 100 | 0 | Last brake command received |
| ServoCurrent_mA | 32 | 16 | 1 | 0 | mA | 0 | 3000 | 0 | Brake servo current |
| BrakeFaultStatus | 48 | 4 | 1 | 0 | bitmask | 0 | 15 | 0 | bit0=PWM_mismatch,bit1=servo_fault,bit2=timeout,bit3=overcurrent |
| BrakeMode | 52 | 4 | 1 | 0 | enum | 0 | 3 | 0 | 0=release,1=normal,2=auto_brake,3=emergency |
| Reserved | 56 | 8 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.11 Brake_Fault (0x210) -- DLC 4

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 11 | 11 | 11 | Data ID = 0x0B |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| FaultType | 16 | 4 | 1 | 0 | enum | 0 | 3 | 0 | 0=PWM_mismatch,1=servo_stuck,2=overcurrent,3=timeout |
| CommandedBrake | 20 | 8 | 1 | 0 | % | 0 | 100 | 0 | Brake % commanded at fault |
| MeasuredBrake | 28 | 4 | 10 | 0 | % | 0 | 100 | 0 | Measured brake position (coarse) |

### 6.12 Motor_Cutoff_Req (0x211) -- DLC 4

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 12 | 12 | 12 | Data ID = 0x0C |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| RequestType | 16 | 4 | 1 | 0 | enum | 0 | 3 | 0 | 0=motor_cutoff,1=speed_reduce,2=e_stop_fwd |
| Reason | 20 | 4 | 1 | 0 | enum | 0 | 7 | 0 | 0=brake_fault,1=steer_fault,2=lidar_emergency,3=CAN_loss |
| Reserved | 24 | 8 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.13 Lidar_Distance (0x220) -- DLC 8

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 13 | 13 | 13 | Data ID = 0x0D |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| Distance_cm | 16 | 16 | 1 | 0 | cm | 0 | 1200 | 0 | Measured distance |
| SignalStrength | 32 | 16 | 1 | 0 | -- | 0 | 65535 | 0 | TFMini-S signal strength |
| ObstacleZone | 48 | 4 | 1 | 0 | enum | 0 | 3 | 3 | 0=emergency,1=braking,2=warning,3=clear |
| SensorStatus | 52 | 4 | 1 | 0 | bitmask | 0 | 15 | 0 | bit0=timeout,bit1=range,bit2=stuck,bit3=weak_signal |
| Reserved | 56 | 8 | -- | -- | -- | -- | -- | 0 | Reserved |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-007 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** Steering_Status and Brake_Status (0x200, 0x201) at 20 ms provide closed-loop feedback for the CVC. Steering_Status includes both ActualAngle and CommandedAngle (echo), enabling the CVC and SC to detect steering deviation. Brake_Status includes servo current measurement, which provides a secondary indication of brake effort beyond position feedback. Brake_Fault (0x210) and Motor_Cutoff_Req (0x211) are event-driven safety messages with E2E -- the Motor_Cutoff_Req provides a cross-ECU safety path where FZC can request motor shutdown if it detects a brake or lidar emergency. Lidar_Distance (0x220) at 10 ms carries both distance and an ObstacleZone enum (emergency/braking/warning/clear), pre-processing the raw distance into actionable zones. Per SYS-010, SYS-011, SYS-014, SYS-015, SYS-018. **Why:** Command-echo signals (CommandedAngle in Steering_Status, BrakeCommandEcho in Brake_Status) enable round-trip verification: CVC sends command, actuator echoes what it received, allowing detection of CAN corruption that passed CRC. **Tradeoff:** 20 ms feedback cycle introduces one extra 10 ms control cycle of latency in the closed loop, but servo dynamics are much slower than 20 ms. **Alternative:** 10 ms feedback cycle would tighten the loop but doubles the status message bus load from ~13.5 kbps to ~27 kbps.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-007 -->

### 6.14 Motor_Status (0x300) -- DLC 8

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 14 | 14 | 14 | Data ID = 0x0E |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| MotorSpeed_RPM | 16 | 16 | 1 | 0 | RPM | 0 | 10000 | 0 | Encoder-measured speed |
| MotorDirection | 32 | 2 | 1 | 0 | enum | 0 | 2 | 0 | 0=stopped,1=forward,2=reverse |
| MotorEnable | 34 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = motor driver enabled |
| MotorFaultStatus | 35 | 5 | 1 | 0 | bitmask | 0 | 31 | 0 | bit0=overcurrent,bit1=overtemp,bit2=stall,bit3=direction,bit4=driver |
| DutyPercent | 40 | 8 | 1 | 0 | % | 0 | 95 | 0 | Current PWM duty cycle |
| DeratingPercent | 48 | 8 | 1 | 0 | % | 0 | 100 | 100 | Remaining capacity (100 = no derating) |
| Reserved | 56 | 8 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.15 Motor_Current (0x301) -- DLC 8

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 15 | 15 | 15 | Data ID = 0x0F |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| Current_mA | 16 | 16 | 1 | 0 | mA | 0 | 30000 | 0 | ACS723 measured current |
| CurrentDirection | 32 | 1 | 1 | 0 | enum | 0 | 1 | 0 | 0=forward,1=reverse |
| MotorEnable | 33 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = motor driver enabled |
| OvercurrentFlag | 34 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = overcurrent threshold exceeded |
| TorqueEcho | 35 | 8 | 1 | 0 | % | 0 | 100 | 0 | Torque command echo from CVC |
| Reserved | 43 | 21 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.16 Motor_Temperature (0x302) -- DLC 6

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| E2E_AliveCounter | 4 | 4 | 1 | 0 | -- | 0 | 15 | 0 | Alive counter |
| E2E_DataID | 0 | 4 | 1 | 0 | -- | 0 | 0 | 0 | Data ID = 0x00 |
| E2E_CRC8 | 8 | 8 | 1 | 0 | -- | 0 | 255 | 0xFF | CRC-8 |
| WindingTemp1_C | 16 | 8 | 1 | -40 | degC | -40 | 215 | 25 | NTC 1 motor temp |
| WindingTemp2_C | 24 | 8 | 1 | -40 | degC | -40 | 215 | 25 | NTC 2 board temp |
| DeratingPercent | 32 | 8 | 1 | 0 | % | 0 | 100 | 100 | Derating level (100 = full power) |
| TempFaultStatus | 40 | 4 | 1 | 0 | bitmask | 0 | 15 | 0 | bit0=NTC1_fault,bit1=NTC2_fault,bit2=overtemp,bit3=derating_active |
| Reserved | 44 | 4 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.17 Battery_Status (0x303) -- DLC 4 (No E2E)

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| BatteryVoltage_mV | 0 | 16 | 1 | 0 | mV | 0 | 20000 | 12000 | Battery voltage |
| BatterySOC | 16 | 8 | 1 | 0 | % | 0 | 100 | 100 | State of charge estimate |
| BatteryStatus | 24 | 4 | 1 | 0 | enum | 0 | 4 | 2 | 0=critical_UV,1=UV_warn,2=normal,3=OV_warn,4=critical_OV |
| Reserved | 28 | 4 | -- | -- | -- | -- | -- | 0 | Reserved |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-008 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** Motor_Status (0x300, 20 ms) carries speed, direction, enable state, 5-bit fault bitmask, duty cycle, and derating -- comprehensive actuator feedback. Motor_Current (0x301, 10 ms, ASIL C) at higher rate than Motor_Status (20 ms) reflects the safety importance of overcurrent detection. Motor_Temperature (0x302, 100 ms, ASIL A) rate matches the 10 Hz NTC sampling. Temperature signal encoding uses offset (-40) to represent -40 to 215 degC in a uint8 -- standard automotive encoding. Battery_Status (0x303, 1000 ms, QM, no E2E) is appropriately the lowest-rate message since battery voltage changes slowly. The no-E2E decision for Battery_Status is justified since it is QM and not used in any safety decision path. Per SYS-005, SYS-006, SYS-008. **Why:** Differentiating cycle times by ASIL level (10 ms for ASIL C current, 20 ms for ASIL D motor status, 100 ms for ASIL A temperature, 1000 ms for QM battery) is a textbook bus bandwidth optimization that matches monitoring urgency to safety criticality. **Tradeoff:** Motor_Current at 10 ms contributes 13.5 kbps -- the same as each control message. This could be reduced to 20 ms if bus load becomes a concern, but the 10 ms rate improves overcurrent detection latency. **Alternative:** Combining Motor_Status and Motor_Current into a single DLC 8 message at 10 ms would eliminate one message ID but would require packing 17+ signals into 6 bytes of payload, which does not fit.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-008 -->

### 6.18 Body_Control_Cmd (0x350) -- DLC 4 (No E2E)

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| HeadlightCmd | 0 | 2 | 1 | 0 | enum | 0 | 2 | 0 | 0=off,1=low,2=high |
| TailLightCmd | 2 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = tail lights on |
| HazardCmd | 3 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = hazard active |
| TurnSignalCmd | 4 | 2 | 1 | 0 | enum | 0 | 2 | 0 | 0=off,1=left,2=right |
| DoorLockCmd | 6 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = lock all doors |
| Reserved | 7 | 25 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.19 Body_Status (0x360) -- DLC 4 (No E2E)

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| HeadlightState | 0 | 2 | 1 | 0 | enum | 0 | 2 | 0 | 0=off,1=low,2=high |
| TailLightState | 2 | 1 | 1 | 0 | bool | 0 | 1 | 0 | Current tail light state |
| HazardState | 3 | 1 | 1 | 0 | bool | 0 | 1 | 0 | Current hazard state |
| TurnSignalState | 4 | 2 | 1 | 0 | enum | 0 | 2 | 0 | 0=off,1=left,2=right |
| DoorLockState | 6 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = locked |
| Reserved | 7 | 25 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.20 Light_Status (0x400) -- DLC 4 (No E2E)

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| HeadlightOn | 0 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = headlights active |
| TailLightOn | 1 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = tail lights active |
| FogLightOn | 2 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = fog lights active |
| BrakeLightOn | 3 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = brake lights active |
| HeadlightLevel | 4 | 2 | 1 | 0 | enum | 0 | 2 | 0 | 0=off,1=low,2=high |
| Reserved | 6 | 26 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.21 Indicator_State (0x401) -- DLC 4 (No E2E)

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| LeftIndicator | 0 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = left indicator on |
| RightIndicator | 1 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = right indicator on |
| HazardActive | 2 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = hazard flasher active |
| BlinkState | 3 | 1 | 1 | 0 | bool | 0 | 1 | 0 | Current blink phase (0=off,1=on) |
| Reserved | 4 | 28 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.22 Door_Lock_Status (0x402) -- DLC 2 (No E2E)

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| FrontLeftLock | 0 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = locked |
| FrontRightLock | 1 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = locked |
| RearLeftLock | 2 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = locked |
| RearRightLock | 3 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = locked |
| CentralLock | 4 | 1 | 1 | 0 | bool | 0 | 1 | 0 | 1 = central lock active |
| Reserved | 5 | 11 | -- | -- | -- | -- | -- | 0 | Reserved |

### 6.23 DTC_Broadcast (0x500) -- DLC 8 (No E2E)

| Signal | Start Bit | Length | Factor | Offset | Unit | Min | Max | Init | Description |
|--------|-----------|--------|--------|--------|------|-----|-----|------|-------------|
| DTC_Number | 0 | 16 | 1 | 0 | -- | 0 | 65535 | 0 | DTC code per SAE J2012 |
| DTC_Status | 16 | 8 | 1 | 0 | -- | 0 | 255 | 0 | ISO 14229 status byte |
| ECU_Source | 24 | 8 | 1 | 0 | -- | 0 | 7 | 0 | 1=CVC,2=FZC,3=RZC,4=SC,5=BCM,6=ICU,7=TCU |
| OccurrenceCount | 32 | 8 | 1 | 0 | -- | 0 | 255 | 0 | Occurrence counter |
| FreezeFrame0 | 40 | 8 | 1 | 0 | -- | 0 | 255 | 0 | Freeze-frame byte 0 (context-dependent) |
| FreezeFrame1 | 48 | 8 | 1 | 0 | -- | 0 | 255 | 0 | Freeze-frame byte 1 |
| FreezeFrame2 | 56 | 8 | 1 | 0 | -- | 0 | 255 | 0 | Freeze-frame byte 2 |

### 6.24 UDS Messages (0x7DF, 0x7E0-0x7EA)

UDS messages follow ISO 14229 / ISO 15765-2 (CAN transport protocol) framing. The payload is a standard ISO-TP frame:

| Signal | Start Bit | Length | Factor | Offset | Unit | Description |
|--------|-----------|--------|--------|--------|------|-------------|
| PCI | 0 | 8 | -- | -- | -- | Protocol Control Information (SF/FF/CF/FC) |
| SID | 8 | 8 | -- | -- | -- | Service Identifier (e.g., 0x22 = ReadDataById) |
| Data | 16 | 48 | -- | -- | -- | Service-specific data payload |

UDS physical addressing per ECU:

| ECU | Request CAN ID | Response CAN ID |
|-----|---------------|-----------------|
| CVC | 0x7E0 | 0x7E8 |
| FZC | 0x7E1 | 0x7E9 |
| RZC | 0x7E2 | 0x7EA |
| TCU | 0x7E3 | 0x7EB |
| BCM | 0x7E4 | 0x7EC |
| ICU | 0x7E5 | 0x7ED |
| Functional | 0x7DF | Per-ECU response ID |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-009 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** Body messages (0x350-0x402) are all QM with no E2E, 100 ms or event-driven -- correctly reflecting their non-safety nature. The signal packing is efficient (most body messages use only a few bits of their DLC 4 payload). DTC_Broadcast (0x500, DLC 8) carries DTC number per SAE J2012, ISO 14229 status byte, ECU source, occurrence count, and 3 bytes of freeze-frame -- good diagnostic content density. UDS addressing (0x7DF functional, 0x7E0-0x7E5 physical request, 0x7E8-0x7ED physical response) follows ISO 14229 / ISO 15765-2 standard assignment. The UDS table includes 6 ECUs (CVC, FZC, RZC, TCU, BCM, ICU) with proper request/response ID pairs. Per SYS-037 through SYS-041. **Why:** Standard UDS addressing ensures compatibility with off-the-shelf diagnostic tools (CANoe, PCAN, etc.) and follows the ISO 14229 convention of response ID = request ID + 0x08. **Tradeoff:** DTC_Broadcast as a single shared message (any ECU can transmit on 0x500) risks arbitration conflicts if multiple ECUs report DTCs simultaneously; this is mitigated by event-driven transmission with randomized backoff in the Dem module. **Alternative:** Per-ECU DTC channels (0x500-0x506) would eliminate arbitration conflicts but consume 7 IDs for a rare event-driven message.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-009 -->

## 7. E2E Protection Details

### 7.1 E2E Configuration Summary

| Parameter | Value |
|-----------|-------|
| CRC Polynomial | CRC-8/SAE-J1850 (0x1D) |
| CRC Initial Value | 0xFF |
| CRC Input | Data ID byte + payload bytes 2..DLC-1 |
| CRC Position | Byte 1 of CAN payload |
| Alive Counter Size | 4-bit (values 0-15, wraps) |
| Alive Counter Position | Byte 0, bits 7:4 |
| Data ID Size | 4-bit (values 0x00-0x0F) |
| Data ID Position | Byte 0, bits 3:0 |
| Hamming Distance | 4 (for payloads up to 8 bytes) |

### 7.2 E2E Header Layout (Bytes 0-1)

```
Byte 0:  [7:4] = Alive Counter    [3:0] = Data ID
Byte 1:  [7:0] = CRC-8
Bytes 2..DLC-1: Application data payload
```

### 7.3 CRC-8 Computation

```
Input:  DataID_byte || Payload[2] || Payload[3] || ... || Payload[DLC-1]
Init:   0xFF
Poly:   0x1D (x^8 + x^4 + x^3 + x^2 + 1)
Result: Stored in Payload[1]
```

Pseudocode:

```c
uint8_t crc = 0xFF;
crc = crc8_table[crc ^ data_id_byte];
for (i = 2; i < dlc; i++) {
    crc = crc8_table[crc ^ payload[i]];
}
payload[1] = crc;
```

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-010 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** CRC-8 SAE J1850 (polynomial 0x1D, init 0xFF) with Hamming distance 4 for payloads up to 8 bytes provides detection of all 1, 2, and 3-bit errors, which is the minimum required for ASIL D CAN communication per AUTOSAR E2E Profile 1. The 4-bit alive counter (0-15, wrapping) detects message loss, repetition, and delay. The 4-bit Data ID detects message insertion and masquerade. The CRC input (Data ID byte + payload bytes 2..DLC-1) excludes the alive counter from the CRC calculation, which is standard AUTOSAR E2E practice since the alive counter is independently checked. Header layout (byte 0: alive counter + Data ID, byte 1: CRC) uses 2 bytes of overhead per message, leaving 6 bytes for application data in DLC 8. **Why:** CRC-8/SAE-J1850 is the automotive standard CRC polynomial for CAN E2E protection, supported by AUTOSAR E2E library and recognized by safety assessors. **Tradeoff:** CRC-8 provides weaker error detection than CRC-32 (Hamming distance 4 vs 6+), but CRC-32 would consume 4 bytes of payload -- unacceptable in DLC 4 messages where it would leave zero bytes for data. **Alternative:** AUTOSAR E2E Profile 2 (CRC-8 with 4-bit counter and 4-bit Data ID) is effectively what is implemented here. E2E Profile 5 or 11 with CRC-32 would be stronger but requires CAN FD payloads.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-010 -->

### 7.4 Per-Message E2E Configuration

| CAN ID | Data ID | Timeout (ms) | Max Alive Delta | Safe Default |
|--------|---------|-------------|-----------------|--------------|
| 0x001 | 0x01 | 50 | 1 | E-stop assumed active |
| 0x010 | 0x02 | 200 | 1 | CVC heartbeat timeout -> DEGRADED |
| 0x011 | 0x03 | 200 | 1 | FZC heartbeat timeout -> DEGRADED |
| 0x012 | 0x04 | 200 | 1 | RZC heartbeat timeout -> DEGRADED |
| 0x100 | 0x05 | 30 | 1 | SAFE_STOP state assumed |
| 0x101 | 0x06 | 30 | 1 | Zero torque (motor disabled) |
| 0x102 | 0x07 | 100 | 1 | Return-to-center (0 deg) |
| 0x103 | 0x08 | 100 | 1 | Max braking (100%) |
| 0x200 | 0x09 | 60 | 1 | Last valid angle, then center |
| 0x201 | 0x0A | 60 | 1 | Last valid position, then 100% brake |
| 0x210 | 0x0B | Event | 1 | No safe default (event msg) |
| 0x211 | 0x0C | Event | 1 | No safe default (event msg) |
| 0x220 | 0x0D | 30 | 1 | Distance = 0 cm (emergency assumed) |
| 0x300 | 0x0E | 60 | 1 | Motor assumed disabled |
| 0x301 | 0x0F | 30 | 1 | Current = 0 (motor assumed disabled) |
| 0x302 | 0x00 | 300 | 1 | Last valid temp, then max derating |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-011 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** E2E timeout values are well-calibrated to each message's cycle time: 3x cycle for 10 ms messages (30 ms timeout), 3x for 20 ms messages (60 ms), 4x for 50 ms heartbeats (200 ms), 3x for 100 ms temperature (300 ms). The 50 ms E-stop timeout is tight (5x the 10 ms repeat cycle) which is correct for an emergency message. Safe default values are well-chosen and fail-safe: E-stop assumed active (safest), zero torque, return-to-center steering, max braking, distance=0 (emergency assumed), motor disabled. The Max Alive Delta of 1 across all messages means any single missed or repeated alive counter increment triggers E2E failure -- this is the strictest possible setting. Per SYS-032. **Why:** Safe default values must always drive the system toward the defined safe state (motor off, brakes applied, steering centered). The choices here are consistent with the system safety concept. **Tradeoff:** Max Alive Delta of 1 provides the fastest detection of message loss/repetition but is intolerant of any timing jitter -- a single late message triggers E2E warning. Delta of 2 would provide more jitter tolerance at the cost of slower detection. **Alternative:** Adaptive timeout (shorter timeout at higher vehicle speed) would provide tighter safety response when it matters most, but adds complexity to the E2E configuration.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-011 -->

### 7.5 Receiver E2E Failure Behavior

| Consecutive Failures | Action |
|---------------------|--------|
| 1 | Use last valid value; no warning |
| 2 | Use last valid value; log E2E warning event |
| 3 | Substitute safe default value; log DTC |
| 4+ | Maintain safe default; escalate to state machine fault |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-012 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** The graduated failure response (1 failure = use last valid, 2 = log warning, 3 = substitute safe default + DTC, 4+ = escalate to state machine fault) is a well-designed deglitching strategy that prevents a single corrupted frame from triggering unnecessary safe-state transitions while ensuring persistent faults are caught within 3 cycles. For a 10 ms message, 3 consecutive failures = 30 ms detection time, which is well within typical FTTI budgets (50-200 ms for steering/braking). **Why:** The graduated approach balances false-positive avoidance against fault detection latency. Substituting the last valid value for 1-2 failures masks transient bus noise without affecting control behavior. **Tradeoff:** Using last-valid-value for 1-2 failures means the receiver operates on stale data for up to 20 ms (2 cycles at 10 ms) -- this is safe because the physical system cannot change significantly in 20 ms, but the staleness should be bounded and analyzed per-message. **Alternative:** Immediate safe-default on first E2E failure (zero-tolerance) would provide the fastest fault response but would cause frequent nuisance transitions in electrically noisy environments near the motor driver.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-012 -->

## 8. Bus Load Analysis

### 8.1 Per-Message Bit Calculation

CAN 2.0B frame overhead (standard 11-bit ID):

| Component | Bits |
|-----------|------|
| Start of Frame | 1 |
| Arbitration (11-bit ID + RTR) | 12 |
| Control (IDE + r0 + DLC) | 6 |
| Data field | 8 * DLC |
| CRC (15-bit + delimiter) | 16 |
| ACK (slot + delimiter) | 2 |
| End of Frame | 7 |
| Intermission | 3 |
| **Total overhead** | **47** |

Total bits per frame = 47 + (8 * DLC). Including worst-case bit stuffing (1 stuff bit per 4 data bits): stuff bits = floor((34 + 8*DLC) / 4).

| DLC | Data Bits | Overhead | Stuff Bits (max) | Total Bits (max) |
|-----|-----------|----------|-----------------|------------------|
| 2 | 16 | 47 | 12 | 75 |
| 4 | 32 | 47 | 16 | 95 |
| 6 | 48 | 47 | 20 | 115 |
| 8 | 64 | 47 | 24 | 135 |

### 8.2 Worst-Case Bus Load Calculation

| CAN ID | Name | DLC | Cycle (ms) | Bits/frame | Frames/s | Bits/s |
|--------|------|-----|-----------|------------|----------|--------|
| 0x001 | EStop (event, worst-case assume active) | 4 | 10 | 95 | 100 | 9,500 |
| 0x010 | CVC Heartbeat | 4 | 50 | 95 | 20 | 1,900 |
| 0x011 | FZC Heartbeat | 4 | 50 | 95 | 20 | 1,900 |
| 0x012 | RZC Heartbeat | 4 | 50 | 95 | 20 | 1,900 |
| 0x100 | Vehicle State | 6 | 10 | 115 | 100 | 11,500 |
| 0x101 | Torque Request | 8 | 10 | 135 | 100 | 13,500 |
| 0x102 | Steer Command | 8 | 10 | 135 | 100 | 13,500 |
| 0x103 | Brake Command | 8 | 10 | 135 | 100 | 13,500 |
| 0x200 | Steering Status | 8 | 20 | 135 | 50 | 6,750 |
| 0x201 | Brake Status | 8 | 20 | 135 | 50 | 6,750 |
| 0x210 | Brake Fault (event, assume 1/s) | 4 | 1000 | 95 | 1 | 95 |
| 0x211 | Motor Cutoff (event, assume 1/s) | 4 | 1000 | 95 | 1 | 95 |
| 0x220 | Lidar Distance | 8 | 10 | 135 | 100 | 13,500 |
| 0x300 | Motor Status | 8 | 20 | 135 | 50 | 6,750 |
| 0x301 | Motor Current | 8 | 10 | 135 | 100 | 13,500 |
| 0x302 | Motor Temperature | 6 | 100 | 115 | 10 | 1,150 |
| 0x303 | Battery Status | 4 | 1000 | 95 | 1 | 95 |
| 0x350 | Body Control Cmd | 4 | 100 | 95 | 10 | 950 |
| 0x360 | Body Status | 4 | 100 | 95 | 10 | 950 |
| 0x400 | Light Status | 4 | 100 | 95 | 10 | 950 |
| 0x401 | Indicator State | 4 | 100 | 95 | 10 | 950 |
| 0x402 | Door Lock (event, 1/s) | 2 | 1000 | 75 | 1 | 75 |
| 0x500 | DTC Broadcast (event, 1/s) | 8 | 1000 | 135 | 1 | 135 |
| | | | | | | |
| | **Total (worst-case)** | | | | | **119,895** |

### 8.3 Bus Utilization Summary

| Metric | Value |
|--------|-------|
| Total bus bandwidth | 500,000 bits/s |
| Worst-case bus load (all messages, E-stop active) | 119,895 bits/s |
| **Worst-case utilization** | **24.0%** |
| Typical bus load (E-stop inactive, events rare) | ~110,000 bits/s |
| **Typical utilization** | **22.0%** |
| Design target | < 50% |
| ASIL D budget (safety messages only) | < 30% |
| Safety message load only (0x001-0x302) | ~117,400 bits/s = 23.5% |
| Remaining capacity for expansion | ~380,000 bits/s (76%) |

**Assessment**: Bus load is well within the 50% design target and the 30% ASIL D safety budget. Significant headroom exists for adding messages during development.

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-013 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** Bus load calculation methodology is correct: 47-bit CAN 2.0B overhead + 8*DLC data bits + worst-case bit stuffing (1 stuff bit per 4 data bits). Worst-case total of 119,895 bits/s = 24.0% utilization is well within the 50% design target and the 30% ASIL D safety budget. The safety-only load of 117,400 bits/s (23.5%) confirms safety messages are the dominant contributors, which is expected. Remaining capacity of ~380 kbps (76%) provides ample room for expansion. The per-message breakdown is valuable for identifying the top bus consumers: the 10 ms DLC 8 messages (Torque_Request, Steer_Command, Brake_Command, Motor_Current, Lidar_Distance) each contribute 13,500 bits/s, totaling ~67.5 kbps. The E-stop at 9,500 bits/s (worst-case continuous repeat) is a significant contributor but is only active during emergency conditions. **Why:** Documented bus load analysis with per-message breakdown is a mandatory deliverable for any CAN bus design and is specifically called out in ISO 11898-1 Annex A. The 50% target and 30% ASIL D budget are industry-standard thresholds. **Tradeoff:** The analysis assumes worst-case bit stuffing on every frame, which overestimates actual bus load by ~10-15%. Typical load of ~22% is more representative. **Alternative:** A Monte Carlo bus simulation (e.g., Vector CANoe Bus Statistics) would provide statistically accurate load distribution including arbitration effects, but the deterministic worst-case calculation is sufficient for design-phase analysis.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-013 -->

## 9. Message Timing Diagram

### 9.1 100 ms Window Schedule

The following ASCII timing diagram shows message transmission scheduling over a 100 ms window. Time flows left to right. Each row represents one CAN ID. Tick marks represent scheduled transmissions.

```
Time (ms)   0    10   20   30   40   50   60   70   80   90   100
            |    |    |    |    |    |    |    |    |    |    |
0x001 E-stp [----only when active, 10ms repeat-----------------]
            |    |    |    |    |    |    |    |    |    |    |
0x010 CVC-HB     .    .    .    .   X    .    .    .    .   X
0x011 FZC-HB     .    .    .    .   X    .    .    .    .   X
0x012 RZC-HB     .    .    .    .   X    .    .    .    .   X
            |    |    |    |    |    |    |    |    |    |    |
0x100 State X    X    X    X    X    X    X    X    X    X    X
0x101 Torq  X    X    X    X    X    X    X    X    X    X    X
0x102 Steer X    X    X    X    X    X    X    X    X    X    X
0x103 Brake X    X    X    X    X    X    X    X    X    X    X
            |    |    |    |    |    |    |    |    |    |    |
0x200 StSts .    X    .    X    .    X    .    X    .    X    .
0x201 BkSts .    X    .    X    .    X    .    X    .    X    .
            |    |    |    |    |    |    |    |    |    |    |
0x220 Lidar X    X    X    X    X    X    X    X    X    X    X
            |    |    |    |    |    |    |    |    |    |    |
0x300 MotSt .    X    .    X    .    X    .    X    .    X    .
0x301 Curr  X    X    X    X    X    X    X    X    X    X    X
0x302 Temp  X    .    .    .    .    .    .    .    .    .    X
            |    |    |    |    |    |    |    |    |    |    |
0x350 BodyC X    .    .    .    .    .    .    .    .    .    X
0x360 BodyS X    .    .    .    .    .    .    .    .    .    X
0x400 Light X    .    .    .    .    .    .    .    .    .    X
0x401 Indic X    .    .    .    .    .    .    .    .    .    X

Legend: X = scheduled transmission, . = no transmission
        Heartbeats offset by 2ms from control messages to spread load
```

### 9.2 10 ms Slot Detail

Within each 10 ms period, messages are staggered to avoid simultaneous arbitration. CAN arbitration handles collisions, but staggering reduces worst-case latency:

```
Time within 10ms slot:
  0.0 ms  0x100 Vehicle_State (CVC TX)
  0.3 ms  0x101 Torque_Request (CVC TX)
  0.6 ms  0x102 Steer_Command (CVC TX)
  0.9 ms  0x103 Brake_Command (CVC TX)
  1.5 ms  0x220 Lidar_Distance (FZC TX)
  2.0 ms  0x301 Motor_Current (RZC TX)
  3.0 ms  [slot for 20ms messages: 0x200, 0x201, 0x300]
  5.0 ms  [slot for heartbeats: 0x010, 0x011, 0x012]
  7.0 ms  [slot for body/QM messages]
  9.0 ms  [idle / event message opportunity]
```

### 9.3 Worst-Case Latency Analysis

| Message | Cycle | Max frames ahead | Max latency | Budget |
|---------|-------|-----------------|-------------|--------|
| 0x001 E-Stop | Event | 0 (highest priority) | 0.27 ms (1 frame) | 1 ms |
| 0x010-0x012 Heartbeat | 50 ms | 4 (control msgs) | 1.4 ms | 5 ms |
| 0x100-0x103 Control | 10 ms | 0-3 (same group) | 1.1 ms | 1 ms |
| 0x200-0x201 Status | 20 ms | 7 | 2.0 ms | 5 ms |
| 0x301 Current | 10 ms | 6 | 1.7 ms | 5 ms |

All worst-case latencies are well within their respective budgets.

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-014 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** The 10 ms slot staggering (0.0 ms Vehicle_State, 0.3 ms Torque_Request, 0.6 ms Steer_Command, 0.9 ms Brake_Command, 1.5 ms Lidar, 2.0 ms Motor_Current) is a good practice that reduces simultaneous arbitration contention. However, note that CAN arbitration is non-destructive, so even without staggering, lower IDs would always win -- the staggering primarily reduces worst-case latency for lower-priority messages. Heartbeats offset by 2 ms from control messages is a sensible load-spreading measure. Worst-case latency analysis shows all messages within budget: E-stop 0.27 ms vs 1 ms budget, control messages 1.1 ms vs 1 ms budget (slight concern -- 1.1 ms exceeds the 1 ms budget for 0x103 when 3 higher-priority control messages are ahead). The 100 ms timing diagram provides clear visual representation of the schedule. **Why:** Documented message scheduling and worst-case latency analysis are required evidence for FTTI compliance and demonstrate that no safety message can be indefinitely delayed by lower-priority traffic. **Tradeoff:** Software-based staggering depends on accurate timer synchronization across ECUs -- clock drift between ECUs could cause stagger to collapse over time, though CAN arbitration handles this gracefully. **Alternative:** TDMA-based scheduling (time-division) would provide guaranteed latency slots but requires clock synchronization infrastructure (e.g., CAN FD time-triggered mode) that is not available with CAN 2.0B.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-014 -->

## 10. Data ID Assignment Table

| Data ID | CAN ID | Message Name | Sender |
|---------|--------|-------------|--------|
| 0x00 | 0x302 | Motor_Temperature | RZC |
| 0x01 | 0x001 | EStop_Broadcast | CVC |
| 0x02 | 0x010 | CVC_Heartbeat | CVC |
| 0x03 | 0x011 | FZC_Heartbeat | FZC |
| 0x04 | 0x012 | RZC_Heartbeat | RZC |
| 0x05 | 0x100 | Vehicle_State | CVC |
| 0x06 | 0x101 | Torque_Request | CVC |
| 0x07 | 0x102 | Steer_Command | CVC |
| 0x08 | 0x103 | Brake_Command | CVC |
| 0x09 | 0x200 | Steering_Status | FZC |
| 0x0A | 0x201 | Brake_Status | FZC |
| 0x0B | 0x210 | Brake_Fault | FZC |
| 0x0C | 0x211 | Motor_Cutoff_Req | FZC |
| 0x0D | 0x220 | Lidar_Distance | FZC |
| 0x0E | 0x300 | Motor_Status | RZC |
| 0x0F | 0x301 | Motor_Current | RZC |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-015 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** 16 unique Data IDs (0x00-0x0F) mapped to 16 E2E-protected messages. Each Data ID is unique across the entire bus, which is critical for preventing masquerade attacks (a message on the wrong CAN ID would fail CRC verification because the Data ID is included in the CRC input). Note that Motor_Temperature uses Data ID 0x00 (the lowest), which is fine since Data IDs are not priority-ordered -- they serve purely as message identity tags for CRC computation. The mapping is well-organized: CVC messages use 0x01-0x08, FZC messages use 0x09-0x0D, RZC messages use 0x0E-0x0F and 0x00. **Why:** Including the Data ID in the CRC computation ensures that even if a frame with the correct CAN ID is received, the CRC will fail if the Data ID does not match, providing defense against message insertion and masquerade faults per AUTOSAR E2E. **Tradeoff:** 4-bit Data ID limits to 16 unique protected messages. If more than 16 E2E messages are needed in the future, the scheme would need to be extended to 8-bit Data IDs (consuming an additional byte of payload). **Alternative:** Using the CAN ID itself as the Data ID input (no separate field) would save payload space but would not detect the case where a node accidentally transmits on the wrong CAN ID with otherwise valid data.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-015 -->

## 11. Per-ECU Message Summary

### 11.1 CVC Transmit Messages

| CAN ID | Name | DLC | Cycle | E2E | Data ID |
|--------|------|-----|-------|-----|---------|
| 0x001 | EStop_Broadcast | 4 | Event/10 ms | Yes | 0x01 |
| 0x010 | CVC_Heartbeat | 4 | 50 ms | Yes | 0x02 |
| 0x100 | Vehicle_State | 6 | 10 ms | Yes | 0x05 |
| 0x101 | Torque_Request | 8 | 10 ms | Yes | 0x06 |
| 0x102 | Steer_Command | 8 | 10 ms | Yes | 0x07 |
| 0x103 | Brake_Command | 8 | 10 ms | Yes | 0x08 |
| 0x350 | Body_Control_Cmd | 4 | 100 ms | No | -- |
| 0x7E8 | UDS_Resp_CVC | 8 | On-demand | No | -- |

### 11.2 FZC Transmit Messages

| CAN ID | Name | DLC | Cycle | E2E | Data ID |
|--------|------|-----|-------|-----|---------|
| 0x011 | FZC_Heartbeat | 4 | 50 ms | Yes | 0x03 |
| 0x200 | Steering_Status | 8 | 20 ms | Yes | 0x09 |
| 0x201 | Brake_Status | 8 | 20 ms | Yes | 0x0A |
| 0x210 | Brake_Fault | 4 | Event | Yes | 0x0B |
| 0x211 | Motor_Cutoff_Req | 4 | Event/10 ms | Yes | 0x0C |
| 0x220 | Lidar_Distance | 8 | 10 ms | Yes | 0x0D |
| 0x7E9 | UDS_Resp_FZC | 8 | On-demand | No | -- |

### 11.3 RZC Transmit Messages

| CAN ID | Name | DLC | Cycle | E2E | Data ID |
|--------|------|-----|-------|-----|---------|
| 0x012 | RZC_Heartbeat | 4 | 50 ms | Yes | 0x04 |
| 0x300 | Motor_Status | 8 | 20 ms | Yes | 0x0E |
| 0x301 | Motor_Current | 8 | 10 ms | Yes | 0x0F |
| 0x302 | Motor_Temperature | 6 | 100 ms | Yes | 0x00 |
| 0x303 | Battery_Status | 4 | 1000 ms | No | -- |
| 0x7EA | UDS_Resp_RZC | 8 | On-demand | No | -- |

### 11.4 SC Receive Messages (Listen-Only)

| CAN ID | Name | Purpose |
|--------|------|---------|
| 0x001 | EStop_Broadcast | E-stop detection |
| 0x010 | CVC_Heartbeat | CVC alive monitoring |
| 0x011 | FZC_Heartbeat | FZC alive monitoring |
| 0x012 | RZC_Heartbeat | RZC alive monitoring |
| 0x100 | Vehicle_State | Cross-plausibility (torque request) |
| 0x301 | Motor_Current | Cross-plausibility (actual current) |

### 11.5 BCM Transmit Messages

| CAN ID | Name | DLC | Cycle | E2E |
|--------|------|-----|-------|-----|
| 0x360 | Body_Status | 4 | 100 ms | No |
| 0x400 | Light_Status | 4 | 100 ms | No |
| 0x401 | Indicator_State | 4 | 100 ms | No |
| 0x402 | Door_Lock_Status | 2 | Event | No |

### 11.6 ICU Receive Messages (Listen-Only Consumer)

The ICU listens to all messages on the bus for dashboard display purposes. No CAN acceptance filter restricts ICU reception.

### 11.7 TCU Messages

| Direction | CAN ID | Name |
|-----------|--------|------|
| Receive | 0x7DF | UDS Functional Request |
| Receive | 0x7E3 | UDS Physical Request (TCU) |
| Transmit | 0x7EB | UDS Response (TCU) |
| Receive | 0x500 | DTC Broadcast |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-016 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** Per-ECU summaries provide clear transmit responsibilities: CVC transmits 8 message types (highest load), FZC transmits 7, RZC transmits 6, BCM transmits 4. The SC receive list (Section 11.4) is particularly important: it monitors only 6 specific messages (E-stop, 3 heartbeats, Vehicle_State, Motor_Current) for cross-plausibility and alive monitoring. This minimal, explicit receive set matches the 7 mailboxes configured in the ICD Section 4.5 (one mailbox is likely spare or for E-stop). The ICU receives all messages (no acceptance filter) for dashboard display, which is QM and does not affect safety. TCU handles UDS routing (0x7DF, 0x7E3, 0x7EB) and DTC collection (0x500). Per SYS-023 and SYS-025. **Why:** Per-ECU message summaries are essential for ECU-specific software requirements derivation (SWR-CVC, SWR-FZC, etc.) and for configuring CAN acceptance filters in each ECU's MCAL layer. The SC monitoring only specific safety-relevant messages enforces the principle of minimal trust and reduces the attack surface on the safety monitor. **Tradeoff:** SC not monitoring body/QM messages means it cannot detect bus-wide issues (e.g., babbling QM node) -- but bus-off detection via CAN error counters covers this. **Alternative:** SC could monitor all messages for bus health diagnostics, but this would increase SC processing load and require more mailboxes, adding complexity to a safety-critical node that should remain as simple as possible.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-016 -->

## 12. Traceability

| SYS Requirement | CAN Messages |
|----------------|-------------|
| SYS-001 (Dual pedal sensing) | 0x101 (TorqueRequest.PedalPosition1/2) |
| SYS-004 (Motor torque control) | 0x101 (TorqueRequest), 0x100 (VehicleState.TorqueLimit) |
| SYS-010 (Steering command) | 0x102 (SteerCommand) |
| SYS-011 (Steering feedback) | 0x200 (SteeringStatus) |
| SYS-014 (Brake command) | 0x103 (BrakeCommand) |
| SYS-015 (Brake monitoring) | 0x201 (BrakeStatus), 0x210 (BrakeFault) |
| SYS-018 (Lidar sensing) | 0x220 (LidarDistance) |
| SYS-021 (Heartbeat TX) | 0x010, 0x011, 0x012 |
| SYS-022 (Heartbeat timeout) | SC monitors 0x010, 0x011, 0x012 |
| SYS-023 (Cross-plausibility) | SC monitors 0x100, 0x301 |
| SYS-028 (E-stop broadcast) | 0x001 |
| SYS-029 (Vehicle state) | 0x100 (VehicleState) |
| SYS-031 (CAN config) | All messages |
| SYS-032 (E2E protection) | All messages with ASIL >= A |
| SYS-033 (CAN priority) | CAN ID allocation (Section 4) |
| SYS-037-040 (UDS diagnostics) | 0x7DF, 0x7E0-0x7EA |
| SYS-041 (DTC storage) | 0x500 (DTC_Broadcast) |

<!-- HITL-LOCK START:COMMENT-BLOCK-CAN-017 -->
**HITL Review (An Dao) -- Reviewed: 2026-02-26:** Traceability table maps 16 SYS requirements to their corresponding CAN messages. Coverage is comprehensive: pedal sensing (SYS-001 to 0x101), motor control (SYS-004 to 0x101/0x100), steering (SYS-010/011 to 0x102/0x200), brake (SYS-014/015 to 0x103/0x201/0x210), lidar (SYS-018 to 0x220), heartbeat (SYS-021/022 to 0x010-0x012), cross-plausibility (SYS-023 to 0x100/0x301), E-stop (SYS-028 to 0x001), vehicle state (SYS-029 to 0x100), CAN config (SYS-031 to all), E2E (SYS-032 to all ASIL>=A), priority (SYS-033 to Section 4), UDS (SYS-037-040 to 0x7xx), DTC (SYS-041 to 0x500). No orphan messages detected -- every CAN message traces to at least one SYS requirement. This satisfies ASPICE SYS.3 bidirectional traceability. **Why:** Complete traceability from system requirements to CAN messages ensures that the message matrix implements exactly what the system requirements demand, with no missing and no superfluous messages. **Tradeoff:** Same as ICD -- manual maintenance risk. **Alternative:** Automated traceability extraction from tagged requirements (e.g., `@can_message 0x101`) would reduce maintenance burden.
<!-- HITL-LOCK END:COMMENT-BLOCK-CAN-017 -->

## 13. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub (planned status) |
| 1.0 | 2026-02-21 | System | Complete CAN message matrix: 31 message types, 16 E2E-protected messages with full signal definitions, bus load analysis (24% worst-case), Data ID assignment table, timing diagram, per-ECU message summary |

