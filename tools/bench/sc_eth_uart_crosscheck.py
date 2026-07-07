#!/usr/bin/env python3
"""
@file       sc_eth_uart_crosscheck.py
@brief      S-UDP-05 criterion-1 comparator — cross-checks the SC's UDP
            telemetry stream against the UART DBGDUMP [5s] status dump.

Inputs:
    --udp-csv   capture from tools/bench/eth_telemetry_rx.py
    --uart-log  capture from scripts/serial-monitor.py --log (COM11), taken
                on the SAME host so both clocks are time.time()-consistent
                (the UDP CSV stores UTC epoch; the UART log stores local
                HH:MM:SS — this tool reconciles the two)

Firmware source of truth (ETH=1 DBGDUMP=1 [HIL=1] build):
    firmware/ecu/sc/src/sc_main.c            [5s] state line, every 500 ticks
    firmware/platform/tms570/src/sc_hw_tms570.c  tx7 counters (HIL=1 only)
    firmware/ecu/sc/src/sc_monitoring.c      status bit packing

Method — the dump's ~280 ms SCI3 stall drops ~27 telemetry ticks, so every
UART dump leaves a distinctive missed_periods burst in the UDP stream. The
tool aligns UART dump k with UDP stall k (ordinal + wall-clock sanity), then
checks two tick-exact identities per inter-dump interval:
    1. delta tx7 call == 500 ticks / SC_MONITORING_TX_PERIOD(10) == 50
    2. UDP frames received + sum(missed_periods) == 500
and per dump compares the state fields (CVC/FZC/RZC OK|TIMEOUT vs ecu_health
bits, relay ON|OFF vs relay_energized, kill=N vs fault_reason).

"Match within one sampling period" (plan-sc-ethernet-roadmap.md S-UDP-05):
the UART sampling period is one 5 s dump; a state disagreement is accepted
as TRANSITION when the UDP stream shows the matching state within +/- half a
dump period of the dump time, otherwise it is a MISMATCH.

Exit codes: 0 = all checks pass, 1 = mismatch/identity failure, 2 = input error.

Usage:
    python tools/bench/sc_eth_uart_crosscheck.py \
        --udp-csv tmp/sc_eth_sudp05_runC.csv \
        --uart-log tmp/sc_eth_sudp05_runC_uart.log \
        --out test/hil/reports/artifacts/sudp05_runC_crosscheck.md

    python tools/bench/sc_eth_uart_crosscheck.py --self-test

@aspice     SWE.6 — Software Qualification Testing (HIL support tooling)
"""

import argparse
import csv
import datetime as dt
import io
import re
import sys

TICKS_PER_DUMP = 500          # sc_main.c dbg_tick_counter threshold
TX_PERIOD_TICKS = 10          # Sc_Hw_Cfg.h SC_MONITORING_TX_PERIOD
EXPECTED_TX_PER_DUMP = TICKS_PER_DUMP // TX_PERIOD_TICKS   # 50

UART_TS_RE = re.compile(r"^\[(\d{2}):(\d{2}):(\d{2})\]")
STATE_RE = re.compile(
    r"SC: CVC=(OK|TIMEOUT) FZC=(OK|TIMEOUT) RZC=(OK|TIMEOUT)"
    r" relay=(ON|OFF)(?: kill=(\d+))?")
TX7_RE = re.compile(r"tx7 call=(\d+) ok=(\d+) fail=(\d+)")

KILL_REASON_NAMES = {
    0: "NONE", 1: "HB_TIMEOUT", 2: "PLAUSIBILITY", 3: "SELFTEST", 4: "ESM",
    5: "BUSOFF", 6: "READBACK", 7: "ESTOP", 8: "BUS_SILENCE", 9: "E2E_FAIL",
    10: "CREEP_GUARD",
}


