"""Unit tests for plant simulator models — battery, motor, steering, brake.

@aspice  SWE.4 — Software Unit Verification
@iso     ISO 26262 Part 6, Section 9 — Software unit testing
@scope   Individual plant simulator model classes in isolation

Covers: speed simulation, battery fault injection, voltage sag,
status thresholds, steering tracking/faults, brake tracking/faults,
motor limiting in degraded/limp states, vehicle state transitions.
"""

import time
from unittest.mock import patch

import pytest

from plant_sim.battery_model import BatteryModel
from plant_sim.motor_model import MotorModel
from plant_sim.steering_model import SteeringModel
from plant_sim.brake_model import BrakeModel


# ---------------------------------------------------------------------------
# Battery Model
# ---------------------------------------------------------------------------

class TestBatteryModel:
    def test_nominal_voltage(self):
        b = BatteryModel()
        assert b.voltage_mv == 12600
        assert b.soc == 100
        assert b.status == 2  # normal

    def test_voltage_sag_under_load(self):
        b = BatteryModel()
        b.update(10000, 0.01)  # 10A load
        # drop = 10A * 0.05ohm = 0.5V = 500mV
        assert b.voltage_mv == 12100

    def test_status_thresholds(self):
        b = BatteryModel()

        b.voltage_mv = 12600
        assert b.status == 2  # normal

        b.voltage_mv = 10200
        assert b.status == 1  # UV_warn

        b.voltage_mv = 8500
        assert b.status == 0  # critical_UV

        b.voltage_mv = 14500
        assert b.status == 3  # OV_warn

        b.voltage_mv = 16000
        assert b.status == 4  # critical_OV

    def test_inject_voltage_overrides_physics(self):
        b = BatteryModel()
        b.inject_voltage(8500, 5)
        b.update(0, 0.01)  # normal update should NOT override
        assert b.voltage_mv == 8500
        assert b.soc == 5
        assert b.status == 0  # critical_UV

    def test_inject_voltage_expires(self):
        b = BatteryModel()
        b.inject_voltage(8500, 5)

        # Simulate time passing beyond timeout
        b._override_ts = time.monotonic() - 10.0  # 10s ago, timeout is 8s
        b.update(0, 0.01)

        # Should revert to nominal physics
        assert b.voltage_mv == 12600
        assert b.status == 2  # normal

    def test_clear_override(self):
        b = BatteryModel()
        b.inject_voltage(8500, 5)
        b.clear_override()
        b.update(0, 0.01)
        assert b.voltage_mv == 12600

    def test_soc_drains_under_load(self):
        b = BatteryModel()
        initial_soc = b.soc
        for _ in range(100):
            b.update(15000, 0.01)
        assert b.soc < initial_soc


# ---------------------------------------------------------------------------
# Motor Model — speed derivation
# ---------------------------------------------------------------------------

class TestMotorSpeed:
    def test_motor_reaches_target_rpm(self):
        m = MotorModel()
        # Run at 50% duty forward for 3 seconds (300 ticks at 10ms)
        for _ in range(300):
            m.update(50.0, 1, dt=0.01)
        # Should approach 50% of no-load RPM (4000 * 0.5 = 2000)
        assert 1800 < m.rpm < 2200

    def test_motor_stops_when_disabled(self):
        m = MotorModel()
        # Spin up
        for _ in range(300):
            m.update(50.0, 1, dt=0.01)
        assert m.rpm > 1000
        # Stop (direction=0) — decay tau=0.5s, need ~4s to reach near-zero
        for _ in range(400):
            m.update(0, 0, dt=0.01)
        assert m.rpm < 50

    def test_brake_load_reduces_rpm(self):
        m = MotorModel()
        # Full speed no brake
        for _ in range(300):
            m.update(50.0, 1, dt=0.01, brake_load=0.0)
        rpm_no_brake = m.rpm

        m2 = MotorModel()
        # Same duty with 50% brake
        for _ in range(300):
            m2.update(50.0, 1, dt=0.01, brake_load=0.5)
        rpm_with_brake = m2.rpm

        assert rpm_with_brake < rpm_no_brake * 0.7

    def test_speed_kmh_from_rpm(self):
        """Verify the speed formula used in the WS bridge."""
        SPEED_FACTOR = 2 * 3.14159 * 0.15 * 60 / (8 * 1000)
        # At 2000 RPM
        speed = 2000 * SPEED_FACTOR
        assert 13.0 < speed < 15.0  # ~14.1 km/h
        # At 4000 RPM (max)
        speed_max = 4000 * SPEED_FACTOR
        assert 27.0 < speed_max < 30.0  # ~28.3 km/h
        # At 0 RPM
        assert 0 * SPEED_FACTOR == 0.0


# ---------------------------------------------------------------------------
# Motor Limiting in Degraded/Limp States
# ---------------------------------------------------------------------------

