# openbsw-rt-sched

Experimental declarative real-time scheduling layer for Eclipse openbsw.

Tracks openbsw issue [#370 "Facilitate real-time operation"][i370].

[i370]: https://github.com/eclipse-openbsw/openbsw/issues/370

## How to read this

Audience: a future maintainer (human or AI) landing cold, no prior context.

This experiment prototypes an API that replaces the scattered
`async::scheduleAtFixedRate(...)` calls in openbsw application code with a
single, declarative manifest. The manifest is the only place the schedule
lives; it is validated at compile time against a rate-monotonic feasibility
bound; and it is armed in one call at startup.

Read in this order:

1. `DESIGN.md` — problem statement, API surface, semantics, trade-offs.
2. `libs/rtScheduling/include/rtScheduling/*.h` — the API itself. Start
   with `Types.h`, then `Feasibility.h`, then `Registry.h`.
3. `demo/src/manifest.cpp` — a worked example. This is what a real openbsw
   app would look like after adoption.
4. `libs/rtScheduling/test/src/` — unit tests that exercise the feasibility
   math without openbsw.

## Folder layout

```
experiments/openbsw-rt-sched/
├── CMakeLists.txt              root build
├── README.md                   this file
├── DESIGN.md                   architecture + semantics
├── libs/rtScheduling/          pure library (no async dep)
│   ├── include/rtScheduling/
│   │   ├── Types.h                  EntityDecl, Criticality, Timing
│   │   ├── Feasibility.h            constexpr feasibility analysis
│   │   ├── Registry.h               singleton registry + Binder + arm_manifest
│   │   ├── MonitoredRunnable.h      WCET-enforcing wrapper
│   │   └── AsyncBridge.h            openbsw async glue (only header that
│   │                                touches async::)
│   ├── src/Registry.cpp
│   ├── module.spec                  upstream-ready stub
│   └── test/                        gtest suite
└── demo/                       drop-in example for referenceApp
    ├── include/demo/                stubs for three IRunnable systems
    ├── src/*.cpp                    system bodies
    └── src/manifest.cpp             THE declarative schedule
```

## Build shapes

### 1. Standalone unit tests (no openbsw needed)

Validates the feasibility math and the registry logic. Works on any
workstation with a C++17 compiler.

```bash
cmake -S . -B build -DRT_SCHED_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### 2. Demo built against a local openbsw checkout

```bash
cmake -S . -B build \
      -DRT_SCHED_BUILD_DEMO=ON \
      -DOPENBSW_SOURCE_DIR=/path/to/openbsw
cmake --build build
```

This compiles `demo/` as a STATIC library. It does not produce a runnable
executable on its own — the demo is designed to be linked into
openbsw's `referenceApp` by editing
`executables/referenceApp/application/src/app/app.cpp` to call
`demo::bootstrap_schedule(...)` from `startApp`. See `DESIGN.md §7`.

### 3. Demo built with openbsw fetched from GitHub

Requires a network connection at configure time.

```bash
cmake -S . -B build \
      -DRT_SCHED_BUILD_DEMO=ON \
      -DRT_SCHED_FETCH_OPENBSW=ON
cmake --build build
```

## Status

This is an experiment, not a production library. The Registry singleton is
deliberately simple; the RM priority assignment is only a hint; the
feasibility analysis uses sufficient (not exact) tests. See `DESIGN.md §9`
for the roadmap of what would need to change before upstreaming.
