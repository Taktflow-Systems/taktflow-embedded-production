"""Dashboard E2E test specifications — verdict definitions for 8 testable scenarios.

Each spec defines the scenario ID, safety goal reference, observe window,
and verdict checks (expected state transitions, DTCs, fault flags).

These specs are consumed by test_runner.py to drive automated E2E testing
from the dashboard with real-time pass/fail verdicts.
"""

from dataclasses import dataclass, field


@dataclass
class VerdictCheck:
    """Single verdict check within a scenario."""
    description: str
    check_type: str          # "vehicle_state", "dtc", "fault_flag", "motor_stop", "torque_zero", "state_stays"
    expected: str            # human-readable expected value
    field: str = ""          # MQTT field to monitor
    value: object = None     # expected value (int, str, etc.)
    timeout_ms: int = 5000   # max time to wait for this verdict


@dataclass
class TestSpec:
    """Full specification for one testable scenario."""
    id: str
    label: str
    scenario: str            # fault_inject scenario name to trigger
    sg: str                  # Safety Goal ID
    asil: str                # ASIL level
    he: str                  # Hazardous Event ID
    description: str = ""    # What the test proves (HARA context)
    injection: str = ""      # What CAN/fault is injected
    prep: str = "normal_drive"  # scenario to run before injection
    post_run_settle_sec: float = 0.0  # wait after RUN before injection
    observe_sec: float = 5.0
    verdicts: list[VerdictCheck] = field(default_factory=list)


# ---------------------------------------------------------------------------
# 8 testable scenarios (injectable via CAN, clear MQTT-observable verdicts)
# ---------------------------------------------------------------------------

