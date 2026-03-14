# TMS570 Session Handoff

Updated: 2026-03-14

Purpose:

- preserve the exact flow of the TMS570 bootstrap port work
- show what is already done
- show what is still only `model-tested` / `build-tested`
- give the next session a concrete implementation order

## Ground Rules

- Keep `OSEK/AUTOSAR` semantics as the source of truth.
- Use `ThreadX` only as a low-level reference for interrupt/context mechanics.
- Keep test files small for later `MC/DC`.
- If there is doubt about low-level behavior, re-read the exact local ThreadX file first.
- Do not treat the current TMS570 work as target-verified.

## Exact Local References In Use

Local vendor ThreadX tree:

- `d:\workspace_ccstheia\taktflow-embedded-production\private\vendor\threadx-master`

Current interrupt/context references used repeatedly:

- `ports/cortex_r5/gnu/src/tx_thread_context_save.S`
- `ports/cortex_r5/gnu/src/tx_thread_context_restore.S`
- `ports/cortex_r5/gnu/src/tx_thread_irq_nesting_start.S`
- `ports/cortex_r5/gnu/src/tx_thread_irq_nesting_end.S`
- `ports/cortex_r5/gnu/src/tx_thread_fiq_context_save.S`
- `ports/cortex_r5/gnu/src/tx_thread_fiq_context_restore.S`
- `ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_start.S`
- `ports/cortex_r5/gnu/src/tx_thread_fiq_nesting_end.S`
- `ports/cortex_r5/gnu/src/tx_timer_interrupt.S`

Older bootstrap slices were initially cross-checked against the matching
`ports/arm11/gnu/src/*` files before this local vendor tree was tucked into the
repo. From the next session onward, prefer the local `cortex_r5` files first.

Current HALCoGen/TI references used repeatedly:

- `firmware/ecu/sc/halcogen/source/HL_sys_vim.c`
- `firmware/ecu/sc/halcogen/include/HL_reg_vim.h`
- `firmware/ecu/sc/halcogen/source/HL_rti.c`

## What We Have Done

Main runtime files:

- `firmware/platform/tms570/include/Os_Port_Tms570.h`
- `firmware/platform/tms570/src/Os_Port_Tms570.c`
- `firmware/platform/tms570/src/Os_Port_Tms570_Asm.S`

Main test registrars:

- `firmware/bsw/os/bootstrap/test/test_Os_Port_Tms570_bootstrap_core.c`
- `firmware/bsw/os/bootstrap/test/test_Os_Port_Tms570_bootstrap_irq.c`
- `firmware/bsw/os/bootstrap/test/test_Os_Port_Tms570_bootstrap_fiq.c`
- `firmware/bsw/os/bootstrap/test/test_Os_Port_Tms570_bootstrap_integration.c`

What is already modeled:

