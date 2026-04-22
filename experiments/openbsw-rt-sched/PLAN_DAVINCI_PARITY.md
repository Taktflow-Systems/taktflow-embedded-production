# PLAN — DaVinci/TA-parity features for rtScheduling

## How to read this

**Audience**: an AI worker landing cold. You have access to the file paths below and can run the commands listed. You do not have the conversation that led to this plan — everything you need is written down here.

**Step structure**. Every step carries a fixed shape:

- **Step ID** — stable token (e.g. `V1-S01`). Use to cross-reference.
- **Goal** — one sentence, what outcome this step produces.
- **Inputs** — concrete artifacts that must exist before you start. If none, the field says `none`.
- **Deliverables** — concrete file paths. If it's a code change, the function/module is named.
- **Acceptance criteria** — bullets each checkable by a test run or a grep. No vague language.
- **Gate / review reference** — the test label / static_assert / command that gates completion.
- **Definition of done** — one observable fact that closes the step.

**Gates live here**:
- `cmake --preset tests-posix-debug && cmake --build --preset tests-posix-debug` in `d:\openbsw` is the authoritative compile gate.
- `ctest --preset tests-posix-debug -L rtSchedulingTest --output-on-failure` is the authoritative test gate; 100% required.
- `arm-none-eabi-g++ -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mthumb -std=c++17 -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -Werror -Os -I <include>` on `Registry.cpp`, `Json.cpp`, and the header-integration test is the embedded-cleanliness gate.
- `node --check` on the inline JS of `gui/schedule-audit.html` is the GUI-syntax gate.
- `python3 -m json.tool` on every sample JSON is the format gate.

