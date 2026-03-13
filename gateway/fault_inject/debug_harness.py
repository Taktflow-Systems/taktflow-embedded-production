"""Diagnostic harness for SC plausibility debugging.

Sniffs CAN frames during scenario injection and logs the exact values
that the Safety Controller sees for its plausibility check:
  - Vehicle_State (0x100) byte 4 = torque % (from CVC)
  - Motor_Current (0x301) bytes 2-3 = current mA (from plant-sim)
  - SC_Monitoring (0x013) = SC mode, fault flags, kill reason

Run on the VPS (gateway container or host with vcan0 access):
    python -m fault_inject.debug_harness runaway_accel
    python -m fault_inject.debug_harness --probe-only   # just sniff, no inject
"""

import argparse
import can
import os
import struct
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field

# SC plausibility lookup table (from sc_plausibility.c)
SC_LOOKUP_TORQUE = [0, 7, 15, 25, 40, 55, 70, 85, 100]
SC_LOOKUP_CURRENT = [0, 1750, 3750, 6250, 10000, 13750, 17500, 21250, 25000]
SC_ABS_THRESHOLD = 2000
SC_REL_THRESHOLD_PCT = 20
SC_DEBOUNCE_TICKS = 5  # consecutive implausible ticks before kill


@dataclass
class CANSnapshot:
    """Single CAN observation with derived plausibility verdict."""
    t: float                 # seconds since start
    torque_pct: int = 0      # from Vehicle_State byte 4
    current_ma: int = 0      # from Motor_Current bytes 2-3
    vehicle_state: int = 0   # from Vehicle_State byte 0
    sc_mode: int = 0         # from SC_Monitoring byte 2 low nibble
    sc_fault_flags: int = 0  # from SC_Monitoring byte 2 high nibble
    sc_kill_reason: int = 0  # from SC_Monitoring byte 3 bits 3-6
    sc_relay: int = 1        # from SC_Monitoring byte 3 bit 7
    source: str = ""         # which frame updated this snapshot


def sc_expected_current(torque_pct: int) -> int:
    """Interpolate the SC lookup table for a given torque %."""
    if torque_pct <= 0:
        return 0
    if torque_pct >= 100:
        return 25000
    for i in range(len(SC_LOOKUP_TORQUE) - 1):
        t_lo = SC_LOOKUP_TORQUE[i]
        t_hi = SC_LOOKUP_TORQUE[i + 1]
        if t_lo <= torque_pct <= t_hi:
            frac = (torque_pct - t_lo) / (t_hi - t_lo)
            c_lo = SC_LOOKUP_CURRENT[i]
            c_hi = SC_LOOKUP_CURRENT[i + 1]
            return int(c_lo + frac * (c_hi - c_lo))
    return 25000


def is_implausible(torque_pct: int, current_ma: int) -> tuple[bool, int, int]:
    """Check SC plausibility logic. Returns (implausible, expected, threshold)."""
    expected = sc_expected_current(torque_pct)
    diff = abs(current_ma - expected)
    if torque_pct < 5 and expected < SC_ABS_THRESHOLD:
        threshold = SC_ABS_THRESHOLD
    else:
        threshold = max(int(expected * SC_REL_THRESHOLD_PCT / 100), SC_ABS_THRESHOLD)
    return diff > threshold, expected, threshold