1. First-task bootstrap frame and first-task start flow.
2. IRQ save/restore lifecycle with nested IRQ handling.
3. FIQ save/restore lifecycle kept separate from IRQ-return dispatch.
4. Explicit initial-frame versus runtime-frame ownership for prepared tasks, including runtime IRQ return-address capture, restore-frame metadata helpers, direct restore-block helpers for task-lower, scratch, preserved, and VFP snapshots, the first saved task-lower register block, the first saved IRQ scratch-register snapshot, dynamic saved CPSR capture, an expanded task-upper block that now reaches `R4-R12`, explicit saved task `LR` ownership, optional VFPv3-D16 runtime state, selected-task restore-side application on task switch, narrowed resume-current restore behavior for IRQ/FIQ, explicit interrupt-versus-solicited frame-byte footprints, saved-frame `SP` commit from the live running `SP`, the first matching FIQ runtime-metadata capture/resume path, explicit IRQ interrupt-context byte tracking with the exact local Cortex-R5 `32/32/0` save-shape split for running-thread, nested, and idle-system save paths, per-level IRQ saved `SPSR/CPSR` ownership in LIFO order instead of one flat live slot, per-level IRQ scratch-register ownership in LIFO order instead of one flat live scratch placeholder, matching stacked FIQ `SPSR/CPSR` plus scratch ownership, the exact minimal-FIQ `r0-r3`-only scratch restore split for `24-byte` first-entry frames, a fixed runtime-frame setter rule that preserves valid solicited `StackType = 0` across later field updates, coherent packed saved/restored IRQ/FIQ interrupt-context views plus asm-facing peek wrappers for the interrupt-context seam, save-side runtime-frame metadata refresh from the packed saved interrupt context object itself, switch-away capture that now prefers the packed restored IRQ/FIQ context over later mutated live restore-side fields, resume-current behavior that now prefers the packed restored IRQ/FIQ context for interrupt-state fields over corrupted saved-frame metadata, packed saved/restored task-frame views with `Frame + FrameBytes` payloads for the task-context seam, unified pending save/restore task-frame views plus unified pending save/restore interrupt-context views for the live save/restore windows, explicit restore-in-progress ownership so those pending views exist only during the live restore window, explicit save-in-progress ownership so the save seam can expose truthful in-flight save objects too, pending effective IRQ/FIQ save-context/save-task-frame views, direct pending-save scalar getters for the asm seam, explicit solicited-system-return begin/finish ownership with a truthful pending solicited save-task-frame view during the live save window, the new frozen solicited-save snapshot behavior so `FinishSolicitedSystemReturn()` consumes one fixed frame object instead of later-mutated live state, explicit first-task-start begin/finish ownership with a truthful pending first-task frame view during the live launch window, the removal of the old dedicated prepared-`SP` first-task shortcut in favor of the generic restore seam, the start-path use of generic restore `StackType/CPSR` metadata instead of the old synthetic-frame header shortcut, the new explicit IRQ idle-system restore branch, the new explicit IRQ switch-task scheduler-return marker plus asm-side scheduler-placeholder return branch for non-resume IRQ restore paths, the new explicit `IrqSchedulerReturnInProgress` window plus a truthful pending scheduler-return task-frame view, the post-`Os_PortExitIsr2()` upgrade path that can turn an originally computed IRQ `resume current` into a real `switch task` once the selected prepared task becomes known before restore finishes, and the matching explicit `FiqSchedulerReturnInProgress` window plus truthful pending FIQ scheduler-return task-frame view for the local `preempt scheduler` branch, including the new asm-side split between ordinary exception return and scheduler-return placeholder flow.
5. Selected-next-task and deferred dispatch bootstrap model, including the new post-exit IRQ upgrade path that keeps ISR/tick-triggered handoff timing aligned with the local Cortex-R5 restore shape without collapsing back into one opaque restore helper, plus the new FIQ scheduler-return completion paths through both the shared configured-dispatch completion helper and the kernel-dispatch observe hook.
6. Time-slice bookkeeping and timer-side service seam.
7. RTI compare0 source model with notification gate, counter gate, pending flag, acknowledge, and periodic compare update.
8. VIM request path with:
   - request mapping via `CHANCTRL`
   - pending request latch/resync
   - active `IRQINDEX`
   - active vector
   - VIM RAM-style ISR table slot
   - `REQMASKCLR/REQMASKSET` pulse
   - mapped-vector invocation
   - wrapper/core split around IRQ entry
9. Solicited system-return modeling from the local `tx_thread_system_return.S` path, including minimal `StackType = 0` runtime-frame capture, cleared live task ownership, and a task-switch restore path that now honors the interrupt-versus-solicited frame split from the local `tx_thread_schedule.S` logic.
10. Save-time full task register capture: `FinishIrqContextSave` (CAPTURE_CURRENT) and `FinishFiqContextSave` (FIRST_ENTRY) now capture lower (R0-R3), preserved (R4-R12), LR, and VFP into the task RuntimeFrame at save-finish time instead of deferring to scheduler-return. Scheduler-return no longer re-captures these — it only updates interrupt metadata from the restored context, commits time slice, and commits SP. RuntimeFrame is now the single authoritative source of captured task state right after save.
12. HALCoGen bring-up glue: 7 bridge functions (`HalVimInit`, `HalVimMapTickChannel`, `HalVimEnableChannel`, `HalVimDisableChannel`, `HalRtiInit`, `HalRtiStart`, `HalRtiAcknowledgeCompare0`) with `UNIT_TEST` model paths and `TODO:HARDWARE` target stubs. Full lifecycle tested: VIM init → map → enable → RTI init → start → ack.
11. Phase 1 completion: restore-side `PeekRestore*` functions read directly from the target task's RuntimeFrame (via `PeekRestoreTaskFrame()`). Flat state variables (`CurrentTaskLower`, `CurrentTaskPreserved`, `CurrentTaskVfp`, `CurrentTaskLinkRegister`) serve as the live CPU register file — populated during restore, consumed during the next save. Round-trip integration tests verify: (a) `PeekRestoreTaskFrame` returns the target task's RuntimeFrame during switch, (b) scheduler-return does not re-capture lower/preserved/LR/VFP (save-time capture is authoritative), (c) FIQ scheduler-return preserves save-time register values.

