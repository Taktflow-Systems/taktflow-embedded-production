# DESIGN — openbsw-rt-sched

## 1. Problem (openbsw issue #370)

> Applications and basic software components have schedulable entities with
> strict real-time requirements. \[...\]
>
> This method `async::scheduleAtFixedRate` can be called at any time and
> with any task context as input; this makes it quite difficult to gain an
> overview of the entire system behavior.

Concretely, in the openbsw reference application the schedule is currently
spread across at least thirteen `scheduleAtFixedRate` call sites — each in a
system's `run()` method, each with a magic-number period, each without any
recorded WCET or deadline. Examples (paths are in the openbsw repo):

| Call site | Period |
| --- | --- |
| `executables/referenceApp/application/src/systems/SafetySystem.cpp:39` | 10 ms |
| `executables/referenceApp/application/src/systems/DemoSystem.cpp:108` | 100 ms |
| `executables/referenceApp/application/src/systems/RuntimeSystem.cpp:32` | 10 ms |
| `executables/referenceApp/application/src/systems/UdsSystem.cpp:72` | 10 ms |
| `executables/referenceApp/application/src/systems/EthernetSystem.cpp:137` | 1 ms |
| `executables/referenceApp/application/src/systems/DoCanSystem.cpp:113` | CYCLE_TIME |
| `executables/referenceApp/application/src/systems/SysAdminSystem.cpp:27` | 100 ms |
| `executables/referenceApp/platforms/posix/main/src/systems/CanSystem.cpp:30` | 5 ms |
| `executables/referenceApp/platforms/posix/main/src/systems/TapEthernetSystem.cpp:28` | 1 ms |
| `executables/referenceApp/platforms/s32k148evb/main/src/systems/BspSystem.cpp:29` | 10 ms |
| ... and others in DoIP / transceivers |

Four concrete pain points follow from this decentralisation:

1. **No single source of truth.** There is no file you can open to learn
   "what periodic activities exist in this binary". You grep.
2. **No design-time feasibility check.** Nothing catches a late edit that
   pushes TASK_BACKGROUND from 70 % to 110 % utilization. The symptom is a
   runtime deadline miss, possibly in the field.
3. **No WCET contract.** `scheduleAtFixedRate` takes a period. It does not
   take a worst-case execution time or a deadline. Runtime has no basis on
   which to detect overrun.
4. **No enumeration API.** openbsw keeps the active periodic set in a
   private `Timer::_timeoutList` per TaskContext. A supervisor, a console
   command, or an audit trace can't iterate it.

## 2. Proposed API

### 2.1 Declaration

Every periodic activity is described by a POD `EntityDecl`:

```cpp
struct EntityDecl {
    std::string_view name;
    ContextId        context;
    Timing           timing;        // cycle, WCET, deadline, phase — all µs
    Criticality      criticality;   // QM / ASIL_A..D
    OverrunPolicy    overrun;       // Ignore / LogAndCount / Safety
    std::uint8_t     priority_hint; // 0 = derive from RM
};
```

`EntityDecl` is trivially constexpr-constructible. A system-wide manifest is
a `std::array<EntityDecl, N>`. That array is the thing #370 asks for —
a single, inspectable object naming every schedulable entity in the binary.

### 2.2 Validation

`Feasibility.h` provides constexpr analyses:

- `entity_is_sane(e)` — enforces `C > 0`, `T > 0`, `C ≤ D ≤ T`, `phase < T`.
- `utilization_ppm(manifest, ctx)` — Σ Cᵢ/Tᵢ in parts-per-million.
- `rm_bound_ppm(n)` — Liu & Layland n(2^(1/n)-1), tabulated, converges to
  ln(2). Sufficient but not necessary.
- `hyperbolic_bound_met(manifest, ctx)` — Bini/Buttazzo 2003
  Π(uᵢ+1) ≤ 2. Tighter than L&L but still sufficient; still does not
  prove infeasibility on its own.
- `analyze(manifest)` — rolls the above into a single `Verdict` enum.
- `hyperperiod_us(manifest)` — lcm of cycles, 64-bit to delay overflow.

Every check is `constexpr`. The intended use is:

```cpp
static_assert(rtSched::analyze(rtSched::view(MANIFEST)) == rtSched::Verdict::Ok,
              "schedule is infeasible");
```

An edit that makes the manifest infeasible fails the build, not the
vehicle.

### 2.3 Binding and arming

The declaration layer has no pointers and no runnables. The binding layer
attaches each decl to a concrete runnable and a concrete `TimeoutType`:

