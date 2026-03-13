# Lessons Learned — CAN Bus Bring-Up (STM32G474RE)

**Date:** 2026-03-04
**Context:** F2 — First CAN bus communication between CVC and FZC on real Nucleo hardware

## Issue 1: FDCAN Kernel Clock Defaults to HSE

**Mistake:** STM32G4 FDCAN peripheral defaults its kernel clock source (RCC CCIPR FDCANSEL) to HSE (0x00). Our config uses HSI→PLL, HSE is not enabled. FDCAN silently fails to clock.

**Fix:** Explicitly select PCLK1 as FDCAN kernel clock in `Can_Hw_InitMode()`:
```c
RCC_PeriphCLKInitTypeDef pclk = {0};
pclk.PeriphClockSelection = RCC_PERIPHCLK_FDCAN;
pclk.FdcanClockSelection  = RCC_FDCANCLKSOURCE_PCLK1;
(void)HAL_RCCEx_PeriphCLKConfig(&pclk);
```

**Principle:** Never assume peripheral clock defaults match your system clock configuration. Always explicitly configure kernel clock sources for peripherals that have independent clock muxes (FDCAN, LPUART, I2C, etc.).

## Issue 2: SJW=1 Too Tight for HSI Oscillators

**Mistake:** CAN bit timing used SJW (Synchronization Jump Width) = 1. At 500 kbps with HSI-based clocks (±1% tolerance per chip), two independent ECUs can have up to ±2% relative clock error. SJW=1 cannot compensate, causing ACK errors (LEC=3) and TEC saturation.

**Fix:** Increased SJW from 1 to 4. At 125 kbps (8µs bit time) SJW=1 worked because the absolute timing margin is wider, but at 500 kbps (2µs bit time) the margin is too tight.

**Principle:** For CAN bus with RC/HSI oscillators (not crystal), always use SJW ≥ 4. CubeMX defaults SJW=1 which is only valid for crystal oscillators with <0.1% tolerance. HSI has ±1%, meaning ±2% worst-case relative error between two nodes.

## Issue 3: Transceiver Module Pin Incompatibility

**Mistake:** SN65HVD230 and CJMCU-1051 (TJA1051) CAN transceiver modules have CANH/CANL pins in different physical positions. Wiring them identically resulted in CANH↔CANL crossed, causing no communication (LEC=3 ACK errors on both sides).

**Fix:** Verify CANH/CANL pin positions per module datasheet. Swapped CANH↔CANL wires on the SN65HVD230 side.

**Principle:** Never assume two CAN transceiver breakout modules have the same pinout. Always verify CANH/CANL positions from the module silkscreen or datasheet before wiring.

## Issue 4: Breadboard Contact Quality

**Mistake:** CJMCU-1051 modules had header pins just pushed through the holes, not soldered. This caused intermittent contact, manifesting as TEC=248 (error-passive).

**Fix:** Soldered header pins to the CJMCU-1051 module. TEC improved from 248 to 128 (still needed CANH/CANL fix for full communication).

**Principle:** For any bring-up involving CAN transceivers, solder all connections. Push-fit pins on breadboard modules cause enough contact resistance and intermittency to prevent reliable CAN communication.

## Diagnostic Approach That Worked

1. **LEC (Last Error Code)** from FDCAN PSR register was the key diagnostic — LEC=3 (ACK error) immediately told us frames were being transmitted correctly but not acknowledged, pointing to a bus-level issue rather than FDCAN peripheral or GPIO misconfiguration.
2. **GPIO register reads** (MODER + AFRH for PA11/PA12) confirmed pin configuration was correct (AF9, alternate function mode).
3. **FDCAN kernel clock register** (RCC CCIPR FDCANSEL bits 25:24) confirmed clock source selection.
4. **Fast diagnostic sampling** (every 200ms for first 3s) caught the exact moment of first error.

## Final Configuration

- PSC=17, Seg1=15, Seg2=4, SJW=4 → 500 kbps at 170 MHz PCLK1
- Sample point: (1 + 15) / 20 = 80%
- FDCAN kernel clock: PCLK1 (explicitly configured)
- AutoRetransmission: ENABLE
- Filter: Accept-all standard IDs → FIFO0