Current VIM/RTI runtime seam now looks like this:

1. RTI source becomes pending.
2. VIM request is latched/resynchronized.
3. `Os_Port_Tms570_ReadMappedChannelForRequest()` decodes `CHANCTRL`.
4. `Os_Port_Tms570_SelectPendingIrq()` latches `IRQINDEX` and active vector.
5. `Os_Port_Tms570_ReadActiveIrqChannel()` decodes `IRQINDEX - 1U`.
6. `Os_Port_Tms570_ReadActiveIrqVector()` reads the mapped handler from the VIM RAM-style table.
7. `Os_Port_Tms570_PulseActiveIrqMask()` pulses `REQMASKCLR/REQMASKSET`.
8. `Os_Port_Tms570_ServiceActiveIrqChannelCore()` dispatches by decoded channel plus mapped vector.
9. `Os_Port_Tms570_InvokeActiveIrqVectorCore()` reuses that channel-driven service seam.
10. The Cortex-R5 bootstrap `.S` file now exposes thin asm-facing labels for mapped-channel read, active-vector read, mask pulse, channel-driven service, mapped-vector invoke, and VIM entry/service flow.
11. `Os_Port_Tms570_VimIrqEntryCore()` owns core entry behavior.
12. `Os_Port_Tms570_VimIrqEntry()` owns the IRQ wrapper.

Why this matters:

- the path is no longer one large helper
- the model now resembles the HALCoGen dispatcher shape much more closely
- future real vector glue can replace seams one by one instead of rewriting everything

## Current Evidence

Current status:

- `Model-tested`: yes
- `Build-tested`: yes
- `Spec-backed`: partly
- `Target-verified`: partial (RTI compare0 IRQ — PASS, first-task launch — PASS, 2026-03-14)

Current passing checks:

- TMS570 split bootstrap suite: `210 tests, 0 failures`
- TMS570 assembly build check: passed
- shared SchM regression: `11 tests, 0 failures`

Exact verification commands used recently:

```powershell
$root='d:\workspace_ccstheia\taktflow-embedded-production'
$out="$root\firmware\bsw\os\bootstrap\test\build\test_Os_Port_Tms570_bootstrap.exe"
$testSrc=Get-ChildItem "$root\firmware\bsw\os\bootstrap\test\test_Os_Port_Tms570_bootstrap*.c" | ForEach-Object { $_.FullName }
$bootstrapSrc=Get-ChildItem "$root\firmware\bsw\os\bootstrap\src\*.c" | ForEach-Object { $_.FullName }
$cmd=@(
  'gcc','-Wall','-Wextra','-Werror','-std=c99','-pedantic','-g',
  '-DUNIT_TEST','-DPLATFORM_TMS570',
  "-I$root\firmware\bsw\include",
  "-I$root\firmware\bsw\services\Det\include",
  "-I$root\firmware\ecu\sc\halcogen\include",
  "-I$root\firmware\bsw\os\bootstrap\include",
  "-I$root\firmware\bsw\os\bootstrap\port\include",
  "-I$root\firmware\bsw\os\bootstrap\src",
  "-I$root\firmware\platform\tms570\include",
  "-I$root\firmware\lib\vendor\unity",
  "$root\firmware\lib\vendor\unity\unity.c"
) + $testSrc + $bootstrapSrc + @(
  "$root\firmware\bsw\os\bootstrap\port\src\Os_Port_TaskBinding.c",
  "$root\firmware\platform\tms570\src\Os_Port_Tms570.c",
  "$root\firmware\bsw\services\Det\src\Det.c",
  '-o', $out
)
& $cmd[0] $cmd[1..($cmd.Length-1)]
& $out
```

```powershell
arm-none-eabi-gcc -mcpu=cortex-r5 -marm -c `
  d:\workspace_ccstheia\taktflow-embedded-production\firmware\platform\tms570\src\Os_Port_Tms570_Asm.S `
  -o d:\workspace_ccstheia\taktflow-embedded-production\firmware\bsw\os\bootstrap\test\build\Os_Port_Tms570_Asm.o
```

```powershell
cd d:\workspace_ccstheia\taktflow-embedded-production\firmware\bsw
make test-SchM_asild
```

## What Is Still Not Done

