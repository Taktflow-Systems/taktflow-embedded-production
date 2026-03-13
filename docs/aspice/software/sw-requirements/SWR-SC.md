---
document_id: SWR-SC
title: "Software Requirements — SC"
version: "1.0"
status: draft
aspice_process: SWE.1
ecu: SC
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

Every requirement (SWR-SC-NNN) in this document that undergoes HITL review discussion MUST have its own lessons-learned file in [`docs/aspice/software/lessons-learned/`](../lessons-learned/). One file per requirement (SWR-SC-NNN). File naming: `SWR-SC-NNN-<short-title>.md`.


# Software Requirements — Safety Controller (SC)

<!-- NO AUTOSAR — independent safety monitor, bare-metal, ~400 LOC -->

## 1. Purpose

This document specifies the complete software requirements for the Safety Controller (SC), the TI TMS570LC43x LaunchPad-based independent safety monitor of the Taktflow Zonal Vehicle Platform. These requirements are derived from system requirements (SYS), technical safety requirements (TSR), and software safety requirements (SSR) per Automotive SPICE 4.0 SWE.1 (Software Requirements Analysis).

The SC is bare-metal firmware (no RTOS, no AUTOSAR BSW) running a simple cooperative main loop with a 10 ms tick. Total code size target: approximately 400 lines of C. The SC monitors all zone ECUs via CAN listen-only mode, performs cross-plausibility checks, controls the kill relay, drives fault LEDs, and relies on the TMS570's hardware lockstep CPU for computation error detection.

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

Same conventions as SWR-CVC document section 3. Note: all SC requirements trace down to `firmware/sc/src/` (no BSW subdirectory -- SC is bare-metal).

---

## 4. CAN Listen-Only Requirements

### SWR-SC-001: DCAN1 Silent Mode Configuration

- **ASIL**: C
- **Traces up**: SYS-025, TSR-024, SSR-SC-001
- **Traces down**: firmware/sc/src/sc_can.c:SC_CAN_Init()
- **Verified by**: TC-SC-001, TC-SC-002
- **Verification method**: Unit test + PIL + hardware test
- **Status**: draft

The SC software shall configure DCAN1 for listen-only (silent) mode by setting bit 3 of the DCAN TEST register. This mode enables the SC to receive all CAN messages on the bus without transmitting any frames, ACKs, or error frames. The initialization shall: (a) enable DCAN1 peripheral clock, (b) set baud rate to 500 kbps (matching the system CAN bus), (c) enter initialization mode, (d) set the TEST register bit 3 (silent mode), (e) configure acceptance masks and filters per SWR-SC-002, (f) exit initialization mode. The software shall verify the TEST register readback confirms silent mode is active. The SC shall never call any CAN transmit function.

---

### SWR-SC-002: CAN Message Acceptance Filtering

- **ASIL**: D
- **Traces up**: SYS-025, TSR-024, SSR-SC-002
- **Traces down**: firmware/sc/src/sc_can.c:SC_CAN_Init()
- **Verified by**: TC-SC-003
- **Verification method**: Unit test + PIL
- **Status**: draft

The SC software shall configure DCAN1 message acceptance filters to accept only the CAN IDs required for safety monitoring:

| Mailbox | CAN ID | Message | Purpose |
|---------|--------|---------|---------|
| 1 | 0x001 | E-stop | E-stop detection |
| 2 | 0x010 | CVC heartbeat | CVC alive monitoring |
| 3 | 0x011 | FZC heartbeat | FZC alive monitoring |
| 4 | 0x012 | RZC heartbeat | RZC alive monitoring |
| 5 | 0x100 | Vehicle state / torque request | Cross-plausibility input |
| 6 | 0x301 | Motor current | Cross-plausibility input |

All other CAN IDs shall be filtered out at the hardware level to reduce processing overhead and eliminate potential for spurious message processing. Each mailbox shall be configured with an exact-match acceptance mask (all ID bits checked).

---

### SWR-SC-003: CAN E2E Receive Validation

- **ASIL**: D
- **Traces up**: SYS-032, TSR-022, TSR-024, SSR-SC-001
- **Traces down**: firmware/sc/src/sc_e2e.c:SC_E2E_Check()
- **Verified by**: TC-SC-004, TC-SC-005
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The SC software shall validate all received safety-critical CAN messages using E2E check: (a) extract alive counter and Data ID from byte 0, (b) recompute CRC-8 (polynomial 0x1D, init 0xFF) over bytes 2-7 plus Data ID and compare with byte 1, (c) verify the alive counter has incremented by exactly 1 from the previously received value. The SC shall maintain independent E2E state (expected alive counters) for each monitored message type: CVC torque request (0x100), RZC motor current (0x301), CVC heartbeat (0x010), FZC heartbeat (0x011), RZC heartbeat (0x012). On E2E failure, the message shall be discarded and a per-message-type E2E failure counter incremented. If the E2E failure counter for any message type exceeds 3 consecutive failures, the SC shall treat it as equivalent to a heartbeat timeout for that ECU.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-001-003 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** CAN listen-only requirements SWR-SC-001 through SWR-SC-003 establish the SC's fundamental communication architecture. SWR-SC-001 (ASIL C) correctly specifies DCAN1 silent mode via TEST register bit 3, ensuring the SC can never transmit -- this is critical for the SC's independent safety monitor role. SWR-SC-002 (ASIL D) specifies exact-match acceptance filters for 6 CAN IDs with specific mailbox assignments, which eliminates spurious message processing and reduces the attack surface. SWR-SC-003 (ASIL D) provides a detailed E2E check implementation including byte-level field extraction, which is more explicit than the CVC/FZC/RZC E2E specs. The 3-consecutive-failure escalation to heartbeat timeout is a good bridging mechanism between E2E and heartbeat monitoring. The explicit "SC shall never call any CAN transmit function" in SWR-SC-001 is a strong design constraint that should be verified via static analysis. Traces to SYS-025/032 and TSR-022/024 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-001-003 -->

---

## 5. Heartbeat Monitoring Requirements

### SWR-SC-004: Per-ECU Heartbeat Timeout Counter

- **ASIL**: C
- **Traces up**: SYS-022, TSR-027, SSR-SC-003
- **Traces down**: firmware/sc/src/sc_heartbeat.c:SC_Heartbeat_Monitor()
- **Verified by**: TC-SC-006, TC-SC-007, TC-SC-008
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The SC software shall maintain three independent timeout counters, one per zone ECU (CVC, FZC, RZC). Each counter shall be reset to 0 upon reception of a valid heartbeat message (passing E2E check) from the corresponding ECU. The counters shall be incremented every 10 ms by the SC main loop. When any counter exceeds 15 (150 ms = 3 heartbeat periods), the SC shall declare a heartbeat timeout for that ECU.

