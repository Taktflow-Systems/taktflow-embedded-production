# Lessons Learned — Embedded Bringup & HIL Testing

Session-level lessons from firmware bringup, HIL test automation, and platform porting.

## 2026-03-13 — HIL test recovery: ECU Reset restarts latch counters, natural recovery is faster

**Context**: HIL test suite on Pi bench — 26 scenarios running sequentially. Fault injection tests (steering, brake, overcurrent) put CVC into SAFE_STOP. Subsequent tests need CVC in RUN. Initially tried UDS ECU Reset (0x11) to recover.
**Mistake**: ECU Reset via BswM_RequestMode triggers module reinit but restarts the 9-latch recovery counter (300 cycles × 9 latches = 27s each time). Sending ECU Reset during latch clearing resets the counter, making recovery take >130s instead of ~35s. Also: FZC has no UDS CanIf routing (0x7DF not in RX config), so broadcast ECU Reset doesn't reach FZC — its fault latches persist independently.
**Fix**: Removed ECU Reset from pre-scenario recovery. Instead: (1) MQTT reset to clear plant-sim faults, (2) wait up to 60s for natural CVC latch recovery, (3) ECU Reset only as fallback if still stuck after 60s. Natural recovery clears all latches in ~35s (parallel clearing with clean signals).
**Principle**: Don't fight the safety controller's recovery mechanism — work with it. Sending reset commands during latch clearing restarts the timer. Wait for natural recovery first, intervene only as a last resort. Also: verify UDS routing exists on ALL physical ECUs — broadcast 0x7DF only reaches ECUs with CanIf RX entries for it.

## 2026-03-13 — Plant-sim _active_dtcs guard causes DTC deduplication across scenarios

**Context**: HIL-034 (motor overtemp) failed because DTC 0xE302 wasn't broadcast on CAN 0x500, despite plant-sim correctly detecting overtemp.
**Mistake**: Plant-sim's `_send_dtc()` guards with `if dtc_code in self._active_dtcs: return` — fires once per DTC code then suppresses. Between HIL scenarios, if a previous test triggered the same DTC (or manual fault injection did), `_active_dtcs` was never cleared. The "new" DTC was already in the set.
**Fix**: Added `reset_mqtt_faults()` call at the start of each scenario to clear `_active_dtcs`. Also fixed `motor_model.reset_faults()` to reset `temp_c` to ambient — without this, residual temp_c=120 from overtemp injection persisted through reset and immediately re-triggered overtemp.
**Principle**: When physics models use once-per-event guards (deduplication sets), the guard must be cleared between test scenarios. Also: `reset_faults()` must reset ALL physical state, not just boolean flags — residual continuous values (temperature, current) can re-trigger the fault condition immediately after reset.

## 2026-03-13 — RZC overcurrent flag in wrong CAN frame for CVC detection

**Context**: HIL-030 overcurrent injection — DTC 0xE301 broadcast correctly, but CVC stayed in RUN instead of transitioning to DEGRADED/SAFE_STOP.
**Mistake**: RZC puts `OvercurrentFlag` in Motor_Current (0x301) byte[4] bit 2, but CVC reads `motor_fault_rzc` from Motor_Status (0x300) byte[7]. The `RZC_SIG_MOTOR_FAULT` Rte signal isn't linked to overcurrent detection — only set by other fault paths. CVC never sees the overcurrent.
**Fix**: (Scenario) Changed HIL-030 verdict to check DTC + Motor_Current frame presence instead of vehicle_state. Tracked as firmware gap RZC-OC-001: RZC should set motor_fault in 0x300 when overcurrent is detected.
**Principle**: When fault detection and fault reporting use different CAN signals/frames, verify the complete data path: sensor → SWC → Rte signal → Com TX → CAN frame → receiving ECU's Com RX → Rte read → state machine. A DTC broadcast (0x500) is for logging/cloud — the CVC state machine reads fault status from dedicated signal frames, not DTC broadcasts.

## 2026-03-13 — TMS570 --be32 linker flag silently missing from Makefile.tms570

**Context**: SC (TMS570) E2E validation persistently failed on HIL bench, killing the relay and preventing RUN state. Implemented SC_E2E_BYPASS to unblock, but new firmware appeared not to take effect after DSLite flash — alive counter never reset, debug marker absent.
**Mistake**: `firmware/Makefile.tms570` was missing `-Wl,--be32` in LDFLAGS. The original `firmware/sc/Makefile` had it, but the newer Makefile.tms570 (used by the standard build) omitted it. Without `--be32`, the linker produces BE-8 format but TMS570 boots in BE-32 mode. DSLite reports "Success" because it programs the flash correctly — but the CPU fetches instructions with wrong byte order and executes garbage.
**Fix**: Added `-Wl,--be32` to Makefile.tms570 LDFLAGS. Clean rebuild + reflash resolved immediately — debug marker appeared, relay energized, all ECUs healthy.
**Principle**: When a flash tool reports success but firmware behavior doesn't change, suspect the binary format, not the flash tool. For big-endian ARM targets (TMS570), always verify `--be32` is in the linker flags — it's a linker flag, not a debugger/flash flag. Keep both Makefiles for the same target in sync.

