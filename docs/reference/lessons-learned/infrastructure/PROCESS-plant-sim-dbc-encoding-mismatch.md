# Lessons Learned — Plant-Sim DBC Encoding Mismatch Causing Startup SAFE_STOP

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Date:** 2026-03-04
**Scope:** Plant-sim applied DBC encoding to steer command signal; CVC firmware sends plain degrees

## Context

After Docker container restart, the vehicle state machine reached SAFE_STOP within
~1 second of CVC INIT→RUN transition. FZC fired a steering plausibility fault which
propagated as motor_cutoff → DEGRADED → SAFE_STOP.

## Root Cause

**Encoding mismatch between CVC firmware and plant-sim for CAN 0x102 (Steer_Command):**

- **CVC firmware**: Sends `sint16 tx_steer = 0` as plain degrees with NO DBC scaling.
  Comment in `Swc_CvcCom.c`: `/* Center = 0 degrees (plain degrees, no DBC) */`
- **Plant-sim (Python)**: Applied DBC decoding: `raw * 0.01 - 45.0`.
  For raw=0: `0 * 0.01 - 45.0 = -45.0°` (full lock instead of center!)

This caused plant-sim's steering model to drive actual angle from 0° toward -45° at
30°/sec. FZC's sensor feeder injected the drifting angle into SPI. At ~500ms, the
|cmd - actual| exceeded 5° plausibility threshold, latching the steering fault.

## Diagnostic Path

1. Added `SIL_DIAG` to FZC Swc_Steering and Swc_FzcSafety (previously only CVC had DIAG)
2. Enabled `DIAG=1` for all 7 ECU builds in `Dockerfile.vecu`
3. FZC diagnostic output revealed: `cmd=0 act=0` for cycles 1-33, then `act=-1` at c=34,
   `act=-2` at c=37, etc. — exactly matching 30°/sec rate limit × 10ms tick
4. Traced the SPI injection chain: plant-sim → CAN 0x400 → FZC sensor feeder → SPI stub
5. Compared CVC Com config (sint16, plain degrees) vs plant-sim decoding (DBC factor/offset)

## Fix

Changed plant-sim to decode steer command as plain `sint16` degrees (matching firmware):

```python
# OLD (DBC encoding — wrong):
raw = struct.unpack_from('<H', data, 2)[0]
angle = raw * 0.01 - 45.0

# NEW (plain degrees — matches CVC firmware):
raw = struct.unpack_from('<h', data, 2)[0]
angle = max(-45.0, min(45.0, float(raw)))
```

Also fixed the matching encoder in `_steer_frame()` (fault injection) and the SIL
scenario display harness.

## Principle

**When firmware and simulation use different signal encodings, the mismatch may be
invisible in steady-state but catastrophic at startup.** The DBC file documents the
intended encoding, but the firmware's BSW Com layer uses simplified raw byte packing
with no DBC scaling. Any external tool (plant-sim, test harness, trace analyzer) that
applies DBC encoding to signals from this firmware will misinterpret the values.

## Generalizable Takeaways

1. **Document the ACTUAL encoding, not just the DBC**: If firmware deviates from DBC
   (e.g., plain degrees instead of scaled), add explicit comments at both TX and RX sites
2. **Add diagnostics to ALL ECUs, not just one**: The bug was in FZC/plant-sim but only
   CVC had SIL_DIAG enabled. Adding DIAG to FZC immediately revealed the drift pattern.
3. **Startup is the hardest phase to debug**: Race conditions, boot ordering, and default
   values create unique failure modes that don't appear in steady-state testing
4. **Test the plant model in isolation**: A simple check that `steer_cmd=0 → actual=0°`
   at the plant-sim level would have caught this immediately
