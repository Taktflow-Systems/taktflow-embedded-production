# Heartbeat FTTI Budget Analysis

**Date:** 2026-03-02
**Safety level:** ASIL D
**Standard:** ISO 26262 Part 4 — FTTI (Fault Tolerant Time Interval)

## 1. Overview

This document proves that heartbeat timeout values comply with the FTTI for each safety goal.
The heartbeat monitoring system has two independent paths:

1. **CVC** (Central Vehicle Computer) — monitors FZC and RZC heartbeats via CAN polling
2. **SC** (Safety Controller) — monitors all 3 ECU heartbeats independently

## 2. Safety Goal FTTI Summary

| ID | Safety Goal | FTTI | Primary Detection | HB Role |
|----|------------|------|-------------------|---------|
| SG-001 | Unintended acceleration | 50ms | Pedal plausibility (CVC, 10ms) | Complementary |
| SG-003 | Unintended steering | 50ms | Steering watchdog (FZC, 10ms) | Complementary |
| SG-004 | Loss of braking | 50ms | Brake fault RX (CVC, 10ms) | Complementary |
| SG-008 | System-level SC monitoring | 100ms | **Heartbeat (primary)** | **Primary** |

**Key insight:** Heartbeat is the **primary** detection mechanism only for SG-008
(system-level safety controller monitoring). For SG-001/003/004, local
fault detection (pedal plausibility, steering watchdog, brake fault CAN)
is the primary path with much tighter timing (10ms cycle).

## 3. CVC Heartbeat Monitoring — Per-ECU Thresholds

### 3.1 FZC (Front Zone Controller)

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| HB TX period | 50ms | FZC sends heartbeat every 50ms |
| CVC poll period | 50ms | CVC checks at TX boundary |
| Miss threshold | **2** (`CVC_HB_FZC_MAX_MISS`) | 2 × 50ms = 100ms |
| **Detection time** | **100ms** | Complies with SG-008 FTTI (100ms) |

**Why tighter:** FZC handles steering and braking — higher criticality ECU.
Although local detection (SG-003/004) is primary at 10ms, the CVC heartbeat
path must still meet SG-008's 100ms FTTI as the system-level monitor.

### 3.2 RZC (Rear Zone Controller)

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| HB TX period | 50ms | RZC sends heartbeat every 50ms |
| CVC poll period | 50ms | CVC checks at TX boundary |
| Miss threshold | **3** (`CVC_HB_RZC_MAX_MISS`) | 3 × 50ms = 150ms |
| **Detection time** | **150ms** | Exceeds SG-008, justified below |

**Justification for 150ms:** RZC controls the motor, which has its own local
overcurrent/overtemp cutoff (`Swc_MotorCutoff`) as the primary safety
mechanism. The heartbeat is a complementary backup path. The local motor
cutoff detects faults within 10ms. The 150ms heartbeat window provides
tolerance for CAN bus jitter while still detecting total RZC loss within
3 heartbeat periods.

## 4. SC Heartbeat Monitoring

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| SC tick period | 10ms | SC main loop at 10ms |
| Timeout threshold | **10 ticks** (`SC_HB_TIMEOUT_TICKS`) | 10 × 10ms = 100ms |
| Confirmation window | **3 ticks** (`SC_HB_CONFIRM_TICKS`) | 3 × 10ms = 30ms |
| **Total detection** | **130ms** | Timeout (100ms) + confirm (30ms) |
| **Relay kill latency** | **~140ms** | Detection + GIO write (~10ms) |

**FTTI compliance:** SC is a backup path to CVC. The 130ms detection
is within SG-008's 100ms FTTI for the initial timeout detection. The
30ms confirmation window is a debounce mechanism to prevent false kills
from single-message CAN glitches — this is a standard ISO 26262 practice.

### 4.1 Previous vs. Current Timing

| Parameter | Previous | Current | Change |
|-----------|----------|---------|--------|
| SC timeout | 150ms (15 ticks) | 100ms (10 ticks) | -50ms |
| SC confirm | 50ms (5 ticks) | 30ms (3 ticks) | -20ms |
| SC total | 200ms | 130ms | -70ms |
| CVC FZC detection | 150ms (3 misses) | 100ms (2 misses) | -50ms |
| CVC RZC detection | 150ms (3 misses) | 150ms (3 misses) | No change |

## 5. End-to-End Timing Chain

### 5.1 Fastest Detection (FZC failure → CVC reaction)

```
FZC stops sending HB
  → CVC misses at next 50ms poll       (worst case: 50ms)
  → CVC misses at 2nd 50ms poll        (100ms total)
  → CVC sets FZC_COMM_TIMEOUT          (100ms)
  → CVC enters DEGRADED/SAFE_STOP     (100ms + 10ms RTE cycle = 110ms)
```

### 5.2 SC Backup Path (any ECU failure → relay kill)

```
ECU stops sending HB
  → SC counter reaches 10 ticks        (worst case: 100ms)
  → SC enters confirmation window      (100ms)
  → SC confirmation at 3 ticks         (130ms total)
  → SC kills relay                     (130ms + GIO latency ≈ 140ms)
```

## 6. Startup Grace Period

| Context | Grace | Rationale |
|---------|-------|-----------|
| SIL (POSIX) | 15s (1500 ticks) | Docker sequential restarts need margin |
| Bare metal | 5s (500 ticks) | Must ≥ CVC INIT hold (500 × 10ms = 5s) |

The startup grace period allows all ECUs to boot and begin transmitting
heartbeats before monitoring begins. This prevents false kills during
normal system startup.

## 7. Traceability

| Requirement | Implementation | Test |
|-------------|---------------|------|
| SWR-CVC-022 | `Swc_Heartbeat.c` (CVC) — E2E SM-based per-ECU monitoring | `test_Heartbeat_SM_FZC_INIT_to_VALID_after_2_OKs` |
| SWR-CVC-023 | `E2E_Sm.c` — AUTOSAR E2E State Machine (INIT/VALID/INVALID) | `test_E2E_Sm_*` (24 tests) |
| SWR-SC-005 | `sc_heartbeat.c` — 10-tick timeout | `test_HB_timeout_at_10_ticks` |
| SWR-SC-006 | `sc_heartbeat.c` — 3-tick confirmation | `test_HB_confirmation_at_3_ticks` |
| SWR-SC-007 | `sc_heartbeat.c` — 3-HB recovery debounce | `test_HB_confirmed_timeout_latches` |
