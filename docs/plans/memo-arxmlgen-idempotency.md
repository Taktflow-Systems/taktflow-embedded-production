# Memo: arxmlgen Pipeline Idempotency Triage & Fix Plan

**Status:** TRIAGE COMPLETE — fix steps S-AG-01..S-AG-04 and S-AG-06
implemented on `fix/arxmlgen-idempotency` (twice-run check green, 8 new unit
tests, no new failures in the arxmlgen suite vs the 35 pre-existing at
00299a9); S-AG-05 (baseline regeneration commit) PENDING review.
**Branch:** `fix/arxmlgen-idempotency`
**Date:** 2026-07-09

## How to read this

Audience: an AI worker (or human) landing cold with no prior conversation
context. Section 1 reproduces the defect, section 2 gives the exact drift,
section 3 the root causes, section 4 the fix plan as steps `S-AG-NN`, each
with Goal / Inputs / Deliverables / Acceptance criteria / Gate / Definition
of done per `.claude/rules/` plan-writing conventions. All paths are
repo-relative. Run all commands from the repo root.

## 1. Reproduction

The "full pipeline" is what CI runs (`.github/workflows/ci.yml`, Step 3),
NOT the bare invocation documented in `README.md`:

```bash
# Full pipeline (CI-equivalent), run from repo root:
python tools/arxml/dbc2arxml.py gateway/taktflow_vehicle.dbc arxml_v2/ arxml_v2/swc_model.json
cp arxml_v2/TaktflowSystem.arxml arxml/TaktflowSystem.arxml
python -m tools.arxmlgen --config project.yaml

# Reproduce:
#   run once  -> git diff        (drift vs committed baseline = D2, D1)
#   git add -A (stage run-1 output as comparison baseline; do not commit)
#   run again -> git diff        (drift vs run 1 = D1 only)
```

Observed on a clean checkout of `feat/os-osek-migration` (00299a9),
Python 3.14, and equally on CI's Python 3.12 — the defect is
version-independent (see root cause D1).

**WARNING — bare invocation hazard (D3):** `README.md` and
`docs/arxmlgen/user-guide.md` document
`python tools/arxml/dbc2arxml.py gateway/taktflow_vehicle.dbc arxml/`
(no model JSON, output straight into `arxml/`). That form silently deletes
the entire SWC model from `arxml/TaktflowSystem.arxml`: 48
`APPLICATION-SW-COMPONENT-TYPE`s, 848 ports, 71 runnables (~4574 lines),
and flips two E2E parameters (`MAX-DELTA-COUNTER-INIT`,
`MAX-NO-NEW-OR-REPEATED-DATA`). Never run it without the third
(`swc_model.json`) argument.

## 2. Exact drift

### Run 1 vs committed baseline (~30 files with content changes)

| File | Drift | Committed decision reverted |
|---|---|---|
| `firmware/ecu/cvc/cfg/Com_Cfg_Cvc.c` | `CVC_COM_TX_FZC_VIRTUAL_SENSORS` (CAN 0x600) and `CVC_COM_TX_RZC_VIRTUAL_SENSORS` (0x601): `COM_TX_MODE_NONE`/0ms → `COM_TX_MODE_PERIODIC`/10ms | Commit `4ef2bde` "fix(cvc): remove virtual sensor TX — plant-sim is sole sender" |
| `firmware/ecu/cvc/cfg/CanIf_Cfg_Cvc.c` | 3-line rationale comment for the 0x600/0x601 placeholder entries deleted | Same commit `4ef2bde` |
| `firmware/ecu/fzc/cfg/Com_Cfg_Fzc.c` | `FZC_COM_TX_MOTOR_CUTOFF_REQ` (CAN 0x211): `COM_TX_MODE_DIRECT`/0ms → `COM_TX_MODE_PERIODIC`/50ms | Commit `46629dc` "fix(fzc): Motor_Cutoff_Req periodic→direct to stop phantom cutoff" |
| `arxml/TaktflowSystem.arxml`, `arxml_v2/TaktflowSystem.arxml` | SWC P/R port ordering reshuffled | none (pure churn, see D1) |
| 28 × `firmware/ecu/*/include/Rte_Swc_*.h` | ~5670 lines of accessor-function reordering | none (pure churn, see D1) |

Reverting `4ef2bde` re-enables periodic CVC TX on 0x600/0x601, which
collides with plant-sim (the sole intended sender on SIL). Reverting
`46629dc` re-introduces the phantom-cutoff defect (periodic transmission of
a safety-relevant event message, @satisfies SG-008).

