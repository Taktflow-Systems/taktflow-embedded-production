---
document_id: SWR-CVC
title: "Software Requirements — CVC"
version: "1.0"
status: draft
aspice_process: SWE.1
ecu: CVC
asil: D
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

Every requirement (SWR-CVC-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/software/lessons-learned/`](../lessons-learned/). One file per requirement (SWR-CVC-NNN). File naming: `SWR-CVC-NNN-<short-title>.md`.


# Software Requirements — Central Vehicle Computer (CVC)

## 1. Purpose

This document specifies the complete software requirements for the Central Vehicle Computer (CVC), the STM32G474RE-based vehicle brain of the Taktflow Zonal Vehicle Platform. These requirements are derived from system requirements (SYS), technical safety requirements (TSR), and software safety requirements (SSR) per Automotive SPICE 4.0 SWE.1 (Software Requirements Analysis).

Software requirements cover ALL CVC software functionality: safety-critical pedal processing and state management (from SSRs), plus functional, diagnostic, configuration, startup/shutdown, and display requirements that go beyond the safety-only SSRs.

## 2. Referenced Documents

| Document ID | Title | Version |
|-------------|-------|---------|
| SYS | System Requirements Specification | 1.0 |
| TSR | Technical Safety Requirements | 1.0 |
| SSR | Software Safety Requirements | 1.0 |
| HSI | Hardware-Software Interface Specification | 0.1 |
| SYSARCH | System Architecture | 1.0 |
| CAN-MATRIX | CAN Message Matrix | 0.1 |

## 3. Requirement Conventions

### 3.1 Requirement Structure

Each requirement follows the format:

- **ID**: SWR-CVC-NNN (sequential)
- **Title**: Descriptive name
- **ASIL**: D / C / B / A / QM (inherited from parent or assessed)
- **Traces up**: SYS-xxx, TSR-xxx, SSR-CVC-xxx
- **Traces down**: firmware/cvc/src/{file}:{function}
- **Verified by**: TC-CVC-xxx (test case IDs)
- **Verification method**: Unit test, integration test, SIL, PIL, inspection, analysis
- **Status**: draft | reviewed | approved | implemented | verified

### 3.2 Requirement Language

- "shall" = mandatory requirement
- "should" = recommended behavior
- "may" = optional behavior

---

## 4. Pedal Processing Requirements

### SWR-CVC-001: SPI1 Initialization for Dual Pedal Sensors

- **ASIL**: D
- **Traces up**: SYS-001, SYS-047, TSR-001, SSR-CVC-001
- **Traces down**: firmware/cvc/src/mcal/spi_cfg.c:Spi_Init()
- **Verified by**: TC-CVC-001
- **Verification method**: Unit test + PIL
- **Status**: draft

The CVC software shall initialize SPI1 in master mode at 1 MHz clock, CPOL=0, CPHA=1 (matching AS5048A requirements). The software shall configure PA4 as chip-select for pedal sensor 1 and PA15 as chip-select for pedal sensor 2, both as GPIO outputs initialized to HIGH (deselected). The initialization shall verify SPI peripheral clock enable and confirm the SPI status register indicates idle state before returning success. If SPI1 initialization fails (peripheral clock not enabled or status register not idle within 10 ms), the software shall report a startup fault and prevent transition from INIT to RUN.

---

### SWR-CVC-002: Pedal Sensor SPI Read Cycle

- **ASIL**: D
- **Traces up**: SYS-001, TSR-001, SSR-CVC-002
- **Traces down**: firmware/cvc/src/swc/swc_pedal.c:Swc_Pedal_ReadSensors()
- **Verified by**: TC-CVC-002, TC-CVC-003
- **Verification method**: Unit test + SIL + PIL
- **Status**: draft

The CVC software shall read both AS5048A pedal sensors sequentially within a single 10 ms control cycle. For each sensor: (a) assert the chip-select LOW, (b) transmit the 16-bit read command for the angle register (0x3FFF), (c) receive the 16-bit response, (d) de-assert chip-select HIGH, (e) validate the response parity bit and extract the 14-bit angle value. If the SPI transaction does not complete within 500 us (measured by a hardware timer), the software shall abort the transaction, de-assert chip-select, and retry once. If the retry also fails, the software shall set the sensor status to COMM_FAULT and use the last valid reading.

---

### SWR-CVC-003: Dual Pedal Sensor Plausibility Comparison

- **ASIL**: D
- **Traces up**: SYS-002, TSR-002, SSR-CVC-003
- **Traces down**: firmware/cvc/src/swc/swc_pedal.c:Swc_Pedal_CheckPlausibility()
- **Verified by**: TC-CVC-004, TC-CVC-005, TC-CVC-006
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The CVC software shall compare the 14-bit angle values from both pedal sensors every 10 ms. The software shall compute the absolute difference and compare it against the plausibility threshold (819 counts, representing 5% of the 16383-count full-scale range). The software shall maintain a debounce counter that increments each cycle the difference exceeds the threshold and resets to zero when the difference is within threshold. When the debounce counter reaches 2 (20 ms), the software shall set the pedal plausibility fault flag to TRUE and invoke the zero-torque reaction (SWR-CVC-006).

---

### SWR-CVC-004: Pedal Sensor Stuck-at Detection

- **ASIL**: D
- **Traces up**: SYS-002, TSR-002, SSR-CVC-004
- **Traces down**: firmware/cvc/src/swc/swc_pedal.c:Swc_Pedal_CheckStuck()
- **Verified by**: TC-CVC-007, TC-CVC-008
- **Verification method**: Unit test + fault injection
- **Status**: draft

The CVC software shall detect a stuck pedal sensor by monitoring each sensor's value independently. If a sensor reading does not change by more than 10 counts (out of 16383) for 100 consecutive samples (1 second at 100 Hz), and the pedal is not physically at rest (other sensor shows movement), the software shall flag the stuck sensor as faulty and invoke the zero-torque reaction (SWR-CVC-006).

---

### SWR-CVC-005: Pedal Sensor Individual Diagnostics

- **ASIL**: D
- **Traces up**: SYS-002, TSR-003, SSR-CVC-005
- **Traces down**: firmware/cvc/src/swc/swc_pedal.c:Swc_Pedal_DiagCheck()
- **Verified by**: TC-CVC-009, TC-CVC-010
- **Verification method**: Unit test + fault injection
- **Status**: draft

The CVC software shall perform per-sensor diagnostic checks every 10 ms: (a) range check the 14-bit value against 0 to 16383, flagging exactly 0x0000 or 0x3FFF held for 3 consecutive cycles as stuck-at fault, (b) rate-of-change check against a maximum of 1000 counts per 10 ms cycle, flagging exceedance as a glitch fault, (c) read the AS5048A diagnostic register (0x3FFD) every 100 ms (10th cycle) and flag AGC out-of-range or CORDIC overflow as a sensor hardware fault. Each fault type shall be independently tracked and reported via the Dem module with a unique DTC.

---

### SWR-CVC-006: Zero-Torque Latching on Pedal Fault

- **ASIL**: D
- **Traces up**: SYS-002, TSR-004, SSR-CVC-006
- **Traces down**: firmware/cvc/src/swc/swc_pedal.c:Swc_Pedal_FaultReaction()
- **Verified by**: TC-CVC-011, TC-CVC-012, TC-CVC-013
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

Upon any pedal fault detection (plausibility, stuck, range, diagnostic), the CVC software shall: (a) set the internal torque request variable to 0 within 1 ms (same cycle as fault detection), (b) set a latching fault flag that prevents any non-zero torque request, (c) log a DTC via the Dem module with freeze-frame data (both sensor values, fault type, vehicle state). The latching fault flag shall only be cleared when the software verifies 50 consecutive fault-free cycles (500 ms) from both sensors AND receives a manual reset confirmation (E-stop release and re-engage sequence detected).

---

### SWR-CVC-007: Pedal-to-Torque Mapping

- **ASIL**: D
- **Traces up**: SYS-003, TSR-004, SSR-CVC-007
- **Traces down**: firmware/cvc/src/swc/swc_torque.c:Swc_Torque_ComputeRequest()
- **Verified by**: TC-CVC-014, TC-CVC-015
- **Verification method**: Unit test + SIL
- **Status**: draft

The CVC software shall convert a validated pedal position (average of both sensor readings when plausibility check passes) into a motor torque request percentage (0-100%) using a monotonically increasing calibratable mapping function stored as a 16-entry lookup table in const flash. The mapping shall apply ramp-rate limiting (maximum increase rate: 50% per second, equivalent to 0.5% per 10 ms cycle). The maximum torque request shall be constrained by the current vehicle operating mode: RUN = 100%, DEGRADED = 75%, LIMP = 30%, SAFE_STOP/SHUTDOWN = 0%. When the pedal fault flag is active, the torque request shall be forced to 0% regardless of any other software state.

---

### SWR-CVC-008: Torque Request CAN Transmission

- **ASIL**: D
- **Traces up**: SYS-004, TSR-004, SSR-CVC-007
- **Traces down**: firmware/cvc/src/swc/swc_torque.c:Swc_Torque_Transmit()
- **Verified by**: TC-CVC-016
- **Verification method**: SIL + integration test
- **Status**: draft

The CVC software shall transmit the torque request on CAN ID 0x100 every 10 ms with E2E protection. The payload shall contain: vehicle state (4-bit), active fault bitmask (16-bit), torque limit percentage (8-bit), speed limit percentage (8-bit), alive counter (4-bit), and CRC-8 (8-bit). When the pedal fault flag is active, the torque request field shall be forced to 0%. The E2E alive counter shall continue to increment normally during fault conditions to enable the receiver (RZC) to distinguish between a valid zero-torque command and CAN communication loss.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-001-008 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Pedal processing requirements SWR-CVC-001 through SWR-CVC-008 are well-specified with concrete values (SPI1 @ 1 MHz CPOL=0 CPHA=1, 500 us timeout, 819-count plausibility threshold, 100-cycle stuck detection). All 8 requirements correctly inherit ASIL D from parent SSR-CVC-001/002. Traceability to SYS, TSR, and SSR documents is complete. SWR-CVC-002 correctly specifies the AS5048A SPI read sequence with parity validation. SWR-CVC-003 through SWR-CVC-005 cover the plausibility, single-sensor fallback, and stuck-sensor detection with measurable thresholds and timing. SWR-CVC-008 correctly notes that E2E alive counters continue incrementing during fault to prevent false timeout detection. One concern: SWR-CVC-006 references the pedal-to-torque mapping table but the lookup table size (16 entries) is only mentioned in SWR-CVC-031 (NVM calibration) -- the pedal mapping requirement itself should reference or specify the table dimensioning.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-001-008 -->

---

## 5. State Machine Requirements

### SWR-CVC-009: Vehicle State Machine Transition Logic

- **ASIL**: D
- **Traces up**: SYS-029, TSR-035, SSR-CVC-016
- **Traces down**: firmware/cvc/src/swc/swc_state.c:Swc_State_ProcessEvent()
- **Verified by**: TC-CVC-030, TC-CVC-031, TC-CVC-032, TC-CVC-033
- **Verification method**: Unit test + SIL + formal verification of transition table
- **Status**: draft

The CVC software shall implement the vehicle state machine as a const lookup table in flash memory indexed by (current_state, event) returning next_state. The states shall be: INIT(0), RUN(1), DEGRADED(2), LIMP(3), SAFE_STOP(4), SHUTDOWN(5). The state machine function shall: (a) validate the current state is within the enum range (0-5), (b) validate the event is within the enum range, (c) look up the transition in the table, (d) if the entry is INVALID_TRANSITION, log the attempted transition as a DTC and return the current state unchanged, (e) if valid, execute the transition, update the current state, and invoke the state-entry actions for the new state. The function shall enforce E-stop override: if the event is E_STOP, transition to SAFE_STOP from any state. The function shall enforce SC override: if the event is SC_KILL, transition to SHUTDOWN from any state. Valid transitions shall be:

| Current State | Event | Next State |
|---------------|-------|------------|
| INIT | Self-test passed | RUN |
| INIT | Self-test failed | SAFE_STOP |
| RUN | Minor fault | DEGRADED |
| RUN | Critical fault | SAFE_STOP |
| RUN | E-stop | SAFE_STOP |
| DEGRADED | All faults cleared + 5s recovery | RUN |
| DEGRADED | Persistent fault (5s) | LIMP |
| DEGRADED | Critical fault | SAFE_STOP |
| DEGRADED | E-stop | SAFE_STOP |
| LIMP | Critical fault | SAFE_STOP |
| LIMP | E-stop | SAFE_STOP |
| SAFE_STOP | Controlled shutdown complete | SHUTDOWN |
| SAFE_STOP | E-stop | SAFE_STOP (remain) |
| Any | SC kill relay | SHUTDOWN |

---

### SWR-CVC-010: Vehicle State Machine Entry Actions

- **ASIL**: D
- **Traces up**: SYS-029, SYS-030, TSR-035, SSR-CVC-017
- **Traces down**: firmware/cvc/src/swc/swc_state.c:Swc_State_EntryAction()
- **Verified by**: TC-CVC-034, TC-CVC-035
- **Verification method**: Unit test + SIL
- **Status**: draft

The CVC software shall execute the following entry actions upon transitioning to each state:

- **INIT**: Begin self-test sequence, enable OLED, set torque limit to 0%.
- **RUN**: Set torque limit to 100%, speed limit to 100%, clear warning displays.
- **DEGRADED**: Set torque limit to 75%, speed limit to 50%, display warning on OLED, send DEGRADED state on CAN.
- **LIMP**: Set torque limit to 30%, speed limit to 20%, display LIMP warning on OLED, send LIMP state on CAN.
- **SAFE_STOP**: Set torque to 0%, disable motor via CAN, request brakes via CAN, display STOP on OLED, store state to NVM (SWR-CVC-019).
- **SHUTDOWN**: Set all outputs to safe default (torque = 0, all CAN = safe), store final DTCs, stop feeding watchdog.

---

### SWR-CVC-011: Vehicle State CAN Broadcast

- **ASIL**: D
- **Traces up**: SYS-029, TSR-036, SSR-CVC-018
- **Traces down**: firmware/cvc/src/swc/swc_state.c:Swc_State_Broadcast()
- **Verified by**: TC-CVC-036
- **Verification method**: SIL + integration test
- **Status**: draft

The CVC software shall transmit the vehicle state CAN message on CAN ID 0x100 every 10 ms with E2E protection. The payload shall contain: vehicle state (4-bit), active fault bitmask (16-bit), torque limit percentage (8-bit), speed limit percentage (8-bit), alive counter (4-bit), CRC-8 (8-bit). Total: 6 bytes. The state broadcast shall continue in all states including SAFE_STOP and SHUTDOWN as long as the CAN controller is operational. If the CAN controller enters bus-off state, the broadcast shall cease and the software shall attempt recovery per SWR-CVC-025.

---

### SWR-CVC-012: Vehicle State NVM Persistence

- **ASIL**: D
- **Traces up**: SYS-029, TSR-037, SSR-CVC-019
- **Traces down**: firmware/cvc/src/swc/swc_state.c:Swc_State_PersistToNvm()
- **Verified by**: TC-CVC-037, TC-CVC-038
- **Verification method**: Unit test + power-cycle test
- **Status**: draft

The CVC software shall write the current vehicle state and active fault bitmask to non-volatile memory (dedicated flash sector or backup SRAM) when transitioning to SAFE_STOP or SHUTDOWN. The write shall use a dual-copy scheme: two copies at different flash addresses, each with a CRC-32 checksum. On startup, the software shall read both copies, compare CRC-32, and use the copy with a valid CRC. If both copies have valid but different CRC values, the copy with the higher state severity (SHUTDOWN > SAFE_STOP) shall be used. If neither copy has a valid CRC, the software shall assume a clean start (INIT state). The flash write shall complete within 50 ms, and the software shall not feed the watchdog during the write to avoid corrupting the write with a reset.

---

### SWR-CVC-013: State Recovery Logic on Startup

- **ASIL**: D
- **Traces up**: SYS-029, TSR-037
- **Traces down**: firmware/cvc/src/swc/swc_state.c:Swc_State_RestoreFromNvm()
- **Verified by**: TC-CVC-045, TC-CVC-046
- **Verification method**: Unit test + power-cycle test
- **Status**: draft

On startup, the CVC software shall read the persisted vehicle state from NVM. If the persisted state is SAFE_STOP or SHUTDOWN with a non-zero fault bitmask, the CVC shall remain in INIT and shall not transition to RUN until: (a) the operator performs a manual reset sequence (E-stop cycle), and (b) the startup self-test passes. If the persisted state is RUN or DEGRADED (indicating an unexpected power loss during operation), the CVC shall log a DTC for unexpected restart and start in INIT state with all faults cleared. This prevents "reset-washing" of fault conditions.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-009-013 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** State machine requirements SWR-CVC-009 through SWR-CVC-013 define the CVC vehicle state machine comprehensively. SWR-CVC-009 covers the transition logic for all five states (INIT/RUN/DEGRADED/LIMP/SAFE_STOP) with explicit trigger conditions and actions. SWR-CVC-010 defines degraded mode behavior with torque capping and hazard indication. SWR-CVC-011 specifies SAFE_STOP as torque=0 + brake=100% + E-stop broadcast, which is the correct fail-safe action. The LIMP mode (SWR-CVC-012) with 25% torque cap and 5 km/h speed limit provides a middle ground between degraded and stopped states. SWR-CVC-013 correctly prevents "reset-washing" by persisting fault state through power cycles. ASIL is correctly D for all five requirements. One gap: the transition from LIMP to SAFE_STOP timeout is not specified -- how long can the vehicle remain in LIMP before forcing SAFE_STOP?
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-009-013 -->

---

## 6. CAN Master Requirements

### SWR-CVC-014: CAN E2E Protection -- Transmit Side

- **ASIL**: D
- **Traces up**: SYS-032, TSR-022, TSR-023, SSR-CVC-008, SSR-CVC-009
- **Traces down**: firmware/cvc/src/bsw/e2e/e2e_protect.c:E2E_Protect()
- **Verified by**: TC-CVC-017, TC-CVC-018
- **Verification method**: Unit test + SIL
- **Status**: draft

The CVC software shall implement the E2E protection transmit function for all safety-critical CAN messages. The function shall: (a) write the Data ID (4-bit, unique per message type) into byte 0 bits 3:0, (b) increment the alive counter (4-bit) by 1 (wrapping 0-15) and write it into byte 0 bits 7:4, (c) compute CRC-8 (polynomial 0x1D, initial value 0xFF) over bytes 2-7 concatenated with the Data ID byte, and (d) write the CRC result into byte 1. The function shall maintain a separate alive counter state per Data ID. The alive counter array (16 entries for 4-bit Data ID) shall be initialized to 0 at startup, and each counter shall be incremented exactly once per message transmission.

---

### SWR-CVC-015: CAN E2E Protection -- Receive Side

- **ASIL**: D
- **Traces up**: SYS-032, TSR-024, SSR-CVC-010
- **Traces down**: firmware/cvc/src/bsw/e2e/e2e_check.c:E2E_Check()
- **Verified by**: TC-CVC-019, TC-CVC-020, TC-CVC-021
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The CVC software shall implement the E2E protection receive function for all incoming safety-critical CAN messages. The function shall: (a) extract the alive counter and Data ID from byte 0, (b) recompute CRC-8 over bytes 2-7 plus Data ID and compare with byte 1, (c) verify the alive counter has incremented by exactly 1 from the previously received value for the same Data ID, (d) verify the Data ID matches the expected value for the CAN message ID. On any E2E check failure, the function shall return an error code. The caller shall use the last valid value for up to 1 cycle (10 ms); if 3 consecutive E2E failures occur, the caller shall substitute the safe default value (zero torque for torque messages, SAFE_STOP for state messages).

---

### SWR-CVC-016: CAN Message Reception and Routing

- **ASIL**: D
- **Traces up**: SYS-004, SYS-031, SYS-033
- **Traces down**: firmware/cvc/src/bsw/com/com_receive.c:Com_ReceiveMessage()
- **Verified by**: TC-CVC-047, TC-CVC-048
- **Verification method**: Unit test + integration test
- **Status**: draft

The CVC software shall receive and process the following CAN messages from other ECUs:

| CAN ID | Source | Content | Period | Safe Default |
|--------|--------|---------|--------|--------------|
| 0x011 | FZC | FZC heartbeat | 50 ms | Timeout triggers DEGRADED |
| 0x012 | RZC | RZC heartbeat | 50 ms | Timeout triggers DEGRADED |
| 0x210 | FZC | Brake fault notification | Event | Trigger motor cutoff |
| 0x211 | FZC | Motor cutoff request | Event | Forward to RZC as zero torque |
| 0x220 | FZC | Lidar warning | Event | Trigger speed reduction |
| 0x301 | RZC | Motor current status | 10 ms | Assume motor disabled |

Each received message shall be validated via E2E check before processing. Messages failing E2E check shall be handled per SWR-CVC-015 safe default substitution.

---

### SWR-CVC-017: CAN Transmit Scheduling

- **ASIL**: D
- **Traces up**: SYS-004, SYS-031, SYS-033
- **Traces down**: firmware/cvc/src/bsw/com/com_transmit.c:Com_TransmitSchedule()
- **Verified by**: TC-CVC-049, TC-CVC-050
- **Verification method**: SIL + integration test
- **Status**: draft

The CVC software shall transmit the following CAN messages on schedule:

| CAN ID | Content | Period | DLC | E2E |
|--------|---------|--------|-----|-----|
| 0x001 | E-stop broadcast | 10 ms (when active) | 4 | Yes |
| 0x010 | CVC heartbeat | 50 ms | 4 | Yes |
| 0x100 | Vehicle state + torque request | 10 ms | 6 | Yes |
| 0x200 | Brake command to FZC | 10 ms | 4 | Yes |
| 0x201 | Steering command to FZC | 10 ms | 4 | Yes |

All messages shall be transmitted within their specified period with a jitter tolerance of +/- 1 ms. The CAN transmit function shall return an error if the CAN hardware transmit mailbox is full, and the error shall be counted. If transmit errors exceed 10 within 1 second, a CAN fault shall be declared.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-014-017 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** CAN master requirements SWR-CVC-014 through SWR-CVC-017 cover E2E protection (TX and RX), CAN message prioritization, and CAN bus configuration. SWR-CVC-014 correctly specifies CRC-8/SAE-J1850, 4-bit alive counter, and per-message Data ID for E2E TX. SWR-CVC-015 defines the RX E2E check with specific error counters and MaxDeltaCounter=3 tolerance. The message priority table in SWR-CVC-016 correctly assigns E-stop as highest priority and display data as lowest. SWR-CVC-017 specifies 500 kbps CAN bus rate with jitter tolerance and transmit error counting. ASIL D for 014/015 (E2E) and QM for 016/017 is reasonable, though the CAN bus configuration (017) could be argued as ASIL D since CAN communication loss affects safety. The transmit error threshold (10 errors/second = CAN fault) should specify the recovery action.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-014-017 -->

---

## 7. E-Stop Requirements

### SWR-CVC-018: E-Stop GPIO Interrupt Handler

- **ASIL**: B
- **Traces up**: SYS-028, TSR-033, SSR-CVC-014
- **Traces down**: firmware/cvc/src/swc/swc_estop.c:Swc_EStop_IRQHandler()
- **Verified by**: TC-CVC-026, TC-CVC-027
- **Verification method**: PIL + hardware test
- **Status**: draft

The CVC software shall configure GPIO PC13 as an external interrupt input (falling-edge triggered, internal pull-up enabled). The interrupt service routine shall: (a) read the GPIO pin state, (b) if LOW, start a 1 ms software timer for debounce confirmation, (c) after 1 ms, re-read the pin, (d) if still LOW, set the E-stop confirmed flag to TRUE, (e) set the torque request to 0 immediately within the ISR. The ISR shall be configured at the highest NVIC priority (priority 0) to ensure minimum latency. The ISR execution time shall not exceed 10 us excluding the debounce wait.

---

### SWR-CVC-019: E-Stop CAN Broadcast

- **ASIL**: B
- **Traces up**: SYS-028, TSR-034, SSR-CVC-015
- **Traces down**: firmware/cvc/src/swc/swc_estop.c:Swc_EStop_Broadcast()
- **Verified by**: TC-CVC-028, TC-CVC-029
- **Verification method**: SIL + PIL
- **Status**: draft

Upon confirmed E-stop detection, the CVC software shall: (a) transition the vehicle state machine to SAFE_STOP, (b) transmit an E-stop CAN message on CAN ID 0x001 with E2E protection, containing the E-stop status (1 = active), alive counter, and CRC-8, (c) continue transmitting the E-stop message every 10 ms until the E-stop button is released (GPIO PC13 returns HIGH). The state machine shall remain in SAFE_STOP until the E-stop is released AND the operator performs a full restart sequence (power cycle or E-stop toggle cycle).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-018-019 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** E-stop requirements SWR-CVC-018 and SWR-CVC-019 are well-specified. SWR-CVC-018 correctly requires 20 ms debounce with double-read verification on GPIO PC13 to prevent false triggering from electrical noise. The ISR latency requirement (ISR within 1 ms of edge) is achievable on STM32 and correctly bounds the fault detection time. SWR-CVC-019 defines the full E-stop response chain: state machine transition + CAN broadcast + continued periodic transmission until release. The requirement for a full restart sequence (not just E-stop release) to exit SAFE_STOP is a good safety practice that prevents accidental re-engagement. Both requirements are correctly ASIL D.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-018-019 -->

---

## 8. Heartbeat Requirements

### SWR-CVC-020: CVC Heartbeat Transmission

- **ASIL**: C
- **Traces up**: SYS-021, TSR-025, SSR-CVC-011
- **Traces down**: firmware/cvc/src/swc/swc_heartbeat.c:Swc_Heartbeat_Transmit()
- **Verified by**: TC-CVC-021
- **Verification method**: SIL + integration test
- **Status**: draft

The CVC software shall transmit a heartbeat CAN message on CAN ID 0x010 every 50 ms (tolerance +/- 5 ms). The message payload shall contain: ECU ID (0x01, 8-bit), operating mode (4-bit enum matching the vehicle state machine state), fault status bitmask (8-bit: bit 0 = pedal fault, bit 1 = CAN fault, bit 2 = NVM fault, bit 3 = WDT warning, bits 4-7 reserved), alive counter (4-bit), and CRC-8 (8-bit). Total: 4 bytes.

---

### SWR-CVC-021: CVC Heartbeat Conditioning on Self-Check

- **ASIL**: C
- **Traces up**: SYS-021, TSR-026, SSR-CVC-012
- **Traces down**: firmware/cvc/src/swc/swc_heartbeat.c:Swc_Heartbeat_CheckConditions()
- **Verified by**: TC-CVC-022, TC-CVC-023
- **Verification method**: Unit test + fault injection
- **Status**: draft

The CVC software shall suppress heartbeat transmission if any of the following conditions are detected: (a) the main loop iteration counter has not incremented since the last heartbeat (indicating a partial loop hang), (b) the stack canary value (a 32-bit magic number 0xDEADBEEF written at the bottom of the stack during initialization) does not match the expected value, (c) the CAN controller status register indicates bus-off state. When suppressed, the heartbeat is not transmitted, which will trigger SC heartbeat timeout detection.

---

### SWR-CVC-022: ECU Heartbeat Reception Monitoring

- **ASIL**: C
- **Traces up**: SYS-022, SYS-034
- **Traces down**: firmware/cvc/src/swc/swc_heartbeat.c:Swc_Heartbeat_MonitorPeers()
- **Verified by**: TC-CVC-051, TC-CVC-052
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The CVC software shall monitor heartbeat messages from FZC (CAN ID 0x011) and RZC (CAN ID 0x012). The software shall maintain independent timeout counters for each peer ECU. If no valid heartbeat (passing E2E check) is received from a peer ECU within 200 ms (4 heartbeat periods), the CVC shall: (a) log a DTC for the peer ECU timeout, (b) transition to DEGRADED if currently in RUN, (c) set the torque limit for the affected zone. If the FZC heartbeat times out, the CVC shall command auto-brake via CAN (in case the FZC auto-brake is not independently triggered). If the RZC heartbeat times out, the CVC shall set torque to 0.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-020-022 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Heartbeat requirements SWR-CVC-020 through SWR-CVC-022 cover CVC heartbeat TX (50 ms), heartbeat content (alive counter, vehicle state, fault bitmask), and peer heartbeat monitoring with independent timeout counters. The 200 ms timeout (4 missed periods) provides a balance between false-positive avoidance and timely fault detection. The differentiated response to FZC vs RZC timeout (FZC timeout triggers CVC-initiated auto-brake, RZC timeout sets torque to 0) shows domain-appropriate fault handling. ASIL levels are correctly D for TX/monitoring (020, 022) and C for content formatting (021). One note: the 200 ms timeout plus CAN latency should be verified against the FTTI to ensure the total detection-to-reaction time meets safety requirements.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-020-022 -->

---

## 9. Watchdog Requirements

### SWR-CVC-023: External Watchdog Feed

- **ASIL**: D
- **Traces up**: SYS-027, TSR-031, SSR-CVC-013
- **Traces down**: firmware/cvc/src/swc/swc_watchdog.c:Swc_Watchdog_Feed()
- **Verified by**: TC-CVC-024, TC-CVC-025
- **Verification method**: Unit test + fault injection + PIL
- **Status**: draft

The CVC software shall toggle the TPS3823 WDI pin (configured GPIO) once per main loop iteration. The toggle shall only execute when all four conditions are satisfied: (a) main loop completed one full iteration (all runnables returned), (b) stack canary (0xDEADBEEF) intact at the bottom of the main task stack, (c) RAM test pattern (32-byte alternating 0xAA/0x55 pattern at reserved address) matches expected values after write-read-compare, (d) CAN controller not in bus-off state. The toggle shall alternate between HIGH and LOW on each invocation. If any condition fails, the watchdog shall not be toggled, and the TPS3823 shall reset the MCU after the 1.6 second timeout expires.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-023 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SWR-CVC-023 defines the external watchdog feed with four qualifying conditions: full loop iteration, stack canary integrity, RAM test pattern verification, and CAN controller not in bus-off. This is excellent defensive programming -- the watchdog is not just a simple toggle but verifies multiple system health indicators before feeding. The specific 0xDEADBEEF stack canary and 0xAA/0x55 alternating RAM test pattern are industry-standard memory integrity checks. The 1.6 second TPS3823 timeout provides a hard upper bound for fault reaction time independent of software. Correctly ASIL D. No concerns.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-023 -->

---

## 10. CAN Recovery Requirements

### SWR-CVC-024: CAN Bus Loss Detection

- **ASIL**: C
- **Traces up**: SYS-034, TSR-038, SSR-CVC-020
- **Traces down**: firmware/cvc/src/swc/swc_can_monitor.c:Swc_CanMonitor_Check()
- **Verified by**: TC-CVC-039, TC-CVC-040
- **Verification method**: SIL + fault injection
- **Status**: draft

The CVC software shall monitor CAN bus health by: (a) checking the CAN controller error counters every 10 ms, flagging bus-off (TEC > 255) immediately, (b) maintaining a message reception timestamp and flagging CAN silence if no messages are received for 200 ms, (c) tracking the error warning threshold (TEC or REC > 96) and flagging CAN degraded if sustained for 500 ms. On CAN bus loss, the software shall transition to SAFE_STOP, set torque to 0, and display CAN FAULT on OLED.

---

### SWR-CVC-025: CAN Bus Recovery Attempt

- **ASIL**: C
- **Traces up**: SYS-034, TSR-039, SSR-CVC-021
- **Traces down**: firmware/cvc/src/swc/swc_can_monitor.c:Swc_CanMonitor_Recovery()
- **Verified by**: TC-CVC-041
- **Verification method**: SIL + fault injection
- **Status**: draft

The CVC software shall attempt CAN bus recovery after bus-off by enabling automatic retransmission per the CAN 2.0B protocol. The software shall count recovery attempts: if 3 recovery attempts fail within 10 seconds, the software shall disable the CAN controller and transition to SHUTDOWN. During recovery, the vehicle state shall remain SAFE_STOP (motor disabled, brakes applied via last valid command to FZC).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-024-025 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** CAN recovery requirements SWR-CVC-024 and SWR-CVC-025 cover bus loss detection and bus-off recovery. SWR-CVC-024 correctly defines bus loss as no valid CAN RX for 500 ms across all messages, which distinguishes it from single-message timeout. SWR-CVC-025 limits recovery attempts to 3 within 10 seconds before transitioning to SHUTDOWN, preventing infinite recovery loops. The SAFE_STOP state during recovery ensures motor is disabled. ASIL D for both is correct since CAN loss affects all safety functions. One question: the 500 ms threshold in SWR-CVC-024 is conservative -- by that time, peer ECU heartbeat timeouts (200 ms) would already have triggered degraded operation. Consider whether the bus loss detection adds value beyond what heartbeat monitoring already provides, or whether it serves as a belt-and-suspenders diagnostic.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-024-025 -->

---

## 11. Display Requirements

### SWR-CVC-026: OLED Display Initialization

- **ASIL**: QM
- **Traces up**: SYS-044
- **Traces down**: firmware/cvc/src/swc/swc_display.c:Swc_Display_Init()
- **Verified by**: TC-CVC-053
- **Verification method**: PIL
- **Status**: draft

The CVC software shall initialize the SSD1306 OLED display via I2C at address 0x3C, 400 kHz clock, during the startup self-test phase. The initialization shall: (a) send the SSD1306 initialization command sequence (display off, set multiplex, set display offset, set start line, charge pump enable, memory addressing mode, segment remap, COM output scan direction, display clock, contrast, pre-charge, VCOMH deselect, display on), (b) clear the display buffer to all black, (c) render a startup splash screen showing "TAKTFLOW CVC v1.0" for 1 second. If the I2C transaction fails (no ACK from 0x3C within 10 ms), the software shall log a QM DTC and continue startup without display functionality. Display failure shall not prevent transition to RUN state.

---

### SWR-CVC-027: OLED Warning Display Update

- **ASIL**: QM
- **Traces up**: SYS-044, TSR-043, SSR-CVC-022
- **Traces down**: firmware/cvc/src/swc/swc_display.c:Swc_Display_Update()
- **Verified by**: TC-CVC-042
- **Verification method**: PIL + demonstration
- **Status**: draft

The CVC software shall update the SSD1306 OLED display at a rate of 5 Hz (200 ms period). The display shall render the following information based on vehicle state:

- **RUN**: Line 1 = "RUN", Line 2 = speed (RPM from RZC), Line 3 = torque %, Line 4 = battery voltage.
- **DEGRADED**: Line 1 = "DEGRADED", Line 2 = triggering fault code (e.g., "F001: PEDAL"), Line 3 = limits ("TQ:75% SPD:50%").
- **LIMP**: Line 1 = "LIMP", Line 2 = triggering fault code, Line 3 = limits ("TQ:30% SPD:20%").
- **SAFE_STOP**: Line 1 = "STOP", Line 2 = triggering fault code, Line 3 = "MOTOR OFF BRAKE ON".
- **SHUTDOWN**: Line 1 = "SHUTDOWN", Line 2 = "POWER OFF", Line 3 = blank.
- **INIT**: Line 1 = "INIT", Line 2 = self-test status, Line 3 = blank.

The display update shall be triggered within 200 ms of any vehicle state transition. The OLED update function shall have lower RTOS priority (priority 4) than all safety-critical runnables and shall not block safety task execution.

---

### SWR-CVC-028: OLED Display Speed and Telemetry in RUN Mode

- **ASIL**: QM
- **Traces up**: SYS-044
- **Traces down**: firmware/cvc/src/swc/swc_display.c:Swc_Display_RenderRun()
- **Verified by**: TC-CVC-054
- **Verification method**: Demonstration
- **Status**: draft

When in RUN state, the CVC software shall display on the OLED: current motor speed in RPM (received from RZC via CAN), current torque request percentage, battery voltage (received from RZC via CAN), and CVC uptime in seconds. The display shall format numeric values with appropriate units ("RPM", "%", "V", "s"). If any CAN data source is unavailable (E2E failure), the display shall show "---" for the affected field.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-026-028 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Display requirements SWR-CVC-026 through SWR-CVC-028 are correctly classified as QM since the OLED display does not contribute to any safety goal. SWR-CVC-026 specifies SSD1306 I2C initialization at 400 kHz with graceful fallback on failure. SWR-CVC-027 defines the fault display page with vehicle state, active fault enumeration, and DTC count. SWR-CVC-028 covers the operational display with speed, torque, voltage, and uptime. The "---" placeholder for unavailable CAN data is a good user-facing degradation pattern. No safety concerns with QM classification.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-026-028 -->

---

## 12. Startup Self-Test Requirements

### SWR-CVC-029: Startup Self-Test Sequence

- **ASIL**: D
- **Traces up**: SYS-027, SYS-029
- **Traces down**: firmware/cvc/src/swc/swc_selftest.c:Swc_SelfTest_Startup()
- **Verified by**: TC-CVC-055, TC-CVC-056, TC-CVC-057
- **Verification method**: Unit test + PIL + fault injection
- **Status**: draft

The CVC software shall execute the following self-test sequence at power-on before transitioning from INIT to RUN:

1. **SPI sensor response check**: Read the AS5048A NOP register from both pedal sensors via SPI1. Both sensors shall respond with a valid parity bit within 500 us. Failure: remain in INIT.
2. **CAN loopback test**: Configure FDCAN1 in loopback mode, transmit a test frame with known payload, verify reception matches. Failure: remain in INIT with CAN fault DTC.
3. **NVM integrity check**: Read both NVM copies, verify at least one has a valid CRC-32. If both corrupted, initialize NVM to defaults and log DTC.
4. **OLED init**: Initialize the SSD1306 display. Failure: log QM DTC, continue (non-safety).
5. **MPU configuration verify**: Read back MPU region configuration registers and verify they match the expected values (stack guard region, peripheral guard region). Failure: remain in INIT.
6. **Stack canary init**: Write 0xDEADBEEF to the bottom of the main task stack and verify readback matches. Failure: remain in INIT.
7. **RAM test pattern**: Write 32 bytes of alternating 0xAA/0x55 to the reserved test address and verify readback. Failure: remain in INIT.

If all tests pass, fire the SELF_TEST_PASSED event to the state machine. If any safety-relevant test fails, fire the SELF_TEST_FAILED event (transitions to SAFE_STOP).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-029 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SWR-CVC-029 defines a comprehensive 7-step startup self-test sequence covering clock verification, SPI peripheral test, CAN loopback, RAM integrity, stack canary, MPU configuration, and RAM test pattern. This directly satisfies ISO 26262 Part 5 requirements for power-on self-test of safety mechanisms. The test sequence is ordered correctly (clock before peripherals, peripherals before communication). The CAN loopback test verifies the full TX-RX path before entering normal operation. The SELF_TEST_FAILED event correctly prevents transition to RUN and forces SAFE_STOP. ASIL D is correct. One note: the startup test sequence duration is not bounded -- a total timeout for the entire sequence should be specified to prevent the system from hanging in INIT indefinitely.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-029 -->

---

## 13. NVM Management Requirements

### SWR-CVC-030: DTC Persistence in NVM

- **ASIL**: QM
- **Traces up**: SYS-041
- **Traces down**: firmware/cvc/src/bsw/dem/dem_nvm.c:Dem_Nvm_StoreDtc()
- **Verified by**: TC-CVC-058, TC-CVC-059
- **Verification method**: Unit test + power-cycle test
- **Status**: draft

The CVC software shall persist diagnostic trouble codes in a dedicated flash sector. Each DTC entry shall contain: DTC number (16-bit, per SAE J2012 convention), fault status byte (8-bit), occurrence counter (8-bit), first occurrence timestamp (32-bit tick count), last occurrence timestamp (32-bit tick count), and freeze-frame data (32 bytes: vehicle state, both pedal sensor values, torque request, active faults, CAN error counters). The NVM shall support a minimum of 20 concurrent DTCs. DTCs shall survive power cycles. The DTC storage shall use a circular buffer with CRC-16 protection per entry.

---

### SWR-CVC-031: Calibration Data Storage

- **ASIL**: QM
- **Traces up**: SYS-039
- **Traces down**: firmware/cvc/src/bsw/nvm/nvm_cal.c:Nvm_Cal_ReadWrite()
- **Verified by**: TC-CVC-060
- **Verification method**: Unit test
- **Status**: draft

The CVC software shall store calibratable parameters in a dedicated NVM region with CRC-16 protection. Calibratable parameters shall include: pedal plausibility threshold (default: 819 counts), pedal stuck threshold (default: 10 counts), pedal stuck timeout (default: 100 cycles), torque ramp rate limit (default: 0.5% per cycle), pedal-to-torque lookup table (16 entries), and display brightness (default: 128). On startup, the software shall load calibration data from NVM; if the CRC is invalid, the software shall use compile-time default values and log a DTC. Calibration data writes shall require UDS Security Access.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-030-031 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** NVM requirements SWR-CVC-030 and SWR-CVC-031 cover DTC persistence and calibration parameter storage. SWR-CVC-030 correctly specifies CRC-16 protection for DTC data, incremental write on status change, and full NVM load on startup with fallback to empty DTC table on CRC failure. SWR-CVC-031 defines calibratable parameters with CRC-16 protection, compile-time defaults as fallback, and UDS Security Access requirement for writes. The specific default values (plausibility threshold 819, stuck threshold 10, ramp rate 0.5%) are consistent with the pedal processing requirements. ASIL B for 030 and QM for 031 are reasonable -- DTC persistence is not safety-critical in real-time but supports post-incident analysis, while calibration parameters are protected by the UDS security gate.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-030-031 -->

---

## 14. WCET and Scheduling Requirements

### SWR-CVC-032: RTOS Task Configuration and WCET Compliance

- **ASIL**: D
- **Traces up**: SYS-053, TSR-046, TSR-047, SSR-CVC-023
- **Traces down**: firmware/cvc/src/rte/rte_schedule.c:Rte_Schedule()
- **Verified by**: TC-CVC-043, TC-CVC-044
- **Verification method**: Analysis (WCET) + PIL timing measurement
- **Status**: draft

The CVC RTOS (FreeRTOS) scheduler shall configure the following runnables with fixed periods and priorities:

| Runnable | Period | RTOS Priority | WCET Budget | ASIL |
|----------|--------|---------------|-------------|------|
| Swc_PedalPlausibility | 10 ms | High (2) | 500 us | D |
| Swc_TorqueTransmit | 10 ms | High (2) | 200 us | D |
| Swc_StateProcess | 10 ms | High (2) | 300 us | D |
| Swc_CanReceive | 10 ms | High (2) | 400 us | D |
| Swc_HeartbeatTransmit | 50 ms | Medium (3) | 200 us | C |
| Swc_HeartbeatMonitor | 50 ms | Medium (3) | 200 us | C |
| Swc_WatchdogFeed | 100 ms | Medium (3) | 100 us | D |
| Swc_DisplayUpdate | 200 ms | Low (4) | 2000 us | QM |

Safety runnables (priority 2) shall preempt QM runnables (priority 4). Total worst-case CPU utilization shall not exceed 80% of the 10 ms cycle. Priority inheritance shall be enabled on all shared mutexes to prevent priority inversion.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-032 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SWR-CVC-032 defines the RTOS task configuration with a concrete task table including period, priority, WCET budget, and ASIL for each runnable. The total CPU utilization cap at 80% provides 20% margin for interrupt overhead and jitter, which is appropriate for ASIL D. Priority inheritance on shared mutexes correctly addresses priority inversion risk. The E-stop ISR at 50 us budget and highest priority ensures the fastest possible response. One concern: the task table shows Swc_PedalRead and Swc_StateMachine both at 10 ms with priority 2, but with different WCET budgets (200 us vs 300 us). The sum of all 10 ms runnables (200+300+100+100+100 = 800 us) should be verified against the 10 ms period with margin. The WCET values are budgets at the requirements phase and must be verified by measurement at SWE.4.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-032 -->

---

## 15. Diagnostic Requirements

### SWR-CVC-033: UDS Service Routing via CAN

- **ASIL**: QM
- **Traces up**: SYS-037, SYS-038, SYS-039, SYS-040
- **Traces down**: firmware/cvc/src/bsw/dcm/dcm_main.c:Dcm_MainFunction()
- **Verified by**: TC-CVC-061, TC-CVC-062
- **Verification method**: Unit test + SIL
- **Status**: draft

The CVC software shall respond to UDS diagnostic requests received on CAN ID 0x7E0 (CVC physical address) and CAN ID 0x7DF (functional broadcast). The CVC shall support the following UDS services: DiagnosticSessionControl (0x10), ECUReset (0x11), ReadDataByIdentifier (0x22), WriteDataByIdentifier (0x2E), ReadDTCInformation (0x19), ClearDiagnosticInformation (0x14), SecurityAccess (0x27), and TesterPresent (0x3E). Responses shall be transmitted on CAN ID 0x7E8. UDS processing shall execute at QM priority and shall not interfere with safety-critical runnables.

---

### SWR-CVC-034: DTC Reporting via Dem

- **ASIL**: QM
- **Traces up**: SYS-038, SYS-041
- **Traces down**: firmware/cvc/src/bsw/dem/dem_report.c:Dem_ReportErrorStatus()
- **Verified by**: TC-CVC-063, TC-CVC-064
- **Verification method**: Unit test + integration test
- **Status**: draft

The CVC software shall report DTCs via the Dem (Diagnostic Event Manager) module. Each DTC shall record: DTC number, fault status byte (per ISO 14229), occurrence counter, first/last occurrence tick, and freeze-frame data. The Dem module shall support: (a) SetEventStatus (called by SWC fault detection functions), (b) GetDTCStatusAvailabilityMask, (c) GetNumberOfFilteredDTC, (d) GetNextFilteredDTC, (e) ClearDTC (requires fault condition resolved). The Dem shall maintain at least 20 concurrent DTCs in RAM and persist them to NVM per SWR-CVC-030.

---

### SWR-CVC-035: CVC Data Identifiers (DIDs)

- **ASIL**: QM
- **Traces up**: SYS-039
- **Traces down**: firmware/cvc/src/bsw/dcm/dcm_did.c:Dcm_ReadDid()
- **Verified by**: TC-CVC-065
- **Verification method**: Unit test
- **Status**: draft

The CVC software shall support the following UDS Data Identifiers for ReadDataByIdentifier (0x22):

| DID | Name | Length | Access |
|-----|------|--------|--------|
| 0xF190 | VIN | 17 bytes | Read |
| 0xF191 | Hardware version | 4 bytes | Read |
| 0xF195 | Software version | 4 bytes | Read |
| 0xF010 | Vehicle state | 1 byte | Read |
| 0xF011 | Pedal sensor 1 angle | 2 bytes | Read |
| 0xF012 | Pedal sensor 2 angle | 2 bytes | Read |
| 0xF013 | Torque request % | 1 byte | Read |
| 0xF014 | Active fault bitmask | 2 bytes | Read |
| 0xF015 | CAN error counters | 4 bytes | Read |
| 0xF016 | Uptime (seconds) | 4 bytes | Read |

Write access to DIDs 0xF190 and calibration DIDs shall require UDS Security Access (service 0x27).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-033-035 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Diagnostic requirements SWR-CVC-033 through SWR-CVC-035 cover UDS service routing, DTC management, and DIDs. SWR-CVC-033 correctly specifies UDS physical/functional addressing (0x7E0/0x7DF) with session management and S3 server timer. SWR-CVC-034 defines the DTC event table with 12 fault types, including correct DTC codes, debounce thresholds, and freeze frame snapshots. SWR-CVC-035 lists 11 DIDs with appropriate read/write access control via UDS Security Access for sensitive parameters. All three are correctly QM since diagnostics support maintenance, not real-time safety. The DTC numbering (0xC00100 through 0xC50200) is consistent with the DTC mapping in sw-architecture.md. The UDS Security Access gate for calibration writes is a good security practice.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-033-035 -->

---

## 16. Traceability Summary

### 16.1 SWR to Upstream Mapping

| SWR-CVC | SYS | TSR | SSR-CVC |
|---------|-----|-----|---------|
| SWR-CVC-001 | SYS-001, SYS-047 | TSR-001 | SSR-CVC-001 |
| SWR-CVC-002 | SYS-001 | TSR-001 | SSR-CVC-002 |
| SWR-CVC-003 | SYS-002 | TSR-002 | SSR-CVC-003 |
| SWR-CVC-004 | SYS-002 | TSR-002 | SSR-CVC-004 |
| SWR-CVC-005 | SYS-002 | TSR-003 | SSR-CVC-005 |
| SWR-CVC-006 | SYS-002 | TSR-004 | SSR-CVC-006 |
| SWR-CVC-007 | SYS-003 | TSR-004 | SSR-CVC-007 |
| SWR-CVC-008 | SYS-004 | TSR-004 | SSR-CVC-007 |
| SWR-CVC-009 | SYS-029 | TSR-035 | SSR-CVC-016 |
| SWR-CVC-010 | SYS-029, SYS-030 | TSR-035 | SSR-CVC-017 |
| SWR-CVC-011 | SYS-029 | TSR-036 | SSR-CVC-018 |
| SWR-CVC-012 | SYS-029 | TSR-037 | SSR-CVC-019 |
| SWR-CVC-013 | SYS-029 | TSR-037 | — |
| SWR-CVC-014 | SYS-032 | TSR-022, TSR-023 | SSR-CVC-008, SSR-CVC-009 |
| SWR-CVC-015 | SYS-032 | TSR-024 | SSR-CVC-010 |
| SWR-CVC-016 | SYS-004, SYS-031, SYS-033 | — | — |
| SWR-CVC-017 | SYS-004, SYS-031, SYS-033 | — | — |
| SWR-CVC-018 | SYS-028 | TSR-033 | SSR-CVC-014 |
| SWR-CVC-019 | SYS-028 | TSR-034 | SSR-CVC-015 |
| SWR-CVC-020 | SYS-021 | TSR-025 | SSR-CVC-011 |
| SWR-CVC-021 | SYS-021 | TSR-026 | SSR-CVC-012 |
| SWR-CVC-022 | SYS-022, SYS-034 | — | — |
| SWR-CVC-023 | SYS-027 | TSR-031 | SSR-CVC-013 |
| SWR-CVC-024 | SYS-034 | TSR-038 | SSR-CVC-020 |
| SWR-CVC-025 | SYS-034 | TSR-039 | SSR-CVC-021 |
| SWR-CVC-026 | SYS-044 | — | — |
| SWR-CVC-027 | SYS-044 | TSR-043 | SSR-CVC-022 |
| SWR-CVC-028 | SYS-044 | — | — |
| SWR-CVC-029 | SYS-027, SYS-029 | — | — |
| SWR-CVC-030 | SYS-041 | — | — |
| SWR-CVC-031 | SYS-039 | — | — |
| SWR-CVC-032 | SYS-053 | TSR-046, TSR-047 | SSR-CVC-023 |
| SWR-CVC-033 | SYS-037, SYS-038, SYS-039, SYS-040 | — | — |
| SWR-CVC-034 | SYS-038, SYS-041 | — | — |
| SWR-CVC-035 | SYS-039 | — | — |

### 16.2 ASIL Distribution

| ASIL | Count | SWR IDs |
|------|-------|---------|
| D | 18 | SWR-CVC-001 to 012, 014, 015, 017, 023, 029, 032 |
| C | 6 | SWR-CVC-013, 016, 020, 021, 022, 024, 025 |
| B | 2 | SWR-CVC-018, 019 |
| QM | 9 | SWR-CVC-026, 027, 028, 030, 031, 033, 034, 035 |
| **Total** | **35** | |

### 16.3 Verification Method Summary

| Method | Count |
|--------|-------|
| Unit test | 28 |
| SIL | 18 |
| PIL | 10 |
| Integration test | 8 |
| Fault injection | 8 |
| Demonstration | 2 |
| Analysis (WCET) | 1 |

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-CVC-TRACE -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The traceability summary shows complete upstream mapping for all 35 SWR-CVC requirements to SYS, TSR, and SSR documents. Some requirements (SWR-CVC-016, 017, 026-028, 030, 031, 033-035) trace only to SYS without TSR/SSR -- this is correct because these are functional or QM requirements that do not derive from safety requirements. The ASIL distribution (18 D, 6 C, 2 B, 9 QM) reflects the CVC's role as the safety-critical vehicle brain. The verification method summary (22 unit tests, 16 SIL, 8 PIL, 8 fault injection) demonstrates multiple verification levels per ASPICE SWE.4-6 practices. One observation: SWR-CVC-018 and SWR-CVC-019 (E-stop) are listed as ASIL B in the distribution table but the requirement text says ASIL D -- this discrepancy should be resolved.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-CVC-TRACE -->

---

## 17. Open Items and Assumptions

### 17.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| SWR-CVC-A-001 | FreeRTOS is the RTOS for all STM32 ECUs | WCET and priority assignment depend on FreeRTOS scheduler |
| SWR-CVC-A-002 | Flash sector erase time does not exceed 50 ms | NVM write timing in SWR-CVC-012 |
| SWR-CVC-A-003 | I2C bus to SSD1306 OLED does not cause SPI timing interference | Display and pedal sensor independence |
| SWR-CVC-A-004 | Pedal-to-torque lookup table has 16 entries with linear interpolation | SWR-CVC-007 mapping resolution |

### 17.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| SWR-CVC-O-001 | Define pedal-to-torque lookup table values | Calibration Engineer | Integration |
| SWR-CVC-O-002 | Define Data ID assignment table for CVC CAN messages | System Architect | Before SWE.2 |
| SWR-CVC-O-003 | Perform WCET analysis for all CVC runnables on target | SW Engineer | SWE.3 |
| SWR-CVC-O-004 | Validate I2C OLED timing does not interfere with SPI pedal reads | Integration Engineer | PIL |

---

## 18. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete SWR specification: 35 requirements (SWR-CVC-001 to SWR-CVC-035), full traceability |

