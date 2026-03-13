---
document_id: HARA
title: "Hazard Analysis and Risk Assessment"
version: "1.0"
status: draft
iso_26262_part: 3
iso_26262_clause: "7"
aspice_process: SYS.1
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

Every hazard in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/safety/lessons-learned/`](../lessons-learned/). One file per hazard. File naming: `HARA-<hazard>.md`.


# Hazard Analysis and Risk Assessment

<!-- DECISION: ADR-002 — HARA scope: ratings assume real vehicle operation per A-001 -->

## 1. Purpose

This document identifies hazardous events arising from malfunctions of the Taktflow Zonal Vehicle Platform, assesses the associated risk according to ISO 26262-3 Clause 7, and determines the Automotive Safety Integrity Level (ASIL) for each hazardous event. The HARA serves as the foundation for deriving safety goals (ISO 26262-3 Clause 8) and the functional safety concept (ISO 26262-3 Clause 9).

## 2. Scope

### 2.1 Item Under Analysis

The item is the Taktflow Zonal Vehicle Platform as defined in the Item Definition (document ID: ITEM-DEF). The item comprises 7 ECU nodes (4 physical, 3 simulated), sensors, actuators, safety hardware, and CAN bus communication infrastructure. The item provides the following vehicle-level functions:

| Function ID | Function Name | Description | ECUs Involved |
|-------------|---------------|-------------|---------------|
| F-DBW | Drive-by-Wire | Pedal input to motor torque control | CVC, RZC |
| F-STR | Steering Control | Steering command to servo angle | CVC, FZC |
| F-BRK | Braking Control | Brake command to servo force, emergency braking | CVC, FZC |
| F-DIST | Distance Sensing | Lidar obstacle detection and response | FZC |
| F-SAF | Safety Monitoring | Independent heartbeat, plausibility, kill relay | SC |
| F-BODY | Body Control | Headlights, indicators, door locks | BCM |
| F-DIAG | Diagnostics | UDS, DTCs, dashboard display, telemetry | ICU, TCU |

### 2.2 Assumption on Operational Context

Per assumption A-001 in the Item Definition, all Severity (S), Exposure (E), and Controllability (C) ratings assume the platform controls a real vehicle in real traffic scenarios. This is intentional to demonstrate full competence with the ISO 26262 HARA process and to derive meaningful ASIL levels. The platform is physically an indoor bench demo; the elevated ratings are a standard practice in academic and portfolio HARA work.

## 3. Methodology

### 3.1 Process

The HARA follows the process prescribed in ISO 26262-3 Clause 7:

1. **Situation Analysis**: Define operational situations covering the vehicle's usage profile.
2. **Hazard Identification**: Systematically identify malfunctioning behaviors for each item-level function and determine the resulting hazardous events.
3. **Risk Assessment**: For each hazardous event, independently assess Severity (S), Exposure (E), and Controllability (C).
4. **ASIL Determination**: Determine the ASIL using the ISO 26262-3 Table 4 matrix.

### 3.2 Participants

The HARA was conducted in a cross-functional workshop format per ISO 26262-3, 7.4.1.1.

| Role | Responsibility |
|------|----------------|
| FSE Lead (Functional Safety Engineer) | Workshop facilitator, S/E/C rating moderation, ASIL determination |
| System Engineer | System architecture expertise, interface knowledge, function descriptions |
| HW Engineer | Sensor/actuator characteristics, failure modes, hardware constraints |
| SW Engineer | Software function behavior, error detection capability, diagnostic coverage |
| Domain Expert (Vehicle Dynamics) | Driving scenarios, vehicle behavior under fault, controllability assessment |

### 3.3 References

| Document | Version | Relevance |
|----------|---------|-----------|
| ISO 26262-3:2018 | 2nd edition | HARA process, S/E/C classification tables, ASIL matrix |
| ITEM-DEF (Item Definition) | 1.0 | Functions, interfaces, system boundary, assumptions |
| Master Plan | current | Platform architecture, demo scenarios, ECU roles |
| ISO 26262-3 Table B.1 | -- | Severity classification guidance |
| ISO 26262-3 Table B.2 | -- | Exposure classification guidance |
| ISO 26262-3 Table B.3 | -- | Controllability classification guidance |
| ISO 26262-3 Table 4 | -- | ASIL determination matrix |

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE0 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Sections 1-3 establish a solid foundation for the HARA. The purpose, scope, and methodology are clearly stated and aligned with ISO 26262-3 Clause 7. The cross-functional workshop format (Section 3.2) follows best practice. The explicit acknowledgment that S/E/C ratings assume real-vehicle operation (Section 2.2, referencing A-001) is essential context for all subsequent ratings. The reference table is complete with the correct ISO 26262-3 tables cited.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE0 -->

## 4. Operational Situations

The following operational situations represent the driving/usage scenarios under which the vehicle platform could be operating. These are used to contextualize each hazardous event.

### OS-1: Driving Forward at Moderate Speed

| Parameter | Value |
|-----------|-------|
| Speed | 30-80 km/h |
| Direction | Forward |
| Road type | Urban or suburban road, mixed traffic |
| Driver attention | Normal (eyes on road, hands on wheel) |
| Environment | Dry or wet road surface, daylight or artificial lighting |
| Traffic density | Medium (other vehicles, pedestrians, cyclists nearby) |

**Rationale**: This is the most common driving scenario and represents the highest exposure to hazards from drive-by-wire, steering, and braking malfunctions.

### OS-2: Stationary (Vehicle at Rest)

| Parameter | Value |
|-----------|-------|
| Speed | 0 km/h |
| Direction | None |
| Road type | Parking lot, roadside, intersection stop |
| Driver attention | Variable (may be distracted — phone, passengers) |
| Environment | Any surface, any lighting condition |
| Traffic density | Variable (parked or stopped in traffic) |

**Rationale**: While the vehicle is not moving, unintended acceleration or steering movement can cause harm to nearby pedestrians, other vehicles, or the occupants themselves.

### OS-3: Turning at Low Speed

| Parameter | Value |
|-----------|-------|
| Speed | 5-30 km/h |
| Direction | Turning left or right |
| Road type | Intersection, curve, roundabout |
| Driver attention | High (actively steering, checking mirrors/blind spots) |
| Environment | Mixed traffic, potential pedestrians in crosswalk |
| Traffic density | Medium to high |

**Rationale**: Turning requires active steering control. Loss of steering or unintended steering movement during a turn presents severe risk of leaving the lane or striking obstacles/pedestrians in the turning path.

### OS-4: Braking (Active Deceleration)

| Parameter | Value |
|-----------|-------|
| Speed | 10-80 km/h, decelerating |
| Direction | Forward |
| Road type | Any — approaching stop sign, traffic light, obstacle, vehicle ahead |
| Driver attention | High (actively braking, focused on stopping distance) |
| Environment | Any surface condition (dry, wet, gravel) |
| Traffic density | Variable |

**Rationale**: Loss of braking during active deceleration directly affects the ability to stop before an obstacle or intersection, with potentially fatal consequences.

### OS-5: Reversing at Low Speed

| Parameter | Value |
|-----------|-------|
| Speed | 0-10 km/h |
| Direction | Reverse |
| Road type | Parking lot, driveway, garage |
| Driver attention | Divided (looking backward, using mirrors, limited visibility) |
| Environment | Confined spaces, potential pedestrians behind vehicle |
| Traffic density | Low but pedestrians may be close |

**Rationale**: Reversing involves limited visibility. Unintended acceleration in reverse or motor direction errors can strike pedestrians or objects behind the vehicle.

### OS-6: Diagnostic Mode (Service/Maintenance)

| Parameter | Value |
|-----------|-------|
| Speed | 0 km/h (vehicle stationary, ignition on) |
| Direction | None |
| Road type | Service bay, workshop |
| Driver attention | Technician focused on diagnostic tool, not on vehicle motion |
| Environment | Indoor service environment, people may be near vehicle |
| Traffic density | None (controlled environment) |

**Rationale**: During diagnostic sessions, unexpected vehicle motion (motor activation, servo movement) poses risk to service technicians who are not expecting motion. The vehicle is assumed to be on a lift or with wheels blocked, reducing but not eliminating risk.

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE1 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The 7 operational situations provide good coverage of the vehicle usage profile. OS-1 (moderate speed), OS-3 (turning), OS-4 (braking), and OS-7 (high speed) cover the primary driving scenarios. OS-2 (stationary), OS-5 (reversing), and OS-6 (diagnostic mode) cover non-driving situations where hazards can still occur. Each OS includes relevant parameters (speed, direction, road type, driver attention, environment, traffic density) which supports consistent S/E/C rating. One potential gap: there is no operational situation for "driving on a slope/gradient" which could affect braking and acceleration hazard analysis. For a portfolio demonstration this is acceptable, but in a production HARA an incline scenario would typically be included.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE1 -->

### OS-7: Driving Forward at High Speed

| Parameter | Value |
|-----------|-------|
| Speed | 80-130 km/h |
| Direction | Forward |
| Road type | Highway, expressway |
| Driver attention | Normal to low (highway monotony, possible fatigue) |
| Environment | Multi-lane, high closing speeds, barriers |
| Traffic density | Variable (free-flow to dense) |

**Rationale**: High-speed driving amplifies the severity of any loss-of-control event. Unintended acceleration, steering deviation, or braking failure at highway speeds is more likely to result in life-threatening outcomes.

## 5. Malfunctioning Behaviors

The following malfunctioning behaviors are systematically derived from each item-level function. Each malfunction is linked to the function and the ECU(s) responsible.

### 5.1 Pedal / Drive-by-Wire Malfunctions (F-DBW)

| MB-ID | Malfunction | Function | ECU | Mechanism |
|-------|-------------|----------|-----|-----------|
| MB-001 | Both pedal sensors read high (stuck/offset/drift) | F-DBW | CVC | SPI communication error, sensor magnet displacement, or EMI causing both AS5048A sensors to report maximum angle simultaneously |
| MB-002 | Both pedal sensors read low (stuck at zero) | F-DBW | CVC | SPI bus failure (MISO stuck low), sensor power loss, or magnet missing |
| MB-003 | One pedal sensor fails (disagreement) | F-DBW | CVC | Single sensor cable break, connector fault, or sensor IC failure |
| MB-004 | Torque request stuck at last value (CAN TX failure) | F-DBW | CVC | FDCAN peripheral hang, TJA1051T transceiver fault, or CAN bus short |
| MB-005 | Motor does not respond to torque request (RZC hang) | F-DBW | RZC | RZC software hang, BTS7960 enable pin fault, or motor winding open circuit |
| MB-006 | Motor runs at full power uncontrolled | F-DBW | RZC | BTS7960 H-bridge FET short-circuit, PWM stuck at 100%, or RZC software runaway |

### 5.2 Motor Control Malfunctions (F-DBW)

| MB-ID | Malfunction | Function | ECU | Mechanism |
|-------|-------------|----------|-----|-----------|
| MB-007 | Motor overcurrent not detected | F-DBW | RZC | ACS723 sensor failure, ADC channel fault, or overcurrent threshold misconfiguration |
| MB-008 | Motor overtemperature not detected | F-DBW | RZC | NTC sensor open circuit, ADC channel fault, or threshold misconfiguration |
| MB-009 | Motor direction reversal while driving forward | F-DBW | RZC | H-bridge direction control logic error, GPIO fault (R_EN/L_EN), or CAN message corruption inverting direction bit |
| MB-010 | Motor cannot be stopped (enable pin stuck active) | F-DBW | RZC | BTS7960 R_EN or L_EN pin stuck high (GPIO output fault), FET gate short |

### 5.3 Steering Malfunctions (F-STR)

| MB-ID | Malfunction | Function | ECU | Mechanism |
|-------|-------------|----------|-----|-----------|
| MB-011 | Unintended steering movement (wrong angle command) | F-STR | FZC | CAN message corruption, FZC software logic error, or servo PWM calculation fault |
| MB-012 | Loss of steering response (servo not responding) | F-STR | FZC | Servo mechanical failure, PWM output fault, servo power loss, or FZC software hang |
| MB-013 | Steering angle sensor failure (no feedback) | F-STR | FZC | AS5048A SPI failure, sensor disconnection, or magnet displacement |

### 5.4 Braking Malfunctions (F-BRK)

| MB-ID | Malfunction | Function | ECU | Mechanism |
|-------|-------------|----------|-----|-----------|
| MB-014 | Loss of braking (servo not applying force) | F-BRK | FZC | Servo mechanical failure, PWM output fault, servo power loss, or FZC software hang |
| MB-015 | Unintended braking (brake applied without command) | F-BRK | FZC | CAN message corruption with brake command, FZC software error, servo PWM glitch |
| MB-016 | Brake force insufficient (partial application) | F-BRK | FZC | Servo mechanical wear, linkage loosening, servo power brownout |

### 5.5 Distance Sensing Malfunctions (F-DIST)

| MB-ID | Malfunction | Function | ECU | Mechanism |
|-------|-------------|----------|-----|-----------|
| MB-017 | Lidar reports no obstacle when obstacle present (false negative) | F-DIST | FZC | TFMini-S lens contamination, UART communication error, sensor blind spot, target reflectivity below threshold |
| MB-018 | Lidar reports obstacle when none present (false positive) | F-DIST | FZC | TFMini-S sensor noise, reflective surface glare, UART data corruption, EMI |
| MB-019 | Lidar stuck at last value (frozen output) | F-DIST | FZC | TFMini-S internal fault, UART DMA stall, or sensor power glitch |

### 5.6 Safety Monitoring Malfunctions (F-SAF)

| MB-ID | Malfunction | Function | ECU | Mechanism |
|-------|-------------|----------|-----|-----------|
| MB-020 | Safety Controller fails to detect ECU timeout | F-SAF | SC | DCAN peripheral fault, heartbeat logic error, RTI timer failure |
| MB-021 | Kill relay does not open on fault (relay stuck closed) | F-SAF | SC | Relay contact welding, MOSFET gate short, GIO output fault |
| MB-022 | Safety Controller hangs (no monitoring) | F-SAF | SC | Software hang (mitigated by lockstep + external WDT), but both could theoretically fail |

### 5.7 CAN Bus Malfunctions

| MB-ID | Malfunction | Function | ECU | Mechanism |
|-------|-------------|----------|-----|-----------|
| MB-023 | Total CAN bus failure (bus-off, open circuit) | All | All | CAN_H or CAN_L wire break, both termination resistors removed, transceiver failure on dominant node |
| MB-024 | CAN message corruption (bit errors beyond CRC) | All | All | EMI, ground loop, improper termination, transceiver degradation |
| MB-025 | CAN bus babbling node (one ECU floods the bus) | All | Any | ECU software failure sending continuous high-priority frames, locking out other nodes |

### 5.8 E-Stop Malfunction

| MB-ID | Malfunction | Function | ECU | Mechanism |
|-------|-------------|----------|-----|-----------|
| MB-026 | E-stop button pressed but system does not stop | F-SAF | CVC | GPIO input debounce failure, EXTI interrupt disabled, CVC software hang preventing broadcast |

### 5.9 Power Supply Malfunctions

| MB-ID | Malfunction | Function | ECU | Mechanism |
|-------|-------------|----------|-----|-----------|
| MB-027 | Battery overvoltage (>16V) or undervoltage (<9V) | F-DBW | RZC | Bench supply regulator fault, load dump, or supply failure |

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE2 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The malfunctioning behaviors are systematically derived from each item-level function with 27 distinct malfunctions covering F-DBW (10), F-STR (3), F-BRK (3), F-DIST (3), F-SAF (3), CAN bus (3), E-stop (1), and power supply (1). Each malfunction identifies the specific mechanism which supports downstream FMEA. Notably, MB-016 (brake force insufficient / partial application) is identified but not carried forward into the hazardous events table -- this should either generate a hazardous event or be documented as not generating one with rationale. Similarly, MB-013 (steering angle sensor failure) and MB-019 (lidar stuck at last value) are identified but not directly mapped to hazardous events. The traceability in Section 10.1 should confirm that every MB maps to at least one HE or is explicitly dispositioned.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE2 -->

## 6. Hazardous Events

### 6.1 Hazardous Event Table

Each hazardous event is the combination of a malfunctioning behavior occurring during an operational situation. The S/E/C ratings and ASIL determination follow.

| HE-ID | Operational Situation | Malfunction | Hazardous Event | S | E | C | ASIL |
|-------|----------------------|-------------|-----------------|---|---|---|------|
| HE-001 | OS-1: Driving forward at moderate speed | MB-001: Both pedal sensors read high | Unintended acceleration — vehicle accelerates without driver command, into traffic or obstacles | S3 | E4 | C3 | **D** |
| HE-002 | OS-1: Driving forward at moderate speed | MB-005: Motor does not respond to torque request | Loss of drive torque — vehicle decelerates unexpectedly in traffic, risk of rear-end collision | S2 | E4 | C2 | **B** |
| HE-003 | OS-1: Driving forward at moderate speed | MB-011: Unintended steering movement | Unintended steering — vehicle deviates from lane into oncoming traffic or off-road | S3 | E3 | C3 | **C** |
| HE-004 | OS-3: Turning at low speed | MB-012: Loss of steering response | Loss of steering during turn — vehicle fails to follow intended path, strikes curb, pedestrians, or other vehicles | S3 | E4 | C3 | **D** |
| HE-005 | OS-4: Braking | MB-014: Loss of braking | Loss of braking during deceleration — vehicle cannot stop, collides with obstacle or vehicle ahead | S3 | E4 | C3 | **D** |
| HE-006 | OS-1: Driving forward at moderate speed | MB-015: Unintended braking | Unintended braking — vehicle decelerates sharply without driver command, risk of rear-end collision by following vehicle | S2 | E3 | C2 | **A** |
| HE-007 | OS-1: Driving forward at moderate speed | MB-007: Motor overcurrent not detected | Motor overcurrent undetected — motor overheats, potential fire or smoke, driver exposed to thermal hazard | S2 | E3 | C2 | **A** |
| HE-008 | OS-1: Driving forward at moderate speed | MB-008: Motor overtemperature not detected | Motor overtemperature undetected — gradual thermal degradation, potential smoke/fire over extended operation | S2 | E2 | C2 | **QM** |
| HE-009 | OS-1: Driving forward at moderate speed | MB-017: Lidar false negative | Obstacle not detected — vehicle does not brake for obstacle, collision at driving speed | S3 | E3 | C3 | **C** |
| HE-010 | OS-1: Driving forward at moderate speed | MB-018: Lidar false positive | Phantom braking — vehicle brakes unexpectedly for non-existent obstacle, rear-end collision risk from following vehicle | S2 | E3 | C2 | **A** |
| HE-011 | OS-1: Driving forward at moderate speed | MB-023: Total CAN bus failure | Loss of all inter-ECU communication — motor and steering uncontrolled, no coordination, vehicle behavior unpredictable | S3 | E3 | C3 | **C** |
| HE-012 | OS-1: Driving forward at moderate speed | MB-022: Safety Controller hangs | Loss of independent safety monitoring — faults in zone controllers go undetected, no kill relay backup | S3 | E2 | C3 | **B** |
| HE-013 | OS-1: Driving forward at moderate speed | MB-026: E-stop not functional | E-stop pressed during emergency but system does not stop — driver unable to force safe state during critical event | S3 | E2 | C3 | **B** |
| HE-014 | OS-1: Driving forward at moderate speed | MB-009: Motor direction reversal | Unintended motor reversal during forward driving — sudden reverse torque causing loss of control, mechanical stress, occupant injury | S3 | E3 | C3 | **C** |
| HE-015 | OS-1: Driving forward at moderate speed | MB-027: Battery overvoltage or undervoltage | Battery voltage out of range — electronics may malfunction or reset, motor performance affected, gradual degradation | S1 | E3 | C1 | **QM** |

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE3 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The primary hazardous events table (HE-001 to HE-015) covers the most critical combinations of malfunctions and operational situations. ASIL verification against the ISO 26262-3 Table 4 matrix: HE-001 (S3/E4/C3 = D) is correct. HE-002 (S2/E4/C2 = B) is correct. HE-003 (S3/E3/C3 = C) is correct. HE-004 (S3/E4/C3 = D) is correct. HE-005 (S3/E4/C3 = D) is correct. HE-006 (S2/E3/C2 = A) is correct. HE-007 (S2/E3/C2 = A) is correct. HE-008 (S2/E2/C2 = QM) is correct. HE-009 (S3/E3/C3 = C) is correct. HE-010 (S2/E3/C2 = A) is correct. HE-011 (S3/E3/C3 = C) is correct. HE-012 (S3/E2/C3 = B) is correct. HE-013 (S3/E2/C3 = B) is correct. HE-014 (S3/E3/C3 = C) is correct. HE-015 (S1/E3/C1 = QM) is correct. All ASIL assignments are consistent with the determination matrix.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE3 -->

### 6.2 Additional Hazardous Events

The following events extend the analysis to cover additional operational situations and malfunctions that could lead to harm.

| HE-ID | Operational Situation | Malfunction | Hazardous Event | S | E | C | ASIL |
|-------|----------------------|-------------|-----------------|---|---|---|------|
| HE-016 | OS-7: Driving forward at high speed | MB-001: Both pedal sensors read high | Unintended acceleration at high speed — vehicle accelerates beyond safe highway speed, loss of control | S3 | E3 | C3 | **C** |
| HE-017 | OS-2: Stationary | MB-006: Motor runs at full power uncontrolled | Unintended vehicle motion from rest — vehicle lurches forward into pedestrians or obstacles while parked/stopped | S3 | E4 | C3 | **D** |
| HE-018 | OS-5: Reversing | MB-009: Motor direction reversal | Unintended forward motion during reversing — vehicle moves forward when reverse is expected, strikes objects in front | S2 | E3 | C2 | **A** |
| HE-019 | OS-1: Driving forward at moderate speed | MB-010: Motor cannot be stopped | Motor enable stuck — motor continues to run despite torque request of zero, vehicle does not decelerate | S3 | E3 | C3 | **C** |
| HE-020 | OS-1: Driving forward at moderate speed | MB-025: CAN bus babbling node | CAN bus flooding — safety-critical messages blocked by babbling node, loss of coordination between ECUs | S3 | E2 | C3 | **B** |

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE4 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The additional hazardous events (HE-016 to HE-020) extend the analysis to cover additional operational situations and malfunctions. ASIL verification: HE-016 (S3/E3/C3 = C) is correct. HE-017 (S3/E4/C3 = D) is correct -- this is a critical finding as unintended vehicle motion from rest with pedestrians present warrants the highest ASIL. HE-018 (S2/E3/C2 = A) is correct. HE-019 (S3/E3/C3 = C) is correct. HE-020 (S3/E2/C3 = B) is correct. Note that HE-016 through HE-020 are NOT traced in the Safety Goals document (SG) Section 4, which only lists HE-001 through HE-015. This is a traceability gap -- the SG document must account for all 20 hazardous events, not just the first 15. The HARA Section 9.3 safety goals preview does reference HE-016 and HE-017 but not HE-018, HE-019, or HE-020 explicitly.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE4 -->

## 7. S/E/C Rating Justifications

### 7.1 HE-001: Unintended Acceleration During Driving

**Severity S3**: Unintended acceleration into traffic or obstacles at moderate speed (30-80 km/h) can result in high-energy collisions with other vehicles, pedestrians, or fixed objects. The kinetic energy involved is sufficient to cause life-threatening or fatal injuries to occupants and third parties. Real-world unintended acceleration events (e.g., Toyota 2009-2010) have caused fatalities.

**Exposure E4**: Driving forward at moderate speed is the most common operational state of a vehicle. Virtually every trip involves sustained forward driving. Per ISO 26262-3 Table B.2, this represents high probability (> 10% of operating time). The dual pedal sensors are continuously active during all forward driving.

**Controllability C3**: If both pedal sensors simultaneously report a high value, the vehicle accelerates without any driver input on the pedal. The driver has no immediate intuitive response to counteract this — releasing the pedal has no effect because the sensors are reporting a false high regardless of pedal position. The driver must recognize the situation, identify that the accelerator is not the cause, and apply brakes or use the E-stop, all while the vehicle is accelerating. This requires an atypical response and significant reaction time, making it difficult to control for most drivers.

### 7.2 HE-002: Loss of Drive Torque During Driving

**Severity S2**: Sudden loss of motive power during moderate-speed driving causes the vehicle to decelerate unexpectedly. While the vehicle does not actively endanger its occupants (it is slowing down), following vehicles may collide with the decelerating vehicle. This can result in severe injuries but survival is probable, especially in moderate-speed urban scenarios.

**Exposure E4**: Forward driving is the dominant operating condition. The motor torque function is active continuously during all forward driving.

**Controllability C2**: The driver experiences a sudden loss of acceleration. The vehicle slows down but does not lose steering or braking. The driver can steer to the roadside, apply hazard lights, and brake to a controlled stop. This is an unusual but manageable situation for a normally attentive driver.

### 7.3 HE-003: Unintended Steering During Driving

**Severity S3**: Unintended steering deviation at moderate speed (30-80 km/h) can cause the vehicle to cross into oncoming traffic lanes or leave the roadway. Head-on collisions or rollover events at these speeds are life-threatening or fatal.

**Exposure E3**: Unintended steering requires a specific malfunction condition (CAN corruption, software fault) to coincide with active driving. The steering function is always active during driving, but the specific malfunction that causes unintended movement (rather than loss of steering) is a medium-probability event. The CAN message carrying the steering command is transmitted every 10 ms, meaning a corruption event has many opportunities to occur, but the E2E protection reduces the probability somewhat.

**Controllability C3**: A sudden unexpected steering deviation at moderate speed gives the driver very little time to react. At 50 km/h, the vehicle is traveling at approximately 14 m/s. A steering deviation that moves the vehicle one lane width (3.5 m) in less than 0.25 seconds gives insufficient time for most drivers to correct. The corrective action (counter-steer) is instinctive but may be insufficient if the servo is actively fighting the driver's input.

### 7.4 HE-004: Loss of Steering During Turning

**Severity S3**: Loss of steering response during an active turn means the vehicle will travel straight (or in a widening arc) instead of following the intended path. At intersections, this can result in collision with crossing traffic, pedestrians in crosswalks, or roadside infrastructure. The geometry of intersections means even low-speed failures can direct the vehicle into high-severity collision scenarios (e.g., T-bone collision with crossing traffic).

**Exposure E4**: Turning is a frequent driving maneuver. Urban driving involves numerous turns at intersections, roundabouts, and parking lots. Per ISO 26262-3 Table B.2, the frequency of turning maneuvers during normal driving exceeds the threshold for high exposure.

**Controllability C3**: Once steering response is lost during a turn, the driver cannot redirect the vehicle. Braking is the only available response, but the vehicle's trajectory is already committed to a straight line through the turn. At intersection speeds (15-30 km/h), the time between loss of steering and collision with crossing traffic or pedestrians may be 1-2 seconds, which is insufficient for most drivers to stop completely. The driver must recognize the loss of steering, switch from steering correction to braking, and stop before the collision — a complex multi-step response under time pressure.

### 7.5 HE-005: Loss of Braking During Braking

**Severity S3**: Loss of braking when the driver is actively decelerating means the vehicle will not stop as expected. This leads to collision with the obstacle, vehicle, or intersection that the driver was braking for. At moderate speeds, this is a life-threatening scenario (e.g., failure to stop at a red light, failure to stop for a pedestrian crossing).

**Exposure E4**: Braking is a continuous and frequent activity during all driving. Urban driving involves braking for every intersection, traffic light, and traffic situation. The braking function is invoked dozens of times per trip.

**Controllability C3**: If the brake servo does not apply force when commanded, the driver has no alternative braking mechanism in this drive-by-wire system (no mechanical fallback brake in the platform design). The driver can attempt engine braking by releasing the accelerator, or use the E-stop, but these responses are atypical and require the driver to recognize the brake failure, shift strategy, and act — all while the vehicle is approaching the obstacle. For most drivers, the instinctive response is to press the brake pedal harder, which has no effect.

### 7.6 HE-006: Unintended Braking During Driving

**Severity S2**: Unexpected braking during forward driving causes sudden deceleration. Occupants experience a jolt but are restrained by seatbelts. The primary risk is from following vehicles that may rear-end the suddenly decelerating vehicle. At moderate speeds, rear-end collisions typically result in severe injuries but survival is probable (whiplash, airbag deployment).

**Exposure E3**: The malfunction requires a CAN message corruption or software error specifically affecting the brake command during driving. While the brake function receives commands continuously, the specific condition causing unintended activation is a medium-probability event.

**Controllability C2**: The driver feels unexpected deceleration and can release the brake (though the brake is applied by software, not the driver's foot). The driver can signal intent via hazard lights and steer to the roadside. Following drivers have some reaction time (brake lights illuminate). The situation is unusual but manageable for a normally attentive driver.

### 7.7 HE-007: Motor Overcurrent Not Detected During Driving

**Severity S2**: Undetected overcurrent causes progressive motor heating. In the short term (seconds to minutes), this does not directly affect vehicle motion. Over sustained periods, it can lead to motor winding insulation failure, smoke, or fire within the motor housing. Thermal events in the vehicle powertrain are severe (survival probable) but rarely immediately fatal.

**Exposure E3**: Overcurrent conditions require a specific load condition (mechanical blockage, short circuit, overload) combined with sensor failure. The current sensor is continuously monitoring, so the exposure is tied to the probability of the sensor failing coincidentally with a high-current condition.

**Controllability C2**: The driver may notice unusual smells (hot insulation), reduced motor performance, or warning signs of overheating. The driver can stop the vehicle voluntarily. The progression from overcurrent to thermal hazard is gradual (tens of seconds to minutes), giving the driver time to react.

### 7.8 HE-008: Motor Overtemperature Not Detected

**Severity S2**: Similar to overcurrent, undetected overtemperature can lead to motor degradation, smoke, or fire. The thermal mass of the motor means temperature rises gradually over minutes, not seconds.

**Exposure E2**: Overtemperature requires sustained high-load operation (e.g., hill climbing, towing) combined with temperature sensor failure. The scenario is a coincidence of two conditions, making it low probability during normal driving.

**Controllability C2**: The driver has minutes of warning before a thermal event becomes dangerous. Reduced motor performance and unusual sounds/smells provide additional cues. The driver can stop the vehicle voluntarily.

### 7.9 HE-009: Lidar False Negative (Obstacle Not Detected) During Driving

**Severity S3**: If the lidar fails to detect an obstacle in the vehicle's path, the autonomous emergency braking function does not activate. The vehicle collides with the obstacle at driving speed. At moderate speeds (30-80 km/h), collision with a pedestrian or vehicle is life-threatening or fatal.

**Exposure E3**: The lidar is one layer of obstacle detection. A false negative requires a specific combination of conditions: obstacle present in the driving path, lidar failing to detect it (lens contamination, reflectivity issue, UART error), AND the driver not independently braking. This combined probability is medium.

**Controllability C3**: The driver is assumed to be relying on the lidar-based emergency braking as a safety net. If the lidar fails silently, the driver does not receive an expected warning and may not react in time. The driver's own observation of the obstacle depends on attention, visibility, and reaction time. For scenarios where the driver is inattentive or visibility is poor, the obstacle may be uncontrollable.

### 7.10 HE-010: Lidar False Positive (Phantom Braking) During Driving

**Severity S2**: Phantom braking causes unexpected deceleration similar to HE-006. The primary risk is from following vehicles. The braking is limited to the emergency brake force level, which is a strong but survivable deceleration.

**Exposure E3**: False positive events depend on environmental conditions (reflective surfaces, rain, dust). They are not constant but occur in specific scenarios.

**Controllability C2**: The driver can override the phantom brake by pressing the accelerator (if the system allows driver override) or by waiting for the false detection to clear. Following drivers have some reaction time from brake light activation. The situation is controllable with normal driver attention.

### 7.11 HE-011: CAN Bus Total Failure During Driving

**Severity S3**: Total CAN bus failure means all inter-ECU communication ceases simultaneously. The CVC can no longer send torque, steering, or brake commands. The RZC, FZC, and SC receive no updates. Depending on the failure mode of each ECU upon communication loss, the vehicle may continue at the last commanded speed/angle (stuck-at-last-value) or enter an uncoordinated state. The unpredictability of vehicle behavior during total communication loss is life-threatening.

**Exposure E3**: Total CAN bus failure requires a catastrophic event: bus wire break, both termination resistors failing, or a dominant transceiver fault. These are uncommon but not incredible events, especially considering vibration, connector quality, and environmental exposure over vehicle lifetime.

**Controllability C3**: When all communication fails simultaneously, the vehicle behavior becomes unpredictable. The driver may lose steering, braking, and throttle control simultaneously. The driver has no single corrective action that can restore control. The E-stop may work (hardwired to CVC GPIO) but requires recognition of the total failure and physical activation of the E-stop, which is a non-standard response.

### 7.12 HE-012: Safety Controller Failure During Driving

**Severity S3**: If the Safety Controller (SC) hangs or fails, the independent safety monitoring layer is lost. Subsequent faults in zone controllers (CVC, FZC, RZC) will not be detected by the SC, and the kill relay will not be opened. This is a latent fault — no immediate hazard, but the safety net is removed, meaning the next fault in the system goes unmitigated. The severity is rated S3 because the consequence of the unmitigated subsequent fault could be life-threatening.

**Exposure E2**: The TMS570LC43x has lockstep cores and external watchdog (TPS3823). Both must fail simultaneously for the SC to hang undetected. The probability of this dual failure is low. The lockstep CPU detects approximately 99% of CPU faults, and the external watchdog detects software hangs within its timeout period.

**Controllability C3**: If the SC fails silently, the driver has no indication that the safety monitoring layer is lost (unless the fault is reported via CAN, which requires the SC to be functional to report). The driver cannot detect or control this latent fault. The C3 rating reflects the inability of the driver to compensate for loss of an invisible safety function.

### 7.13 HE-013: E-Stop Not Functional During Emergency

**Severity S3**: If the E-stop button is pressed during an emergency but the system does not stop, the driver's last-resort safety mechanism has failed. The underlying emergency condition (which prompted the E-stop use) continues unmitigated. The severity is rated based on the underlying emergency, which is assumed to be life-threatening (since the driver chose to activate the E-stop).

**Exposure E2**: The E-stop is activated only during emergencies, which are rare events. The E-stop malfunction must coincide with an emergency requiring its use. This combined probability is low.

**Controllability C3**: The E-stop is the driver's last-resort action. If it fails, the driver has exhausted their known options for stopping the vehicle. Remaining options (turning off the power supply, disconnecting the battery) are not intuitive or immediately accessible. The driver is already in a high-stress emergency situation with degraded decision-making capacity.

### 7.14 HE-014: Unintended Motor Reversal During Driving

**Severity S3**: If the motor direction suddenly reverses while driving forward at moderate speed, the vehicle experiences a sudden negative torque. This can cause loss of traction, vehicle instability, drivetrain mechanical stress, and potentially throw occupants against restraints. At higher speeds, the deceleration from reverse torque can exceed the friction limit of the tires, causing loss of directional stability.

**Exposure E3**: Motor direction reversal requires a specific fault: H-bridge direction control logic error, GPIO pin fault, or CAN message corruption that inverts the direction bit. These are uncommon faults that require specific conditions.

**Controllability C3**: The driver experiences a sudden, unexpected deceleration or reversal of the vehicle's motion. This is a highly unusual event with no intuitive corrective action. The driver may instinctively brake, which compounds the deceleration. The driver cannot counteract the reverse torque through the pedal (the pedal position is irrelevant if the direction control is faulty).

### 7.15 HE-015: Battery Overvoltage/Undervoltage During Driving

**Severity S1**: Battery voltage excursions outside the nominal range (9-16V) cause gradual performance degradation rather than sudden loss of function. Undervoltage may cause MCU brownout resets (which trigger watchdog-based safe state). Overvoltage may stress components but does not immediately cause injury. The effects are light to moderate — reduced motor power, sensor inaccuracy, or a controlled shutdown.

**Exposure E3**: Voltage excursions on a bench power supply are uncommon but not impossible. In a real vehicle, load dump and cranking transients are medium-frequency events.

**Controllability C1**: The driver notices reduced vehicle performance and can stop the vehicle voluntarily. The voltage monitoring system on the RZC provides warnings before the voltage reaches dangerous levels. The situation is simply controllable with minimal driver action.

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE5 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The S/E/C justifications for HE-001 through HE-015 are detailed and well-reasoned, each providing multi-paragraph rationale for the assigned ratings. The justifications reference real-world analogies (e.g., Toyota 2009-2010 unintended acceleration for HE-001), physical calculations (kinetic energy, stopping distance), and ISO 26262 Table B.1/B.2/B.3 criteria. HE-005 (loss of braking) correctly identifies that this drive-by-wire platform has no mechanical fallback brake, which is a critical architectural constraint that elevates C to C3. HE-012 (SC failure) is correctly identified as a latent fault with S3 rating based on the consequence of the unmitigated subsequent fault. The thermal time constant arguments for HE-007 and HE-008 are physically sound. One observation: HE-009 (lidar false negative) assigns C3 based on the driver relying on the lidar as a safety net, but the lidar function is listed as ASIL C (not D), suggesting the driver is the primary obstacle detection path. This tension in the controllability argument should be documented.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE5 -->

### 7.16 HE-016: Unintended Acceleration at High Speed

**Severity S3**: Unintended acceleration at highway speeds (80-130 km/h) is life-threatening. The vehicle may exceed safe speed for road conditions, other traffic, or road geometry. Collision energy increases with the square of velocity.

**Exposure E3**: Highway driving is common but less frequent than urban driving. The same dual sensor failure mode applies but the operational situation is less frequent than OS-1.

**Controllability C3**: Same controllability challenges as HE-001, compounded by higher speed. The driver has even less time to react and the consequences of delayed reaction are more severe.

### 7.17 HE-017: Unintended Vehicle Motion from Rest

**Severity S3**: A stationary vehicle that suddenly lurches forward at full motor power can strike pedestrians who are near the vehicle (loading cargo, crossing in front, children playing). The initial acceleration from rest is rapid and the victims have no warning. This is life-threatening, especially for vulnerable road users.

**Exposure E4**: The vehicle is stationary frequently (parking, traffic stops, loading/unloading). The motor control malfunction (BTS7960 FET short, PWM stuck at 100%) can occur at any time, including when stationary.

**Controllability C3**: Pedestrians near a stationary vehicle have no expectation of sudden vehicle motion and cannot react in time. The driver may not have hands on the steering wheel or feet near the brake pedal when stationary. The sudden motion is surprising and the driver's reaction to press the brake or E-stop requires recognition time.

### 7.18 HE-018: Unintended Forward Motion During Reversing

**Severity S2**: During reversing at low speed (0-10 km/h), if the motor direction reverses to forward, the vehicle moves forward unexpectedly. This can strike objects in front of the vehicle. At low speed (0-10 km/h), the collision energy is moderate, resulting in severe injuries but survival is probable.

**Exposure E3**: Reversing is a less common operating situation than forward driving. The direction reversal fault must coincide with the reversing maneuver.

**Controllability C2**: At low reversing speed, the driver can brake to stop the vehicle relatively quickly. The unexpected forward motion is surprising but the low speed gives the driver more time to react. The corrective action (brake) is intuitive.

### 7.19 HE-019: Motor Enable Stuck During Driving

**Severity S3**: If the motor enable signal is stuck active, the motor cannot be electrically disabled by the RZC software. Even with zero torque command, the BTS7960 may continue to drive the motor if the enable pins are stuck. The vehicle cannot decelerate through motor control. This is effectively a loss of motor controllability, similar to unintended acceleration.

**Exposure E3**: A stuck GPIO pin (BTS7960 R_EN or L_EN) is an uncommon hardware fault. It requires a specific failure mode (pin short to VCC, solder bridge, or FET gate short).

**Controllability C3**: The driver cannot control the motor through the pedal. Braking is available but the motor continues to fight the brakes. The driver must use the E-stop or rely on the Safety Controller kill relay to cut power. This is a non-standard response that most drivers would not immediately identify.

### 7.20 HE-020: CAN Bus Babbling Node During Driving

**Severity S3**: A babbling node floods the CAN bus with continuous high-priority frames, effectively blocking all other ECU communication. Safety-critical messages (heartbeats, torque commands, brake commands) cannot get through. The effect is similar to total CAN bus failure (HE-011) but caused by a software/hardware fault in a single ECU rather than a bus-level fault.

**Exposure E2**: A babbling node requires a specific software or hardware fault in one ECU (e.g., interrupt storm, CAN controller fault, infinite loop in CAN TX). This is a low-probability event, especially with proper software watchdog and CAN error handling.

**Controllability C3**: The driver experiences the same loss of vehicle coordination as in HE-011. The babbling may not be immediately obvious to the driver (no physical symptom until vehicle control is lost). The driver cannot restore CAN communication. The E-stop (hardwired) and the Safety Controller (independent CAN monitor that can detect abnormal bus activity) are the only mitigation paths.

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE6 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The S/E/C justifications for HE-016 through HE-020 maintain the same quality and depth as the primary events. HE-017 (unintended vehicle motion from rest) correctly identifies the pedestrian risk scenario and the driver's likely unpreparedness when stationary, justifying S3/E4/C3 = ASIL D. HE-019 (motor enable stuck) provides a clear explanation of why the BTS7960 enable pin fault creates a scenario where the motor "fights the brakes," which is distinct from simple unintended acceleration. HE-020 (babbling node) correctly notes that the SC can detect abnormal CAN bus activity, which links to SM-019 in the FSC.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE6 -->

## 8. ASIL Determination Matrix

The following matrix is the ISO 26262-3 Table 4 ASIL determination table, used to assign ASIL based on the combination of Severity (S), Exposure (E), and Controllability (C).

### 8.1 Classification Scales

**Severity (S)** — ISO 26262-3 Table B.1:

| Class | Description | Injury Examples |
|-------|-------------|-----------------|
| S0 | No injuries | No harm to persons |
| S1 | Light and moderate injuries | Bruises, minor cuts, whiplash (AIS 1-2) |
| S2 | Severe and life-threatening injuries (survival probable) | Fractures, concussion, internal injuries (AIS 3-4) |
| S3 | Life-threatening injuries (survival uncertain), fatal injuries | Critical organ damage, spinal cord injury, fatality (AIS 5-6) |

**Exposure (E)** — ISO 26262-3 Table B.2:

| Class | Description | Probability / Duration |
|-------|-------------|----------------------|
| E0 | Incredible | Considered impossible |
| E1 | Very low probability | Extremely rare situation (< 1% of operating time) |
| E2 | Low probability | Rare but realistic situation (1-5% of operating time) |
| E3 | Medium probability | Situation occurs regularly (5-30% of operating time) |
| E4 | High probability | Situation occurs frequently (> 30% of operating time) |

**Controllability (C)** — ISO 26262-3 Table B.3:

| Class | Description | Driver Action |
|-------|-------------|---------------|
| C0 | Controllable in general | Trivially controllable by any driver |
| C1 | Simply controllable | > 99% of drivers can handle the situation |
| C2 | Normally controllable | > 90% of drivers can handle the situation |
| C3 | Difficult to control or uncontrollable | < 90% of drivers can handle the situation |

### 8.2 ASIL Determination Table (ISO 26262-3, Table 4)

|  | | **C1** | **C2** | **C3** |
|--|--|--------|--------|--------|
| **S1** | **E1** | QM | QM | QM |
| **S1** | **E2** | QM | QM | QM |
| **S1** | **E3** | QM | QM | A |
| **S1** | **E4** | QM | A | B |
| **S2** | **E1** | QM | QM | QM |
| **S2** | **E2** | QM | QM | A |
| **S2** | **E3** | QM | A | B |
| **S2** | **E4** | A | B | C |
| **S3** | **E1** | QM | QM | A |
| **S3** | **E2** | QM | A | B |
| **S3** | **E3** | A | B | C |
| **S3** | **E4** | B | C | **D** |

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE7 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The ASIL determination matrix in Section 8 correctly reproduces ISO 26262-3 Table 4. The classification scales for S, E, and C are accurately described with injury examples (AIS scale for severity), probability ranges for exposure, and driver capability percentages for controllability. This section serves as a self-contained reference for verifying all ASIL assignments in the hazardous event tables. Cross-checked: all 20 hazardous events in Sections 6.1 and 6.2 produce correct ASIL results when applied to this matrix.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE7 -->

## 9. HARA Summary

### 9.1 Hazardous Events per ASIL Level

| ASIL | Count | HE-IDs |
|------|-------|--------|
| **D** | 4 | HE-001, HE-004, HE-005, HE-017 |
| **C** | 6 | HE-003, HE-009, HE-011, HE-014, HE-016, HE-019 |
| **B** | 4 | HE-002, HE-012, HE-013, HE-020 |
| **A** | 4 | HE-006, HE-007, HE-010, HE-018 |
| **QM** | 2 | HE-008, HE-015 |
| **Total** | **20** | |

### 9.2 Highest ASIL per Function

| Function | Highest ASIL | Governing HE |
|----------|-------------|--------------|
| F-DBW (Drive-by-Wire) | **D** | HE-001 (unintended acceleration) |
| F-STR (Steering Control) | **D** | HE-004 (loss of steering during turning) |
| F-BRK (Braking Control) | **D** | HE-005 (loss of braking) |
| F-DIST (Distance Sensing) | **C** | HE-009 (obstacle not detected) |
| F-SAF (Safety Monitoring) | **B** | HE-012 (SC failure), HE-013 (E-stop failure) |
| F-BODY (Body Control) | **QM** | No safety-critical hazardous events identified |
| F-DIAG (Diagnostics) | **QM** | No safety-critical hazardous events identified |

### 9.3 Safety Goals (Preview)

The following safety goals are derived from the HARA results. Full safety goal specification, safe state definitions, and FTTI values are documented in the Safety Goals document (SG).

| SG-ID | Safety Goal | ASIL | Source HE |
|-------|-------------|------|-----------|
| SG-001 | The system shall prevent unintended acceleration due to pedal sensor malfunction | D | HE-001, HE-016 |
| SG-002 | The system shall prevent loss of steering control during active maneuvering | D | HE-004, HE-003 |
| SG-003 | The system shall prevent loss of braking capability during active deceleration | D | HE-005 |
| SG-004 | The system shall prevent unintended vehicle motion from a stationary state | D | HE-017 |
| SG-005 | The system shall detect and mitigate loss of drive torque | B | HE-002 |
| SG-006 | The system shall prevent unintended braking without driver command | A | HE-006, HE-010 |
| SG-007 | The system shall detect motor overcurrent and initiate protective action | A | HE-007 |
| SG-008 | The system shall detect obstacles in the forward path and initiate braking | C | HE-009 |
| SG-009 | The system shall maintain inter-ECU communication integrity or achieve safe state | C | HE-011, HE-020 |
| SG-010 | The system shall ensure the Safety Controller remains operational or is detected as failed | B | HE-012 |
| SG-011 | The system shall ensure the E-stop function is available at all times | B | HE-013 |
| SG-012 | The system shall prevent unintended motor direction reversal during driving | C | HE-014 |
| SG-013 | The system shall ensure motor enable can always be deactivated by the control system | C | HE-019 |

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE8 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The HARA summary correctly counts 4 ASIL D, 6 ASIL C, 4 ASIL B, 4 ASIL A, and 2 QM hazardous events across the 20 total. The highest ASIL per function table is accurate and consistent with the hazardous event assignments. However, there is a significant discrepancy between the safety goals preview in Section 9.3 (13 safety goals, SG-001 through SG-013) and the Safety Goals document (SG) which defines only 8 safety goals (SG-001 through SG-008) by grouping related hazardous events. This discrepancy should be resolved -- either the HARA preview should be updated to reflect the final 8 SG grouping, or the SG document should be updated to include all 13. The current state creates confusion about the intended number of safety goals. Additionally, SG-004 in the preview ("prevent unintended vehicle motion from stationary state") maps to HE-017, but in the SG document SG-004 is "prevent unintended loss of braking" (from HE-005), and HE-017 is not explicitly mapped.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE8 -->

## 10. Traceability

### 10.1 Function-to-Hazardous-Event Traceability

| Function | Malfunctions | Hazardous Events |
|----------|-------------|-----------------|
| F-DBW | MB-001 to MB-010 | HE-001, HE-002, HE-007, HE-008, HE-014, HE-015, HE-016, HE-017, HE-018, HE-019 |
| F-STR | MB-011 to MB-013 | HE-003, HE-004 |
| F-BRK | MB-014 to MB-016 | HE-005, HE-006 |
| F-DIST | MB-017 to MB-019 | HE-009, HE-010 |
| F-SAF | MB-020 to MB-022, MB-026 | HE-012, HE-013 |
| CAN Bus | MB-023 to MB-025 | HE-011, HE-020 |

### 10.2 Downstream Traceability

| HARA Output | Downstream Document | Document ID |
|-------------|---------------------|-------------|
| Safety Goals | Safety Goals Specification | SG |
| Safety Goals | Functional Safety Concept | FSC |
| ASIL assignments | Functional Safety Requirements | FSR |
| ASIL assignments | Technical Safety Requirements | TSR |
| ASIL assignments | SW Safety Requirements | SSR |
| ASIL assignments | HW Safety Requirements | HSR |
| Hazardous events | FMEA (failure mode analysis) | FMEA |
| Hazardous events | DFA (dependent failure analysis) | DFA |

<!-- HITL-LOCK START:COMMENT-BLOCK-HARA-HE9 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The traceability section provides bidirectional mapping from functions to malfunctions to hazardous events (upstream) and from HARA outputs to downstream documents (SG, FSC, FSR, TSR, SSR, HSR, FMEA, DFA). The function-to-HE traceability table (Section 10.1) is complete for the functions listed. However, F-BODY and F-DIAG are not listed in the traceability table -- even though they are QM-rated with no safety-critical hazardous events, they should appear in the table with "None" or "QM" notation for completeness. The downstream traceability table (Section 10.2) correctly identifies all expected ISO 26262 work products that consume the HARA output.
<!-- HITL-LOCK END:COMMENT-BLOCK-HARA-HE9 -->

## 11. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete HARA: 7 operational situations, 27 malfunctions, 20 hazardous events, S/E/C justifications, ASIL determination, safety goal preview, traceability |

## 12. Approval

| Role | Name | Date | Signature |
|------|------|------|-----------|
| FSE Lead | _________________ | __________ | __________ |
| System Engineer | _________________ | __________ | __________ |
| HW Engineer | _________________ | __________ | __________ |
| SW Engineer | _________________ | __________ | __________ |
| Domain Expert (Vehicle Dynamics) | _________________ | __________ | __________ |
| Safety Manager | _________________ | __________ | __________ |

