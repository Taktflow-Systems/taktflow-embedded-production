"""Simple battery voltage model: V = V_nom - I*R_int.

Supports external override via inject_voltage() for fault injection demos.
"""

import time


class BatteryModel:
    V_NOMINAL_MV = 12600   # 12.6V fully charged
    R_INTERNAL_MOHM = 50   # 50 milliohm internal resistance
    OVERRIDE_TIMEOUT = 8.0 # seconds before override expires

    def __init__(self):
        self.voltage_mv = self.V_NOMINAL_MV
        self.soc = 100
        self._override_voltage: int | None = None
        self._override_soc: int | None = None
        self._override_ts = 0.0

    def inject_voltage(self, voltage_mv: int, soc_pct: int):
        """Override battery state (called when external Battery_Status CAN received)."""
        self._override_voltage = voltage_mv
        self._override_soc = soc_pct
        self._override_ts = time.monotonic()

    def clear_override(self):
        """Clear voltage override (called on reset)."""
        self._override_voltage = None
        self._override_soc = None

    def reset_state(self):
        """Full physics reset — return to power-on defaults."""
        self.voltage_mv = self.V_NOMINAL_MV
        self.soc = 100
        self.clear_override()

    def update(self, motor_current_ma: float, dt: float):
        # Check if override is active and not expired
        if self._override_voltage is not None:
            if time.monotonic() - self._override_ts < self.OVERRIDE_TIMEOUT:
                self.voltage_mv = self._override_voltage
                if self._override_soc is not None:
                    self.soc = self._override_soc
                return
            else:
                self._override_voltage = None
                self._override_soc = None

        # Normal physics
        drop_mv = (motor_current_ma / 1000.0) * self.R_INTERNAL_MOHM
        self.voltage_mv = int(self.V_NOMINAL_MV - drop_mv)
        self.voltage_mv = max(0, min(20000, self.voltage_mv))

        # Simple SOC drain — realistic rate for demo stability.
        # At 30A continuous, 100% → 0% takes ~33 hours (like a real 1Ah pack).
        energy_used = motor_current_ma * dt / 3600000.0  # Ah
        self.soc -= energy_used * 0.1
        self.soc = max(0, min(100, self.soc))

    @property
    def status(self) -> int:
        """0=critical_UV, 1=UV_warn, 2=normal, 3=OV_warn, 4=critical_OV."""
        if self.voltage_mv < 9000:
            return 0
        elif self.voltage_mv < 10500:
            return 1
        elif self.voltage_mv > 15000:
            return 4
        elif self.voltage_mv > 14000:
            return 3
        return 2
