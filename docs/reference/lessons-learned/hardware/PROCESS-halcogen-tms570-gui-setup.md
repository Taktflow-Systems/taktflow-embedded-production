# HALCoGen GUI Setup for TMS570LC4357 (LAUNCHXL2-570LC43)

**Date**: 2026-03-05
**Context**: SC (Safety Controller) TMS570 target bring-up — Phase F5.0

## Summary

HALCoGen v04.07.01 generates initialization code for TI TMS570 MCUs. The GUI configuration required manual review and corrections for the LAUNCHXL2-570LC43 LaunchPad. This documents the exact settings used and pitfalls discovered.

## HALCoGen Project Settings

- **Device**: TMS570LC4357ZWT
- **HALCoGen version**: 04.07.01
- **Output directory**: `firmware/sc/cfg/halcogen/`

## Peripheral Configuration

### GIO (General I/O)
- **Port A bits 0-5**: Output (relay control, status LEDs, WDI)
- **Port B bit 1**: Output (heartbeat LED)
- All other pins: input (default)
- Interrupts: disabled (SC uses polling)
- Pull control: disabled on output pins

### RTI (Real-Time Interrupt)
- **Clock source**: VCLK (75 MHz on TMS570LC4357)
- **Prescaler**: 8 (RTICLK = 9.375 MHz)
- **Compare 0**: 93750 counts = **10 ms tick** (matches SC's 10ms runnable period)
- **Counter blocks**: Only block 0 used
- All compare interrupts initially disabled (SC enables CMP0 interrupt only)

### CAN1
- **Module**: DCAN1 (CAN1 on LaunchPad headers)
- **Baud rate**: 500 kbps
- **Configured via HALCoGen GUI**: standard frames, no FD

### SCI1 (UART Debug)
- **Baud rate**: 115200
- **Data bits**: 8
- **Stop bits**: 2 (HALCoGen default — **should be 1** for standard terminals)
- **Parity**: None
- **Flow control**: None

> **Gotcha**: HALCoGen defaults SCI to 2 stop bits. Most USB-UART adapters and terminal emulators expect 1 stop bit. Either change in HALCoGen GUI before regeneration, or handle in `sc_sci_init()` by overriding the SCIGCR1 register.

### ESM (Error Signaling Module)
- Reviewed in GUI — default configuration retained
- Group 1/2/3 error channels mapped to TMS570LC4357 defaults

## Key Discoveries

### 1. HALCoGen Generates TI-Compiler-Specific Code

HALCoGen output assumes the legacy TI ARM compiler (armcl), NOT GCC:
- `.asm` files use TI assembler syntax (`.def`, `.asmfunc`, `.endasmfunc`)
- `.cmd` linker scripts (not GNU `.ld`)
- `#pragma WEAK`, `#pragma CODE_STATE`, `#pragma INTERRUPT` (TI-specific)
- `__TI_auto_init()` runtime initialization

**Solution**: Use **tiarmclang** (TI ARM Clang, LLVM-based) from CCS 20.4.1. It understands:
- TI `.cmd` linker scripts via `tiarmlnk` backend
- TI assembly syntax via `-x ti-asm` flag
- TI pragmas (with `-Wno-ti-pragmas` to suppress warnings)
- `__TI_auto_init()` from TI runtime library

### 2. Symbol Conflicts Between HALCoGen and SC

HALCoGen's `HL_gio.c` and `HL_rti.c` define functions with pointer-based signatures:
```c
// HALCoGen signatures:
void gioSetBit(gioPORT_t *port, uint32 bit, uint32 value);
void rtiStartCounter(rtiBASE_t *rtiREG, uint32 counter);
```

SC defines the same function names with value-based signatures:
```c
// SC signatures:
void gioSetBit(uint8 port, uint8 pin, uint8 value);
void rtiStartCounter(void);
```

**Solution**: Exclude `HL_gio.c`, `HL_rti.c`, `HL_sys_main.c` from the build via `filter-out` in the Makefile. Provide replacement implementations in `sc_hw_tms570.c` using raw register writes.

### 3. Type Conflicts Between HALCoGen and SC

`HL_hal_stdtypes.h` and `sc_types.h` both define `uint32`, `uint8`, `boolean` with different guard macros and underlying types (`boolean = _Bool` vs `boolean = uint8`). Including both in the same translation unit causes redefinition errors.

**Solution**: SC sources include only `sc_types.h`. HALCoGen sources include their own headers. The excluded HALCoGen files that would cross the boundary are replaced with direct register access in `sc_hw_tms570.c`.

### 4. TI Assembly Files Need `-x ti-asm` Flag

tiarmclang's default LLVM integrated assembler cannot parse TI assembler syntax. The `-x ti-asm` flag routes `.asm` files through TI's assembler backend:
```
tiarmclang -mcpu=cortex-r5 -marm -x ti-asm -c HL_sys_core.asm -o HL_sys_core.o
```

### 5. RTI INTFLAG Register Is Write-1-to-Clear

The RTI interrupt flag register at offset 0x88 (`RTIINTFLAG`) uses write-1-to-clear (W1C) semantics. Writing a 1 to a bit clears that interrupt flag. The register at offset 0x8C is **RESERVED** — do not write to it.

### 6. Split Warning Levels for HALCoGen vs SC Code

HALCoGen auto-generated code triggers many warnings under strict settings:
- `__little_endian__` macro → `-Wno-ti-macros`
- `#pragma WEAK/CODE_STATE/INTERRUPT` → `-Wno-ti-pragmas`
- Unused parameters throughout → `-Wno-unused-parameter`
- Unused variables → `-Wno-unused-variable`

**Solution**: Two sets of CFLAGS in the Makefile:
- `HAL_CFLAGS`: relaxed warnings (no `-Werror`)
- `SC_CFLAGS`: strict warnings (`-Wall -Wextra -Werror`)

### 7. TMS570 Is Big-Endian

The TMS570 Cortex-R5F runs in **big-endian** mode. DSLite will reject a little-endian ELF with "Does not match the target endianness." Add `-mbig-endian` to MCU_FLAGS.

### 8. CCXML Requires Driver Instance Elements

A minimal CCXML that only references the device XML will fail with "An invalid processor ID has been found." The CCXML **must** include `<instance>` elements for three driver files inside the `<connection>`:
```xml
<instance href="drivers/tixds510icepick_c.xml" id="drivers" xml="tixds510icepick_c.xml" xmlpath="drivers"/>
<instance href="drivers/tixds510cs_dap.xml" id="drivers" xml="tixds510cs_dap.xml" xmlpath="drivers"/>
<instance href="drivers/tixds510cortexR.xml" id="drivers" xml="tixds510cortexR.xml" xmlpath="drivers"/>
```
Without these, DSLite cannot map the ICEPick → CS_DAP → Cortex-R5 debug path. Reference: [RTI Connext TMS570 CCXML](https://github.com/rticommunity/rticonnextddsmicro-tms570-demo/blob/master/targetConfigs/TMS570LC43xx.ccxml).

### 9. HALCoGen Object Output Path Escapes Build Directory

HALCoGen sources now live under `firmware/sc/cfg/halcogen/` and objects land under `build/tms570/sc/cfg/halcogen/` — inside the normal build directory. No special clean handling needed.

### 10. XDS110 Firmware Auto-Update

First connection via DSLite may trigger an XDS110 firmware update (v3.0.0.15 → v3.0.0.41). This is automatic and takes ~10 seconds. Do NOT unplug during update.

## Principles

1. **Always review HALCoGen defaults** — stop bits, clock sources, and prescalers may not match your hardware setup
2. **HALCoGen + tiarmclang is the supported path** — don't fight it with GCC
3. **Isolate HALCoGen from application code** — separate warning levels, exclude conflicting sources, use raw register access at the boundary
4. **Document every register offset you use** — HALCoGen headers have type conflicts, so you can't include them directly; raw offsets need clear comments
5. **TMS570 is big-endian** — always pass `-mbig-endian` to tiarmclang
6. **CCXML needs driver instances** — copy from a working reference, don't hand-craft from scratch