---

### SWR-SC-005: Fault LED Activation on Heartbeat Timeout

- **ASIL**: QM
- **Traces up**: SYS-046, TSR-027, SSR-SC-004
- **Traces down**: firmware/sc/src/sc_heartbeat.c:SC_Heartbeat_FaultLED()
- **Verified by**: TC-SC-009
- **Verification method**: PIL + hardware test
- **Status**: draft

Upon heartbeat timeout detection for a specific ECU, the SC software shall drive the corresponding fault LED GPIO: GIO_A1 = HIGH for CVC timeout, GIO_A2 = HIGH for FZC timeout, GIO_A3 = HIGH for RZC timeout. The LED shall remain ON as long as the timeout condition persists. If the heartbeat resumes (counter resets), the LED shall turn OFF.

---

### SWR-SC-006: Heartbeat Timeout Confirmation and Kill Relay

- **ASIL**: D
- **Traces up**: SYS-022, SYS-024, TSR-028, SSR-SC-005
- **Traces down**: firmware/sc/src/sc_heartbeat.c:SC_Heartbeat_ConfirmAndRelay()
- **Verified by**: TC-SC-010, TC-SC-011, TC-SC-012
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

After detecting a heartbeat timeout (SWR-SC-004), the SC software shall start a confirmation timer of 50 ms. During the confirmation period, the SC shall continue monitoring for the missing heartbeat. If the heartbeat is not received by the end of the confirmation period (total elapsed: 150 ms timeout + 50 ms confirmation = 200 ms), the software shall call the kill relay de-energize function (SWR-SC-008). If a valid heartbeat is received during the confirmation period, the software shall cancel the confirmation timer and clear the timeout condition.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-004-006 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Heartbeat monitoring requirements SWR-SC-004 through SWR-SC-006 establish the SC's primary ECU health monitoring mechanism. SWR-SC-004 (ASIL C) specifies per-ECU timeout counters with a 150 ms threshold (3 missed heartbeats at 50 ms period), which is correct. SWR-SC-005 (QM) provides visual fault indication via dedicated per-ECU LEDs -- this is a good operator observability feature. SWR-SC-006 (ASIL D) adds a 50 ms confirmation period before relay de-energize, making the total detection-to-action time 200 ms. This confirmation timer is important for avoiding false positives from CAN bus transients while keeping the total reaction time well within the system FTTI. The cancellation of the confirmation timer on heartbeat recovery is correct behavior for transient bus errors. The escalation from ASIL C (detection) to ASIL D (relay action) correctly reflects the safety criticality of the kill relay control. Traces to SYS-022/024/046 and TSR-027/028 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-004-006 -->

---

## 6. Cross-Plausibility Requirements

### SWR-SC-007: Torque-Current Lookup Table

- **ASIL**: C
- **Traces up**: SYS-023, TSR-041, SSR-SC-010
- **Traces down**: firmware/sc/src/sc_plausibility.c:SC_Plausibility_ComputeExpected()
- **Verified by**: TC-SC-018, TC-SC-019
- **Verification method**: Unit test + SIL
- **Status**: draft

The SC software shall maintain a 16-entry const lookup table in flash mapping torque request percentage (0%, 7%, 13%, 20%, 27%, 33%, 40%, 47%, 53%, 60%, 67%, 73%, 80%, 87%, 93%, 100%) to expected motor current in milliamps. The table shall be calibrated during hardware integration testing (default values: linear mapping 0-25000 mA). For torque values between table entries, the SC shall use linear interpolation. The SC shall extract the torque request from the CVC state message (CAN ID 0x100, byte 4) and the actual current from the RZC current message (CAN ID 0x301, bytes 2-3).

---

### SWR-SC-008: Cross-Plausibility Debounce and Fault Detection

- **ASIL**: C
- **Traces up**: SYS-023, TSR-041, SSR-SC-011
- **Traces down**: firmware/sc/src/sc_plausibility.c:SC_Plausibility_Check()
- **Verified by**: TC-SC-020, TC-SC-021, TC-SC-022
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The SC software shall compare the expected current (from lookup, SWR-SC-007) with the actual current (from RZC CAN message) every 10 ms. If |expected - actual| > 20% of expected value (or > 2000 mA absolute when expected is near zero), the software shall increment a debounce counter. The counter resets when the difference is within threshold. When the counter reaches 5 (50 ms continuous fault), the software shall declare a cross-plausibility fault and invoke relay de-energize (SWR-SC-010).

---

### SWR-SC-009: Cross-Plausibility Fault Reaction

- **ASIL**: C
- **Traces up**: SYS-023, TSR-042, SSR-SC-012
- **Traces down**: firmware/sc/src/sc_plausibility.c:SC_Plausibility_FaultReaction()
- **Verified by**: TC-SC-023
- **Verification method**: SIL + hardware test
- **Status**: draft

Upon cross-plausibility fault detection, the SC software shall: (a) drive GIO_A4 HIGH (system fault LED), (b) call the kill relay de-energize function (SWR-SC-010), (c) set the de-energize latch flag. The fault LED shall remain ON permanently (until power cycle).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-007-009 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Cross-plausibility requirements SWR-SC-007 through SWR-SC-009 implement the SC's independent torque-current monitoring function. All three are correctly ASIL C. SWR-SC-007 specifies a 16-entry const lookup table for torque-to-current mapping with linear interpolation between entries -- the table approach is simple, deterministic, and appropriate for the ~400 LOC bare-metal design. SWR-SC-008 specifies a 20% relative threshold (or 2000 mA absolute near zero) with a 50 ms debounce -- the absolute threshold prevents division-by-zero issues at low torque and the debounce prevents false positives during motor transients. SWR-SC-009 correctly escalates to relay de-energize with a permanent fault LED (until power cycle). The cross-plausibility check compares data from two independent ECUs (CVC torque command vs RZC current measurement), providing true independent monitoring. One note: the lookup table values are listed as "default: linear mapping 0-25000 mA" and will need hardware calibration -- this is correctly captured in open item SWR-SC-O-001. Traces to SYS-023, TSR-041/042, and SSR-SC-010/011/012 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-007-009 -->