```cpp
struct Binding {
    EntityDecl const* decl;
    void*             runnable_ptr; // async::RunnableType*, erased
    void*             timeout_ptr;  // async::TimeoutType*, erased
    EntityStats*      stats;        // optional
};
```

The `Registry` singleton accumulates bindings (either via the `Binder`
RAII helper or from an explicit `std::array<Binding, N>`), and `arm()`:

1. collects all decls, runs `analyze(...)`;
2. if feasible, calls the installed bridge function for each binding;
3. returns a diagnostic code.

The bridge is installed by `AsyncBridge.h` (the only header that touches
`async::`). It maps a Binding to a single `async::scheduleAtFixedRate(
ctx, runnable, timeout, cycle_us, MICROSECONDS)` call — exactly what
application code was doing by hand, but now behind one controlled gate.

### 2.4 Runtime enforcement

`MonitoredRunnable<Inner>` wraps a user runnable. Every `execute()` sample
the clock, dispatches, resamples, and compares against `wcet_us`. On
overrun the declared `OverrunPolicy` fires. This converts the declared WCET
from a comment into a runtime invariant.

Stats (`EntityStats`: invocations / overruns / min / max / last) are
accumulated per entity and are read-only reachable via the Registry. A
console command or a supervisor task can dump them.

## 3. Before / after call-site diff

**Before** (current openbsw idiom, e.g. `SafetySystem.cpp:39`):

```cpp
void SafetySystem::run()
{
    ::async::scheduleAtFixedRate(
        _context, *this, _timeout, SYSTEM_CYCLE_TIME, ::async::TimeUnit::MILLISECONDS);
    transitionDone();
}
```

The knowledge that SafetySystem runs at 10 ms on TASK_SAFETY lives inside a
`.cpp`. It's discoverable by grep, not by reading a spec file.

**After**:

```cpp
// SafetySystem.cpp — no scheduling call. The runnable just runs.
void SafetySystem::run() { transitionDone(); }

// manifest.cpp — single source of truth
constexpr rtSched::EntityDecl DECL_SAFETY{
    "SafetySystem", TASK_SAFETY,
    {/*cycle_us*/ 10'000, /*wcet_us*/ 1'500, /*deadline*/ 10'000, /*phase*/ 0},
    rtSched::Criticality::ASIL_D,
    rtSched::OverrunPolicy::Safety,
};
```

Now SafetySystem's timing is:

- in one known file;
- accompanied by a WCET budget and criticality;
- validated against rate-monotonic bounds at compile time;
- runtime-enforced by `MonitoredRunnable`.

## 4. Decoupling from openbsw

The `rtScheduling` static library has **zero** dependency on openbsw's
`async` module. It uses only the C++17 standard library. All openbsw
coupling sits in `AsyncBridge.h` (header-only) and is only compiled when
the consuming project already pulls in `async`.

Consequence: the unit test suite builds and runs on any workstation with
`cmake`, `g++` or MSVC, and internet access for `FetchContent`ing gtest.
That is the primary verification harness for the feasibility math.

## 5. Lifetime and thread-safety

- `Registry` is a Meyers singleton. Construction is thread-safe since
  C++11. It is meant to be populated during static initialisation or from
  the main thread before `arm()`.
- `Binding` must outlive the Registry. Typical usage is static storage
  (the manifest is a file-scope `std::array`). Heap allocation is not
  required.
- After `arm()`, the Registry state is effectively frozen; nothing in the
  current API mutates it. A future "replace at lifecycle-level-change"
  path would need per-entry atomicity; it is out of scope here.
- `MonitoredRunnable` stats are written by the task that runs the
  runnable and read by anyone that reads them — writes are a few 32-bit
  stores, acceptable for statistical observability but not for control
  flow. A supervisor that wants exact counts should snapshot under the
  async lock.

## 6. What the API does NOT do (yet)

- **Response-time analysis (Audsley RTA).** The feasibility check is
  sufficient, not exact. For a hard ASIL-D sign-off you want iterative
  RTA. It fits as a pure constexpr function on the same manifest — next
  increment.
- **Priority assignment.** `priority_hint` is carried in `EntityDecl` but
  not consumed: openbsw's FreeRTOS adapter assigns one FreeRTOS task per
  context, and the scheduling within a context is FIFO by expiry. A proper
  RM priority assignment would require changes inside
  `asyncFreeRtos`/`asyncThreadX` — a separate workstream.
