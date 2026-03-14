# TMS570 Port Traceability

Updated: 2026-03-14

Purpose:

- turn the current "cannot assume" list into a tracked evidence map
- separate `model-tested`, `build-tested`, `spec-backed`, and `target-verified`
- show what official online references can reduce uncertainty before hardware bring-up

## Evidence Levels

`Model-tested`

- host-side bootstrap tests passed against the C state model
- useful for regression control and design consistency
- does not prove real Cortex-R5 exception behavior

`Build-tested`

- the code compiled or assembled successfully with the intended toolchain
- useful for catching syntax, symbol, and integration breakage
- does not prove runtime correctness

`Spec-backed`

- the behavior is anchored to official vendor or architecture references
- useful for tightening assumptions and writing stronger assertions
- still does not replace live target verification

`Target-verified`

- behavior observed on real hardware or a trusted target-faithful environment
- needed before we can claim the port is correct

## Current TMS570 Status

As of 2026-03-14:

- `Model-tested`: yes, for the bootstrap host model
- `Build-tested`: yes, for TMS570 C and `.S` syntax/buildability
- `Spec-backed`: partly
- `Target-verified`: no

Current repo evidence:

- TMS570 split bootstrap suite: `185 tests, 0 failures`
- TMS570 assembly build check: `arm-none-eabi-gcc -mcpu=cortex-r5 -marm -c ...` passed
- Shared SchM regression check: `11 tests, 0 failures`

Recent local-model additions:

- solicited system-return save seam modeled from the local `threadx-master/ports/cortex_r5/gnu/src/tx_thread_system_return.S`
- solicited task-switch restore now model-tested against the local `threadx-master/ports/cortex_r5/gnu/src/tx_thread_schedule.S` split between interrupt and solicited frame types
- explicit interrupt-versus-solicited task-frame byte footprints, with asm-facing restore-frame byte inspection, now model-tested against the same local Cortex-R5 save/restore shapes
- real solicited return, IRQ switch-away, and FIQ scheduler-return now commit saved task `SP` from the live running `SP` minus the modeled frame-byte footprint, and focused IRQ/FIQ tests now prove that saved-frame `SP` rule directly
- explicit IRQ interrupt-context byte tracking is now model-tested against the exact local `tx_thread_context_save.S` save shapes: `32` bytes for running-thread first entry, `32` bytes for nested IRQ save, and `0` persistent bytes for the idle-system scheduler path
- per-level IRQ saved `SPSR/CPSR` ownership is now model-tested against the same local save file, so nested IRQ save/restore restores the saved status in LIFO order instead of relying on one flat live slot
- per-level IRQ scratch-register ownership is now model-tested against the same local save file, so nested IRQ save/restore restores the saved `r0-r3, r10, r12` snapshot in LIFO order instead of relying on one flat live scratch slot
- matching per-level FIQ saved `SPSR/CPSR` and scratch ownership is now model-tested against the local FIQ save/restore files, including the exact `24-byte` first-entry rule where only `r0-r3` belong to the minimal saved FIQ scratch frame
- the runtime-frame setter bug that treated valid solicited `StackType = 0` as "unset" is now fixed and covered by a direct solicited VFP restore-split test anchored to the local `tx_thread_schedule.S` / `tx_thread_system_return.S` pair
- coherent packed saved/restored IRQ/FIQ interrupt-context views are now model-tested and build-tested, the save-side runtime frame now refreshes from that packed saved-context object, switch-away capture now prefers the packed restored-context object over later live restore-side mutations, and narrow resume-current behavior now prefers the packed restored-context object over corrupted saved-frame metadata for interrupt-state fields, so the asm-facing interrupt-context seam can consume `ReturnAddress + CPSR + FrameBytes + Scratch` as one object instead of reconstructing it from separate globals
- packed saved/restored task-frame views are now model-tested and build-tested, so the save side and the applied-restore side can publish one coherent `Frame + FrameBytes` object instead of only scattered `LastSaved*` and `LastRestored*` fields
- pending effective IRQ/FIQ restore-task-frame views plus scalar peek helpers are now model-tested and build-tested, so the asm-facing restore seam can see the real resume-current or switch-task payload before `Finish*Restore()` runs instead of reconstructing it from the raw selected-task runtime frame
- explicit restore-in-progress ownership is now model-tested and build-tested, so those pending effective restore-task-frame views exist only during the live IRQ/FIQ restore window and the generic `PeekRestoreTask*` seam can safely prefer the effective pending frame during that window without leaking stale state afterward
- saved-task-frame scalar and block getters are now model-tested and build-tested, so the asm-facing save seam can consume the saved task context as an object instead of depending on raw C struct layout
- explicit save-in-progress ownership plus pending effective IRQ/FIQ save-context/save-task-frame views are now model-tested and build-tested, so the asm-facing save seam can inspect the live save window instead of waiting for post-save bookkeeping
- solicited system return begin/finish ownership plus the pending solicited save-task-frame view are now model-tested and build-tested, so the local model no longer treats solicited save as only one opaque step
- first-task start begin/finish ownership plus the pending first-task frame view are now model-tested and build-tested, the generic `PeekRestoreTask*` seam now tells the truth during that live launch window, the start path no longer depends on a dedicated prepared-`SP` shortcut outside the restore-object model, and the asm skeleton now reads first-task `StackType/CPSR` from that same generic restore seam instead of the synthetic-frame header shortcut
- unified pending save/restore task-frame views plus unified pending save/restore interrupt-context views are now model-tested and build-tested, so the asm-facing seam can inspect one truthful in-flight save/restore object instead of branching between first-task, solicited, IRQ, and FIQ windows for every lookup
- explicit IRQ idle-system restore is now model-tested and build-tested, aligned with the local `tx_thread_context_restore.S` idle-system branch, so a final IRQ restore with no running task no longer fakes a resume-current path
- explicit IRQ switch-task scheduler-return bookkeeping is now model-tested and build-tested, and the asm skeleton now has a separate scheduler-placeholder return branch for `switch task` and `idle system`, so those local-model paths no longer pretend to be ordinary interrupted-thread resumes
- the local IRQ restore model now also has an explicit `scheduler return in progress` window plus a truthful pending scheduler-return task-frame view, and `FinishIrqContextRestore()` can now upgrade an originally computed `resume current` into a real `switch task` after `Os_PortExitIsr2()` once a selected prepared task is actually known, which keeps the local model aligned with Cortex-R5-style IRQ-exit timing in the ISR/tick-driven preemption cases without collapsing back to one opaque restore helper
- the local FIQ restore model now also has an explicit `scheduler return in progress` window plus a truthful pending FIQ scheduler-return task-frame view, that new phase is now model-tested through both the shared configured-dispatch completion helper and the kernel-dispatch observe hook, and the asm scaffold now distinguishes ordinary FIQ exception return from the scheduler-return placeholder branch so the local `preempt scheduler` branch from `tx_thread_fiq_context_restore.S` no longer collapses back into one opaque restore helper
- the local solicited system-return save window now also snapshots one fixed pending task-frame object at `BeginSolicitedSystemReturn()`, and `FinishSolicitedSystemReturn()` now consumes that frozen frame instead of later-mutated live task state, so this non-interrupt save path is also more honestly object-backed and less dependent on parallel live-state bookkeeping

