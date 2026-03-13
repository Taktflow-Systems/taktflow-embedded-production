# Lessons Learned

## 2026-03-09 — DCAN IFCMD bit position regression breaks CAN TX on TMS570

**Context**: SC (Safety Controller) CAN TX stopped working after flashing firmware built from HEAD. CAN ID 0x013 frames were absent from the bus. DCAN error status showed TxOk=0 and LEC=6 (CRC error).

**Mistake**: Commit `865d7ce` ("refactor: move CubeMX/HALCoGen configs under firmware ECU dirs") rewrote DCAN_IFCMD_* bit definitions using little-endian [7:0] positions instead of the correct big-endian [23:16] positions required by TMS570LC43x. Also changed DCAN register offsets (BTR: 0x0C→0x08, TEST: 0x14→0x10) and replaced the atomic NewDat read-clear with an explicit write-back using the wrong command bits.

**Fix**: Reverted DCAN_IFCMD_* defines to shifts [16..23], restored BTR/TEST offsets, removed redundant NewDat write-back (read with NEWDAT flag already clears atomically when WR=0).

**Principle**: TMS570 is big-endian ARM Cortex-R5F — DCAN IFxCMD command mask byte maps to bits [23:16], not [7:0]. Never assume little-endian register layout on TMS570. When refactoring files that move between directories, verify register-level constants against the TRM, not against ARM Cortex-M conventions. Always do a CAN smoke test (candump) after any firmware rebuild that touches DCAN code.

## 2026-03-09 — CAN signal byte-position mismatches masked by DTC belt-and-suspenders

**Context**: SIL overcurrent test (SG-001) failed — vehicle stayed in RUN instead of transitioning to SAFE_STOP. Additionally, motor temperature spiked to 146°C after overcurrent injection.

**Mistake**: Three independent encoding bugs were present simultaneously:
1. Plant-sim `_tx_motor_status()` packed MotorFaultStatus at byte 4 instead of byte 7 (DBC defines bit 56). CVC signal 21 reads byte 7 → always saw 0 → never confirmed motor fault.
2. RZC firmware `Swc_RzcCom.c` wrote `pdu[4] = (uint8)overcurrent` for Motor_Current (0x301), clobbering direction/enable bits at the same byte. DBC defines OvercurrentFlag at bit 34 (byte 4, bit 2) — should be `pdu[4] |= overcurrent << 2`.
3. Motor thermal model kept using latched 28A current for heat calculation even after `_hw_disabled = True`, producing unrealistic temperature runaway.

All signal-based fault detection was broken, but tests appeared to "mostly work" because fault scenarios inject DTC directly on CAN 0x500 as a fallback — masking the root cause for weeks.

**Fix**:
- Rewrote Motor_Status encoding to match DBC byte layout (byte 7 for MotorFaultStatus).
- Fixed RZC Motor_Current encoding to pack dir|enable|overcurrent as individual bits per DBC.
- Thermal model uses `thermal_current = 0.0 if _hw_disabled` to stop heating when motor driver is off.
- Added DBC byte-layout docstrings to all plant-sim TX functions to prevent future drift.

**Principle**: DBC is the single source of truth for CAN signal layout. Every TX function — firmware and plant-sim — must have an explicit byte-layout comment referencing DBC bit positions. When a belt-and-suspenders mechanism (direct DTC injection) masks signal-path failures, bugs hide for weeks. Always test the primary detection path in isolation before adding fallbacks. Treat "test passes but via wrong path" as a failure.

## 2026-03-10 — Dual TX path on same CAN ID breaks E2E protection

**Context**: FZC brake fault detection worked internally (fault=1, latch=1), but CVC never transitioned to SAFE_STOP. CAN 0x210 frames from FZC had all-zero payload despite active fault.

**Mistake**: `Swc_Brake.c` called `Com_SendSignal(FZC_COM_SIG_TX_BRAKE_FAULT)` every 10ms, which packed the fault value into the Com PDU buffer and set `com_tx_pending`. `Com_MainFunction_Tx` then sent this frame via `PduR_Transmit` — without E2E protection (no CRC, no alive counter). Meanwhile, `Swc_FzcCom_TransmitSchedule` correctly sent an E2E-protected frame on the same CAN ID (0x210) every 100ms. The Com layer frame ran at lower priority and fired 10x more often, so CVC received mostly invalid-E2E frames and rejected them.

**Fix**: Removed `Com_SendSignal` from `Swc_Brake.c`. The manual TX path in `Swc_FzcCom_TransmitSchedule` already reads brake fault from RTE and sends with proper E2E. One CAN ID = one TX path.

**Principle**: Never have two TX paths (Com layer + manual PduR_Transmit) for the same CAN ID. The Com layer doesn't add E2E protection — it just packs signals into PDU buffers. If a message needs E2E, use the manual path exclusively. Grep for `Com_SendSignal` and `PduR_Transmit` on the same PDU ID to detect conflicts.

## 2026-03-10 — SIL timing differences require platform-specific debounce

**Context**: runaway_accel scenario triggered SAFE_STOP instead of DEGRADED. SC plausibility check killed the relay before CVC could limit torque.

