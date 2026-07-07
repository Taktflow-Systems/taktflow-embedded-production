---
document_id: HIL-RPT-SC-ETH-01
title: "SC Ethernet Telemetry Bench Validation (S-UDP-05)"
version: "0.1"
status: in-progress
aspice_process: SWE.6
---

# SC Ethernet Telemetry Bench Validation — S-UDP-05

Validates the SC (TMS570) Ethernet telemetry channel under real HIL load per
`docs/plans/plan-sc-ethernet-roadmap.md` S-UDP-05. Gate:
`.claude/rules/development-discipline.md` Layer 5 (multi-node bench).

## How to read this

Audience: an AI or human worker executing or auditing the runs cold. The
Procedure section is the approved scenario plan (2026-07-07); each run
section below it records evidence and carries a status marker
(PENDING / IN PROGRESS / DONE). Bench port assignments live in
`tools/bench/hardware-map.local.toml` (not repeated here by rule). Raw
captures live in `tmp/` (gitignored); this report quotes the derived numbers.

## Acceptance criteria (from the roadmap)

| # | Criterion | Where proven |
|---|---|---|
| 1 | Counter values via UDP match UART within one sampling period (one 5 s dump) | Run C |
| 2 | Packet loss 0 over the scenario run | Run B (all phases) |
| 3 | CAN scenario results unchanged vs a run without ETH=1 | Run A vs Run B |

Criteria 1 and 2 are split across runs deliberately: the only counter-bearing
UART output is the DBGDUMP 5 s dump, whose ~280 ms SCI3 stall itself causes
telemetry loss (S-UDP-03 root cause, commit `87a73b1`). Approved 2026-07-07.

## Telemetry/UART observables

- SCET UDP frame fields: `docs/api/sc-eth-telemetry-format.md`; status packing
  per `firmware/ecu/sc/src/sc_monitoring.c` (`mon_build_payload`).
- UART `[5s]` dump: `firmware/ecu/sc/src/sc_main.c` Step 8b; with `HIL=1`,
  `tx7 call/ok/fail` counters from `sc_hw_tms570.c` (`sc_dcan_tx_stats_get`).
- Tick-exact identities per inter-dump interval (basis of the criterion-1
  counter check): `d(tx7 call) = 50` and `UDP frames + missed_periods = 500`.
  Both counters are loop-tick-driven, so the identities hold regardless of
  the dump stall's wall-clock drift.

## Tooling

- CAN load + fault injection: `scripts/hil/sc_eth_hil_scenario.py`
  (Waveshare USB-CAN-A serial protocol; heartbeats 0x010/0x011/0x012 @ 50 ms
  valid E2E; optional rest-bus 0x600/0x601 @ 10 ms; dropout / CRC-corrupt
  injection; RX log decodes SC_Status 0x013).
- UDP receiver: `tools/bench/eth_telemetry_rx.py`.
- UART capture: `scripts/serial-monitor.py --log` (SC App UART).
- Criterion-1 comparator: `tools/bench/sc_eth_uart_crosscheck.py`
  (stall-aligned; see its docstring for method).

## Procedure

### Pre-checks (before Run B)

1. Confirm nothing else is using the board or UDP port 55001 (operator).
2. Identify the Waveshare adapter COM port (passive:
   `python scripts/debug/waveshare_can_sniffer.py --port <COM> --duration 10`);
   this also records whether SC_Status 0x013 is present on the bus, which
   selects the criterion-3 comparison channel (see Run A).
3. Board image: deployed ELF is `87a73b1` `ETH=1`; its `HIL` flag is not
   determinable from artifacts (TI toolchain stores no macro strings).
   Determined behaviorally in Run B phase 2: dropout reaction ~130 ms and a
   kill on CRC corruption imply non-HIL; ~500+ ms reaction and no reaction
   to corruption imply `HIL=1` (E2E bypass). If `HIL=1`, reflash a plain
   `ETH=1` build (operator approval) and restart Run B.