def parse_uart_log(lines, base_date):
    """Extract dump records: (epoch_s, state dict or None, tx7 tuple or None).

    base_date: dt.date used for the first timestamp; rollover past midnight
    is handled by adding a day whenever time-of-day goes backwards.
    """
    dumps = []
    day_offset = 0
    prev_tod = None
    for line in lines:
        ts_match = UART_TS_RE.match(line)
        if not ts_match:
            continue
        state_match = STATE_RE.search(line)
        tx7_match = TX7_RE.search(line)
        if not state_match and not tx7_match:
            continue
        hh, mm, ss = (int(g) for g in ts_match.groups())
        tod = hh * 3600 + mm * 60 + ss
        if prev_tod is not None and tod < prev_tod - 43200:
            day_offset += 1
        prev_tod = tod
        local = dt.datetime.combine(
            base_date + dt.timedelta(days=day_offset),
            dt.time(hh, mm, ss))
        epoch = local.timestamp()   # naive datetime -> local timezone epoch

        state = None
        if state_match:
            cvc, fzc, rzc, relay, kill = state_match.groups()
            state = {
                "cvc_ok": cvc == "OK",
                "fzc_ok": fzc == "OK",
                "rzc_ok": rzc == "OK",
                "relay_on": relay == "ON",
                "kill": None if kill is None else int(kill),
            }
        tx7 = None
        if tx7_match:
            tx7 = tuple(int(g) for g in tx7_match.groups())
        dumps.append({"epoch": epoch, "state": state, "tx7": tx7})
    return dumps


def parse_udp_csv(reader):
    """Load receiver rows: epoch, state fields, missed_periods."""
    rows = []
    for raw in reader:
        try:
            rows.append({
                "epoch": float(raw["host_time_s"]),
                "sc_mode": int(raw["sc_mode"]),
                "fault_flags": int(raw["fault_flags"]),
                "ecu_health": int(raw["ecu_health"]),
                "fault_reason": int(raw["fault_reason"]),
                "relay_energized": int(raw["relay_energized"]),
                "missed_periods": int(raw["missed_periods"] or 0),
            })
        except (KeyError, ValueError) as exc:
            raise SystemExit(f"ERROR: bad UDP CSV row: {exc}")
    return rows


def find_stalls(udp_rows, stall_threshold):
    """Indices of rows whose missed_periods burst marks a UART dump stall."""
    return [i for i, row in enumerate(udp_rows)
            if row["missed_periods"] >= stall_threshold]


def state_agrees(uart_state, udp_row):
    """One UART [5s] state vs one UDP row (same predicates on both sides)."""
    checks = [
        uart_state["cvc_ok"] == bool(udp_row["ecu_health"] & 0x01),
        uart_state["fzc_ok"] == bool(udp_row["ecu_health"] & 0x02),
        uart_state["rzc_ok"] == bool(udp_row["ecu_health"] & 0x04),
        uart_state["relay_on"] == bool(udp_row["relay_energized"]),
    ]
    if uart_state["relay_on"]:
        checks.append(udp_row["fault_reason"] == 0)
    elif uart_state["kill"] is not None:
        checks.append(udp_row["fault_reason"] == uart_state["kill"])
    return all(checks)


def describe_udp(row):
    return (f"health=0x{row['ecu_health']:X}"
            f" relay={row['relay_energized']}"
            f" reason={KILL_REASON_NAMES.get(row['fault_reason'], '?')}")


def describe_uart(state):
    txt = (f"CVC={'OK' if state['cvc_ok'] else 'TIMEOUT'}"
           f" FZC={'OK' if state['fzc_ok'] else 'TIMEOUT'}"
           f" RZC={'OK' if state['rzc_ok'] else 'TIMEOUT'}"
           f" relay={'ON' if state['relay_on'] else 'OFF'}")
    if state["kill"] is not None:
        txt += f" kill={KILL_REASON_NAMES.get(state['kill'], state['kill'])}"
    return txt


def nearest_row(udp_rows, epoch):
    return min(udp_rows, key=lambda row: abs(row["epoch"] - epoch))