These are still missing in the real sense:

1. Real TMS570 VIM register programming on target.
2. Real vector table / VIM RAM hookup on target.
3. Real RTI hardware interrupt firing on target.
4. Real Cortex-R5 context save into a live task context object.
5. Real next-task restore on IRQ return.
6. Real first-task launch on hardware.
7. Real register preservation proof on target.
8. Real AUTOSAR/OSEK integration outside the bootstrap sandbox.

Do not overclaim:

- the current port is still a strong host model
- it is not yet a real target-verified port

## Next Work In Detail

Do these in order.

### 1. [DONE] Start replacing model-only context ownership with live task-context ownership

Goal:

- stop modeling context switching as only state bookkeeping

Next slices:

1. Expand the current `{InitialFrame, RuntimeFrame}` model from `SP + TimeSlice + ReturnAddress + LinkRegister + CPSR + StackType + task-lower snapshot + IRQ/FIQ scratch snapshots + task-upper block through R12 + optional VFP state + restore-applied live-state handoff` into the next real saved-register runtime context fields and then make save/restore consume even more of that object directly.
 - Keep the new packed saved IRQ/FIQ interrupt-context views aligned with the exact local Cortex-R5 save/restore files if any future asm-facing change touches `LastSavedIrqContext`, `LastRestoredIrqContext`, `LastSavedFiqContext`, or `LastRestoredFiqContext`.
 - Keep the new packed saved/restored task-frame views, restore-in-progress ownership, unified pending save/restore task-frame views, and unified pending save/restore interrupt-context views aligned with the same local Cortex-R5 save/restore split if any future asm-facing change touches the task-context seam.
 - Keep the new explicit IRQ scheduler-return window, the pending scheduler-return task-frame view, and the post-`Os_PortExitIsr2()` resume-to-switch upgrade aligned with the local `tx_thread_context_restore.S` scheduler-return branch whenever future work changes how deferred dispatch becomes a live selected-task handoff.
 - Keep the new explicit FIQ scheduler-return window, the pending FIQ scheduler-return task-frame view, and the new completion hooks aligned with the local `tx_thread_fiq_context_restore.S` scheduler-return branch whenever future work changes how that FIQ preempt branch hands control back to the scheduler model.
2. Keep the initial prepared frame immutable once a task is prepared.
3. Move IRQ save toward writing that runtime frame object directly.
5. Keep the new solicited system-return seam aligned with the local `tx_thread_system_return.S` save shape instead of falling back to manual stack-type test overrides.
4. Move restore toward reading the selected task’s runtime frame directly.
6. Keep the new IRQ interrupt-context byte tracking aligned with the exact local `tx_thread_context_save.S` shape if any future save-path change touches `LastSavedIrqContextBytes`, `LastRestoredIrqContextBytes`, or `IrqInterruptStackBytes`.
7. Keep the new stacked IRQ `SPSR/CPSR` ownership aligned with the same local save file if any future restore-path change touches `LastSavedIrqContextCpsr`, `LastRestoredIrqContextCpsr`, or the per-level IRQ restore order.
8. Keep the new stacked IRQ scratch ownership aligned with the same local save file if any future restore-path change touches `LastSavedIrqContextScratch`, `LastRestoredIrqContextScratch`, or the per-level IRQ restore order.
9. Keep the new stacked FIQ `SPSR/CPSR` and scratch ownership aligned with the local `tx_thread_fiq_context_save.S` / `tx_thread_fiq_context_restore.S` pair, especially the `24-byte` first-entry rule where `r10/r12` are not part of the saved minimal FIQ frame.
10. Keep the `StackType = 0` solicited-frame preservation fix intact in the runtime-frame setters, because `0` is a valid stack-type value in this model and must not be treated as "unset".

Done when:

- [DONE] IRQ save/restore updates a real per-task runtime context object
- [DONE] switching tasks is no longer mostly synthetic state changes

### 2. [DONE] Prepare real HALCoGen/TI bring-up glue

Goal:

- make the bootstrap seams line up with the real hardware files we will call later

Implemented bridge functions (declared in `Os_Port_Tms570.h`, implemented in `Os_Port_Tms570.c`):