## 2026-03-14 — STM32 Startup ASM Missing C++ Constructors (`__libc_init_array`)

**Context**: OpenBSW STM32 platform port — custom startup_stm32g474xx.s written from scratch for Eclipse OpenBSW contribution.

**Mistake**: Wrote minimal 70-line startup ASM from scratch instead of using ST's full CMSIS template. Two critical omissions:
1. Missing `bl __libc_init_array` — the GCC ARM C runtime function that iterates the `.init_array` section to call C++ static constructors before `main()`.
2. Incomplete vector table — only 41 of 102 interrupt vectors defined. Any interrupt beyond position 40 would jump to address 0x00000000 (reset vector address space), causing an immediate HardFault.

**Symptom**: `Uart::init()` HardFaulted with CFSR=0x00000400 (INVSTATE) at PC=0x00000000. The BSP declares `static Uart instances[] = { Uart(Uart::Id::TERMINAL) }` which requires constructor calls before main(). Without `__libc_init_array`, the `_uartConfig` member reference was null — dereferencing it branched to address 0.

**Debugging approach**: Built progressively deeper test harnesses:
1. Simple `uart_puts()` trace → confirmed USART2 works in SystemInit
2. Per-subsystem tests (PLL, RCC, GPIO, DWT) → all passed, narrowed to `staticBsp.init()`
3. HardFault handler with PC/LR/CFSR dump on USART2 → pinpointed exact fault location
4. `arm-none-eabi-nm` + disassembly → confirmed `Uart::init()` loads `_uartConfig` from `this` pointer
5. S32K148 reference analysis → confirmed its startUp.S has `bl __libc_init_array` as step 14 of 15

**Root cause attribution**: Our mistake, not OpenBSW's. OpenBSW assumes a correct startup file is provided by the platform port. The S32K148 platform (NXP-derived) had it. ST's official CMSIS startup templates have it. We wrote ours from scratch and missed it.

**Fix**:
1. Added `bl __libc_init_array` between `bl SystemInit` and `bl main` in Reset_Handler
2. Completed STM32G474RE vector table — all 102 entries from CMSIS `stm32g474xx.h` IRQn_Type enum, with reserved slot at position 85
3. Completed STM32F413ZH vector table — all 102 entries from CMSIS `stm32f413xx.h`, with reserved slots at positions 48, 78-79, 86, 90-91, 93-94
4. Used DeepSeek MCP for second-opinion review before shipping

**Principle**: Never write platform-critical code (startup ASM, linker scripts) from scratch. Use vendor reference as baseline, verify against datasheet/reference manual, and get a second-opinion review before shipping. The startup sequence for any C++ embedded project MUST include: (1) SP init, (2) .data copy, (3) .bss zero, (4) SystemInit, (5) `__libc_init_array`, (6) main. Missing step 5 silently breaks all C++ static initialization. This is contribution-grade code for OEM engineers — quality over speed always.

## 2026-03-14 — Incomplete vector table causes silent misdirected interrupts

**Context**: Same startup ASM rewrite as above. Original file had 41 vector entries for a chip with 102 interrupt sources.

**Mistake**: Vector table was truncated after EXTI15_10 (position 40). Any peripheral interrupt with IRQn > 40 (e.g., UART4=52, DMA2=56-60, CAN2=63-66, FPU=81) would read the next word in flash as the handler address. Depending on what the linker placed after the vector table, this could:
- Jump to an unrelated function (silent corruption)
- Jump to 0x00000000 (HardFault)
- Jump to erased flash 0xFFFFFFFF (HardFault)

**Fix**: Both startup files now have complete vector tables:
- STM32G474RE: 102 entries (IRQn 0-101), reserved at position 85, matching RM0440 Table 97
- STM32F413ZH: 102 entries (IRQn 0-101), reserved at positions 48, 78-79, 86, 90-91, 93-94, matching RM0430 Table 40
- Every entry has a corresponding `.weak` / `.thumb_set` alias to `Default_Handler` (infinite loop)

**Verification method**: Cross-reference each position against the CMSIS device header's `IRQn_Type` enum. Count entries manually. Gaps (reserved positions) must be `.word 0`. Handler names must match CMSIS convention exactly (e.g., F4 uses `DMA1_Stream0` not `DMA1_Channel1`, F4 uses `CAN1_TX` not `FDCAN1_IT0`).

**Principle**: A vector table is not "good enough" with partial entries. Every interrupt source on the silicon must have a vector — even if unused, it must point to Default_Handler. An incomplete vector table is a latent HardFault waiting for any peripheral to fire an interrupt. Count your vectors against the datasheet.

## 2026-03-14 — Systematic HardFault debugging on bare-metal STM32

**Context**: OpenBSW reference app HardFaulted immediately on boot. No debugger connected (ST-LINK used only for flash). Serial output was only diagnostic channel.

