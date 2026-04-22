# PR proposal: `rtScheduling` library for openbsw (#370)

> Draft — not yet pushed upstream. Local branch in `d:\openbsw`:
> `experiment/rt-sched`, commit `b4428beb`.

## Summary

Add a new library `libs/bsw/rtScheduling/` that addresses
[#370 "Facilitate real-time operation"][i370].

The library turns the schedule of an application into a single, declarative
manifest. One `arm()` call replaces every scattered
`async::scheduleAtFixedRate(...)` invocation. Feasibility is proven at
**compile time** via constexpr Liu-Layland / hyperbolic / Joseph-Pandya RTA
analyses — an edit that over-subscribes a task context fails the build, not
the fleet. A stable JSON export (schema `rt-sched/1`) feeds a single-file
HTML audit tool that visualises every bound derivation, for ASIL sign-off
evidence.

[i370]: https://github.com/eclipse-openbsw/openbsw/issues/370

## Why merge this

- #370 asks for "an overall view over all mappings between schedulable
  entities and tasks" and "assign entities to tasks according to their cycle
  time and WCET". `EntityDecl` carries all four attributes (cycle, WCET,
  deadline, context) in one place; the manifest is that overview.
- #370 asks for "rate-monotonic scheduling" optimisation. The library
  exposes L&L sufficient bounds, the tighter hyperbolic bound, and
  Joseph-Pandya exact RTA. Compile-time verdict on either the sufficient or
  exact tests.
- ASIL-D users get a compile-time feasibility gate — the strongest possible
  evidence shape. Failures surface in CI, not in the vehicle.

## Commit

```
b4428beb feat(rtScheduling): add declarative real-time scheduling library (#370)
  19 files changed, 2597 insertions(+)
```

All changes live under `libs/bsw/rtScheduling/` plus one line added to
`libs/bsw/CMakeLists.txt`.

## Test evidence

| Harness | Target | Result |
| --- | --- | --- |
| `cmake --preset tests-posix-debug && ctest -L rtSchedulingTest` | openbsw unit tests on MinGW-w64 g++ 15.2.0 | **48/48 pass** |
| Standalone FetchContent harness in the development experiment | host g++ 15.2.0 | 48/48 pass |
| `arm-none-eabi-g++ -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -Werror -Os` | Cortex-M4 cross-compile | clean; ~10.7 KB text, 1 KB bss |

Compile-time feasibility `static_assert`s verify the gate works end-to-end
on both host and arm-none-eabi toolchains.

## Design choices

- **C++17 standard library only.** `std::array`, `std::string_view`,
  `std::cstdint`. No etl dependency; `AsyncBridge.h` couples to `async`
  only at the consuming application TU level so the core library is
  unit-testable without the RTOS stack.
- **ppm integers everywhere on the wire.** No floating-point appears in
  feasibility math or JSON output; fractions are a display concern.
- **Strict rate-monotonic tie-breaking by declaration order.** Matches the
  FIFO-within-priority behaviour of FreeRTOS, ThreadX, and OSEK.
- **Singleton Registry, static-storage bindings.** No heap; capacity is a
  compile-time constant (`RT_SCHED_MAX_ENTITIES`, default 64).
- **JSON as the runtime-to-tooling interface.** Chunked emit variant is
  UART-friendly for firmware dumps; buffered variant keeps hostside
  tooling simple.

## Scope explicitly NOT in this PR

- Removal of the existing `scheduleAtFixedRate` call sites in
  `executables/referenceApp/application/src/systems/*` — done as a
  follow-up so this PR stays pure additive.
- Multi-core response-time analysis — openbsw's async model is
  single-core per task; the library's single-context RTA is exact for
  that model.
- Priority assignment feedback into `asyncFreeRtos` / `asyncThreadX`
  adapters — `priority_hint` is carried in `EntityDecl` but not yet
  consumed by the adapters.
- Audit GUI — shipped as a separate tool repo asset, not as a build
  artifact. Screenshot and JSON sample attached to the PR description.

## Follow-up PRs (after this one lands)

1. Migrate reference-app systems to rtScheduling (one-line `run()` bodies;
   manifest lives in new `executables/referenceApp/application/src/app/Schedule.cpp`)
2. Wire `priority_hint` into `asyncFreeRtos`/`asyncThreadX` so declared
   priorities reach the RTOS
3. Console command `rtsched dump` emitting the JSON over the existing
   runtime console
4. HIL test on Nucleo G474RE verifying declared WCETs against observed
   stats under worst-case load

## Reviewer checklist

- [ ] Library builds and tests pass under `cmake --preset tests-posix-debug`
- [ ] `libs/bsw/CMakeLists.txt` diff is one line (add_subdirectory)
- [ ] Module follows `libs/bsw/<name>/` convention (compare to
      `libs/bsw/timer/`): CMakeLists, module.spec, doc/index.rst,
      include/<name>/, src/, test/
- [ ] Copyright header uses `// Copyright 2026 Accenture.` on every source
      and test file
- [ ] No new dependencies beyond the C++17 standard library
- [ ] Cross-compile clean for Cortex-M4 with `-fno-exceptions -fno-rtti
      -Wpedantic -Werror`
- [ ] `static_assert`-driven ASIL-D compile-time gate demonstrated in
      `test/src/RtaTest.cpp::compile_time_rta`
- [ ] JSON format stable and versioned (`rt-sched/1`)

## Contact

Developed as an experiment in the Taktflow / ASIL-D zonal-architecture
programme. Design doc: `DESIGN.md` in the experiment tree. Audit GUI and
sample JSON: `gui/` in the experiment tree.