def crosscheck(uart_dumps, udp_rows, args, out):
    failures = 0
    state_dumps = [d for d in uart_dumps if d["state"] is not None]
    stalls = find_stalls(udp_rows, args.stall_threshold)

    out.write("# S-UDP-05 UART/UDP cross-check\n\n")
    out.write(f"- UART dumps with state: {len(state_dumps)}\n")
    out.write(f"- UDP rows: {len(udp_rows)}\n")
    out.write(f"- UDP stall markers (missed_periods >= "
              f"{args.stall_threshold}): {len(stalls)}\n\n")

    if not state_dumps or not udp_rows:
        out.write("ERROR: empty input — nothing to compare.\n")
        return 2

    # --- state comparison, one row per UART dump -----------------------
    out.write("## State agreement (criterion 1, state fields)\n\n")
    out.write("| # | UART time | UART state | nearest UDP state | dt (s) "
              "| verdict |\n|---|---|---|---|---|---|\n")
    for idx, dump in enumerate(state_dumps):
        row = nearest_row(udp_rows, dump["epoch"])
        skew = row["epoch"] - dump["epoch"]
        if abs(skew) > args.align_skew + args.dump_period:
            verdict = "NO_UDP_DATA"
            failures += 1
        elif state_agrees(dump["state"], row):
            verdict = "MATCH"
        else:
            window = [r for r in udp_rows
                      if abs(r["epoch"] - dump["epoch"]) <= args.dump_period / 2]
            if any(state_agrees(dump["state"], r) for r in window):
                verdict = "TRANSITION"
            else:
                verdict = "MISMATCH"
                failures += 1
        local = dt.datetime.fromtimestamp(dump["epoch"]).strftime("%H:%M:%S")
        out.write(f"| {idx} | {local} | {describe_uart(dump['state'])} "
                  f"| {describe_udp(row)} | {skew:+.1f} | {verdict} |\n")

    # --- tick-exact counter identities between consecutive stalls ------
    out.write("\n## Counter identities (criterion 1, counters)\n\n")
    tx7_dumps = [d for d in uart_dumps if d["tx7"] is not None]
    if len(stalls) >= 2:
        out.write("| interval | UDP frames | missed | frames+missed "
                  f"(exp {TICKS_PER_DUMP}) | d(tx7 call) "
                  f"(exp {EXPECTED_TX_PER_DUMP}) | d(ok) | d(fail) "
                  "| verdict |\n|---|---|---|---|---|---|---|---|\n")
        for k in range(len(stalls) - 1):
            lo, hi = stalls[k], stalls[k + 1]
            frames = hi - lo                       # rows received in interval
            missed = sum(r["missed_periods"] for r in udp_rows[lo + 1:hi + 1])
            total = frames + missed
            ok_total = abs(total - TICKS_PER_DUMP) <= args.counter_tol

            dcall = dok = dfail = None
            ok_tx7 = True
            if k + 1 < len(tx7_dumps):
                a, b = tx7_dumps[k]["tx7"], tx7_dumps[k + 1]["tx7"]
                dcall, dok, dfail = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
                ok_tx7 = abs(dcall - EXPECTED_TX_PER_DUMP) <= args.counter_tol
            verdict = "PASS" if (ok_total and ok_tx7) else "FAIL"
            if verdict == "FAIL":
                failures += 1
            fmt = lambda v: "-" if v is None else str(v)
            out.write(f"| {k} | {frames} | {missed} | {total} "
                      f"| {fmt(dcall)} | {fmt(dok)} | {fmt(dfail)} "
                      f"| {verdict} |\n")
    else:
        out.write("Fewer than two stall markers found — counter identity "
                  "not evaluated (was DBGDUMP=1 active and the capture "
                  "longer than two dump periods?).\n")
        failures += 1

    # --- alignment sanity ----------------------------------------------
    out.write("\n## Alignment sanity\n\n")
    n = min(len(state_dumps), len(stalls))
    worst = 0.0
    for k in range(n):
        worst = max(worst,
                    abs(udp_rows[stalls[k]]["epoch"] - state_dumps[k]["epoch"]))
    out.write(f"- ordinal pairs checked: {n}; worst UART-vs-stall clock "
              f"skew: {worst:.1f} s (limit {args.align_skew:.1f} s)\n")
    if n and worst > args.align_skew:
        out.write("- ALIGNMENT FAILURE: stall/dump pairing unreliable\n")
        failures += 1

    out.write(f"\n**Result: {'PASS' if failures == 0 else 'FAIL'}** "
              f"({failures} failing check(s))\n")
    return 0 if failures == 0 else 1