1. `Os_Port_Tms570_HalVimInit()` — resets ISR table, VimReqmaskset0, VimChanctrl0, sets VimConfigured. Target: `vimInit()`.
2. `Os_Port_Tms570_HalVimMapTickChannel(VimChannel, RequestId)` — stores RtiTickHandler in ISR table slot, sets VimRtiCompare0HandlerAddress. Target: `vimChannelMap(RequestId, VimChannel, &handler)`. Rejects reserved channels 0-1 and >=96.
3. `Os_Port_Tms570_HalVimEnableChannel(VimChannel)` — sets bit in VimReqmaskset0. Target: `vimEnableInterrupt(VimChannel, SYS_IRQ)`. Rejects channel >=32.
4. `Os_Port_Tms570_HalVimDisableChannel(VimChannel)` — clears bit in VimReqmaskset0. Target: `vimDisableInterrupt(VimChannel)`. Rejects channel >=32.
5. `Os_Port_Tms570_HalRtiInit(Compare0Period)` — sets RtiCmp0Comp/Udcp, clears flags, sets RtiConfigured. Target: `rtiInit()` + `rtiSetPeriod()`.
6. `Os_Port_Tms570_HalRtiStart()` — enables counter via RtiGctrl, enables compare0 interrupt via RtiSetintena. Target: `rtiStartCounter()` + `rtiEnableNotification()`.
7. `Os_Port_Tms570_HalRtiAcknowledgeCompare0()` — calls existing `os_port_tms570_acknowledge_rti_compare0()`. Target: `rtiREG1->INTFLAG = rtiNOTIFICATION_COMPARE0`.

All functions use `#if defined(UNIT_TEST)` / `#else` branches. Target-side branches now contain real hardware calls (see Phase 3 step 1). 14 new tests in `test_Os_Port_Tms570_bootstrap_core_hal_bridge.c` covering state changes, boundary validation, and full lifecycle.

Done when:

- [DONE] the repo has a clear bridge from bootstrap seam to real HALCoGen call sites

### 3. [IN PROGRESS] Then do real target bring-up

Goal:

- move from `model-tested` to `target-verified`

Step 1 — Fill in HAL bridge target branches: [DONE]

- VIM functions (`HalVimInit`, `HalVimMapTickChannel`, `HalVimEnableChannel`, `HalVimDisableChannel`) call HALCoGen APIs directly via `#include "HL_sys_vim.h"` — `vimInit()`, `vimChannelMap()`, `vimEnableInterrupt()`, `vimDisableInterrupt()`.
- RTI functions (`HalRtiInit`, `HalRtiStart`, `HalRtiAcknowledgeCompare0`) use direct register access via `rtiREG1` from `#include "HL_reg_rti.h"` — `HL_rti.c` is excluded from the target build due to signature conflicts with `sc_hw_tms570.c`.
- HALCoGen includes are guarded by `#if !defined(UNIT_TEST)` to avoid polluting the host test build.
- Host tests still pass: 210 tests, 0 failures.
- Target build (`make -f Makefile.tms570 all`) still produces `sc.elf` — Os_Port_Tms570.c is not yet linked into the SC firmware build but all headers resolve.

Step 2 — Prove RTI compare0 fires as IRQ via VIM channel 2: [DONE — TARGET VERIFIED]

- New file: `firmware/platform/tms570/src/Os_Port_Tms570_Bringup.c`
- Standalone bring-up test: `bringup_test_rti_compare0_irq()`
  - Maps RTI compare0 (request 2) → VIM channel 2 with a minimal ISR
  - ISR increments counter + acknowledges via `rtiREG1->INTFLAG = 1`
  - Enables CPU IRQs, waits ~4.4s delay loop, reports count over SCI UART
  - Reports pass/fail, then restores polled mode
- **Hardware result**: `[BRINGUP-1] IRQ count: 442 (expect ~20)` — PASS
  - Count higher than expected because delay loop runs ~22× longer than estimated at 300 MHz (1 cycle/iteration vs assumed 3)
  - 442 IRQs at 10ms period ≈ 4.42s total delay, confirming continuous IRQ delivery
- Key fix: ISR must use `__attribute__((interrupt("IRQ")))` — regular `bx lr` crashes on first IRQ return (CPSR not restored from SPSR, return address not adjusted by 4)
- Key fix: `systemInit()` in sc_main.c resets SCI peripheral — added `sc_sci_init()` after it to restore UART output
- Call site: `sc_main.c` calls `Os_Port_Tms570_BringupAll()` between `rtiStartCounter()` and main loop, guarded by `#ifdef OS_BOOTSTRAP_BRINGUP`
- Build: `make -f firmware/platform/tms570/Makefile.tms570 BRINGUP=1 all`
- Flash: `make -f firmware/platform/tms570/Makefile.tms570 flash`
- Does NOT use Os_Port_Tms570.c bridge functions — standalone HALCoGen calls
- HALCoGen header conflict: `sc_types.h` defines `boolean` as `uint8`, HALCoGen as `bool`. Bring-up file uses HALCoGen types only, declares SCI externs manually.