## Official Online References

These are the strongest public references we can use before target bring-up:

1. TI TMS570LC4357 product page
   - https://www.ti.com/product/TMS570LC4357
   - What it gives us:
     - device overview
     - links to the datasheet
     - links to the TMS570LC43x Technical Reference Manual
     - links to device errata
     - links to safety collateral

2. Official Eclipse ThreadX repository
   - https://github.com/eclipse-threadx/threadx
   - What it gives us:
     - official upstream implementation reference
     - supported architecture list
     - portable-kernel and port structure conventions
  - Important note:
    - our current local vendor copy under
      `d:\workspace_ccstheia\taktflow-embedded-production\private\vendor\threadx-master`
      does contain `ports/cortex_r5`
    - future low-level TMS570 decisions should prefer those exact local
      `ports/cortex_r5/gnu/src/*` files first
    - earlier bootstrap slices were initially cross-checked against matching
      local `ports/arm11/gnu/src/*` files, and those remain useful as a
      secondary comparison reference

3. Official Arm ABI documents
   - https://github.com/ARM-software/abi-aa
   - What they give us:
     - `AAPCS32`
     - `AAELF32`
     - alignment, calling convention, and frame-layout rules

4. Arm processor manuals
   - Public web availability varies by document and licensing
   - Use official Arm documentation when accessible through:
     - https://developer.arm.com/
   - What they help with:
     - Cortex-R5 exception model
     - mode switching
     - banked-register ownership
     - MPU details

## What We Can Tighten Before Hardware

The internet and official docs can move several items from "hand-wavy" to
`spec-backed`:

- VIM register ownership and vector routing
- RTI timer register programming and interrupt acknowledge sequence
- stack alignment and frame-layout assumptions
- ABI-safe handoff between C and assembly
- expected MPU resource count and configuration model
- silicon errata checks for interrupt and timer assumptions

They cannot finish these items alone:

- live exception entry/exit correctness
- real register preservation across task switches
- real interrupt latency/preemption timing
- real board-level vector hookup
- real MPU fault behavior on this MCU/board

## Traceability Table

| Concern | Current repo evidence | Official source(s) | What we can assume after source review | What still needs target proof |
|---|---|---|---|---|
| VIM vector ownership and interrupt routing | `Model-tested` for the bootstrap register image and channel dispatch seam: VIM channel 2 is routed to IRQ, enabled as the RTI compare 0 source, bound into a private VIM RAM-style ISR table, blocked correctly when disabled, and now uses an explicit `RTI source pending -> VIM request latch/resync -> read mapped channel from CHANCTRL -> select active IRQINDEX/vector -> read active channel (IRQINDEX - 1U) -> read active vector from the VIM RAM-style table -> pulse active REQMASKCLR/SET -> invoke mapped vector core -> VIM IRQ entry wrapper -> VIM IRQ entry core -> service active IRQ core` flow. The convenience pending-dispatch helper now stays quiet when no IRQ is latched, while the true VIM entry path owns the IRQ wrapper. The model preserves one-based `IRQINDEX`, records the local `REQMASKCLR0/REQMASKSET0` service pulse alongside `INTREQ0`, `IRQINDEX`, `IRQVECREG`, and the local `CHANCTRL0` request mapping state, and keeps the RTI tick service core separate from the IRQ wrapper. The Cortex-R5 bootstrap `.S` file now mirrors those runtime seams instead of lagging behind the C model. | TI product page -> TRM, datasheet, errata: https://www.ti.com/product/TMS570LC4357 | Register names, vector structure, CHANCTRL-based request mapping, one-based `IRQINDEX` encoding, active-channel decode, active-vector fetch, route-to-IRQ enable flow, handler binding shape, source-pending/request resynchronization, active-vector selection, active-service ownership, explicit mask-pulse ownership, mapped-vector invocation, vector-entry wrapper/core split, request-mask pulse semantics, request mapping, and documented caveats. | Real ISR firing on hardware, vector table hookup, readback of enable/pending state. |
| RTI tick source and counter hookup | `Model-tested` for the bootstrap tick flow plus the RTI register image: compare 0 period/update, source-side notification enable/disable, pending-flag delivery, `counter 0` advance-to-compare behavior, a distinct RTI tick service core separated from the IRQ wrapper, write-1-to-clear acknowledge, periodic compare-value advance, and the `counter 0 must be running` delivery gate are now host-tested. | TI product page -> TRM and errata: https://www.ti.com/product/TMS570LC4357 | RTI register model, tick source selection, start/stop gate semantics, source pending semantics, compare-match generation from counter progress, notification gate semantics, wrapper-versus-core ownership, compare/interrupt flow, acknowledge semantics, periodic compare update behavior, and documented errata checks. | Correct tick rate, compare interrupt firing, real alarm wakeup timing on target. |
| IRQ context save / restore structure | `Model-tested` in split IRQ suites and `Build-tested` in `.S`. The bootstrap now captures runtime-frame `SP`, `ReturnAddress`, saved task `LR`, `TimeSlice`, `CPSR`, a task-lower `r0-r3` block, IRQ scratch, an expanded task-upper block through `R12`, and optional VFP state on save/switch-away, task-switch restore reapplies the selected task's runtime frame into live restore-side TMS570 state, resume-current restore uses a narrower frame apply path, direct lower/scratch/preserved/VFP restore-block helpers plus restore-link-register helpers exist for the assembly-facing seam, and the earlier zero-scratch overwrite bug is covered by regression tests. | Official ThreadX repo: https://github.com/eclipse-threadx/threadx and official Arm ABI docs: https://github.com/ARM-software/abi-aa | The port structure, save/restore handoff shape, outgoing task lower-register ownership, upper-register save ownership through `R12` including task `LR`, optional VFP save/restore ownership, resume-versus-switch distinction, and ABI assumptions can be made explicit and reviewed against known patterns. | Real preservation of lower and upper task registers through `R12`, optional VFP state, saved task `LR`, SPSR/CPSR handling, and return-to-thread behavior on target. |
| FIQ context save / restore structure | `Model-tested` in split FIQ suites and `Build-tested` in `.S`. The bootstrap now captures FIQ scheduler-return runtime metadata from live FIQ state, including the saved task-lower `r0-r3` block, expanded task-upper block through `R12`, task `LR`, and optional VFP state, first-entry FIQ save seeds a task runtime frame for later no-preempt resume, and FIQ resume-previous-mode now reapplies the saved FIQ resume fields without pretending to be a full task switch. | Official ThreadX repo: https://github.com/eclipse-threadx/threadx and Arm docs: https://developer.arm.com/ | The FIQ save/restore split, scheduler-return metadata handoff, saved task-lower block ownership, saved task-upper block ownership through `R12`, saved task `LR` ownership, optional VFP ownership, and no-preempt resume behavior can be made source-backed. | Actual FIQ lower/upper register preservation through `R12`, optional VFP state, task `LR` handoff, mode-return behavior, and scheduler handoff on target. |
| FIQ context separation from IRQ return | `Model-tested` in split FIQ suites and `Build-tested` in `.S`. | Official ThreadX repo: https://github.com/eclipse-threadx/threadx and TI product page: https://www.ti.com/product/TMS570LC4357 | FIQ can remain a separately modeled path with explicit non-IRQ-return ownership. | Actual FIQ delivery, nesting, and return behavior on hardware. |
| Banked registers and mode switching | `Model-tested` only. | Arm docs portal: https://developer.arm.com/ and ABI docs: https://github.com/ARM-software/abi-aa | The required mode and banked-state transitions can be specified more tightly. | That our assembly performs them correctly on real Cortex-R5 silicon. |
| Initial task frame and first-task launch | `Model-tested` and `Build-tested`. | ABI docs: https://github.com/ARM-software/abi-aa and TI product page: https://www.ti.com/product/TMS570LC4357 | Stack alignment and general frame assumptions can be constrained by spec. | Real first-task branch into a live runnable context. |
| Time-slice save / restore handoff | `Model-tested` in scheduler, IRQ-return, FIQ scheduler-return, tick-side countdown/expiry bookkeeping, the separate post-expiry time-slice service hook, the reload of the running task's configured next slice, and the explicit no-rotation boundary for same-priority ready peers in the OSEK-oriented bootstrap. | Official ThreadX repo: https://github.com/eclipse-threadx/threadx | The intended save/restore sequencing plus the timer-side countdown and service-hook shape can be made source-backed, while same-priority ThreadX rotation is deliberately kept out of the current OSEK bootstrap semantics. | Real timer-driven slice expiry and scheduler handoff on target. |
| MPU-backed SC3 protection | `Not implemented` on target. Only software modeling exists today. | TI product page -> TRM and safety collateral: https://www.ti.com/product/TMS570LC4357 and Arm docs portal: https://developer.arm.com/ | MPU resource model, region-programming plan, and fault-path design can be specified. | Real MPU programming, access-fault behavior, and trusted/non-trusted separation. |
| Errata-sensitive assumptions | `Not checked` in code yet. | TI product page -> errata: https://www.ti.com/product/TMS570LC4357 | Known silicon hazards can be added to bring-up and code review checklists. | That our exact MCU revision/board behavior matches the reviewed errata state. |

