# SC (Safety Controller) Toolchain Setup — TMS570LC43x

## Hardware

| Item | Details |
|------|---------|
| Board | LAUNCHXL2-570LC43 (TMS570LC4357 LaunchPad) |
| MCU | TMS570LC4357 — ARM Cortex-R5F, 300 MHz, big-endian, VFPv3-D16 |
| Debugger | XDS110 (on-board) |
| User LEDs | D9/D10 on GIOB[6:7] (active HIGH, green) |
| UART | SCI1/LIN1 TX on ball A5, RX on ball C2 — 115200 baud |
| CAN | DCAN1 TX on ball C3, RX on ball C1 |

## Compiler

| Component | Path |
|-----------|------|
| Toolchain | TI ARM Clang (tiarmclang) v4.0.4.LTS |
| Install | `C:/ti/ccs2041/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS` |
| Compiler | `bin/tiarmclang.exe` |
| CCS IDE | Code Composer Studio 20.4.1 (optional — Makefile build works standalone) |

## HALCoGen

| Component | Details |
|-----------|---------|
| Version | HALCoGen v04.07.01 |
| Config | `firmware/sc/cfg/halcogen/` |
| Sources | `firmware/sc/cfg/halcogen/source/` (C + ASM) |
| Headers | `firmware/sc/cfg/halcogen/include/` |

## Build

```bash
cd firmware/sc
make            # debug build
make release    # optimized build
make flash      # flash via DSLite
make clean      # clean build artifacts
```

Output: `build/tms570/sc.out` (ELF)

## Critical Compiler/Linker Flags

### `--be32` (MANDATORY)

The TMS570 Cortex-R5 uses **BE-32** (big-endian 32-bit) byte ordering. The tiarmclang linker defaults to BE-8. Without `--be32`, the binary contains byte-swapped instructions and the CPU executes garbage. **This is the #1 cause of "nothing happens after flash".**

```
-Wl,--be32
```

### `--rom_model` (MANDATORY)

Required for C runtime initialization. `__TI_auto_init` copies `.data` from ROM to RAM and zeros `.bss`. Without this, all global variables start with undefined values.

```
-Wl,--rom_model
```

### `-xti-asm` (for HALCoGen `.asm` files)

tiarmclang supports original TI-syntax assembly (`.asm`) via the `-xti-asm` flag. This eliminates the need to hand-port HALCoGen assembly to GNU syntax.

```
-xti-asm
```

### `-marm`

Forces ARM instruction set (not Thumb). Required for Cortex-R5 startup code.

### `libc.a` (link order)

Must come **after** object files on the link line. Contains `__TI_auto_init`, `__TI_zero_init`, `exit`.

```
# In Makefile:
LDPOST := $(HCG_SRC)/HL_sys_link.cmd -Wl,-llibc.a
$(TARGET): $(ALL_OBJ)
    $(LNK) $(LDFLAGS) -o $@ $(ALL_OBJ) $(LDPOST)
```

### Full compiler flags

```
CFLAGS := -mcpu=cortex-r5 -mbig-endian -mfloat-abi=hard -mfpu=vfpv3-d16 \
          -marm -Og \
          -DPLATFORM_TMS570 -D__big_endian__ \
          -I$(SC_INC) -I$(HCG_INC) -I$(CGT_ROOT)/include \
          -Wall -Wno-unused-parameter -g
```

### Full linker flags

```
LDFLAGS := -mcpu=cortex-r5 -mbig-endian -mfloat-abi=hard -mfpu=vfpv3-d16 \
           -marm \
           -Wl,--rom_model -Wl,--be32 \
           -Wl,-i$(CGT_ROOT)/lib -Wl,--reread_libs \
           -Wl,-m,$(BUILD)/sc.map
```

## tiarmclang Pragma Warnings

tiarmclang ignores these legacy TI pragmas (warnings only, no functional impact):

| Pragma | Impact |
|--------|--------|
| `#pragma CODE_STATE` | No effect — `-marm` forces ARM mode anyway |
| `#pragma INTERRUPT` | No effect — `_c_int00` gets no prologue with `-Og` |
| `#pragma WEAK` | No effect — avoid duplicate symbols by excluding conflicting HALCoGen sources |

## HALCoGen Source Selection

Our `sc_hw_tms570.c` provides custom implementations of GIO, RTI, SCI, and ESM notification functions. These HALCoGen sources are **excluded** to avoid duplicate symbols:

- `HL_gio.c` — we provide `gioInit`, `gioSetBit`, `gioGetBit`, `gioSetDirection`
- `HL_rti.c` — we provide `rtiInit`, `rtiStartCounter`, `rtiIsTickPending`, `rtiClearTick`
- `HL_sci.c` — we provide `sc_sci_init`, `sc_sci_puts`
- `HL_notification.c` — we provide `esmGroup1/2/3Notification`

## Flash Tool

| Component | Details |
|-----------|---------|
| Tool | DSLite v20.4.0 |
| Path | `C:/ti/ccs2041/ccs/ccs_base/DebugServer/bin/DSLite.exe` |
| Config | `firmware/sc/TMS570LC43xx_XDS110.ccxml` |
| Command | `DSLite load -c TMS570LC43xx_XDS110.ccxml -f build/tms570/sc.out` |

DSLite sets PC to the ELF entry point symbol (`_c_int00`) and runs. It does NOT do a hardware reset to address 0x0.

## Startup Sequence

```
Reset/DSLite → _c_int00 (HL_sys_startup.c)
  → _coreInitRegisters_ (HL_sys_core.asm)    — zero all CPU regs in all modes
  → _coreInitStackPointer_ (HL_sys_core.asm)  — set per-mode stack pointers
  → getResetSource (HL_system.c)              — check reset reason
  → _memInit_ (HL_sys_core.asm)              — RAM ECC initialization
  → _errata_SSWF021_45_both_plls             — PLL errata workaround
  → systemInit (HL_system.c)                 — PLL to 300 MHz
  → vimInit (HL_sys_vim.c)                   — VIM interrupt controller
  → esmInit (HL_esm.c)                       — Error Signaling Module
  → _mpuInit_ (HL_sys_mpu.asm)              — Memory Protection Unit
  → _cacheEnable_ (HL_sys_core.asm)         — enable instruction/data cache
  → __TI_auto_init (libc.a)                 — C runtime (.data copy, .bss zero)
  → main() (sc_main.c)                      — SC application
```

## LED Diagnostics (without UART)

| Observation | Meaning |
|-------------|---------|
| No LEDs at all | Binary not executing — check `--be32` flag |
| Green user LEDs ON | `main()` reached, SCI init done |
| Green LEDs blinking (reset loop) | Startup self-test failing, watchdog resets |
| GIOA[4] blink pattern (N blinks + pause) | BIST failed at step N (1-7) |

## Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| No output after flash | Missing `--be32` | Add `-Wl,--be32` to linker flags |
| Undefined `__TI_auto_init` | `libc.a` not linked or wrong order | Put `-Wl,-llibc.a` after object files |
| Duplicate symbol errors | HALCoGen source conflicts | Exclude `HL_gio.c`, `HL_rti.c`, `HL_sci.c`, `HL_notification.c` |
| Globals have garbage values | Missing `--rom_model` | Add `-Wl,--rom_model` |