### Run 2 vs run 1 (true nondeterminism)

30 files, 5641 insertions / 5641 deletions — `arxml/TaktflowSystem.arxml`
+ `arxml_v2/TaktflowSystem.arxml` (1662 `P-PORT-PROTOTYPE`-related lines
reordered) and the same 28 `Rte_Swc_*.h` headers reshuffle **again**.
CanIf/Com/PduR/E2E/Os configs are stable run-to-run — only the SWC
port order churns.

Additionally ~80 more files show as modified in `git status` but produce an
empty `git diff`: the generators write LF while the working tree is
checked out CRLF (D4, cosmetic on Windows, absent on CI/Linux).

## 3. Root cause

**D1 — nondeterministic SWC port order (`tools/arxml/dbc2arxml.py`).**
`_create_swc_types()` builds `tx_sigs` / `rx_sigs` as Python `set`s of
signal names (lines ~530-531) and `_create_one_swc()` iterates them
directly (`for sn in tx_sigs:`). Python string hashing is randomized per
process (PYTHONHASHSEED), so set iteration order — hence P/R-port document
order in the ARXML, hence `Rte_Swc_*.h` accessor order downstream in
arxmlgen — changes on every run. All other `set()`s in the converter are
membership-only dedup guards iterated via ordered `db.messages`; they are
harmless.

**D2 — TX-mode decisions not representable in the pipeline inputs.**
The Com generator template (`tools/arxmlgen/templates/com/Com_Cfg.c.j2`,
TX table) derives the mode purely from cycle time:
`cycle_ms == 0 → COM_TX_MODE_DIRECT, else COM_TX_MODE_PERIODIC`.
`COM_TX_MODE_NONE` (which exists in `firmware/bsw/services/Com/include/Com.h`
and is handled by `Com.c`) cannot be produced at all, and:

- **D2a (0x211):** the DBC *already encodes* the decision —
  `BA_ "GenMsgSendType" BO_ 529 "event"` — but
  `tools/arxmlgen/reader.py` only reads `GenMsgCycleTime` (line ~197) and
  never `GenMsgSendType`. Cycle 50ms → PERIODIC wins.
  Caveat found during implementation: `EStop_Broadcast` (BO_ 1, CAN 0x001)
  is *also* `GenMsgSendType "event"` in the DBC, yet its committed Com row
  is `PERIODIC`/10ms (the DBC comment documents "event + 10ms repeat x4",
  which this Com stack models as continuous periodic TX). A blanket
  event→DIRECT rule would therefore flip 0x001 too — 0x001 must be pinned
  `periodic` in the sidecar (S-AG-03).
- **D2b (0x600/0x601):** the DBC declares CVC as sender with
  `GenMsgSendType "Cyclic"` @10ms because the DBC grammar requires a sender
  node and plant-sim is not a DBC node. The "CVC must not transmit"
  decision exists **only** in the hand-edited generated output. There is no
  sidecar mechanism for it (the sidecar's `message_routing` only supports
  `additional_tx_ecus`).

Both committed decisions were hand-edits to `/* GENERATED */` files
(violating `.claude/rules/global.md`), which is precisely why regeneration
reverts them. Per the repo's DBC-first principle the fix is to encode them
in the inputs (DBC attribute + sidecar), never in the output.

**D3 — divergent documented invocation.** See warning in section 1.
`README.md` / `docs/arxmlgen/user-guide.md` document the bare 2-arg
invocation; CI uses the 3-arg one. Anyone following the README destroys
the committed ARXML SWC model.

**D4 — EOL noise (cosmetic).** Generators write LF; Windows worktrees
check out CRLF. Empty content diff; no action beyond noting it.

## 4. Fix plan

### S-AG-01 — Deterministic SWC port order

- **Goal:** dbc2arxml emits SWC ports in a stable, input-defined order so
  the ARXML and every downstream `Rte_Swc_*.h` are byte-identical across
  runs.
- **Inputs:** `tools/arxml/dbc2arxml.py` (`_create_swc_types`,
  `_create_one_swc`).
- **Deliverables:** `tools/arxml/dbc2arxml.py` — `tx_sigs`/`rx_sigs`
  converted to sorted lists at the build site.
- **Acceptance criteria:**
  - Full pipeline run twice on a clean tree → `git diff` between run 1 and
    run 2 outputs is empty (checked by S-AG-04 script).
  - `PYTHONHASHSEED=1` and `PYTHONHASHSEED=2` runs produce identical ARXML.