**Approach that worked**:
1. **Early UART in SystemInit** — Configure USART2 before any C++ code runs (in `SystemInit()`, called from ASM before `__libc_init_array`). This gives a diagnostic channel even if constructors crash.
2. **HardFault handler that dumps to UART** — Extract faulting PC, LR, and CFSR from the exception stack frame using inline ASM. Print hex values on USART2. Key registers:
   - `CFSR` (0xE000ED28): tells you fault type (INVSTATE=0x400, IBUSERR=0x100, etc.)
   - `PC`: where the fault occurred (0x00000000 = null pointer / missing constructor)
   - `LR`: caller of the faulting function (EXC_RETURN values like 0xFFFFFFF9 = MSP thread mode)
3. **Progressive test harness** — Test subsystems individually before combining: clock→GPIO→UART→DWT→BSP→lifecycle. First failure point is the root cause.
4. **`arm-none-eabi-nm` for symbol lookup** — When HardFault gives you a PC address, look it up in the symbol table to identify which function faulted.

**Principle**: On bare-metal without a debugger, your only diagnostic is what you can print before the crash. Configure UART as early as possible (in SystemInit, before constructors), and always implement a HardFault handler that dumps fault registers. Never deploy firmware without a HardFault handler — even "hello world" level code.

## 2026-03-19 — E-Stop 0x001 never on CAN: 4 bugs in the injection chain

**Context**: SIL E-Stop test (SIL-003). UDP fault-inject sends E-Stop command to CVC. CVC should detect via DIO, transition to SAFE_STOP, and broadcast CAN 0x001. Vehicle went to SAFE_STOP (local state) but 0x001 never appeared on the bus.

**Mistakes (4 layered bugs)**:
1. **SPI UDP socket never drained** — `IoHwAb_ReadPedalAngle` on POSIX reads from injected sensor values, not SPI. Nobody called `Spi_Hw_Transmit`, so the UDP socket that receives pedal/E-Stop commands was never read.
2. **DIO channel mismatch** — `IoHwAb_Inject_SetDigitalPin(IOHWAB_PIN_ESTOP=0, HIGH)` wrote DIO channel 0. But `IoHwAb_ReadEStop` reads `Dio_ReadChannel(config->EStopDioChannel=5)`. Wrong channel.
3. **Dual static config pointer** — `IoHwAb.c` and `IoHwAb_Posix.c` both define `static iohwab_config`. `IoHwAb_Init` (from IoHwAb.c) only sets its own copy. The Posix copy stays NULL, so the config-guarded Dio write was skipped.
4. **Stale CAN injection resets DIO every cycle** — `Swc_CvcCom_BridgeRxToRte()` called `Com_ReceiveSignal(19u)` (dev-repo leftover, always returns 0), then `CvcCom_Hw_InjectEstop(0)` which wrote DIO channel 5 = LOW every 10ms. The UDP-injected HIGH was overwritten within one cycle.

**Fixes**:
1. Added `Spi_Hw_PollUdp()` called from CVC main loop every 10ms — drains UDP, writes pedal angles to IoHwAb sensor values.
2. Hardcoded DIO channel 5 for E-Stop in `IoHwAb_Inject_SetDigitalPin` (bypasses NULL config).
3. Same as #2 — removed config guard.
4. Removed the broken `Com_ReceiveSignal(19u)` → `CvcCom_Hw_InjectEstop` path entirely.

**Additional fix — broadcast pattern**:
- Old: 4 fire-and-forget sends then silent. Other ECUs could miss E-Stop if frames lost or if they reset after activation.
- New: Cyclic broadcast every 10ms while latched (IfActive pattern). Industry standard per ISO 26262 FTTI compliance — CAN has no application-layer ACK, cyclic is the only way to guarantee all nodes receive.

**Principles**:
1. **When two .c files both define `static X`, the linker picks one — the other is silently ignored.** If both files are linked, functions in file A use A's static, functions in file B use B's. This is correct C behavior but creates hidden state splits. Never link two files that both implement the same API with separate static state.
2. **Every "temporary" dev hack that writes to shared state will eventually conflict with production code.** The `Com_ReceiveSignal(19u)` was harmless when it was the only E-Stop path. Once the real DIO path was added, the old hack silently undid every E-Stop activation.
3. **Safety-critical signals must be cyclic, not event-only.** Fire-and-forget violates the FTTI contract — recovering nodes, late-joining testers, and bus-off recovery all need to see the current state. IfActive (cyclic while latched) is the industry standard.
4. **Test the full chain, not just the endpoint.** SIL-003 was "passing" by checking only CVC's local state (0x100 = SAFE_STOP). The real safety chain requires 0x001 on the bus. A test that doesn't check the broadcast is a false pass.
5. **On POSIX/SIL, verify the hardware abstraction actually connects.** The IoHwAb, DIO, and SPI layers form a 4-layer abstraction on POSIX. A break at any layer is invisible unless you trace the value end-to-end (DIO readback trace was the key diagnostic).