**Rules**:
- STRICT MODE applies: do only the step you are on, no side actions, no destructive git.
- Every code file keeps its existing copyright header (`// Copyright 2026 Accenture.` in `d:\openbsw\libs\bsw\rtScheduling\`, `// Copyright 2026 Taktflow.` in `H:\taktflow-embedded-production\experiments\openbsw-rt-sched\libs\rtScheduling\`). Do not cross the two conventions.
- The two copies of the library (taktflow dev + openbsw upstream shape) diverge only on that one line. If you touch one side, touch the other.
- After each phase, the entire test suite in openbsw's harness must pass before moving to the next phase.

**Repositories you will touch**:
- **DEV**: `H:\taktflow-embedded-production\experiments\openbsw-rt-sched\` — fast iteration, FetchContent gtest.
- **UPSTREAM**: `d:\openbsw\libs\bsw\rtScheduling\` — authoritative, on branch `experiment/rt-sched`.
- **GUI ONLY**: `H:\taktflow-embedded-production\experiments\openbsw-rt-sched\gui\` — lives in the dev tree only; does not get ported to openbsw.

---

# Phase V — DaVinci / TA Tool Suite parity

This phase closes the four gaps identified in the earlier review:

1. **V.1** ISR preemption (all contexts, not just the running task's context)
2. **V.2** OS overhead per entity (context switch + RTE dispatcher cost)
3. **V.3** Event chains (end-to-end latency across task boundaries)
4. **V.4** Gantt SVG over one hyperperiod

Each sub-phase has its own step sequence. Phases run in order; within a phase, steps run in the listed order.

---

## V.1 — ISR preemption

ISRs run at hardware priority and preempt every task on every RTOS context. The current RTA only considers same-context interference, which is optimistic. This phase adds a `Kind` discriminator to `EntityDecl` so ISRs and Tasks coexist in one manifest, and extends every feasibility function to treat `Kind::Interrupt` entities as system-wide interference.

### V1-S01 — Add `Kind` enum to Types.h

**Goal**: Introduce a kind discriminator so the same manifest can list tasks and ISRs without a separate data type.

**Inputs**: none.

**Deliverables**:
- Edit `libs/rtScheduling/include/rtScheduling/Types.h`:
  - Add `enum class Kind : std::uint8_t { Task, Interrupt };` directly under the existing `OverrunPolicy` enum.
  - Add `Kind kind = Kind::Task;` as a new field in `struct EntityDecl`, positioned between `overrun` and `priority_hint`.
- No other file changes.

**Acceptance criteria**:
- Existing unit tests still compile without modification (field has a default).
- `grep -n "Kind::Task" libs/rtScheduling/include/rtScheduling/Types.h` returns at least one line.

**Gate**: `cmake --build build -j` in the dev tree (openbsw unit-test preset not yet needed).

**Definition of done**: Types.h compiles; all pre-existing 48 tests still pass.

### V1-S02 — Handle `Kind::Interrupt` in `entity_is_sane` and `deadlines_plausible`

**Goal**: ISR entities have a cycle (min inter-arrival) and a WCET but may not have a conventional deadline. Validation must not reject legal ISR declarations.

**Inputs**: V1-S01 complete.

**Deliverables**:
- Edit `libs/rtScheduling/include/rtScheduling/Feasibility.h`:
  - Update `entity_is_sane(EntityDecl const& e)`: if `e.kind == Kind::Interrupt`, skip the `effective_deadline() > cycle_us` check. Keep the `cycle_us > 0`, `wcet_us > 0`, `phase_us < cycle_us` checks.
  - Update `deadlines_plausible(DeclView v)`: for `Kind::Interrupt` entries, only check `wcet_us <= cycle_us` (no deadline concept).

**Acceptance criteria**:
- An ISR with `deadline_us = 0` passes sanity.
- An ISR with `wcet_us > cycle_us` fails sanity (ISR can't run longer than its minimum inter-arrival).

**Gate**: New test `Feasibility.IsrSanity_AcceptsIsrWithoutDeadline` and `Feasibility.IsrSanity_RejectsIsrLongerThanCycle` added in V1-S05.

**Definition of done**: entity_is_sane returns true for a well-formed ISR and false for a malformed one.

### V1-S03 — Per-context utilization excludes ISRs; new `isr_utilization_ppm`

**Goal**: Show task CPU per context (unchanged semantics) plus a separate system-wide ISR CPU so a reviewer can see the split.

**Inputs**: V1-S01 complete.

**Deliverables**:
- Edit `libs/rtScheduling/include/rtScheduling/Feasibility.h`:
  - `utilization_ppm(v, ctx)`: skip entities with `kind == Kind::Interrupt` entirely (they are not a per-context load).
  - `count_on_context(v, ctx)`: same — skip ISRs.
  - Add `constexpr std::uint32_t isr_utilization_ppm(DeclView v) noexcept` computing Σ(C/T) for all `Kind::Interrupt` entries.

**Acceptance criteria**:
- `utilization_ppm(manifest_with_one_task_and_one_isr, task_ctx)` returns the task's u_ppm only.
- `isr_utilization_ppm(same_manifest)` returns the ISR's u_ppm.

**Gate**: Tests `Feasibility.IsrDoesNotInflatePerContextUtilization` and `Feasibility.IsrUtilization_SummedAcrossIsrs` in V1-S05.

**Definition of done**: Both functions return the expected numbers on a hand-worked 1-task-1-ISR manifest.

### V1-S04 — Extend RTA to account for ISR interference

**Goal**: Every task's worst-case response time must include interference from every ISR (all of them, regardless of context). Blocking term and same-context preemption logic stays as-is.

**Inputs**: V1-S01 through V1-S03.

**Deliverables**:
- Edit `libs/rtScheduling/include/rtScheduling/Feasibility.h`:
  - In the body of `rta_response_time_us`, replace the inner `hp` check with:
    ```
    bool const isr_preempts       = (other.kind == Kind::Interrupt);
    bool const same_ctx_strict_hp = (other.kind == Kind::Task) && (other.context == ctx) && (other.timing.cycle_us <  Ti);
    bool const same_ctx_tied_hp   = (other.kind == Kind::Task) && (other.context == ctx) && (other.timing.cycle_us == Ti) && (j < idx);
    if (!(isr_preempts || same_ctx_strict_hp || same_ctx_tied_hp)) continue;
    ```
  - Also: when computing response time for an ISR entity itself, use the same logic — an ISR can be preempted by other higher-priority ISRs. Since ISRs don't belong to a task context, `ctx` matching is irrelevant for them; treat ISR-to-ISR preemption as "any other ISR with shorter period is hp". Keep it simple: don't try to compute an ISR's own response time in this phase — `rta_response_time_us` on an ISR index returns `C + B` and checks against `cycle_us`. That is a TODO for a follow-up.

**Acceptance criteria**:
- Given a task with R=C=5 at T=100 and an ISR with T=10, C=1, RTA must produce R_task > 5 (the ISR steals 1 µs every 10 µs).
- Computed by hand: R_task init = 5; iter 1 = 5 + ⌈5/10⌉·1 = 6; iter 2 = 5 + ⌈6/10⌉·1 = 6 (converged); R=6.
- Cross-context preemption: a task on ctx 2 feels the ISR; a task on ctx 1 also feels it. Both get the same +1 µs bump per ISR period.

**Gate**: Test `Rta.IsrPreemptsAcrossContexts` in V1-S05.

**Definition of done**: Hand-worked example above produces R=6.

### V1-S05 — V.1 test suite

**Goal**: Nail down V.1 semantics with tests that will also act as regression gate for future refactors.

**Inputs**: V1-S01 through V1-S04.

**Deliverables**:
- Edit `libs/rtScheduling/test/src/FeasibilityTest.cpp`: add tests
  - `Feasibility.IsrSanity_AcceptsIsrWithoutDeadline`
  - `Feasibility.IsrSanity_RejectsIsrLongerThanCycle`
  - `Feasibility.IsrDoesNotInflatePerContextUtilization`
  - `Feasibility.IsrUtilization_SummedAcrossIsrs`
- Edit `libs/rtScheduling/test/src/RtaTest.cpp`: add tests
  - `Rta.IsrPreemptsAcrossContexts` — task on ctx 2 + ISR; R inflates by ISR contribution
  - `Rta.TwoIsrsAndOneTask` — R includes both ISR contributions summed

**Acceptance criteria**:
- All new tests pass on the openbsw harness (`ctest -L rtSchedulingTest`).
- All pre-existing 48 tests still pass.

**Gate**: `ctest --preset tests-posix-debug -L rtSchedulingTest --output-on-failure` reports 100%.

**Definition of done**: Test count 48 → ≥ 54, all green.

---

## V.2 — OS overhead per entity

A real context switch, RTE dispatcher, stack save/restore takes non-zero time. Ignoring it makes RTA optimistic. This phase adds an additive `os_overhead_us` to `Timing`, folds it into every math path, and exposes it in JSON + GUI.

### V2-S01 — Add `os_overhead_us` to Timing + helper

**Goal**: Extend the timing struct with one optional field and a helper that returns the effective compute budget.

**Inputs**: V.1 complete.

**Deliverables**:
- Edit `libs/rtScheduling/include/rtScheduling/Types.h`:
  - In `struct Timing`, add `Duration os_overhead_us = 0;` between `phase_us` and the closing brace.
  - Add a `constexpr Duration total_wcet_us() const noexcept { return wcet_us + os_overhead_us; }` member on `Timing`.

**Acceptance criteria**:
- `Timing{1000, 100}.total_wcet_us() == 100`.
- `Timing{1000, 100, 0, 0, 15}.total_wcet_us() == 115`.

**Gate**: Build gate.

**Definition of done**: Types.h compiles, helper returns expected values.

### V2-S02 — Use `total_wcet_us` in feasibility math

**Goal**: Every utilization/interference term must account for OS overhead.

**Inputs**: V2-S01.

**Deliverables**:
- Edit `libs/rtScheduling/include/rtScheduling/Feasibility.h`:
  - `utilization_ppm`: replace `e.timing.wcet_us` with `e.timing.total_wcet_us()` in the ppm formula.
  - `hyperbolic_product_ppm`: same.
  - `utilization_by_criticality_ppm`: same.
  - `isr_utilization_ppm` (added in V1-S03): same.
  - `rta_response_time_us`: use `me.timing.total_wcet_us()` for `Ci` and `other.timing.total_wcet_us()` for `Cj`.

**Acceptance criteria**:
- Given `Timing{1000, 80, 0, 0, 20}` alone on a context, `utilization_ppm` returns 100'000 (10%, because 100/1000 = 0.10).
- Given a task `T=1000, C=100, O=0` and an ISR `T=10, C=1, O=1`, RTA for the task includes ⌈R/10⌉·2 per iteration, not ⌈R/10⌉·1.

**Gate**: Tests `V2.OsOverhead_InflatesUtilization` and `V2.OsOverhead_InflatesRta` in V2-S04.

**Definition of done**: Hand-worked examples reproduce in tests.

### V2-S03 — Keep `wcet_us` field stable; no renames

**Goal**: The public `wcet_us` field name stays — it represents the pure computation. `total_wcet_us()` is the new helper. This avoids a breaking rename across the whole codebase.

**Inputs**: none.

**Deliverables**: none (this is a design constraint applied during V2-S02).

**Acceptance criteria**:
- `grep -rn 'wcet_us' libs/rtScheduling/include/ libs/rtScheduling/src/` still returns results from pre-existing code — we did not rename.

**Gate**: none.

**Definition of done**: documented in this plan.

### V2-S04 — V.2 tests

**Deliverables**:
- Edit `libs/rtScheduling/test/src/FeasibilityTest.cpp`: add
  - `V2.OsOverhead_InflatesUtilization`
  - `V2.OsOverhead_InflatesHyperbolicBound`
- Edit `libs/rtScheduling/test/src/RtaTest.cpp`: add
  - `V2.OsOverhead_InflatesRta`
  - `V2.OsOverhead_TogglesFeasibility` (a schedule that passes RTA with O=0 and fails with O=50)

**Acceptance criteria**: all four tests pass on the openbsw harness.

**Gate**: `ctest -L rtSchedulingTest` 100%.

**Definition of done**: Test count ≥ 58; all green.

---

## V.3 — Event chains

A DaVinci TIMEX `LatencyTimingConstraint` defines an end-to-end latency across multiple runnables (e.g. sensor-read → filter → control → actuator-write). This phase adds a simple chain representation, a latency computation that sums the per-entity response times along the path, and a pass/fail against a declared `max_latency_us`.

### V3-S01 — Add `EventChainDecl` type

**Goal**: A constexpr-constructible struct describing an ordered chain of entities and its latency bound.

**Inputs**: V.2 complete.

**Deliverables**:
- Edit `libs/rtScheduling/include/rtScheduling/Types.h`:
  ```cpp
  struct EventChainDecl {
      static constexpr std::size_t MAX_PATH = 8;
      std::string_view name;
      std::array<std::size_t, MAX_PATH> path;  // indices into the entity manifest
      std::size_t path_len;
      Duration max_latency_us;
  };
  ```

**Acceptance criteria**:
- Compiles; is constexpr-constructible with brace-init.

**Gate**: build gate.

**Definition of done**: Types.h compiles with a sample `constexpr EventChainDecl CHAIN{...};` declared in a test.

### V3-S02 — `chain_latency_us` computation

**Goal**: Given a DeclView and an EventChainDecl, compute the worst-case end-to-end latency as Σ R_i for i in path.

**Inputs**: V3-S01.

**Deliverables**:
- Edit `libs/rtScheduling/include/rtScheduling/Feasibility.h`: add
  ```cpp
  constexpr Duration chain_latency_us(DeclView v, EventChainDecl const& c) noexcept;
  constexpr bool chain_feasible(DeclView v, EventChainDecl const& c) noexcept; // latency <= c.max_latency_us AND no path entity is RTA-infeasible
  ```
- Implementation: for each index in `c.path[0..c.path_len)`, compute `rta_response_time_us(v, idx)`; if any returns `RTA_INFEASIBLE`, return `RTA_INFEASIBLE`; else sum them.

**Acceptance criteria**:
- For a 3-entity chain with R_i = 10, 30, 50, `chain_latency_us` returns 90.
- If any entity on the path is RTA-infeasible, `chain_latency_us` returns `RTA_INFEASIBLE`.

**Gate**: Test `Rta.EventChain_SumsResponseTimes` in V3-S03.

**Definition of done**: Hand-worked sum reproduces.

### V3-S03 — V.3 tests

**Deliverables**:
- Edit `libs/rtScheduling/test/src/RtaTest.cpp`: add
  - `Rta.EventChain_SumsResponseTimes`
  - `Rta.EventChain_InfeasibleEntity_PropagatesInfeasible`
  - `Rta.EventChain_BoundRespected` (chain latency ≤ max_latency_us)
  - `Rta.EventChain_BoundExceeded` (chain latency > max_latency_us → `chain_feasible` returns false)

**Acceptance criteria**: all four pass.

**Gate**: `ctest -L rtSchedulingTest` 100%.

**Definition of done**: Test count ≥ 62; all green.

---

## V.4 — JSON + GUI updates

Fold everything from V.1/V.2/V.3 into the stable JSON format (schema stays `rt-sched/1` — only adds optional fields) and into the single-file HTML.

### V4-S01 — JSON emits new fields

**Goal**: JSON output must carry Kind, os_overhead_us, isr_utilization_ppm, and the chains array so the GUI can render them.

**Inputs**: V.1, V.2, V.3 complete.

**Deliverables**:
- Edit `libs/rtScheduling/src/Json.cpp`:
  - `write_entity`: emit `"kind": "task" | "interrupt"` and `"os_overhead_us": <Duration>`.
  - `write_context_block`: emit `"isr_utilization_ppm": <u32>` (system-wide, same value in every context block; GUI will display it once at the top instead of per-context).
  - `write_report_core`: after the `contexts` array, emit a `"chains": [...]` array. Each chain: `{"name": "...", "path": ["entityA","entityB",...], "latency_us": <u32>, "max_latency_us": <u32>, "feasible": true|false}`.
- Extend `write_report` and `write_report_chunked` to accept `std::span<EventChainDecl const>` (or `EventChainDecl const*, size_t`) in addition to `DeclView`. Backward compat: keep the no-chains overload.

**Acceptance criteria**:
- `grep -q '"kind":"task"' <sample output>`.
- `grep -q '"chains":\[' <sample output>`.
- Parsing with `python3 -m json.tool` succeeds.

**Gate**: Tests `Json.EmitsKindAndOsOverhead`, `Json.EmitsChainsSection` in V4-S02.

**Definition of done**: A sample dump produces the expected fields.

### V4-S02 — JSON tests

**Deliverables**:
- Edit `libs/rtScheduling/test/src/JsonTest.cpp`: add
  - `Json.EmitsKindAndOsOverhead` (both fields present on every entity)
  - `Json.EmitsChainsSection` (chains array with expected names + latencies)
  - `Json.EmptyChains_StillProducesArray` (chains is `[]` when none declared)

**Acceptance criteria**: all three pass.

**Gate**: `ctest -L rtSchedulingTest` 100%.

**Definition of done**: Test count ≥ 65; all green.

### V4-S03 — GUI renders Kind, OS overhead, ISR load, chains

**Goal**: Extend `gui/schedule-audit.html` to show every new field. Keep the existing layout; add sections, do not rearrange the old ones.

**Inputs**: V4-S01 complete; fresh `gui/sample.json` and `gui/sample-divergence.json` regenerated.

**Deliverables**:
- Edit `H:\taktflow-embedded-production\experiments\openbsw-rt-sched\gui\schedule-audit.html`:
  - Manifest table: add a "Kind" column showing `task` or `isr`. Add an "O (µs)" column showing `os_overhead_us` (blank or dim-0 when zero).
  - Header panel: if `isr_utilization_ppm` > 0, render a small banner under the overall verdict: "ISR load: X.XX% (system-wide, preempts every context)".
  - Per-context RTA section: in the equation caption, include the OS-overhead term when any entity on this context has `os_overhead_us > 0`: `R = (C + O) + B + ∑_hp ⌈R/T_j⌉·(C_j + O_j)`. Otherwise keep the existing form.
  - New top-level section after contexts: `<h2>Event chains</h2>`. Table with columns: name, path (→-separated entity names), latency, max latency, verdict (PASS/FAIL). Show the section only if `report.chains.length > 0`.

**Acceptance criteria**:
- Opening the file with the updated sample renders without errors.
- `node --check` on the extracted `<script>` block succeeds.
- Manual check: loading `sample-divergence.json` shows "ISR load" banner iff the sample contains ISRs.

**Gate**: node syntax check + visual inspection.

**Definition of done**: All three new visual elements appear in the GUI; no JS errors.

### V4-S04 — Regenerate samples

**Goal**: Both `gui/sample.json` and `gui/sample-divergence.json` plus the embedded JS string literals inside `schedule-audit.html` must carry the new fields or the GUI defaults them.

**Inputs**: V4-S01 implemented and building.

**Deliverables**:
- Write `tools/dump_sample.cpp` and `tools/dump_divergence.cpp` inside the taktflow dev tree (not in openbsw) — small utility programs that instantiate two manifests, call `write_report`, and print JSON. See the pre-existing `/tmp/dump_sample.cpp` pattern used in Phase I.
- Build both tools linking against the updated `librtScheduling.a`.
- Regenerate both sample JSONs via stdout → file redirect + `python3 -m json.tool`.
- Update the two JS string literals (`SAMPLE_JSON`, `DIVERGENCE_JSON`) in `schedule-audit.html` to match the new file contents exactly.

**Acceptance criteria**:
- `diff gui/sample.json <(tools/dump_sample.exe | python3 -m json.tool)` is empty.
- `diff gui/sample-divergence.json <(tools/dump_divergence.exe | python3 -m json.tool)` is empty.
- The embedded `SAMPLE_JSON` constant in the HTML parses via `JSON.parse` without throwing.

**Gate**: python JSON validation + node JSON.parse sanity.

**Definition of done**: Opening the HTML auto-renders the sample with all V.1-V.3 fields visible.

---

## V.5 — Gantt SVG over one hyperperiod

Pure GUI addition. Draws a horizontal band per `(context, entity)` and rectangles for each dispatch across one hyperperiod. No library changes.

### V5-S01 — Compute dispatch windows in JS

**Goal**: For each entity, compute the list of dispatch start times over one hyperperiod.

**Inputs**: V.4 complete; JSON carries `phase_us` and `cycle_us`.

**Deliverables**:
- Edit `gui/schedule-audit.html` inline JS: add a function `dispatchesFor(entity, hyperperiod)` returning `Array<{start:int, duration:int}>` where `start = phase + k*cycle` and `duration = wcet + os_overhead` for `k` in `[0, floor((hyperperiod - phase) / cycle) + 1)`.

**Acceptance criteria**:
- For phase=0, cycle=10, wcet=2, hyperperiod=30: returns `[{0,2},{10,2},{20,2}]`.
- For phase=5, cycle=10, wcet=2, hyperperiod=30: returns `[{5,2},{15,2},{25,2}]`.

**Gate**: Not tested (GUI-only; manual check).

**Definition of done**: Function exists and returns the expected shape.

### V5-S02 — Render Gantt SVG

**Goal**: For each context, draw an SVG track with rectangles per entity dispatch, colored by criticality, across the hyperperiod.

**Inputs**: V5-S01.

**Deliverables**:
- Edit `gui/schedule-audit.html`:
  - Add a `renderGantt(report)` function returning an HTML string.
  - Structure per context:
    ```
    <svg width="WIDTH" height="CTX_HEIGHT">
      <!-- axis ticks at hyperperiod quarters -->
      <!-- one <g> per entity: a band with rect per dispatch -->
    </svg>
    ```
  - Dimensions: WIDTH = 960px; CTX_HEIGHT = 24 + N_entities * 22; ENTITY_H = 18; BAND_PAD = 4.
  - Color by criticality: reuse the existing `seg-asil_d` / `seg-asil_b` / `seg-qm` CSS classes (translate to SVG `fill`).
  - Add the Gantt as a new `<h2>Dispatch timeline (one hyperperiod)</h2>` section, rendered before the per-context blocks.

**Acceptance criteria**:
- `node --check` clean.
- The sample file renders with a visible Gantt region above the per-context blocks.
- Each entity band shows the correct number of rectangles (hyperperiod / cycle) at the correct x positions.

**Gate**: node syntax + manual visual.

**Definition of done**: Gantt visible, matches expected dispatch count for sample manifest.

---

## V.6 — Port library changes to openbsw + commit

The dev tree (taktflow) is the authoritative source. After V.1-V.3 library work is green in the dev tree, copy to openbsw with copyright-header adjustment.

### V6-S01 — Copy library files

**Goal**: Sync `d:\openbsw\libs\bsw\rtScheduling\` with the post-V.3 state of `H:\taktflow-embedded-production\experiments\openbsw-rt-sched\libs\rtScheduling\`.

**Inputs**: V.1, V.2, V.3 green in the dev tree.

**Deliverables**:
- Copy each updated file from `<DEV>/libs/rtScheduling/` to `<UPSTREAM>/libs/bsw/rtScheduling/` preserving subdirectory structure.
- For each copied .h, .cpp, .rst: run `sed -i 's|// Copyright 2026 Taktflow\.|// Copyright 2026 Accenture.|' <file>`.

**Acceptance criteria**:
- `grep -rl "Copyright 2026 Taktflow" d:\openbsw\libs\bsw\rtScheduling` returns nothing.
- `diff -r <DEV>/libs/rtScheduling <UPSTREAM>/libs/bsw/rtScheduling` shows only the copyright-line differences.

**Gate**: openbsw build + test run.

**Definition of done**: `cmake --build --preset tests-posix-debug && ctest --preset tests-posix-debug -L rtSchedulingTest` 100%.

### V6-S02 — Commit in openbsw

**Goal**: One clean commit on top of `b4428beb` adding the V.1-V.3 features.

**Inputs**: V6-S01 green.

**Deliverables**:
- `cd d:\openbsw && git add libs/bsw/rtScheduling && git commit -m "feat(rtScheduling): ISR preemption, OS overhead, event chains (#370)"`.
- Do NOT push. Do NOT amend `b4428beb`.

**Acceptance criteria**:
- `git log --oneline -2` shows the new commit on top of `b4428beb`.
- `git status` clean after commit.

**Gate**: git log check.

**Definition of done**: Two commits on branch `experiment/rt-sched`; neither pushed.

---

## V.7 — Documentation

### V7-S01 — Update PR_PROPOSAL.md

**Deliverables**:
- Edit `experiments/openbsw-rt-sched/PR_PROPOSAL.md`:
  - Under "Summary", add a sentence: "Includes ISR-global preemption, per-entity OS overhead, and event-chain latency analysis — closing the parity gap with DaVinci / TA Tool Suite scheduling analysis."
  - Under "Test evidence", bump the test count.
  - Under "Design choices", add bullets for the three new capabilities.

**Acceptance criteria**: diff is additive, does not remove prior content.

**Gate**: manual.

**Definition of done**: file committed to taktflow tree (no openbsw effect).

### V7-S02 — Update DESIGN.md §6 ("What the API does NOT do yet")

**Deliverables**:
- Edit `experiments/openbsw-rt-sched/DESIGN.md`:
  - Remove the "No blocking term" bullet (already done in a prior phase; double-check).
  - Remove the "multi-core" bullet ONLY if it was the ISR bullet — keep if it was about true multi-core.
  - Remove the "priority assignment not consumed" bullet — still true, but now the reasoning is documented.
  - Add a new "Done in V.1-V.3" subsection before the roadmap.

**Acceptance criteria**: doc reflects current state; roadmap table updated.

**Gate**: manual.

**Definition of done**: reader landing on DESIGN.md knows what IS and IS NOT supported as of the V.3 commit.

### V7-S03 — Update the handoff YAML

**Deliverables**:
- Edit `H:\handoff\taktflow-embedded-production\experiments-openbsw-rt-sched\2026-04-22-rt-scheduling-experiment-handoff.yaml`:
  - Add a new achievement block for "Phase V — DaVinci/TA parity".
  - Update file counts and test counts.
  - Update `status.summary` to say V.1-V.3 complete, V.4 GUI rendering done, V.5 Gantt done, V.6 committed.
  - Add next_steps: push experiment/rt-sched upstream when ready; decide on follow-up for the G474RE firmware demo.

**Gate**: YAML parses cleanly (`python3 -c "import yaml; yaml.safe_load(open('<path>'))"`).

**Definition of done**: handoff file reflects the post-V.7 state.

---

## Run-book

End-to-end command sequence a worker should follow.

```bash
# 1. Start on the right branch in openbsw
cd /d/openbsw
git status                          # expect clean
git log --oneline -2                # expect b4428beb on top
git rev-parse --abbrev-ref HEAD     # expect experiment/rt-sched

# 2. Work through V.1 in the dev tree first
cd /h/taktflow-embedded-production/experiments/openbsw-rt-sched
# (implement V1-S01..V1-S05, rebuild, ctest — repeat per step)
cmake --build build -j
ctest --test-dir build/libs/rtScheduling/test --output-on-failure

# 3. V.2
# (implement V2-S01..V2-S04, rebuild, ctest)

# 4. V.3
# (implement V3-S01..V3-S03, rebuild, ctest)

# 5. Validate in openbsw harness
cd /d/openbsw
cmake --build --preset tests-posix-debug -j --target rtSchedulingTest
ctest --preset tests-posix-debug -L rtSchedulingTest --output-on-failure

# 6. Port to openbsw (V.6)
# (copy files, flip copyright, rebuild, ctest)
cd /d/openbsw
cmake --build --preset tests-posix-debug -j --target rtSchedulingTest
ctest --preset tests-posix-debug -L rtSchedulingTest --output-on-failure

# 7. V.4 JSON (happened inline with V.1-V.3 via Json.cpp) + GUI updates + sample regen
cd /h/taktflow-embedded-production/experiments/openbsw-rt-sched
# Regenerate samples (V4-S04)
g++ -std=c++17 -I libs/rtScheduling/include tools/dump_sample.cpp      build/libs/rtScheduling/librtScheduling.a -o /tmp/dump_sample.exe
g++ -std=c++17 -I libs/rtScheduling/include tools/dump_divergence.cpp  build/libs/rtScheduling/librtScheduling.a -o /tmp/dump_divergence.exe
/tmp/dump_sample.exe     | python3 -m json.tool > gui/sample.json
/tmp/dump_divergence.exe | python3 -m json.tool > gui/sample-divergence.json

# 8. V.5 Gantt
# (edit gui/schedule-audit.html, syntax check)
python3 -c "import re; open('/tmp/x.js','w').write(re.search(r'<script>(.*?)</script>', open('gui/schedule-audit.html').read(), re.DOTALL).group(1))"
node --check /tmp/x.js

# 9. Commit in openbsw
cd /d/openbsw
git status
git add libs/bsw/rtScheduling
git commit -m "feat(rtScheduling): ISR preemption, OS overhead, event chains (#370)"
git log --oneline -3

# 10. Handoff
# (edit H:\handoff\...\2026-04-22-rt-scheduling-experiment-handoff.yaml)
```

---

## Exit criteria (Phase V done)

All of the following hold simultaneously:

1. `ctest --preset tests-posix-debug -L rtSchedulingTest` reports 65+ tests, 100% pass.
2. Cross-compile gate (`arm-none-eabi-g++ -Werror -Os`) on `Registry.cpp`, `Json.cpp`, and the header-integration test succeeds with no warnings.
3. `gui/schedule-audit.html` opens in a browser and renders the sample with: (a) kind column, (b) OS overhead column, (c) ISR load banner if any, (d) Event-chains section, (e) Gantt timeline. No JS errors in the console.
4. `d:\openbsw` has two local commits on `experiment/rt-sched`: `b4428beb` (original library) and a new one for V.1-V.3.
5. `PR_PROPOSAL.md` and `DESIGN.md` reflect the expanded scope.
6. Handoff YAML updated with Phase V.

Exit criterion 2 is the ASIL-D embeddability claim. Exit criterion 1 is the correctness gate. Exit criterion 3 is the "proves the math" deliverable. Exit criteria 4-6 are project-management loop closure.

---

## Appendix — hand-worked numbers for V.1 tests

These are the exact expected values. If your implementation produces different numbers for the same inputs, your implementation is wrong — do not change the tests.

### Rta.IsrPreemptsAcrossContexts

Manifest:
- Entity 0 (task, ctx=1): T=100, C=5, D=100, O=0
- Entity 1 (isr):          T=10,  C=1, D=10,  O=0

RTA for entity 0:
- init R = 5
- iter 1: 5 + ⌈5/10⌉·1 = 5 + 1 = 6
- iter 2: 5 + ⌈6/10⌉·1 = 5 + 1 = 6 → converged
- Result: R = 6

Duplicate the task onto ctx 2 and assert the same result.

### Rta.TwoIsrsAndOneTask

Manifest:
- Entity 0 (task, ctx=1): T=1000, C=100, D=1000, O=0
- Entity 1 (isr): T=50, C=2, O=0
- Entity 2 (isr): T=200, C=5, O=0

RTA for entity 0:
- init R = 100
- iter 1: 100 + ⌈100/50⌉·2 + ⌈100/200⌉·5 = 100 + 4 + 5 = 109
- iter 2: 100 + ⌈109/50⌉·2 + ⌈109/200⌉·5 = 100 + 6 + 5 = 111
- iter 3: 100 + ⌈111/50⌉·2 + ⌈111/200⌉·5 = 100 + 6 + 5 = 111 → converged
- Result: R = 111

### V2.OsOverhead_InflatesRta

Manifest:
- Entity 0 (task, ctx=1): T=100, C=40, D=100, O=10
- Entity 1 (task, ctx=1): T=50,  C=10, D=50,  O=5

RTA for entity 1 (higher priority):
- total wcet = 10 + 5 = 15
- R = 15 ≤ 50 → OK

RTA for entity 0 (lower priority):
- total wcet = 40 + 10 = 50
- init R = 50
- iter 1: 50 + ⌈50/50⌉·15 = 50 + 15 = 65
- iter 2: 50 + ⌈65/50⌉·15 = 50 + 30 = 80
- iter 3: 50 + ⌈80/50⌉·15 = 50 + 30 = 80 → converged
- Result: R = 80, ≤ 100 → OK

### Rta.EventChain_SumsResponseTimes

Manifest:
- Entity 0 (task, ctx=1, highest): T=100, C=10, D=100, O=0
- Entity 1 (task, ctx=2, only):     T=100, C=30, D=100, O=0
- Entity 2 (task, ctx=3, only):     T=100, C=50, D=100, O=0

Chain "stim→act" path = [0, 1, 2], max_latency_us = 100.

- R_0 = 10 (no hp on ctx 1 since only one task)
- R_1 = 30 (no hp on ctx 2)
- R_2 = 50 (no hp on ctx 3)
- chain_latency_us = 10 + 30 + 50 = **90**
- chain_feasible = true (90 ≤ 100)

Same chain with max_latency_us = 80 → chain_feasible = false.