---

## 7. Kill Relay Control Requirements

### SWR-SC-010: Kill Relay GPIO Control

- **ASIL**: D
- **Traces up**: SYS-024, TSR-029, SSR-SC-006
- **Traces down**: firmware/sc/src/sc_relay.c:SC_Relay_Control()
- **Verified by**: TC-SC-013, TC-SC-014
- **Verification method**: PIL + hardware test
- **Status**: draft

The SC software shall control the kill relay via GIO_A0: (a) to energize (close relay): write GIO_A0 = HIGH, read back GIO_A0 data-in register to confirm HIGH, (b) to de-energize (open relay): write GIO_A0 = LOW, read back to confirm LOW. The GIO_A0 shall be initialized to LOW (relay open, safe state) during startup. The relay shall only be energized after successful completion of the startup self-test sequence (SWR-SC-019). The de-energize shall complete within 5 ms of the trigger condition. The software shall set a de-energize latch flag that, once set, prevents any code path from re-energizing the relay until the next power cycle.

---

### SWR-SC-011: Kill Relay De-energize Trigger Logic

- **ASIL**: D
- **Traces up**: SYS-024, TSR-030, SSR-SC-007
- **Traces down**: firmware/sc/src/sc_relay.c:SC_Relay_CheckTriggers()
- **Verified by**: TC-SC-015, TC-SC-016, TC-SC-017
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

The SC software shall de-energize the kill relay when any of the following conditions is confirmed: (a) heartbeat timeout confirmed for any zone ECU (SWR-SC-006), (b) cross-plausibility fault detected (SWR-SC-009), (c) startup self-test failure (SWR-SC-019), (d) runtime self-test failure (SWR-SC-020), (e) lockstep CPU ESM interrupt received (immediate, no confirmation), (f) GPIO readback mismatch on GIO_A0 (relay state disagrees with commanded state). The de-energize latch flag shall be set immediately and shall not be clearable by software (only by power cycle).

---

### SWR-SC-012: Kill Relay GPIO Readback Verification

- **ASIL**: D
- **Traces up**: SYS-024, TSR-029
- **Traces down**: firmware/sc/src/sc_relay.c:SC_Relay_VerifyState()
- **Verified by**: TC-SC-024, TC-SC-025
- **Verification method**: Unit test + PIL
- **Status**: draft

The SC software shall verify the kill relay GPIO state every 10 ms (once per main loop) by reading the GIO_A0 data-in register and comparing with the commanded state. If the readback does not match the commanded state for 2 consecutive checks (20 ms), the software shall declare a relay GPIO fault and invoke the de-energize sequence (SWR-SC-011 condition f). This detects GPIO driver failures, stuck-at faults, and wiring faults.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-010-012 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Kill relay control requirements SWR-SC-010 through SWR-SC-012 are all correctly ASIL D as they directly control the system's primary safety mechanism. SWR-SC-010 specifies GIO_A0 control with readback verification for both energize and de-energize, plus a de-energize latch that can only be cleared by power cycle -- this is the correct energize-to-run pattern where the safe state (relay open = motor de-energized) is maintained by default. The 5 ms de-energize timing requirement is well within the FTTI. SWR-SC-011 enumerates 6 trigger conditions (heartbeat timeout, cross-plausibility, startup failure, runtime failure, lockstep ESM, GPIO readback mismatch) which is comprehensive. The immediate lockstep ESM response (no confirmation delay) is correct since lockstep errors indicate fundamental CPU integrity loss. SWR-SC-012 specifies 10 ms GPIO readback verification with a 2-cycle debounce, detecting stuck-at and wiring faults. These three requirements together form a robust kill relay control architecture. Traces to SYS-024, TSR-029/030, and SSR-SC-006/007 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-010-012 -->

---

## 8. Fault LED Requirements

### SWR-SC-013: Fault LED Panel Management

- **ASIL**: QM
- **Traces up**: SYS-046, TSR-045, SSR-SC-013
- **Traces down**: firmware/sc/src/sc_led.c:SC_LED_Update()
- **Verified by**: TC-SC-026, TC-SC-027
- **Verification method**: PIL + hardware test
- **Status**: draft

The SC software shall update fault LED states every 10 ms based on the monitoring results:

| GPIO | LED | Off | Blink (500ms) | Steady On |
|------|-----|-----|---------------|-----------|
| GIO_A1 | CVC | CVC normal | CVC degraded (heartbeat shows degraded mode) | CVC heartbeat timeout |
| GIO_A2 | FZC | FZC normal | FZC degraded | FZC heartbeat timeout |
| GIO_A3 | RZC | RZC normal | RZC degraded | RZC heartbeat timeout |
| GIO_A4 | System | No fault | — | Cross-plausibility or self-test failure |

The blinking state shall be generated by toggling the GPIO with a 500 ms period software timer (25 main loop iterations on, 25 off). LED state changes shall occur within one main loop iteration (10 ms) of the triggering event.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-013 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Fault LED requirement SWR-SC-013 is correctly QM since the LEDs are operator-visible indicators with no safety function. The LED state table with 3 states per LED (Off / Blink 500 ms / Steady On) for 4 GPIOs provides clear visual feedback for fault diagnosis. The addition of a "degraded" blink state (when heartbeat shows degraded mode) is useful for distinguishing between degraded operation and complete heartbeat loss. The 10 ms LED update latency from triggering event is achievable within the main loop. The 500 ms blink period via software timer (25 iterations on/off at 10 ms) is a simple, deterministic approach consistent with the bare-metal ~400 LOC design philosophy. Traces to SYS-046, TSR-045, and SSR-SC-013 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-013 -->

---

## 9. Lockstep CPU Requirements

### SWR-SC-014: Lockstep ESM Configuration

- **ASIL**: D
- **Traces up**: SYS-026, TSR-030
- **Traces down**: firmware/sc/src/sc_esm.c:SC_ESM_Init()
- **Verified by**: TC-SC-028, TC-SC-029
- **Verification method**: Unit test + PIL + fault injection
- **Status**: draft

The SC software shall enable the TMS570LC43x Error Signaling Module (ESM) for lockstep CPU comparison error detection. The ESM shall be configured to: (a) enable ESM group 1 notification for lockstep compare error (channel 2), (b) register the ESM high-level interrupt handler, (c) configure the ESM to assert the error pin on lockstep compare error. On ESM interrupt for lockstep error, the handler shall immediately call the kill relay de-energize function (SWR-SC-010) without confirmation delay. The ESM configuration shall be verified at startup by reading back the ESM enable registers.

