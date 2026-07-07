# Lessons Learned — TMS570 Hardware Bring-up

## 2026-03-14 — ISR must use interrupt attribute for VIM dispatch

**Context:** TMS570 VIM dispatches ISRs by loading the handler address into IRQVECREG. The CPU jumps directly to the ISR via `ldr pc, [pc, #-0x1b0]` in IRQ mode. The ISR is responsible for its own prologue/epilogue.

**Mistake:** Declared `bringup_rti_compare0_isr()` as a regular `static void` function. Compiler generated `bx lr` return. In IRQ mode, this doesn't restore CPSR from SPSR and uses wrong return address (should be `lr - 4` due to ARM pipeline). CPU crashes on first IRQ return.

**Fix:** Use `void __attribute__((interrupt("IRQ")))` (tiarmclang) or `#pragma INTERRUPT(fn, IRQ)` (TI CGT). This makes the compiler generate `subs pc, lr, #4` which atomically restores CPSR from SPSR_irq and adjusts the return address.

**Principle:** On ARM Cortex-R, any function used as a VIM ISR entry point MUST have the interrupt attribute. Regular `bx lr` is never correct for IRQ/FIQ return. HALCoGen uses `#pragma INTERRUPT` — follow the same pattern.

## 2026-03-14 — systemInit() resets SCI peripheral, killing UART

**Context:** `_c_int00()` (HALCoGen startup) calls `systemInit()` before main. `sc_main.c` called `sc_sci_init()` first (to print boot messages), then `systemInit()` again (redundantly).

**Mistake:** The second `systemInit()` call resets peripheral registers including SCI, destroying the baud rate configuration. All UART output after that point was silently lost. Appeared as "UART stops after boot dump" — no garbled data, just silence.

**Fix:** Re-initialize SCI after `systemInit()` with a second `sc_sci_init()` call. Both calls are needed: first for early boot diagnostics, second to restore after peripheral reset.

**Principle:** If `systemInit()` is called after peripheral init, assume all peripherals need re-initialization. Check for silent failures — UART TX with wrong config produces no output (not garbled), making the failure invisible.

## 2026-03-14 — CPSR mask must preserve I/F bits during task launch

**Context:** First-task launch on TMS570. Need to set CPSR to System mode (0x1F) via MSR CPSR_cxsf before branching to task entry. Target CPSR computed from current CPSR by replacing mode bits.

**Mistake:** Used `targetCpsr = (currentCpsr & ~0xFFu) | 0x1Fu` — masking the full low byte zeros I (bit 7) and F (bit 6) bits, enabling both IRQ and FIQ. A pending FIQ fires immediately after the MSR, before BX executes. CPU ends up in FIQ mode (0x11) instead of System mode (0x1F).

**Fix:** Use `targetCpsr = (currentCpsr & ~0x1Fu) | 0x1Fu` — mask only the 5 mode bits [4:0]. This preserves I=1, F=1 (interrupts disabled), T=0 (ARM mode), and A (abort disable) through the mode switch.

**Principle:** When computing a target CPSR from the current one, only mask the bits you intend to change. The mode field is bits [4:0] (mask 0x1F). Never mask the full low byte (0xFF) unless you explicitly set I, F, T, and A bits in the replacement value.

## 2026-03-14 — System mode has no SPSR — exception return is UNPREDICTABLE

**Context:** Tried to use `LDMIA sp!, {r0-r12, lr, pc}^` (ARM exception return) to launch the first task from a synthetic frame. HALCoGen's `_c_int00` runs main() in System mode (0x1F).

**Mistake:** Exception return copies SPSR → CPSR. System mode (and User mode) have no SPSR — the behavior is UNPREDICTABLE per ARM ARM. Three attempts (direct LDMIA ^, CPS-based mode switch to SVC, MSR CPSR_c with computed value) all produced wrong results.

**Fix:** Use direct `MSR CPSR_cxsf, R1` + `BX R3` — no exception return needed. Set SP, zero registers, branch to entry. The bootstrap model's `OS_PORT_TMS570_INITIAL_CPSR = 0x13` (SVC mode) needs updating to 0x1F (System mode).

**Principle:** On Cortex-R5 with HALCoGen, assume main() runs in System mode. First-task launch cannot use exception return from the calling context. Use direct MSR + branch instead. Reserve exception return for ISR exit paths where the CPU is already in an exception mode (IRQ/FIQ/SVC).

## 2026-03-14 — IRQ preemption: mode switches preserve SPSR and unbanked registers