### Run B — trustworthiness under load (criterion 2) — PENDING

Board: current `ETH=1` image, no reflash. Kill latches (SWR-SC-011):
power-cycle the board between phases 2 and 3.

| Phase | Driver | Receiver |
|---|---|---|
| B1 nominal 600 s | `--phase nominal --duration 600 --restbus` | `--duration 620 --csv tmp/sc_eth_sudp05_runB1.csv --expect-rate-hz 100 --rate-tolerance 0.01 --fail-on-gap` |
| B2 FZC dropout | `--phase dropout --target-ecu fzc --inject-at 60 --hold 30` | `--duration 110 --csv tmp/sc_eth_sudp05_runB2.csv --expect-rate-hz 100 --rate-tolerance 0.01 --fail-on-gap` |
| B3 CVC CRC corrupt | `--phase corrupt --target-ecu cvc --inject-at 60 --hold 30` | `--duration 110 --csv tmp/sc_eth_sudp05_runB3.csv --expect-rate-hz 100 --rate-tolerance 0.01 --fail-on-gap` |

Driver invocations add `--port <adapter> --tx-log tmp/..._tx.csv --rx-log
tmp/..._rx.csv`. Start receiver first, then driver, then power-cycle the
board so the startup grace expires into a heartbeat-present bus.

Pass: receiver exit 0 in all phases (100 Hz +/- 1%, zero gaps, zero
invalid); B2 shows FZC fault flag + kill reason HB_TIMEOUT with relay 0;
B3 shows a kill (reason HB_TIMEOUT or E2E_FAIL — record which) while the
stream stays gap-free through both injections.

### Run C — UART cross-check (criterion 1) — PENDING

Build + flash (operator approval): `ETH=1 DBGDUMP=1 HIL=1` at committed HEAD
(`flash_tms570.sh` enforces hash-in-binary). Phases: nominal 300 s, then FZC
dropout `--inject-at 60 --hold 60` (500 ms timeout under HIL). Receiver runs
WITHOUT `--fail-on-gap` (dump stalls are expected and quantified); UART
captured with `scripts/serial-monitor.py --log` on the same host. Evaluate:

```
python tools/bench/sc_eth_uart_crosscheck.py \
    --udp-csv tmp/sc_eth_sudp05_runC.csv \
    --uart-log tmp/sc_eth_sudp05_runC_uart.log \
    --out test/hil/reports/artifacts/sudp05_runC_crosscheck.md
```

Pass: comparator exit 0 — every dump MATCH or TRANSITION, counter
identities within +/- 1, alignment sane.

### Run A — no-ETH baseline (criterion 3) — PENDING

Build + flash (operator approval): default build (no `ETH`, no `DBGDUMP`,
HIL flag matching Run B's determined flag). Re-run phases B1 (shortened to
120 s), B2, B3 with identical driver commands. Observables: driver RX log
(0x013 cadence, alive continuity, state payloads, injection-to-kill reaction
time from TX timeline vs RX rows). Compare against the same measures from
Run B's RX logs.

Pass: scenario outcomes identical (same fault flags, same kill reasons,
reaction times within one 10 ms tick + CAN latency); no behavioral delta
attributable to `ETH=1`.

Fallback if 0x013 is absent on the bus (known SC DCAN TX fragility,
`docs/lessons-learned/embedded-hil-sc-e2e-run-state.md`): repeat both sides
as a matched `DBGDUMP=1` pair (delta = `ETH` only) and compare UART `[5s]`
states; record the deviation here.

### Restore

Reflash the plain `ETH=1` build (operator approval) and verify 60 s of
100 Hz telemetry with the receiver gates.

## Evidence

### Run B — PENDING

### Run C — PENDING

### Run A — PENDING

## Criteria evaluation

| # | Criterion | Verdict |
|---|---|---|
| 1 | UDP vs UART within one sampling period | PENDING |
| 2 | Packet loss 0 | PENDING |
| 3 | No ETH=1 interference | PENDING |

## Deviations and notes

- (none yet)