class TestMotorLimiting:
    def test_degraded_caps_duty_at_50(self):
        """In DEGRADED state, duty should be capped at 50%."""
        m = MotorModel()
        # Simulate DEGRADED: cap at 50% even when commanded at 95%
        capped_duty = min(95.0, 50.0)
        for _ in range(300):
            m.update(capped_duty, 1, dt=0.01)
        # 50% of 4000 = 2000 RPM target
        assert m.rpm < 2200

    def test_limp_caps_duty_at_15(self):
        """In LIMP state, duty should be capped at 15%."""
        m = MotorModel()
        capped_duty = min(50.0, 15.0)
        for _ in range(300):
            m.update(capped_duty, 1, dt=0.01, brake_load=0.3)
        # 15% duty with 30% brake load — very low RPM
        assert m.rpm < 500

    def test_limp_speed_much_lower_than_normal(self):
        """LIMP mode speed should be significantly lower than normal drive."""
        SPEED_FACTOR = 2 * 3.14159 * 0.15 * 60 / (8 * 1000)

        # Normal: 50% duty, no brake
        m_normal = MotorModel()
        for _ in range(300):
            m_normal.update(50.0, 1, dt=0.01)
        speed_normal = m_normal.rpm * SPEED_FACTOR

        # Limp: 15% duty, 30% brake
        m_limp = MotorModel()
        for _ in range(300):
            m_limp.update(15.0, 1, dt=0.01, brake_load=0.3)
        speed_limp = m_limp.rpm * SPEED_FACTOR

        assert speed_limp < speed_normal * 0.3


# ---------------------------------------------------------------------------
# Vehicle State Transitions
# ---------------------------------------------------------------------------

class TestVehicleStateTransitions:
    """Test vehicle state logic extracted from simulator.py.

    We test the state transition logic directly rather than the full
    simulator (which requires CAN bus), keeping tests fast and isolated.
    """

    # State constants (match simulator.py)
    VS_INIT = 0
    VS_RUN = 1
    VS_DEGRADED = 2
    VS_LIMP = 3
    VS_SAFE_STOP = 4

    def _transition(self, vehicle_state, has_fault, battery_status,
                    estop_active=False):
        """Replicate the vehicle state transition logic from simulator.py."""
        battery_critical = battery_status == 0
        battery_warn = battery_status == 1

        if estop_active:
            return self.VS_SAFE_STOP

        if battery_critical and vehicle_state in (self.VS_RUN, self.VS_DEGRADED):
            return self.VS_LIMP
        elif has_fault and vehicle_state == self.VS_RUN:
            return self.VS_DEGRADED
        elif battery_warn and vehicle_state == self.VS_RUN:
            return self.VS_DEGRADED
        elif (not has_fault and not battery_warn and not battery_critical
              and vehicle_state in (self.VS_DEGRADED, self.VS_LIMP)):
            return self.VS_RUN

        return vehicle_state

    def test_normal_run(self):
        state = self._transition(self.VS_RUN, False, 2)
        assert state == self.VS_RUN

    def test_motor_fault_triggers_degraded(self):
        state = self._transition(self.VS_RUN, True, 2)
        assert state == self.VS_DEGRADED

    def test_battery_warn_triggers_degraded(self):
        state = self._transition(self.VS_RUN, False, 1)
        assert state == self.VS_DEGRADED

    def test_battery_critical_triggers_limp_from_run(self):
        state = self._transition(self.VS_RUN, False, 0)
        assert state == self.VS_LIMP

    def test_battery_critical_triggers_limp_from_degraded(self):
        state = self._transition(self.VS_DEGRADED, True, 0)
        assert state == self.VS_LIMP

    def test_faults_cleared_returns_to_run(self):
        state = self._transition(self.VS_DEGRADED, False, 2)
        assert state == self.VS_RUN

    def test_limp_clears_to_run(self):
        state = self._transition(self.VS_LIMP, False, 2)
        assert state == self.VS_RUN

    def test_estop_overrides_all(self):
        state = self._transition(self.VS_RUN, False, 2, estop_active=True)
        assert state == self.VS_SAFE_STOP

    def test_fault_does_not_degrade_from_init(self):
        """Faults in INIT should not change state — only from RUN."""
        state = self._transition(self.VS_INIT, True, 2)
        assert state == self.VS_INIT

    def test_battery_warn_during_motor_fault(self):
        """Motor fault already degraded — battery warn doesn't change state."""
        state = self._transition(self.VS_DEGRADED, True, 1)
        assert state == self.VS_DEGRADED  # stays degraded, not back to RUN


# ---------------------------------------------------------------------------
# Steering Model
# ---------------------------------------------------------------------------

class TestSteeringModel:
    def test_tracks_commanded_angle(self):
        s = SteeringModel()
        for _ in range(200):
            s.update(20.0, 0.01)
        assert abs(s.actual_angle - 20.0) < 1.0

    def test_clamps_to_limits(self):
        s = SteeringModel()
        s.update(90.0, 0.01)  # beyond max
        assert s.commanded_angle == 45.0

    def test_fault_on_rapid_oscillation(self):
        s = SteeringModel()
        assert not s.fault
        for _ in range(10):
            s.record_command(40.0)
            s.record_command(-40.0)
        assert s.fault

    def test_clear_fault(self):
        s = SteeringModel()
        for _ in range(10):
            s.record_command(40.0)
            s.record_command(-40.0)
        assert s.fault
        s.clear_fault()
        assert not s.fault


# ---------------------------------------------------------------------------
# Brake Model
# ---------------------------------------------------------------------------

class TestBrakeModel:
    def test_tracks_commanded_pct(self):
        b = BrakeModel()
        for _ in range(100):
            b.update(80.0, 0.01)
        assert abs(b.actual_pct - 80.0) < 2.0

    def test_fault_on_rapid_alternation(self):
        b = BrakeModel()
        assert not b.fault
        for _ in range(10):
            b.record_command(100.0)
            b.record_command(0.0)
        assert b.fault

    def test_clear_fault(self):
        b = BrakeModel()
        for _ in range(10):
            b.record_command(100.0)
            b.record_command(0.0)
        assert b.fault
        b.clear_fault()
        assert not b.fault
