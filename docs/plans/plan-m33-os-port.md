# Plan: Cortex-M33 OS Port (STM32L552ZE)

**Status**: IN PROGRESS
**Created**: 2026-03-15

## Summary

Bootstrap OS port for Cortex-M33 (ARMv8-M) targeting the STM32L552ZE (NUCLEO-L552ZE-Q).
Extends the existing M4 port with FPU lazy stacking, PSPLIM, and TrustZone-ready EXC_RETURN.

## Phases

### Phase 1 — Port Implementation (DONE)

- [x] `firmware/platform/stm32l5/include/Os_Port_Stm32L5.h`
- [x] `firmware/platform/stm32l5/src/Os_Port_Stm32L5.c`
- [x] `firmware/platform/stm32l5/src/Os_Port_Stm32L5_Asm.S`
- [x] `firmware/platform/stm32l5/Makefile.stm32l5`
- [x] `Os_Port_TaskBinding.c` — added PLATFORM_STM32L5 branches
- [x] `firmware/bsw/os/bootstrap/test/test_Os_Port_Stm32L5_bootstrap.c` — 20 tests
- [x] DeepSeek review — found and fixed PSPLIM ordering bug in first-task launch path

### Phase 2 — Hardware Validation (PENDING)

- [ ] Flash L552ZE with minimal blinky + OS port
- [ ] Verify PendSV fires and first task launches (UART debug trace)
- [ ] Verify context switch between two tasks (toggle two LEDs at different rates)
- [ ] Verify PSPLIM triggers UsageFault on deliberate stack overflow
- [ ] Verify FPU lazy stacking: task A uses FPU, task B does not, context switch preserves s16-s31
- [ ] Measure context switch latency (GPIO toggle around PendSV entry/exit)

### Phase 3 — TrustZone (FUTURE)

- [ ] Evaluate secure/non-secure split for crypto operations
- [ ] Add secure stack context save/restore (SVC handler)
- [ ] Update EXC_RETURN to 0xFFFFFFBC for non-secure PSP with FPU
- [ ] Per-thread secure stack pointer tracking

## Key Design Decisions

1. **No TrustZone in Phase 1** — same EXC_RETURN as M4 (0xFFFFFFFD) works when running non-secure only
2. **FPU lazy stacking via EXC_RETURN bit[4]** — frame size varies (9 or 25 software words)
3. **PSPLIM set on every context switch** — `GetCurrentTaskStackLimit()` avoids hardcoded struct offsets in ASM
4. **StackLimit=0 through TaskBinding** — binding layer passes 0 (no guard) by default; direct API allows explicit limit

## Hardware

- **Board**: NUCLEO-L552ZE-Q
- **MCU**: STM32L552ZE (Cortex-M33, 110 MHz, FPv5-SP-D16, TrustZone)
- **COM port**: COM16
- **No CAN** — this MCU is for crypto/security, not CAN communication