---

### SWR-SC-015: Lockstep ESM Interrupt Handler

- **ASIL**: D
- **Traces up**: SYS-026
- **Traces down**: firmware/sc/src/sc_esm.c:SC_ESM_HighLevelInterrupt()
- **Verified by**: TC-SC-030
- **Verification method**: Fault injection + PIL
- **Status**: draft

The SC ESM high-level interrupt handler shall: (a) read the ESM status register to identify the error source, (b) if lockstep compare error (group 1, channel 2): immediately write GIO_A0 = LOW (de-energize relay), set the de-energize latch, set GIO_A4 = HIGH (system fault LED), (c) clear the ESM error flag, (d) enter an infinite loop (the TPS3823 watchdog will reset the MCU since the main loop is halted). The handler shall execute in fewer than 100 clock cycles to ensure relay de-energize is immediate.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-014-015 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Lockstep CPU requirements SWR-SC-014 and SWR-SC-015 are correctly ASIL D. SWR-SC-014 specifies ESM group 1 channel 2 configuration for lockstep compare error detection with readback verification of ESM enable registers -- this leverages the TMS570's built-in dual-core lockstep hardware, which is one of the strongest processor fault detection mechanisms available. SWR-SC-015 specifies the ESM interrupt handler with immediate relay de-energize (no confirmation delay), system fault LED, ESM flag clear, and infinite loop (relying on TPS3823 for reset). The 100-clock-cycle execution budget for the handler is tight but achievable for the simple sequence (GPIO write + flag set + LED write + loop). The infinite loop approach is correct for a lockstep error since the CPU cannot be trusted after such a failure -- the external watchdog provides the recovery mechanism. Traces to SYS-026 and TSR-030 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-014-015 -->

---

## 10. Startup Self-Test Requirements

### SWR-SC-016: SC Startup Self-Test -- Lockstep CPU

- **ASIL**: D
- **Traces up**: SYS-026, TSR-050, SSR-SC-016
- **Traces down**: firmware/sc/src/sc_selftest.c:SC_SelfTest_Lockstep()
- **Verified by**: TC-SC-031
- **Verification method**: PIL + hardware test
- **Status**: draft

The SC software shall trigger the TMS570LC43x lockstep CPU built-in self-test via the ESM module at startup. The self-test shall: (a) trigger the CPU compare logic test, (b) wait for ESM to report the test result, (c) verify no lockstep error flag is set. If the lockstep self-test fails, the SC shall not energize the kill relay and shall blink GIO_A4 once per second (1 blink pattern = step 1 failure).

---

### SWR-SC-017: SC Startup Self-Test -- RAM PBIST

- **ASIL**: D
- **Traces up**: TSR-050, SSR-SC-016
- **Traces down**: firmware/sc/src/sc_selftest.c:SC_SelfTest_RAM()
- **Verified by**: TC-SC-032
- **Verification method**: PIL + hardware test
- **Status**: draft

The SC software shall execute the TMS570 Programmable Built-In Self-Test (PBIST) for RAM covering the full 256 KB at startup. The PBIST shall use March 13N algorithm for full stuck-at and coupling fault coverage. If PBIST fails, the SC shall not energize the kill relay and shall blink GIO_A4 twice per second (2 blink pattern = step 2 failure).

---

### SWR-SC-018: SC Startup Self-Test -- Flash CRC

- **ASIL**: C
- **Traces up**: TSR-050, SSR-SC-016
- **Traces down**: firmware/sc/src/sc_selftest.c:SC_SelfTest_FlashCRC()
- **Verified by**: TC-SC-033
- **Verification method**: Unit test + PIL
- **Status**: draft

The SC software shall compute CRC-32 over the application flash region (0x00000000 to end of application, size determined at link time) and compare with the reference CRC stored in the last flash sector. The CRC polynomial shall be 0x04C11DB7 (standard CRC-32). If the CRC does not match, the SC shall not energize the kill relay and shall blink GIO_A4 three times per second (3 blink pattern = step 3 failure).

---

### SWR-SC-019: SC Startup Self-Test Sequence

- **ASIL**: D
- **Traces up**: SYS-024, SYS-026, SYS-027, TSR-050, SSR-SC-016
- **Traces down**: firmware/sc/src/sc_selftest.c:SC_SelfTest_Startup()
- **Verified by**: TC-SC-034, TC-SC-035, TC-SC-036
- **Verification method**: Unit test + PIL + fault injection
- **Status**: draft

The SC software shall execute the following self-test sequence at power-on before energizing the kill relay:

1. **Lockstep CPU BIST**: Per SWR-SC-016.
2. **RAM PBIST**: Per SWR-SC-017.
3. **Flash CRC**: Per SWR-SC-018.
4. **CAN controller test**: Initialize DCAN1 in internal loopback mode, transmit a test frame with known payload (0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55), verify reception matches. Then switch to listen-only mode. Failure: blink GIO_A4 4 times (step 4).
5. **GPIO readback**: Drive GIO_A0 through GIO_A4 to known states (alternating HIGH/LOW) and read back via data-in registers. Verify each pin matches the commanded state. Failure: blink GIO_A4 5 times (step 5).
6. **Fault LED lamp test**: Illuminate all 4 fault LEDs (GIO_A1-A4) for 500 ms, then turn off. This is a visual confirmation to the operator.
7. **Watchdog test**: Toggle TPS3823 WDI pin and verify the RESET pin is de-asserted (read back a GPIO connected to the TPS3823 reset output). Failure: blink GIO_A4 7 times (step 7).

If all tests pass, energize the kill relay (GIO_A0 = HIGH) and verify readback. If any test fails, do NOT energize the relay, set GIO_A4 to the failure blink pattern, and halt (watchdog will eventually reset for retry on next power cycle).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-016-019 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Startup self-test requirements SWR-SC-016 through SWR-SC-019 establish the SC's comprehensive power-on validation. SWR-SC-016 (ASIL D) uses the TMS570's built-in lockstep CPU BIST. SWR-SC-017 (ASIL D) uses the TMS570's PBIST with March 13N algorithm for full RAM fault coverage -- this is a standard automotive-grade memory test. SWR-SC-018 (ASIL C) computes CRC-32 over application flash to detect flash corruption. The coordinating requirement SWR-SC-019 (ASIL D) sequences all 7 steps with unique blink patterns per failure step (1-7 blinks), providing visual diagnostic capability without any CAN communication. The fault LED lamp test (step 6, 500 ms all-on) is a good operator-visible confirmation. The watchdog-based retry on failure (halt and let TPS3823 reset) is correct since the SC has no recovery path from self-test failure. One note: step 6 is a lamp test (not a pass/fail test) but is included in the sequence numbering -- this is acceptable since it occurs after all diagnostic tests and before the watchdog test. Traces to SYS-024/026/027, TSR-050, and SSR-SC-016 are consistent across all four requirements.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-016-019 -->

