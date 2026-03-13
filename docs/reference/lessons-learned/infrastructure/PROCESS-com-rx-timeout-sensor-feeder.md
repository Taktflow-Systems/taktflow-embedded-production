# Lessons Learned — Com RX Timeout Zeroes Sensor Feeder Data During Container Restart

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Date:** 2026-03-04
**Scope:** FZC enters SAFE_STOP after "Reset All ECUs" because Com RX deadline zeroes the steering angle shadow buffer while the sensor feeder's DataValid gate is already set
**Result:** Root cause identified and fixed — replaced stateful DataValid gate with stateless zero-substitution

---

## 1. Root Cause: Com RX Deadline Timeout After DataValid Gate Set

The FZC sensor feeder had a `SensorFeeder_DataValid` flag to protect against the Com shadow buffer's initial zero state. Once any non-zero value arrived (from plant-sim), `DataValid` was set to 1 permanently and the guard was bypassed.

The failure sequence during `_restart_ecu_containers()`:

1. FZC restarts **first** in the Phase 1 sequential loop
2. Old plant-sim (still running) sends CAN 0x400 with steer_angle=8191 (center)
3. Sensor feeder receives 8191 → **DataValid = 1** (8191 != 0)
4. Plant-sim container **stops** for restart (last in Phase 1)
5. **No CAN 0x400 for >100ms** (other containers restarting in between)
6. Com_MainFunction_Rx deadline fires → **zeroes shadow buffer** (`sig_rx_virt_steer_angle = 0`)
7. Sensor feeder reads steer_angle = 0 from Com
8. DataValid == 1, so the guard does NOT substitute 8191
9. **Injects raw 0 → steering reads -45°** → plausibility fault (cmd=0 vs act=-45)
10. Fault latches → grace period expires → motor cutoff → CVC → SAFE_STOP

```
Timeline:
t=0    FZC restarts, boots in <1s
t=0.5  Old plant-sim sends CAN 0x400 → DataValid=1
t=2    Plant-sim stopped for restart
t=2.1  Com RX timeout fires → shadow buffer = 0
t=2.1  Sensor feeder injects 0 → steering reads -45° → FAULT LATCHES
t=5    FZC grace period (500 cycles) expires → CUTOFF fires
t=7    CVC starts, receives CUTOFF → SAFE_STOP
```

**Lesson:** One-shot state gates (set once, never cleared) are dangerous when the underlying data source can revert. Com shadow buffers can be zeroed by deadline timeout, making "first non-zero → always valid" assumptions incorrect.

---

## 2. The Fix: Stateless Zero-Substitution

Replaced the stateful `SensorFeeder_DataValid` gate with a simple stateless check:

```c
/* OLD: one-shot gate — vulnerable to Com timeout re-zeroing */
if (SensorFeeder_DataValid == 0u) {
    if (steer_angle != 0u) {
        SensorFeeder_DataValid = 1u;  /* Set once, never cleared */
    } else {
        steer_angle = 8191u;
    }
}

/* NEW: stateless — always safe */
if (steer_angle == 0u) {
    steer_angle = 8191u;  /* Center: safe default */
}
```

This handles both scenarios:
- (a) Initial Com buffer = 0 (no CAN 0x400 received yet)
- (b) Com deadline timeout zeroed the buffer (plant-sim gap during restart)

**Lesson:** Prefer stateless guards over stateful ones when the guarded value can revert to its initial state. Stateless checks are simpler, have no edge cases, and survive re-entry scenarios.

---

## 3. Why compose down/up Works But restart Doesn't

| Operation | vcan0 | Old plant-sim | Gap in CAN 0x400 | DataValid set? |
|-----------|-------|---------------|-------------------|----------------|
| `compose down/up` | Destroyed + recreated | Killed before FZC starts | No gap (fresh start) | No (stays 0) |
| `_restart_ecu_containers` | Preserved | Still running when FZC boots | Yes (plant-sim stops later) | **Yes** (race) |

The compose down/up path never triggers the bug because DataValid stays at 0 (no old plant-sim to trigger it). The restart path triggers it because the sequential container restart creates a window where old plant-sim data sets DataValid, then plant-sim stops, then Com timeout zeroes the buffer.

**Lesson:** Container restart ordering creates temporal dependencies that don't exist in full compose down/up. Test both restart paths — they have different failure modes.

---

## 4. AUTOSAR Com RX Deadline Is Correct But Unexpected

The Com module's behavior (zeroing shadow buffers on timeout) is actually the correct AUTOSAR ComRxDataTimeoutAction = REPLACE behavior. It's designed to prevent stale data from propagating. The problem was that the sensor feeder's one-shot DataValid gate assumed the shadow buffer would never return to zero after being set to a valid value.

**Lesson:** When building on top of AUTOSAR BSW, assume that any Com signal value can be zeroed at any time due to RX deadline timeout. Design injection/feeder modules to handle re-zeroing gracefully.

---

## Key Takeaways

| Topic | Lesson |
|-------|--------|
| One-shot state gates | Unsafe when underlying data can revert (Com timeout) |
| Stateless vs stateful | Prefer stateless zero-substitution over stateful DataValid flags |
| Container restart vs compose | Different restart methods have different failure modes |
| Com RX deadline | Correctly zeroes shadow buffers — consumers must handle it |
| Sequential restart ordering | Creates temporal windows not present in full restart |
