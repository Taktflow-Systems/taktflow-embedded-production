---
plan_id: plan-sc-can-hardening
title: "SC CAN Hardening — Heartbeat Content Validation + SC Status Broadcast"
version: "1.0"
status: approved
date: 2026-03-07
author: System
---

# SC CAN Hardening Plan

## 1. Background and Gap Analysis

Research comparing real automotive safety controller communication patterns against our SC
(SWR-SC) revealed four gaps:

| Gap | Severity | Description |
|-----|----------|-------------|
| GAP-1 | High | SC never transmits — if SC firmware hangs, zone ECUs cannot detect it |
| GAP-2 | High (diagnostic) | No SC status/fault message — post-incident analysis is blind |
| GAP-3 | Medium | SC checks heartbeat aliveness (E2E) but never validates OperatingMode or FaultStatus payload content |
| GAP-4 | Low | SC does not cross-check actuator command vs. feedback (Steering_Status, Brake_Status) |

This plan addresses GAP-1, GAP-2, and GAP-3. GAP-4 is deferred (see section 6).

---

## 2. Architecture Decision: SC TX

### Problem
SWR-SC-001 specifies DCAN1 in silent mode — SC never transmits. This prevents:
- Zone ECUs from detecting SC failure
- Any post-fault diagnostic data reaching gateway/cloud

### Options Considered

| Option | Description | Risk | Effort |
|--------|-------------|------|--------|
| A | Keep listen-only; add relay-state GPIO input to zone ECUs | Zone ECU changes, relay contact wiring change | High |
| B | Add DCAN2 (separate transceiver, same bus) for SC TX only | Extra hardware (transceiver + wiring) | Medium |
| **C** | **DCAN1 normal mode; firmware-enforced TX on mailbox 7 only** | Software discipline only | Low |

### Decision: Option C

DCAN1 operates in normal (non-silent) mode. Firmware constraint: **only mailbox 7**
(CAN ID 0x013, SC_Status) is initialized as TX. All other mailboxes (1–6) are RX-only.
Static analysis rule: no `canTransmit` call outside `sc_monitoring.c`.

Rationale:
- SC_Status is a read-only diagnostic frame. It carries no control signal.
- Zone ECUs are NOT required to act on SC_Status (ASIL QM for receivers).
- The SC's safety function (kill relay) does not depend on SC_Status TX.
- Simpler than Option B for the bench/prototype phase.

**Production recommendation**: Upgrade to Option B (DCAN2 + dedicated transceiver)
for hardware-enforced separation. Record as ADR-XXX.

SWR-SC-001 is extended by SWR-SC-029 which supersedes the silent-mode constraint.

---

## 3. New CAN Message: SC_Status (0x013)

| Field | Value |
|-------|-------|
| CAN ID | 0x013 |
| Sender | SC |
| Receivers | ALL (ICU, gateway — passive monitoring) |
| DLC | 4 |
| Cycle | 500 ms |
| E2E | Simplified (alive counter + CRC-8; no DataID mixing — all 16 DataIDs 0x00–0x0F are allocated) |
| ASIL | C |

### Payload

| Byte | Bits | Signal | Description |
|------|------|--------|-------------|
| 0 | [7:0] | SC_AliveCounter | 8-bit wrapping counter, increments each TX |
| 1 | [7:0] | SC_CRC8 | CRC-8 (poly 0x1D) over bytes 0, 2, 3 |
| 2 | [3:0] | SC_Mode | 0=INIT,1=MONITORING,2=FAULT_DETECTED,3=SAFE_STOP |
| 2 | [7:4] | SC_FaultFlags | bit0=CVC_HB,bit1=FZC_HB,bit2=RZC_HB,bit3=PLAUS |
| 3 | [2:0] | ECU_Health | bit0=CVC_ok,bit1=FZC_ok,bit2=RZC_ok |
| 3 | [6:3] | FaultReason | bit0=HB_timeout,bit1=plaus,bit2=selftest,bit3=content |
| 3 | [7] | RelayState | 1=energized, 0=de-energized |

---

## 4. New Requirements

| SWR | Title | ASIL | Gap |
|-----|-------|------|-----|
| SWR-SC-027 | Heartbeat OperatingMode Content Check | C | GAP-3 |
| SWR-SC-028 | Heartbeat FaultStatus Escalation | C | GAP-3 |
| SWR-SC-029 | SC DCAN1 TX Configuration for Monitoring | C | GAP-1/2 |
| SWR-SC-030 | SC Status Message Transmission | C | GAP-1/2 |

Details in SWR-SC.md sections 17 and 18.

---

## 5. Implementation Phases

### Phase 1 — GAP-3: Heartbeat Content Validation — DONE

- [x] SWR-SC-027 and SWR-SC-028 in SWR-SC.md
- [x] sc_heartbeat.c: SC_Heartbeat_ValidateContent(), SC_Heartbeat_IsContentFault()
- [x] TC-SC-047 (stuck-degraded threshold), TC-SC-048 (fault escalation) — 26/26 pass

**DONE criteria met**: sc_heartbeat.c compiles, unit tests pass in SIL.

### Phase 2 — GAP-1/2: SC Status Broadcast — DONE

- [x] SWR-SC-029, SWR-SC-030 in SWR-SC.md
- [x] CAN matrix updated: 0x013 SC_Status
- [x] dcan1_transmit() in sc_hw_tms570.c (DCAN IF1 mailbox 7 TX) and sc_hw_posix.c (SocketCAN)
- [x] SC_CAN_TransmitStatus() in sc_can.c; mailbox 7 configured TX-only in dcan1_setup_mailboxes()
- [x] TC-SC-049 (normal mode), TC-SC-050 (TransmitStatus TX) — 20/20 pass
- [ ] Integration test: 0x013 at 500 ms ± 50 ms on vcan0 — deferred to hardware PIL bring-up

**DONE criteria partially met**: SC_Status TX path implemented and unit-tested. PIL integration deferred.

### Phase 3 — Remaining SC firmware skeleton — DONE

- [x] All SC source files present: sc_relay.c, sc_plausibility.c, sc_selftest.c, sc_watchdog.c, sc_led.c, sc_esm.c, sc_main.c, sc_e2e.c, sc_heartbeat.c, sc_can.c, sc_monitoring.c
- [x] 9/10 test files compile and pass; test_sc_relay_asild has a pre-existing static-access issue (tracked separately)
- [x] LOC within target

---

## 6. Deferred: GAP-4 (Actuator Feedback Plausibility)

The SC does not independently cross-check steering command vs. Steering_Status (0x200) or
brake command vs. Brake_Status (0x201). This is partially covered by FZC's internal
PWM-mismatch detection (SWR-SC-024 for brake fault). Full actuator plausibility would
require SC to listen to 2 additional CAN IDs (0x200, 0x201) and maintain commanded vs.
actual tracking.

Decision: Defer to post-HIL. This is a QM improvement on top of existing ASIL C coverage.
Track as open item SWR-SC-O-006.

---

## 7. Risks

| Risk | Mitigation |
|------|-----------|
| SC TX corrupts bus during SC fault | SC_Status TX mailbox disabled if de-energize latch is set (SAFE_STOP mode still TXs last frame, then stops) |
| Stuck-degraded threshold too aggressive (false positives) | 5-second threshold is conservative; adjustable via constant SC_STUCK_DEGRADED_MAX |
| LOC exceeds 400 target | Monitor during implementation; architecture is simple enough to trim if needed |

---

## 8. Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-03-07 | System | Initial plan |