---

## 11. Runtime Self-Test Requirements

### SWR-SC-020: SC Runtime Periodic Self-Test

- **ASIL**: D
- **Traces up**: TSR-051, SSR-SC-017
- **Traces down**: firmware/sc/src/sc_selftest.c:SC_SelfTest_Runtime()
- **Verified by**: TC-SC-037, TC-SC-038
- **Verification method**: Unit test + SIL
- **Status**: draft

The SC software shall execute a runtime self-test every 60 seconds:

1. **Flash CRC verification**: Recompute CRC-32 over application flash and compare with reference. To avoid blocking the main loop, compute 1/600th of the flash per main loop iteration (spread over 6 seconds at 10 ms/iteration). Maintain partial CRC state between iterations.
2. **RAM test**: Write/read/compare a 32-byte test pattern (0xAA/0x55 alternating) to a reserved RAM region. This is non-destructive to application data.
3. **CAN status check**: Read DCAN1 error status register. Verify not in bus-off or error passive state.
4. **GPIO readback of GIO_A0**: Verify relay GPIO matches commanded state per SWR-SC-012.

If any runtime self-test fails, invoke relay de-energize (SWR-SC-011 condition d) and set GIO_A4 = HIGH.

---

### SWR-SC-021: SC Stack Canary Check

- **ASIL**: D
- **Traces up**: TSR-031
- **Traces down**: firmware/sc/src/sc_selftest.c:SC_SelfTest_StackCanary()
- **Verified by**: TC-SC-039
- **Verification method**: Unit test + fault injection
- **Status**: draft

The SC software shall write a 32-bit canary value (0xDEADBEEF) at the bottom of the stack during initialization. Every main loop iteration, the software shall read the canary location and compare with the expected value. If the canary is corrupted (stack overflow), the software shall immediately de-energize the kill relay and halt. The stack canary check shall execute before the watchdog feed to ensure the watchdog is not fed if the stack is corrupted.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-020-021 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Runtime self-test requirements SWR-SC-020 and SWR-SC-021 are correctly ASIL D. SWR-SC-020 specifies a comprehensive 4-part runtime self-test every 60 seconds: flash CRC (spread over 6 seconds to avoid blocking), RAM pattern test, CAN status check, and GPIO readback. The flash CRC spreading strategy (1/600th per iteration) is well-designed for the cooperative main loop architecture -- it maintains the 10 ms loop timing while achieving full flash verification in 6 seconds. SWR-SC-021 specifies stack canary verification before watchdog feed, which is the correct ordering -- if the stack is corrupted, the watchdog is starved and forces a reset. The immediate relay de-energize on canary corruption (no debounce) is appropriate since stack overflow indicates memory corruption that could affect any safety function. Traces to TSR-031/051 and SSR-SC-017 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-020-021 -->

---

## 12. Watchdog Requirements

### SWR-SC-022: SC External Watchdog Feed

- **ASIL**: D
- **Traces up**: SYS-027, TSR-031, SSR-SC-008
- **Traces down**: firmware/sc/src/sc_watchdog.c:SC_Watchdog_Feed()
- **Verified by**: TC-SC-040
- **Verification method**: Unit test + fault injection + PIL
- **Status**: draft

The SC software shall toggle the TPS3823 WDI pin once per main loop iteration, conditioned on: (a) main loop complete (all monitoring functions executed -- heartbeat check, plausibility check, relay trigger evaluation, LED update, self-test increment), (b) RAM test pattern intact (32-byte 0xAA/0x55 at reserved address), (c) DCAN1 not in bus-off state, (d) lockstep ESM error flag not asserted, (e) stack canary intact. If any condition fails, the watchdog shall not be toggled, and the TPS3823 shall reset the MCU after the 1.6 second timeout.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-022 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Watchdog requirement SWR-SC-022 is correctly ASIL D. The SC watchdog has 5 conditions (vs 4 for the STM32 zone controllers) because it adds the lockstep ESM error flag check -- this is appropriate since the TMS570 has lockstep capability that the STM32s lack. The explicit enumeration of all monitoring functions in condition (a) ensures the watchdog proves that the entire monitoring pipeline executed, not just that the main loop ran. The 1.6 second TPS3823 timeout is specified, providing a concrete worst-case recovery time. The SC watchdog approach is consistent with the other ECUs while being adapted for the TMS570-specific hardware. Traces to SYS-027, TSR-031, and SSR-SC-008 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-022 -->

---

## 13. CAN Bus Loss Requirements

### SWR-SC-023: SC CAN Bus Loss Detection

- **ASIL**: C
- **Traces up**: SYS-034, TSR-038, SSR-SC-009
- **Traces down**: firmware/sc/src/sc_can.c:SC_CAN_MonitorBus()
- **Verified by**: TC-SC-041
- **Verification method**: SIL + fault injection
- **Status**: draft

The SC software shall detect CAN bus loss by monitoring the DCAN1 error status register and the message reception timestamp. If no CAN messages are received for 200 ms, the SC shall treat this as equivalent to all heartbeats timing out simultaneously and initiate the confirmation-and-relay sequence (SWR-SC-006) for all three zone ECUs. The SC shall also detect CAN bus-off via the DCAN error passive/bus-off status bits and suppress the watchdog feed if bus-off is detected.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-023 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** CAN bus loss requirement SWR-SC-023 is correctly ASIL C. The 200 ms silence threshold for treating CAN loss as a simultaneous heartbeat timeout for all ECUs is appropriate -- it is longer than individual heartbeat timeout (150 ms) but shorter than the confirmation-and-relay total (200 ms), ensuring CAN bus loss is detected before individual timeouts could cascade. The bus-off detection via DCAN error status registers and the watchdog feed suppression on bus-off provides a dual detection mechanism. The decision to suppress the watchdog on bus-off (rather than immediately de-energize) allows the TPS3823 to provide the recovery mechanism, which is consistent with the SC's approach to unrecoverable errors. Traces to SYS-034, TSR-038, and SSR-SC-009 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-023 -->

