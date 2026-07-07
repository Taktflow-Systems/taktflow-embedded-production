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

## Pre-run bench findings (2026-07-07)

Established during Run B bring-up; all register- or log-evidenced. These
forced the procedure revision below (user-approved 2026-07-07).

1. **COM drift vs `tools/bench/hardware-map.local.toml`**: SC App UART now
   COM8 (XDS110), aux COM7; Waveshare USB-CAN-A (CH340) now COM9. The map
   itself warns COM numbers shift with USB enumeration.
2. **The CAN chain works end-to-end.** During the first boot-into-load, the
   SC reported `fault_flags=0, ecu_health=7` for 5.0 s — all three spoofed
   heartbeats received and E2E-validated. JTAG snapshots mid-flood showed
   `DCAN1 ES.RxOK=1`, `NWDAT21=0x0E` (heartbeat mailboxes 2/3/4 fresh),
   `ERRC=0`, and live `e2e_last_alive` tracking.
3. **Non-HIL builds are untenable on this bench**, for three independent
   reasons: (a) the relay pin GIOA[0] is muxed to LIN1TX on the LaunchPad
   (`sc_relay.c` trigger h note) so readback kills ~25 ms after energize
   (`fault_reason=6` observed on every non-HIL boot); (b) one >=100 ms
   host-side TX stall bursts catch-up frames, overwrites the DCAN mailbox
   between 10 ms polls, desyncs the E2E alive check (`expected_alive`
   only advances on valid frames), starves `NotifyRx` >=130 ms and latches
   `hb_confirmed` permanently (`sc_heartbeat.c:97-99` — recovery forbidden
   by design after confirmation); (c) 100 ms timeout vs USB-serial jitter.
   `PLATFORM_HIL` exists precisely for this bench: readback skipped, E2E
   force-accept, 500 ms/10-tick timeout, FZC monitoring compiled out.
4. **HIL-build observability**: `SC_Relay_DeEnergize` is a no-op under
   `PLATFORM_HIL`, so `relay_energized` stays 1 and `fault_reason` stays 0
   permanently; faults surface as `mode=FAULT(2)` (`sc_main.c` step 4a) plus
   `fault_flags`/`ecu_health` bits. `SAFE_STOP(3)` is unreachable.