- **Resource ceiling / PIP.** We don't model blocking. Issue #370 doesn't
  ask for it; if a follow-up asks, PCP/PIP metadata can be added to the
  manifest without breaking the existing shape.
- **Multi-core.** openbsw's async model is single-core per task. If
  multi-core support is added upstream, `ContextId → core` mapping would
  need to extend the feasibility analysis.
- **Replace / remove at runtime.** The API is design-time. Dynamic
  replacement of an entity at a lifecycle transition is intentionally out
  of scope — it's exactly the flexibility that #370 complains about.

## 7. Integration path into openbsw referenceApp

This experiment lives outside the openbsw tree. To run the demo end-to-end
on the posix reference app, the minimal patch to openbsw is:

```diff
  // executables/referenceApp/application/src/app/app.cpp
+ #include <demo/manifest.h>
+ #include <demo/SafetySystem.h>
+ #include <demo/ControlSystem.h>
+ #include <demo/TelemetrySystem.h>
...
  void startApp() {
      // ... existing lifecycle registrations ...
+     static demo::SafetySystem safety{TASK_SAFETY};
+     static demo::ControlSystem control{TASK_BACKGROUND};
+     static demo::TelemetrySystem telemetry{TASK_SYSADMIN};
+     auto const rc = demo::bootstrap_schedule(safety, control, telemetry);
+     // In real code: check rc, transition to safe state on non-Ok.
      lifecycleManager.transitionToLevel(MaxNumLevels);
  }
```

And link `rtSchedDemo` into `app.referenceApp`:

```cmake
# executables/referenceApp/application/CMakeLists.txt
target_link_libraries(app.referenceApp PRIVATE rtSchedDemo)
```

The existing three referenceApp systems that overlap the demo's demo
entries (`SafetySystem`, `RuntimeSystem`, `SysAdminSystem`) would need to
have their `scheduleAtFixedRate` calls removed, since the manifest now owns
that side effect. That edit is the "payoff" of the migration — no
scheduling in system bodies at all.

## 8. Why this shape is ASIL-friendly

The user is building an ASIL-D system on top of openbsw. The API was
shaped with that context:

1. **Single point of scheduling** — easier to argue freedom from
   interference and to audit scheduling decisions.
2. **Compile-time feasibility gate** — moves schedulability from
   verification-time evidence into build-time evidence. Cheaper to
   maintain.
3. **Criticality-aware overrun policy** — ASIL-D entities can trip the
   safety handler on WCET violation; QM entities log. Policy lives with
   the declaration.
4. **No heap, no dynamic allocation** — `std::array`, stack analysis,
   linker-time sizing. Compatible with MISRA / AUTOSAR-C++14 style
   constraints (though headers currently use `std::string_view` — a
   trivial change to `char const*` if that is rejected by a downstream
   guideline).
5. **Test harness independent of RTOS** — the feasibility logic runs in
   hostside unit tests, no qemu.

## 9. Roadmap if this graduates from experiment

| Step | Artifact | Acceptance |
| --- | --- | --- |
| R-01 | Exact RTA implementation (Joseph-Pandya) as constexpr function | `static_assert` passes on a manifest where L&L fails but RTA succeeds |
| R-02 | Priority assignment derived from RM, consumed by async adapter | Reference app tasks keep current ordering under the new API |
| R-03 | Console command `rtsched dump` listing the registry | New entry in `referenceApp`'s console; manual smoke test |
| R-04 | Migration patch removing `scheduleAtFixedRate` from the 13 call sites | referenceApp boot + runtime stats match the pre-migration baseline within jitter envelope |
| R-05 | Upstream as `libs/bsw/rtScheduling/` with Accenture-style copyright headers | PR accepted on eclipse-openbsw/openbsw |
| R-06 | Audsley RTA + blocking-term model | Sign-off against a published textbook example |

## 10. Open questions

- **Units.** We chose µs everywhere. openbsw's async takes `(period, unit)`.
  Demanding µs at the manifest level is simpler and loses no precision; the
  bridge converts once. OK?
- **String storage.** `std::string_view` works on constants fine; if the
  project forbids `<string_view>`, a `char const*` fallback is a one-line
  change.
- **Capacity.** `RT_SCHED_MAX_ENTITIES` defaults to 64, compile-time
  overridable. Reference app uses ~13 periodic entries; 64 is a
  comfortable ceiling for a mid-size ECU. A linker-backed option
  (`extern` array in user code, size derived from its definition) could
  replace the fixed array later if desired.