---

## 14. Backup Motor Cutoff Requirements

### SWR-SC-024: SC Backup Motor Cutoff via Kill Relay

- **ASIL**: D
- **Traces up**: TSR-049, SSR-SC-015
- **Traces down**: firmware/sc/src/sc_plausibility.c:SC_Plausibility_BackupCutoff()
- **Verified by**: TC-SC-042, TC-SC-043
- **Verification method**: SIL + hardware test
- **Status**: draft

The SC software shall monitor for the brake-fault-motor-cutoff scenario: if the FZC heartbeat fault bitmask indicates a brake fault (bit 1), and the RZC motor current (from CAN ID 0x301) remains above 1000 mA for 100 ms after the brake fault is reported, the SC shall conclude that the CVC-RZC motor cutoff chain has failed and shall de-energize the kill relay (SWR-SC-011). This provides a hardware-enforced backup for the software motor cutoff path.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-024 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Backup motor cutoff requirement SWR-SC-024 is correctly ASIL D. This requirement implements a critical safety concept: the SC independently monitors for the failure of the CVC-RZC software motor cutoff chain. The logic is well-specified: FZC brake fault (from heartbeat bitmask) AND RZC motor current still above 1000 mA for 100 ms means the software cutoff path has failed. The 1000 mA threshold provides margin above noise while detecting meaningful motor current. The 100 ms monitoring window gives the software cutoff chain time to act before the SC intervenes with the hardware kill relay. This is a textbook defense-in-depth pattern: software safety mechanism (CVC motor cutoff) backed up by independent hardware safety mechanism (SC kill relay). Traces to TSR-049 and SSR-SC-015 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-024 -->

---

## 15. Main Loop Requirements

### SWR-SC-025: SC Main Loop Structure and Timing

- **ASIL**: D
- **Traces up**: SYS-053, TSR-046, SSR-SC-014
- **Traces down**: firmware/sc/src/sc_main.c:SC_Main_Loop()
- **Verified by**: TC-SC-044, TC-SC-045
- **Verification method**: Analysis (WCET) + PIL timing measurement
- **Status**: draft

The SC main loop shall execute at a 10 ms period using a hardware timer interrupt to set a tick flag. Each iteration shall execute the following functions in order:

1. `SC_CAN_Receive()` -- Read and validate all pending CAN messages.
2. `SC_Heartbeat_Monitor()` -- Update heartbeat timeout counters.
3. `SC_Plausibility_Check()` -- Perform cross-plausibility comparison.
4. `SC_Relay_CheckTriggers()` -- Evaluate relay de-energize conditions.
5. `SC_LED_Update()` -- Update fault LED states.
6. `SC_SelfTest_Runtime()` -- Increment runtime self-test (1 step per iteration).
7. `SC_SelfTest_StackCanary()` -- Verify stack canary.
8. `SC_Watchdog_Feed()` -- Feed watchdog if all checks passed.

The total loop execution time shall not exceed 2 ms to maintain the 10 ms cycle with adequate margin. The SC shall use a hardware timer to measure loop execution time. If any iteration exceeds 5 ms, a loop overrun shall be flagged internally (no DTC system on SC -- the overrun flag suppresses the watchdog feed, causing a reset).

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-025 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** Main loop requirement SWR-SC-025 is correctly ASIL D. The 8-step main loop structure is precisely sequenced: CAN receive, heartbeat monitor, plausibility check, relay trigger evaluation, LED update, runtime self-test, stack canary check, and watchdog feed. This ordering is critical -- the watchdog feed is last and only executes if all preceding steps complete. The 2 ms WCET budget within a 10 ms period provides 80% margin, which is conservative and appropriate for ASIL D. The 5 ms overrun detection with watchdog suppression provides a second timing safety net. The fact that the SC has no DTC system (bare-metal ~400 LOC) is acknowledged with the correct recovery mechanism: overrun flag suppresses watchdog feed, causing TPS3823 reset. The hardware timer for WCET measurement enables PIL timing verification. Traces to SYS-053, TSR-046, and SSR-SC-014 are consistent.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-025 -->

---

## 16. SC Initialization Requirements

### SWR-SC-026: SC System Initialization

- **ASIL**: D
- **Traces up**: SYS-024, SYS-025, SYS-026, SYS-027
- **Traces down**: firmware/sc/src/sc_main.c:SC_Init()
- **Verified by**: TC-SC-046
- **Verification method**: PIL
- **Status**: draft

The SC software shall execute the following initialization sequence at power-on:

1. Initialize system clocks and peripheral clocks.
2. Configure GIO: A0 as output (LOW = relay safe), A1-A4 as outputs (LOW = LEDs off).
3. Configure the 10 ms tick timer (RTI module or GIO timer).
4. Configure the loop execution timer for WCET measurement.
5. Initialize stack canary (0xDEADBEEF at stack bottom).
6. Initialize RAM test pattern (32-byte 0xAA/0x55 at reserved address).
7. Execute startup self-test sequence (SWR-SC-019).
8. If self-test passes: energize kill relay, enter main loop.
9. If self-test fails: blink failure pattern on GIO_A4, halt (watchdog resets).

The total initialization time (including self-test) shall not exceed 5 seconds. The watchdog must be fed during initialization to prevent premature reset.

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-026 -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** SC initialization requirement SWR-SC-026 is correctly ASIL D. The 9-step initialization sequence covers clock setup, GPIO configuration, timer initialization, stack canary, RAM test pattern, startup self-test, and conditional relay energize. The explicit initialization of GIO_A0 to LOW (relay safe state) at step 2 is correct -- the relay starts de-energized and only energizes after successful self-test. The 5-second initialization budget (including self-test) is reasonable for the TMS570 PBIST and lockstep BIST. The requirement to feed the watchdog during initialization prevents premature reset during the potentially long self-test sequence. The conditional relay energize (step 8, only if self-test passes) with fallback to blink pattern and halt (step 9) is the correct fail-safe approach. Traces to SYS-024/025/026/027 cover all major SC subsystems initialized in this sequence.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-026 -->

---

## 17. CAN Hardening Requirements

> Added: 2026-03-07 — see plan-sc-can-hardening.md. Addresses GAP-1 (no SC TX), GAP-2 (no
> diagnostic message), GAP-3 (heartbeat content not validated). SWR-SC-029 supersedes the
> silent-mode constraint in SWR-SC-001 (see rationale in plan).