5. **No bus-off auto-recovery**: putting the adapter (the bus's only ACKer)
   into loopback for ~30 s drove the SC's DCAN to bus-off; 0x013 stayed
   silent until reset. Fault reaction to ACK loss is permanent CAN-TX loss.
6. **Waveshare USB-CAN-A semantics** (bench-verified): settings proto byte
   `0x12` switches adapter->PC emission to variable framing, `0x02` to
   fixed; TX accepts both framings regardless; settings frames take effect
   immediately (mode byte: 0=normal, 1=loopback, 2=silent). The driver now
   sends a deterministic init frame on connect (`--init-adapter`).
7. **Stale hash stamp**: `build/tms570-sudp03fix-eth/sc.elf` embeds the
   pre-commit hash `d6682ee5` (built before `87a73b1` was committed); the
   true `87a73b1` gate-rerun ELF is `build/tms570-sudp03cad-eth/sc.elf`.
   All S-UDP-05 images are fresh builds at committed HEAD.

## Procedure (revised 2026-07-07, user-approved)

All runs use `HIL=1` builds at committed HEAD. Deviations from the original
plan and their rationale are listed under Deviations.

### Pre-checks (before Run B)

1. Confirm nothing else is using the board or UDP port 55001 (operator).
2. Adapter: driver sends the init settings frame on connect (500 kbps,
   normal mode, fixed emission); verify 0x013 parses at 10/s.
3. Build `ETH=1 HIL=1`, `ETH=1 DBGDUMP=1 HIL=1`, and `HIL=1` (default)
   images at HEAD; verify each embeds the HEAD hash (`strings | grep
   "SC Boot"`).

### Run B — trustworthiness under load (criterion 2) — PENDING

Board: fresh `ETH=1 HIL=1` image. Confirmed timeouts latch (recovery
forbidden): reload via DSLite between phases. Startup grace is 10 s (HIL);
start the driver before resetting the board so grace expires into a
heartbeat-present bus, and start the gated receiver only after boot.

| Phase | Driver | Receiver |
|---|---|---|
| B1 nominal 600 s | `--phase nominal --duration 900 --restbus` | after boot: `--duration 600 --csv tmp/sc_eth_sudp05_runB1.csv --expect-rate-hz 100 --rate-tolerance 0.01 --fail-on-gap` |
| B2 CVC dropout | `--phase dropout --target-ecu cvc --inject-at 120 --hold 60 ` | after boot: `--duration 120 --csv tmp/sc_eth_sudp05_runB2.csv --expect-rate-hz 100 --rate-tolerance 0.01 --fail-on-gap` |
| B3 CVC content fault | `--phase content-fault --target-ecu cvc --inject-at 120 --hold 60` | as B2, `tmp/sc_eth_sudp05_runB3.csv` |
| B3b CVC CRC corrupt (negative) | `--phase corrupt --target-ecu cvc --inject-at 120 --hold 60` | as B2, `tmp/sc_eth_sudp05_runB3b.csv` |

Driver invocations add `--port <adapter> --tx-log tmp/..._tx.csv --rx-log
tmp/..._rx.csv`.

Pass: receiver exit 0 in all phases (100 Hz +/- 1%, zero gaps, zero
invalid); B1 holds `mode=MONITORING, ecu_health=7, fault_flags=0`; B2 shows
CVC fault flag + CVC health bit clear within ~600 ms (500 ms timeout +
confirm) and `mode=FAULT`; B3 shows CVC fault flag + `mode=FAULT` after
~1 s (20 receptions); B3b shows NO state change (HIL force-accept —
documented negative check). The stream must stay gap-free through all
injections.

### Run C — UART cross-check (criterion 1) — PENDING

Build + flash: `ETH=1 DBGDUMP=1 HIL=1` at committed HEAD (flags unchanged
from the original approval). Phases: nominal 300 s, then CVC dropout
`--inject-at 60 --hold 60` (500 ms timeout under HIL; FZC unmonitored).
Receiver runs WITHOUT `--fail-on-gap` (dump stalls are expected and
quantified); UART captured with `scripts/serial-monitor.py COM8 115200
--log` on the same host. Note: in HIL the `[5s]` line never carries a
`kill=` token and relay stays ON — state agreement rests on CVC/FZC/RZC
OK|TIMEOUT vs `ecu_health` bits plus the tx7 counter identities. Evaluate:

```
python tools/bench/sc_eth_uart_crosscheck.py \
    --udp-csv tmp/sc_eth_sudp05_runC.csv \
    --uart-log tmp/sc_eth_sudp05_runC_uart.log \
    --out test/hil/reports/artifacts/sudp05_runC_crosscheck.md
```

Pass: comparator exit 0 — every dump MATCH or TRANSITION, counter
identities within +/- 1, alignment sane.

### Run A — no-ETH baseline (criterion 3) — PENDING

Build + flash: `HIL=1` default build (no `ETH`, no `DBGDUMP`) — delta vs
Run B's image is exactly `SC_ETH_ENABLE`. Re-run phases B1 (shortened to
120 s), B2, B3 with identical driver commands. Observables: driver RX log
(0x013 at 10 Hz — confirmed present on this bench — cadence, alive
continuity, mode/fault/health payloads, injection-to-mode-FAULT reaction
time from TX timeline vs RX rows). Compare against the same measures from
Run B's RX logs.

Pass: scenario outcomes identical (same fault flags, same mode transitions,
reaction times within one 10 ms tick + CAN latency); 0x013 cadence and
alive continuity unchanged; no behavioral delta attributable to `ETH=1`.

### Restore

Reflash the `ETH=1 HIL=1` Run B image (the appropriate resident image for
this bench) and verify 60 s of 100 Hz telemetry with the receiver gates.

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

Deviations from the originally approved procedure (all user-approved
2026-07-07, forced by the bench findings above):

- **All runs use `HIL=1` builds** instead of Run B on the deployed non-HIL
  `ETH=1` image — non-HIL cannot hold MONITORING on this bench (finding 3).
  The criterion-3 comparison remains valid: both sides differ only in
  `SC_ETH_ENABLE`.
- **B2 dropout target is CVC, not FZC** — FZC monitoring is compiled out
  under `PLATFORM_HIL` (`sc_heartbeat.c:127-131`).
- **B3 is content-fault injection** (FaultStatus bits >= 2 for >= 20
  receptions -> latched content fault) instead of an E2E-CRC kill — HIL
  force-accepts corrupt CRC. The CRC corruption is retained as negative
  check B3b (expected: no reaction; documents the HIL E2E bypass).
- **Fault evidence fields are `mode`/`fault_flags`/`ecu_health`** —
  `relay_energized`/`fault_reason` are inert in HIL builds (finding 4).
- The `kill=` token comparison in the UART cross-check is vacuous under HIL
  (never printed); state agreement uses the three per-ECU tokens + relay=ON.
