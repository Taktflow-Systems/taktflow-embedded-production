# TMS570 DCAN1 TX Debugging — 2026-03-20

## Context
SC (Safety Controller) on TMS570LC4357 LaunchPad (LAUNCHXL2-570LC43) could blink LEDs and pass DCAN1 loopback self-test, but never transmitted CAN frames on the physical bus.

## Mistakes / Root Causes

### 1. CAN Transceiver VCC Not Connected
**Mistake**: SN65HVD230 CAN transceiver module had VCC plugged into wrong pin on LaunchPad.
**Fix**: Corrected VCC wiring. Transceiver needs 3.3V from the board.
**Principle**: Always verify transceiver power LED is on before debugging CAN software.

### 2. Wrong Pin Documentation (sprr397 Markdown Summaries)
**Mistake**: Hand-written markdown summaries of LaunchPad schematic (sprr397-sections/*.md) listed J10 pin 44/45 as FlexRay/GIOB pins. The actual PDF schematic shows J10.44=DCAN1RX, J10.45=DCAN1TX.
**Fix**: Deleted all sprr397 markdown summaries. Always read the original PDF.
**Principle**: Never trust manually transcribed datasheets. Always verify against the original PDF. Markdown summaries of hardware docs are dangerous.

### 3. BE8 vs BE32 Linker Flag
**Mistake**: Test ELF linked without `--be32` flag. TMS570 Cortex-R5 runs in BE32 (word-invariant) mode. Default tiarmclang linking produces BE8 (byte-invariant) which is incompatible — CPU executes garbage instructions, no crash visible (just doesn't run).
**Fix**: Add `-Wl,--be32` to all TMS570 linker commands.
**Principle**: Always check ELF flags with `readelf -h` — look for `0x5000000` (BE32), NOT `0x5800000` (BE8).

### 4. Entry Point Must Be `_c_int00`, Not `main()`
**Mistake**: Test firmware defined `main()` but `sc_startup.S` branches to `_c_int00`. The TI runtime's `_c_int00` wasn't linked because `HL_sys_startup.c` was excluded.
**Fix**: Define `_c_int00` directly in test code, or include `HL_sys_startup.c` which provides `_c_int00` → `main()`.
**Principle**: On TMS570 with custom startup ASM, the entry point is `_c_int00`. Check what the reset vector branches to.

### 5. HALCoGen Configures All DCAN Mailboxes as RX
**Mistake**: HALCoGen's `canInit()` configures all message objects with Dir=0 (RX). The SC code's `dcan1_config_tx_mailbox()` was supposed to override mailbox 7 as TX at runtime, but it doesn't work (still investigating).
**Fix**: Manually configure TX mailbox after `canInit()` with Dir=1 (bit 29 of IF1ARB = 0x20000000).
**Principle**: After `canInit()`, verify mailbox direction by checking the ARB register Dir bit. Don't assume HALCoGen configured TX mailboxes unless you explicitly enabled them in the GUI.

### 6. HALCoGen's `HL_sys_startup.c` Doesn't Work on LaunchPad
**Mistake**: HALCoGen's startup code hangs on the LaunchPad due to CCM lockstep errors. Only the custom `sc_startup.S` (which zeros registers, does memInit, clears ESM/CCM, enables PENA) works.
**Fix**: Always use `sc_startup.S` for TMS570 LaunchPad builds, never HALCoGen's `HL_sys_startup.c`.
**Principle**: TMS570LC43x with lockstep requires careful startup sequence. HALCoGen's startup works with TI compiler (`armcl`) pragmas but not with `tiarmclang`.

## Verification Commands

```bash
# Check ELF endianness (must be 0x5000000, NOT 0x5800000)
arm-none-eabi-readelf -h firmware.elf | grep Flags

# Check vector table branches correctly
arm-none-eabi-objdump -d -j .intvecs firmware.elf

# Check CAN bus for 0x013
candump can0 | grep 013

# Flash with erase + verify
DSLite.exe flash -c TMS570LC43xx_XDS110.ccxml -f firmware.elf -e --verify
```

## Open Issue
Production SC firmware's `dcan1_config_tx_mailbox()` / `dcan1_transmit()` still doesn't produce frames on the bus even with working transceiver. The minimal test using HALCoGen's `canTransmit()` API works. Root cause TBD — likely IF1CMD register write sequence or missing mailbox reconfiguration.