### SWR-SC-027: Heartbeat OperatingMode Content Validation

- **ASIL**: C
- **Traces up**: SYS-022, TSR-027, SSR-SC-003
- **Traces down**: firmware/sc/src/sc_heartbeat.c:SC_Heartbeat_ValidateContent()
- **Verified by**: TC-SC-047
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

For each zone ECU heartbeat that passes E2E check (SWR-SC-003), the SC software shall extract
the `OperatingMode` field (bits [3:0] of byte 3) and compare with the last observed mode. The SC
shall maintain a per-ECU `stuck_degraded_cnt` counter that increments each main loop iteration
when the ECU's OperatingMode is DEGRADED (2) or LIMP (3). The counter resets when the mode
transitions back to RUN (1). If `stuck_degraded_cnt` exceeds 100 (5 seconds at 50 ms heartbeat
period), the SC shall set the `content_fault` flag for that ECU, activate the corresponding fault
LED (per SWR-SC-005 pattern), and invoke relay de-energize via SWR-SC-011. The threshold of 5
seconds is intentionally longer than the heartbeat-timeout relay trigger (200 ms) so that
legitimate degraded recovery is not penalized.

---

### SWR-SC-028: Heartbeat FaultStatus Escalation

- **ASIL**: C
- **Traces up**: SYS-022, TSR-027, SSR-SC-003
- **Traces down**: firmware/sc/src/sc_heartbeat.c:SC_Heartbeat_ValidateContent()
- **Verified by**: TC-SC-048
- **Verification method**: Unit test + SIL + fault injection
- **Status**: draft

For each zone ECU heartbeat that passes E2E check, the SC software shall inspect the
`FaultStatus` bitmask (bits [7:4] of byte 3). The SC shall maintain a per-ECU
`fault_escalate_cnt` counter that increments each main loop iteration when 2 or more FaultStatus
bits are simultaneously set. The counter resets when fewer than 2 bits are set. If
`fault_escalate_cnt` exceeds 20 (1 second at 50 ms heartbeat period), the SC shall set the
`content_fault` flag for that ECU and add `SC_FAULT_CONTENT` to `sc_fault_flags`. The content
fault shall be reflected in the SC_Status broadcast (SWR-SC-030) but shall NOT by itself trigger
relay de-energize — the relay is triggered only if the `stuck_degraded_cnt` threshold of
SWR-SC-027 is also exceeded. This two-tier design allows the operator to observe a
multi-fault-flag condition before SC escalates to relay action.

---

### SWR-SC-029: SC DCAN1 TX Configuration for Monitoring

- **ASIL**: C
- **Traces up**: plan-sc-can-hardening.md §2 (Architecture Decision C)
- **Traces down**: firmware/sc/src/sc_can.c:SC_CAN_Init()
- **Verified by**: TC-SC-049
- **Verification method**: Static analysis + PIL
- **Status**: draft

The SC software shall configure DCAN1 in **normal (non-silent) mode** — superseding the
listen-only constraint of SWR-SC-001. The following firmware enforcement applies:
(a) only mailbox 7 shall be configured for TX (direction=TX, CAN ID 0x013, DLC 4),
(b) mailboxes 1–6 shall remain RX-only (direction=RX),
(c) no `canTransmit` call shall appear outside `sc_monitoring.c:SC_Monitoring_Update()` —
enforced via static analysis (cppcheck custom rule `sc-tx-isolation`).

Rationale: CAN ID 0x013 (SC_Status) is a read-only diagnostic broadcast. It carries no control
signal and zone ECUs are not required to act on it (ASIL QM for receivers). This change enables
SC health visibility (GAP-1, GAP-2) while maintaining the invariant that SC never transmits
any control frame.

Production note: Upgrade to DCAN2 (dedicated transceiver, hardware-enforced TX isolation) before
series production.

---

### SWR-SC-030: SC Status Message Transmission

- **ASIL**: C
- **Traces up**: plan-sc-can-hardening.md §3
- **Traces down**: firmware/sc/src/sc_monitoring.c:SC_Monitoring_Update()
- **Verified by**: TC-SC-050
- **Verification method**: SIL + PIL + integration test
- **Status**: draft

The SC software shall transmit a SC_Status frame (CAN ID 0x013, DLC 4) every 500 ms
(50 main loop iterations at 10 ms). The frame payload shall be:

| Byte | Bits | Signal | Value |
|------|------|--------|-------|
| 0 | [7:0] | SC_AliveCounter | 8-bit wrapping counter, increments each TX |
| 1 | [7:0] | SC_CRC8 | CRC-8 (poly 0x1D, init 0xFF) over bytes 0, 2, 3 |
| 2 | [3:0] | SC_Mode | SC_MODE_INIT=0, MONITORING=1, FAULT_DETECTED=2, SAFE_STOP=3 |
| 2 | [7:4] | SC_FaultFlags | bit0=CVC_HB, bit1=FZC_HB, bit2=RZC_HB, bit3=PLAUS |
| 3 | [2:0] | ECU_Health | bit0=CVC_ok, bit1=FZC_ok, bit2=RZC_ok |
| 3 | [6:3] | FaultReason | bit0=HB_timeout, bit1=plaus, bit2=selftest, bit3=content |
| 3 | [7] | RelayState | 1=energized, 0=de-energized |

The SC shall continue transmitting SC_Status in FAULT_DETECTED and SAFE_STOP modes so that
the fault reason is available to the gateway and cloud logger after the relay de-energizes.
The SC_AliveCounter shall allow ICU and gateway to detect if the SC itself has halted.

Note on E2E DataID: all 16 DataIDs (0x00–0x0F) are allocated in the CAN matrix. SC_Status
uses a simplified E2E (alive counter + CRC without DataID mixing), sufficient for ASIL C
since there is exactly one SC transmitter and the ICU/gateway receivers are QM.

---

## 18. Traceability Summary

### 17.1 SWR to Upstream Mapping

