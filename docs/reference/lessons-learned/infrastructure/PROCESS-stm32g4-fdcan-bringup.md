# Lessons Learned: STM32G4 FDCAN1 Bring-Up (F2)

**Date:** 2026-03-03
**Phase:** F2 — CVC↔FZC CAN Bring-Up
**Hardware:** STM32G474RE Nucleo, FDCAN1

---

## 2026-03-03 — STM32G4 FDCAN DLC encoding differs from STM32H7

**Context**: Implementing `Can_Hw_Receive()` for FDCAN1 on STM32G474RE. The RX DLC conversion was written based on STM32H7 HAL documentation.

**Mistake**: Assumed `FDCAN_DLC_BYTES_8 = 0x00080000` (bit-shifted by 16) as documented for STM32H7 MCAN IP. Used `(rxHeader.DataLength >> 16u) & 0xFu` to extract byte count. On STM32G4, `FDCAN_DLC_BYTES_8 = 0x00000008` (raw value). The shift returned DLC=0 for all frames.

**Fix**: Changed to `dlcRaw = rxHeader.DataLength` (direct assignment). Classic CAN DLC 0-8 maps 1:1 to byte count on STM32G4 HAL.

**Principle**: Always check the actual HAL header defines for the target MCU family. STM32 HAL is NOT consistent across families — the same struct field uses different encodings on G4 vs H7.

---

## 2026-03-03 — HAL_FDCAN_Init requires DeInit before reconfiguration

**Context**: CAN internal loopback self-test stops FDCAN (normal mode), reconfigures in `FDCAN_MODE_INTERNAL_LOOPBACK`, then restores to `FDCAN_MODE_NORMAL`.

**Mistake**: Called `HAL_FDCAN_Stop()` then `Can_Hw_InitMode()` (which calls `HAL_FDCAN_Init()`). Init failed silently because the HAL state machine was in READY state, not RESET. `HAL_FDCAN_Init()` expects RESET state (only calls `HAL_FDCAN_MspInit()` when state is RESET).

**Fix**: Added `HAL_FDCAN_DeInit(&hfdcan1)` before every `Can_Hw_InitMode()` call. The sequence must be: `Stop → DeInit → Init → Start`.

**Principle**: STM32 HAL peripherals have a strict state machine. Reconfiguring a peripheral mid-flight requires the full cycle: Stop → DeInit (READY→RESET) → Init (RESET→READY) → Start. Skipping DeInit leaves the handle in READY state and Init won't properly reconfigure.

---

## Debugging Approach

1. Added UART debug prints at each step of the loopback test: `stop+deinit`, `init loopback`, `start`, `transmit`, `polling RX`, `got frame`, `PASS`/`TIMEOUT`
2. Printed hex values of received ID, DLC, and first data byte
3. Output `ID=07FF DLC=0000 D0=00CA` — ID and data correct but DLC=0 → pointed directly to DLC encoding bug
4. After DLC fix, loopback passed immediately

**Principle**: For hardware bring-up, instrument each step with UART output. Print actual values in hex, not just pass/fail. A single hex dump can point you straight at the root cause.
