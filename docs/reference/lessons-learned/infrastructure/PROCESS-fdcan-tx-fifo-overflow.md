# Lessons Learned — FDCAN TX FIFO Overflow Starving Heartbeat Messages

**Project:** Taktflow Embedded — Zonal Vehicle Platform
**Period:** 2026-03-03 to 2026-03-04
**Scope:** RZC heartbeat (CAN 0x012) never received by CVC; root cause was FDCAN TX FIFO depth = 3 slots vs 4 frames/cycle

## Context

During 3-ECU CAN bring-up (CVC + FZC + RZC on STM32G474RE at 500 kbps), CVC
consistently showed `RZC=TIMEOUT` and `Ra=0` despite the CanIf/PduR/Com routing
being correctly configured.  FZC heartbeat (0x011) worked intermittently.

## Mistake

Both RZC and FZC `TransmitSchedule` functions sent 4+ CAN frames per 10ms cycle:
- RZC: motor_status (0x300), motor_current (0x301), motor_temp (0x302), battery (0x303)
- FZC: steer_status (0x200), brake_status (0x201), brake_fault (0x210), motor_cutoff (0x211)

The STM32G4 FDCAN TX FIFO has only **3 slots**.  Queueing 4 frames per cycle caused
the lowest-priority frames (heartbeat at 50ms) to hit `CAN_BUSY` every time,
resulting in 120 TX failures/second and zero heartbeat delivery.

## Fix

1. **TX scheduling with cycle counters**: Paced slow-changing status signals to
   lower rates with staggered offsets to avoid collisions:
   - RZC: motor_temp → 100ms (offset 3), battery → 1000ms (offset 7)
   - FZC: brake_fault → 100ms (offset 3), motor_cutoff → 100ms (offset 7)
   - Result: max 2 frames/cycle from TransmitSchedule, plus 1 from heartbeat every 5th cycle

2. **Spin-wait retry in Can_Hw_Transmit**: Added 5000-iteration retry (~300µs at
   170 MHz) as safety net for transient FIFO-full conditions.

3. **1ms CAN RX polling**: Changed `Can_MainFunction_Read` from 10ms to 1ms period
   to prevent RX FIFO overflow (bus carries >3 frames per 10ms window).

## Principle

**Never exceed FDCAN TX FIFO depth per scheduling tick.**  STM32 FDCAN has only
3 TX FIFO slots by default (hardware limitation).  Count the maximum frames that
can be queued in a single tick across all sources (TransmitSchedule + heartbeat +
Com_MainFunction_Tx), and pace them to stay at or below the FIFO depth.  Use
cycle counters with staggered offsets to spread frames across time.

## Diagnostic Pattern

When debugging CAN delivery:
1. Add per-CAN-ID RX counters in `Can_MainFunction_Read` (h11, h12)
2. Add TX busy counter in `Can_Write` (TXbusy)
3. Compare h-counters with expected rates to quantify delivery ratio
4. If h-count is zero but bus is active: TX-side problem (FIFO overflow)
5. If h-count is non-zero but Com signal is zero: RX-side timing (Com timeout)