class CANSniffer:
    """Sniff Vehicle_State, Motor_Current, SC_Monitoring and log plausibility."""

    CAN_VEHICLE_STATE = 0x100
    CAN_MOTOR_CURRENT = 0x301
    CAN_SC_MONITORING = 0x013

    def __init__(self, channel: str = "vcan0"):
        self.bus = can.interface.Bus(channel=channel, interface="socketcan")
        self.start_time = time.time()
        self.log: list[CANSnapshot] = []
        self._running = False
        self._lock = threading.Lock()

        # Rolling state
        self._torque_pct = 0
        self._current_ma = 0
        self._vehicle_state = 0
        self._sc_mode = 0
        self._sc_fault_flags = 0
        self._sc_kill_reason = 0
        self._sc_relay = 1

        # Debounce simulation
        self._debounce_count = 0
        self._max_debounce_seen = 0

    def start(self):
        self._running = True
        self._thread = threading.Thread(target=self._recv_loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        self._thread.join(timeout=2)
        self.bus.shutdown()

    def _recv_loop(self):
        while self._running:
            msg = self.bus.recv(timeout=0.1)
            if msg is None:
                continue
            t = time.time() - self.start_time
            source = ""

            if msg.arbitration_id == self.CAN_VEHICLE_STATE and len(msg.data) >= 5:
                self._vehicle_state = msg.data[2] & 0x0F  # VehicleState at byte 2 low nibble
                self._torque_pct = msg.data[4]  # Torque % at byte 4
                source = "VS"

            elif msg.arbitration_id == self.CAN_MOTOR_CURRENT and len(msg.data) >= 4:
                self._current_ma = msg.data[2] | (msg.data[3] << 8)
                source = "MC"

            elif msg.arbitration_id == self.CAN_SC_MONITORING and len(msg.data) >= 4:
                self._sc_mode = msg.data[2] & 0x0F
                self._sc_fault_flags = (msg.data[2] >> 4) & 0x0F
                self._sc_kill_reason = (msg.data[3] >> 3) & 0x0F
                self._sc_relay = (msg.data[3] >> 7) & 0x01
                source = "SC"

            if source:
                snap = CANSnapshot(
                    t=t,
                    torque_pct=self._torque_pct,
                    current_ma=self._current_ma,
                    vehicle_state=self._vehicle_state,
                    sc_mode=self._sc_mode,
                    sc_fault_flags=self._sc_fault_flags,
                    sc_kill_reason=self._sc_kill_reason,
                    sc_relay=self._sc_relay,
                    source=source,
                )
                with self._lock:
                    self.log.append(snap)

                # Simulate SC debounce tracking
                if source in ("VS", "MC"):
                    impl, exp, thr = is_implausible(self._torque_pct, self._current_ma)
                    if impl:
                        self._debounce_count += 1
                        self._max_debounce_seen = max(self._max_debounce_seen, self._debounce_count)
                    else:
                        self._debounce_count = 0

    def dump_log(self, path: str = None):
        """Print and optionally save the captured CAN log with plausibility analysis."""
        VS_NAMES = {0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP", 4: "SAFE_STOP", 5: "SHUTDOWN"}
        SC_KILL = {0: "NONE", 1: "HB_TIMEOUT", 2: "PLAUSIBILITY", 3: "SELFTEST",
                   4: "CMD_KILL", 5: "WATCHDOG", 6: "CAN_LOSS", 7: "BAT_UV",
                   8: "OVERTEMP", 9: "E2E_FAIL"}

        lines = []
        header = (f"{'t(s)':>7} {'src':>3} {'VS':>10} {'torque%':>7} {'cur_mA':>7} "
                  f"{'exp_mA':>7} {'thr_mA':>7} {'diff':>7} {'plaus':>6} "
                  f"{'SC_mode':>7} {'SC_flg':>6} {'SC_kill':>12} {'relay':>5}")
        lines.append(header)
        lines.append("-" * len(header))

        prev_state = None
        debounce_run = 0

        with self._lock:
            for snap in self.log:
                impl, expected, threshold = is_implausible(snap.torque_pct, snap.current_ma)
                diff = abs(snap.current_ma - expected)
                vs_name = VS_NAMES.get(snap.vehicle_state, f"?{snap.vehicle_state}")
                kill_name = SC_KILL.get(snap.sc_kill_reason, f"?{snap.sc_kill_reason}")
                plaus_str = "FAIL" if impl else "OK"

                if impl:
                    debounce_run += 1
                else:
                    debounce_run = 0

                # Mark state transitions
                marker = ""
                if snap.vehicle_state != prev_state and prev_state is not None:
                    old_name = VS_NAMES.get(prev_state, f"?{prev_state}")
                    marker = f"  <<< {old_name} -> {vs_name}"
                prev_state = snap.vehicle_state

                # Mark debounce danger
                if debounce_run >= SC_DEBOUNCE_TICKS:
                    marker += "  !!! KILL THRESHOLD"
                elif debounce_run >= 3:
                    marker += f"  ** debounce={debounce_run}"

                line = (f"{snap.t:7.3f} {snap.source:>3} {vs_name:>10} {snap.torque_pct:7d} "
                        f"{snap.current_ma:7d} {expected:7d} {threshold:7d} {diff:7d} "
                        f"{plaus_str:>6} {snap.sc_mode:7d} {snap.sc_fault_flags:6d} "
                        f"{kill_name:>12} {snap.sc_relay:5d}{marker}")
                lines.append(line)

        output = "\n".join(lines)
        print(output)
        print(f"\nMax consecutive debounce seen: {self._max_debounce_seen} "
              f"(kill at {SC_DEBOUNCE_TICKS})")
        print(f"Total frames captured: {len(self.log)}")

        if path:
            with open(path, "w") as f:
                f.write(output)
            print(f"Saved to {path}")

    def get_events(self) -> list[str]:
        """Return a summary of notable events (state changes, plausibility failures)."""
        VS_NAMES = {0: "INIT", 1: "RUN", 2: "DEGRADED", 3: "LIMP", 4: "SAFE_STOP", 5: "SHUTDOWN"}
        events = []
        prev_state = None
        debounce_run = 0

        with self._lock:
            for snap in self.log:
                impl, expected, threshold = is_implausible(snap.torque_pct, snap.current_ma)

                if impl:
                    debounce_run += 1
                    if debounce_run == 1:
                        events.append(
                            f"t={snap.t:.3f}s PLAUS_FAIL start: "
                            f"torque={snap.torque_pct}% current={snap.current_ma}mA "
                            f"expected={expected}mA diff={abs(snap.current_ma - expected)}mA "
                            f"threshold={threshold}mA"
                        )
                    if debounce_run == SC_DEBOUNCE_TICKS:
                        events.append(
                            f"t={snap.t:.3f}s DEBOUNCE REACHED {SC_DEBOUNCE_TICKS} — SC WOULD KILL"
                        )
                else:
                    if debounce_run > 0:
                        events.append(
                            f"t={snap.t:.3f}s PLAUS_OK after {debounce_run} consecutive fails"
                        )
                    debounce_run = 0

                if snap.vehicle_state != prev_state and prev_state is not None:
                    old = VS_NAMES.get(prev_state, f"?{prev_state}")
                    new = VS_NAMES.get(snap.vehicle_state, f"?{snap.vehicle_state}")
                    events.append(f"t={snap.t:.3f}s STATE {old} -> {new}")
                prev_state = snap.vehicle_state

                if snap.sc_kill_reason != 0 and snap.source == "SC":
                    SC_KILL = {1: "HB_TIMEOUT", 2: "PLAUSIBILITY", 3: "SELFTEST"}
                    events.append(
                        f"t={snap.t:.3f}s SC_KILL reason={SC_KILL.get(snap.sc_kill_reason, snap.sc_kill_reason)} "
                        f"relay={snap.sc_relay}"
                    )

        return events


def run_probe(channel: str, duration: float, scenario_fn=None, scenario_name: str = ""):
    """Run the CAN sniffer, optionally triggering a scenario."""
    sniffer = CANSniffer(channel=channel)
    sniffer.start()

    if scenario_fn:
        print(f"Waiting 1s for sniffer to warm up...")
        time.sleep(1.0)
        print(f"Triggering scenario: {scenario_name}")
        result = scenario_fn()
        print(f"  -> {result}")

    print(f"Sniffing for {duration}s...")
    time.sleep(duration)
    sniffer.stop()

    print("\n" + "=" * 100)
    print("CAN PLAUSIBILITY LOG")
    print("=" * 100)
    sniffer.dump_log(path=f"/tmp/debug_{scenario_name or 'probe'}.log")

    print("\n" + "=" * 100)
    print("NOTABLE EVENTS")
    print("=" * 100)
    for event in sniffer.get_events():
        print(f"  {event}")

    return sniffer


def main():
    parser = argparse.ArgumentParser(description="SC plausibility debug harness")
    parser.add_argument("scenario", nargs="?", default=None,
                        help="Scenario to trigger (e.g., runaway_accel)")
    parser.add_argument("--probe-only", action="store_true",
                        help="Just sniff CAN, don't inject")
    parser.add_argument("--duration", type=float, default=15.0,
                        help="Sniff duration in seconds")
    parser.add_argument("--channel", default="vcan0",
                        help="CAN channel")
    parser.add_argument("--with-reset", action="store_true",
                        help="Reset containers before scenario")
    parser.add_argument("--settle", type=float, default=12.0,
                        help="Seconds to wait after reset before injection")
    args = parser.parse_args()

    scenario_fn = None
    scenario_name = args.scenario or "probe"

    if args.scenario and not args.probe_only:
        # Import scenario functions
        from . import scenarios
        if args.scenario not in scenarios.SCENARIOS:
            print(f"Unknown scenario: {args.scenario}")
            print(f"Available: {', '.join(scenarios.SCENARIOS.keys())}")
            sys.exit(1)

        if args.with_reset:
            print("Resetting containers...")
            scenarios.reset()
            print(f"Waiting {args.settle}s for stable RUN...")
            time.sleep(args.settle)

        scenario_fn = scenarios.SCENARIOS[args.scenario]["fn"]

    run_probe(
        channel=args.channel,
        duration=args.duration,
        scenario_fn=scenario_fn,
        scenario_name=scenario_name,
    )


if __name__ == "__main__":
    main()
