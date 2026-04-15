---
document_id: HARA-SOVD-PRELIM
title: "Preliminary HARA Delta - SOVD Diagnostics"
version: "0.1"
status: draft
iso_26262_part: 3
iso_26262_clause: "7"
aspice_process: SYS.1
date: 2026-04-14
---

# Preliminary HARA Delta - SOVD Diagnostics

## 1. Purpose

This document is a preliminary delta to `docs/safety/concept/hara.md` for adding remotely
reachable diagnostic services through the planned SOVD path.

Scope of this delta:

- `0x19` ReadDTCInformation
- `0x14` ClearDiagnosticInformation
- `0x31` RoutineControl for at least:
  - motor self-test
  - brake check

This is not the full Phase 1 safety review. It is a Phase 0 hazard scan so the embedded team can
start implementation with the main safety questions already visible.

## 2. Assumptions used for this delta

- Existing operational situations `OS-1` through `OS-7` from `docs/safety/concept/hara.md`
  remain the baseline.
- SOVD can reach the ECU diagnostic surface without a technician being physically attached to the
  CAN bus.
- Remote invocation increases accessibility and misuse potential even if the UDS handler logic is
  otherwise unchanged.
- Generic BSW DCM/DEM in this checkout do not yet provide full audit, filtering, or selective
  clear support.

## 3. Safety-relevant change summary

The new safety concern is not "UDS exists" by itself; the new concern is that diagnostic actions
which were previously local and tool-driven become remotely reachable and easier to invoke outside
the intended workshop context.

Safety-relevant deltas:

- `0x19` can influence service decisions if it exposes incomplete or stale fault state.
- `0x14` can erase evidence of an active or recently confirmed safety fault.
- `0x31` can intentionally move or energize actuators while the item is in a non-driving
  diagnostic context.

## 4. Preliminary hazardous events

| Delta ID | Added capability | Operational situation | Hazardous event | S | E | C | Preliminary ASIL | Notes |
|----------|------------------|-----------------------|-----------------|---|---|---|------------------|-------|
| SOVD-HE-01 | `0x31` motor self-test | `OS-6` Diagnostic Mode, `OS-2` Stationary | Remote routine starts torque-producing motor motion while a technician or bystander is near the vehicle | S2 | E2 | C2 | ASIL A | Highest concern if routine can run with wheels on ground or no service inhibit |
| SOVD-HE-02 | `0x31` brake check | `OS-6` Diagnostic Mode, `OS-2` Stationary | Remote routine applies or releases brake unexpectedly, causing pinch, roll, or startle hazard | S2 | E2 | C2 | ASIL A | Risk increases if brake check is allowed outside parked and secured state |
| SOVD-HE-03 | `0x14` ClearDiagnosticInformation | `OS-1`, `OS-4`, `OS-6` | Safety-relevant DTCs are cleared before repair or root cause capture, enabling unsafe return to service | S3 | E2 | C3 | ASIL B | Current generic DEM only supports global clear, which amplifies the scope of the action |
| SOVD-HE-04 | `0x19` ReadDTCInformation | `OS-6` leading into `OS-1` or `OS-4` | Technician receives incomplete or stale fault picture, misses an active safety issue, and releases the vehicle | S2 | E2 | C3 | QM to ASIL A | Indirect safety hazard; depends on service workflow and diagnostic completeness |
| SOVD-HE-05 | Any remotely reachable diagnostic control path | `OS-1`, `OS-3`, `OS-4` | Session, authorization, or gateway defect allows `0x14` or `0x31` while vehicle is in motion | S3 | E2 | C3 | ASIL B | Should be treated as the main misuse case for Phase 1 architecture review |

## 5. Preliminary controls to carry into Phase 1

- Gate `0x14` and `0x31` behind authenticated security access and an explicit service session.
- Define a shared platform-status DID for routine gating:
  - `0xF018 bit0` `stationary`
  - `0xF018 bit1` `brake_secured`
  - `0xF018 bit2` `service_session`
  - `0xF018 bit3` `service_mode_enabled`
- On the current platform, derive `brake_secured` from brake position `>= 90 %` because there is
  no dedicated park-brake signal in this checkout.
- Add hard precondition checks inside each handler:
  - vehicle speed zero
  - brake secured
  - service session active
  - actuator command path inhibited
- Keep routine control fail-safe:
  - bounded execution time
  - watchdog-aware abort path
  - no persistent actuator enable on transport loss
- Keep the initial `0x31` ECU handlers non-actuating until the full HARA delta is signed off.
- Make DTC clear auditable:
  - who cleared
  - when cleared
  - pre-clear snapshot
  - post-clear result
- Treat `0x19` as safety-relevant diagnostic evidence, not just convenience telemetry.

## 6. Phase 1 open questions for the full safety review

- What ECU is the authoritative SOVD entry point, and how are service permissions partitioned by ECU?
- Are the `stationary < 50 RPM` and `brake_secured >= 90 %` thresholds acceptable as the platform
  interpretation of `SR-3.1` and `SR-3.2`, or do they need calibration/safety tightening?
- Can any future `0x31` routine physically actuate motor or brake hardware with the vehicle not mechanically secured?
- Should `0x14` be limited to a small DTC group, or blocked entirely, until selective clear exists in DEM?
- How will the system prove that `0x19` output is complete when generic DEM currently lacks filter APIs,
  operation-cycle semantics, and freeze-frame support?
- What logging and cybersecurity evidence will exist for remote diagnostic actions that change item state?
- Does the safety concept require a second independent inhibit from `SC` before any remote routine can move hardware?

## 7. Engineering takeaway

The highest-priority delta is not `0x19`; it is the combination of remote reachability plus
state-changing services `0x14` and `0x31`. Phase 1 should therefore treat routine preconditions,
authorization, and clear-auditability as first-class safety requirements, not as later hardening.
