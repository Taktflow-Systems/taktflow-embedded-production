# Plan: ThreadX on STM32L552ZE (Cortex-M33 / HSM ECU)

## Board: NUCLEO-L552ZE-Q

| Feature | Value |
|---------|-------|
| MCU | STM32L552ZETxQ |
| Core | Cortex-M33 + FPU + DSP |
| Clock | MSI 4 MHz -> PLL -> 110 MHz |
| Flash | 512 KB |
| SRAM | 256 KB (192 + 64) |
| CAN | FDCAN1 (PA11/PA12, AF9) |
| UART | LPUART1 PG7(TX) via ST-LINK VCP |
| LED | PC7 (LD1 green) |
| TrustZone | Available but NOT used in this experiment |

## Approach: Same incremental steps as FZC/RZC experiments

### Step 1: LED blink + UART (CURRENT)
- [x] Bare-metal register init (no HAL dependency)
- [x] SystemClock: MSI -> PLL -> 110 MHz
- [x] LED toggle on PC7 via ThreadX thread
- [x] LPUART1 printf via bare registers
- [x] ThreadX Cortex-M33 port (GNU)
- [x] Linker script for L552ZE
- [x] tx_initialize_low_level.s (SysTick 1000 Hz)
- [ ] **VERIFY**: Flash and confirm LED blinks, UART prints tick count

### Step 2: Add FDCAN
- [ ] Init FDCAN1 via bare registers (PA11/PA12 AF9)
- [ ] 500 kbps classic CAN (same timing as G4 experiments)
- [ ] TX one frame: `cansend` equivalent
- [ ] RX via polling
- [ ] **VERIFY**: `candump can0` on Pi shows frames

### Step 3: BSW CAN stack
- [ ] Add Can.c + Can_Hw_STM32.c (adapt for L5 FDCAN)
- [ ] Add CanIf, PduR, Com
- [ ] Add Rte + Swc_Heartbeat
- [ ] **VERIFY**: Heartbeat frames on CAN bus

### Step 4: UDS diagnostics
- [ ] Add CanTp, Dcm, BswM, WdgM, Dem
- [ ] **VERIFY**: TesterPresent response

### Step 5: TrustZone split (future)
- [ ] Secure world: crypto keys, secure boot verification
- [ ] Non-secure world: application + BSW
- [ ] ThreadX Module Manager for secure/non-secure context switch

## Key Differences from G4/F4 Experiments

| Feature | G4 (FZC) | F4 (RZC) | L5 (HSM) |
|---------|----------|----------|----------|
| CAN | FDCAN (HAL) | bxCAN (HAL) | FDCAN (bare) |
| Clock | 170 MHz | 96 MHz | 110 MHz |
| FPU | FPv4-SP | FPv4-SP | FPv5-SP |
| ThreadX port | cortex_m4 | cortex_m4 | cortex_m33 |
| TrustZone | N/A | N/A | Available |
| HAL | STM32G4xx | STM32F4xx | None (bare-metal) |

## Rules
Same as PLAN.md in threadx-can:
1. One change per step
2. Verify immediately
3. If it breaks, revert
4. UART debug always on