def self_test() -> int:
    """Synthetic end-to-end check of parsing, alignment and identities."""
    base = dt.datetime(2026, 7, 7, 12, 0, 0)   # local
    t0 = base.timestamp()

    uart_lines = []
    udp_rows = []
    call = 1000
    epoch = t0
    seq_time = t0
    for k in range(4):
        # 472 clean rows @10 ms, then the stall row carrying missed=28
        for _ in range(TICKS_PER_DUMP - 28):
            udp_rows.append({"epoch": seq_time, "sc_mode": 1, "fault_flags": 0,
                             "ecu_health": 7, "fault_reason": 0,
                             "relay_energized": 1, "missed_periods": 0})
            seq_time += 0.010
        seq_time += 0.28
        udp_rows.append({"epoch": seq_time, "sc_mode": 1, "fault_flags": 0,
                         "ecu_health": 7, "fault_reason": 0,
                         "relay_energized": 1, "missed_periods": 28})
        stamp = dt.datetime.fromtimestamp(seq_time).strftime("%H:%M:%S")
        uart_lines.append(
            f"[{stamp}] [5s] SC: CVC=OK FZC=OK RZC=OK relay=ON"
            f" tx7 call={call + 50 * (k + 1)} ok={call + 50 * (k + 1)} fail=0")

    args = argparse.Namespace(stall_threshold=10, align_skew=2.0,
                              dump_period=5.0, counter_tol=1)
    dumps = parse_uart_log(uart_lines, base.date())
    buf = io.StringIO()
    rc = crosscheck(dumps, udp_rows, args, buf)
    print(buf.getvalue())
    print("self-test:", "PASS" if rc == 0 else "FAIL")

    # negative case: relay disagreement must fail
    bad = [dict(row) for row in udp_rows]
    for row in bad:
        row["relay_energized"] = 0
        row["fault_reason"] = 1
    rc_bad = crosscheck(dumps, bad, args, io.StringIO())
    print("self-test negative:", "PASS" if rc_bad == 1 else "FAIL")
    return 0 if (rc == 0 and rc_bad == 1) else 1


def main():
    parser = argparse.ArgumentParser(
        description="Cross-check SC UDP telemetry against UART DBGDUMP output")
    parser.add_argument("--udp-csv", help="eth_telemetry_rx.py capture CSV")
    parser.add_argument("--uart-log", help="serial-monitor.py --log capture")
    parser.add_argument("--date", default=None,
                        help="local date of the UART log start (YYYY-MM-DD); "
                             "default: derived from the first UDP row")
    parser.add_argument("--out", default=None,
                        help="write the markdown report here (default stdout)")
    parser.add_argument("--dump-period", type=float, default=5.0,
                        help="UART dump period in seconds (default 5)")
    parser.add_argument("--stall-threshold", type=int, default=10,
                        help="missed_periods burst marking a dump stall")
    parser.add_argument("--align-skew", type=float, default=2.0,
                        help="max allowed UART-vs-stall clock skew in seconds")
    parser.add_argument("--counter-tol", type=int, default=1,
                        help="tolerance on tick-exact identities (default 1)")
    parser.add_argument("--self-test", action="store_true",
                        help="run synthetic parser/identity checks and exit")
    args = parser.parse_args()

    if args.self_test:
        sys.exit(self_test())
    if not args.udp_csv or not args.uart_log:
        print("ERROR: --udp-csv and --uart-log are required "
              "(or use --self-test)", file=sys.stderr)
        sys.exit(2)

    with open(args.udp_csv, newline="", encoding="utf-8") as fh:
        udp_rows = parse_udp_csv(csv.DictReader(fh))
    if not udp_rows:
        print("ERROR: UDP CSV contains no rows", file=sys.stderr)
        sys.exit(2)

    if args.date:
        base_date = dt.date.fromisoformat(args.date)
    else:
        base_date = dt.datetime.fromtimestamp(udp_rows[0]["epoch"]).date()

    with open(args.uart_log, encoding="utf-8", errors="replace") as fh:
        uart_dumps = parse_uart_log(fh, base_date)

    if args.out:
        with open(args.out, "w", encoding="utf-8") as out:
            rc = crosscheck(uart_dumps, udp_rows, args, out)
        print(f"report written to {args.out} (exit {rc})")
    else:
        rc = crosscheck(uart_dumps, udp_rows, args, sys.stdout)
    sys.exit(rc)


if __name__ == "__main__":
    main()