- **Gate / review:** PR review on `fix/arxmlgen-idempotency`; CI Step 4
  (ARXML validation) must stay green.
- **Definition of done:** S-AG-04 script passes its run-vs-run leg.

### S-AG-02 — Honor DBC GenMsgSendType (event → DIRECT)

- **Goal:** a DBC message with `GenMsgSendType` = `event` (case-insensitive;
  DBC also uses `Event`) generates `COM_TX_MODE_DIRECT` with `cycleMs 0`
  regardless of `GenMsgCycleTime`, so the committed 0x211 decision comes
  from the DBC.
- **Inputs:** `gateway/taktflow_vehicle.dbc` (already has
  `BA_ "GenMsgSendType" BO_ 529 "event"` — no DBC change needed),
  `tools/arxmlgen/reader.py`, `tools/arxmlgen/model.py`,
  `tools/arxmlgen/templates/com/Com_Cfg.c.j2`.
- **Deliverables:**
  - `tools/arxmlgen/model.py` — `Pdu.tx_mode: str = ""` field
    (`"" | "DIRECT" | "PERIODIC" | "NONE"`; empty = legacy cycle heuristic).
  - `tools/arxmlgen/reader.py` — parse `GenMsgSendType` into a
    `_dbc_send_type_map`; set `Pdu.tx_mode = "DIRECT"` for event messages.
    RX-side E2E derivations keep using `_dbc_cycle_map` (unchanged).
  - `tools/arxmlgen/templates/com/Com_Cfg.c.j2` — TX table emits mode from
    `pdu.tx_mode` when set, else the existing cycle heuristic; cycleMs
    forced to 0 for DIRECT/NONE.
  - `tools/arxmlgen/tests/test_tx_mode.py` — unit tests.
- **Acceptance criteria:**
  - Regenerated `Com_Cfg_Fzc.c` row for `FZC_COM_TX_MOTOR_CUTOFF_REQ` reads
    `0u, COM_TX_MODE_DIRECT` (matches committed decision of `46629dc`).
  - No other ECU's Com TX row changes mode (only 0x211 has send type
    `event` today) — assert via S-AG-04 baseline diff review.
  - `python -m pytest tools/arxmlgen/tests/test_tx_mode.py` passes.
- **Gate / review:** PR review on `fix/arxmlgen-idempotency`.
- **Definition of done:** unit test asserting event→DIRECT passes and the
  regenerated FZC row matches the committed decision.

### S-AG-03 — Sidecar per-ECU TX-mode override (NONE)

- **Goal:** the "CVC never transmits 0x600/0x601, plant-sim is sole sender"
  decision is encoded in `model/ecu_sidecar.yaml` and survives any
  regeneration.
- **Inputs:** `model/ecu_sidecar.yaml`, `tools/arxmlgen/reader.py`,
  S-AG-02's `Pdu.tx_mode` plumbing.
- **Deliverables:**
  - `model/ecu_sidecar.yaml` — new documented section:
    ```yaml
    # TX-mode overrides: force a Com TX mode for the DBC sender ECU.
    # 'none' keeps the PDU slot (index consistency) but never transmits.
    message_tx_mode:
      FZC_Virtual_Sensors: none   # plant-sim is sole sender on SIL (4ef2bde)
      RZC_Virtual_Sensors: none   # plant-sim is sole sender on SIL (4ef2bde)
      EStop_Broadcast: periodic   # DBC says event, Com models the
                                  # "event + 10ms repeat" strategy as periodic
    ```
  - `tools/arxmlgen/reader.py` — load `message_tx_mode`, apply to the
    sender ECU's `Pdu.tx_mode` (sidecar override wins over DBC send type).
  - `tools/arxmlgen/tests/test_tx_mode.py` — override precedence tests.
- **Acceptance criteria:**
  - Regenerated `Com_Cfg_Cvc.c` rows for 0x600/0x601 read
    `0u, COM_TX_MODE_NONE` (matches committed decision of `4ef2bde`).
  - Regenerated `Com_Cfg_Cvc.c` row for 0x001 `EStop_Broadcast` stays
    `10u, COM_TX_MODE_PERIODIC` (sidecar pin beats event→DIRECT).
  - Template emits a generator-standard marker comment
    (`TX disabled via sidecar`) on NONE rows; the hand-written rationale
    prose from `4ef2bde` is superseded by the sidecar comments above.
  - Precedence order is sidecar > GenMsgSendType > cycle heuristic, proven
    by unit test.
- **Gate / review:** PR review on `fix/arxmlgen-idempotency`.
- **Definition of done:** unit tests pass and regenerated CVC rows carry
  `COM_TX_MODE_NONE`.