TEST_SPECS: list[TestSpec] = [
    TestSpec(
        id="overcurrent",
        label="Overcurrent",
        scenario="overcurrent",
        sg="SG-001", asil="D", he="HE-001",
        description="Verifies motor overcurrent triggers SAFE_STOP and DTC. "
                    "Safety Goal SG-001: prevent unintended vehicle acceleration from motor fault.",
        injection="SPI pedal 95% + MQTT inject_overcurrent to plant-sim",
        post_run_settle_sec=10.0,
        observe_sec=5.0,
        verdicts=[
            VerdictCheck(
                description="Vehicle enters SAFE_STOP or SHUTDOWN",
                check_type="vehicle_state",
                expected="SAFE_STOP or SHUTDOWN",
                # Same residual-current effect as motor_reversal: after
                # the overcurrent injection ends and pedal releases,
                # plant-sim may still publish elevated motor current
                # while torque has dropped to 0, which trips SC's creep
                # guard as a delayed secondary effect.
                value=[4, 5],  # SAFE_STOP or SHUTDOWN
                timeout_ms=5000,
            ),
            VerdictCheck(
                description="DTC 0xE301 broadcast",
                check_type="dtc",
                expected="DTC 0xE301 received",
                value=0xE301,
                timeout_ms=5500,
            ),
        ],
    ),
    TestSpec(
        id="estop",
        label="E-Stop",
        scenario="estop",
        sg="SG-008", asil="C", he="HE-011/013",
        description="Verifies emergency stop brings vehicle to SAFE_STOP. "
                    "Safety Goal SG-008: immediate halt on E-Stop activation. "
                    "HW target: 200ms. SIL budget: 2000ms (Docker scheduling overhead).",
        injection="CAN frame EStop_Command (0x010) with EStop_Active=1",
        observe_sec=5.5,
        verdicts=[
            VerdictCheck(
                description="Vehicle enters SAFE_STOP within 2000ms (SIL)",
                check_type="vehicle_state",
                expected="SAFE_STOP",
                value=4,       # SAFE_STOP enum
                timeout_ms=2000,
            ),
            VerdictCheck(
                description="DTC 0xE101 broadcast",
                check_type="dtc",
                expected="DTC 0xE101 received",
                value=0xE101,
                # CVC_DTC_ESTOP_ACTIVATED maps to UDS code 0xE101. Swc_EStop
                # re-reports FAILED every cycle so DEM saturates the 3-cycle
                # debounce ~30ms after the edge, but the 0x500 broadcast +
                # gateway + monitor pipeline lands at ~5.5s under Docker
                # scheduling (observed 5540ms).
                timeout_ms=6000,
            ),
        ],
    ),
    TestSpec(
        id="steer_fault",
        label="Steer Fault",
        scenario="steer_fault",
        sg="SG-003", asil="D", he="HE-003/004",
        description="Verifies steering fault triggers SAFE_STOP and DTC. "
                    "Safety Goal SG-003: prevent unintended steering from actuator malfunction.",
        injection="MQTT inject_steer_fault to plant-sim",
        post_run_settle_sec=10.0,
        # DTC lands 2500-5100ms depending on scheduler jitter; widen so
        # slow runs don't flake the same way brake_fault did.
        observe_sec=5.5,
        verdicts=[
            VerdictCheck(
                description="Vehicle enters SAFE_STOP",
                check_type="vehicle_state",
                expected="SAFE_STOP",
                value=4,       # SAFE_STOP enum
                timeout_ms=5000,
            ),
            VerdictCheck(
                description="DTC 0xD001 broadcast",
                check_type="dtc",
                expected="DTC 0xD001 received",
                value=0xD001,
                timeout_ms=5500,
            ),
        ],
    ),
    TestSpec(
        id="brake_fault",
        label="Brake Fault",
        scenario="brake_fault",
        sg="SG-004", asil="D", he="HE-005",
        description="Verifies brake fault triggers SAFE_STOP and DTC. "
                    "Safety Goal SG-004: prevent loss of braking from actuator malfunction.",
        injection="MQTT inject_brake_fault to plant-sim",
        post_run_settle_sec=10.0,
        # Widened from 5.0s / 5000ms: DEM confirmation + 0x500 broadcast
        # for CVC-raised 0xE202 BRAKE_FAULT_RX lands at ~5.0s depending
        # on scheduler jitter. The original 5000ms window was a flake
        # (passed at 4636ms, failed at 5034ms in the same config).
        observe_sec=5.5,
        verdicts=[
            VerdictCheck(
                description="Vehicle enters SAFE_STOP",
                check_type="vehicle_state",
                expected="SAFE_STOP",
                value=4,       # SAFE_STOP enum
                timeout_ms=5000,
            ),
            VerdictCheck(
                description="DTC 0xE202 broadcast",
                check_type="dtc",
                expected="DTC 0xE202 received",
                value=0xE202,
                timeout_ms=5500,
            ),
        ],
    ),
    TestSpec(
        id="battery_low",
        label="Battery Drain",
        scenario="battery_low",
        sg="SG-006", asil="A", he="HE-015",
        description="Verifies low battery voltage triggers DEGRADED/LIMP and DTC. "
                    "Safety Goal SG-006: prevent loss of vehicle control from power loss.",
        injection="CAN frame Battery_Status (0x400) with BatteryVoltage=9V (<10V threshold)",
        # Drain profile in scenarios.battery_low runs 7.5s end-to-end; DTC
        # broadcast lands after drain reaches DISABLE_LOW + RZC 4-sample
        # average + DEM confirm/broadcast. Observed 7871..25147ms across
        # runs as suite length and system load vary — budget 30s to stop
        # flaking on the slow end.
        observe_sec=30.0,
        verdicts=[
            VerdictCheck(
                description="DTC 0xE401 broadcast",
                check_type="dtc",
                expected="DTC 0xE401 received",
                value=0xE401,
                timeout_ms=30000,
            ),
            VerdictCheck(
                description="Vehicle enters DEGRADED, LIMP, or SAFE_STOP (battery drain cascades)",
                check_type="vehicle_state",
                expected="DEGRADED, LIMP, or SAFE_STOP",
                value=[2, 3, 4],  # DEGRADED=2, LIMP=3, or SAFE_STOP=4 (continuous drain cascades)
                timeout_ms=30000,
            ),
        ],
    ),
    TestSpec(
        id="runaway_accel",
        label="Runaway Acceleration",
        scenario="runaway_accel",
        sg="SG-011", asil="C", he="HE-016",
        description="Verifies excessive pedal input triggers safe reaction. "
                    "Safety Goal SG-011: prevent runaway acceleration. "
                    "100% pedal may trigger overcurrent → SAFE_STOP (stricter than DEGRADED).",
        injection="SPI pedal override at 100% (enters CVC full pipeline)",
        observe_sec=5.0,
        verdicts=[
            VerdictCheck(
                description="Vehicle enters DEGRADED or SAFE_STOP (both valid safety reactions)",
                check_type="vehicle_state",
                expected="DEGRADED or SAFE_STOP",
                value=[2, 4],  # DEGRADED=2 or SAFE_STOP=4
                timeout_ms=5000,
            ),
        ],
    ),
    TestSpec(
        id="motor_reversal",
        label="Motor Reversal",
        scenario="motor_reversal",
        sg="SG-002", asil="C", he="HE-014",
        description="Verifies motor blockage/reversal triggers SAFE_STOP and DTC. "
                    "Safety Goal SG-002: prevent unintended motor reversal during forward motion.",
        injection="SPI pedal 80% + MQTT inject_stall + inject_overcurrent to plant-sim",
        post_run_settle_sec=10.0,
        # Widened from 5.0s / 5000ms: motor_reversal needs stall + overcurrent
        # to both propagate through RZC's debounce + DEM confirmation + 0x500
        # broadcast, which lands on 5.0-5.1s in SIL. A tighter budget turned
        # this into a timing-flake that passed on the fast path and failed
        # when Docker scheduling was busier (5037ms vs 5068ms).
        observe_sec=6.0,
        verdicts=[
            VerdictCheck(
                description="Vehicle enters SAFE_STOP or SHUTDOWN",
                check_type="vehicle_state",
                expected="SAFE_STOP or SHUTDOWN",
                # Accept SHUTDOWN because after the overcurrent/stall
                # injection ends and the pedal releases, plant-sim may
                # still be publishing residual motor current while
                # torque has already dropped to 0 — SC's creep guard
                # can then fire and drive CVC to SHUTDOWN via
                # EVT_SC_KILL. The vehicle is still safely stopped.
                value=[4, 5],
                timeout_ms=5000,
            ),
            VerdictCheck(
                description="DTC 0xE301 broadcast",
                check_type="dtc",
                expected="DTC 0xE301 received",
                value=0xE301,
                timeout_ms=6000,
            ),
        ],
    ),
    TestSpec(
        id="creep_from_stop",
        label="Creep from Stop",
        scenario="creep_from_stop",
        sg="SG-001", asil="D", he="HE-017",
        description="Verifies SC creep guard detects BTS7960 FET short (MB-006) and "
                    "kills relay. Fault model: motor draws 1000mA despite zero torque "
                    "command. SC cross-plausibility (SSR-SC-018, SM-024) detects "
                    "torque=0 AND current>500mA for 2 cycles → kill relay → SAFE_STOP. "
                    "Safety Goal SG-001: prevent unintended vehicle motion from motor fault.",
        injection="Plant-sim injects 1000mA motor current (BTS7960 FET short simulation)",
        prep="",  # No pedal input — vehicle must be at standstill with torque=0
        post_run_settle_sec=10.0,
        observe_sec=5.0,
        verdicts=[
            VerdictCheck(
                description="Vehicle enters SAFE_STOP or SHUTDOWN (SC creep guard → kill relay)",
                check_type="vehicle_state",
                expected="SAFE_STOP or SHUTDOWN",
                # CVC VSM maps EVT_SC_KILL → SHUTDOWN from every non-INIT
                # state (TSR-035: SC kill is an external override that
                # takes precedence). Accept either here because the test
                # is asking "did the vehicle reach a safe, non-operational
                # state because SC detected creep?" — SHUTDOWN is the
                # strictly safer answer to that question.
                value=[4, 5],  # SAFE_STOP or SHUTDOWN
                timeout_ms=5000,
            ),
        ],
    ),
]

# Lookup by ID
TEST_SPECS_BY_ID: dict[str, TestSpec] = {s.id: s for s in TEST_SPECS}