**Context:** Implementing IRQ-driven preemption on TMS570. The naked ISR needs to switch from IRQ mode to System mode (to access the interrupted task's SP/LR), do a cooperative context switch to a different task, then switch back to IRQ mode for exception return.

**Key insight 1:** R4-R11 are NOT banked between System and IRQ modes — they are the same physical registers. When the ISR fires, R4-R11 hold the interrupted task's values. The cooperative switch (in System mode) saves/restores them via a context structure. After switching back to IRQ mode, they're still correct.

**Key insight 2:** SPSR_irq is preserved across MSR CPSR_c mode switches. Switching to System mode only writes CPSR, not SPSR. System mode has no SPSR, so nothing can corrupt it. When the ISR switches back to IRQ mode, SPSR_irq still contains the original interrupted CPSR. The final `LDMIA sp!, {r0-r3, r12, pc}^` correctly restores the interrupted task's processor state.

**Key insight 3:** LR_sys (Task A's link register) must be explicitly saved/restored across the cooperative switch because BLX clobbers LR. PUSH/POP {lr} on Task A's System stack handles this — the cooperative switch saves the modified SP, and the restore returns it to the pre-push state.

**Pattern:** `SUB lr,#4 → STMDB {r0-r3,r12,lr} → [ack+check] → MSR CPSR_c 0x9F → PUSH {lr} → BLX switch_context → POP {lr} → MSR CPSR_c 0x92 → LDMIA {r0-r3,r12,pc}^`. This is the ThreadX preemption pattern (tx_thread_context_restore.S lines 166-229) without the scheduler.

## 2026-03-14 — ISR preemption path must not fire when idle (no current task)

**Context:** Phase 4 OSEK integration. RTI ISR calls `Os_BootstrapProcessCounterTick()` → alarm expires → task becomes READY → ISR's preemption path tries to do a cooperative context switch. The `StartOS()` idle loop is running in main() context (not an OS task).

**Mistake:** `RtiTickServiceCore()` unconditionally signaled preemption when `Os_BootstrapProcessCounterTick()` returned TRUE. The assembly preemption path called `GetPendingSaveCoopCtx()` which returned NULL (no current task — `os_tgt_current_task == 0xFF`). Assembly then called `SwitchContextAsm(NULL, restoreCtx)` — `STR r4, [r0, #0]` wrote to address 0x00000000 (flash). CPU crashed or entered unpredictable state.

**Fix:** Guard `os_tgt_switch_pending = TRUE` with `(os_tgt_current_task < TARGET_MAX_TASKS)`. When idle, the ISR returns normally via `LDMIA {r0-r3,r12,pc}^`, and the `StartOS()` idle loop's `os_dispatch_one()` picks up the READY task.

**Principle:** In a run-to-completion model with alarm-driven activation, the ISR preemption path must only fire when an OS task is actually running. The idle loop is NOT an OS task — it's bare main() context with no save context. Always check for a valid current task before requesting a cooperative context switch from an ISR.

## 2026-03-14 — ESM now_SR1 bit 21 is DCC1, not lockstep — waiver HIL-PF-008 was misdiagnosed

**Context:** SC ESM lockstep monitoring was disabled via `#ifdef SC_ESM_ENABLED` (waiver HIL-PF-008) because "CCM-R5F asserts a persistent ESM Group 2 error causing SC_ESM_Init() to enter an infinite ISR loop." Runtime UART register dumps showed `now_SR1=0x00200000`.

**Mistake:** Assumed `now_SR1=0x00200000` was a lockstep error. It's actually ESM Group 1 Channel 21 = DCC1 (Dual Clock Comparator 1), a clock validation module — NOT Channel 2 (CCM-R5F lockstep). All lockstep registers (CCMSR1-4, ESM SR3) were clean at runtime. The original lockstep error was transient (caused by JTAG debug reset desync) and was already being cleared by `esmGroup3Notification()` during startup.

**Fix:** Made `SC_ESM_Init()` defensive: (1) clear residual Group 1 flags for ch2 (lockstep) and ch21 (DCC1) before enabling, (2) verify ch2 is clean — if persistent, latch error without enabling EEPAPR1, (3) set a runtime mode flag so `esmGroup3Notification()` triggers `SC_ESM_HighLevelInterrupt()` (relay off + halt) for runtime errors instead of clear-and-continue. Removed `#ifdef SC_ESM_ENABLED` guard.

**Principle:** Always decode ESM channel numbers to their peripheral source before diagnosing. `0x00200000` = bit 21, not bit 2. The TMS570 ESM has 128+ channels across 3 groups — DCC, CCM, ADC, and other peripherals share the same status registers. Use the device TRM ESM channel mapping table, not assumptions.

## 2026-03-14 — GIO DIN readback unreliable on TMS570 LaunchPad (N2HET muxing)

**Context:** Implementing Phase 2 self-tests — `hw_gpio_readback_test` and `hw_watchdog_test` needed to verify GIO pins can be driven and read back.

**Mistake:** Used `gioGetBit()` (reads DIN register) to verify output pin state after `gioSetBit()` (writes DSET/DCLR). GIOB[6:7] (user LEDs) and GIOA[0:5] (relay/LED/WDI) all read incorrect values from DIN. DINB=0xF0 regardless of DOUTB state. On the LAUNCHXL2-570LC43, these pins are routed through N2HET pads — GIO DOUT drives the pin correctly (LEDs toggle), but DIN reads the pad state from the N2HET input path, not the GIO output latch.

**Fix:** Read DOUT register instead of DIN for output pin verification. DOUT reflects what was written via DSET/DCLR and confirms the GIO register bus and SET/CLR logic are functional. Production PCB with direct GIO pins will support true DIN readback.

**Principle:** On TMS570 with multiplexed pads (GIO vs N2HET vs SPI etc.), DIN reads the physical pad, not the GIO output latch. If a pad is muxed as GIO for output, the output works, but the input path may be routed to a different peripheral's input register. Always verify DIN readback on the actual hardware before relying on it in self-tests.
