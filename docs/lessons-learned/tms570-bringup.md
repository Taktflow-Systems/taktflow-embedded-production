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
