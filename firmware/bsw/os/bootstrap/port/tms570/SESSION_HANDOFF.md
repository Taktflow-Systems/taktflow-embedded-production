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

Local extracted ThreadX tree:

- `d:\workspace_ccstheia\.tmp_threadx\threadx-master`

Current interrupt/context references used repeatedly:

- `ports/arm11/gnu/src/tx_thread_context_save.S`
- `ports/arm11/gnu/src/tx_thread_context_restore.S`
- `ports/arm11/gnu/src/tx_thread_irq_nesting_start.S`
- `ports/arm11/gnu/src/tx_thread_irq_nesting_end.S`
- `ports/arm11/gnu/src/tx_thread_fiq_context_save.S`
- `ports/arm11/gnu/src/tx_thread_fiq_context_restore.S`
- `ports/arm11/gnu/src/tx_thread_fiq_nesting_start.S`
- `ports/arm11/gnu/src/tx_thread_fiq_nesting_end.S`
- `ports/arm11/gnu/src/tx_timer_interrupt.S`

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
4. Runtime SP ownership for prepared tasks.
5. Selected-next-task and deferred dispatch bootstrap model.
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

Current VIM/RTI runtime seam now looks like this:

1. RTI source becomes pending.
2. VIM request is latched/resynchronized.
3. `Os_Port_Tms570_ReadMappedChannelForRequest()` decodes `CHANCTRL`.
4. `Os_Port_Tms570_SelectPendingIrq()` latches `IRQINDEX` and active vector.
5. `Os_Port_Tms570_ReadActiveIrqChannel()` decodes `IRQINDEX - 1U`.
6. `Os_Port_Tms570_ReadActiveIrqVector()` reads the mapped handler from the VIM RAM-style table.
7. `Os_Port_Tms570_PulseActiveIrqMask()` pulses `REQMASKCLR/REQMASKSET`.
8. `Os_Port_Tms570_InvokeActiveIrqVectorCore()` invokes the mapped service core.
9. `Os_Port_Tms570_VimIrqEntryCore()` owns core entry behavior.
10. `Os_Port_Tms570_VimIrqEntry()` owns the IRQ wrapper.

Why this matters:

- the path is no longer one large helper
- the model now resembles the HALCoGen dispatcher shape much more closely
- future real vector glue can replace seams one by one instead of rewriting everything

## Current Evidence

Current status:

- `Model-tested`: yes
- `Build-tested`: yes
- `Spec-backed`: partly
- `Target-verified`: no

Current passing checks:

- TMS570 split bootstrap suite: `106 tests, 0 failures`
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

### 1. Finish the VIM-side decomposition

Goal:

- remove the last remaining hidden assumptions in the IRQ service flow

Next slices:

1. Add an explicit `service active IRQ by channel` seam so the service core branches from decoded channel, not just from the RTI compare0 happy path.
2. Add a tiny per-channel dispatch table or branch seam in C, still only for channel 2 at first.
3. Make `InvokeActiveIrqVectorCore()` use the decoded channel and the mapped vector together, not only the RTI handler-address equality check.

Done when:

- service does not rely on a direct `channel == 2` shortcut except at the final leaf
- channel decode, vector fetch, and service dispatch are all explicit seams

### 2. Move more of the IRQ wrapper shape into assembly-facing ownership

Goal:

- make the `.S` file mirror the runtime C flow one-to-one

Next slices:

1. Add explicit asm-facing wrappers for:
   - mapped channel read
   - active vector read
   - mask pulse
   - mapped vector invoke
2. Keep them thin at first.
3. Verify the C path still remains the source of truth.

Done when:

- every important VIM runtime seam has a matching asm-facing label
- there is no hidden C-only ownership step in the vector-entry path

### 3. Start replacing model-only context ownership with live task-context ownership

Goal:

- stop modeling context switching as only state bookkeeping

Next slices:

1. Introduce explicit saved-register frame ownership per prepared task context.
2. Separate:
   - initial prepared frame
   - last saved runtime frame
3. Move IRQ save toward writing that runtime frame object directly.
4. Move restore toward reading the selected task’s runtime frame directly.

Done when:

- IRQ save/restore updates a real per-task runtime context object
- switching tasks is no longer mostly synthetic state changes

### 4. Prepare real HALCoGen/TI bring-up glue

Goal:

- make the bootstrap seams line up with the real hardware files we will call later

Next slices:

1. Add a target-facing note or stub for:
   - `vimInit()`
   - `vimChannelMap()`
   - `vimEnableInterrupt()`
   - RTI compare0 init/start/ack path
2. Keep it out of the live build if needed.
3. Do not fake target verification.

Done when:

- the repo has a clear bridge from bootstrap seam to real HALCoGen call sites

### 5. Then do real target bring-up

Goal:

- move from `model-tested` to `target-verified`

Bring-up order:

1. Prove RTI compare0 fires.
2. Prove VIM channel 2 routes to IRQ.
3. Prove first-task launch.
4. Prove same-task IRQ return.
5. Prove two-task switch.
6. Prove IRQ-driven preemption.
7. Prove FIQ does not break IRQ-return ownership.

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
4. `firmware/bsw/os/bootstrap/test/test_Os_Port_Tms570_bootstrap_core.c`
5. `firmware/bsw/os/bootstrap/port/tms570/README.md`
6. `docs/reference/tms570-port-traceability.md`

If there is doubt on interrupt/vector behavior, re-open:

1. `firmware/ecu/sc/halcogen/source/HL_sys_vim.c`
2. `d:\workspace_ccstheia\.tmp_threadx\threadx-master\ports\arm11\gnu\src\tx_thread_context_save.S`
3. `d:\workspace_ccstheia\.tmp_threadx\threadx-master\ports\arm11\gnu\src\tx_thread_context_restore.S`
4. `d:\workspace_ccstheia\.tmp_threadx\threadx-master\ports\arm11\gnu\src\tx_timer_interrupt.S`

## Short Resume Prompt

If you want a compact resume prompt for the next session, use:

`Continue the TMS570 OSEK-first bootstrap port from firmware/bsw/os/bootstrap/port/tms570/SESSION_HANDOFF.md. Stay OSEK/AUTOSAR-first, keep tests small, use local ThreadX only as low-level reference, and continue from the current VIM runtime seam toward real target/vector/context ownership.`