| SWR-SC | SYS | TSR | SSR-SC |
|--------|-----|-----|--------|
| SWR-SC-001 | SYS-025 | TSR-024 | SSR-SC-001 |
| SWR-SC-002 | SYS-025 | TSR-024 | SSR-SC-002 |
| SWR-SC-003 | SYS-032 | TSR-022, TSR-024 | SSR-SC-001 |
| SWR-SC-004 | SYS-022 | TSR-027 | SSR-SC-003 |
| SWR-SC-005 | SYS-046 | TSR-027 | SSR-SC-004 |
| SWR-SC-006 | SYS-022, SYS-024 | TSR-028 | SSR-SC-005 |
| SWR-SC-007 | SYS-023 | TSR-041 | SSR-SC-010 |
| SWR-SC-008 | SYS-023 | TSR-041 | SSR-SC-011 |
| SWR-SC-009 | SYS-023 | TSR-042 | SSR-SC-012 |
| SWR-SC-010 | SYS-024 | TSR-029 | SSR-SC-006 |
| SWR-SC-011 | SYS-024 | TSR-030 | SSR-SC-007 |
| SWR-SC-012 | SYS-024 | TSR-029 | — |
| SWR-SC-013 | SYS-046 | TSR-045 | SSR-SC-013 |
| SWR-SC-014 | SYS-026 | TSR-030 | — |
| SWR-SC-015 | SYS-026 | — | — |
| SWR-SC-016 | SYS-026 | TSR-050 | SSR-SC-016 |
| SWR-SC-017 | — | TSR-050 | SSR-SC-016 |
| SWR-SC-018 | — | TSR-050 | SSR-SC-016 |
| SWR-SC-019 | SYS-024, SYS-026, SYS-027 | TSR-050 | SSR-SC-016 |
| SWR-SC-020 | — | TSR-051 | SSR-SC-017 |
| SWR-SC-021 | — | TSR-031 | — |
| SWR-SC-022 | SYS-027 | TSR-031 | SSR-SC-008 |
| SWR-SC-023 | SYS-034 | TSR-038 | SSR-SC-009 |
| SWR-SC-024 | — | TSR-049 | SSR-SC-015 |
| SWR-SC-025 | SYS-053 | TSR-046 | SSR-SC-014 |
| SWR-SC-026 | SYS-024, SYS-025, SYS-026, SYS-027 | — | — |
| SWR-SC-027 | SYS-022 | TSR-027 | SSR-SC-003 |
| SWR-SC-028 | SYS-022 | TSR-027 | SSR-SC-003 |
| SWR-SC-029 | — | — | — |
| SWR-SC-030 | — | — | — |

### 18.2 ASIL Distribution

| ASIL | Count | SWR IDs |
|------|-------|---------|
| D | 14 | SWR-SC-002, 003, 006, 010, 011, 012, 014, 015, 016, 017, 019, 021, 022, 025, 026 |
| C | 7 | SWR-SC-001, 004, 007, 008, 009, 018, 023, 024 |
| QM | 5 | SWR-SC-005, 013, 020 |
| **Total** | **26** | |

### 18.3 Verification Method Summary

| Method | Count |
|--------|-------|
| Unit test | 16 |
| SIL | 10 |
| PIL | 12 |
| Hardware test | 6 |
| Fault injection | 8 |
| Analysis (WCET) | 1 |

<!-- HITL-LOCK START:COMMENT-BLOCK-SWR-SC-TRACE -->
**HITL Review (An Dao) — Reviewed: 2026-02-27:** The traceability summary is comprehensive with all 26 requirements mapped to SYS, TSR, and SSR-SC upstream traces. The ASIL distribution shows 14 ASIL D, 7 ASIL C, and 5 QM -- however, the ASIL D count in the table lists 15 IDs (SWR-SC-002, 003, 006, 010, 011, 012, 014, 015, 016, 017, 019, 021, 022, 025, 026) which is 15, not 14. The ASIL C count lists 8 IDs (SWR-SC-001, 004, 007, 008, 009, 018, 023, 024) which is 8, not 7. The QM count lists 3 IDs (SWR-SC-005, 013, 020) which is 3, not 5. The total should be 15+8+3=26 which matches, but the individual counts are incorrect -- 14+7+5=26 does not match the listed IDs. This discrepancy should be corrected. Notably, SWR-SC-020 (Runtime Self-Test) is listed as QM but traces to TSR-051 and SSR-SC-017, which suggests it should carry an ASIL rating -- this should be reviewed. Verification method coverage is strong with 12 PIL tests (highest among all ECUs), reflecting the SC's critical safety role requiring extensive hardware-level validation.
<!-- HITL-LOCK END:COMMENT-BLOCK-SWR-SC-TRACE -->

---

## 19. Open Items and Assumptions

### 19.1 Assumptions

| ID | Assumption | Impact |
|----|-----------|--------|
| SWR-SC-A-001 | SC runs bare-metal with cooperative main loop (no RTOS) | Timing analysis assumes no preemption |
| SWR-SC-A-002 | TMS570LC43x DCAN1 silent mode works correctly (TEST reg bit 3) | CAN listen-only depends on correct hardware behavior |
| SWR-SC-A-003 | HALCoGen v04.07.01 DCAN1 is used (not DCAN4 due to known mailbox bug) | SWR-SC-001 initialization |
| SWR-SC-A-004 | Kill relay dropout time is less than 10 ms (electromechanical) | FTTI analysis in TSR-046 |
| SWR-SC-A-005 | Total SC code size is approximately 400 LOC | Finishability constraint |

### 19.2 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| SWR-SC-O-001 | Calibrate torque-to-current lookup table on target hardware | Integration Engineer | Integration |
| SWR-SC-O-002 | Verify DCAN1 silent mode behavior on TMS570LC43x LaunchPad | Integration Engineer | PIL |
| SWR-SC-O-003 | Measure main loop WCET on target hardware | SW Engineer | SWE.3 |
| SWR-SC-O-004 | Verify PBIST execution time does not exceed 2 seconds | SW Engineer | PIL |
| SWR-SC-O-005 | Verify ESM lockstep self-test does not cause unintended reset | SW Engineer | PIL |
| SWR-SC-O-006 | GAP-4 deferred: actuator command vs feedback plausibility (Steering_Status 0x200, Brake_Status 0x201) | SW Engineer | Post-HIL |
| SWR-SC-O-007 | LOC target increase from ~400 to ~500 due to hardening additions — verify WCET stays within 2 ms | SW Engineer | SWE.3 |
| SWR-SC-O-008 | Production: upgrade DCAN1 TX to DCAN2 (dedicated transceiver) per SWR-SC-029 production note | HW Engineer | Production |

---

## 20. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 0.1 | 2026-02-21 | System | Initial stub |
| 1.0 | 2026-02-21 | System | Complete SWR specification: 26 requirements (SWR-SC-001 to SWR-SC-026), full traceability |