### S-AG-04 — CI twice-run idempotency check

- **Goal:** CI (and any developer) can prove in one command that the full
  pipeline is idempotent: second run produces zero diff over the first.
- **Inputs:** S-AG-01..03 merged; `.github/workflows/ci.yml` Step 3 as the
  canonical invocation.
- **Deliverables:** `tools/ci/check_codegen_idempotency.sh` — runs the full
  pipeline twice from the current tree, fails (exit 1, prints diffstat) if
  the second run changes any file relative to the first; `--against-head`
  flag additionally fails if run 1 differs from the committed baseline
  (usable as a CI gate after S-AG-05).
- **Acceptance criteria:**
  - Script exits 0 on the fixed pipeline (run-vs-run leg).
  - Script exits 1 when either defect is re-introduced (verified once by
    reverting S-AG-01 locally).
  - Script only ever compares/restores files the pipeline itself writes
    (`arxml/`, `arxml_v2/TaktflowSystem.arxml`, `firmware/ecu/*/cfg`,
    `firmware/ecu/*/include`, `firmware/ecu/*/src`); it must not touch
    unrelated working-tree changes.
- **Gate / review:** wire into `.github/workflows/ci.yml` as a step after
  ARXML validation (separate follow-up commit; CI edit is ungated locally).
- **Definition of done:** `bash tools/ci/check_codegen_idempotency.sh`
  exits 0 on `fix/arxmlgen-idempotency`.

### S-AG-05 — One-time baseline regeneration commit (PENDING — needs review)

- **Goal:** commit the new canonical generated baseline so that
  `--against-head` holds: full pipeline run on a clean tree = zero diff,
  with the TX-mode decisions intact.
- **Inputs:** S-AG-01..04 merged; clean tree.
- **Deliverables:** one `chore(codegen):` commit (per
  `.claude/rules/development-discipline.md` §7 — generated files committed
  separately) containing regenerated `arxml/TaktflowSystem.arxml`,
  `arxml_v2/TaktflowSystem.arxml`, `firmware/ecu/*/cfg/**`,
  `firmware/ecu/*/include/Rte_*`, with:
  - 0x600/0x601 rows: `COM_TX_MODE_NONE` (decision preserved),
  - 0x211 row: `COM_TX_MODE_DIRECT` (decision preserved),
  - `Rte_Swc_*.h` accessors in the new sorted order (one-time churn,
    ~5.6k lines — content-equivalent, ordering only).
- **Acceptance criteria:**
  - `bash tools/ci/check_codegen_idempotency.sh --against-head` exits 0.
  - `make test` (unit/SIL layers runnable on the host) passes; POSIX build
    compiles.
  - Reviewer confirms the only value-level diffs vs the old baseline are
    the two E2E ARXML parameters already drifting today and the
    generator-standard comments replacing hand-written ones.
- **Gate / review:** human review REQUIRED before this commit — it rewrites
  every ECU's committed configs and must not land while other branches
  (e.g. the OS migration) have open work against the old baseline.
  Coordinate merge order with `feat/os-osek-migration`.
- **Definition of done:** `--against-head` green on the branch after the
  chore(codegen) commit.

### S-AG-06 — Fix documented invocation (D3)

- **Goal:** no documented command can silently destroy the ARXML SWC model.
- **Inputs:** `README.md`, `docs/arxmlgen/user-guide.md`,
  `tools/arxml/dbc2arxml.py`.
- **Deliverables:**
  - `README.md` + `docs/arxmlgen/user-guide.md` — replace the bare 2-arg
    example with the CI-equivalent 3-arg sequence from section 1.
  - `tools/arxml/dbc2arxml.py` — print a prominent warning when invoked
    without the model JSON ("SWC model NOT merged — output will lack all
    SWCs; pass arxml_v2/swc_model.json").
- **Acceptance criteria:** grep for the bare invocation in README/docs
  returns no hits; warning appears on bare invocation.
- **Gate / review:** PR review on `fix/arxmlgen-idempotency`.
- **Definition of done:** docs show only the 3-arg pipeline.

## 5. Overall acceptance (from the task definition)

1. Full pipeline run twice = zero diff between the runs (S-AG-01, S-AG-04).
2. Committed TX-mode configuration (0x600/0x601 `NONE`, 0x211 `DIRECT`)
   survives regeneration because it is derived from DBC + sidecar, not from
   hand-edited output (S-AG-02, S-AG-03, proven at S-AG-05).
