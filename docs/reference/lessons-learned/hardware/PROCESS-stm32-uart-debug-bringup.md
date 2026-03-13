# Lessons Learned — STM32 UART Debug Bring-up (F1.5)

## 2026-03-03 — SysTick ISR conflict with CubeMX

**Context**: Implementing SysTick + UART debug output for CVC firmware on Nucleo-G474RE.
**Mistake**: Initial plan called for a custom `SysTick_Handler` ISR in `cvc_hw_stm32.c`. This would cause a duplicate symbol error because CubeMX's `stm32g4xx_it.c` (included in the build) already defines `SysTick_Handler` calling `HAL_IncTick()`.
**Fix**: Used `HAL_Init()` (which configures SysTick at 1ms) + `HAL_GetTick() * 1000u` for microsecond conversion. No custom ISR needed.
**Principle**: Always check CubeMX-generated interrupt files before defining ISRs. The CubeMX `stm32g4xx_it.c` owns all Cortex-M exception handlers — add to its USER CODE sections or use HAL callbacks, never define competing strong symbols.

## 2026-03-03 — Serial capture requires open-before-reset

**Context**: Verifying UART boot output after flashing firmware.
**Mistake**: First attempt opened serial port after OpenOCD reset — boot messages were already printed and lost (no buffering on VCP).
**Fix**: Open serial port first (pyserial), then reset board via OpenOCD in a background thread, then read. The serial port must be listening before the first byte is transmitted.
**Principle**: For capturing boot-time UART output, always open the serial monitor before triggering reset. ST-LINK VCP has no internal buffer — bytes not read are lost.

## 2026-03-03 — USART2 TEACK wait required before first TX

**Context**: Bare-metal USART2 init for debug output.
**Mistake**: Without waiting for TEACK (TE Acknowledge) in USART ISR after enabling CR1_TE + CR1_UE, the first character can be corrupted or lost.
**Fix**: Added `while ((USART2->ISR & USART_ISR_TEACK) == 0u)` spin loop after enabling USART.
**Principle**: STM32G4 USART requires TEACK confirmation before first write to TDR. Always wait for peripheral acknowledge bits after enable.