Step 3 — Prove first-task launch via direct MSR+BX: [DONE — TARGET VERIFIED]

- Test: `bringup_test_first_task_launch()` in `Os_Port_Tms570_Bringup.c`
  - Builds synthetic 68-byte initial frame (17 × uint32) on 512-byte aligned stack
  - Sets PC to `bringup_first_task_entry` at frame[16]
  - Computes target CPSR from current CPSR, replacing only mode bits [4:0]
  - Launches via naked `bringup_launch_task()`: MSR CPSR_cxsf → MOV SP → zero R0-R12,LR → BX
  - Task entry reads CPSR via MRS, verifies mode = 0x1F (System), reports pass/fail
  - Enters polled RTI LED blink loop as proof-of-life
- **Hardware result**: `[BRINGUP-2] CPSR = 0x600003DF (mode = 0x0000001F)` — PASS
- Key finding: HALCoGen runs main() in System mode (0x1F), not SVC (0x13). System mode has no SPSR, so exception return (LDMIA ^) is UNPREDICTABLE. Direct MSR+BX is the correct approach.
- Key bug: CPSR mask must be `~0x1Fu` (mode bits only), NOT `~0xFFu` (full low byte). Using `~0xFFu` clears I/F bits, enabling interrupts during the launch. Pending FIQ fires immediately after MSR, leaving CPU in FIQ mode (0x11) instead of System mode.
- Implication: `OS_PORT_TMS570_INITIAL_CPSR` in the bootstrap model should be 0x1F (System), not 0x13 (SVC). The real `Os_Port_Tms570_StartFirstTaskAsm` needs the same MSR+BX approach.
- One-way trip — test 2 never returns; the task entry prints final summary and blinks LED.

Bring-up order (remaining):

1. [DONE] Prove RTI compare0 fires + VIM channel 2 routes to IRQ — TARGET VERIFIED 2026-03-14.
2. [DONE] Prove first-task launch — TARGET VERIFIED 2026-03-14.
3. Prove same-task IRQ return.
4. Prove two-task switch.
5. Prove IRQ-driven preemption.
6. Prove FIQ does not break IRQ-return ownership.

## What Not To Do Next Session

- Do not switch to STM32 before TMS570 reaches real target bring-up shape.
- Do not import ThreadX round-robin behavior into the OSEK scheduler.
- Do not collapse tests back into large files.
- Do not call the model “target-verified”.

## Best Re-entry Files

Start next session here:

1. `firmware/platform/tms570/src/Os_Port_Tms570.c`
2. `firmware/platform/tms570/include/Os_Port_Tms570.h`
3. `firmware/platform/tms570/src/Os_Port_Tms570_Asm.S`
4. `firmware/platform/tms570/src/Os_Port_Tms570_Bringup.c`
4. `firmware/bsw/os/bootstrap/test/test_Os_Port_Tms570_bootstrap_core.c`
5. `firmware/bsw/os/bootstrap/port/tms570/README.md`
6. `docs/reference/tms570-port-traceability.md`

If there is doubt on interrupt/vector behavior, re-open:

1. `firmware/ecu/sc/halcogen/source/HL_sys_vim.c`
2. `d:\workspace_ccstheia\taktflow-embedded-production\private\vendor\threadx-master\ports\cortex_r5\gnu\src\tx_thread_context_save.S`
3. `d:\workspace_ccstheia\taktflow-embedded-production\private\vendor\threadx-master\ports\cortex_r5\gnu\src\tx_thread_context_restore.S`
4. `d:\workspace_ccstheia\taktflow-embedded-production\private\vendor\threadx-master\ports\cortex_r5\gnu\src\tx_timer_interrupt.S`

## Short Resume Prompt

If you want a compact resume prompt for the next session, use:

`Continue the TMS570 OSEK-first bootstrap port from firmware/bsw/os/bootstrap/port/tms570/SESSION_HANDOFF.md. Stay OSEK/AUTOSAR-first, keep tests small, use local ThreadX only as low-level reference, and continue from the current VIM runtime seam toward real target/vector/context ownership.`