**Mistake**: SC plausibility debounce was 5 ticks (50ms) — designed for bare-metal where FOC inverter responds in <1ms. In SIL, CAN round-trip between CVC torque command and plant-sim current response is ~20-30ms. The 50ms debounce window was too tight — SC saw 2-3 "implausible" ticks during the CAN latency window and killed prematurely.

**Fix**: `#ifdef PLATFORM_POSIX` debounce = 10 ticks (100ms) for SIL, 5 ticks for HW. Same pattern already used for FZC steering/brake debounce.

**Principle**: Any debounce or timeout constant that depends on signal latency needs a PLATFORM_POSIX override. SIL CAN adds 20-30ms per hop vs <1ms on real hardware. Document the latency assumption next to every debounce constant. Use the existing `#ifdef PLATFORM_POSIX` pattern consistently across all ECUs.

## 2026-03-10 — Platform abstraction cleanup: four patterns eliminate 35+ ifdefs

**Context**: Application code (SWC + SC) had 35+ `#ifdef PLATFORM_*` directives scattered across 10+ files. SIL and target executed different code paths, hiding bugs and violating AUTOSAR portability principle.

**Mistake**: Platform differences were handled ad-hoc: fprintf for debug, direct MCAL stub calls in SWCs, MMIO register reads inline in SC main loop, timing constants inside `#ifdef` blocks. Each developer added ifdefs as needed with no architectural pattern. The NvM temp buffer in Dem.c was only used on POSIX — target builds had a latent BSS overflow (NVM_BLOCK_SIZE=1024 > sizeof(dem_events)=~224 bytes).

**Fix**: Applied four AUTOSAR-aligned patterns:
1. **Link-time hw-file swap** (Vector vVIRTUALtarget): `sc_hw_tms570.c` / `sc_hw_posix.c`, `IoHwAb_Posix.c` / `IoHwAb_Target.c` — Makefile selects which `.c` to link.
2. **Config header split** (AUTOSAR EcuC): `*_Cfg_Platform.h` in `cfg/platform_posix/` and `cfg/platform_target/`, selected via Makefile `-I` path ordering.
3. **Det callout** for debug text: `Det_ReportRuntimeError()` with structured IDs; `Det_Callout_Sil.c` maps to human-readable text, linked only in POSIX builds.
4. **MMIO register isolation**: TMS570 MMIO reads extracted into hw-file functions (`sc_hw_debug_boot_dump`, `sc_hw_debug_periodic`) to prevent POSIX segfaults.

Result: Zero `#ifdef PLATFORM_*` in any SWC source, SC main, or BSW service. Only hw-files and MCAL retain platform guards (by design). All 47 tests pass.

**Principle**: Never put `#ifdef PLATFORM_*` in application code. Use link-time swap for behavior differences, config header split for constant differences, and Det callout for debug output. MMIO register access must be isolated in hw-files — one segfault-causing dereference in shared code breaks all POSIX builds. When fixing ifdefs, check for latent bugs that the ifdef was masking (e.g., the NvM BSS overflow was harmless only because the temp buffer was ifdef'd out on target).

## 2026-03-11 — CanIf direct-index assumption breaks heartbeat TX on TCU/ICU

**Context**: TCU and ICU heartbeats (CAN 0x015 and 0x014) were not appearing on the CAN bus despite processes running and main loops calling Com_MainFunction_Tx.

**Mistake**: Three independent bugs:
1. **CanIf TX array order**: `CanIf_Transmit` uses `TxPduId` as a **direct array index** (`txPduConfig[TxPduId]`), not a lookup. TCU's `canif_tx_config[]` had UDS response (PDU ID 1) at index 0 and heartbeat (PDU ID 0) at index 1. Result: heartbeat data was sent on CAN 0x644 (UDS) instead of 0x015.
2. **Wrong Com signal IDs**: TCU used `Com_SendSignal(6u/7u)` and ICU used `Com_SendSignal(19u/20u)` for heartbeat. These were RX signal IDs (ESTOP/FZC heartbeat), not TX. Correct IDs are 3 (AliveCounter) and 4 (ECU_ID) — the first 5 signals (0-4) in each ECU's Com config are TX heartbeat signals.
3. **Missing BSW cyclic functions**: ICU main loop only called `Can_MainFunction_Read()` and `Rte_MainFunction()` — missing `Com_MainFunction_Rx()` and `Com_MainFunction_Tx()`. Without `Com_MainFunction_Tx()`, no PDU ever reaches CanIf.

**Fix**: Reordered CanIf TX array so index matches PDU ID. Changed signal IDs to 3u/4u. Added Com_MainFunction_Rx/Tx to ICU main loop. Added explicit Icu_Heartbeat_500ms call (not in RTE runnable config).

**Principle**: CanIf uses TxPduId as a direct array index — the `canif_tx_config[]` array MUST be ordered so that entry at index N has PduId == N. Every ECU main loop MUST call `Can_MainFunction_Read()`, `Com_MainFunction_Rx()`, `Com_MainFunction_Tx()` — the RTE scheduler only dispatches SWC runnables, never BSW cyclic functions. When adding heartbeat functions, verify the Com signal IDs against `Com_Cfg_*.c` signal config table (TX signals are always the first entries, starting at ID 0).