## Immediate Actions We Can Do Now

These do not require hardware and should reduce future target risk:

1. Keep source tags on each TMS570 VIM/RTI register assumption and expand them as the TI TRM details are pulled in.
2. Add ABI-oriented assertions for stack alignment, frame size, and preserved-register expectations.
3. Add a "reviewed errata" checklist item to the TMS570 bring-up plan.
4. Keep the TMS570 test files small and functionally grouped so later MC/DC work stays feasible.
5. Replace vague verification wording with the four evidence levels above in port docs and status updates.

## Required Target Bring-Up Checklist

These items remain outside the reach of internet references alone:

1. Prove first-task launch on target.
2. Prove two-task switch on target with preserved registers.
3. Prove IRQ return to interrupted task.
4. Prove IRQ-driven preemption to another task.
5. Prove FIQ does not corrupt IRQ-return ownership.
6. Prove RTI periodic tick at the expected rate.
7. Prove VIM routing and final interrupt acknowledge path.
8. Prove stack frame and SP values by on-target inspection.
9. Prove MPU programming and fault handling once SC3 work begins.

## Repo Counterparts

The main TMS570 bootstrap counterparts for this traceability work are:

- `firmware/platform/tms570/include/Os_Port_Tms570.h`
- `firmware/platform/tms570/src/Os_Port_Tms570.c`
- `firmware/platform/tms570/src/Os_Port_Tms570_Asm.S`
- `firmware/bsw/os/bootstrap/test/test_Os_Port_Tms570_bootstrap_*.c`
- `firmware/bsw/os/bootstrap/port/tms570/README.md`
- `docs/reference/threadx-local-reference-map.md`

## Plain-English Bottom Line

What we can now say honestly:

- the TMS570 bootstrap port is becoming `model-tested` and `build-tested`
- several open assumptions can be made `spec-backed` with TI, Arm, and official ThreadX references
- the port is still `not target-verified`

That is the right line to hold until live TMS570 bring-up starts.
